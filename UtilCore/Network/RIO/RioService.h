
//***************************************************************************
// RioService.h : interface for the CRioServerService & CRioClientService.
//
//***************************************************************************

#ifndef __RIOSERVICE_H__
#define __RIOSERVICE_H__

#ifndef __NET_SERVICE_H__
#include <Network/NetService.h>
#endif

#ifndef __RIO_CORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __RIO_LISTENER_H__
#include <Network/Rio/RioListener.h>
#endif

#ifndef __RIOSESSIONMANAGER_H__
#include <Network/Rio/RioSessionManager.h>
#endif

class CRioBuffer;

//***************************************************************************
// @class CRioServerService
// @brief RIO(Registered I/O) 기반의 서버 전용 서비스 클래스
// @details
// 역할:
//      1. CRioListener를 내부에 두고 동기 블로킹 accept 루프 및 RIO RQ 할당 총괄 제어
//      2. 클라이언트 접속 시 세션 팩토리를 통해 세션을 생성하고 콜백을 통해 관리
//
//      송신 버퍼(RIO_BUFFERID)는 더 이상 이 서비스가 소유하지 않습니다. 세션마다
//      자기 소유의 송신 링버퍼(CRingBuffer)를 갖고 있고, 그 실제 메모리를
//      CRioSession::Init() 내부에서 스스로 RIORegisterBuffer()로 등록합니다.
//***************************************************************************
class CRioServerService : public CNetService
{
public:
	//***************************************************************************
	// @brief CRioServerService 생성자
	// @param address 서버가 바인딩할 로컬 네트워크 주소(IP/Port)
	// @param rioCore 이 서비스가 사용할 RIO Core 참조(공유 소유, Start()가 초기화함)
	// @param factory 접속마다 세션 객체를 생성할 팩토리 함수
	// @param maxSessionCount 동시에 수용할 최대 세션 수 (기본값 1)
	//***************************************************************************
	CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1);

	//***************************************************************************
	// @brief 소멸자 (기본 소멸자 — 실제 자원 정리는 Close()가 담당)
	//***************************************************************************
	virtual ~CRioServerService() = default;

	//***************************************************************************
	// @brief RIO 서버 구동 (이벤트 풀/수신 버퍼 풀 초기화, 워커 스레드 시작, Listener 시작)
	// @return bool 모든 초기화 단계와 Listener 시작까지 성공하면 true, 어느 한 단계라도
	//         실패하면 false(그 시점까지 만든 자원은 내부에서 역순 정리됨)
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 서버 서비스 종료 (모든 세션에 종료 통지 → Listener 정지 → 부모 Close)
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 서비스 전역에서 관리되는 수신용 글로벌 CRioBuffer 객체를 반환합니다.
	// @return CRioBuffer* 수신 버퍼 풀 포인터 (Start() 성공 전까지는 nullptr일 수 있음)
	//***************************************************************************
	CRioBuffer* GetGlobalRecvBuffer() { return _globalRecvBuffer.get(); }

	//***************************************************************************
	// @brief 소속된 RIO Core 객체를 반환합니다.
	// @return CRioCoreRef RIO Core 참조 객체
	//***************************************************************************
	CRioCoreRef		GetRioCore() const { return _rioCore; }

	//***************************************************************************
	// @brief 소속된 RIO 세션 매니저 참조를 반환합니다.
	// @return CRioSessionManager& 세션 매니저 참조
	//***************************************************************************
	CRioSessionManager& GetSessionManager() { return _sessionManager; }

private:
	CRioCoreRef		_rioCore = nullptr;					// 연동된 RIO 코어 참조
	CRioListenerRef _listener = nullptr;				// RIO 접속 수락 리스너
	CRioSessionManager _sessionManager;					// 서버 서비스가 소유하는 RIO 세션 매니저
	CRioEventPool	_eventPool;							// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef	_globalRecvBuffer;					// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
};

//***************************************************************************
// @class CRioClientService
// @brief RIO(Registered I/O) 기반의 클라이언트 전용 서비스 클래스
// @details
// 역할:
//      1. 지정된 개수만큼 RIO 세션을 생성하고 RIO 링 버퍼 및 RQ 설정 준비
//      2. 클라이언트 네트워크 연결, 소켓 등록 및 수명 주기 관리
//
//      송신 버퍼 소유 방식은 CRioServerService와 동일하게 세션 자기 소유입니다.
//***************************************************************************
class CRioClientService : public CNetService
{
public:
	//***************************************************************************
	// @brief CRioClientService 생성자
	// @param address 접속할 서버의 네트워크 주소 정보
	// @param rioCore 생성자 파라미터로는 받지만 Start() 내부에서 이 서비스 전용
	//        CRioCore로 새로 교체됩니다(현재 미사용 파라미터 — 아래 참고 항목 확인)
	// @param factory 세션 객체 생성을 위한 팩토리 함수
	// @param maxSessionCount 생성 및 관리할 최대 클라이언트 세션 수 (기본값: 1)
	//***************************************************************************
	CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1);

	//***************************************************************************
	// @brief 소멸자 (기본 소멸자 — 실제 자원 정리는 Close()가 담당)
	//***************************************************************************
	virtual ~CRioClientService() = default;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 구동합니다 (코어 초기화, 워커 시작, N개 세션 연결).
	// @return bool 이벤트 풀/코어/수신 버퍼 초기화 및 요청한 세션 수만큼의 접속이
	//         모두 성공하면 true. 세션 연결 도중 하나라도 실패하면 그 이전까지
	//         맺어진 세션들을 정리하고 false를 반환합니다.
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 종료합니다 (모든 세션에 종료 통지 → 부모 Close).
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 소속된 RIO Core 객체의 참조를 반환합니다.
	// @return CRioCoreRef RIO Core 참조 객체
	//***************************************************************************
	CRioCoreRef		GetRioCore() const { return _rioCore; }

	//***************************************************************************
	// @brief 서비스 전역에서 비동기 수신(Receive)용으로 관리되는 CRioBuffer 객체를 반환합니다.
	// @return CRioBuffer* 글로벌 수신 버퍼 포인터 (Start() 성공 전까지는 nullptr일 수 있음)
	//***************************************************************************
	CRioBuffer* GetGlobalRecvBuffer() { return _globalRecvBuffer.get(); }

	//***************************************************************************
	// @brief 소속된 RIO 세션 매니저 참조를 반환합니다.
	// @return CRioSessionManager& 세션 매니저 참조
	//***************************************************************************
	CRioSessionManager& GetSessionManager() { return _sessionManager; }

private:
	CRioCoreRef			_rioCore = nullptr;						// 연동된 RIO 코어 참조 객체
	CRioSessionManager	_sessionManager;						// 클라이언트 서비스가 소유하는 RIO 세션 매니저
	CRioEventPool		_eventPool;								// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef		_globalRecvBuffer;						// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
};

#endif // ndef __RIOSERVICE_H__