
//***************************************************************************
// HttpMessageParser.h : interface for the CHttpMessageParser base class.
//
//***************************************************************************

#ifndef UC_HTTPMESSAGEPARSER_H
#define UC_HTTPMESSAGEPARSER_H

#include <Network/HTTP/HttpParseUtil.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

//***************************************************************************
// @class CHttpMessageParser
// @brief CHttpRequestParser/CHttpResponseParser가 공유하는 증분(incremental) 파싱 상태머신의 뼈대
//
// @details
//      HTTP/1.x 요청과 응답은 start-line만 다를 뿐, 그 뒤의 헤더 섹션 / Content-Length 본문 /
//      chunked 본문 / trailer 처리는 완전히 같다. 이 클래스가 그 공통 부분(상태 전이, 줄 단위
//      소비, 청크 처리, 상한 검사)을 모두 구현하고, 파생 클래스는 다음 훅만 채운다.
//        - HandleStartLine()  : 요청 라인 / 상태 라인 해석
//        - OnHeadersComplete(): 헤더가 끝났을 때 본문 길이 규칙을 판정해 다음 상태를 결정
//        - OnBodyData()       : 본문 바이트의 행선지(기본은 m_body에 누적)
//        - MaxBodyLen()       : 이번 메시지에 적용할 본문 상한
//        - ConsumeBodyBytes() : Content-Length 본문 소비(응답 파서는 "연결 종료까지" 본문도 처리)
//      훅은 줄/청크 단위로만 호출되므로 가상 호출 비용은 무시할 수 있다.
//
//      [char 전용] 입력은 소켓에서 온 와이어 바이트라 항상 1바이트 단위(char)로 처리한다.
//      TCHAR(UNICODE 빌드에서 wchar_t) 기반으로 바꾸면 "TCP 스트림이 2바이트 단위로 온다"고
//      가정하게 되어 파싱이 깨지므로, TCHAR 문자열이 필요한 호출부는 GetBody()/GetHeaders()
//      결과를 받아 그 경계에서 변환한다.
//
//      [메시지 경계 안전성] 헤더 이름 앞/뒤 공백, 잘못된 Content-Length, 서로 다른 Content-Length
//      중복, chunked 뒤 CRLF 불일치는 모두 Error로 처리한다 — 이런 입력을 관대하게 넘기면 본문의
//      끝을 파서와 상대가 다르게 해석해 다음 메시지가 어긋나기 때문이다. trailer 필드는 헤더
//      목록에 합치지 않고 버린다(RFC 7230 §4.1.2).
//***************************************************************************
class CHttpMessageParser
{
public:
	virtual ~CHttpMessageParser() = default;

	//***************************************************************************
	// @brief 수신 바이트를 밀어넣습니다. 여러 번 나눠 호출해도 내부 상태로 이어붙여 처리합니다.
	// @param data 수신 바이트 포인터 (와이어 바이트, 항상 char 기준)
	// @param len data의 길이
	// @return HTTP::EParseState 이 호출 이후의 현재 상태 (Complete/Error면 호출부가 후속 처리)
	// @details Complete/Error로 루프가 조기 종료되면 소비량이 len보다 작을 수 있다(파이프라이닝된
	//          다음 메시지의 선두 바이트가 같은 버퍼에 이어 붙어 온 경우). 실제 소비량은
	//          GetLastFeedConsumed()로 알 수 있다.
	//***************************************************************************
	HTTP::EParseState Feed(const char* data, size_t len)
	{
		size_t pos = 0;
		while( pos < len && m_state != HTTP::EParseState::Complete && m_state != HTTP::EParseState::Error )
		{
			switch( m_state )
			{
			case HTTP::EParseState::StartLine:
			case HTTP::EParseState::Headers:
			case HTTP::EParseState::ChunkedTrailer:
				pos += ConsumeLine(data + pos, len - pos);
				break;
			case HTTP::EParseState::Body:
				pos += ConsumeBodyBytes(data + pos, len - pos);
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
			default:
				break;
			}
		}

		m_lastFeedConsumed = pos;
		return m_state;
	}

	//***************************************************************************
	// @brief 직전 Feed() 호출에서 실제로 소비된 바이트 수를 반환합니다.
	// @details Feed()가 Complete를 반환했을 때 (len - 이 값)이 다음 메시지로 넘겨야 할 남은 바이트다.
	//***************************************************************************
	size_t GetLastFeedConsumed() const noexcept { return m_lastFeedConsumed; }

	//***************************************************************************
	// @brief 현재 파싱 진행 상태를 반환합니다.
	//***************************************************************************
	HTTP::EParseState GetState() const noexcept { return m_state; }

	//***************************************************************************
	// @brief 파싱된 헤더 목록을 바이트 문자열 쌍 그대로 반환합니다 (삽입 순서 보존).
	//***************************************************************************
	const HTTP::HeaderList& GetHeaders() const noexcept { return m_headers; }

	//***************************************************************************
	// @brief 지금까지 누적된 body를 바이트 그대로 반환합니다.
	// @details body는 임의의 인코딩(UTF-8 JSON, 바이너리 등)일 수 있어 Content-Type에 맞는
	//          변환은 호출부 책임.
	//***************************************************************************
	const std::string& GetBody() const noexcept { return m_body; }

