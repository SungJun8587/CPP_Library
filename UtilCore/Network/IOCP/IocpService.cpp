
//***************************************************************************
// IocpService.cpp: implementation of the CIocpService classes.
//
//***************************************************************************

#include "pch.h"
#include "IocpService.h"

//***************************************************************************
// CIocpServerService Implementation
//***************************************************************************

//***************************************************************************
// @brief CIocpServerService 생성자 구현
// @param address 서버 바인딩 주소
// @param iocpCore IOCP 코어 객체
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
// @param workerThreadCount 워커 스레드 개수
//***************************************************************************
CIocpServerService::CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _iocpCore(iocpCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 서버 시작, 워커 스레드 풀 구동 및 Listener 초기화
// @return bool 구동 성공 여부
// @details
// - 1. workerThreadCount가 0인 경우 하드웨어 코어 수를 기반으로 워커 스레드 수를 자동 산정합니다.
// - 2. CThreadManager를 통해 스레드를 생성하여 DispatchBatch 루프를 구동합니다.
// - 3. CIocpListener를 생성하고 비동기 AcceptEx 작업을 시작합니다.
//***************************************************************************
bool CIocpServerService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	uint32 workerThreadCount = _workerThreadCount;
	if( workerThreadCount == 0 )
	{
		unsigned int hwThreads = std::thread::hardware_concurrency();
		workerThreadCount = (hwThreads > 0) ? hwThreads : 2;
	}

	// 1. CThreadManager를 통해 워커 스레드 풀 구동 (자동 TLS 초기화 및 종료 감지 적용)
	for( uint32 i = 0; i < workerThreadCount; ++i )
	{
		bool created = _threadManager.CreateThread([this]() {
			while( !_threadManager.IsShuttingDown() )
			{
				_iocpCore->DispatchBatch(10);
			}
			});

		if( !created )
		{
			Close();
			return false;
		}
	}

	// 2. Listener 생성 및 AcceptEx 개시
	_listener = MakeShared<CIocpListener>();
	if( _listener == nullptr )
	{
		Close();
		return false;
	}

	std::weak_ptr<CIocpServerService> serviceWeak = std::static_pointer_cast<CIocpServerService>(shared_from_this());

	bool result = _listener->StartAccept(
		_iocpCore,
		_address,
		[serviceWeak]() -> CIocpObjectRef
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return nullptr;

			CSessionRef session = service->CreateSession();
			return std::static_pointer_cast<CIocpSession>(session);
		},
		10,
		[serviceWeak](CIocpObjectRef session, CNetAddress netAddr)
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return;

			CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
			if( iocpSession )
			{
				iocpSession->SetNetAddress(netAddr);
				service->AddSession(iocpSession);

				uint64 sessionId = service->GetSessionManager().GenerateSessionId();
				iocpSession->SetSessionId(sessionId);
				service->GetSessionManager().AddSession(sessionId, iocpSession);

				iocpSession->ProcessConnect();
			}
		}
	);

	if( !result )
	{
		Close();
		return false;
	}

	// 3. 세션 reap 스레드 시작 — Running 중 자연 종료된 세션의 _sessionManager
	//    엔트리를 kSessionReapInterval마다 정리한다(예전에는 Close() 시점에만
	//    정리돼 서버가 오래 떠 있을수록 map이 무한정 커질 수 있었음). Listener/
	//    워커가 이미 정상 구동 중인 이 시점 이후에 시작해야, reap 스레드가
	//    도는 동안 세션이 실제로 늘어나는 정상 상태와 겹쳐도 안전하다.
	_sessionReapThread = std::thread([this]() { _sessionReapQueue.ProcessExpiredTasks(); });
	ScheduleSessionReap();

	return true;
}

//***************************************************************************
// @brief _sessionManager에 다음 reap tick을 예약합니다(self-rescheduling).
//***************************************************************************
void CIocpServerService::ScheduleSessionReap()
{
	_sessionReapQueue.Reserve(kSessionReapInterval, [this]()
		{
			_sessionManager.RemoveClosedSessions();
			ScheduleSessionReap(); // 다음 tick 재예약. Close() 이후엔 Reserve()가 false를 반환하며 조용히 멈춤.
		});
}

