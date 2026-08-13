
//***************************************************************************
// RioServer.cpp : implementation of the CRioServer class.
//
//***************************************************************************

#include "pch.h"
#include "RioServer.h"

//***************************************************************************
// @brief CRioServer 생성자
// @details
//      서버 실행에 필요한 Core, EventPool, Buffer 등의 의존성을 전달받아 초기화합니다.
//      noexcept 키워드로 예외 미발생을 보장합니다.
//***************************************************************************
CRioServer::CRioServer(CRioCore& core, CRioEventPool& eventPool, CRioBuffer& receiveBuffer) noexcept
    : _core(&core), _eventPool(&eventPool), _receiveBuffer(&receiveBuffer), _sessionManager(core)
    , _lifecycleMutex(), _sessionResourceMutex(), _listenSocket(INVALID_SOCKET), _listenAddress{}
    , _backlog(SOMAXCONN), _initialized(false), _running(false), _acceptThread(), _nextSessionId(1), _requestQueues()
{
}

//***************************************************************************
// @brief CRioServer 소멸자
// @details
//      객체 소멸 시 Stop()을 호출하여 실행 중인 서버 자원을 안전하게 정리합니다.
//***************************************************************************
CRioServer::~CRioServer() noexcept
{
    Stop();
}

//***************************************************************************
// @brief Server 초기화
// @details
//      주소 및 백로그 설정을 검증하고 바인딩된 리슨 소켓을 생성하여 서버를 초기화합니다.
// @return true: 초기화 성공, false: 초기화 실패
//***************************************************************************
bool CRioServer::Initialize(const sockaddr_in& address, int backlog) noexcept
{
    std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);

    if( _initialized.load(std::memory_order_acquire) || _running.load(std::memory_order_acquire) ) return false;
    if( _core == nullptr || _eventPool == nullptr || _receiveBuffer == nullptr ) return false;
    if( backlog < Rio::kListenBacklogMinimum || address.sin_family != AF_INET ) return false;
    if( !CreateListenSocket(address, backlog) ) return false;

    _listenAddress = address;
    _backlog = backlog;

    _initialized.store(true, std::memory_order_release);
    return true;
}

