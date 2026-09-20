
//***************************************************************************
// HttpRequestParser.h : interface for the CHttpRequestParser class.
//
// [수정 — 대용량 본문 스트리밍 지원] 원래 이 클래스는 본문을 항상 m_body
// (단일 std::string)에 전부 누적하고, kMaxBodyLen(64MB)을 넘으면 즉시
// Error로 처리했다 — 1GB급 파일 업로드는 이 캡에 걸려 거부되고, 캡을
// 없애더라도 서버 메모리에 파일 전체가 올라가는 문제가 있었다.
//
// 이번에 추가한 것은 전부 "옵션"이다 — 아무도 새 콜백을 등록하지 않으면
// 이 클래스는 예전과 완전히 동일하게 동작한다(기존 호출부에 영향 없음):
//   1. SetHeadersCompleteCallback() — 헤더 파싱이 끝나는 시점(본문이 오기
//      직전)에 호출자에게 알려준다. 이 콜백 안에서 요청의 메서드/경로/
//      Content-Type을 보고 "이건 대용량 업로드다"라고 판단되면, 그 자리에서
//      SetBodyStreamCallback()을 걸어 본문 처리 방식을 스트리밍으로 바꿀
//      기회를 준다(본문이 시작되기 전에 결정해야 하므로 시점이 중요하다).
//   2. SetBodyStreamCallback() — 등록하면 본문 바이트가 m_body에 쌓이는
//      대신 이 콜백으로 그때그때 넘어온다(m_body는 계속 비어있음 —
//      GetBody()는 스트리밍 모드에선 의미가 없다). 동시에 넘긴
//      maxBodyLenOverride가 0보다 크면, 그 값이 이번 요청에 한해
//      kMaxBodyLen(64MB) 대신 적용된다 — 스트리밍 중이라 메모리에 안
//      쌓이므로 훨씬 큰 상한(예: 여러 GB)을 안전하게 줄 수 있다.
//
// 두 콜백의 수명 관리가 다르다: HeadersCompleteCallback은 보통 세션이
// 생성될 때 한 번 걸어두고 계속 재사용하는 것을 의도해서 Reset()이 지우지
// 않는다. BodyStreamCallback/maxBodyLenOverride는 요청 하나에 대한
// 일회성 결정이라 Reset()이 매번 지운다 — 다음 요청에서 다시 필요하면
// HeadersCompleteCallback 안에서 다시 걸어야 한다.
//***************************************************************************

#ifndef UC_HTTPREQUESTPARSER_H
#define UC_HTTPREQUESTPARSER_H

#include <Network/HTTP/HttpParseUtil.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <charconv>
#include <algorithm>
#include <cctype>
#include <functional>

