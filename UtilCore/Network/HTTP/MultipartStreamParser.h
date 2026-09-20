
//***************************************************************************
// MultipartStreamParser.h : multipart/form-data 본문을 점진적으로(스트리밍)
//                           파싱하는 상태머신.
//
// [설계 배경] MultipartFormParser.h(HTTP::ParseMultipartFormData())는 완성된
// body 전체를 std::string_view 하나로 받아 한 번에 스캔한다 — 짧은 필드
// (닉네임, 토큰 등)에는 충분하지만, 1GB급 파일이 통째로 메모리에(그것도
// CHttpRequestParser::m_body에 한 번, 이 파서가 또 한 번 복사하며) 올라가
// 있어야 한다는 전제 자체가 대용량 업로드와 맞지 않는다.
//
// 이 클래스는 HttpRequestParser.h에 새로 생긴 SetBodyStreamCallback()과
// 짝을 이룬다 — CHttpRequestParser가 본문을 청크 단위로 흘려줄 때마다
// Feed()를 호출해주면, 이 파서가 boundary를 기준으로 "지금 어느 파트를
// 읽고 있는지"만 상태로 들고 있으면서, 파트 데이터가 나오는 대로 즉시
// OnPartData 콜백으로 넘긴다 — 파일 파트라면 그 콜백 안에서 디스크에
// 바로 쓰면 되므로, 이 파서 자신도 파일 전체를 절대 메모리에 쌓지 않는다
// (경계가 청크 경계에 걸쳐 있을 가능성 때문에 boundary 길이 정도의 아주
// 작은 "보류 버퍼"만 유지한다 — 자세한 설명은 PartBody 처리 부분 참고).
//
// [알려진 한계]
//   - 헤더 섹션(각 파트의 Content-Disposition/Content-Type 등)은 여전히
//     메모리에 버퍼링한다 — 다만 이건 항상 몇백 바이트 수준이라 문제되지
//     않는다(kMaxHeaderSection으로 비정상적으로 큰 경우만 방어).
//   - 헤더 값에 boundary 문자열과 완전히 같은 바이트열이 우연히 등장하는
//     극단적인 경우는 고려하지 않는다(RFC 7578 관례상 boundary는 무작위
//     생성되므로 실질적으로 발생하지 않음).
//***************************************************************************

#ifndef UC_MULTIPARTSTREAMPARSER_H
#define UC_MULTIPARTSTREAMPARSER_H

#include <Network/HTTP/HttpParseUtil.h>
#include <Network/HTTP/MultipartFormParser.h>	// ParseContentDisposition() 재사용

#include <string>
#include <string_view>
#include <functional>

namespace HTTP
{
	//***************************************************************************
	// @struct SMultipartPartInfo
	// @brief 파트 하나의 헤더에서 뽑아낸 정보 — OnPartBegin 콜백으로 전달된다.
	//***************************************************************************
	struct SMultipartPartInfo
	{
		std::string name;			// Content-Disposition의 name="..."
		std::string filename;		// filename="..."이 있으면(파일 필드) 그 값, 없으면 빈 문자열
		std::string contentType;	// 이 파트의 Content-Type(명시 안 됐으면 빈 문자열)
	};

	//***************************************************************************
	// @class CMultipartStreamParser
	// @brief boundary 하나를 기준으로 multipart/form-data 본문을 점진적으로
	//        스캔하는 상태머신. Feed()를 여러 번(임의 크기로 나눠) 호출해도
	//        내부 상태로 이어붙여 처리한다.
	//***************************************************************************
	class CMultipartStreamParser
	{
	public:
		using OnPartBeginFn = std::function<void(const SMultipartPartInfo& info)>;
		using OnPartDataFn = std::function<void(const char* data, size_t len)>;
		using OnPartEndFn = std::function<void()>;

		//***************************************************************************
		// @brief CMultipartStreamParser 생성자
		// @param boundary Content-Type의 boundary= 값(HTTP::ExtractBoundary()가
		//        뽑아준 것 — 앞의 "--"는 이 클래스가 내부적으로 붙인다).
		//***************************************************************************
		explicit CMultipartStreamParser(std::string boundary)
			: m_delimiter("--" + boundary)
			, m_bodyDelimiter("\r\n--" + boundary)
		{
		}