//***************************************************************************
// @brief 서버 종료 처리
// @details
// 순서:
// 1. 소속된 세션 매니저의 모든 세션 일괄 종료
// 2. Listener 소켓 닫기
// 3. PostQueuedCompletionStatus를 호출하여 대기 중인 워커 스레드들을 깨우고 Join 대기
//***************************************************************************
void CIocpServerService::Close()
{
	// 0. reap 스레드부터 정지 — 아래에서 _sessionManager를 직접 조작하는
	//    (BeginCloseAllSessions/RemoveClosedSessions) 동안 reap 스레드가
	//    동시에 같은 맵을 건드리지 않도록 가장 먼저 멈춘다. Stop()은 신호만
	//    보내고 반환하므로 반드시 join까지 해야 한다(CDelayedTaskQueue의
	//    Lifetime 계약).
	_sessionReapQueue.Stop();
	if( _sessionReapThread.joinable() )
		_sessionReapThread.join();

	// 1. 신규 연결 차단을 가장 먼저 (RIO 쪽과 동일한 이유 — 세션 정리 도중에도
	//    계속 새 세션이 들어와 BeginCloseAllSessions()의 스냅샷에서 누락되는
	//    상황을 막기 위함)
	if( _listener )
	{
		_listener->CloseSocket();
		_listener = nullptr;
	}

	// 2. 기존 세션 종료 게시
	_sessionManager.BeginCloseAllSessions();

	// 3. 실제로 다 닫힐 때까지 대기 — 워커 스레드(_threadManager)가 아직
	//    살아있어서 close 완료 통지를 계속 처리해줄 수 있는 동안에 해야 한다.
	//    AreAllSessionsClosed()/BeginCloseAllSessions()/RemoveClosedSessions()가
	//    폴링 전용으로 설계돼 있어(RIO 쪽과 동일) 여기서도 짧은 간격 폴링으로 대기.
	while( !_sessionManager.AreAllSessionsClosed() )
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	_sessionManager.RemoveClosedSessions();

	// 4. 세션 정리가 다 끝난 뒤에야 워커 스레드 정지
	//    [수정] RequestShutdown()으로 종료 플래그를 먼저 세팅한 뒤 PQCS를
	//    게시한다 — 순서가 바뀌면(PQCS 먼저) 깨어난 워커가 IsShuttingDown()을
	//    아직 false로 관측해 DispatchBatch(10)을 한 번 더 돈 뒤에야(최대 10ms)
	//    종료를 인지하는 지연이 생긴다. JoinThreads() 자신도 내부에서 플래그를
	//    세팅하므로 기능적 hang은 없지만, 여기서 먼저 세팅해두면 그 지연이 없다.
	_threadManager.RequestShutdown();
	if( _iocpCore && _iocpCore->GetHandle() != INVALID_HANDLE_VALUE )
	{
		size_t threadCount = _threadManager.GetThreadCount();
		for( size_t i = 0; i < threadCount; ++i )
		{
			::PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, 0, nullptr);
		}
	}
	_threadManager.JoinThreads();

	CNetService::Close();
}

//***************************************************************************
// CIocpClientService Implementation
//***************************************************************************