//***************************************************************************
// @class CHttpRequestParser
// @brief HTTP/1.x 요청을 증분(incremental)으로 파싱하는 상태머신 (CHttpResponseParser의
//        서버 측 대응)
//
// @details
//      [char 전용] HTTP 관련 클래스는 전부 char 타입으로 통일한다 — 설계
//      원칙은 CHttpResponseParser와 동일하다(이유는 HttpResponseParser.h의
//      클래스 설명 참고 — UNICODE 빌드에서 TCHAR=wchar_t 기반으로 바꾸면
//      와이어 바이트 스트림 가정이 깨짐). TCHAR 문자열이 필요한 호출부는
//      CIconvUtil 등으로 미리 변환해서 char로 넘기거나, 파싱 결과를 받아
//      직접 변환하는 것을 권장한다.
//      대소문자 비교(HTTP::EqualsIgnoreCaseAscii)/공백 트림(HTTP::Trim)/파싱
//      상태(HTTP::EParseState)는 HttpParseUtil.h의 공용 정의를 그대로 재사용해
//      CHttpResponseParser와 중복 정의하지 않는다(HTTP::EParseState::StartLine
//      하나로 요청 라인/상태 라인을 함께 표현 — RFC 7230 §3.1의 start-line =
//      request-line / status-line 문법을 그대로 반영한 것).
//
//      [응답 파서와의 차이점]
//      - 상태 라인 대신 요청 라인("METHOD URI VERSION")을 파싱한다 — 상태
//        코드/Reason-Phrase 대신 Method/URI/Version 세 필드를 채운다.
//      - URI는 경로와 쿼리 문자열을 분리해서 조회할 수 있다(GetPath()/
//        GetQueryString(), '?' 기준 분리, 제로카피 뷰).
//      - Content-Length도 chunked도 없는 요청은 CHttpResponseParser와 달리
//        "알려진 제약"이 아니라 RFC 7230상 실제로 올바른 동작이다 — 요청은
//        (일부 레거시 케이스를 제외하면) "연결 종료까지 body"라는 개념 자체가
//        없으므로, 본 구현이 body 없음으로 간주하고 즉시 Complete 처리하는 것이
//        스펙에 맞는 정상 동작이다.
//      - IsKeepAlive()로 HTTP 버전 + Connection 헤더를 함께 고려한 keep-alive
//        여부를 제공한다(HTTP/1.1은 명시적 "Connection: close"가 없으면 기본
//        keep-alive, HTTP/1.0은 명시적 "Connection: keep-alive"가 없으면 기본
//        close — RFC 7230 6.3절).
//
//      [사용 패턴 — 서버 세션의 수신 훅에서]
//      CHttpRequestParser parser;
//      // ... Dispatch() 콜백마다:
//      auto state = parser.Feed(recvData, recvLen);
//      if (state == HTTP::EParseState::Complete) { /* 요청 처리 -> 응답 전송 */ }
//      else if (state == HTTP::EParseState::Error) { /* 400 Bad Request 등, 커넥션 폐기 */ }
//      // 같은 커넥션에서 다음 요청을 받기 전 반드시 parser.Reset() 호출 (keep-alive 재사용)
//
//      [사용 패턴 — 대용량 본문 스트리밍]
//      parser.SetHeadersCompleteCallback([](CHttpRequestParser& p) {
//          if (p.GetMethod() == "POST" && p.GetPath() == "/upload")
//          {
//              p.SetBodyStreamCallback(
//                  [](const char* data, size_t len) { /* 파일에 바로 쓰기 등 */ },
//                  4LL * 1024 * 1024 * 1024 /* 4GB 상한 */);
//          }
//      });
//***************************************************************************
class CHttpRequestParser
{
public:
	//***************************************************************************
	// @brief 본문 바이트가 도착할 때마다 호출되는 콜백 — 스트리밍 모드에서만 쓰인다.
	//***************************************************************************
	using BodyStreamCallback = std::function<void(const char* data, size_t len)>;

	//***************************************************************************
	// @brief 헤더 파싱이 끝나는 시점(본문이 시작되기 직전)에 호출되는 콜백.
	//        이 안에서 SetBodyStreamCallback()을 걸면 이번 요청의 본문 처리
	//        방식을 스트리밍으로 바꿀 수 있다.
	//***************************************************************************
	using HeadersCompleteCallback = std::function<void(CHttpRequestParser&)>;

	//***************************************************************************
	// @brief CHttpRequestParser 생성자
	// @details 헤더 목록 벡터를 미리 reserve해 초기 파싱 중 재할당을 줄인다.
	//***************************************************************************
	CHttpRequestParser() { m_headers.reserve(16); }