		//***************************************************************************
		// @brief 콜백 세 개를 등록한다.
		// @param onBegin 새 파트의 헤더 파싱이 끝났을 때(그 파트의 데이터가 오기
		//        직전) 호출 — 여기서 info.filename 유무 등을 보고 이후 onData로
		//        오는 바이트를 메모리에 쌓을지 디스크에 바로 쓸지 결정하면 된다.
		// @param onData 현재 파트의 데이터 바이트가 도착할 때마다(여러 번, 임의
		//        크기로) 호출.
		// @param onEnd 현재 파트의 데이터가 전부 끝났을 때 호출(다음 onBegin
		//        전까지 한 번만).
		//***************************************************************************
		void SetCallbacks(OnPartBeginFn onBegin, OnPartDataFn onData, OnPartEndFn onEnd)
		{
			m_onBegin = std::move(onBegin);
			m_onData = std::move(onData);
			m_onEnd = std::move(onEnd);
		}

		//***************************************************************************
		// @brief 본문 바이트를 밀어넣는다. CHttpRequestParser::SetBodyStreamCallback()의
		//        콜백에서 그대로 호출하는 용도.
		// @return 형식이 심하게 깨져 있으면 false(호출부는 400 Bad Request 등으로 응답
		//         후 연결을 끊는 것을 권장 — 이미 일부 파트를 디스크에 썼을 수 있으므로
		//         호출부가 그 임시 파일 정리를 책임져야 한다).
		//***************************************************************************
		bool Feed(const char* data, size_t len)
		{
			m_pending.append(data, len);

			// 한 번의 Feed() 안에서 여러 파트/상태 전이가 연속으로 끝날 수 있으므로
			// (예: 아주 작은 필드 여러 개가 한 청크에 다 들어있는 경우), "더 이상
			// 진행할 수 없을 때(추가 데이터 필요)"까지 계속 반복한다.
			while( true )
			{
				switch( m_state )
				{
				case EState::Preamble:
					if( !StepPreamble() ) return m_state != EState::Error;
					break;
				case EState::AfterDelimiter:
					if( !StepAfterDelimiter() ) return m_state != EState::Error;
					break;
				case EState::PartHeaders:
					if( !StepPartHeaders() ) return m_state != EState::Error;
					break;
				case EState::PartBody:
					if( !StepPartBody() ) return m_state != EState::Error;
					break;
				case EState::Done:
					return true;
				case EState::Error:
					return false;
				}
			}
		}

		//***************************************************************************
		// @brief 마지막 종료 경계("--boundary--")까지 정상적으로 처리됐는지.
		//***************************************************************************
		bool IsDone() const { return m_state == EState::Done; }

	private:
		enum class EState { Preamble, AfterDelimiter, PartHeaders, PartBody, Done, Error };

		static constexpr size_t kMaxHeaderSection = 8192;	// 파트 헤더 섹션 상한(비정상 방어)
		static constexpr size_t kMaxPreambleScan = 4096;	// 첫 boundary를 찾기 전 보류할 최대 바이트(그 이상은 델리미터 길이만 남기고 버림)

		//***************************************************************************
		// @brief 첫 "--boundary"가 나오기 전까지의 프리앰블(보통 없거나 아주 짧음)을 건너뛴다.
		// @return false면 이번 Feed() 호출분으로는 더 진행 불가(추가 데이터 필요) —
		//         이 경우 바깥 루프가 즉시 반환해야 하므로 별도 상태 전이 없이 false.
		//***************************************************************************
		bool StepPreamble()
		{
			size_t pos = m_pending.find(m_delimiter);
			if( pos == std::string::npos )
			{
				// boundary가 이번 청크 끝에 걸쳐 있을 수 있으니, m_delimiter
				// 길이만큼만 꼬리로 남기고 나머지(어차피 프리앰블이라 버려도
				// 되는 내용)는 비운다 — 프리앰블이 무한정 쌓이는 것 방지.
				if( m_pending.size() > kMaxPreambleScan )
					m_pending.erase(0, m_pending.size() - m_delimiter.size());
				return false;
			}
			m_pending.erase(0, pos + m_delimiter.size());
			m_state = EState::AfterDelimiter;
			return true;
		}

		//***************************************************************************
		// @brief boundary 직후 — "--"(종료) 또는 "\r\n"(다음 파트로) 중 어느
		//        쪽인지 확인한다.
		//***************************************************************************
		bool StepAfterDelimiter()
		{
			if( m_pending.size() < 2 )
				return false; // 2바이트도 안 왔으면 판단 불가 — 더 받아야 함

			if( m_pending.compare(0, 2, "--") == 0 )
			{
				if( m_currentPartOpen && m_onEnd ) { m_onEnd(); }
				m_currentPartOpen = false;
				m_state = EState::Done;
				return true;
			}

			if( m_pending.compare(0, 2, "\r\n") == 0 )
			{
				m_pending.erase(0, 2);
				m_state = EState::PartHeaders;
				return true;
			}

			// "--"도 "\r\n"도 아니면 형식이 깨진 것.
			m_state = EState::Error;
			return false;
		}