//***************************************************************************
// @brief CIocpClientService 생성자 구현
// @param address 접속할 서버 주소
// @param iocpCore IOCP 코어 객체
// @param factory 세션 생성 팩토리
// @param maxSessionCount 생성할 세션 개수
// @param workerThreadCount 워커 스레드 개수
//***************************************************************************
CIocpClientService::CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _iocpCore(iocpCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
// @details Start()의 접속 루프 본체와 동일한 절차(세션 생성 → IOCP Core 등록 →
//          서비스에 즉시 등록 → ConnectEx 비동기 게시)를 그대로 수행합니다.
//
//          [비동기 전환] 과거 버전은 동기 connect()를 사용해 이 함수가 "연결까지
//          끝난 세션"을 그 자리에서 반환했습니다. 그 과정에서 connect()가
//          WSAEWOULDBLOCK을 반환해도 곧바로 ProcessConnect()를 호출해버려, TCP
//          핸드셰이크가 실제로 끝나기 전에 WSARecv를 거는 버그가 있었습니다
//          (select() 기반 유계 대기로 임시 수정했던 이력 있음). 지금은
//          CIocpSession::ConnectAsync()가 ConnectEx 기반 진짜 비동기라 그 문제
//          자체가 사라졌습니다 — 대신 이 함수의 반환값 의미가 바뀌었습니다.
//          자세한 계약은 헤더의 ConnectOneMoreSession() 주석 참고.
//
//          Register()나 CreateSession() 실패 시 로컬 shared_ptr(session/
//          iocpSession)이 스코프를 벗어나며 소멸자가 소켓을 정리하므로 별도
//          정리가 필요 없습니다. ConnectAsync() 자신의 모든 실패 경로는 내부에서
//          FailConnect()를 호출해 정리 및 OnDisconnected() 통지까지 완료합니다.
// @return CIocpSessionRef 세션 생성 + IOCP 등록 + ConnectEx 게시까지 성공하면
//         세션 참조(연결 완료 보장 아님), 그 전 단계 실패 시 nullptr.
//***************************************************************************
CIocpSessionRef CIocpClientService::ConnectOneMoreSession()
{
	if( _iocpCore == nullptr )
		return nullptr;

	CSessionRef session = CreateSession();
	if( session == nullptr )
		return nullptr;

	CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
	if( iocpSession == nullptr )
		return nullptr;

	if( _iocpCore->Register(iocpSession) == false )
		return nullptr;

	iocpSession->SetNetAddress(_address);

	// 연결 완료를 기다리지 않고 즉시 추적 목록에 등록합니다. 연결이 실패하면
	// CIocpSession::FailConnect()가 호출하는 CSession::OnDisconnected()가
	// DisconnectHandler(ReleaseSession 콜백)를 통해 자동으로 제거하므로,
	// "연결 시도 중" 세션이 목록에 남는 leak은 없습니다.
	AddSession(iocpSession);

	// ConnectAsync()의 반환값은 "게시 시도" 성공 여부일 뿐입니다 — false든 true든
	// 최종 연결 결과는 세션의 OnConnected()/OnDisconnected()로 비동기 통지됩니다.
	// 여기서는 게시 자체의 성공 여부만 보고합니다.
	if( !iocpSession->ConnectAsync(_address) )
		return nullptr;

	return iocpSession;
}

//***************************************************************************
// @brief 클라이언트 구동, 워커 스레드 시작 및 세션 IOCP 등록 + 연결 게시
// @return bool 성공 여부. [중요] true를 반환해도 각 세션의 실제 TCP 연결이
//         완료됐다는 뜻이 아닙니다 — Start()의 doc 주석 참고.
// @details
// - 1. 워커 스레드 풀을 구동하여 완료 이벤트를 처리할 준비를 합니다.
// - 2. _maxSessionCount 개수만큼 ConnectOneMoreSession()을 호출해 세션을 생성하고
//      IOCP Core에 등록한 뒤 ConnectEx 비동기 연결을 게시합니다. 세션 하나라도
//      "게시 시도" 자체가 실패하면(세션 생성/IOCP 등록 실패 등) 즉시 Close()로
//      전체를 정리하고 false를 반환합니다.
//***************************************************************************
bool CIocpClientService::Start()
{
	if( CanStart() == false || _iocpCore == nullptr )
		return false;

	uint32 workerThreadCount = _workerThreadCount;
	if( workerThreadCount == 0 )
	{
		unsigned int hwThreads = std::thread::hardware_concurrency();
		workerThreadCount = (hwThreads > 0) ? hwThreads : 2;
	}

	// 1. 클라이언트 워커 스레드 풀 구동
	for( uint32 i = 0; i < workerThreadCount; ++i )
	{
		bool created = _threadManager.CreateThread([this]() {
			while( !_threadManager.IsShuttingDown() )
			{
				_iocpCore->DispatchBatch(10);
			}
			});

		if( !created )
		{
			Close();
			return false;
		}
	}

	// 2. 세션 연결 게시 (실제 절차는 ConnectOneMoreSession()에 위임)
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		if( ConnectOneMoreSession() == nullptr )
		{
			Close();
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 클라이언트 서비스 종료 처리
// @details 세션을 정리하고, 대기 중인 워커 스레드들을 깨운 뒤 Join을 수행합니다.
//***************************************************************************
void CIocpClientService::Close()
{
	// 1. 세션 정리를 게시하고, 실제로 세션이 0개가 될 때까지 블로킹 대기한다.
	//    이 시점엔 아직 워커 스레드(_threadManager)가 살아있어서 disconnect
	//    완료 통지를 계속 처리해줄 수 있다 — 그래서 여기서 먼저 기다려야 한다.
	//    2번(워커 스레드 정지)을 먼저 해버리면, 게시된 세션 정리들이 완료 통지를
	//    처리해줄 스레드가 없어져서 영원히 안 끝나는 문제가 생긴다.
	CNetService::Close();	// 세션이 실제로 0개 될 때까지 블로킹 대기

	// 2. 세션 정리가 다 끝났으니, 이제 워커 스레드들에게 종료 신호를 보낸다.
	//    [수정] RequestShutdown()을 PQCS 게시보다 먼저 호출 — 이유는
	//    CIocpServerService::Close()와 동일(깨어난 워커가 곧바로 종료를
	//    인지하도록, 최대 10ms 지연 제거).
	_threadManager.RequestShutdown();
	if( _iocpCore && _iocpCore->GetHandle() != INVALID_HANDLE_VALUE )
	{
		// 2-1. 워커 스레드 개수만큼 wake-up(빈 overlapped) 패킷을 게시한다.
		//      GetQueuedCompletionStatus()로 블로킹 중인 각 워커 스레드가
		//      이 패킷을 하나씩 받아 깨어나 종료 조건을 확인하게 하기 위함.
		size_t threadCount = _threadManager.GetThreadCount();
		for( size_t i = 0; i < threadCount; ++i )
		{
			::PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, 0, nullptr);
		}
	}

	// 3. 모든 워커 스레드가 실제로 종료될 때까지 join으로 확실하게 대기한다.
	_threadManager.JoinThreads();
}