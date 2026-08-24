
//***************************************************************************
// HttpResponseParser.h : interface for the CHttpResponseParser class.
//
//***************************************************************************

#ifndef __HTTPRESPONSEPARSER_H__
#define __HTTPRESPONSEPARSER_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <charconv>
#include <algorithm>
#include <cctype>

//***************************************************************************
// @enum EHttpParseState
// @brief CHttpResponseParser의 파싱 진행 상태
//***************************************************************************
enum class EHttpParseState
{
	StatusLine,     // "HTTP/1.1 200 OK\r\n" 파싱 중
	Headers,        // 헤더 라인들 파싱 중
	Body,           // Content-Length 기준 body 수신 중
	ChunkedSize,    // chunked: 다음 청크 크기 라인 대기
	ChunkedData,    // chunked: 청크 데이터 수신 중
	ChunkedCRLF,    // chunked: 청크 데이터 뒤 CRLF 소비
	ChunkedTrailer, // chunked: 마지막 0-size 청크 뒤 trailer 헤더(있으면) 소비
	Complete,       // 응답 하나 완전히 파싱됨
	Error           // 프로토콜 위반 등으로 더 이상 진행 불가 (커넥션 폐기 대상)
};

//***************************************************************************
// 내부 유틸리티 (ASCII 바이트 문자열 <-> 프로젝트 표준 문자열 변환)
//***************************************************************************
namespace http
{
	//***************************************************************************
	// @brief ASCII 바이트 문자열을 프로젝트 표준 문자열(_tstring)로 변환합니다.
	// @param ascii 변환할 ASCII 범위 바이트 문자열
	// @return _tstring 변환된 문자열
	// @details HTTP 헤더 이름/값, 상태 메시지(Reason-Phrase)는 RFC 7230상
	//          사실상 항상 ASCII(가시 문자 + 일부 공백류) 범위이므로, 바이트
	//          값을 그대로 TCHAR로 캐스팅하는 이 방식이 안전합니다. body는
	//          임의의 인코딩(UTF-8 JSON, 바이너리 등)일 수 있어 이 함수로
	//          변환하지 않습니다 — body 처리는 호출부가 Content-Type에 맞게
	//          별도로 수행해야 합니다.
	//***************************************************************************
	inline _tstring ToTString(std::string_view ascii)
	{
		_tstring result;
		result.reserve(ascii.size());
		for( char c : ascii )
			result.push_back(static_cast<TCHAR>(static_cast<unsigned char>(c)));
		return result;
	}

	//***************************************************************************
	// @brief 프로젝트 표준 문자열 뷰(_tstring_view)를 ASCII 바이트 문자열(std::string)로 좁힙니다.
	// @param sv 변환할 ASCII 범위 문자열 뷰
	// @return std::string 변환된 바이트 문자열
	// @details FindHeaderT()가 내부적으로 char 기반 FindHeader()를 재사용하기
	//          위한 역방향 변환. 헤더 이름 조회 용도라 ASCII 범위만 지원합니다.
	//***************************************************************************
	inline std::string ToAsciiString(_tstring_view sv)
	{
		std::string result;
		result.reserve(sv.size());
		for( TCHAR c : sv )
			result.push_back(static_cast<char>(c));
		return result;
	}
}

