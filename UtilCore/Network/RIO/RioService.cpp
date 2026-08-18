
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
// @brief CRioServerService 생성자 구현
// @param address 서버 바인딩 주소
// @param rioCore RIO 코어 참조
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
//***************************************************************************
CRioServerService::CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Server, address, factory, maxSessionCount), _rioCore(rioCore)
{
}

//***************************************************************************
// @brief 서버 구동 및 Listener 초기화 및 시작
// @return bool 서버 가동 성공 여부
// @note RioSessionFactory와 OnRioAcceptCallback 람다는 this가 아니라
//       weak_ptr<CRioServerService>를 캡처합니다 — _listener가 이 서비스의
//       멤버이므로 shared_ptr(this)를 캡처하면 CRioServerService → _listener
//       → (콜백이 쥔) shared_ptr<CRioServerService>로 이어지는 순환 참조가
//       생겨 서비스가 절대 소멸하지 않습니다.
//       송신 버퍼는 이 서비스가 등록하지 않습니다 — 세션 자신의
//       CRioSession::Init()이 자기 소유 CRingBuffer를 직접 RIORegisterBuffer()합니다.
//***************************************************************************
bool CRioServerService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 서버 전용 이벤트 풀 초기화 (멤버 변수 _eventPool 사용)
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

	// 2. 생성자에서 주입받은 _rioCore를 초기화 (이 시점에 내부 상태가
	//    Rio::State::Initialized로 변경됨)
	if( !_rioCore->Initialize(listenSocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		::closesocket(listenSocket);
		return false;
	}

	// 3. 코어가 Initialized 상태가 된 직후 워커 스레드 구동 — 이후 세션들의
	//    실제 I/O 제출/완료 처리가 이 스레드의 DispatchBatch() 루프에서 이뤄집니다.
	bool workerStarted = _rioCore->StartWorker([this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] CRioCore StartWorker failed!"));
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		::closesocket(listenSocket);
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 수신용 글로벌 CRioBuffer 초기화 — 실패 시 이미 Running인 워커를
	//    정식으로 정지/드레인한 뒤에 반환해야 합니다(그냥 소켓만 닫고 끝내면
	//    워커/CQ/IOCP가 방치됨).
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		::closesocket(listenSocket);
		return false;
	}

	// 5. Listener 생성
	_listener = MakeShared<CRioListener>();
	if( _listener == nullptr )
	{
		_globalRecvBuffer.reset();
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		::closesocket(listenSocket);
		return false;
	}

	// 리슨/RQ 생성용 rioTable을 얻는 데만 쓰인 소켓은 여기서 정리합니다.
	::closesocket(listenSocket);

	// 순환 참조 방지: shared_ptr가 아니라 weak_ptr를 캡처합니다.
	std::weak_ptr<CRioServerService> weakService = std::static_pointer_cast<CRioServerService>(shared_from_this());

	// 6. Listener 구동 시작 — 이후 accept가 발생할 때마다 아래 두 콜백이 호출됩니다.
	bool result = _listener->Start(
		_rioCore,
		_address,
		//***********************************************************************
		// @brief 클라이언트 접속마다 세션 객체를 생성하는 팩토리 콜백
		// @return CRioSessionRef 생성된 세션(서비스가 이미 소멸했거나 최대 세션 수
		//         초과 등으로 생성 실패 시 nullptr)
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
		// @param session 이미 생성된 세션 객체 (팩토리 콜백에서 만든 것)
		// @param clientSocket 새로 연결된 클라이언트 소켓 (ownership이 여기로 이전됨)
		// @param requestQueue 해당 소켓용으로 미리 생성된 RIO_RQ (ownership이 여기로 이전됨)
		// @param netAddr 클라이언트의 원격 IP/Port
		//***********************************************************************
		[weakService](CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)
		{
			auto service = weakService.lock();
			if( !service )
			{
				// 서비스가 이미 소멸한 상태 — 넘겨받은 소켓 ownership을 여기서 정리합니다.
				// (RIO_RQ는 closesocket()되면서 커널이 함께 정리하므로 별도 해제 불필요)
				::closesocket(clientSocket);
				return;
			}

			if( session )
			{
				CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);

				uint64_t sessionId = service->GetSessionManager().GenerateSessionId();
				CRioCore* rioCore = service->GetRioCore().get();
				CRioBuffer* globalRecvBuffer = service->GetGlobalRecvBuffer();

				// Init()이 세션 자신의 송신 버퍼를 RIO에 등록합니다. 실패하면(리소스
				// 고갈 등) 세션이 Active로 전이하지 않으므로, 이 콜백이 직접
				// clientSocket/requestQueue를 정리해야 합니다(세션은 아직 Active가
				// 아니라 Close()로 자기 자신을 정리시킬 수 없는 상태).
				if( !rioSession->Init(sessionId, rioCore, globalRecvBuffer, clientSocket, requestQueue) )
				{
					LOG_ERROR(_T("[Error] CRioSession::Init failed (send buffer registration)!"));
					::closesocket(clientSocket);
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
				::closesocket(clientSocket);
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
// @note 순서: 모든 세션에 종료 통지 → Listener 정지 → _rioCore
//       RequestStop()+Shutdown()(outstanding I/O drain 완료 보장) →
//       _globalRecvBuffer 해제 → 부모 Close. 이 순서를 명시적으로 지키는
//       이유는 클래스 상단 @details 참고 — 멤버 소멸자 순서에 맡기면
//       _globalRecvBuffer/_eventPool이 _rioCore보다 먼저 파괴되어 위험합니다.
//***************************************************************************
void CRioServerService::Close()
{
	// 1. 관리 중인 모든 세션에 종료 통지 브로드캐스트
	_sessionManager.BeginCloseAllSessions();

	// 2. Accept 스레드 정지
	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

	// 3. RIO 코어 정식 정지 — 워커 Join + outstanding I/O 드레인까지 완료 후 반환
	if( _rioCore )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
	}

	// 4. 위 3번이 outstanding I/O == 0을 보장한 뒤이므로 이제 안전하게 해제 가능합니다.
	_globalRecvBuffer.reset();

	// 5. 부모 클래스 정리 위임
	CNetService::Close();
}


//***************************************************************************
// CRioClientService Implementation
//***************************************************************************

//***************************************************************************
// @brief CRioClientService 생성자 구현
// @param address 접속 대상 주소
// @param rioCore RIO 코어 참조
// @param factory 세션 생성 팩토리
// @param maxSessionCount 최대 세션 수
//***************************************************************************
CRioClientService::CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _rioCore(rioCore)
{
}

//***************************************************************************
// @brief 클라이언트 구동 및 세션 할당
// @return bool 시작 성공 여부
// @note _maxSessionCount 수만큼 세션을 할당 후 관리 목록에 등록합니다.
//       송신 버퍼는 서비스가 아니라 각 세션이 자기 소유 CRingBuffer를 직접
//       RIORegisterBuffer()로 등록합니다(CRioSession::Init() 내부).
//       루프 중 i번째 세션 연결이 실패하면 0~i-1번째로 이미 맺어진 세션들을
//       Close()로 정리한 뒤 false를 반환합니다 — 부분 성공 상태로 남기지 않습니다.
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

	// RIO 함수 테이블을 얻기 위한 용도의 더미 소켓
	SOCKET dummySocket = CSocketUtils::CreateRioSocket();
	if( dummySocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = Rio::kServiceMaxCompletionResults;
	ULONG_PTR cqIdentifier = Rio::kClientCqIdentifier;

	// 2. 생성자에서 주입받은 _rioCore를 초기화
	if( !_rioCore->Initialize(dummySocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		::closesocket(dummySocket);
		return false;
	}

	// dummySocket은 rioTable을 얻는 데만 쓰였으므로 더 이상 필요 없습니다.
	::closesocket(dummySocket);

	// 3. 세션과 수신 대기를 등록하기 전에 워커 스레드를 먼저 띄워 코어 상태를
	//    Running으로 만들어야 합니다 — 그래야 PostInitialReceive()가 정상 동작합니다.
	//    실패 시 이미 Initialize()된 코어를 방치하지 않도록 정식으로 정지/드레인합니다.
	bool workerStarted = _rioCore->StartWorker([this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] Client CRioCore StartWorker failed!"));
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 글로벌 수신 버퍼 초기화 — 실패 시 이미 Running인 워커를 정식으로 정지/드레인합니다.
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
		return false;
	}

	// 지금까지 맺은 세션들의 스냅샷. i번째 연결이 실패하면 이 목록을 역순으로 정리합니다.
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

	// 5. 세션 연결 및 RQ 바인딩 (코어가 이미 Running 상태이므로 PostInitialReceive가 정상 성공합니다)
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		// 5-1. 세션 객체 생성
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

		// 5-2. 클라이언트 소켓 생성 및 서버에 동기 connect
		SOCKET clientSocket = CSocketUtils::CreateRioSocket();
		if( clientSocket == INVALID_SOCKET )
		{
			rollbackConnectedSessions();
			return false;
		}

		if( !CSocketUtils::Connect(clientSocket, _address) )
		{
			::closesocket(clientSocket);
			LOG_ERROR(_T("[Client] Error: Connection failed!"));
			rollbackConnectedSessions();
			return false;
		}

		// 5-3. 이 소켓 전용 RIO Request Queue 생성
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
			::closesocket(clientSocket);
			rollbackConnectedSessions();
			return false;
		}

		// 5-4. 세션 ID 발급 및 세션 초기화. Init()이 세션 자신의 송신 버퍼를
		//      RIO에 등록합니다. 실패 시 세션은 아직 Active가 아니므로 여기서
		//      소켓을 직접 정리해야 합니다.
		uint64_t sessionId = _sessionManager.GenerateSessionId();

		if( !rioSession->Init(sessionId, _rioCore.get(), _globalRecvBuffer.get(), clientSocket, requestQueue) )
		{
			LOG_ERROR(_T("[Client] Error: CRioSession::Init failed (send buffer registration)!"));
			::closesocket(clientSocket);
			rollbackConnectedSessions();
			return false;
		}

		// 5-5. 서비스/세션 매니저에 등록하고 최초 수신 대기 게시
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
// @note 순서: 모든 세션에 종료 통지 → _rioCore RequestStop()+Shutdown()
//       (outstanding I/O drain 완료 보장) → _globalRecvBuffer 해제 → 부모 Close.
//***************************************************************************
void CRioClientService::Close()
{
	// 1. 관리 중인 모든 세션에 종료 통지 브로드캐스트
	_sessionManager.BeginCloseAllSessions();

	// 2. RIO 코어 정식 정지 — 워커 Join + outstanding I/O 드레인까지 완료 후 반환
	if( _rioCore )
	{
		_rioCore->RequestStop();
		_rioCore->Shutdown();
	}

	// 3. 위 2번이 outstanding I/O == 0을 보장한 뒤이므로 이제 안전하게 해제 가능합니다.
	_globalRecvBuffer.reset();

	// 4. 부모 클래스 정리 위임
	CNetService::Close();
}