
//***************************************************************************
// RioService.h : interface for the CRioServerService & CRioClientService.
//
//***************************************************************************

#ifndef UC_RIOSERVICE_H
#define UC_RIOSERVICE_H

#include <Network/NetService.h>
#include <Network/RIO/RioCore.h>
#include <Network/RIO/RioListener.h>
#include <Network/RIO/RioConnectDispatcher.h>
#include <Network/RIO/RioSessionManager.h>
#include <Containers/Queue/DelayedTaskQueue.h>

#include <thread>
#include <atomic>

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
	CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

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
	//***************************************************************************
	// @brief _sessionManager에 다음 reap tick을 예약합니다(self-rescheduling).
	// @details 최초 호출은 Start()에서, 이후로는 이 함수 자신이 실행될 때마다
	//          다음 tick을 다시 예약합니다 — CDelayedTaskQueue::Reserve()가
	//          일회성이라 주기적 동작을 만들려면 이 패턴이 필요합니다. Close()가
	//          _sessionReapQueue.Stop()을 호출하면 그 이후의 재예약 시도는
	//          Reserve()가 false를 반환하며 조용히 무시되어 재귀가 자연스럽게
	//          끊깁니다.
	//***************************************************************************
	void ScheduleSessionReap();

	// Running 중 자연 종료된(원격 종료/에러 등) 세션의 _sessionManager 엔트리를
	// 방치하면 서버가 오래 떠 있을수록 map이 무한정 커진다 — 예전에는
	// RemoveClosedSessions()가 Close()(종료 시점)에서만 호출됐다. 이제
	// ScheduleSessionReap()이 kSessionReapInterval마다 주기적으로 호출한다.
	static constexpr std::chrono::seconds kSessionReapInterval{ 30 }; // 임의로 잡은 기본값 — 세션 처리량/서버 규모에 맞춰 조정 가능
	CDelayedTaskQueue	_sessionReapQueue;						// reap tick 예약 큐 (스스로 워커 스레드를 안 가짐)
	std::thread			_sessionReapThread;						// _sessionReapQueue.ProcessExpiredTasks()를 실행하는 전용 스레드

private:
	CRioCoreRef			_rioCore = nullptr;					// 연동된 RIO 코어 참조 (생성자에서 주입받음)
	CRioListenerRef		_listener = nullptr;				// RIO 접속 수락 리스너
	CRioSessionManager	_sessionManager;					// 서버 서비스가 소유하는 RIO 세션 매니저
	CRioEventPool		_eventPool;							// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef		_globalRecvBuffer;					// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
	uint32				_workerThreadCount = 0;				// StartWorkers()에 넘길 워커 스레드 개수 (0=자동)
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
	CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1, uint32 workerThreadCount = 0);

	//***************************************************************************
	// @brief 소멸자 (기본 소멸자 — 실제 자원 정리는 Close()가 담당)
	//***************************************************************************
	virtual ~CRioClientService() = default;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 구동합니다 (코어 초기화, 멀티 워커 시작, N개 세션 연결).
	// @return bool 이벤트 풀/코어/수신 버퍼 초기화, ConnectEx 전용 디스패처 시작,
	//         요청한 세션 수만큼의 연결 "게시"가 모두 성공하면 true.
	//         [중요] ConnectAsync()가 ConnectEx 기반 진짜 비동기로 바뀌면서, 이
	//         함수가 true를 반환해도 실제 TCP 연결이 전부(혹은 하나라도) 완료됐다는
	//         보장이 없습니다 — 연결 성공/실패는 각 세션의 OnConnected()/
	//         OnDisconnected(reason) 오버라이드로 나중에 비동기 통지됩니다(IOCP의
	//         CIocpClientService::Start()와 동일한 계약 변화). 세션 하나를 연결
	//         게시하는 실제 절차는 ConnectOneMoreSession()에 있습니다.
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 종료합니다.
	// @details 순서: 모든 세션에 종료 통지 → _connectDispatcher 정지(진행 중이던
	//          ConnectEx 게시들을 더 이상 처리하지 않음) → _rioCore RequestStop()+
	//          Shutdown()(outstanding I/O drain 완료 보장) → _globalRecvBuffer
	//          해제 → 부모 CNetService::Close().
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
	// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
	// @details 세션 생성 → 서비스에 즉시 등록 → CRioSession::ConnectAsync()로
	//          ConnectEx 비동기 게시, 순서로 동작합니다(RIOCreateRequestQueue/
	//          Init()/PostInitialReceive()는 더 이상 이 함수가 직접 하지 않고
	//          CRioSession::ProcessConnectEx()가 연결 완료 통지를 받은 뒤 이어서
	//          수행합니다 — RioSession.h 클래스 설명 참고).
	//
	//          [중요 — 반환값의 의미가 "연결 완료"가 아님] 이 함수는 "세션을 만들고
	//          연결 시도를 게시하는 데까지 성공했는지"만 동기로 알려줍니다. 실제
	//          TCP 연결 성공/실패는 반환된 세션의 OnConnected()/OnDisconnected(reason)
	//          오버라이드로 나중에 비동기 통지됩니다 — 호출부(예: HTTP 커넥션 풀)는
	//          반환된 CRioSessionRef를 즉시 "사용 가능한 커넥션"으로 취급해서는 안
	//          됩니다. IOCP의 ConnectOneMoreSession()과 이제 완전히 동일한 계약입니다.
	//
	//          세션은 ConnectEx 게시 이전에 이미 AddSession()으로 서비스의 추적
	//          목록에 들어갑니다. 연결이 실패하면 CRioSession::FailConnect()가
	//          호출하는 CSession::OnDisconnected() → DisconnectHandler →
	//          CNetService::ReleaseSession() 경로로 자동 제거되므로, 실패한 세션이
	//          목록에 남는 leak은 없습니다.
	//
	//          _maxSessionCount 상한 체크는 호출자 책임입니다(이전 버전에 있던
	//          "여러 건의 배치 롤백" 개념은 사라졌습니다 — 개별 세션 실패가 이제
	//          비동기이고 자체적으로 정리되므로, Start() 루프도 더 이상 일괄
	//          롤백을 수행하지 않습니다. IOCP Start()와 동일한 설계 변화).
	// @return CRioSessionRef 세션 생성 + 서비스 등록 + ConnectEx 게시 "시도" 자체가
	//         전부 성공하면 세션 참조(연결 완료 보장 아님), 그 전 단계 실패 시 nullptr.
	//***************************************************************************
	CRioSessionRef	ConnectOneMoreSession();

