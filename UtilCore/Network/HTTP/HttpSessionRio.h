
//***************************************************************************
// HttpSessionRio.h : interface for the CHttpSessionRio class.
//
//***************************************************************************

#ifndef __HTTPSESSIONRIO_H__
#define __HTTPSESSIONRIO_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__RIOSESSION_H__
#include <Network/RIO/RioSession.h>
#endif

#ifndef	__HTTPCONNPOOLCOMMON_H__
#include <Network/HTTP/HttpConnPoolCommon.h>
#endif

#ifndef	__HTTPCLIENTCORE_H__
#include <Network/HTTP/HttpClientCore.h>
#endif

//***************************************************************************
// @class CHttpSessionRio
// @brief CRioSession + CHttpClientCore(합성) — RIO 엔진에서 HTTP 요청/응답을
//        주고받는 세션
//
// @details
//      OnDataReceived()는 CRioSession의 순수가상함수라 필수 구현 대상이다.
//      IOCP와 달리 파라미터 없이 호출되므로 GetRecvBuffer()를 직접 소비해야
//      한다(CIocpSession::ProcessRecv()가 하던 것과 동일한 패턴을 여기서
//      수동으로 반복).
//
//      OnConnected()/OnDisconnected(reason)은 CRioSession의 protected
//      virtual 훅이다. CRioConnectDispatcher 도입 이후 RIO의 연결도 IOCP와
//      동일하게 완전 비동기라(ConnectEx 기반), 이 두 훅이 비동기로 호출되는
//      시점 역시 IOCP와 동일한 그림이다 — CRioSession::ProcessConnectEx()가
//      연결 완료를 검증한 뒤 Init()을 호출하고, 그 안에서 OnConnected()가
//      호출된다(RioSession.h 클래스 설명 참고).
//***************************************************************************
class CHttpSessionRio final : public CRioSession
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
	// @brief 연결 완료 시 호출됩니다(CRioSession::Init() 내부에서). 등록된
	//        콜백이 있으면 connected=true로 통지합니다.
	//***************************************************************************
	void OnConnected() override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), true);
	}

	//***************************************************************************
	// @brief 연결 종료 시 호출됩니다(연결 실패 포함). 등록된 콜백이 있으면
	//        connected=false로 통지합니다. reason은 풀 쪽에서 쓰지 않으므로 무시.
	//***************************************************************************
	void OnDisconnected(Rio::CloseReason /*reason*/) override
	{
		if( _connStateHandler )
			_connStateHandler(shared_from_this(), false);
	}

	//***************************************************************************
	// @brief 수신 링버퍼를 직접 소비해 CHttpClientCore로 넘겨 응답 파싱을 진행합니다.
	// @details CIocpSession::ProcessRecv()가 프레임워크 차원에서 해주는 것과
	//          달리, CRioSession은 OnDataReceived()가 파라미터 없이 호출되므로
	//          GetRecvBuffer()에서 직접 꺼내 소비해야 한다. 경계 래핑(wrap-around)
	//          오버런 방지를 위해 GetSizeDirectDequeueAble() 크기만큼만 한 번에
	//          처리하며, 남은 데이터가 있으면 루프를 반복한다.
	//***************************************************************************
	void OnDataReceived() override
	{
		while( true )
		{
			int64 dataSize = GetRecvBuffer().GetSizeUsed();
			if( dataSize <= 0 )
				break;

			int64 directSize = GetRecvBuffer().GetSizeDirectDequeueAble();
			BYTE* readPos = reinterpret_cast<BYTE*>(GetRecvBuffer().GetReadBuffer());

			_httpCore.FeedRecv(reinterpret_cast<char*>(readPos), static_cast<size_t>(directSize));

			GetRecvBuffer().MoveReadBuffer(directSize);
		}
	}

private:
	CHttpClientCore _httpCore;               // HTTP 요청/응답 오케스트레이션 (엔진 비의존)
	HttpConnStateHandler _connStateHandler;  // 연결 상태 변화를 풀에 통지하는 콜백
};

#endif // ndef __HTTPSESSIONRIO_H__