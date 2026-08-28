
//***************************************************************************
// RioConnectEvent.h : interface for the RioConnectEvent class.
//
//***************************************************************************

#ifndef UC_RIOCONNECTEVENT_H
#define UC_RIOCONNECTEVENT_H

#include <WinSock2.h>
#include <memory>

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioSession.h>

class CRioSession;
using CRioSessionRef = std::shared_ptr<CRioSession>;

//***************************************************************************
// @class RioConnectEvent
// @brief CRioConnectDispatcher 전용 OVERLAPPED 파생 이벤트 (ConnectEx 완료 통지용)
//
// @details
//      RIO의 실제 데이터 송수신 완료는 CRioEvent/RequestContext 메커니즘
//      (RIODequeueCompletion + RIORESULT.RequestContext)을 통해 CRioCore가
//      처리하는데, 이건 그 경로와 완전히 무관한 별개의 통로다 — ConnectEx는
//      일반 Winsock 확장함수라 RIO CQ가 아니라 평범한 OVERLAPPED/IOCP 완료
//      통지로 온다. RIO 쪽엔 CIocpEvent 같은 공통 베이스가 없어서, 이 하나의
//      용도(ConnectEx 완료 통지)만을 위한 최소 구조체를 새로 둔다.
//***************************************************************************
class RioConnectEvent : public OVERLAPPED
{
public:
	//***************************************************************************
	// @brief RioConnectEvent 생성자
	// @details OVERLAPPED 필드를 0으로 초기화합니다(Init() 호출).
	//***************************************************************************
	RioConnectEvent() { Init(); }

	//***************************************************************************
	// @brief OVERLAPPED 필드를 0으로 초기화합니다. ConnectEx 재사용 전 반드시 호출.
	//***************************************************************************
	void Init()
	{
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		hEvent = nullptr;
	}

public:
	// 완료 통지가 올 때까지 세션 수명을 보장하는 owner. IOCP의 ConnectEvent::owner와
	// 동일한 역할이지만, 여기서는 CRioObjectRef가 아니라 CRioSessionRef로 직접
	// 잡는다 — 완료 처리가 CRioObject::Dispatch(CRioEvent*, ULONG, LONG) 공용
	// 인터페이스가 아니라 CRioSession 전용 ProcessConnectEx()를 호출해야 하기 때문.
	CRioSessionRef owner;
};

#endif // ndef UC_RIOCONNECTEVENT_H