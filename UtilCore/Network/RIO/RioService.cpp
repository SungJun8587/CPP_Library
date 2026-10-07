
//***************************************************************************
// RioService.cpp: implementation of the CRioService classes.
//
//***************************************************************************

#include "pch.h"
#include "RioService.h"

//***************************************************************************
// CRioServerService Implementation
//***************************************************************************

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
CRioServerService::CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Server, address, std::move(factory), maxSessionCount), _rioCore(std::move(rioCore)), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 서버 구동 및 Listener 초기화 및 시작
// @note RioSessionFactory와 OnRioAcceptCallback 람다는 this가 아니라
//       weak_ptr<CRioServerService>를 캡처합니다(순환 참조 방지).
//       송신 버퍼는 이 서비스가 등록하지 않습니다 — 세션 자신의
//       CRioSession::Init()이 자기 소유 CRingBuffer를 직접 RIORegisterBuffer()합니다.
//       워커 개수 0은 CRioCore가 자동 산정합니다(hardware_concurrency()/2).
//***************************************************************************
bool CRioServerService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 이벤트 풀 / RIO 코어 / 워커 스레드 그룹 / 글로벌 수신 버퍼 준비
	if( !CRioServiceHelper::StartCore(*_rioCore, _eventPool, _globalRecvBuffer, Rio::kServerCqIdentifier, _workerThreadCount) )
		return false;

	// 2. Listener 생성. MakeShared는 실패 시 nullptr이 아니라 예외를 던진다.
	try
	{
		_listener = MakeShared<CRioListener>();
	}
	catch( ... )
	{
		LOG_ERROR(_T("[CRioServerService] failed to create listener"));
		CRioServiceHelper::StopCore(_rioCore, _globalRecvBuffer, _eventPool);
		return false;
	}

	std::weak_ptr<CRioServerService> weakService = std::static_pointer_cast<CRioServerService>(shared_from_this());

	_listener->SetAddressMode(_listenAddressMode);

	// 3. Listener 구동 시작 (Accept IOCP + AcceptContext Pool 기반 — CRioListener 참고)
	//    acceptPoolSize/acceptWorkerCount는 기본값(Rio::kDefaultAcceptPoolSize/
	//    kDefaultAcceptWorkerCount)을 쓴다. 대량 동시 접속 환경이라면 명시적으로 늘려 호출한다.
	const bool result = _listener->StartAccept(
		_rioCore,
		_address,
		//***********************************************************************
		// @brief 클라이언트 접속마다 세션 객체를 생성하는 팩토리 콜백
		//***********************************************************************
		[weakService]() -> CRioSessionRef
		{
			auto service = weakService.lock();
			if( !service ) return nullptr;

			CSessionRef session = service->CreateSession();
			if( session == nullptr )
			{
				// 최대 세션 수에 도달한 동안 접속이 몰리면 매번 기록하지 않도록 첫 회와 1000회마다만 남긴다.
				static std::atomic<uint64> failedCount{ 0 };
				const uint64 failed = failedCount.fetch_add(1, std::memory_order_relaxed) + 1;
				if( failed == 1 || failed % 1000 == 0 )
					LOG_WARNING(_T("[CRioServerService] CreateSession() returned nullptr (max session count reached?) - total=%llu"), failed);
			}
			return std::static_pointer_cast<CRioSession>(session);
		},
		//***********************************************************************
		// @brief Accept 완료 시 세션을 초기화하고 서비스/세션 매니저에 등록하는 콜백
		//***********************************************************************
		[weakService](CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)
		{
			auto service = weakService.lock();
			if( !service )
			{
				CSocketUtils::Close(clientSocket);
				return;
			}

			if( session == nullptr )
			{
				LOG_ERROR(_T("[Error] Session is nullptr inside accept callback!"));
				CSocketUtils::Close(clientSocket);
				return;
			}

			const uint64 sessionId = service->GetSessionManager().GenerateSessionId();

			if( !session->Init(sessionId, service->GetRioCore().get(), service->GetGlobalRecvBuffer(), clientSocket, requestQueue) )
			{
				LOG_ERROR(_T("[Error] CRioSession::Init failed (send buffer registration)!"));
				CSocketUtils::Close(clientSocket);
				return;
			}

			session->SetNetAddress(netAddr);

			// 서비스가 종료 중이면 등록이 거부되고, 거부된 세션은 AddSession()이 이미 Disconnect했다.
			if( !service->AddSession(session) )
				return;

			service->GetSessionManager().AddSession(sessionId, session);

			session->PostInitialReceive();
		}
	);

	if( !result )
	{
		_listener.reset();
		CRioServiceHelper::StopCore(_rioCore, _globalRecvBuffer, _eventPool);
		return false;
	}

	// 4. 세션 reap 스레드 시작 — Running 중 자연 종료된 세션의 _sessionManager 엔트리를
	//    Rio::kSessionReapInterval마다 정리한다. Listener/워커가 정상 구동 중인 이 시점
	//    이후에 시작해야, 세션이 실제로 늘어나는 정상 상태와 겹쳐도 안전하다.
	//    큐는 Start()마다 새로 만든다. 스레드 생성/예약이 예외로 실패하면 구동한 Listener/워커를
	//    Close()로 되돌리고 실패를 반환한다(예외가 새면 서비스가 반쯤 구동된 채 남는다).
	try
	{
		_sessionReapQueue = std::make_unique<CDelayedTaskQueue>();
		CDelayedTaskQueue* const reapQueue = _sessionReapQueue.get();
		_sessionReapThread = std::thread([reapQueue]() { reapQueue->ProcessExpiredTasks(); });
		ScheduleSessionReap();
	}
	catch( ... )
	{
		LOG_ERROR(_T("[CRioServerService] failed to start session reap thread"));
		Close();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief _sessionManager에 다음 reap tick을 예약합니다(self-rescheduling).
//***************************************************************************
void CRioServerService::ScheduleSessionReap()
{
	if( _sessionReapQueue == nullptr )
		return;

	_sessionReapQueue->Reserve(Rio::kSessionReapInterval, [this]()
		{
			_sessionManager.RemoveClosedSessions();
			ScheduleSessionReap(); // 다음 tick 재예약. Close() 이후엔 Reserve()가 false를 반환하며 조용히 멈춤.
		});
}

//***************************************************************************
// @brief 서버 종료 처리
// @details Listener를 먼저 멈춰 신규 연결(및 그로부터 생성될 새 세션)을 차단한 뒤에
//          기존 세션을 정리해야, 세션 종료 도중 들어온 세션이 BeginCloseAllSessions()
//          스냅샷에서 빠지는 일이 없다. 세션은 CNetService::_sessions에도 등록되어
//          있으므로(Start()의 accept 콜백), 모든 세션의 OnDisconnected() 이후
//          ReleaseSession()까지 끝나 _sessions가 빌 때까지 기다린다.
//          그 뒤 _rioCore->Shutdown()이 outstanding I/O drain을 보장하고 나면
//          수신 버퍼와 이벤트 풀도 해제한다.
//***************************************************************************
void CRioServerService::Close()
{
	// 0. reap 스레드부터 정지 — 아래에서 _sessionManager를 직접 조작하는 동안 같은 맵을
	//    동시에 건드리지 않도록 가장 먼저 멈춘다. Stop()은 신호만 보내고 반환하므로
	//    반드시 join까지 해야 한다(CDelayedTaskQueue의 Lifetime 계약).
	if( _sessionReapQueue )
		_sessionReapQueue->Stop();
	if( _sessionReapThread.joinable() )
		_sessionReapThread.join();

	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

	_sessionManager.BeginCloseAllSessions();

	if( !WaitForSessionsEmpty(Rio::kDefaultDrainTimeout) )
	{
		LOG_ERROR(_T("[CRioServerService] Close timeout: %d session(s) remaining"), GetCurrentSessionCount());
	}
	_sessionManager.RemoveClosedSessions();

	CRioServiceHelper::StopCore(_rioCore, _globalRecvBuffer, _eventPool);

	CNetService::Close();
}


//***************************************************************************
// CRioClientService Implementation
//***************************************************************************

//***************************************************************************
// @brief CRioClientService 생성자
// @param address 접속할 서버의 네트워크 주소 정보
// @param rioCore 이 서비스가 사용할 RIO Core 참조(공유 소유). Start()가
//        이 인스턴스를 그대로 Initialize()합니다.
// @param factory 세션 객체 생성을 위한 팩토리 함수
// @param maxSessionCount 생성 및 관리할 최대 클라이언트 세션 수 (기본값: 1)
// @param workerThreadCount RIO 완료 처리용 워커 스레드 개수 (기본값 0 = 자동 산정)
//***************************************************************************
CRioClientService::CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount, uint32 workerThreadCount)
	: CNetService(NetServiceType::Client, address, std::move(factory), maxSessionCount), _rioCore(std::move(rioCore)), _workerThreadCount(workerThreadCount)
{
	_remoteAddress.Set(address);
}