	//***************************************************************************
	// @brief 수신 바이트를 밀어넣습니다. 여러 번 나눠 호출해도 내부 상태로 이어붙여 처리합니다.
	// @param data 수신 바이트 포인터 (와이어 바이트, 항상 char 기준)
	// @param len data의 길이
	// @return HTTP::EParseState 이 호출 이후의 현재 상태 (Complete/Error면 호출부가 후속 처리)
	//***************************************************************************
	HTTP::EParseState Feed(const char* data, size_t len)
	{
		size_t pos = 0;
		while( pos < len && m_state != HTTP::EParseState::Complete && m_state != HTTP::EParseState::Error )
		{
			switch( m_state )
			{
			case HTTP::EParseState::StartLine:
				pos += ConsumeLine(data + pos, len - pos, /*isRequestLine=*/true);
				break;
			case HTTP::EParseState::Headers:
				pos += ConsumeLine(data + pos, len - pos, /*isRequestLine=*/false);
				break;
			case HTTP::EParseState::Body:
				pos += ConsumeBody(data + pos, len - pos);
				break;
			case HTTP::EParseState::ChunkedSize:
				pos += ConsumeChunkSizeLine(data + pos, len - pos);
				break;
			case HTTP::EParseState::ChunkedData:
				pos += ConsumeChunkData(data + pos, len - pos);
				break;
			case HTTP::EParseState::ChunkedCRLF:
				pos += ConsumeChunkTrailingCrlf(data + pos, len - pos);
				break;
			case HTTP::EParseState::ChunkedTrailer:
				pos += ConsumeLine(data + pos, len - pos, /*isRequestLine=*/false, /*isTrailer=*/true);
				break;
			default:
				break;
			}
		}
		// Complete/Error 상태로 루프가 조기 종료되면 pos < len일 수 있다 (예: 파이프라이닝된
		// 다음 요청의 바이트가 같은 수신 버퍼에 이어 붙어 온 경우). 호출부가 실제 소비량을
		// 알 수 있도록 이번 호출에서 소비한 바이트 수를 기록해둔다 (GetLastFeedConsumed() 참고).
		m_lastFeedConsumed = pos;
		return m_state;
	}

	//***************************************************************************
	// @brief 직전 Feed() 호출에서 실제로 소비된 바이트 수를 반환합니다.
	// @details Complete/Error로 상태가 바뀌며 루프가 조기 종료된 경우, 전달한 len
	//          전부가 소비되지 않았을 수 있다(파이프라이닝된 다음 요청의 선두 바이트가
	//          같은 버퍼에 섞여 들어온 경우 등). 호출부는 Feed()가 Complete를 반환했을 때
	//          이 값을 이번에 "처리된" 바이트 수로 사용하고, 나머지(len - 반환값)는
	//          다음 처리를 위해 버퍼에 남겨두어야 한다.
	//***************************************************************************
	size_t GetLastFeedConsumed() const noexcept { return m_lastFeedConsumed; }

	//***************************************************************************
	// @brief 같은 커넥션(keep-alive)에서 다음 요청을 위해 재사용합니다.
	// @details [수정] m_headersCompleteCallback은 지우지 않는다 — 보통 세션
	//          생성 시 한 번 걸어두고 그 커넥션이 살아있는 동안 모든 요청에
	//          공통으로 재사용하는 것을 의도하기 때문이다(사용 패턴 예시
	//          참고). 반면 m_bodyStreamCallback/m_maxBodyLenOverride는 방금
	//          끝난 "그 요청 하나"에 대한 일회성 결정이라 반드시 지운다 —
	//          안 지우면 다음(전혀 다른 목적의) 요청의 본문까지 실수로
	//          스트리밍 모드로 처리되는 사고가 난다.
	//***************************************************************************
	void Reset()
	{
		m_state = HTTP::EParseState::StartLine;
		m_lineBuffer.clear();
		m_method.clear();
		m_uri.clear();
		m_version.clear();
		m_headers.clear();
		m_body.clear();
		m_contentLength = 0;
		m_hasContentLength = false;
		m_chunked = false;
		m_chunkRemaining = 0;
		m_crlfSkipped = 0;
		m_keepAlive = false;
		m_lastFeedConsumed = 0;
		m_bodyBytesConsumed = 0;
		m_bodyStreamCallback = nullptr;
		m_maxBodyLenOverride = 0;
	}

	//***************************************************************************
	// @brief 현재 파싱 진행 상태를 반환합니다.
	//***************************************************************************
	HTTP::EParseState GetState() const noexcept { return m_state; }

	//***************************************************************************
	// @brief HTTP 메서드를 바이트 문자열 그대로 반환합니다 (예: "GET", "POST").
	//***************************************************************************
	const std::string& GetMethod() const noexcept { return m_method; }

	//***************************************************************************
	// @brief 요청 라인의 URI 전체(경로+쿼리)를 바이트 문자열 그대로 반환합니다.
	//***************************************************************************
	const std::string& GetUri() const noexcept { return m_uri; }