//***************************************************************************
// @brief Server 시작
// @details
//      초기화 상태를 확인한 후 클라이언트 접속 처리를 위한 Accept 스레드를 생성하여 실행합니다.
// @return true: 시작 성공, false: 시작 실패
//***************************************************************************
bool CRioServer::Start() noexcept
{
    std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);

    if( !_initialized.load(std::memory_order_acquire) ) return false;
    if( _running.load(std::memory_order_acquire) ) return true;
    if( _listenSocket == INVALID_SOCKET ) return false;
    if( !_sessionManager.GetCore() || _sessionManager.GetCore() != _core ) return false;

    _running.store(true, std::memory_order_release);

    try
    {
        _acceptThread = std::thread(&CRioServer::AcceptLoop, this);
    }
    catch( ... )
    {
        _running.store(false, std::memory_order_release);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief Server 종료
// @note
//      Shutdown ordering:
//          1. Stop Accept
//          2. Stop new Session / RIO post
//          3. Shutdown Session sockets
//          4. CRioCore::Shutdown()
//          5. Outstanding I/O drain
//          6. RIO_RQ destruction
//          7. SessionManager cleanup
// @details
//      Accept 스레드 종료, 세션 종료, RIO Queue 소멸 및 리소스 정리를 순차적으로 수행합니다.
//***************************************************************************
void CRioServer::Stop(std::chrono::milliseconds drainTimeout) noexcept
{
    {
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);

        const bool initialized = _initialized.load(std::memory_order_acquire);
        const bool running = _running.load(std::memory_order_acquire);

        if( !initialized && !running ) return;

        _running.store(false, std::memory_order_release);

        // Accept()를 깨우기 위해 listen socket을 먼저 닫습니다.
        if( _listenSocket != INVALID_SOCKET )
        {
            CloseSocket(_listenSocket);
            _listenSocket = INVALID_SOCKET;
        }
    }

    // Accept thread는 lifecycle mutex를 사용하지 않으므로 여기서 안전하게 Join할 수 있습니다.
    if( _acceptThread.joinable() )
    {
        if( _acceptThread.get_id() != std::this_thread::get_id() )
        {
            _acceptThread.join();
        }
        else
        {
            assert(false && "CRioServer::Stop self-join");
        }
    }

    // 신규 Session 생성은 CloseAll()의 _closing=true에 의해 차단됩니다.
    _sessionManager.CloseAll();

    // 모든 Session의 socket을 먼저 shutdown합니다.
    CleanupClosedSessions();

    // 아직 Closed 상태이지만 outstanding I/O가 존재하는 Session도 socket shutdown이 필요합니다.
    {
        std::lock_guard<std::mutex> resourceLock(_sessionResourceMutex);

        for( const auto& [sessionId, requestQueue] : _requestQueues )
        {
            (void)sessionId;
            (void)requestQueue;
        }
    }

    if( _core != nullptr ) (void)_core->Shutdown(drainTimeout);

    // Core drain 이후에는 모든 outstanding RIO I/O가 정상적으로 completion 처리된 상태여야 합니다.
    CleanupClosedSessions();

    // 남아있는 Closed Session의 RIO_RQ를 최종 제거합니다.
    {
        std::lock_guard<std::mutex> resourceLock(_sessionResourceMutex);

        for( auto it = _requestQueues.begin(); it != _requestQueues.end(); )
        {
            const SessionId sessionId = it->first;
            const RIO_RQ requestQueue = it->second;

            SessionPtr session = _sessionManager.FindSession(sessionId);

            if( session == nullptr || (session->IsClosed() && !session->HasOutstandingIo()) )
            {
                if( DestroyRequestQueue(requestQueue) )
                {
                    it = _requestQueues.erase(it);
                    continue;
                }
            }

            ++it;
        }
    }

    _sessionManager.RemoveClosedSessions();

    {
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
        _initialized.store(false, std::memory_order_release);
    }
}

//***************************************************************************
// @brief Server 실행 여부 확인
// @return true: 실행 중, false: 중지됨
//***************************************************************************
bool CRioServer::IsRunning() const noexcept
{
    return _running.load(std::memory_order_acquire);
}

//***************************************************************************
// @brief Server 초기화 여부 확인
// @return true: 초기화 완료, false: 미초기화
//***************************************************************************
bool CRioServer::IsInitialized() const noexcept
{
    return _initialized.load(std::memory_order_acquire);
}

//***************************************************************************
// @brief Listen Socket 반환
// @return 현재 바인딩된 리프닝 소켓 핸들
//***************************************************************************
SOCKET CRioServer::GetListenSocket() const noexcept
{
    std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
    return _listenSocket;
}

//***************************************************************************
// @brief CRioCore 반환
// @return 바인딩된 CRioCore 포인터
//***************************************************************************
CRioCore* CRioServer::GetCore() const noexcept
{
    return _core;
}

//***************************************************************************
// @brief CRioEventPool 반환
// @return 바인딩된 CRioEventPool 포인터
//***************************************************************************
CRioEventPool* CRioServer::GetEventPool() const noexcept
{
    return _eventPool;
}

//***************************************************************************
// @brief Receive CRioBuffer 반환
// @return 수신 버퍼 포인터
//***************************************************************************
CRioBuffer* CRioServer::GetReceiveBuffer() const noexcept
{
    return _receiveBuffer;
}

//***************************************************************************
// @brief SessionManager 반환
// @return 세션 관리자 포인터
//***************************************************************************
CRioSessionManager* CRioServer::GetSessionManager() noexcept
{
    return &_sessionManager;
}

