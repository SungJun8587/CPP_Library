
//***************************************************************************
// HttpResponseParser.h : interface for the CHttpResponseParser class.
//
//***************************************************************************

#ifndef UC_HTTPRESPONSEPARSER_H
#define UC_HTTPRESPONSEPARSER_H

#include <Network/HTTP/HttpMessageParser.h>

#include <string>

//***************************************************************************
// @class CHttpResponseParser
// @brief HTTP/1.1 응답을 증분(incremental)으로 파싱하는 상태머신
//
// @details
//      헤더/본문 프레이밍은 CHttpMessageParser가 처리하고, 이 클래스는 상태 라인과 응답 쪽
//      본문 규칙(HEAD/1xx/204/304, "연결 종료까지" 본문, keep-alive 판단)을 담당한다.
//
//      요청 빌더(HttpPacketBuilder.h)와 달리 이 쪽은 "제로카피 뷰"가 아니라 내부에 데이터를
//      복사/누적한다: 응답 바이트는 수신 링버퍼에서 오는데 그 버퍼는 다음 수신 때 재사용되므로,
//      Feed() 호출이 끝난 뒤에도 살아있어야 하는 상태(헤더, 지금까지 읽은 body)는 소유권을
//      가져야 한다.
//
//      [사용 패턴 — 세션의 수신 훅에서]
//      CHttpResponseParser parser;
//      // ... 수신 콜백마다:
//      auto state = parser.Feed(recvData, recvLen);
//      if( state == HTTP::EParseState::Complete ) { /* 콜백/future에 결과 전달 */ }
//      else if( state == HTTP::EParseState::Error ) { /* 커넥션 폐기, 재시도 판단 */ }
//      // 같은 커넥션에서 다음 요청을 보내기 전 반드시 parser.Reset() 호출 (keep-alive 재사용)
//
//      [본문 길이 규칙] RFC 7230 3.3.3에 따라 HEAD 응답과 1xx/204/304 응답은 본문이 없고,
//      Transfer-Encoding(마지막 코딩이 chunked), Content-Length 순으로 본문 길이를 정한다.
//      Transfer-Encoding의 마지막 코딩이 chunked가 아니면 본문은 연결 종료까지이고,
//      Content-Length가 잘못됐거나 서로 다른 값이 중복되면 Error다.
//      둘 다 없을 때: 서버가 연결을 닫는다고 알린 경우(Connection: close, 또는 명시적 keep-alive가
//      없는 HTTP/1.0)에는 본문이 "연결 종료까지"이므로 응답을 Complete로 만들지 않고 종료 신호를
//      기다린다. 호출 측은 상대의 정상 종료(FIN)를 알게 되면 IsReadingUntilClose()가 true인
//      파서에 FinishAtConnectionClose()를 호출해 완결시킨다(CHttpClientCore가 한다). 리셋 등
//      비정상 종료는 본문이 잘렸을 수 있으므로 완결시키지 않는다. 종료 신호도 길이도 없는 모호한
//      응답(keep-alive인데 길이를 주지 않는 비표준 서버)은 빈 본문으로 즉시 Complete 처리한다.
//***************************************************************************
class CHttpResponseParser : public CHttpMessageParser
{
public:
	//***************************************************************************
	// @brief 수신 바이트를 밀어넣습니다. 여러 번 나눠 호출해도 내부 상태로 이어붙여 처리합니다.
	// @return HTTP::EParseState 이 호출 이후의 현재 상태 (Complete/Error면 호출부가 후속 처리)
	//***************************************************************************
	HTTP::EParseState Feed(const char* data, size_t len)
	{
		m_bytesFed += len;
		return CHttpMessageParser::Feed(data, len);
	}

	//***************************************************************************
	// @brief 같은 커넥션(keep-alive)에서 다음 요청/응답을 위해 재사용합니다.
	//***************************************************************************
	void Reset()
	{
		ResetMessageState();
		m_statusCode = 0;
		m_reasonPhrase.clear();
		m_connectionClose = false;
		m_headResponse = false;
		m_timedOut = false;
		m_bytesFed = 0;
		m_http10 = false;
		m_untilClose = false;
	}

	//***************************************************************************
	// @brief 다음에 파싱할 응답이 HEAD 요청에 대한 것임을 알립니다 (Reset() 이후, Feed() 이전에 호출).
	// @details HEAD 응답은 Content-Length/Transfer-Encoding 헤더가 있어도 본문이 없다(RFC 7230 3.3.3).
	//          파서는 요청 메서드를 모르므로 호출 측이 알려줘야 한다.
	//***************************************************************************
	void SetHeadResponse(bool isHead) noexcept { m_headResponse = isHead; }

