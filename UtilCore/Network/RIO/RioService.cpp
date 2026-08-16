//***************************************************************************
// RioService.cpp: implementation of the CRioService classes.
//
//***************************************************************************

#include "pch.h"
#include "RioService.h"
#include "RioBuffer.h"

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
//***************************************************************************
bool CRioServerService::Start()
{
	if( CanStart() == false || _rioCore == nullptr )
		return false;

	// 1. 서버 전용 이벤트 풀 초기화 (멤버 변수 _eventPool 사용)
	if( !_eventPool.Initialize(4096) )
	{
		std::cout << "[Error] CRioEventPool Initialize failed!\n";
		return false;
	}

	SOCKET listenSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED);
	if( listenSocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = 64;
	ULONG_PTR cqIdentifier = 0x1000;

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
		std::cout << "[Error] CRioCore StartWorker failed!\n";
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 4. 수신용 글로벌 CRioBuffer 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, 1000, 8192) == false )
		return false;

	// 5. 전송 버퍼 초기화 및 RIO 등록
	size_t sendPoolSize = 1024 * 1024 * 10; // 10MB
	_sendBufferPtr = ::VirtualAlloc(nullptr, sendPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if( _sendBufferPtr == nullptr )
		return false;

	_sendBufferId = rioTable.RIORegisterBuffer(static_cast<PCHAR>(_sendBufferPtr), static_cast<DWORD>(sendPoolSize));
	if( _sendBufferId == RIO_INVALID_BUFFERID )
		return false;

	_listener = MakeShared<CRioListener>();
	if( _listener == nullptr )
		return false;

	CRioServerServiceRef service = std::static_pointer_cast<CRioServerService>(shared_from_this());

	// 6. Listener 구동 시작
	bool result = _listener->Start(
		_rioCore,
		_address,
		[service]() -> CRioSessionRef
		{
			CSessionRef session = service->CreateSession();
			if( session == nullptr )
			{
				std::cout << "[Error] CreateSession() returned nullptr! (Max session count reached?)\n";
			}
			return std::static_pointer_cast<CRioSession>(session);
		},
		[service](CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)
		{
			if( session )
			{
				CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);

				uint64_t sessionId = CRioSessionManager::Instance().GenerateSessionId();
				CRioCore* rioCore = service->GetRioCore().get();

				CRioBuffer* globalRecvBuffer = service->GetGlobalRecvBuffer();
				RIO_BUFFERID sendBufferId = service->GetSendBufferId();

				rioSession->Init(sessionId, rioCore, globalRecvBuffer, clientSocket, requestQueue, sendBufferId);

				rioSession->SetNetAddress(netAddr);
				service->AddSession(rioSession);
				CRioSessionManager::Instance().AddSession(rioSession->GetSocket(), sessionId, rioSession);

				rioSession->PostInitialReceive();
			}
			else
			{
				std::cout << "[Error] Session is nullptr inside accept callback!\n";
			}
		}
	);

	return result;
}