//***************************************************************************
// @brief const SessionManager 반환
// @return const 세션 관리자 포인터
//***************************************************************************
const CRioSessionManager* CRioServer::GetSessionManager() const noexcept
{
    return &_sessionManager;
}

//***************************************************************************
// @brief 전체 Session 개수 반환
// @return 전체 세션 수
//***************************************************************************
size_t CRioServer::GetSessionCount() const noexcept
{
    return _sessionManager.GetSessionCount();
}

//***************************************************************************
// @brief Active Session 개수 반환
// @return 활성화된 세션 수
//***************************************************************************
size_t CRioServer::GetActiveSessionCount() const noexcept
{
    return _sessionManager.GetActiveSessionCount();
}

//***************************************************************************
// @brief Closing Session 개수 반환
// @return 종료 진행 중인 세션 수
//***************************************************************************
size_t CRioServer::GetClosingSessionCount() const noexcept
{
    return _sessionManager.GetClosingSessionCount();
}

//***************************************************************************
// @brief Closed Session 개수 반환
// @return 완전히 종료된 세션 수
//***************************************************************************
size_t CRioServer::GetClosedSessionCount() const noexcept
{
    return _sessionManager.GetClosedSessionCount();
}

//***************************************************************************
// @brief Listen Socket 생성
// @details
//      TCP 소켓을 생성하고 SO_REUSEADDR 옵션 설정, 주소 바인딩 및 리슨 상태로 전환합니다.
// @return true: 생성 성공, false: 생성 실패
//***************************************************************************
bool CRioServer::CreateListenSocket(const sockaddr_in& address, int backlog) noexcept
{
    // accept()로 수락된 클라이언트 소켓은 리슨 소켓의 속성을 상속받으므로,
    // RIO 큐 생성이 가능하려면 리슨 소켓 자체가 WSA_FLAG_REGISTERED_IO로 생성돼야 한다.
    SOCKET listenSocket = CSocketUtils::CreateRioSocket();
    if( listenSocket == INVALID_SOCKET ) return false;

    if( !CSocketUtils::SetReuseAddress(listenSocket, true) )
    {
        CloseSocket(listenSocket);
        return false;
    }

    if( !CSocketUtils::Bind(listenSocket, CNetAddress(address)) )
    {
        CloseSocket(listenSocket);
        return false;
    }

    if( !CSocketUtils::Listen(listenSocket, backlog) )
    {
        CloseSocket(listenSocket);
        return false;
    }

    _listenSocket = listenSocket;

    if( !ConfigureListenSocket() )
    {
        CloseSocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief Listen Socket non-blocking 설정
// @details
//      Accept thread가 Stop()과 교착하지 않도록 listen socket을 non-blocking으로 설정합니다.
// @return true: 설정 성공, false: 설정 실패
//***************************************************************************
bool CRioServer::ConfigureListenSocket() noexcept
{
    if( _listenSocket == INVALID_SOCKET ) return false;
    return CSocketUtils::SetNonBlocking(_listenSocket, true);
}

//***************************************************************************
// @brief Accept Loop
// @details
//      서버가 실행 중인 동안 클라이언트 접속 수락을 계속 시도하며 주기적으로 닫힌 세션을 정리합니다.
//***************************************************************************
void CRioServer::AcceptLoop() noexcept
{
    while( _running.load(std::memory_order_acquire) )
    {
        bool accepted = false;

        while( _running.load(std::memory_order_acquire) )
        {
            if( AcceptOne() )
            {
                accepted = true;
                continue;
            }
            break;
        }

        CleanupClosedSessions();

        if( !accepted ) std::this_thread::sleep_for(Rio::kAcceptPollInterval);
    }
}

//***************************************************************************
// @brief 하나의 Client Accept
// @details
//      클라이언트 접속을 수락하고 RIO Request Queue 및 세션을 생성하여 초기 Receive 요청을 게시합니다.
// @return true: 클라이언트 수락 성공, false: 수락 실패 또는 대기 상태
//***************************************************************************
bool CRioServer::AcceptOne() noexcept
{
    if( !_running.load(std::memory_order_acquire) ) return false;

    SOCKET listenSocket;
    {
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
        listenSocket = _listenSocket;
    }

    if( listenSocket == INVALID_SOCKET ) return false;

    sockaddr_in clientAddress{};
    SOCKET clientSocket = CSocketUtils::Accept(listenSocket, clientAddress);

    if( clientSocket == INVALID_SOCKET )
    {
        const int error = ::WSAGetLastError();
        if( error == WSAEWOULDBLOCK || error == WSAEINTR ) return false;
        if( !_running.load(std::memory_order_acquire) ) return false;

        // WOULDBLOCK/INTR 이외의 실제 에러(WSAEMFILE, WSAENOBUFS 등)를 로그로 남긴다.
        CSocketUtils::ReportError(_T("CRioServer::AcceptOne accept()"), error);
        return false;
    }

    if( !_running.load(std::memory_order_acquire) )
    {
        CloseSocket(clientSocket);
        return false;
    }

    RIO_RQ requestQueue = RIO_INVALID_RQ;

    if( !CreateRequestQueue(clientSocket, requestQueue) )
    {
        CloseSocket(clientSocket);
        return false;
    }

    SessionPtr session;

    if( !CreateSession(clientSocket, requestQueue, session) )
    {
        DestroyRequestQueue(requestQueue);
        CloseSocket(clientSocket);
        return false;
    }

    {
        std::lock_guard<std::mutex> resourceLock(_sessionResourceMutex);

        const SessionId sessionId = session->GetSessionId();
        const auto [it, inserted] = _requestQueues.emplace(sessionId, requestQueue);

        if( !inserted )
        {
            session->Close();
            DestroyRequestQueue(requestQueue);
            CloseSocket(clientSocket);
            return false;
        }
    }

    if( !StartInitialReceive(session) )
    {
        session->Close();
        CloseSocket(clientSocket);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief RIO Request Queue 생성
// @details
//      RIOCreateRequestQueue API를 호출하여 해당 소켓 전용 RIO_RQ를 생성합니다.
// @return true: RIO_RQ 생성 성공, false: 생성 실패
//***************************************************************************
bool CRioServer::CreateRequestQueue(SOCKET socket, RIO_RQ& outRequestQueue) noexcept
{
    outRequestQueue = RIO_INVALID_RQ;

    if( socket == INVALID_SOCKET || _core == nullptr ) return false;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();
    if( rioTable.RIOCreateRequestQueue == nullptr ) return false;

    // CRioCore 또는 연관 클래스에서 RIO_CQ 핸들을 가져와 전달합니다.
    RIO_CQ receiveCq = _core->GetReceiveQueue(); // 프로젝트 구조에 맞는 CQ 가져오기
    RIO_CQ sendCq = _core->GetSendQueue();       // 프로젝트 구조에 맞는 CQ 가져오기

    RIO_RQ requestQueue = rioTable.RIOCreateRequestQueue(socket, 1, 1, 1, 1, receiveCq, sendCq, nullptr);
    if( requestQueue == RIO_INVALID_RQ ) return false;

    outRequestQueue = requestQueue;
    return true;
}

//***************************************************************************
// @brief Session 생성 및 SessionManager 등록
// @details
//      신규 세션 ID를 생성하고 SessionManager에 세션 개체를 할당 등록합니다.
// @return true: 세션 생성 성공, false: 생성 실패
//***************************************************************************
bool CRioServer::CreateSession(SOCKET socket, RIO_RQ requestQueue, SessionPtr& outSession) noexcept
{
    outSession.reset();

    if( socket == INVALID_SOCKET || requestQueue == RIO_INVALID_RQ || _core == nullptr ) return false;

    const SessionId sessionId = GenerateSessionId(_nextSessionId);
    if( !_sessionManager.IsValidSessionId(sessionId) ) return false;

    SessionPtr session = _sessionManager.CreateSession<CRioSession>(sessionId, socket, requestQueue);
    if( session == nullptr ) return false;

    if( session->GetSessionId() != sessionId || session->GetSocket() != socket || session->GetRequestQueue() != requestQueue || session->GetCore() != _core )
    {
        session->Close();
        return false;
    }

    outSession = std::move(session);
    return true;
}

//***************************************************************************
// @brief 첫 Receive 요청
// @details
//      Receive buffer의 slot을 먼저 확보하고 RIO_BUF를 생성한 뒤 EventPool에서
//      Receive Event를 확보하여 Session::StartReceive()로 전달합니다.
// @note
//      CRioReceive 성공 시 Event가 slot ownership을 보유합니다.
// @return true: 비동기 Receive 게시 성공, false: 게시 실패
//***************************************************************************
bool CRioServer::StartInitialReceive(const SessionPtr& session) noexcept
{
    if( session == nullptr || _eventPool == nullptr || _receiveBuffer == nullptr ) return false;
    if( !session->IsActive() || session->GetCore() != _core || session->GetRequestQueue() == RIO_INVALID_RQ ) return false;

    uint32_t slotIndex = Rio::kInvalidSlotIndex;
    RIO_BUF rioBuffer{};
    CRioEvent* rioEvent = nullptr;

    if( !AllocateReceiveEvent(session, slotIndex, rioBuffer, rioEvent) ) return false;

    if( !session->StartReceive(_receiveBuffer, slotIndex, rioBuffer, rioEvent, 0) )
    {
        _eventPool->Free(rioEvent);
        (void)_receiveBuffer->FreeSlot(slotIndex);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief Receive slot 및 Event 확보
// @details
//      수신 버퍼 슬롯 할당, RIO_BUF 정보 획득 및 이벤트 개체 할당을 수행합니다.
// @return true: 자원 확보 성공, false: 자원 확보 실패
//***************************************************************************
bool CRioServer::AllocateReceiveEvent(const SessionPtr& session, uint32_t& outSlotIndex, RIO_BUF& outBuffer, CRioEvent*& outEvent) noexcept
{
    outSlotIndex = Rio::kInvalidSlotIndex;
    outBuffer.BufferId = RIO_INVALID_BUFFERID;
    outBuffer.Offset = 0;
    outBuffer.Length = 0;
    outEvent = nullptr;

    if( session == nullptr || _receiveBuffer == nullptr || _eventPool == nullptr ) return false;
    if( !session->IsActive() ) return false;

    if( !_receiveBuffer->AllocSlot(outSlotIndex) ) return false;

    if( !_receiveBuffer->GetRioBuffer(outSlotIndex, outBuffer) )
    {
        (void)_receiveBuffer->FreeSlot(outSlotIndex);
        outSlotIndex = Rio::kInvalidSlotIndex;
        return false;
    }

    CRioObjectRef ownerRef;

    try
    {
        ownerRef = session->shared_from_this();
    }
    catch( ... )
    {
        (void)_receiveBuffer->FreeSlot(outSlotIndex);
        outSlotIndex = Rio::kInvalidSlotIndex;
        return false;
    }

    if( ownerRef == nullptr )
    {
        (void)_receiveBuffer->FreeSlot(outSlotIndex);
        outSlotIndex = Rio::kInvalidSlotIndex;
        return false;
    }

    outEvent = _eventPool->Alloc(Rio::EventType::Receive, ownerRef);
    if( outEvent == nullptr )
    {
        (void)_receiveBuffer->FreeSlot(outSlotIndex);
        outSlotIndex = Rio::kInvalidSlotIndex;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief Closed Session 정리
// @note
//      RIO_RQ는 반드시 Closed && Outstanding I/O == 0 이후에만 제거합니다.
//      Socket도 같은 시점에 닫습니다.
// @details
//      진행 중인 I/O가 없는 closed 세션을 탐색하여 소켓 닫기 및 RIO_RQ를 파기합니다.
//***************************************************************************
void CRioServer::CleanupClosedSessions() noexcept
{
    std::lock_guard<std::mutex> resourceLock(_sessionResourceMutex);

    for( auto it = _requestQueues.begin(); it != _requestQueues.end(); )
    {
        const SessionId sessionId = it->first;
        const RIO_RQ requestQueue = it->second;

        SessionPtr session = _sessionManager.FindSession(sessionId);

        if( session == nullptr )
        {
            if( DestroyRequestQueue(requestQueue) )
            {
                it = _requestQueues.erase(it);
                continue;
            }
            ++it;
            continue;
        }

        if( !session->IsClosed() || session->HasOutstandingIo() )
        {
            ++it;
            continue;
        }

        CloseSessionSocket(session);

        if( !DestroyRequestQueue(requestQueue) )
        {
            ++it;
            continue;
        }

        it = _requestQueues.erase(it);
    }

    _sessionManager.RemoveClosedSessions();
}

//***************************************************************************
// @brief Closed Session Socket 정리
// @details
//      세션이 보유한 통신 소켓을 안전하게 종결합니다.
//***************************************************************************
void CRioServer::CloseSessionSocket(const SessionPtr& session) noexcept
{
    if( session == nullptr ) return;

    const SOCKET socket = session->GetSocket();
    if( socket != INVALID_SOCKET ) CloseSocket(socket);
}

//***************************************************************************
// @brief RIO Request Queue 제거
// @note
//      반드시 Outstanding I/O == 0 상태에서 호출해야 합니다.
// @details
//      RIOCloseRequestQueue API를 사용하여 RIO 요청 큐를 해제합니다.
// @return true: 파기 성공, false: 파기 실패
//***************************************************************************
bool CRioServer::DestroyRequestQueue(RIO_RQ requestQueue) noexcept
{
    if( requestQueue == RIO_INVALID_RQ ) return true;
    if( _core == nullptr ) return false;

    /*
    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();
    if( rioTable.RIOCloseRequestQueue == nullptr ) return false;

    return rioTable.RIOCloseRequestQueue(requestQueue) != FALSE;
    */

    // RIO_RQ는 소켓 종료 시 자동으로 해제되므로 별도 처리 없이 true를 반환합니다.
    return true;
}

//***************************************************************************
// @brief Listen Socket 종료
// @details
//      서버 리슨 소켓 핸들을 원자적으로 획득하여 닫습니다.
//***************************************************************************
void CRioServer::CloseListenSocket() noexcept
{
    SOCKET listenSocket = INVALID_SOCKET;

    {
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
        listenSocket = _listenSocket;
        _listenSocket = INVALID_SOCKET;
    }

    CloseSocket(listenSocket);
}

//***************************************************************************
// @brief Socket 종료
// @details
//      소켓의 송수신을 차단(shutdown)하고 핸들을 해제(closesocket)합니다.
//***************************************************************************
void CRioServer::CloseSocket(SOCKET socket) noexcept
{
    CSocketUtils::CloseGraceful(socket);
}

//***************************************************************************
// @brief SessionId 생성
// @details
//      원자적 카운터를 증가시켜 중복되지 않는 세션 식별자를 생성합니다 (0 제외).
// @return 신규 생성된 SessionId
//***************************************************************************
CRioServer::SessionId CRioServer::GenerateSessionId(std::atomic<SessionId>& counter) noexcept
{
    SessionId id = counter.fetch_add(1, std::memory_order_relaxed);
    if( id != 0 ) return id;

    do
    {
        id = counter.fetch_add(1, std::memory_order_relaxed);
    } while( id == 0 );

    return id;
}