		//***************************************************************************
		// @brief 파트 하나의 헤더 섹션("\r\n\r\n"으로 끝남)을 파싱한다.
		//***************************************************************************
		bool StepPartHeaders()
		{
			size_t headerEnd = m_pending.find("\r\n\r\n");
			if( headerEnd == std::string::npos )
			{
				if( m_pending.size() > kMaxHeaderSection )
				{
					m_state = EState::Error;
					return false;
				}
				return false; // 더 받아야 함
			}

			SMultipartPartInfo info;
			ParseHeaderLines(std::string_view(m_pending.data(), headerEnd), info);
			m_pending.erase(0, headerEnd + 4);

			m_currentPartOpen = true;
			if( m_onBegin )
				m_onBegin(info);

			m_state = EState::PartBody;
			return true;
		}

		//***************************************************************************
		// @brief 파트 데이터를 "\r\n--boundary"가 나올 때까지 흘려보낸다.
		// @details [핵심] 경계 문자열이 두 번의 Feed() 호출(=두 번의 recv 청크)에
		//          걸쳐 나뉘어 도착할 수 있다 — 그래서 매번 m_bodyDelimiter를
		//          찾되, 못 찾은 경우 "안전하게 흘려보낼 수 있는 만큼만"(꼬리에
		//          m_bodyDelimiter.size()-1바이트를 남기고) 내보낸다. 그
		//          꼬리에 실제로는 경계가 아닌 일반 데이터가 남아있을 수도
		//          있지만, 그건 다음 Feed()에서 마저 처리되므로 데이터 손실은
		//          없다 — 그저 "이 바이트가 경계의 일부인지 아직 확신 못
		//          하니 한 박자 늦게 내보낸다"는 것뿐이다.
		//***************************************************************************
		bool StepPartBody()
		{
			size_t pos = m_pending.find(m_bodyDelimiter);
			if( pos == std::string::npos )
			{
				if( m_pending.size() > m_bodyDelimiter.size() )
				{
					const size_t flushLen = m_pending.size() - (m_bodyDelimiter.size() - 1);
					if( m_onData && flushLen > 0 )
						m_onData(m_pending.data(), flushLen);
					m_pending.erase(0, flushLen);
				}
				return false; // 더 받아야 함(경계를 아직 못 찾음)
			}

			if( pos > 0 && m_onData )
				m_onData(m_pending.data(), pos);

			if( m_onEnd )
				m_onEnd();
			m_currentPartOpen = false;

			m_pending.erase(0, pos + m_bodyDelimiter.size());
			m_state = EState::AfterDelimiter;
			return true;
		}

		//***************************************************************************
		// @brief 헤더 섹션(빈 줄 이전까지)을 한 줄씩 나눠 Content-Disposition/
		//        Content-Type만 뽑아낸다. MultipartFormParser.h의
		//        ParseContentDisposition()을 그대로 재사용한다.
		//***************************************************************************
		static void ParseHeaderLines(std::string_view headerSection, SMultipartPartInfo& info)
		{
			size_t lineStart = 0;
			while( lineStart < headerSection.size() )
			{
				size_t lineEnd = headerSection.find("\r\n", lineStart);
				if( lineEnd == std::string_view::npos )
					lineEnd = headerSection.size();

				std::string_view line = headerSection.substr(lineStart, lineEnd - lineStart);
				size_t colon = line.find(':');
				if( colon != std::string_view::npos )
				{
					std::string_view headerName = line.substr(0, colon);
					std::string_view headerValue = HTTP::Trim(line.substr(colon + 1));

					if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Disposition") )
						ParseContentDisposition(headerValue, info.name, info.filename);
					else if( HTTP::EqualsIgnoreCaseAscii(headerName, "Content-Type") )
						info.contentType = std::string(headerValue);
				}

				lineStart = (lineEnd < headerSection.size()) ? lineEnd + 2 : lineEnd;
			}
		}

	private:
		std::string	m_delimiter;		// "--boundary" (첫 파트 시작/AfterDelimiter 판정용)
		std::string	m_bodyDelimiter;	// "\r\n--boundary" (파트 데이터 끝 판정용)
		std::string	m_pending;			// 아직 처리 못 했거나 경계 판정을 위해 보류 중인 바이트(파일 전체가 아니라 항상 작은 꼬리만 유지됨)

		EState	m_state = EState::Preamble;
		bool	m_currentPartOpen = false;

		OnPartBeginFn	m_onBegin;
		OnPartDataFn	m_onData;
		OnPartEndFn		m_onEnd;
	};
}

#endif // ndef UC_MULTIPARTSTREAMPARSER_H