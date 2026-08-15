//***************************************************************************
// RioServer.cpp : implementation of the CRioServer class.
//
//***************************************************************************

#include "pch.h"
#include "RioServer.h"

#include <cassert>
#include <chrono>
#include <limits>

//***************************************************************************
// @brief CRioServer 생성자
//***************************************************************************
CRioServer::CRioServer()
    : _serverState(Rio::ServerState::Created)
    , _listenSocket(INVALID_SOCKET)
    , _sendBufferId(RIO_INVALID_BUFFERID)
{
}

//***************************************************************************
// @brief CRioServer 소멸자
//***************************************************************************
CRioServer::~CRioServer()
{
    Stop();
}

//***************************************************************************
// @brief 서버를 시작하고 소켓 바인딩 및 RIO 엔진을 가동합니다.
// @param port 수신 대기 포트
// @param maxSessions 최대 동시 연결 가능 세션 수
// @return 시작 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioServer::Start(uint16_t port, uint32_t maxSessions)
{
    Rio::ServerState expectedState = Rio::ServerState::Created;
    if( !_serverState.compare_exchange_strong(
        expectedState,
        Rio::ServerState::Initialized,
        std::memory_order_acq_rel,
        std::memory_order_acquire) )
    {
        return false;
    }

    if( maxSessions == 0 )
    {
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    if( maxSessions > (std::numeric_limits<uint32_t>::max() / 4u) )
    {
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    const uint32_t eventPoolCapacity = maxSessions * 4u;
    const uint32_t recvBufferSlotCount = maxSessions * 2u;

    SOCKET dummySocket = ::WSASocket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        nullptr,
        0,
        WSA_FLAG_REGISTERED_IO);

    if( dummySocket == INVALID_SOCKET )
    {
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    // 1. CRioCore 초기화
    if( !_rioCore.Initialize(dummySocket, Rio::kMaxOutstandingIo, 1001, &_eventPool) )
    {
        ::closesocket(dummySocket);
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    ::closesocket(dummySocket);

    // 2. EventPool 및 BufferPool 초기화
    if( !_eventPool.Initialize(eventPoolCapacity) )
    {
        _rioCore.Shutdown();
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore.GetRioTable();

    if( !_globalRecvBufferPool.Initialize(
        &rioTable,
        recvBufferSlotCount,
        Rio::kRecvBufferSlotSize,
        Rio::kDefaultAlignment) )
    {
        _eventPool.Release();
        _rioCore.Shutdown();
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    if( !_globalSendBufferPool.Initialize(
        &rioTable,
        maxSessions,
        65536,
        Rio::kDefaultAlignment) )
    {
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();
        _rioCore.Shutdown();
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    // 3. 송신 버퍼 ID 획득
    _sendBufferId = _globalSendBufferPool.GetBufferId();

    if( _sendBufferId == RIO_INVALID_BUFFERID )
    {
        _globalSendBufferPool.Shutdown();
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();
        _rioCore.Shutdown();
        _sendBufferId = RIO_INVALID_BUFFERID;
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    // 4. Listen 소켓 생성 및 바인딩
    _listenSocket = ::WSASocket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        nullptr,
        0,
        WSA_FLAG_REGISTERED_IO);

    if( _listenSocket == INVALID_SOCKET )
    {
        _globalSendBufferPool.Shutdown();
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();
        _rioCore.Shutdown();
        _sendBufferId = RIO_INVALID_BUFFERID;
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
    serverAddr.sin_port = ::htons(port);

    if( ::bind(
        _listenSocket,
        reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr)) == SOCKET_ERROR ||
        ::listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR )
    {
        ::closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;

        _globalSendBufferPool.Shutdown();
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();
        _rioCore.Shutdown();

        _sendBufferId = RIO_INVALID_BUFFERID;
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    // 5. RIO Worker 구동
    if( !_rioCore.StartWorker([this]() {
        while( _rioCore.CanSubmitIo() )
        {
            const int32 dispatched = _rioCore.DispatchBatch(Rio::DispatchMode::Wait);
            if( dispatched < 0 ) break;
        }
        }) )
    {
        ::closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;

        _globalSendBufferPool.Shutdown();
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();
        _rioCore.Shutdown();

        _sendBufferId = RIO_INVALID_BUFFERID;
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    // 6. 서버 Running 상태 전환 및 Accept 스레드 시작
    _serverState.store(Rio::ServerState::Running, std::memory_order_release);

    try
    {
        _acceptThread = std::thread(&CRioServer::AcceptLoop, this);
    }
    catch( ... )
    {
        _serverState.store(Rio::ServerState::Stopping, std::memory_order_release);

        if( _listenSocket != INVALID_SOCKET )
        {
            ::closesocket(_listenSocket);
            _listenSocket = INVALID_SOCKET;
        }

        _sessionManager.BeginCloseAllSessions();

        while( !_sessionManager.AreAllSessionsClosed() )
        {
            _sessionManager.RemoveClosedSessions();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        _sessionManager.RemoveClosedSessions();

        _rioCore.RequestStop();
        _rioCore.Shutdown();

        _globalSendBufferPool.Shutdown();
        _globalRecvBufferPool.Shutdown();
        _eventPool.Release();

        _sendBufferId = RIO_INVALID_BUFFERID;
        _serverState.store(Rio::ServerState::Created, std::memory_order_release);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 서버 가동을 정지하고 모든 세션의 드레인 및 자원 회수를 순차적으로 수행합니다.
//***************************************************************************
void CRioServer::Stop()
{
    Rio::ServerState expectedState = Rio::ServerState::Running;

    if( !_serverState.compare_exchange_strong(
        expectedState,
        Rio::ServerState::Stopping,
        std::memory_order_acq_rel,
        std::memory_order_acquire) )
    {
        if( expectedState == Rio::ServerState::Created ||
            expectedState == Rio::ServerState::Stopped )
        {
            return;
        }

        // 이미 다른 스레드가 Stopping 중인 경우 해당 스레드가 종료 절차를 담당합니다.
        return;
    }

    // 1. Accept 스레드 종료를 먼저 유도합니다.
    if( _listenSocket != INVALID_SOCKET )
    {
        SOCKET listenSocket = _listenSocket;
        _listenSocket = INVALID_SOCKET;

        ::closesocket(listenSocket);
    }

    if( _acceptThread.joinable() )
    {
        _acceptThread.join();
    }

    // 2. 모든 활성 세션에 Closing 신호 브로드캐스트
    _sessionManager.BeginCloseAllSessions();

    // 3. 모든 세션이 완전히 Closed 될 때까지 대기합니다.
    //
    // 중요:
    // 세션이 Closed 되기 전에 Global RIO BufferPool/EventPool을 해제하면
    // 아직 완료되지 않은 RIO completion이 해당 리소스를 참조할 수 있습니다.
    //
    // 따라서 timeout 이후에도 리소스 해제를 진행하지 않습니다.
    // 안전한 RIO shutdown을 위해서는 Outstanding I/O가 완전히 0이 되어야 합니다.
    int retryCount = 0;
    const int maxRetries = 1000; // 10ms * 1000 = 10초

    while( !_sessionManager.AreAllSessionsClosed() )
    {
        _sessionManager.RemoveClosedSessions();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if( ++retryCount >= maxRetries )
        {
            break;
        }
    }

    _sessionManager.RemoveClosedSessions();

    // 4. 타임아웃 방어 후에도 아직 세션이 존재한다면
    //    RIO BufferPool/EventPool을 절대로 해제하지 않습니다.
    //
    //    이 상태에서 강제로 리소스를 해제하면 Outstanding RIO completion이
    //    해제된 Buffer/Event를 접근할 수 있어 Use-After-Free가 발생할 수 있습니다.
    if( !_sessionManager.AreAllSessionsClosed() )
    {
        // 현재 CRioCore/RIO 정책에서는 강제 리소스 해제보다
        // Outstanding I/O의 완전한 Drain이 우선되어야 합니다.
        //
        // 따라서 여기서는 안전성을 위해 계속 Drain을 기다립니다.
        while( !_sessionManager.AreAllSessionsClosed() )
        {
            _sessionManager.RemoveClosedSessions();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        _sessionManager.RemoveClosedSessions();
    }

    // 5. RIO Core 정지 및 완료 큐 드레인 수행
    _rioCore.RequestStop();
    _rioCore.Shutdown();

    // 6. 리소스 역순 해제
    _globalSendBufferPool.Shutdown();
    _globalRecvBufferPool.Shutdown();
    _eventPool.Release();

    _sendBufferId = RIO_INVALID_BUFFERID;

    _serverState.store(Rio::ServerState::Stopped, std::memory_order_release);
}

//***************************************************************************
// @brief 클라이언트의 신규 연결 요청을 대기하고 수락하는 루프 함수
//***************************************************************************
void CRioServer::AcceptLoop()
{
    while( _serverState.load(std::memory_order_acquire) == Rio::ServerState::Running )
    {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        SOCKET clientSocket = ::accept(
            _listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen);

        if( clientSocket == INVALID_SOCKET )
        {
            if( _serverState.load(std::memory_order_acquire) != Rio::ServerState::Running )
                break;

            continue;
        }

        // accept()가 반환된 직후 Stop()이 실행될 수 있으므로 다시 확인합니다.
        if( _serverState.load(std::memory_order_acquire) != Rio::ServerState::Running )
        {
            ::closesocket(clientSocket);
            break;
        }

        RIO_RQ requestQueue = CreateRequestQueueForSocket(clientSocket);

        if( requestQueue == RIO_INVALID_RQ )
        {
            ::closesocket(clientSocket);
            continue;
        }

        // RIO RQ 생성 이후 Stop()이 발생했을 수 있습니다.
        if( _serverState.load(std::memory_order_acquire) != Rio::ServerState::Running )
        {
            ::closesocket(clientSocket);
            continue;
        }

        CRioSessionRef session = CreateSession();

        if( session == nullptr )
        {
            // [수정] _RIO_EXTENSION_FUNCTION_TABLE에 존재하지 않는 RIOCloseRequestQueue 호출부 제거
            // RIO_RQ는 연결된 socket이 closesocket() 되면서 함께 정리됩니다.
            ::closesocket(clientSocket);
            continue;
        }

        uint64_t sessionId = _sessionManager.GenerateSessionId();

        session->Init(
            sessionId,
            &_rioCore,
            &_globalRecvBufferPool,
            clientSocket,
            requestQueue,
            _sendBufferId);

        // Init() 이후 Stop()이 발생한 경우 세션을 등록하지 않고 종료합니다.
        if( _serverState.load(std::memory_order_acquire) != Rio::ServerState::Running )
        {
            session->Close(Rio::CloseReason::ForcedClose);
            continue;
        }

        if( !_sessionManager.AddSession(clientSocket, sessionId, session) )
        {
            session->Close(Rio::CloseReason::InternalError);
            continue;
        }

        if( !session->PostInitialReceive() )
        {
            // PostInitialReceive() 내부에서 이미 실패 원인에 따라 Close()가 호출됩니다.
            _sessionManager.RemoveSession(clientSocket, sessionId);
        }
    }
}

//***************************************************************************
// @brief 지정된 클라이언트 소켓용 RIO Request Queue를 생성합니다.
// @param clientSocket 바인딩할 클라이언트 소켓
// @return 생성된 RIO_RQ 핸들 (실패 시 RIO_INVALID_RQ)
//***************************************************************************
RIO_RQ CRioServer::CreateRequestQueueForSocket(SOCKET clientSocket)
{
    if( clientSocket == INVALID_SOCKET ) return RIO_INVALID_RQ;

    if( _serverState.load(std::memory_order_acquire) != Rio::ServerState::Running )
        return RIO_INVALID_RQ;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore.GetRioTable();
    if( rioTable.RIOCreateRequestQueue == nullptr ) return RIO_INVALID_RQ;

    const RIO_CQ receiveCq = _rioCore.GetReceiveQueue();
    const RIO_CQ sendCq = _rioCore.GetSendQueue();

    if( receiveCq == RIO_INVALID_CQ || sendCq == RIO_INVALID_CQ )
        return RIO_INVALID_RQ;

    return rioTable.RIOCreateRequestQueue(
        clientSocket,
        Rio::kMaxOutstandingIo,
        1,
        Rio::kMaxOutstandingIo,
        1,
        receiveCq,
        sendCq,
        nullptr
    );
}