	//***************************************************************************
	// @brief URI에서 쿼리 문자열을 제외한 경로 부분만 반환합니다 (제로카피 뷰).
	// @return std::string_view '?' 이전까지의 경로. '?'가 없으면 URI 전체.
	//***************************************************************************
	std::string_view GetPath() const noexcept
	{
		size_t q = m_uri.find('?');
		return std::string_view(m_uri).substr(0, q);
	}

	//***************************************************************************
	// @brief URI에서 쿼리 문자열 부분만 반환합니다 (제로카피 뷰, '?' 제외).
	// @return std::string_view '?' 다음부터 끝까지. '?'가 없으면 빈 뷰.
	//***************************************************************************
	std::string_view GetQueryString() const noexcept
	{
		size_t q = m_uri.find('?');
		if( q == std::string::npos ) return {};
		return std::string_view(m_uri).substr(q + 1);
	}

	//***************************************************************************
	// @brief HTTP 버전 문자열을 바이트 문자열 그대로 반환합니다 (예: "HTTP/1.1").
	//***************************************************************************
	const std::string& GetVersion() const noexcept { return m_version; }

	//***************************************************************************
	// @brief 파싱된 헤더 목록을 바이트 문자열 쌍 그대로 반환합니다 (삽입 순서 보존).
	//***************************************************************************
	const std::vector<std::pair<std::string, std::string>>& GetHeaders() const noexcept { return m_headers; }

	//***************************************************************************
	// @brief 지금까지 누적된 body를 바이트 그대로 반환합니다.
	// @details [주의] SetBodyStreamCallback()으로 스트리밍 모드가 걸린
	//          요청에서는 본문이 이 버퍼에 전혀 쌓이지 않는다 — 이 경우
	//          이 함수는 항상 빈 문자열을 돌려준다. 스트리밍 모드 여부는
	//          호출부(HeadersCompleteCallback을 건 쪽)가 스스로 알고
	//          있어야 한다(파서가 별도로 "스트리밍이었는지" 플래그를
	//          제공하지 않음 — 애초에 그걸 결정한 쪽이 호출부이므로).
	//***************************************************************************
	const std::string& GetBody() const noexcept { return m_body; }

	//***************************************************************************
	// @brief 이 요청이 keep-alive로 처리돼야 하는지 반환합니다.
	// @return bool true면 응답 전송 후 커넥션을 유지, false면 응답 후 닫아야 함.
	// @details HTTP/1.1은 "Connection: close"가 명시되지 않는 한 기본 keep-alive,
	//          HTTP/1.0(또는 그 이하)은 "Connection: keep-alive"가 명시되지
	//          않는 한 기본 close — RFC 7230 6.3절 규칙을 그대로 따른다.
	//***************************************************************************
	bool IsKeepAlive() const noexcept { return m_keepAlive; }

	//***************************************************************************
	// @brief 헤더를 이름으로 찾습니다 (대소문자 무시, ASCII 바이트 문자열 기준).
	// @param key 찾을 헤더 이름
	// @return std::string_view 찾은 헤더 값(원본 버퍼를 가리키는 뷰). 없으면 빈 뷰.
	//***************************************************************************
	std::string_view FindHeader(std::string_view key) const noexcept
	{
		for( const auto& [k, v] : m_headers )
			if( HTTP::EqualsIgnoreCaseAscii(k, key) )
				return v;
		return {};
	}

	//***************************************************************************
	// @brief [추가] 헤더 파싱 완료 시점 콜백을 등록합니다. Reset()으로 지워지지
	//        않는다 — 보통 세션이 살아있는 동안 계속 재사용한다.
	//***************************************************************************
	void SetHeadersCompleteCallback(HeadersCompleteCallback cb)
	{
		m_headersCompleteCallback = std::move(cb);
	}