	//***************************************************************************
	// @brief 응답 상태 코드를 반환합니다 (예: 200, 404).
	//***************************************************************************
	int GetStatusCode() const noexcept { return m_statusCode; }

	//***************************************************************************
	// @brief 요청이 타임아웃(무응답 시간 초과)으로 실패했는지 반환합니다.
	//***************************************************************************
	bool IsTimedOut() const noexcept { return m_timedOut; }

	//***************************************************************************
	// @brief 이 응답을 마지막으로 연결을 재사용하지 못하게 표시합니다 (IsConnectionCloseRequested()가 true가 된다).
	// @details 응답이 완결된 뒤에도 같은 수신에 바이트가 더 붙어 온 경우(서버가 요청 하나에 응답을 둘
	//          보냈거나 응답 뒤에 쓰레기를 붙인 경우)에 호출부가 쓴다. 남은 바이트는 버려지므로 그 연결로
	//          다음 요청을 보내면 뒤늦게 도착하는 나머지가 다음 응답과 섞일 수 있다.
	//***************************************************************************
	void RequestConnectionClose() noexcept { m_connectionClose = true; }

	//***************************************************************************
	// @brief 요청을 타임아웃으로 표시합니다 (CHttpClientCore가 실패 콜백 직전에 호출).
	//***************************************************************************
	void MarkTimedOut() noexcept { m_timedOut = true; }

	//***************************************************************************
	// @brief 본문이 "연결 종료까지"인 응답을 읽는 중인지 반환합니다.
	//***************************************************************************
	bool IsReadingUntilClose() const noexcept { return m_untilClose && m_state == HTTP::EParseState::Body; }

	//***************************************************************************
	// @brief 연결 종료까지가 본문인 응답을 읽던 중 서버가 연결을 정상 종료했을 때 응답을 완결시킵니다.
	//***************************************************************************
	void FinishAtConnectionClose() noexcept
	{
		if( IsReadingUntilClose() )
			m_state = HTTP::EParseState::Complete;
	}

	//***************************************************************************
	// @brief 이 응답으로 지금까지 Feed()에 들어온 총 바이트 수를 반환합니다.
	// @details 0이면 서버로부터 응답 바이트를 하나도 받지 못한 것이다(연결 수준 실패 판단용).
	//***************************************************************************
	size_t BytesFed() const noexcept { return m_bytesFed; }

	//***************************************************************************
	// @brief 상태 메시지(Reason-Phrase)를 바이트 문자열 그대로 반환합니다.
	//***************************************************************************
	const std::string& GetReasonPhrase() const noexcept { return m_reasonPhrase; }

	//***************************************************************************
	// @brief 응답 후 이 커넥션을 재사용하면 안 되는지 반환합니다.
	// @return bool true면 풀에 반납하지 말고 커넥션을 폐기해야 함 — 서버가 "Connection: close"를
	//         명시했거나, 명시적 keep-alive가 없는 HTTP/1.0 응답이거나, 본문 프레이밍이
	//         모호해(Transfer-Encoding과 Content-Length 동시 존재 등) 연결을 닫아야 하는 경우
	//***************************************************************************
	bool IsConnectionCloseRequested() const noexcept { return m_connectionClose; }

protected:
	//***************************************************************************
	// @brief 상태 라인("HTTP/1.1 200 OK")을 파싱해 상태 코드/메시지를 채웁니다.
	//***************************************************************************
	void HandleStartLine(const std::string& line) override
	{
		if( line.compare(0, 5, "HTTP/") != 0 )
		{
			SetError();
			return;
		}

		const size_t sp1 = line.find(' ');
		if( sp1 == std::string::npos ) { SetError(); return; }
		const size_t sp2 = line.find(' ', sp1 + 1);
		const size_t codeEnd = (sp2 == std::string::npos) ? line.size() : sp2;

		// 상태 코드는 정확히 세 자리 숫자다 (RFC 7230 §3.1.2).
		size_t code = 0;
		if( codeEnd - sp1 - 1 != 3 || !HTTP::ParseUnsigned(std::string_view(line).substr(sp1 + 1, 3), code) )
		{
			SetError();
			return;
		}

		m_statusCode = static_cast<int>(code);
		m_http10 = (line.compare(0, 8, "HTTP/1.0") == 0);
		if( sp2 != std::string::npos )
			m_reasonPhrase.assign(line, sp2 + 1, std::string::npos);

		m_state = HTTP::EParseState::Headers;
	}

