
//***************************************************************************
// HttpRequestParser.h : interface for the CHttpRequestParser class.
//
//***************************************************************************

#ifndef UC_HTTPREQUESTPARSER_H
#define UC_HTTPREQUESTPARSER_H

#include <Network/HTTP/HttpMessageParser.h>

#include <functional>
#include <string>
#include <string_view>

//***************************************************************************
// @class CHttpRequestParser
// @brief HTTP/1.x 요청을 증분(incremental)으로 파싱하는 상태머신 (CHttpResponseParser의 서버 측 대응)
//
// @details
//      헤더/본문 프레이밍은 CHttpMessageParser가 처리하고, 이 클래스는 요청 라인
//      ("METHOD URI VERSION")과 요청 쪽 본문 규칙을 담당한다.
//
//      [응답 파서와의 차이점]
//      - 상태 라인 대신 요청 라인을 파싱해 Method/URI/Version을 채운다. URI는 경로와 쿼리
//        문자열을 분리해 조회할 수 있다(GetPath()/GetQueryString(), '?' 기준, 제로카피 뷰).
//      - Content-Length도 chunked도 없는 요청은 본문이 없는 요청이다(RFC 7230 §3.3.3) —
//        요청에는 "연결 종료까지 본문"이라는 개념이 없으므로 즉시 Complete로 처리한다.
//      - Transfer-Encoding의 마지막 코딩이 chunked가 아니거나, Transfer-Encoding과
//        Content-Length가 함께 오거나, Content-Length가 잘못된 요청은 Error다.
//      - IsKeepAlive()는 HTTP 버전 + Connection 헤더를 함께 고려한다(HTTP/1.1은 "close"가
//        없으면 keep-alive, HTTP/1.0은 "keep-alive"가 없으면 close — RFC 7230 §6.3).
//
//      [본문 처리 방식 — 누적 또는 스트리밍]
//      기본은 본문을 m_body에 누적하고 kMaxBodyLen(64MB)을 넘으면 Error로 처리한다. 대용량
//      업로드(수 GB)는 메모리에 올릴 수 없으므로 두 개의 선택 콜백으로 스트리밍할 수 있다.
//        - SetHeadersCompleteCallback(): 헤더 파싱이 끝나 본문이 시작되기 직전에 호출된다.
//          메서드/경로/Content-Type을 보고 스트리밍 여부를 정할 수 있는 유일한 시점이다.
//        - SetBodyStreamCallback(): 본문 바이트를 m_body에 쌓는 대신 이 콜백으로 흘려보낸다
//          (GetBody()는 비어 있음). maxBodyLenOverride가 0보다 크면 이번 요청에 한해
//          kMaxBodyLen 대신 그 값이 상한이 된다.
//      두 콜백의 수명이 다르다: HeadersCompleteCallback은 세션 생성 때 한 번 걸어 계속 재사용하므로
//      Reset()이 지우지 않고, BodyStreamCallback/maxBodyLenOverride는 요청 하나에 대한 일회성
//      결정이라 Reset()이 지운다(안 지우면 다음 요청의 본문까지 스트리밍되는 사고가 난다).
//
//      [사용 패턴 — 서버 세션의 수신 훅에서]
//      CHttpRequestParser parser;
//      // ... Dispatch() 콜백마다:
//      auto state = parser.Feed(recvData, recvLen);
//      if( state == HTTP::EParseState::Complete ) { /* 요청 처리 -> 응답 전송 */ }
//      else if( state == HTTP::EParseState::Error ) { /* 400 Bad Request 등, 커넥션 폐기 */ }
//      // 같은 커넥션에서 다음 요청을 받기 전 반드시 parser.Reset() 호출 (keep-alive 재사용)
//
//      [사용 패턴 — 대용량 본문 스트리밍]
//      parser.SetHeadersCompleteCallback([](CHttpRequestParser& p) {
//          if( p.GetMethod() == "POST" && p.GetPath() == "/upload" )
//          {
//              p.SetBodyStreamCallback(
//                  [](const char* data, size_t len) { /* 파일에 바로 쓰기 등 */ },
//                  4LL * 1024 * 1024 * 1024 /* 4GB 상한 */);
//          }
//      });
//***************************************************************************
class CHttpRequestParser : public CHttpMessageParser
{
public:
	//***************************************************************************
	// @brief 본문 바이트가 도착할 때마다 호출되는 콜백 — 스트리밍 모드에서만 쓰인다.
	//***************************************************************************
	using BodyStreamCallback = std::function<void(const char* data, size_t len)>;