	//***************************************************************************
	// @brief [추가] 지금 파싱 중인 요청 하나에 한해, 본문을 m_body에 누적하는
	//        대신 콜백으로 그때그때 넘기도록 전환합니다.
	// @details 반드시 HeadersCompleteCallback 안에서(본문이 시작되기 전에)
	//          호출해야 의미가 있다 — 본문이 이미 일부라도 m_body에 쌓인
	//          뒤에 호출하면 그 앞부분은 콜백으로 못 받는다(이 클래스는
	//          그 시점 전환까지는 지원하지 않음). Reset() 호출 시 자동으로
	//          해제된다(다음 요청에는 기본적으로 적용되지 않음).
	// @param onBodyChunk 본문 바이트가 도착할 때마다(여러 번) 호출된다.
	// @param maxBodyLenOverride 0보다 크면 이번 요청에 한해 kMaxBodyLen(64MB)
	//        대신 이 값을 상한으로 쓴다. 스트리밍 중이라 메모리에 쌓이지
	//        않으므로 훨씬 크게(예: 수 GB) 줘도 안전하다. 0이면 기존
	//        kMaxBodyLen을 그대로 적용한다(스트리밍은 하되 크기 제한은
	//        기존과 동일하게 유지하고 싶은 경우).
	//***************************************************************************
	void SetBodyStreamCallback(BodyStreamCallback onBodyChunk, size_t maxBodyLenOverride = 0)
	{
		m_bodyStreamCallback = std::move(onBodyChunk);
		m_maxBodyLenOverride = maxBodyLenOverride;
	}

private:
	//***************************************************************************
	// @brief data 안에서 개행(\n)을 찾아 한 줄을 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @param isRequestLine 요청 라인 파싱 중인지 여부 (true면 HandleRequestLine으로)
	// @param isTrailer chunked 요청의 trailer 헤더 섹션인지 여부
	// @return size_t 소비한 바이트 수. 개행을 못 찾으면 m_lineBuffer에 누적만
	//         하고 len 전체를 반환, 찾으면 그 줄까지 소비한 바이트 수를 반환.
	//***************************************************************************
	size_t ConsumeLine(const char* data, size_t len, bool isRequestLine, bool isTrailer = false)
	{
		const char* nl = static_cast<const char*>(memchr(data, '\n', len));
		if( !nl )
		{
			m_lineBuffer.append(data, len);
			if( m_lineBuffer.size() > kMaxLineLen ) { m_state = HTTP::EParseState::Error; }
			return len;
		}
		size_t consumed = static_cast<size_t>(nl - data) + 1;
		m_lineBuffer.append(data, consumed - 1); // '\n' 직전까지
		if( !m_lineBuffer.empty() && m_lineBuffer.back() == '\r' )
			m_lineBuffer.pop_back();

		if( isRequestLine )
		{
			HandleRequestLine(m_lineBuffer);
		}
		else if( m_lineBuffer.empty() )
		{
			// 빈 줄 = 헤더(or trailer) 섹션 종료
			if( isTrailer )
				m_state = HTTP::EParseState::Complete;
			else
				OnHeadersComplete();
		}
		else
		{
			HandleHeaderLine(m_lineBuffer);
		}
		m_lineBuffer.clear();
		return consumed;
	}

	//***************************************************************************
	// @brief 요청 라인("GET /path?query HTTP/1.1")을 파싱해 Method/URI/Version을 채웁니다.
	// @param line 개행이 제거된 요청 라인 원문
	//***************************************************************************
	void HandleRequestLine(const std::string& line)
	{
		size_t sp1 = line.find(' ');
		if( sp1 == std::string::npos ) { m_state = HTTP::EParseState::Error; return; }
		size_t sp2 = line.find(' ', sp1 + 1);
		if( sp2 == std::string::npos ) { m_state = HTTP::EParseState::Error; return; }

		m_method.assign(line, 0, sp1);
		m_uri.assign(line, sp1 + 1, sp2 - sp1 - 1);
		m_version.assign(line, sp2 + 1, line.size() - sp2 - 1);

		if( m_method.empty() || m_uri.empty() || m_version.empty() )
		{
			m_state = HTTP::EParseState::Error;
			return;
		}

		// HTTP/1.1 기본 keep-alive, 그 외(HTTP/1.0 등) 기본 close — Connection
		// 헤더를 실제로 확인하기 전까지의 잠정값. 최종값은 OnHeadersComplete()에서 확정.
		m_keepAlive = HTTP::EqualsIgnoreCaseAscii(m_version, "HTTP/1.1");

		m_state = HTTP::EParseState::Headers;
	}

