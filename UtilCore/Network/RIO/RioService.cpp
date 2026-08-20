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

	// 6. Listener 구동 시작
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
//***************************************************************************
void CRioServerService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

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
// @brief 클라이언트 구동 및 세션 할당
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

	std::vector<CRioSessionRef> connectedSessions;
	connectedSessions.reserve(static_cast<size_t>(_maxSessionCount));

	//***********************************************************************
	// @brief 지금까지 연결에 성공한 세션들을 역순으로 Close()합니다.
	//***********************************************************************
	auto rollbackConnectedSessions = [&connectedSessions]()
		{
			for( auto it = connectedSessions.rbegin(); it != connectedSessions.rend(); ++it )
			{
				if( *it ) (*it)->Close(Rio::CloseReason::InternalError);
			}
			connectedSessions.clear();
		};

	// 5. 세션 연결 및 RQ 바인딩
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		CSessionRef session = CreateSession();
		if( session == nullptr )
		{
			rollbackConnectedSessions();
			return false;
		}

		CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);
		if( rioSession == nullptr )
		{
			rollbackConnectedSessions();
			return false;
		}

		SOCKET clientSocket = CSocketUtils::CreateRioSocket();
		if( clientSocket == INVALID_SOCKET )
		{
			rollbackConnectedSessions();
			return false;
		}

		if( !CSocketUtils::Connect(clientSocket, _address) )
		{
			CSocketUtils::Close(clientSocket);
			LOG_ERROR(_T("[Client] Error: Connection failed!"));
			rollbackConnectedSessions();
			return false;
		}

		RIO_RQ requestQueue = rioTable.RIOCreateRequestQueue(
			clientSocket,
			Rio::kRequestQueueMaxReceiveOutstanding,
			Rio::kRequestQueueMaxReceiveDataBuffers,
			Rio::kRequestQueueMaxSendOutstanding,
			Rio::kRequestQueueMaxSendDataBuffers,
			_rioCore->GetReceiveQueue(),
			_rioCore->GetSendQueue(),
			nullptr
		);

		if( requestQueue == RIO_INVALID_RQ )
		{
			CSocketUtils::Close(clientSocket);
			rollbackConnectedSessions();
			return false;
		}

		uint64_t sessionId = _sessionManager.GenerateSessionId();

		if( !rioSession->Init(sessionId, _rioCore.get(), _globalRecvBuffer.get(), clientSocket, requestQueue) )
		{
			LOG_ERROR(_T("[Client] Error: CRioSession::Init failed (send buffer registration)!"));
			CSocketUtils::Close(clientSocket);
			rollbackConnectedSessions();
			return false;
		}

		rioSession->SetNetAddress(_address);
		AddSession(rioSession);
		_sessionManager.AddSession(sessionId, rioSession);

		rioSession->PostInitialReceive();

		connectedSessions.push_back(rioSession);
	}

	LOG_INFO(_T("[RIO Client] Connected to Server!"));
	return true;
}

//***************************************************************************
// @brief 클라이언트 서비스 종료 처리
//***************************************************************************
void CRioClientService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	if( _rioCore )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
	}

	_globalRecvBuffer.reset();

	CNetService::Close();
}