//***************************************************************************
// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
// @details 세션 생성 → 서비스 등록 → CRioSession::ConnectAsync()로 ConnectEx 비동기
//          게시 순서로 진행합니다. RIOCreateRequestQueue/Init()/PostInitialReceive()는
//          연결 완료 통지(_connectDispatcher 워커 스레드)를 받은 CRioSession::
//          ProcessConnectEx()가 이어서 수행합니다. 반환값의 의미는 헤더의
//          ConnectOneMoreSession() 주석을 참고하세요.
//
//          서비스가 종료 중이라 AddSession()이 거부하면 연결을 게시하지 않고 nullptr을
//          반환합니다. 이 세션은 아직 Active가 된 적이 없어 별도 정리할 자원이 없고,
//          로컬 shared_ptr이 스코프를 벗어나며 소멸자가 정리합니다.
//          ConnectAsync()의 모든 실패 경로는 내부에서 FailConnect()를 호출해 정리 및
//          OnDisconnected(reason) 통지까지 완료합니다.
// @return CRioSessionRef 세션 생성 + 서비스 등록 + ConnectEx 게시까지 성공하면
//         세션 참조(연결 완료 보장 아님), 그 전 단계 실패 시 nullptr.
//***************************************************************************
CRioSessionRef CRioClientService::ConnectOneMoreSession()
{
	if( _rioCore == nullptr || _globalRecvBuffer == nullptr )
		return nullptr;

	CSessionRef session = CreateSession();
	if( session == nullptr )
		return nullptr;

	CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);
	if( rioSession == nullptr )
		return nullptr;

	// 접속 대상은 SetRemoteAddress()로 바뀔 수 있으므로 한 번 읽어 이 연결 전체에 같은 주소를 쓴다.
	const CNetAddress remote = GetRemoteAddress();
	rioSession->SetNetAddress(remote);

	// 연결 완료를 기다리지 않고 즉시 추적 목록에 등록합니다. 연결이 실패하면
	// CRioSession::FailConnect()가 호출하는 CSession::OnDisconnected()가
	// DisconnectHandler(ReleaseSession 콜백)를 통해 자동으로 제거하므로
	// "연결 시도 중" 세션이 목록에 남지 않습니다.
	if( !AddSession(rioSession) )
		return nullptr;

	const uint64 sessionId = GenerateSessionId();

	// ConnectAsync()의 반환값은 "게시 시도" 성공 여부일 뿐입니다 — 최종 연결 결과는
	// 세션의 OnConnected()/OnDisconnected(reason)으로 비동기 통지됩니다.
	if( !rioSession->ConnectAsync(_connectDispatcher, sessionId, _rioCore.get(), _globalRecvBuffer.get(), remote) )
		return nullptr;

	return rioSession;
}