	//***************************************************************************
	// @brief 헤더 한 줄("Key: Value")을 파싱해 m_headers에 추가합니다.
	// @param line 개행이 제거된 헤더 라인 원문
	//***************************************************************************
	void HandleHeaderLine(const std::string& line)
	{
		size_t colon = line.find(':');
		if( colon == std::string::npos ) return; // 관례상 무시 (엄격 모드가 필요하면 Error로 바꿀 것)
		std::string key(line.data(), colon);
		std::string_view value = HTTP::Trim(std::string_view(line).substr(colon + 1));
		m_headers.emplace_back(std::move(key), std::string(value));
	}

	//***************************************************************************
	// @brief 헤더 섹션이 끝났을 때(빈 줄 도달) 호출되어, Transfer-Encoding/
	//        Content-Length/Connection 헤더를 검사해 다음 파싱 상태와 keep-alive
	//        여부를 확정합니다.
	// @details [수정] 가장 먼저 m_headersCompleteCallback을 호출한다 — 본문이
	//          시작되기 전인 이 시점에 호출부가 SetBodyStreamCallback()으로
	//          스트리밍 여부/상한을 결정할 수 있게 하기 위함이다. 그 다음에야
	//          기존 로직(Transfer-Encoding/Content-Length 확인, 크기 상한 검사)이
	//          이어진다 — 크기 상한 검사는 스트리밍 여부에 따라 kMaxBodyLen
	//          또는 m_maxBodyLenOverride 중 적용할 값이 달라지므로, 콜백이
	//          그 값을 정할 기회를 준 "다음"에 검사해야 한다.
	//***************************************************************************
	void OnHeadersComplete()
	{
		if( m_headersCompleteCallback )
			m_headersCompleteCallback(*this);

		std::string_view te = FindHeader("Transfer-Encoding");
		std::string_view cl = FindHeader("Content-Length");
		std::string_view conn = FindHeader("Connection");

		// Connection 헤더가 명시돼 있으면 버전 기본값을 덮어씀.
		if( HTTP::EqualsIgnoreCaseAscii(conn, "close") )
			m_keepAlive = false;
		else if( HTTP::EqualsIgnoreCaseAscii(conn, "keep-alive") )
			m_keepAlive = true;

		if( HTTP::EqualsIgnoreCaseAscii(te, "chunked") )
		{
			m_chunked = true;
			m_state = HTTP::EParseState::ChunkedSize;
			return;
		}

		if( !cl.empty() )
		{
			size_t v = 0;
			auto res = std::from_chars(cl.data(), cl.data() + cl.size(), v);
			if( res.ec == std::errc() )
			{
				m_hasContentLength = true;
				m_contentLength = v;
			}
		}

		// [수정] 스트리밍 모드가 걸려 있으면(m_bodyStreamCallback != nullptr)
		// 본문이 메모리에 안 쌓이므로, maxBodyLenOverride가 지정돼 있으면
		// 그 값을(0이면 기존 kMaxBodyLen을 그대로) 상한으로 쓴다. 스트리밍이
		// 아니면(기존과 동일한 경로) 항상 kMaxBodyLen을 쓴다.
		const size_t effectiveMaxBodyLen =
			(m_bodyStreamCallback && m_maxBodyLenOverride > 0) ? m_maxBodyLenOverride : kMaxBodyLen;

		if( m_hasContentLength && m_contentLength > effectiveMaxBodyLen )
		{
			// 비정상적으로 큰 Content-Length — 메모리 고갈 방지를 위해 즉시 에러 처리.
			m_state = HTTP::EParseState::Error;
			return;
		}

		if( !m_hasContentLength || m_contentLength == 0 )
		{
			// Content-Length가 없거나 0 — 요청은 스펙상 이 경우 body가 없는 게
			// 정상이므로(클래스 설명의 [응답 파서와의 차이점] 참고), 즉시 완료 처리.
			m_state = HTTP::EParseState::Complete;
			return;
		}

		// [수정] 스트리밍 모드면 m_body에 안 쌓이므로 reserve()가 무의미할
		// 뿐더러, m_contentLength가 GB 단위면 reserve() 자체가 오히려 거대한
		// 메모리를 미리 잡아버리는 역효과가 난다 — 스트리밍이 아닐 때만 한다.
		if( !m_bodyStreamCallback )
			m_body.reserve(m_contentLength);

		m_state = HTTP::EParseState::Body;
	}