	//***************************************************************************
	// @brief 헤더 파싱이 끝나는 시점(본문이 시작되기 직전)에 호출되는 콜백.
	//        이 안에서 SetBodyStreamCallback()을 걸면 이번 요청의 본문 처리 방식을
	//        스트리밍으로 바꿀 수 있다.
	//***************************************************************************
	using HeadersCompleteCallback = std::function<void(CHttpRequestParser&)>;

	//***************************************************************************
	// @brief 같은 커넥션(keep-alive)에서 다음 요청을 위해 재사용합니다.
	// @details HeadersCompleteCallback은 유지하고, 요청 하나에 한정된 BodyStreamCallback/
	//          maxBodyLenOverride는 지운다(클래스 설명 참고).
	//***************************************************************************
	void Reset()
	{
		ResetMessageState();
		m_method.clear();
		m_uri.clear();
		m_version.clear();
		m_keepAlive = false;
		m_bodyStreamCallback = nullptr;
		m_maxBodyLenOverride = 0;
	}

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
		const size_t q = m_uri.find('?');
		return std::string_view(m_uri).substr(0, q);
	}

	//***************************************************************************
	// @brief URI에서 쿼리 문자열 부분만 반환합니다 (제로카피 뷰, '?' 제외).
	// @return std::string_view '?' 다음부터 끝까지. '?'가 없으면 빈 뷰.
	//***************************************************************************
	std::string_view GetQueryString() const noexcept
	{
		const size_t q = m_uri.find('?');
		if( q == std::string::npos ) return {};
		return std::string_view(m_uri).substr(q + 1);
	}

	//***************************************************************************
	// @brief HTTP 버전 문자열을 바이트 문자열 그대로 반환합니다 (예: "HTTP/1.1").
	//***************************************************************************
	const std::string& GetVersion() const noexcept { return m_version; }

	//***************************************************************************
	// @brief 이 요청이 keep-alive로 처리돼야 하는지 반환합니다.
	// @return bool true면 응답 전송 후 커넥션을 유지, false면 응답 후 닫아야 함.
	//***************************************************************************
	bool IsKeepAlive() const noexcept { return m_keepAlive; }

	//***************************************************************************
	// @brief 헤더 파싱 완료 시점 콜백을 등록합니다. Reset()으로 지워지지 않는다 —
	//        보통 세션이 살아있는 동안 계속 재사용한다.
	//***************************************************************************
	void SetHeadersCompleteCallback(HeadersCompleteCallback cb)
	{
		m_headersCompleteCallback = std::move(cb);
	}

	//***************************************************************************
	// @brief 지금 파싱 중인 요청 하나에 한해, 본문을 m_body에 누적하는 대신 콜백으로
	//        그때그때 넘기도록 전환합니다.
	// @details 반드시 HeadersCompleteCallback 안에서(본문이 시작되기 전에) 호출해야
	//          의미가 있다 — 본문이 이미 m_body에 쌓인 뒤의 전환은 지원하지 않는다.
	//          Reset() 호출 시 자동으로 해제된다.
	// @param onBodyChunk 본문 바이트가 도착할 때마다(여러 번) 호출된다.
	// @param maxBodyLenOverride 0보다 크면 이번 요청에 한해 kMaxBodyLen 대신 이 값을
	//        상한으로 쓴다. 스트리밍 중에는 메모리에 쌓이지 않으므로 수 GB로 줘도 안전하다.
	//        0이면 기존 kMaxBodyLen을 그대로 적용한다.
	//***************************************************************************
	void SetBodyStreamCallback(BodyStreamCallback onBodyChunk, size_t maxBodyLenOverride = 0)
	{
		m_bodyStreamCallback = std::move(onBodyChunk);
		m_maxBodyLenOverride = maxBodyLenOverride;
	}