	//***************************************************************************
	// @brief 헤더 섹션이 끝났을 때 keep-alive 여부와 본문 길이 규칙을 확정해 다음 파싱 상태를 정합니다.
	//***************************************************************************
	void OnHeadersComplete() override
	{
		// 1xx 임시 응답(100 Continue 등, 101 제외)은 최종 응답이 아니다 — 버리고 이어서 오는 다음
		// 응답을 파싱한다. 그대로 두면 임시 응답이 완결로 처리돼 최종 응답이 유실된다.
		if( m_statusCode >= 100 && m_statusCode < 200 && m_statusCode != 101 )
		{
			m_statusCode = 0;
			m_reasonPhrase.clear();
			m_headers.clear();
			m_connectionClose = false;
			m_state = HTTP::EParseState::StartLine;
			return;
		}

		if( HTTP::HeaderHasToken(m_headers, "Connection", "close") )
			m_connectionClose = true;

		// HTTP/1.0 응답은 명시적인 keep-alive가 없으면 응답 후 연결이 닫힌다 — 풀이 재사용하지 않게 한다.
		if( m_http10 && !HTTP::HeaderHasToken(m_headers, "Connection", "keep-alive") )
			m_connectionClose = true;

		// HEAD 응답과 204/304 응답은 헤더에 Content-Length/Transfer-Encoding이 있어도 본문이 없다
		// (RFC 7230 3.3.3). 본문을 기다리면 응답이 영원히 완결되지 않는다.
		if( m_headResponse || m_statusCode == 204 || m_statusCode == 304 )
		{
			m_state = HTTP::EParseState::Complete;
			return;
		}

		const HTTP::SBodyFraming framing = HTTP::ResolveBodyFraming(m_headers, /*rejectBothTeAndCl=*/false);
		switch( framing.kind )
		{
		case HTTP::EBodyFraming::Invalid:
			SetError(); // 본문의 끝을 확정할 수 없다 — 다음 응답과 섞이므로 연결을 폐기해야 한다
			return;

		case HTTP::EBodyFraming::Chunked:
			// Transfer-Encoding과 Content-Length가 함께 온 응답은 요청 스머글링의 징후일 수 있어
			// (RFC 7230 §3.3.3) 이 응답을 읽은 뒤 연결을 닫는다.
			if( !HTTP::FindHeaderValue(m_headers, "Content-Length").empty() )
				m_connectionClose = true;
			m_state = HTTP::EParseState::ChunkedSize;
			return;

		case HTTP::EBodyFraming::UnknownCoding:
			// 마지막 코딩이 chunked가 아니면 본문은 연결 종료까지다.
			m_connectionClose = true;
			m_untilClose = true;
			m_state = HTTP::EParseState::Body;
			return;

		case HTTP::EBodyFraming::None:
			// 본문 길이를 알려주는 헤더가 없다. 서버가 연결을 닫는다고 알린 경우에는 본문이
			// 연결 종료까지이므로 종료 신호를 기다리고, 그렇지 않은 모호한 응답은 빈 본문으로 완결한다.
			if( m_connectionClose )
			{
				m_untilClose = true;
				m_state = HTTP::EParseState::Body;
			}
			else
			{
				m_state = HTTP::EParseState::Complete;
			}
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
			m_state = HTTP::EParseState::Complete; // body 없음 (CL:0)
			return;
		}

		ReserveBody(m_contentLength);
		m_state = HTTP::EParseState::Body;
	}

	//***************************************************************************
	// @brief 본문 바이트를 소비합니다. 연결 종료까지가 본문이면 받은 바이트를 그대로 붙이되 상한을
	//        넘으면 에러 처리하고, 아니면 Content-Length 기준으로 소비합니다.
	//***************************************************************************
	size_t ConsumeBodyBytes(const char* data, size_t len) override
	{
		if( !m_untilClose )
			return CHttpMessageParser::ConsumeBodyBytes(data, len);

		if( m_body.size() + len > MaxBodyLen() )
		{
			SetError();
			return len;
		}
		OnBodyData(data, len);
		return len;
	}

private:
	int m_statusCode = 0;           // 응답 상태 코드
	std::string m_reasonPhrase;     // 상태 메시지 (Reason-Phrase)

	bool m_connectionClose = false; // 응답 후 연결을 재사용하면 안 되는지
	bool m_headResponse = false;    // 이 응답이 HEAD 요청에 대한 것인지 (본문이 없어야 함)
	bool m_http10 = false;          // 상태 라인이 HTTP/1.0인지 (명시적 keep-alive가 없으면 응답 후 연결이 닫힌다)
	bool m_untilClose = false;      // 본문이 연결 종료까지인지
	bool m_timedOut = false;        // 요청이 타임아웃으로 실패했는지 (CHttpClientCore가 실패 콜백 직전에 표시)
	size_t m_bytesFed = 0;          // 이 응답으로 Feed()에 들어온 총 바이트 수 (응답을 하나도 못 받았는지 판단용)
};

#endif // ndef UC_HTTPRESPONSEPARSER_H