//***************************************************************************
// @class CHttpResponseParser
// @brief HTTP/1.1 응답을 증분(incremental)으로 파싱하는 상태머신
//
// @details
//      요청 빌더(HttpPacketBuilder.h)와 달리 이 쪽은 "제로카피 뷰"가 아니라
//      내부에 데이터를 복사/누적해야 한다: 응답 바이트는 CRecvBuffer 같은
//      링/시프트 버퍼에서 오는데, 그 버퍼는 다음 Dispatch() 콜에서 덮어써지거나
//      재사용될 수 있어 Feed() 호출이 끝난 뒤에도 살아있어야 하는 상태(헤더,
//      지금까지 읽은 body)는 소유권을 가져야 한다.
//
//      [내부는 char, 조회 결과만 TCHAR] Feed()가 받는 데이터는 소켓에서 그대로
//      온 와이어 바이트라 항상 1바이트 단위(char)로 처리해야 한다 — UNICODE
//      빌드에서 TCHAR=wchar_t로 바꿔버리면 "TCP 스트림이 2바이트 단위로 온다"고
//      잘못 가정하게 되어 파싱이 깨진다(HttpPacketBuilder.h의 UNICODE 빌드
//      주의사항과 동일한 문제). 그래서 내부 파싱/저장(m_headers, m_reasonPhrase
//      등)은 항상 char 기반으로 유지하고, 그 결과를 조회하는 용도로만
//      GetReasonPhraseT()/GetHeadersT()/FindHeaderT() 같은 TCHAR 반환 함수를
//      별도로 제공한다(내부 재계산 없이 조회 시점에 변환). body는 임의의
//      인코딩일 수 있어 TCHAR 변환 함수를 제공하지 않는다 — GetBody()로 받은
//      바이트를 호출부가 Content-Type에 맞게 직접 처리해야 한다.
//
//      [사용 패턴 — 세션의 수신 훅에서]
//      CHttpResponseParser parser;
//      // ... Dispatch() 콜백마다:
//      auto state = parser.Feed(recvData, recvLen);
//      if (state == EHttpParseState::Complete) { /* 콜백/future에 결과 전달 */ }
//      else if (state == EHttpParseState::Error) { /* 커넥션 폐기, 재시도 판단 */ }
//      // 같은 커넥션에서 다음 요청을 보내기 전 반드시 parser.Reset() 호출 (keep-alive 재사용)
//
//      [알려진 제약] Content-Length도 chunked도 없는 응답은 본 구현이 "연결
//      종료까지 body"를 지원하지 않아 body 없음으로 간주하고 즉시 Complete
//      처리한다(OnHeadersComplete() 참고). keep-alive 풀에서 이런 응답을 만나면
//      그 커넥션을 재사용하지 말고 폐기해야 한다 — 상위 계층이 명시적으로
//      처리해야 하는 부분.
//***************************************************************************
class CHttpResponseParser
{
public:
	//***************************************************************************
	// @brief CHttpResponseParser 생성자
	// @details 헤더 목록 벡터를 미리 reserve해 초기 파싱 중 재할당을 줄인다.
	//***************************************************************************
	CHttpResponseParser() { m_headers.reserve(16); }

	//***************************************************************************
	// @brief 수신 바이트를 밀어넣습니다. 여러 번 나눠 호출해도 내부 상태로 이어붙여 처리합니다.
	// @param data 수신 바이트 포인터 (와이어 바이트, 항상 char 기준)
	// @param len data의 길이
	// @return EHttpParseState 이 호출 이후의 현재 상태 (Complete/Error면 호출부가 후속 처리)
	//***************************************************************************
	EHttpParseState Feed(const char* data, size_t len)
	{
		size_t pos = 0;
		while( pos < len && m_state != EHttpParseState::Complete && m_state != EHttpParseState::Error )
		{
			switch( m_state )
			{
			case EHttpParseState::StatusLine:
				pos += ConsumeLine(data + pos, len - pos, /*isStatusLine=*/true);
				break;
			case EHttpParseState::Headers:
				pos += ConsumeLine(data + pos, len - pos, /*isStatusLine=*/false);
				break;
			case EHttpParseState::Body:
				pos += ConsumeBody(data + pos, len - pos);
				break;
			case EHttpParseState::ChunkedSize:
				pos += ConsumeChunkSizeLine(data + pos, len - pos);
				break;
			case EHttpParseState::ChunkedData:
				pos += ConsumeChunkData(data + pos, len - pos);
				break;
			case EHttpParseState::ChunkedCRLF:
				pos += ConsumeChunkTrailingCrlf(data + pos, len - pos);
				break;
			case EHttpParseState::ChunkedTrailer:
				pos += ConsumeLine(data + pos, len - pos, /*isStatusLine=*/false, /*isTrailer=*/true);
				break;
			default:
				break;
			}
		}
		return m_state;
	}