protected:
	//***************************************************************************
	// @brief 요청 라인("GET /path?query HTTP/1.1")을 파싱해 Method/URI/Version을 채웁니다.
	//***************************************************************************
	void HandleStartLine(const std::string& line) override
	{
		const size_t sp1 = line.find(' ');
		if( sp1 == std::string::npos ) { SetError(); return; }
		const size_t sp2 = line.find(' ', sp1 + 1);
		if( sp2 == std::string::npos ) { SetError(); return; }

		m_method.assign(line, 0, sp1);
		m_uri.assign(line, sp1 + 1, sp2 - sp1 - 1);
		m_version.assign(line, sp2 + 1, line.size() - sp2 - 1);

		if( m_method.empty() || m_uri.empty() || m_version.compare(0, 5, "HTTP/") != 0 )
		{
			SetError();
			return;
		}

		// HTTP/1.1은 기본 keep-alive, 그 외(HTTP/1.0 등)는 기본 close — Connection 헤더를 확인하기
		// 전의 잠정값이며 최종값은 OnHeadersComplete()에서 확정한다.
		m_keepAlive = HTTP::EqualsIgnoreCaseAscii(m_version, "HTTP/1.1");

		m_state = HTTP::EParseState::Headers;
	}

	//***************************************************************************
	// @brief 헤더 섹션이 끝났을 때 호출되어 keep-alive 여부와 본문 길이 규칙을 확정합니다.
	// @details 가장 먼저 HeadersCompleteCallback을 호출한다 — 본문이 시작되기 전인 이 시점에
	//          호출부가 스트리밍 여부/상한을 정할 수 있어야 하고, 크기 상한 검사는 그 결정을
	//          반영해야 하기 때문이다.
	//***************************************************************************
	void OnHeadersComplete() override
	{
		if( m_headersCompleteCallback )
			m_headersCompleteCallback(*this);

		// 명시된 Connection 헤더가 버전 기본값을 덮어쓴다 ("keep-alive, Upgrade" 같은 목록도 처리).
		if( HTTP::HeaderHasToken(m_headers, "Connection", "close") )
			m_keepAlive = false;
		else if( HTTP::HeaderHasToken(m_headers, "Connection", "keep-alive") )
			m_keepAlive = true;

		const HTTP::SBodyFraming framing = HTTP::ResolveBodyFraming(m_headers, /*rejectBothTeAndCl=*/true);
		switch( framing.kind )
		{
		case HTTP::EBodyFraming::Chunked:
			m_state = HTTP::EParseState::ChunkedSize;
			return;

		case HTTP::EBodyFraming::UnknownCoding:
		case HTTP::EBodyFraming::Invalid:
			SetError(); // 본문의 끝을 확정할 수 없다 — 다음 요청과 섞이므로 연결을 폐기해야 한다
			return;

		case HTTP::EBodyFraming::None:
			m_state = HTTP::EParseState::Complete; // 요청은 길이 헤더가 없으면 본문이 없다
			return;

		case HTTP::EBodyFraming::ContentLength:
			break;
		}

		m_contentLength = framing.contentLength;
		if( m_contentLength > MaxBodyLen() )
		{
			SetError(); // 비정상적으로 큰 Content-Length — 메모리 고갈 방지를 위해 즉시 에러 처리
			return;
		}

		if( m_contentLength == 0 )
		{
			m_state = HTTP::EParseState::Complete;
			return;
		}

		// 스트리밍 모드는 m_body에 쌓지 않으므로 미리 잡을 필요가 없다.
		if( !m_bodyStreamCallback )
			ReserveBody(m_contentLength);

		m_state = HTTP::EParseState::Body;
	}

	//***************************************************************************
	// @brief 본문 바이트를 스트리밍 콜백 또는 m_body로 보냅니다.
	//***************************************************************************
	void OnBodyData(const char* data, size_t len) override
	{
		if( m_bodyStreamCallback )
			m_bodyStreamCallback(data, len);
		else
			m_body.append(data, len);
	}

	//***************************************************************************
	// @brief 스트리밍 중이고 override가 지정됐으면 그 값, 아니면 kMaxBodyLen을 상한으로 씁니다.
	//***************************************************************************
	size_t MaxBodyLen() const noexcept override
	{
		return (m_bodyStreamCallback && m_maxBodyLenOverride > 0) ? m_maxBodyLenOverride : HTTP::kMaxBodyLen;
	}

private:
	std::string m_method;   // HTTP 메서드 (예: "GET")
	std::string m_uri;      // 요청 라인의 URI 전체(경로+쿼리)
	std::string m_version;  // HTTP 버전 문자열 (예: "HTTP/1.1")

	bool m_keepAlive = false; // 이 요청이 keep-alive로 처리돼야 하는지 (버전 기본값 + Connection 헤더로 확정)

	HeadersCompleteCallback m_headersCompleteCallback;  // Reset()이 지우지 않음 — 세션 수명 동안 재사용
	BodyStreamCallback m_bodyStreamCallback;            // Reset()이 지움 — 요청 1건 한정
	size_t m_maxBodyLenOverride = 0;                    // Reset()이 지움 — 0이면 kMaxBodyLen 그대로 사용
};

#endif // ndef UC_HTTPREQUESTPARSER_H