	//***************************************************************************
	// @brief Content-Length 기준으로 body 바이트를 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @return size_t 실제로 소비한 바이트 수 (m_contentLength 남은 만큼만)
	// @details [수정] m_bodyStreamCallback이 설정돼 있으면 m_body에 누적하는
	//          대신 그 콜백으로 바로 넘긴다 — 진행 상황 추적은 m_body.size()
	//          대신 별도 카운터(m_bodyBytesConsumed)로 한다(스트리밍 모드에서는
	//          m_body가 계속 비어있으므로 size()로는 진행률을 알 수 없다).
	//***************************************************************************
	size_t ConsumeBody(const char* data, size_t len)
	{
		size_t remaining = m_contentLength - m_bodyBytesConsumed;
		size_t take = (std::min)(len, remaining);

		if( m_bodyStreamCallback )
			m_bodyStreamCallback(data, take);
		else
			m_body.append(data, take);

		m_bodyBytesConsumed += take;
		if( m_bodyBytesConsumed >= m_contentLength )
			m_state = HTTP::EParseState::Complete;
		return take;
	}

	//***************************************************************************
	// @brief chunked 인코딩의 청크 크기 라인(16진수)을 파싱합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @return size_t 소비한 바이트 수
	//***************************************************************************
	size_t ConsumeChunkSizeLine(const char* data, size_t len)
	{
		const char* nl = static_cast<const char*>(memchr(data, '\n', len));
		if( !nl )
		{
			m_lineBuffer.append(data, len);
			if( m_lineBuffer.size() > kMaxLineLen ) m_state = HTTP::EParseState::Error;
			return len;
		}
		size_t consumed = static_cast<size_t>(nl - data) + 1;
		m_lineBuffer.append(data, consumed - 1);
		if( !m_lineBuffer.empty() && m_lineBuffer.back() == '\r' )
			m_lineBuffer.pop_back();

		// chunk-extension(";" 이후)은 무시하고 크기(hex)만 파싱
		std::string_view sizeSv(m_lineBuffer);
		size_t semi = sizeSv.find(';');
		if( semi != std::string_view::npos ) sizeSv = sizeSv.substr(0, semi);

		size_t chunkSize = 0;
		auto res = std::from_chars(sizeSv.data(), sizeSv.data() + sizeSv.size(), chunkSize, 16);
		if( res.ec != std::errc() ) { m_state = HTTP::EParseState::Error; return consumed; }

		m_lineBuffer.clear();
		if( chunkSize == 0 )
		{
			m_state = HTTP::EParseState::ChunkedTrailer; // 마지막 청크: trailer 헤더(없으면 빈 줄) 처리로 이동
		}
		else
		{
			m_chunkRemaining = chunkSize;
			m_state = HTTP::EParseState::ChunkedData;
		}
		return consumed;
	}

	//***************************************************************************
	// @brief chunked 인코딩의 청크 데이터 바이트를 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @return size_t 실제로 소비한 바이트 수 (m_chunkRemaining 남은 만큼만)
	// @details chunked는 Content-Length처럼 총량을 미리 알 수 없어 OnHeadersComplete()의
	//          사전 체크가 적용되지 않는다 — 그래서 여기서 누적 크기를 직접
	//          kMaxBodyLen과 비교해 상한을 넘으면 Error로 전환한다(메모리 고갈 방지).
	// @details [수정] ConsumeBody()와 동일한 이유로 스트리밍 콜백을 지원한다.
	//          스트리밍 모드에서는 m_body가 안 쌓이므로 크기 검사도
	//          m_bodyBytesConsumed(누적 처리량)로 하고, 상한도 effectiveMaxBodyLen
	//          (스트리밍이면 override, 아니면 kMaxBodyLen)을 쓴다.
	//***************************************************************************
	size_t ConsumeChunkData(const char* data, size_t len)
	{
		size_t take = (std::min)(len, m_chunkRemaining);

		if( m_bodyStreamCallback )
			m_bodyStreamCallback(data, take);
		else
			m_body.append(data, take);

		m_bodyBytesConsumed += take;
		m_chunkRemaining -= take;

		const size_t effectiveMaxBodyLen =
			(m_bodyStreamCallback && m_maxBodyLenOverride > 0) ? m_maxBodyLenOverride : kMaxBodyLen;

		if( m_bodyBytesConsumed > effectiveMaxBodyLen )
		{
			m_state = HTTP::EParseState::Error;
			return take;
		}

		if( m_chunkRemaining == 0 )
			m_state = HTTP::EParseState::ChunkedCRLF;
		return take;
	}