//***************************************************************************
// @brief 클라이언트 구동, 이벤트 풀/코어/워커/수신 버퍼/ConnectEx 디스패처 초기화
//        및 세션 연결 게시
// @return bool 성공 여부. [중요] true를 반환해도 각 세션의 실제 TCP 연결이
//         완료됐다는 뜻이 아닙니다 — 헤더의 Start() 주석 참고.
// @details
// - 1. 이벤트 풀/RIO 코어/워커/글로벌 수신 버퍼를 준비합니다(CRioServiceHelper).
//      세션 등록 전에 코어가 Running 상태여야 PostInitialReceive()가 정상 동작합니다.
// - 2. ConnectEx 완료 통지 전용 디스패처(_connectDispatcher)를 시작합니다
//      (CRioCore와 완전히 분리된 자기 소유 IOCP + 워커 스레드 1개).
// - 3. _maxSessionCount 개수만큼 ConnectOneMoreSession()을 호출해 세션을 생성하고
//      ConnectEx 비동기 연결을 게시합니다. 게시 시도 자체가 하나라도 실패하면
//      Close()로 전체를 정리하고 false를 반환합니다. 개별 연결 실패는 각 세션이
//      비동기로 스스로 정리합니다(IOCP Start()와 동일한 설계).
//***************************************************************************
bool CRioClientService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 이벤트 풀 / RIO 코어 / 워커 스레드 그룹 / 글로벌 수신 버퍼 준비
	if( !CRioServiceHelper::StartCore(*_rioCore, _eventPool, _globalRecvBuffer, Rio::kClientCqIdentifier, _workerThreadCount) )
		return false;

	// 2. ConnectEx 완료 통지 전용 디스패처 시작 (CRioCore와 무관한 별도 IOCP)
	if( !_connectDispatcher.Start() )
	{
		LOG_ERROR(_T("[Error] CRioConnectDispatcher Start failed!"));
		CRioServiceHelper::StopCore(_rioCore, _globalRecvBuffer, _eventPool);
		return false;
	}

	// 3. 세션 연결 게시 (실제 절차는 ConnectOneMoreSession()에 위임)
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
// @details 먼저 CNetService::Close()로 모든 세션을 종료하고 _sessions가 빌 때까지
//          기다립니다. 이 시점에는 _rioCore/_connectDispatcher가 아직 살아 있어
//          disconnect 완료 통지와 진행 중이던 ConnectEx 완료 통지를 계속 처리할 수
//          있습니다(연결 진행 중인 세션은 CRioSession::Close()가 ConnectEx를 취소해
//          완료 통지가 오게 합니다). 이를 먼저 멈추면 세션 정리가 끝나지 않습니다.
//          그다음 디스패처(자체 IOCP라 _rioCore와 정지 순서 의존성 없음), RIO 코어,
//          수신 버퍼와 이벤트 풀 순으로 정지/해제합니다.
//***************************************************************************
void CRioClientService::Close()
{
	CNetService::Close();

	_connectDispatcher.Shutdown();

	CRioServiceHelper::StopCore(_rioCore, _globalRecvBuffer, _eventPool);
}