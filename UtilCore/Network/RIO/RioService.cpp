
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
// @note RioSessionFactory와 OnRioAcceptCallback 람다를 작성하여 세션과 소켓/RQ를 바인딩합니다.
//       송신 버퍼는 더 이상 이 서비스가 등록하지 않습니다 — 세션 자신의
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

	// RIO 함수 테이블을 얻기 위한 용도의 리슨 소켓 생성 (실제 accept에도 재사용)
	SOCKET listenSocket = CSocketUtils::CreateRioSocket();
	if( listenSocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = Rio::kServiceMaxCompletionResults;
	ULONG_PTR cqIdentifier = Rio::kServerCqIdentifier;

	// 2. _rioCore 초기화 (이 시점에 내부 상태가 Rio::State::Initialized로 변경됨)
	if( !_rioCore->Initialize(listenSocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		::closesocket(listenSocket);
		return false;
	}

	// 3. [핵심] 코어가 Initialized 상태가 된 직후에 StartWorker를 호출하여 워커 스레드 구동
	// CRioCore 내부의 DispatchBatch 또는 Dispatch 함수를 호출하는 루프를 전달합니다.
	bool workerStarted = _rioCore->StartWorker([this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			// RIO CQ 완료 이벤트를 처리하는 핵심 디스패치 함수 호출
			// (CRioCore에 정의된 배치 디스패치 함수 활용)
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] CRioCore StartWorker failed!"));
		::closesocket(listenSocket);
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 수신용 글로벌 CRioBuffer 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
	{
		::closesocket(listenSocket);
		return false;
	}

	// 5. Listener 생성
	_listener = MakeShared<CRioListener>();
	if( _listener == nullptr )
	{
		::closesocket(listenSocket);
		return false;
	}

	// 리슨/RQ 생성용 rioTable을 얻는 데만 쓰인 소켓은 여기서 정리합니다.
	// 실제 accept용 리슨 소켓은 CRioListener::Start() 내부에서 별도로 새로 만듭니다.
	::closesocket(listenSocket);

	CRioServerServiceRef service = std::static_pointer_cast<CRioServerService>(shared_from_this());

	// 6. Listener 구동 시작
	bool result = _listener->Start(
		_rioCore,
		_address,
		//***********************************************************************
		// @brief 클라이언트 접속마다 세션 객체를 생성하는 팩토리 콜백
		// @return CRioSessionRef 생성된 세션(최대 세션 수 초과 등으로 생성 실패 시 nullptr)
		//***********************************************************************
		[service]() -> CRioSessionRef
		{
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
		[service](CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)
		{
			if( session )
			{
				CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);

				// 서비스가 소유한 세션 매니저를 통해 고유 세션 ID 발급 및 등록
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
			}
		}
	);

	return result;
}

//***************************************************************************
// @brief 서버 종료 처리
// @note 모든 세션에 종료를 통지한 뒤 Listener를 정지시켜 Accept 스레드를
//       종료하고 부모 Close를 호출합니다. 세션별 송신 버퍼 등록 해제는
//       각 세션의 FinalizeClose()/소멸자가 스스로 책임지므로 여기서
//       별도로 처리할 자원이 없습니다.
//***************************************************************************
void CRioServerService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

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
//***************************************************************************
bool CRioClientService::Start()
{
	if( CanStart() == false )
		return false;

	// 1. 클라이언트 전용 이벤트 풀 초기화
	if( !_eventPool.Initialize(Rio::kServiceEventPoolCapacity) )
	{
		LOG_ERROR(_T("[Error] Client CRioEventPool Initialize failed!"));
		return false;
	}

	// 2. 클라이언트 전용 CRioCore 생성 및 초기화
	_rioCore = MakeShared<CRioCore>();

	// RIO 함수 테이블을 얻기 위한 용도의 더미 소켓
	SOCKET dummySocket = CSocketUtils::CreateRioSocket();
	if( dummySocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = Rio::kServiceMaxCompletionResults;
	ULONG_PTR cqIdentifier = Rio::kClientCqIdentifier;

	if( !_rioCore->Initialize(dummySocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		::closesocket(dummySocket);
		return false;
	}

	// dummySocket은 rioTable을 얻는 데만 쓰였으므로 더 이상 필요 없습니다.
	::closesocket(dummySocket);

	// [필수 수정] 세션과 수신 대기를 등록하기 전에 워커 스레드를 먼저 띄워 코어 상태를 Running으로 만들어야 합니다!
	bool workerStarted = _rioCore->StartWorker([this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] Client CRioCore StartWorker failed!"));
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 3. 글로벌 수신 버퍼 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) == false )
		return false;

	// 4. 세션 연결 및 RQ 바인딩 (코어가 이미 Running 상태이므로 PostInitialReceive가 정상 성공합니다)
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		CSessionRef session = CreateSession();
		if( session == nullptr )
			return false;

		CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);
		if( rioSession == nullptr )
			return false;

		SOCKET clientSocket = CSocketUtils::CreateRioSocket();
		if( clientSocket == INVALID_SOCKET )
			return false;

		if( !CSocketUtils::Connect(clientSocket, _address) )
		{
			::closesocket(clientSocket);
			LOG_ERROR(_T("[Client] Error: Connection failed!"));
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
			::closesocket(clientSocket);
			return false;
		}

		// 서비스가 소유한 세션 매니저를 통해 고유 세션 ID 발급 및 등록
		uint64_t sessionId = _sessionManager.GenerateSessionId();

		// Init()이 세션 자신의 송신 버퍼를 RIO에 등록합니다. 실패 시 세션은 아직
		// Active가 아니므로 여기서 소켓을 직접 정리해야 합니다.
		if( !rioSession->Init(sessionId, _rioCore.get(), _globalRecvBuffer.get(), clientSocket, requestQueue) )
		{
			LOG_ERROR(_T("[Client] Error: CRioSession::Init failed (send buffer registration)!"));
			::closesocket(clientSocket);
			return false;
		}

		rioSession->SetNetAddress(_address);
		AddSession(rioSession);
		_sessionManager.AddSession(sessionId, rioSession);

		// 이제 코어가 Running 상태이므로 정상적으로 수신 대기 등록이 가능합니다!
		rioSession->PostInitialReceive();
	}

	LOG_INFO(_T("[RIO Client] Connected to Server!"));
	return true;
}

//***************************************************************************
// @brief 클라이언트 서비스 종료 처리
// @note 모든 세션에 종료를 통지한 뒤 부모 Close를 호출합니다. 송신 버퍼는
//       각 세션이 스스로 등록 해제하므로 여기서 별도로 처리할 자원이 없습니다.
//***************************************************************************
void CRioClientService::Close()
{
	_sessionManager.BeginCloseAllSessions();

	CNetService::Close();
}