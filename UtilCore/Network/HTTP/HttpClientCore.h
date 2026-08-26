
//***************************************************************************
// HttpClientCore.h : interface for the CHttpClientCore class.
//
//***************************************************************************

#ifndef __HTTPCLIENTCORE_H__
#define __HTTPCLIENTCORE_H__

#ifndef	__HTTPRESPONSEPARSER_H__
#include <Network/HTTP/HttpResponseParser.h>
#endif

#include <functional>
#include <cstdint>
#include <algorithm>
#include <utility>

// 요청 완료 통지. success=false면 파서가 Error 상태(프로토콜 위반 등)로 멈춘 것 —
// 이 경우 호출부는 커넥션을 재사용하지 말고 폐기해야 함.
using HttpRequestCompletionHandler = std::function<void(bool success, CHttpResponseParser& parser)>;

//***************************************************************************
// @enum EHttpClientState
// @brief CHttpClientCore의 요청/응답 진행 상태
//***************************************************************************
enum class EHttpClientState
{
	Idle,             // 요청 없음, 새 요청 받을 수 있음
	AwaitingResponse  // 요청 전송 완료, 응답 수신/파싱 대기 중
};

//***************************************************************************
// @class CHttpClientCore
// @brief 엔진(IOCP/RIO) 비의존 HTTP 요청/응답 오케스트레이션 로직
//
// @details
//      CSession(IOCP/RIO 공통 베이스)의 Send(const void*, uint16_t) 인터페이스에만
//      의존한다. CIocpSession과 CRioSession은 OnRecv()/OnDataReceived() 수신
//      훅의 계약이 서로 달라(전자는 반환값으로 처리 바이트 수 통지, 후자는
//      파라미터 없이 호출되고 스스로 GetRecvBuffer()를 소비) 상속으로 공유할 수
//      없다. 그래서 상속이 아니라 "멤버로 보유(합성)"하는 방식으로 두 세션
//      클래스(CHttpSessionIocp/CHttpSessionRio) 양쪽에서 재사용한다.
//
//      [책임 분리]
//      - 이 클래스: 요청 송신(65535바이트 단위 자동 분할) + 응답 파싱 상태 관리 +
//        완료 콜백
//      - 세션 클래스: 실제 소켓 I/O, 수신 훅에서 받은 바이트를 FeedRecv()로 전달
//      - 상위(CHttpConnPoolT 등): 커넥션 획득/반납, 재연결, keep-alive 판단
//
//      [파이프라이닝 미지원] 이 클래스는 "커넥션 1개 = HTTP 요청 1개씩 순차
//      처리"만 지원한다(대부분의 서버가 HTTP/1.1 파이프라이닝을 사실상 지원 안
//      하므로 의도적 선택). BeginRequest()는 이전 요청이 AwaitingResponse
//      상태면 false를 반환하니, 동시 요청은 상위 풀에서 커넥션을 여러 개 굴려서
//      병렬화해야 한다.
//***************************************************************************
class CHttpClientCore
{
public:
	//***************************************************************************
	// @brief 완성된 요청 패킷을 전송하고 응답 대기 상태로 전이합니다.
	// @tparam SendFn 실제 바이트 전송 콜백 타입, 시그니처는
	//         bool(const void* data, uint16_t size) — 세션의 Send()를 그대로
	//         람다로 감싸서 넘기면 됨: [this](const void* d, uint16_t n) { return Send(d, n); }
	// @param data 완성된 요청 패킷 (예: CHttpRequestBuilderT::Build() 결과)
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool 송신 개시 성공 여부(false면 sender가 중간에 실패했거나 이미
	//         다른 요청이 진행 중)
	//***************************************************************************
	template<typename SendFn>
	bool BeginRequest(SendFn&& sender, const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		if( m_state != EHttpClientState::Idle )
			return false;

		m_parser.Reset();

		size_t offset = 0;
		while( offset < len )
		{
			size_t chunk = std::min<size_t>(len - offset, 65535);
			if( !sender(data + offset, static_cast<uint16_t>(chunk)) )
				return false; // 부분 전송된 상태로 실패 — 호출부가 커넥션을 폐기해야 함
			offset += chunk;
		}

		m_onComplete = std::move(onComplete);
		m_state = EHttpClientState::AwaitingResponse;
		return true;
	}

	//***************************************************************************
	// @brief 세션의 수신 훅(OnRecv/OnDataReceived)에서 호출합니다.
	// @param data 수신 바이트 포인터
	// @param len data의 길이
	// @return bool 이번 호출로 응답이 완결(성공 또는 에러)되었는지 여부. true면
	//         호출부(세션)가 커넥션 반납/폐기를 판단해야 함.
	//***************************************************************************
	bool FeedRecv(const char* data, size_t len)
	{
		if( m_state != EHttpClientState::AwaitingResponse )
			return false; // 요청도 안 했는데 온 데이터 — 프로토콜 위반, 상위에서 별도 처리

		HTTP::EParseState st = m_parser.Feed(data, len);
		if( st != HTTP::EParseState::Complete && st != HTTP::EParseState::Error )
			return false; // 아직 더 필요

		bool success = (st == HTTP::EParseState::Complete);
		m_state = EHttpClientState::Idle;

		if( m_onComplete )
		{
			HttpRequestCompletionHandler cb = std::move(m_onComplete);
			m_onComplete = nullptr;
			cb(success, m_parser); // 콜백 안에서 다음 BeginRequest()를 걸 수도 있으므로 Idle 전이 이후 호출
		}
		return true;
	}

	EHttpClientState GetState() const noexcept { return m_state; }

	//***************************************************************************
	// @brief 직전 응답이 "Connection: close"를 명시했는지 반환합니다.
	// @return bool true면 keep-alive 재사용 금지
	//***************************************************************************
	bool IsConnectionCloseRequested() const noexcept { return m_parser.IsConnectionCloseRequested(); }

	void OnSessionDisconnected()
	{
		if( m_state != EHttpClientState::AwaitingResponse )
			return;

		m_state = EHttpClientState::Idle;
		if( m_onComplete )
		{
			HttpRequestCompletionHandler cb = std::move(m_onComplete);
			m_onComplete = nullptr;               // 호출 전에 비워서 self-cycle을 즉시 끊음
			cb(false, m_parser);                  // success=false로 실패 통지
		}
	}

private:
	EHttpClientState m_state = EHttpClientState::Idle; // 요청/응답 진행 상태
	CHttpResponseParser m_parser;                      // 응답 파서 (Idle 전이 시 재사용)
	HttpRequestCompletionHandler m_onComplete;          // 진행 중인 요청의 완료 콜백
};

#endif // ndef __HTTPCLIENTCORE_H__