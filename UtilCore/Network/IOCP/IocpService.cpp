
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
CIocpServerService::CIocpServerService(const CNetAddress& address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Server, address, std::move(factory), maxSessionCount), _iocpCore(std::move(iocpCore)), _workerThreadCount(workerThreadCount)
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

	// 1. CThreadManager를 통해 워커 스레드 풀 구동 (자동 TLS 초기화 및 종료 감지 적용)
	if( !_workers.Start(_iocpCore.get(), _workerThreadCount) )
	{
		Close();
		return false;
	}

	// 2. Listener 생성 및 AcceptEx 개시
	_listener = MakeShared<CIocpListener>();
	if( _listener == nullptr )
	{
		Close();
		return false;
	}

	_listener->SetAddressMode(_listenAddressMode);

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
			if( session == nullptr )
				return nullptr;

			// CNetService::CreateSession()가 설치한 기본 해제 핸들러를 서버 전용 핸들러로
			// 교체한다 — 서비스 추적 목록(_sessions)뿐 아니라 세션 매니저에서도 즉시 제거한다.
			session->SetDisconnectHandler([serviceWeak](CSessionRef disconnected)
				{
					if( auto svc = serviceWeak.lock() )
						svc->OnSessionDisconnected(std::move(disconnected));
				});

			return std::static_pointer_cast<CIocpSession>(session);
		},
		_acceptPoolSize,
		[serviceWeak](CIocpObjectRef session, CNetAddress netAddr)
		{
			auto service = serviceWeak.lock();
			if( service == nullptr )
				return;

			CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);
			if( iocpSession == nullptr )
				return;

			// 동시 접속 슬롯 예약. 상한을 적용 중이고 초과하면 서비스에 등록하지 않고 버린다 —
			// 서비스/매니저에 등록된 적이 없는 세션이라 이 콜백이 반환되며 참조가 사라지면
			// 소멸자가 소켓을 닫는다(OnConnected/OnDisconnected는 호출되지 않는다).
			const int32 active = service->_activeSessionCount.fetch_add(1, std::memory_order_acq_rel) + 1;
			if( service->_enforceMaxSessionCount.load(std::memory_order_relaxed) && active > service->GetMaxSessionCount() )
			{
				service->_activeSessionCount.fetch_sub(1, std::memory_order_acq_rel);

				const uint64 rejected = service->_rejectedSessionCount.fetch_add(1, std::memory_order_relaxed) + 1;
				if( rejected == 1 || rejected % 1000 == 0 )
				{
					LOG_WARNING(_T("[CIocpServerService] max session count reached (%d) - connection rejected (total rejected=%llu)"),
						service->GetMaxSessionCount(), rejected);
				}
				return;
			}

			iocpSession->SetNetAddress(netAddr);

			// 세션 ID는 서비스/매니저 등록과 ProcessConnect() 이전에 부여한다 — 이후 어느
			// 경로로 해제 통지가 와도 OnSessionDisconnected()가 ID로 슬롯 반환 여부를 판단한다.
			const uint64 sessionId = service->GetSessionManager().GenerateSessionId();
			iocpSession->SetSessionId(sessionId);

			// 서비스가 종료 중이면 AddSession()이 세션을 거부하고 Disconnect()까지 요청한다. 그 세션은
			// 해제 통지(OnSessionDisconnected)가 위에서 예약한 동시 접속 슬롯을 이미 반환하므로 여기서는
			// 매니저 등록이나 ProcessConnect() 없이 끝낸다 (이어서 진행하면 OnDisconnected()가 두 번 통지된다).
			if( !service->AddSession(iocpSession) )
				return;

			service->GetSessionManager().AddSession(sessionId, iocpSession);

			// 서비스에 지정된 TCP 소켓 옵션이 있으면 세션에 전달한다 (ProcessConnect()가 OnConnected() 직전에 적용).
			CIocpSession::SocketOptions socketOptions;
			if( service->GetSessionSocketOptions(socketOptions) )
				iocpSession->SetSocketOptions(socketOptions);

			iocpSession->ProcessConnect();
		}
	);

	if( !result )
	{
		Close();
		return false;
	}

	// 3. 세션 reap 스레드 시작 — 해제 통지 경로(OnSessionDisconnected)가 놓친
	//    _sessionManager 엔트리를 Iocp::kSessionReapInterval마다 정리하는 안전망이다.
	//    Listener/워커가 이미 정상 구동 중인 이 시점 이후에 시작해야, reap 스레드가
	//    도는 동안 세션이 실제로 늘어나는 정상 상태와 겹쳐도 안전하다.
	// 큐는 Start()마다 새로 만든다(Stop()된 큐는 재사용하지 않는다). 스레드 생성/예약이 예외로 실패하면
	// 이미 구동한 Listener/워커를 Close()로 되돌리고 실패를 반환한다 — Start()에서 예외가 새면
	// 서비스가 반쯤 구동된 채 남는다.
	try
	{
		_sessionReapQueue = std::make_unique<CDelayedTaskQueue>();
		CDelayedTaskQueue* const reapQueue = _sessionReapQueue.get();
		_sessionReapThread = std::thread([reapQueue]() { reapQueue->ProcessExpiredTasks(); });
		ScheduleSessionReap();
	}
	catch( ... )
	{
		LOG_ERROR(_T("[CIocpServerService] failed to start session reap thread"));
		Close();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 세션 해제 통지(OnDisconnected) 시 호출되는 서버 전용 핸들러입니다.
// @param session 해제된 세션
// @details 세션 매니저에서 즉시 제거하고 동시 접속 슬롯을 반환한 뒤, 서비스의 추적
//          목록에서도 제거합니다(CNetService::Close()의 대기 해제 포함). 매니저 제거를
//          먼저 하는 이유는 ReleaseSession()이 Close() 대기를 깨우는 시점에 매니저도
//          이미 정리돼 있게 하기 위함입니다.
//***************************************************************************
void CIocpServerService::OnSessionDisconnected(CSessionRef session)
{
	if( session == nullptr )
		return;

	CIocpSessionRef iocpSession = std::static_pointer_cast<CIocpSession>(session);

	const uint64 sessionId = iocpSession->GetSessionId();
	if( sessionId != 0 )
	{
		_sessionManager.RemoveSession(sessionId);
		_activeSessionCount.fetch_sub(1, std::memory_order_acq_rel);
	}

	ReleaseSession(session);
}

//***************************************************************************
// @brief _sessionManager에 다음 reap tick을 예약합니다(self-rescheduling).
//***************************************************************************
void CIocpServerService::ScheduleSessionReap()
{
	if( _sessionReapQueue == nullptr )
		return;

	_sessionReapQueue->Reserve(Iocp::kSessionReapInterval, [this]()
		{
			_sessionManager.RemoveClosedSessions();
			ScheduleSessionReap(); // 다음 tick 재예약. Close() 이후엔 Reserve()가 false를 반환하며 조용히 멈춤.
		});
}

//***************************************************************************
// @brief 서버 종료 처리
// @details
// 순서:
// 0. 세션 reap 스레드 정지
// 1. Listener 정지 (신규 연결 차단)
// 2. 세션 매니저의 모든 세션 종료 게시
// 3. 워커 스레드가 살아있는 동안 세션 해제 통지(OnDisconnected)가 모두 끝날 때까지 대기
//    (Iocp::kCloseDrainTimeout 상한)
// 4. 세션 매니저에 남은 해제 세션 정리
// 5. 워커 스레드를 깨우고 Join
// 6. CNetService::Close()로 3번에서 끝내지 못한 세션을 강제 정리
//***************************************************************************
void CIocpServerService::Close()
{
	// 0. reap 스레드부터 정지 — 아래에서 _sessionManager를 직접 조작하는
	//    (BeginCloseAllSessions/RemoveClosedSessions) 동안 reap 스레드가
	//    동시에 같은 맵을 건드리지 않도록 가장 먼저 멈춘다. Stop()은 신호만
	//    보내고 반환하므로 반드시 join까지 해야 한다(CDelayedTaskQueue의
	//    Lifetime 계약).
	if( _sessionReapQueue )
		_sessionReapQueue->Stop();
	if( _sessionReapThread.joinable() )
		_sessionReapThread.join();

	// 1. 신규 연결 차단을 가장 먼저 (RIO 쪽과 동일한 이유 — 세션 정리 도중에도
	//    계속 새 세션이 들어와 BeginCloseAllSessions()의 스냅샷에서 누락되는
	//    상황을 막기 위함). CloseSocket()이 아니라 Stop()을 쓰는 이유: Stop()이
	//    _closing을 먼저 세워, 취소된 AcceptEx의 완료 통지가 재시도 경로(스레드 생성)를
	//    타지 않고 즉시 포기되게 한다.
	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

	// 2. 기존 세션 종료 게시
	_sessionManager.BeginCloseAllSessions();

	// 3. 세션 해제 통지가 실제로 끝날 때까지 대기 — pending I/O와 DisconnectEx의 완료
	//    통지는 워커 스레드가 처리하므로 워커가 살아있는 동안에 해야 한다. 응답 없는
	//    피어처럼 상한 시간 안에 끝나지 않는 세션은 6단계에서 강제 정리한다.
	WaitForSessionsEmpty(Iocp::kCloseDrainTimeout);

	// 4. 해제 통지 경로가 정리하지 못한 매니저 엔트리 정리
	_sessionManager.RemoveClosedSessions();

	// 5. 세션 정리가 끝난 뒤 워커 스레드 정지
	_workers.Stop(_iocpCore.get());

	// 6. 3번에서 끝내지 못한 세션이 있으면 CNetService::Close()가 Disconnect()의
	//    강제 정리 경로로 마무리하고, 서비스의 세션이 0개가 될 때까지 대기한다.
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
CIocpClientService::CIocpClientService(const CNetAddress& address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Client, address, std::move(factory), maxSessionCount), _iocpCore(std::move(iocpCore)), _workerThreadCount(workerThreadCount), _remoteAddress(address)
{
}

//***************************************************************************
// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
// @details Start()의 접속 루프 본체와 동일한 절차(세션 생성 → IOCP Core 등록 → 서비스에 즉시 등록 →
//          ConnectEx 비동기 게시)를 수행합니다.
//
//          [비동기 계약] CIocpSession::ConnectAsync()가 ConnectEx 기반 비동기라, 이 함수는 연결이
//          끝난 세션이 아니라 "연결 시도를 게시한 세션"을 반환합니다. 실제 연결 성공/실패는 세션의
//          OnConnected()/OnDisconnected()로 통지됩니다. 자세한 계약은 헤더의
//          ConnectOneMoreSession() 주석 참고.
//
//          Register()나 CreateSession() 실패 시 로컬 shared_ptr(session/iocpSession)이 스코프를
//          벗어나며 소멸자가 소켓을 정리하므로 별도 정리가 필요 없습니다. ConnectAsync() 자신의 모든
//          실패 경로는 내부에서 FailConnect()를 호출해 정리 및 OnDisconnected() 통지까지 완료합니다.
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

	// 접속 대상은 SetRemoteAddress()로 바뀔 수 있으므로 한 번 읽어 이 연결 전체에 같은 주소를 쓴다.
	const CNetAddress remote = GetRemoteAddress();
	iocpSession->SetNetAddress(remote);

	// 연결 완료를 기다리지 않고 즉시 추적 목록에 등록합니다. 연결이 실패하면
	// CIocpSession::FailConnect()가 호출하는 CSession::OnDisconnected()가
	// DisconnectHandler(ReleaseSession 콜백)를 통해 자동으로 제거하므로,
	// "연결 시도 중" 세션이 목록에 남는 leak은 없습니다.
	//
	// 서비스가 종료 중이면 AddSession()이 세션을 거부하고 Disconnect()까지 요청해 해제 통지를 이미
	// 마쳤으므로, ConnectAsync()를 게시하지 않고 실패로 반환한다(이어서 진행하면 OnDisconnected()가
	// 두 번 통지된다).
	if( !AddSession(iocpSession) )
		return nullptr;

	// ConnectAsync()의 반환값은 "게시 시도" 성공 여부일 뿐입니다 — false든 true든
	// 최종 연결 결과는 세션의 OnConnected()/OnDisconnected()로 비동기 통지됩니다.
	// 여기서는 게시 자체의 성공 여부만 보고합니다.
	{
		CIocpSession::SocketOptions socketOptions;
		if( GetSessionSocketOptions(socketOptions) )
			iocpSession->SetSocketOptions(socketOptions);
	}

	if( !iocpSession->ConnectAsync(remote) )
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

	// 1. 클라이언트 워커 스레드 풀 구동
	if( !_workers.Start(_iocpCore.get(), _workerThreadCount) )
	{
		Close();
		return false;
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
// @details
// 순서:
// 1. 신규 세션 등록을 막고 모든 세션의 종료를 게시
// 2. 워커 스레드가 살아있는 동안 세션 해제 통지(OnDisconnected)가 모두 끝날 때까지 대기
//    (Iocp::kCloseDrainTimeout 상한) — pending I/O와 DisconnectEx의 완료 통지는 워커가 처리하므로
//    워커를 먼저 멈추면 끝나지 않는다.
// 3. 세션 정리가 끝난 뒤 워커 스레드를 깨우고 Join
// 4. 2번에서 끝내지 못한 세션(응답 없는 피어 등)은 CNetService::Close()가 Disconnect()의
//    강제 정리 경로로 마무리한다. CNetService::Close() 자체는 상한 없이 대기하지만, 강제 정리
//    경로는 즉시 통지하므로 이 단계에서는 오래 걸리지 않는다.
//***************************************************************************
void CIocpClientService::Close()
{
	// 1. 종료 진입: 이후 ConnectOneMoreSession()이 등록하려는 신규 세션을 거부하게 하고, 기존 세션의
	//    종료를 게시한다(스냅샷 수집과 락 밖 Disconnect()는 CloseSessions()가 담당 — 락을 쥔 채
	//    Disconnect()를 호출하면 그 안에서 ReleaseSession()이 같은 락을 잡아 교착된다). 대기는 하지 않는다.
	CloseSessions(false);

	// 2. 워커가 살아있는 동안 해제 통지 대기 (상한 적용)
	WaitForSessionsEmpty(Iocp::kCloseDrainTimeout);

	// 3. 워커 스레드 정지
	_workers.Stop(_iocpCore.get());

	// 4. 2번에서 끝내지 못한 세션 강제 정리 (종료 상태도 여기서 해제된다)
	CNetService::Close();
}