	//***************************************************************************
	// @brief 같은 커넥션(keep-alive)에서 다음 요청/응답을 위해 재사용합니다.
	//***************************************************************************
	void Reset()
	{
		m_state = EHttpParseState::StatusLine;
		m_lineBuffer.clear();
		m_statusCode = 0;
		m_reasonPhrase.clear();
		m_headers.clear();
		m_body.clear();
		m_contentLength = 0;
		m_hasContentLength = false;
		m_chunked = false;
		m_chunkRemaining = 0;
		m_connectionClose = false;
	}

	//***************************************************************************
	// @brief 현재 파싱 진행 상태를 반환합니다.
	//***************************************************************************
	EHttpParseState GetState() const noexcept { return m_state; }

	//***************************************************************************
	// @brief 응답 상태 코드를 반환합니다 (예: 200, 404).
	//***************************************************************************
	int GetStatusCode() const noexcept { return m_statusCode; }

	//***************************************************************************
	// @brief 상태 메시지(Reason-Phrase)를 바이트 문자열 그대로 반환합니다.
	//***************************************************************************
	const std::string& GetReasonPhrase() const noexcept { return m_reasonPhrase; }

	//***************************************************************************
	// @brief 상태 메시지(Reason-Phrase)를 프로젝트 표준 문자열(_tstring)로 변환해 반환합니다.
	//***************************************************************************
	_tstring GetReasonPhraseT() const { return http::ToTString(m_reasonPhrase); }

	//***************************************************************************
	// @brief 파싱된 헤더 목록을 바이트 문자열 쌍 그대로 반환합니다 (삽입 순서 보존).
	//***************************************************************************
	const std::vector<std::pair<std::string, std::string>>& GetHeaders() const noexcept { return m_headers; }

	//***************************************************************************
	// @brief 파싱된 헤더 목록을 프로젝트 표준 문자열(_tstring) 쌍으로 변환해 반환합니다.
	// @return std::vector<std::pair<_tstring, _tstring>> 헤더 (이름, 값) 목록, 삽입 순서 보존
	//***************************************************************************
	std::vector<std::pair<_tstring, _tstring>> GetHeadersT() const
	{
		std::vector<std::pair<_tstring, _tstring>> result;
		result.reserve(m_headers.size());
		for( const auto& [k, v] : m_headers )
			result.emplace_back(http::ToTString(k), http::ToTString(v));
		return result;
	}

	//***************************************************************************
	// @brief 지금까지 누적된 body를 바이트 그대로 반환합니다.
	// @details body는 임의의 인코딩(UTF-8 JSON, 바이너리 등)일 수 있어 TCHAR
	//          변환을 제공하지 않는다 — Content-Type에 맞는 변환은 호출부 책임.
	//***************************************************************************
	const std::string& GetBody() const noexcept { return m_body; }

	//***************************************************************************
	// @brief 서버가 "Connection: close"를 명시했는지 반환합니다.
	// @return bool true면 응답 완료 후 풀에 반납하지 말고 커넥션을 폐기해야 함
	//***************************************************************************
	bool IsConnectionCloseRequested() const noexcept { return m_connectionClose; }

	//***************************************************************************
	// @brief 헤더를 이름으로 찾습니다 (대소문자 무시, ASCII 바이트 문자열 기준).
	// @param key 찾을 헤더 이름
	// @return std::string_view 찾은 헤더 값(원본 버퍼를 가리키는 뷰). 없으면 빈 뷰.
	//***************************************************************************
	std::string_view FindHeader(std::string_view key) const noexcept
	{
		for( const auto& [k, v] : m_headers )
			if( EqualsIgnoreCaseAscii(k, key) )
				return v;
		return {};
	}

