
//***************************************************************************
// HttpSessionIocp.h : interface for the CHttpSessionIocp class.
//
//***************************************************************************

#ifndef __HTTPSESSIONIOCP_H__
#define __HTTPSESSIONIOCP_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__IOCPSESSION_H__
#include <Network/IOCP/IocpSession.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__HTTPCLIENTCORE_H__
#include <Network/HTTP/HttpClientCore.h>
#endif

//***************************************************************************
// @class CHttpSessionIocp
// @brief CIocpSession + CHttpClientCore(합성) — IOCP 엔진에서 HTTP 요청/응답을
//        주고받는 세션
//
// @details
//      OnRecv()는 CIocpSession의 계약대로 "처리한 바이트 수"를 반환해야
//      하는데, CHttpResponseParser는 내부적으로 자기 상태(partial line 등)를
//      들고 있어 주어진 바이트를 항상 전량 소비하므로 그냥 len을 그대로
//      반환한다.
//
//      OnConnected()/OnDisconnected()는 CIocpSession의 protected virtual
//      훅을 오버라이드해 CHttpConnPoolT에게 연결 상태 변화를 통지한다.
//***************************************************************************
class CHttpSessionIocp final : public CIocpSession
{
public:
	//***************************************************************************
	// @brief 연결 상태 변화(연결 완료/종료) 통지 콜백을 등록합니다.
	// @details 실제로는 CHttpConnPoolT::Create()의 세션 팩토리가 세션 생성
	//          직후 이 함수로 자기 자신(풀)에 연결한다.
	//***************************************************************************
	void SetConnStateHandler(HttpConnStateHandler handler) { _connStateHandler = std::move(handler); }

	//***************************************************************************
	// @brief 완성된 요청 패킷을 전송하고, 응답이 완결되면 onComplete를 호출합니다.
	// @param data 완성된 요청 패킷 바이트
	// @param len data의 길이
	// @param onComplete 응답 완결 시 호출되는 콜백
	// @return bool false면 (a) 이전 요청이 아직 진행 중이거나 (b) Send() 자체가
	//         실패한 것 — 어느 쪽이든 이 세션은 더 이상 재사용하지 말고 풀이
	//         폐기 처리해야 한다.
	//***************************************************************************
	bool SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		return _httpCore.BeginRequest(
			[this](const void* d, uint16_t n) { return Send(d, n); },
			data, len, std::move(onComplete));
	}

	//***************************************************************************
	// @brief 직전 응답이 "Connection: close"를 명시했는지 반환합니다.
	// @return bool true면 keep-alive 재사용 금지 — 풀이 이 세션을 폐기해야 함
	//***************************************************************************
	bool IsConnectionCloseRequested() const { return _httpCore.IsConnectionCloseRequested(); }

	//***************************************************************************
	// @brief 현재 HTTP 요청/응답 진행 상태를 반환합니다 (Idle/AwaitingResponse).
	//***************************************************************************
	EHttpClientState GetHttpState() const { return _httpCore.GetState(); }

protected:
	//***************************************************************************
	// @brief 연결 완료 시 호출됩니다(CIocpSession::ProcessConnect() 내부에서).
	//        등록된 콜백이 있으면 connected=true로 통지합니다.
	//***************************************************************************
	void OnConnected() override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), true);
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(연결 실패 포함). 등록된 콜백이 있으면
	//        connected=false로 통지합니다.
	//***************************************************************************
	void OnDisconnected() override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신 바이트를 CHttpClientCore로 넘겨 응답 파싱을 진행합니다.
	// @param buffer 수신 바이트 포인터
	// @param len buffer의 길이
	// @return int32 처리한 바이트 수 (파서가 부분 상태를 자체 보유하므로 항상 len 전량)
	//***************************************************************************
	int32 OnRecv(BYTE* buffer, int32 len) override
	{
		_httpCore.FeedRecv(reinterpret_cast<char*>(buffer), static_cast<size_t>(len));
		return len; // 파서가 부분 상태를 자체 보유하므로 항상 전량 소비
	}

private:
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진 비의존)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화를 풀에 통지하는 콜백
};

#endif // ndef __HTTPSESSIONIOCP_H__