	//***************************************************************************
	// @brief 청크 데이터 뒤에 오는 CRLF 두 바이트를 소비합니다 (부분 수신 대비 1바이트씩 처리).
	// @param len 입력 가능한 바이트 수
	// @return size_t 실제로 소비한 바이트 수 (최대 2, m_crlfSkipped가 2에 도달할 때까지)
	//***************************************************************************
	size_t ConsumeChunkTrailingCrlf(const char* /*data*/, size_t len)
	{
		size_t consumed = 0;
		while( consumed < len && m_crlfSkipped < 2 )
		{
			++consumed;
			++m_crlfSkipped;
		}
		if( m_crlfSkipped >= 2 )
		{
			m_crlfSkipped = 0;
			m_state = HTTP::EParseState::ChunkedSize;
		}
		return consumed;
	}

private:
	static constexpr size_t kMaxLineLen = 8192; // 헤더 한 줄 상한 (비정상 요청/공격 방어)
	static constexpr size_t kMaxBodyLen = 64 * 1024 * 1024; // body 누적 크기 상한(64MB, 스트리밍 모드가 아닐 때의 기본값) — Content-Length/chunked 둘 다 이 상한을 넘으면 Error(메모리 고갈 방지)

	HTTP::EParseState m_state = HTTP::EParseState::StartLine; // 파싱 진행 상태
	std::string m_lineBuffer;                                             // 개행을 못 찾은 부분 라인의 누적 버퍼 (char 기준)

	std::string m_method;                                        // HTTP 메서드 (char 기준, 예: "GET")
	std::string m_uri;                                           // 요청 라인의 URI 전체(경로+쿼리, char 기준)
	std::string m_version;                                       // HTTP 버전 문자열 (char 기준, 예: "HTTP/1.1")
	std::vector<std::pair<std::string, std::string>> m_headers;  // 파싱된 헤더 목록 (삽입 순서 보존, char 기준)
	std::string m_body;                                          // 지금까지 누적된 body (char 기준, 임의 인코딩 가능) — 스트리밍 모드에서는 계속 비어있음

	bool m_hasContentLength = false; // Content-Length 헤더 존재 여부
	size_t m_contentLength = 0;      // Content-Length 값

	bool m_chunked = false;      // Transfer-Encoding: chunked 여부
	size_t m_chunkRemaining = 0; // 현재 청크에서 아직 안 읽은 바이트 수
	int m_crlfSkipped = 0;       // 청크 데이터 뒤 CRLF 소비 진행도(0~2)

	bool m_keepAlive = false; // 이 요청이 keep-alive로 처리돼야 하는지 (버전 기본값 + Connection 헤더로 확정)
	size_t m_lastFeedConsumed = 0; // 직전 Feed() 호출에서 실제로 소비한 바이트 수 (GetLastFeedConsumed() 참고)

	// [추가] 대용량 본문 스트리밍 지원용 상태.
	size_t m_bodyBytesConsumed = 0;                       // 지금까지 처리한 본문 바이트 수(스트리밍/비스트리밍 공용 진행률 카운터)
	HeadersCompleteCallback m_headersCompleteCallback;    // Reset()이 지우지 않음 — 세션 수명 동안 재사용
	BodyStreamCallback m_bodyStreamCallback;              // Reset()이 지움 — 요청 1건 한정
	size_t m_maxBodyLenOverride = 0;                      // Reset()이 지움 — 0이면 kMaxBodyLen 그대로 사용
};

#endif // ndef UC_HTTPREQUESTPARSER_H