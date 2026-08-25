
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
CRioServerService::CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount, uint32_t workerThreadCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _rioCore(rioCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 서버 구동 및 Listener 초기화 및 시작
// @note RioSessionFactory와 OnRioAcceptCallback 람다는 this가 아니라
//       weak_ptr<CRioServerService>를 캡처합니다(순환 참조 방지).
//       송신 버퍼는 이 서비스가 등록하지 않습니다 — 세션 자신의
//       CRioSession::Init()이 자기 소유 CRingBuffer를 직접 RIORegisterBuffer()합니다.
//       StartWorkers(0, ...)로 워커 개수는 CRioCore가 자동 산정합니다
//       (hardware_concurrency()/2). 특정 개수를 강제하고 싶으면(예: 스트레스
//       테스트) 0 대신 원하는 수를 직접 넘기면 됩니다.
//***************************************************************************
bool CRioServerService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 서버 전용 이벤트 풀 초기화
	if( !_eventPool.Initialize(Rio::kServiceEventPoolCapacity) )
	{
		LOG_ERROR(_T("[Error] CRioEventPool Initialize failed!"));
		return false;
	}

	// RIO 함수 테이블을 얻기 위한 용도의 소켓 생성 (실제 accept용 리슨 소켓은
	// CRioListener가 내부에서 별도로 새로 만듭니다 — 이건 오직 WSAIoctl 조회용).
	SOCKET listenSocket = CSocketUtils::CreateRioSocket();
	if( listenSocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = Rio::kServiceMaxCompletionResults;
	ULONG_PTR cqIdentifier = Rio::kServerCqIdentifier;

	// 2. 생성자에서 주입받은 _rioCore를 초기화
	if( !_rioCore->Initialize(listenSocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		CSocketUtils::Close(listenSocket);
		return false;
	}

	// 3. 멀티 워커 스레드 그룹 구동. 0을 넘기면 CRioCore가 hardware_concurrency()/2로
	//    자동 산정합니다.
	bool workerStarted = _rioCore->StartWorkers(_workerThreadCount, [this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] CRioCore StartWorkers failed!"));
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		CSocketUtils::Close(listenSocket);
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 수신용 글로벌 CRioBuffer 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		CSocketUtils::Close(listenSocket);
		return false;
	}

	// 5. Listener 생성
	_listener = MakeShared<CRioListener>();
	if( _listener == nullptr )
	{
		_globalRecvBuffer.reset();
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		CSocketUtils::Close(listenSocket);
		return false;
	}

	CSocketUtils::Close(listenSocket);

	std::weak_ptr<CRioServerService> weakService = std::static_pointer_cast<CRioServerService>(shared_from_this());

	// 6. Listener 구동 시작 (Accept IOCP + AcceptContext Pool 기반 — CRioListener 참고)
	bool result = _listener->Start(
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
				LOG_WARNING(_T("[Error] CreateSession() returned nullptr! (Max session count reached?)"));
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

			if( session )
			{
				CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);

				uint64_t sessionId = service->GetSessionManager().GenerateSessionId();
				CRioCore* rioCore = service->GetRioCore().get();
				CRioBuffer* globalRecvBuffer = service->GetGlobalRecvBuffer();

				if( !rioSession->Init(sessionId, rioCore, globalRecvBuffer, clientSocket, requestQueue) )
				{
					LOG_ERROR(_T("[Error] CRioSession::Init failed (send buffer registration)!"));
					CSocketUtils::Close(clientSocket);
					return;
				}

				rioSession->SetNetAddress(netAddr);
				service->AddSession(rioSession);
				service->GetSessionManager().AddSession(sessionId, rioSession);

				rioSession->PostInitialReceive();
			}
			else
			{
				LOG_ERROR(_T("[Error] Session is nullptr inside accept callback!"));
				CSocketUtils::Close(clientSocket);
			}
		}
		// acceptPoolSize/acceptWorkerCount는 기본값(CRioListener::kDefaultAcceptPoolSize/
		// kDefaultAcceptWorkerCount) 사용. 대량 동시 접속 환경이라면 이 두 값을
		// 명시적으로 늘려 호출하는 것을 고려할 것.
	);

	if( !result )
	{
		_listener.reset();
		_globalRecvBuffer.reset();
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 서버 종료 처리
// @note [수정] 순서를 "세션 종료 -> Listener 정지"에서 "Listener 정지 -> 세션
//       종료"로 바꿨다. Listener를 먼저 멈춰 신규 연결(및 그로부터 생성될
//       새 세션)을 차단한 뒤에 기존 세션들을 정리해야, 세션 종료 도중에도
//       계속 새 세션이 들어와 BeginCloseAllSessions()의 스냅샷에서 누락되는
//       상황을 원천적으로 막을 수 있다(CRioListener 재설계 문서의 11번 항목
//       권장 순서를 반영).
//***************************************************************************
void CRioServerService::Close()
{
	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

	_sessionManager.BeginCloseAllSessions();

	if( _rioCore )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
	}

	_globalRecvBuffer.reset();

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
CRioClientService::CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount, uint32_t workerThreadCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _rioCore(rioCore), _workerThreadCount(workerThreadCount)
{
}

//***************************************************************************
// @brief 이미 구동 중인 서비스에 세션 하나를 추가로 연결 "게시"합니다.
// @details 세션 생성 → 서비스/세션매니저 즉시 등록 → CRioSession::ConnectAsync()로
//          ConnectEx 비동기 게시. RIOCreateRequestQueue/Init()/PostInitialReceive()는
//          더 이상 여기서 하지 않고 CRioSession::ProcessConnectEx()가 연결 완료
//          통지(_connectDispatcher 워커 스레드)를 받은 뒤 이어서 수행합니다.
//
//          [비동기 전환] 과거 버전은 CSocketUtils::Connect()(완전 블로킹)를 사용해
//          이 함수가 "연결까지 끝난 세션"을 그 자리에서 반환했습니다. 이는 세션
//          N개를 완전 직렬로, RTT만큼씩 순서대로 연결하는 성능 특성을 가졌습니다.
//          지금은 CRioSession::ConnectAsync()가 ConnectEx 기반 진짜 비동기라 그
//          문제가 사라졌습니다 — 대신 이 함수의 반환값 의미가 바뀌었습니다(헤더의
//          ConnectOneMoreSession() 주석 참고).
//
//          CreateSession() 실패 시 로컬 shared_ptr(session/rioSession)이 스코프를
//          벗어나며 소멸자가 소켓을 정리하므로 별도 정리가 필요 없습니다.
//          ConnectAsync() 자신의 모든 실패 경로는 내부에서 FailConnect()를 호출해
//          정리 및 OnDisconnected(reason) 통지까지 완료합니다.
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

	rioSession->SetNetAddress(_address);

	// 연결 완료를 기다리지 않고 즉시 추적 목록에 등록합니다. 연결이 실패하면
	// CRioSession::FailConnect()가 호출하는 CSession::OnDisconnected()가
	// DisconnectHandler(ReleaseSession 콜백)를 통해 자동으로 제거하므로,
	// "연결 시도 중" 세션이 목록에 남는 leak은 없습니다.
	AddSession(rioSession);

	uint64_t sessionId = _sessionManager.GenerateSessionId();
	_sessionManager.AddSession(sessionId, rioSession);

	// ConnectAsync()의 반환값은 "게시 시도" 성공 여부일 뿐입니다 — false든 true든
	// 최종 연결 결과는 세션의 OnConnected()/OnDisconnected(reason)으로 비동기
	// 통지됩니다. 여기서는 게시 자체의 성공 여부만 보고합니다.
	if( !rioSession->ConnectAsync(_connectDispatcher, sessionId, _rioCore.get(), _globalRecvBuffer.get(), _address) )
		return nullptr;

	return rioSession;
}

//***************************************************************************
// @brief 클라이언트 구동, 이벤트 풀/코어/워커/수신 버퍼/ConnectEx 디스패처 초기화
//        및 세션 연결 게시
// @return bool 성공 여부. [중요] true를 반환해도 각 세션의 실제 TCP 연결이
//         완료됐다는 뜻이 아닙니다 — Start()의 doc 주석 참고.
// @details
// - 1. 클라이언트 전용 이벤트 풀을 초기화합니다.
// - 2. RIO 함수 테이블 조회용 더미 소켓으로 _rioCore를 초기화합니다.
// - 3. 멀티 워커 스레드 그룹을 구동합니다(세션 등록 전에 코어가 Running 상태여야
//      PostInitialReceive()가 정상 동작합니다).
// - 4. 글로벌 수신 버퍼를 초기화합니다.
// - 5. ConnectEx 완료 통지 전용 디스패처(_connectDispatcher)를 시작합니다
//      (CRioCore와 완전히 분리된 자기 소유 IOCP + 워커 스레드 1개 — 진단 이력 참고).
// - 6. _maxSessionCount 개수만큼 ConnectOneMoreSession()을 호출해 세션을 생성하고
//      ConnectEx 비동기 연결을 게시합니다. 세션 하나라도 "게시 시도" 자체가
//      실패하면(세션 생성 실패 등) 즉시 Close()로 전체를 정리하고 false를
//      반환합니다. 개별 연결 실패에 대한 배치 롤백은 더 이상 이 함수의 책임이
//      아닙니다(각 세션이 비동기로 스스로 정리됨 — IOCP Start()와 동일한 설계).
//***************************************************************************
bool CRioClientService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 클라이언트 전용 이벤트 풀 초기화
	if( !_eventPool.Initialize(Rio::kServiceEventPoolCapacity) )
	{
		LOG_ERROR(_T("[Error] Client CRioEventPool Initialize failed!"));
		return false;
	}

	SOCKET dummySocket = CSocketUtils::CreateRioSocket();
	if( dummySocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = Rio::kServiceMaxCompletionResults;
	ULONG_PTR cqIdentifier = Rio::kClientCqIdentifier;

	// 2. 생성자에서 주입받은 _rioCore를 초기화
	if( !_rioCore->Initialize(dummySocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		CSocketUtils::Close(dummySocket);
		return false;
	}

	CSocketUtils::Close(dummySocket);

	// 3. 멀티 워커 스레드 그룹 구동. 세션과 수신 대기를 등록하기 전에 코어를
	//    Running 상태로 만들어야 PostInitialReceive()가 정상 동작합니다.
	bool workerStarted = _rioCore->StartWorkers(_workerThreadCount, [this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] Client CRioCore StartWorkers failed!"));
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 글로벌 수신 버퍼 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	// 5. ConnectEx 완료 통지 전용 디스패처 시작 (CRioCore와 무관한 별도 IOCP)
	if( !_connectDispatcher.Start() )
	{
		LOG_ERROR(_T("[Error] CRioConnectDispatcher Start failed!"));
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	// 6. 세션 연결 게시 (실제 절차는 ConnectOneMoreSession()에 위임)
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
//***************************************************************************
void CRioClientService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	// ConnectEx 게시/완료 통지를 더 이상 처리하지 않도록 먼저 정지.
	// _rioCore보다 먼저 멈춰도 안전한 이유: 이 디스패처는 _rioCore와 완전히
	// 독립된 리소스(자체 IOCP)라 서로의 정지 순서에 의존성이 없음.
	_connectDispatcher.Shutdown();

	if( _rioCore )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
	}

	_globalRecvBuffer.reset();

	CNetService::Close();
}