private:
	//***************************************************************************
	// @brief 새 세션에 부여할 고유 SessionId를 원자적으로 발급합니다.
	// @details [수정 — CRioSessionManager 멤버 자체를 제거] 이전에는 이
	//          클래스가 CRioSessionManager(클러스터 해시맵 전체)를 멤버로 갖고
	//          있었는데, 실제로 쓰는 기능은 GenerateSessionId()/AddSession()
	//          뿐이었다. SessionId로 세션을 조회하거나(FindSession) 전체에
	//          브로드캐스트해야(Broadcast) 하는 건 "외부에서 특정 세션을 ID로
	//          찾아야 하는" 서버 역할에 필요한 기능이지, 커넥션 풀처럼 스스로
	//          연결을 만들고 관리하는 클라이언트 역할에는 필요 없다 —
	//          CIocpClientService가 애초에 CIocpSessionManager를 아예 안 갖고
	//          CNetService::_sessions만 쓰는 것과 동일한 이유. 무거운 클러스터
	//          맵 전체 대신 원자적 카운터 하나로 충분하다(이 카운터 자체가
	//          reap이 필요한 상태를 아예 안 만드므로, 이전에 추가했던 이
	//          클래스의 세션 reap 스레드/큐도 함께 제거함).
	//***************************************************************************
	uint64 GenerateSessionId() noexcept { return _nextSessionId.fetch_add(1, std::memory_order_relaxed); }

private:
	CRioCoreRef				_rioCore = nullptr;						// 연동된 RIO 코어 참조 (생성자에서 주입받음)
	std::atomic<uint64>		_nextSessionId{ 0 };					// GenerateSessionId() 전용 카운터 (CRioSessionManager 대체)
	CRioEventPool			_eventPool;								// 이 서비스 소속 세션들이 공유하는 RIO 이벤트 풀
	CRioBufferRef			_globalRecvBuffer;						// 클라이언트 비동기 수신(RIOReceive)용 글로벌 CRioBuffer 객체
	CRioConnectDispatcher	_connectDispatcher;						// ConnectEx 완료 통지 전용 디스패처 (CRioCore와 무관, 이 서비스가 소유)
	uint32					_workerThreadCount = 0;					// StartWorkers()에 넘길 워커 스레드 개수 (0=자동)
};

#endif // ndef UC_RIOSERVICE_H