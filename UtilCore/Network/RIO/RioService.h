
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
//      1. CRioListener를 내부에 두고 Accept 처리 및 RIO RQ 할당 총괄 제어
//      2. 클라이언트 접속 시 세션 팩토리를 통해 세션을 생성하고 콜백을 통해 관리
//
//      CRioCore는 생성자에서 외부로부터 주입받아 공유 소유합니다(CIocpServerService와
//      동일한 패턴 — 코어 생성/수명 관리는 호출자 책임, 이 서비스는 Start()에서
//      그 인스턴스를 Initialize()하고 사용만 합니다). CRioCore는 멀티 워커
//      스레드 버전이므로 StartWorkers(N, ...)로 구동합니다.
//
//      송신 버퍼(RIO_BUFFERID)는 이 서비스가 소유하지 않습니다. 세션마다
//      자기 소유의 송신 링버퍼(CRingBuffer)를 갖고 있고, 그 실제 메모리를
//      CRioSession::Init() 내부에서 스스로 RIORegisterBuffer()로 등록합니다.
//
//      [종료 순서]
//      Close()는 세션 종료 통지 → Listener 정지 → _rioCore의 정식
//      RequestStop()+Shutdown()(outstanding I/O drain) → 버퍼/이벤트풀 해제
//      순서를 명시적으로 강제합니다. 멤버 소멸자 순서에 맡기지 않는 이유는,
//      멤버 선언 순서(역순 소멸)상 _globalRecvBuffer/_eventPool이 _rioCore보다
//      먼저 파괴되어 아직 drain되지 않은 outstanding I/O가 있는 상태로
//      CRioBuffer::~CRioBuffer()/CRioEventPool 소멸자가 호출될 수 있기
//      때문입니다(둘 다 outstanding 자원이 남아있으면 assert+terminate).
//***************************************************************************
class CRioServerService : public CNetService
{
public:
	//***************************************************************************
	// @brief CRioServerService 생성자
	// @param address 서버가 바인딩할 로컬 네트워크 주소(IP/Port)
	// @param rioCore 이 서비스가 사용할 RIO Core 참조(공유 소유). Start()가
	//        이 인스턴스를 그대로 Initialize()합니다 — 코어 생성/수명 관리는
	//        호출자 책임입니다(CIocpServerService와 동일한 패턴).
	// @param factory 접속마다 세션 객체를 생성할 팩토리 함수
	// @param maxSessionCount 동시에 수용할 최대 세션 수 (기본값 1)
	// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본값 0 = CRioCore가
	//        hardware_concurrency()/2로 자동 산정)
	//***************************************************************************
	CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1, uint32_t workerThreadCount = 0);

	//***************************************************************************
	// @brief 소멸자 (기본 소멸자 — 실제 자원 정리는 Close()가 담당)
	//***************************************************************************
	virtual ~CRioServerService() = default;

	//***************************************************************************
	// @brief RIO 서버 구동 (코어 초기화, 이벤트 풀/수신 버퍼 풀 초기화,
	//        멀티 워커 스레드 시작, Listener 시작)
	// @return bool 모든 초기화 단계와 Listener 시작까지 성공하면 true. 어느 한
	//         단계라도 실패하면 false를 반환하며, 그 시점까지 만든 자원은
	//         내부에서 역순으로 정리됩니다(워커가 이미 Running 상태였다면
	//         RequestStop()+Shutdown()까지 포함).
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 서버 서비스 종료
	// @details 순서: 모든 세션에 종료 통지 → Listener 정지 → _rioCore
	//          RequestStop()+Shutdown()(outstanding I/O drain 완료 보장) →
	//          _globalRecvBuffer 해제 → 부모 CNetService::Close().
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
	CRioCoreRef		_rioCore = nullptr;					// 연동된 RIO 코어 참조 (생성자에서 주입받음)
	CRioListenerRef _listener = nullptr;				// RIO 접속 수락 리스너
	CRioSessionManager _sessionManager;					// 서버 서비스가 소유하는 RIO 세션 매니저
	CRioEventPool	_eventPool;							// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef	_globalRecvBuffer;					// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
	uint32_t		_workerThreadCount = 0;				// StartWorkers()에 넘길 워커 스레드 개수 (0=자동)
};

//***************************************************************************
// @class CRioClientService
// @brief RIO(Registered I/O) 기반의 클라이언트 전용 서비스 클래스
// @details
// 역할:
//      1. 지정된 개수만큼 RIO 세션을 생성하고 RIO 링 버퍼 및 RQ 설정 준비
//      2. 클라이언트 네트워크 연결, 소켓 등록 및 수명 주기 관리
//
//      CRioCore 소유 정책 및 송신 버퍼/종료 순서 정책은 CRioServerService와
//      동일합니다(위 클래스 @details 참고).
//***************************************************************************
class CRioClientService : public CNetService
{
public:
	//***************************************************************************
	// @brief CRioClientService 생성자
	// @param address 접속할 서버의 네트워크 주소 정보
	// @param rioCore 이 서비스가 사용할 RIO Core 참조(공유 소유). Start()가
	//        이 인스턴스를 그대로 Initialize()합니다.
	// @param factory 세션 객체 생성을 위한 팩토리 함수
	// @param maxSessionCount 생성 및 관리할 최대 클라이언트 세션 수 (기본값: 1)
	// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본값 0 = 자동 산정)
	//***************************************************************************
	CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1, uint32_t workerThreadCount = 0);

	//***************************************************************************
	// @brief 소멸자 (기본 소멸자 — 실제 자원 정리는 Close()가 담당)
	//***************************************************************************
	virtual ~CRioClientService() = default;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 구동합니다 (코어 초기화, 멀티 워커 시작, N개 세션 연결).
	// @return bool 이벤트 풀/코어/수신 버퍼 초기화 및 요청한 세션 수만큼의 접속이
	//         모두 성공하면 true. 세션 연결 도중 하나라도 실패하면 그 이전까지
	//         맺어진 세션들을 전부 정리하고 false를 반환합니다(부분 성공 상태로
	//         남기지 않음).
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 종료합니다.
	// @details 순서: 모든 세션에 종료 통지 → _rioCore RequestStop()+Shutdown()
	//          (outstanding I/O drain 완료 보장) → _globalRecvBuffer 해제 →
	//          부모 CNetService::Close().
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
	CRioCoreRef			_rioCore = nullptr;						// 연동된 RIO 코어 참조 (생성자에서 주입받음)
	CRioSessionManager	_sessionManager;						// 클라이언트 서비스가 소유하는 RIO 세션 매니저
	CRioEventPool		_eventPool;								// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef		_globalRecvBuffer;						// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
	uint32_t			_workerThreadCount = 0;					// StartWorkers()에 넘길 워커 스레드 개수 (0=자동)
};

#endif // ndef __RIOSERVICE_H__