	//***************************************************************************
	// @brief 헤더를 이름으로 찾아 프로젝트 표준 문자열(_tstring)로 반환합니다 (대소문자 무시).
	// @param key 찾을 헤더 이름 (TCHAR 문자열)
	// @return _tstring 찾은 헤더 값. 없으면 빈 문자열.
	//***************************************************************************
	_tstring FindHeaderT(_tstring_view key) const
	{
		return http::ToTString(FindHeader(http::ToAsciiString(key)));
	}

private:
	//***************************************************************************
	// @brief 대소문자 무시 비교 (헤더 이름은 RFC 7230 기준 대소문자 무관, ASCII 범위만 처리).
	//***************************************************************************
	static bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept
	{
		if( a.size() != b.size() ) return false;
		for( size_t i = 0; i < a.size(); ++i )
		{
			char ca = a[i], cb = b[i];
			if( ca >= 'A' && ca <= 'Z' ) ca += 32;
			if( cb >= 'A' && cb <= 'Z' ) cb += 32;
			if( ca != cb ) return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief 문자열 앞뒤의 공백(스페이스/탭)을 제거합니다.
	//***************************************************************************
	static std::string_view Trim(std::string_view sv) noexcept
	{
		while( !sv.empty() && (sv.front() == ' ' || sv.front() == '\t') ) sv.remove_prefix(1);
		while( !sv.empty() && (sv.back() == ' ' || sv.back() == '\t') ) sv.remove_suffix(1);
		return sv;
	}

	//***************************************************************************
	// @brief data 안에서 개행(\n)을 찾아 한 줄을 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @param isStatusLine 상태 라인 파싱 중인지 여부 (true면 HandleStatusLine으로)
	// @param isTrailer chunked 응답의 trailer 헤더 섹션인지 여부
	// @return size_t 소비한 바이트 수. 개행을 못 찾으면 m_lineBuffer에 누적만
	//         하고 len 전체를 반환, 찾으면 그 줄까지 소비한 바이트 수를 반환.
	//***************************************************************************
	size_t ConsumeLine(const char* data, size_t len, bool isStatusLine, bool isTrailer = false)
	{
		const char* nl = static_cast<const char*>(memchr(data, '\n', len));
		if( !nl )
		{
			m_lineBuffer.append(data, len);
			if( m_lineBuffer.size() > kMaxLineLen ) { m_state = EHttpParseState::Error; }
			return len;
		}
		size_t consumed = static_cast<size_t>(nl - data) + 1;
		m_lineBuffer.append(data, consumed - 1); // '\n' 직전까지
		if( !m_lineBuffer.empty() && m_lineBuffer.back() == '\r' )
			m_lineBuffer.pop_back();

		if( isStatusLine )
		{
			HandleStatusLine(m_lineBuffer);
		}
		else if( m_lineBuffer.empty() )
		{
			// 빈 줄 = 헤더(or trailer) 섹션 종료
			if( isTrailer )
				m_state = EHttpParseState::Complete;
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
	// @brief 상태 라인("HTTP/1.1 200 OK")을 파싱해 상태 코드/메시지를 채웁니다.
	// @param line 개행이 제거된 상태 라인 원문
	//***************************************************************************
	void HandleStatusLine(const std::string& line)
	{
		// "HTTP/1.1 200 OK"
		size_t sp1 = line.find(' ');
		if( sp1 == std::string::npos ) { m_state = EHttpParseState::Error; return; }
		size_t sp2 = line.find(' ', sp1 + 1);
		std::string_view codeSv(line.data() + sp1 + 1, (sp2 == std::string::npos ? line.size() : sp2) - sp1 - 1);

		int code = 0;
		auto res = std::from_chars(codeSv.data(), codeSv.data() + codeSv.size(), code);
		if( res.ec != std::errc() ) { m_state = EHttpParseState::Error; return; }
		m_statusCode = code;
		if( sp2 != std::string::npos )
			m_reasonPhrase.assign(line.data() + sp2 + 1, line.size() - sp2 - 1);

		m_state = EHttpParseState::Headers;
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
		std::string_view value = Trim(std::string_view(line).substr(colon + 1));
		m_headers.emplace_back(std::move(key), std::string(value));
	}

	//***************************************************************************
	// @brief 헤더 섹션이 끝났을 때(빈 줄 도달) 호출되어, Transfer-Encoding/
	//        Content-Length/Connection 헤더를 검사해 다음 파싱 상태를 결정합니다.
	//***************************************************************************
	void OnHeadersComplete()
	{
		std::string_view te = FindHeader("Transfer-Encoding");
		std::string_view cl = FindHeader("Content-Length");
		std::string_view conn = FindHeader("Connection");

		if( EqualsIgnoreCaseAscii(conn, "close") )
			m_connectionClose = true;

		if( EqualsIgnoreCaseAscii(te, "chunked") )
		{
			m_chunked = true;
			m_state = EHttpParseState::ChunkedSize;
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

		if( m_hasContentLength && m_contentLength == 0 )
		{
			m_state = EHttpParseState::Complete; // body 없음 (예: 204, 또는 CL:0)
			return;
		}
		if( !m_hasContentLength )
		{
			// Content-Length도 chunked도 없음: 클래스 설명의 [알려진 제약] 참고 —
			// "연결 종료까지 body"는 지원하지 않고 body 없음으로 간주해 종료 처리.
			m_state = EHttpParseState::Complete;
			return;
		}
		m_state = EHttpParseState::Body;
	}

	//***************************************************************************
	// @brief Content-Length 기준으로 body 바이트를 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @return size_t 실제로 소비한 바이트 수 (m_contentLength 남은 만큼만)
	//***************************************************************************
	size_t ConsumeBody(const char* data, size_t len)
	{
		size_t remaining = m_contentLength - m_body.size();
		size_t take = (std::min)(len, remaining);
		m_body.append(data, take);
		if( m_body.size() >= m_contentLength )
			m_state = EHttpParseState::Complete;
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
			if( m_lineBuffer.size() > kMaxLineLen ) m_state = EHttpParseState::Error;
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
		if( res.ec != std::errc() ) { m_state = EHttpParseState::Error; return consumed; }

		m_lineBuffer.clear();
		if( chunkSize == 0 )
		{
			m_state = EHttpParseState::ChunkedTrailer; // 마지막 청크: trailer 헤더(없으면 빈 줄) 처리로 이동
		}
		else
		{
			m_chunkRemaining = chunkSize;
			m_state = EHttpParseState::ChunkedData;
		}
		return consumed;
	}

	//***************************************************************************
	// @brief chunked 인코딩의 청크 데이터 바이트를 소비합니다.
	// @param data 입력 바이트 포인터
	// @param len data의 길이
	// @return size_t 실제로 소비한 바이트 수 (m_chunkRemaining 남은 만큼만)
	//***************************************************************************
	size_t ConsumeChunkData(const char* data, size_t len)
	{
		size_t take = (std::min)(len, m_chunkRemaining);
		m_body.append(data, take);
		m_chunkRemaining -= take;
		if( m_chunkRemaining == 0 )
			m_state = EHttpParseState::ChunkedCRLF;
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
			m_state = EHttpParseState::ChunkedSize;
		}
		return consumed;
	}

private:
	static constexpr size_t kMaxLineLen = 8192; // 헤더 한 줄 상한 (비정상 응답/공격 방어)

	EHttpParseState m_state = EHttpParseState::StatusLine; // 파싱 진행 상태
	std::string m_lineBuffer;                              // 개행을 못 찾은 부분 라인의 누적 버퍼 (char 기준)

	int m_statusCode = 0;                                        // 응답 상태 코드
	std::string m_reasonPhrase;                                  // 상태 메시지 (Reason-Phrase, char 기준)
	std::vector<std::pair<std::string, std::string>> m_headers;  // 파싱된 헤더 목록 (삽입 순서 보존, char 기준)
	std::string m_body;                                          // 지금까지 누적된 body (char 기준, 임의 인코딩 가능)

	bool m_hasContentLength = false; // Content-Length 헤더 존재 여부
	size_t m_contentLength = 0;      // Content-Length 값

	bool m_chunked = false;      // Transfer-Encoding: chunked 여부
	size_t m_chunkRemaining = 0; // 현재 청크에서 아직 안 읽은 바이트 수
	int m_crlfSkipped = 0;       // 청크 데이터 뒤 CRLF 소비 진행도(0~2)

	bool m_connectionClose = false; // "Connection: close" 응답 헤더 존재 여부
};

#endif // ndef __HTTPRESPONSEPARSER_H__