	//***************************************************************************
	// @brief 누적된 body를 복사 없이 꺼냅니다 (호출 후 이 파서의 body는 비게 된다).
	// @details 완료 콜백 안에서 응답을 소유 값으로 옮길 때 쓴다 — 큰 본문의 복사를 피한다.
	//          이 호출 이후에는 Reset() 전까지 GetBody()를 쓰지 않는다.
	//***************************************************************************
	std::string TakeBody() noexcept
	{
		std::string body = std::move(m_body);
		m_body.clear();
		return body;
	}

	//***************************************************************************
	// @brief 파싱된 헤더 목록을 복사 없이 꺼냅니다 (호출 후 이 파서의 헤더 목록은 비게 된다).
	//***************************************************************************
	HTTP::HeaderList TakeHeaders() noexcept
	{
		HTTP::HeaderList headers = std::move(m_headers);
		m_headers.clear();
		return headers;
	}

	//***************************************************************************
	// @brief 헤더를 이름으로 찾습니다 (대소문자 무시, ASCII 바이트 문자열 기준).
	// @param key 찾을 헤더 이름
	// @return std::string_view 찾은 첫 헤더 값(이 파서의 내부 버퍼를 가리키는 뷰). 없으면 빈 뷰.
	//***************************************************************************
	std::string_view FindHeader(std::string_view key) const noexcept
	{
		return HTTP::FindHeaderValue(m_headers, key);
	}

protected:
	CHttpMessageParser() { m_headers.reserve(16); }

	//***************************************************************************
	// @brief 같은 커넥션(keep-alive)에서 다음 메시지를 위해 공통 상태를 초기화합니다.
	//        파생 클래스의 Reset()이 자기 멤버를 초기화한 뒤 호출한다.
	//***************************************************************************
	void ResetMessageState()
	{
		m_state = HTTP::EParseState::StartLine;
		m_lineBuffer.clear();
		m_headers.clear();
		m_body.clear();
		m_contentLength = 0;
		m_chunkRemaining = 0;
		m_crlfProgress = 0;
		m_bodyBytesConsumed = 0;
		m_trailerCount = 0;
		m_lastFeedConsumed = 0;
	}

	//***************************************************************************
	// @brief start-line(요청 라인/상태 라인)을 해석합니다. 성공하면 m_state를 Headers로 바꾸고,
	//        형식이 잘못됐으면 Error로 바꾼다.
	// @param line 개행이 제거된 줄 원문
	//***************************************************************************
	virtual void HandleStartLine(const std::string& line) = 0;

	//***************************************************************************
	// @brief 헤더 섹션이 끝났을 때(빈 줄 도달) 호출됩니다. 본문 길이 규칙을 판정해 m_state를
	//        Body/ChunkedSize/Complete/Error 중 하나로 바꾸고, 필요하면 m_contentLength를 채운다.
	//***************************************************************************
	virtual void OnHeadersComplete() = 0;

	//***************************************************************************
	// @brief 본문 바이트의 행선지. 기본 구현은 m_body에 누적한다.
	//***************************************************************************
	virtual void OnBodyData(const char* data, size_t len)
	{
		m_body.append(data, len);
	}

	//***************************************************************************
	// @brief 이번 메시지에 적용할 본문 크기 상한. 기본은 HTTP::kMaxBodyLen.
	//***************************************************************************
	virtual size_t MaxBodyLen() const noexcept { return HTTP::kMaxBodyLen; }

	//***************************************************************************
	// @brief Content-Length 기준으로 body 바이트를 소비합니다.
	// @return 실제로 소비한 바이트 수 (남은 길이만큼만)
	// @details m_contentLength는 OnHeadersComplete()에서 MaxBodyLen() 이하임이 확인된 값이다.
	//***************************************************************************
	virtual size_t ConsumeBodyBytes(const char* data, size_t len)
	{
		const size_t remaining = m_contentLength - m_bodyBytesConsumed;
		const size_t take = (std::min)(len, remaining);

		OnBodyData(data, take);
		m_bodyBytesConsumed += take;
		if( m_bodyBytesConsumed >= m_contentLength )
			m_state = HTTP::EParseState::Complete;
		return take;
	}

	//***************************************************************************
	// @brief Content-Length 본문을 위한 버퍼를 미리 잡습니다 (상한 HTTP::kMaxBodyReserve).
	// @details 헤더의 숫자만 믿고 최대 64MB를 먼저 할당하면, 본문을 보내지 않는 연결 하나가 그만큼의
	//          메모리를 붙들 수 있다. 일반적인 크기는 한 번에 잡고 그 이상은 append가 키운다.
	//***************************************************************************
	void ReserveBody(size_t contentLength)
	{
		m_body.reserve((std::min)(contentLength, HTTP::kMaxBodyReserve));
	}