//***************************************************************************
// @brief 서버 종료 처리
// @note Listener를 정지시켜 Accept 스레드를 종료하고 부모 Close를 호출합니다.
//***************************************************************************
void CRioServerService::Close()
{
	if( _listener )
	{
		_listener->Stop();
		_listener = nullptr;
	}

	if( _sendBufferPtr != nullptr )
	{
		::VirtualFree(_sendBufferPtr, 0, MEM_RELEASE);
		_sendBufferPtr = nullptr;
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
// @param maxSessionCount 생성할 세션 수
//***************************************************************************
CRioClientService::CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount)
	: CNetService(NetServiceType::Client, address, factory, maxSessionCount), _rioCore(rioCore)
{
}

//***************************************************************************
// @brief 클라이언트 구동 및 세션 할당
// @return bool 시작 성공 여부
// @note _maxSessionCount 수만큼 세션을 할당 후 관리 목록에 등록합니다.
//***************************************************************************
bool CRioClientService::Start()
{
	if( CanStart() == false )
		return false;

	// 1. 클라이언트 전용 이벤트 풀 초기화
	if( !_eventPool.Initialize(4096) )
	{
		std::cout << "[Error] Client CRioEventPool Initialize failed!\n";
		return false;
	}

	// 2. 클라이언트 전용 CRioCore 생성 및 초기화
	_rioCore = MakeShared<CRioCore>();

	SOCKET dummySocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED);
	if( dummySocket == INVALID_SOCKET )
		return false;

	ULONG maxCompletionResults = 64;
	ULONG_PTR cqIdentifier = 0x2000;

	if( !_rioCore->Initialize(dummySocket, maxCompletionResults, cqIdentifier, &_eventPool) )
	{
		::closesocket(dummySocket);
		return false;
	}

	// [필수 수정] 세션과 수신 대기를 등록하기 전에 워커 스레드를 먼저 띄워 코어 상태를 Running으로 만들어야 합니다!
	bool workerStarted = _rioCore->StartWorker([this]() {
		while( _rioCore->GetState() == Rio::State::Running )
		{
			_rioCore->DispatchBatch(Rio::DispatchMode::Wait);
		}
		});

	if( !workerStarted )
	{
		std::cout << "[Error] Client CRioCore StartWorker failed!\n";
		return false;
	}

	const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();

	// 3. 글로벌 수신 버퍼 초기화
	_globalRecvBuffer = MakeShared<CRioBuffer>();
	if( _globalRecvBuffer->Initialize(&rioTable, 1000, 8192) == false )
		return false;

	// 4. 송신 버퍼 등록
	size_t sendPoolSize = 1024 * 1024 * 10;
	_sendBufferPtr = ::VirtualAlloc(nullptr, sendPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if( _sendBufferPtr == nullptr )
		return false;

	_sendBufferId = rioTable.RIORegisterBuffer(static_cast<PCHAR>(_sendBufferPtr), static_cast<DWORD>(sendPoolSize));
	if( _sendBufferId == RIO_INVALID_BUFFERID )
		return false;

	// 5. 세션 연결 및 RQ 바인딩 (코어가 이미 Running 상태이므로 PostInitialReceive가 정상 성공합니다)
	for( int32 i = 0; i < _maxSessionCount; i++ )
	{
		CSessionRef session = CreateSession();
		if( session == nullptr )
			return false;

		CRioSessionRef rioSession = std::static_pointer_cast<CRioSession>(session);
		if( rioSession == nullptr )
			return false;

		SOCKET clientSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED);
		if( clientSocket == INVALID_SOCKET )
			return false;

		SOCKADDR_IN serverAddr = _address.GetSockAddr();
		if( ::connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR )
		{
			::closesocket(clientSocket);
			std::cout << "[Client] Error: Connection failed!\n";
			return false;
		}

		// 동기 connect 직후 클라이언트 소켓의 연결 컨텍스트 업데이트
		if( setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR )
		{
			std::cout << "[Client] Error: SO_UPDATE_CONNECT_CONTEXT failed! WSAError: " << ::WSAGetLastError() << "\n";
			::closesocket(clientSocket);
			return false;
		}

		RIO_RQ requestQueue = rioTable.RIOCreateRequestQueue(
			clientSocket,
			32, 
			1,
			32, 
			1,
			_rioCore->GetReceiveQueue(),
			_rioCore->GetSendQueue(),
			nullptr
		);

		if( requestQueue == RIO_INVALID_RQ )
		{
			::closesocket(clientSocket);
			return false;
		}

		uint64_t sessionId = CRioSessionManager::Instance().GenerateSessionId();

		rioSession->Init(
			sessionId,
			_rioCore.get(),
			_globalRecvBuffer.get(),
			clientSocket,
			requestQueue,
			_sendBufferId
		);

		rioSession->SetNetAddress(_address);
		AddSession(rioSession);
		CRioSessionManager::Instance().AddSession(rioSession->GetSocket(), sessionId, rioSession);

		// 이제 코어가 Running 상태이므로 정상적으로 수신 대기 등록이 가능합니다!
		rioSession->PostInitialReceive();
	}

	std::cout << "[RIO Client] Connected to Server!\n";
	return true;
}

void CRioClientService::Close()
{
	if( _sendBufferPtr != nullptr )
	{
		::VirtualFree(_sendBufferPtr, 0, MEM_RELEASE);
		_sendBufferPtr = nullptr;
	}

	CNetService::Close();
}