	void SetError() noexcept { m_state = HTTP::EParseState::Error; }

private:
	//***************************************************************************
	// @brief start-line / 헤더 / trailer 중 현재 상태에 맞는 줄 하나를 소비합니다.
	// @return 소비한 바이트 수. 개행을 못 찾으면 m_lineBuffer에 누적만 하고 len 전체를 반환.
	//***************************************************************************
	size_t ConsumeLine(const char* data, size_t len)
	{
		size_t consumed = 0;
		switch( HTTP::ReadLine(m_lineBuffer, data, len, consumed) )
		{
		case HTTP::ELineResult::NeedMore:
			return consumed;
		case HTTP::ELineResult::TooLong:
			SetError();
			return consumed;
		case HTTP::ELineResult::Line:
			break;
		}

		switch( m_state )
		{
		case HTTP::EParseState::StartLine:
			// 이전 메시지 뒤에 붙어 온 빈 줄(일부 클라이언트가 POST 본문 뒤에 CRLF를 덧붙임)은 무시한다.
			if( !m_lineBuffer.empty() )
				HandleStartLine(m_lineBuffer);
			break;

		case HTTP::EParseState::Headers:
			if( m_lineBuffer.empty() )
				OnHeadersComplete();
			else if( !HTTP::AddHeaderLine(m_headers, m_lineBuffer) )
				SetError();
			break;

		case HTTP::EParseState::ChunkedTrailer:
			if( m_lineBuffer.empty() )
				m_state = HTTP::EParseState::Complete;
			else if( ++m_trailerCount > HTTP::kMaxHeaderCount )
				SetError(); // trailer 필드는 개수만 세고 버린다
			break;

		default:
			break;
		}

		m_lineBuffer.clear();
		return consumed;
	}

	//***************************************************************************
	// @brief chunked 인코딩의 청크 크기 줄(16진수)을 파싱합니다.
	//***************************************************************************
	size_t ConsumeChunkSizeLine(const char* data, size_t len)
	{
		size_t consumed = 0;
		switch( HTTP::ReadLine(m_lineBuffer, data, len, consumed) )
		{
		case HTTP::ELineResult::NeedMore:
			return consumed;
		case HTTP::ELineResult::TooLong:
			SetError();
			return consumed;
		case HTTP::ELineResult::Line:
			break;
		}

		size_t chunkSize = 0;
		const bool ok = HTTP::ParseChunkSizeLine(m_lineBuffer, chunkSize);
		m_lineBuffer.clear();
		if( !ok )
		{
			SetError();
			return consumed;
		}

		if( chunkSize == 0 )
		{
			m_state = HTTP::EParseState::ChunkedTrailer; // 마지막 청크: trailer(없으면 빈 줄) 처리로 이동
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
	// @details chunked는 총량을 미리 알 수 없어 OnHeadersComplete()의 사전 검사가 적용되지 않는다 —
	//          누적 크기를 여기서 직접 MaxBodyLen()과 비교해 넘으면 Error로 전환한다.
	//***************************************************************************
	size_t ConsumeChunkData(const char* data, size_t len)
	{
		const size_t take = (std::min)(len, m_chunkRemaining);

		OnBodyData(data, take);
		m_bodyBytesConsumed += take;
		m_chunkRemaining -= take;

		if( m_bodyBytesConsumed > MaxBodyLen() )
		{
			SetError();
			return take;
		}

		if( m_chunkRemaining == 0 )
			m_state = HTTP::EParseState::ChunkedCRLF;
		return take;
	}

	//***************************************************************************
	// @brief 청크 데이터 뒤에 오는 CRLF 두 바이트를 검증하며 소비합니다.
	//***************************************************************************
	size_t ConsumeChunkTrailingCrlf(const char* data, size_t len)
	{
		bool error = false;
		const size_t consumed = HTTP::ConsumeChunkCrlf(data, len, m_crlfProgress, error);
		if( error )
		{
			SetError();
			return consumed;
		}

		if( m_crlfProgress >= 2 )
		{
			m_crlfProgress = 0;
			m_state = HTTP::EParseState::ChunkedSize;
		}
		return consumed;
	}

protected:
	HTTP::EParseState m_state = HTTP::EParseState::StartLine; // 파싱 진행 상태
	std::string m_lineBuffer;                                 // 개행을 못 찾은 부분 줄의 누적 버퍼
	HTTP::HeaderList m_headers;                               // 파싱된 헤더 목록 (삽입 순서 보존)
	std::string m_body;                                       // 지금까지 누적된 body

	size_t m_contentLength = 0;      // Content-Length 본문의 총 길이 (OnHeadersComplete()가 채움)
	size_t m_bodyBytesConsumed = 0;  // 지금까지 처리한 본문 바이트 수 (m_body로 누적하지 않는 스트리밍에서도 진행률 추적)

private:
	size_t m_chunkRemaining = 0;  // 현재 청크에서 아직 안 읽은 바이트 수
	int m_crlfProgress = 0;       // 청크 데이터 뒤 CRLF 소비 진행도(0~2)
	size_t m_trailerCount = 0;    // 버려진 trailer 줄 수 (상한 검사용)
	size_t m_lastFeedConsumed = 0; // 직전 Feed()에서 소비한 바이트 수
};

#endif // ndef UC_HTTPMESSAGEPARSER_H