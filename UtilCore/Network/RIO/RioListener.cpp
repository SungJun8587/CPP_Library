
//***************************************************************************
// RioListener.cpp: implementation of the CRioListener class.
//
//***************************************************************************

#include "pch.h"
#include "RioListener.h"

//***************************************************************************
// @brief CRioListener 생성자
//***************************************************************************
CRioListener::CRioListener()
{
}

//***************************************************************************
// @brief CRioListener 소멸자
//***************************************************************************
CRioListener::~CRioListener()
{
    Stop();
}

//***************************************************************************
// @brief 리스너 초기화 및 Accept IOCP + AcceptContext Pool 구동
// @param rioCore RIO 코어 참조 객체
// @param netAddr 리슨할 네트워크 주소 (IP/Port)
// @param sessionFactory 세션 생성 팩터리
// @param onAccept Accept 완료 시 호출될 콜백
// @param acceptPoolSize 상시 유지할 outstanding AcceptEx 개수 (기본 kDefaultAcceptPoolSize)
// @param acceptWorkerCount Accept 전용 IOCP를 소비할 워커 스레드 개수 (기본 kDefaultAcceptWorkerCount)
// @return bool 성공 여부
//***************************************************************************
bool CRioListener::StartAccept(CRioCoreRef rioCore, CNetAddress netAddr, RioSessionFactory sessionFactory, OnRioAcceptCallback onAccept,
    uint32 acceptPoolSize, uint32 acceptWorkerCount)
{
    if( _isListening.load(std::memory_order_acquire) )
        return false;

    // 필수 인자는 멤버에 대입하기 전에 먼저 검증합니다.
    if( rioCore == nullptr || sessionFactory == nullptr )
        return false;

    if( acceptPoolSize == 0 ) acceptPoolSize = 1;
    if( acceptWorkerCount == 0 ) acceptWorkerCount = 1;

    _rioCore = std::move(rioCore);
    _sessionFactory = std::move(sessionFactory);
    _onAcceptCallback = std::move(onAccept);

    // 1. CSocketUtils를 이용한 RIO 전용 Listen 소켓 생성 (WSA_FLAG_REGISTERED_IO 적용)
    SOCKET listenSocket = CSocketUtils::CreateRioSocket();
    if( listenSocket == INVALID_SOCKET )
    {
        _rioCore.reset();
        _sessionFactory = nullptr;
        _onAcceptCallback = nullptr;
        return false;
    }

    // 2. 소켓 옵션 설정 (주소 재사용 및 Linger 설정)
    if( CSocketUtils::SetReuseAddress(listenSocket, true) == false ||
        CSocketUtils::SetLinger(listenSocket, 0, 0) == false )
    {
        CSocketUtils::Close(listenSocket);
        _rioCore.reset();
        _sessionFactory = nullptr;
        _onAcceptCallback = nullptr;
        return false;
    }

    // 3. 주소 바인딩 (Bind) 및 Listen
    if( CSocketUtils::Bind(listenSocket, netAddr) == false ||
        CSocketUtils::Listen(listenSocket, SOMAXCONN) == false )
    {
        CSocketUtils::Close(listenSocket);
        _rioCore.reset();
        _sessionFactory = nullptr;
        _onAcceptCallback = nullptr;
        return false;
    }

    _listenSocket.store(listenSocket, std::memory_order_release);

    // 4. Accept 전용 IOCP 생성 및 Listen 소켓 연결. CRioCore의 RIO CQ/IOCP와는
    //    완전히 독립된 별도 completion 경로다(CRioConnectDispatcher와 동일한 원칙).
    if( !InitializeAcceptIocp(acceptWorkerCount) )
    {
        _listenSocket.store(INVALID_SOCKET, std::memory_order_release);
        CSocketUtils::Close(listenSocket);
        _rioCore.reset();
        _sessionFactory = nullptr;
        _onAcceptCallback = nullptr;
        return false;
    }

    // 5. 리스닝 상태 전환 및 Accept 워커 시작
    _isListening.store(true, std::memory_order_release);

    try
    {
        for( uint32 i = 0; i < acceptWorkerCount; ++i )
        {
            _acceptWorkers.emplace_back(&CRioListener::AcceptWorkerLoop, this);
        }
    }
    catch( ... )
    {
        _isListening.store(false, std::memory_order_release);
        Stop();
        return false;
    }

    // 6. AcceptContext Pool 생성 및 초기 AcceptEx 사전 게시. 이 시점부터
    //    kAcceptPoolSize개의 AcceptEx가 동시에 outstanding 상태가 되어,
    //    이후 완료 하나를 처리하는 동안에도 커널은 나머지로 계속 새 연결을
    //    받을 수 있다(기존의 "연결 하나 완전 처리 후 다음 AcceptEx" 직렬
    //    구조 대비 핵심 개선점).
    try
    {
        _acceptContexts.reserve(acceptPoolSize);

        for( uint32 i = 0; i < acceptPoolSize; ++i )
        {
            auto context = std::make_unique<RioAcceptContext>();
            RioAcceptContext* rawContext = context.get();
            _acceptContexts.push_back(std::move(context));

            PostAccept(rawContext);
        }
    }
    catch( ... )
    {
        Stop();
        return false;
    }

    return true;
}

//***************************************************************************
// @brief Accept 전용 IOCP를 생성하고 Listen 소켓을 등록합니다.
// @param workerCount IOCP 동시성 힌트로 사용할 워커 스레드 개수
// @return bool 성공 여부
//***************************************************************************
bool CRioListener::InitializeAcceptIocp(uint32 workerCount)
{
    _acceptIocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, workerCount);
    if( _acceptIocp == nullptr )
        return false;

    SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);

    if( ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), _acceptIocp, 0, 0) == nullptr )
    {
        ::CloseHandle(_acceptIocp);
        _acceptIocp = nullptr;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 리스너 정지
// @details CancelIoEx로 outstanding AcceptEx를 취소하고, Listen 소켓을 닫은
//          뒤, 워커들에 wake-up 패킷을 포스팅해 Join한다. 이미 정지된
//          상태에서 재호출해도 안전하다(idempotent).
//***************************************************************************
void CRioListener::Stop()
{
    if( !_isListening.exchange(false, std::memory_order_acq_rel) )
        return;

    SOCKET listenSocket = _listenSocket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);

    if( listenSocket != INVALID_SOCKET )
    {
        // 1. Listen 소켓에 걸려 있는 모든 outstanding AcceptEx를 취소.
        //    (AcceptEx의 completion 통지는 listenSocket 핸들 기준으로 이 IOCP에
        //    도착하므로, listenSocket을 대상으로 CancelIoEx하면 전체가 취소된다.)
        ::CancelIoEx(reinterpret_cast<HANDLE>(listenSocket), nullptr);
        CSocketUtils::Close(listenSocket);
    }

    // 2. 워커들이 GetQueuedCompletionStatus(INFINITE)에서 블로킹 대기 중일 수
    //    있으므로, 워커 수만큼 wake-up(빈) 패킷을 포스팅해 깨운다.
    if( _acceptIocp != nullptr )
    {
        for( size_t i = 0; i < _acceptWorkers.size(); ++i )
        {
            ::PostQueuedCompletionStatus(_acceptIocp, 0, 0, nullptr);
        }
    }

    // 3. 전체 워커 Join (자기 자신을 호출한 스레드가 워커 중 하나라면 스킵 —
    //    self-join 방지. 그 경우 이 스레드는 자연 종료를 신뢰한다).
    const std::thread::id currentId = std::this_thread::get_id();

    for( auto& worker : _acceptWorkers )
    {
        if( worker.joinable() && worker.get_id() != currentId )
        {
            worker.join();
        }
    }
    _acceptWorkers.clear();

    // 4. Accept IOCP 파괴 (워커 전원 Join 완료 후에만 — 순서 중요)
    if( _acceptIocp != nullptr )
    {
        ::CloseHandle(_acceptIocp);
        _acceptIocp = nullptr;
    }

    // 5. CancelIoEx로도 완료 통지 없이 남을 수 있는 acceptSocket 방어적 정리
    //    (정상 경로에서는 취소된 AcceptEx도 결국 완료 통지가 와서
    //    ProcessAccept()의 !_isListening 분기가 이미 정리했을 것이다).
    for( auto& context : _acceptContexts )
    {
        if( context && context->acceptSocket != INVALID_SOCKET )
        {
            CSocketUtils::Close(context->acceptSocket);
            context->acceptSocket = INVALID_SOCKET;
        }
    }
}

//***************************************************************************
// @brief 지정된 AcceptContext에 대해 신규 클라이언트 소켓을 만들고 AcceptEx를 게시합니다.
// @param context 게시할 AcceptContext
// @return bool 게시(또는 즉시 성공) 성공 여부
//***************************************************************************
bool CRioListener::PostAccept(RioAcceptContext* context)
{
    if( context == nullptr )
        return false;

    for( ;; )
    {
        if( !_isListening.load(std::memory_order_acquire) )
            return false;

        const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
        if( listenSocket == INVALID_SOCKET )
            return false;

        // AcceptEx는 클라이언트 소켓이 미리 생성되어 있어야 하며, RIO 전용
        // 플래그가 필수다(이후 RIOCreateRequestQueue의 대상이 되므로).
        SOCKET clientSocket = CSocketUtils::CreateRioSocket();
        if( clientSocket == INVALID_SOCKET )
        {
            if( ++context->retryCount > kMaxAcceptRetry )
            {
                // TODO: 로그 - 클라이언트 소켓 생성 반복 실패, 이 슬롯의 Accept 재등록 포기
                return false;
            }
            // Accept 전용 워커 스레드이므로(RIO 데이터 경로와 분리됨) 짧은
            // 블로킹은 처리량에 영향을 주지 않는다.
            ::Sleep(10);
            continue;
        }

        context->Init();
        context->acceptSocket = clientSocket;

        DWORD bytesReceived = 0;

        BOOL result = CSocketUtils::AcceptEx(
            listenSocket,
            clientSocket,
            context->acceptBuffer,
            0, // 첫 데이터 수신 안 함 (0바이트)
            RioAcceptContext::kAddrLen,
            RioAcceptContext::kAddrLen,
            &bytesReceived,
            static_cast<LPOVERLAPPED>(context)
        );

        if( result == FALSE )
        {
            const int32 errorCode = ::WSAGetLastError();
            if( errorCode != WSA_IO_PENDING )
            {
                context->acceptSocket = INVALID_SOCKET;
                CSocketUtils::Close(clientSocket);

                if( ++context->retryCount > kMaxAcceptRetry )
                {
                    // TODO: 로그 - AcceptEx 반복 실패, 이 슬롯의 Accept 재등록 포기
                    return false;
                }
                ::Sleep(10);
                continue;
            }
        }

        // WSA_IO_PENDING(정상 대기) 또는 즉시 성공 → 게시 완료
        return true;
    }
}

//***************************************************************************
// @brief Accept 전용 IOCP를 소비하는 워커 루프
// @note [주의 — self-stop 재진입] ProcessAccept() 안에서 호출되는
//       _onAcceptCallback()이 (예: 실패 처리 로직으로) 이 리스너의 Stop()을
//       재귀적으로 호출할 수 있다. Stop()은 자기 자신(현재 스레드)은 join하지
//       않고 반환하지만, 대신 _acceptIocp를 CloseHandle()까지 완료한 뒤
//       반환한다. 따라서 ProcessAccept()가 반환된 직후 곧바로 다음 루프로
//       진입해 이미 닫힌 핸들에 GetQueuedCompletionStatus()를 호출하면 정의되지
//       않은 동작이 된다. ProcessAccept() 직후 _isListening을 재확인해 이
//       경로를 차단한다.
//***************************************************************************
void CRioListener::AcceptWorkerLoop()
{
    for( ;; )
    {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL success = ::GetQueuedCompletionStatus(_acceptIocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);

        if( overlapped == nullptr )
        {
            // wake-up(정지) 패킷 또는 핸들 자체 오류 — 정지 플래그 재확인 후 계속/종료
            if( !_isListening.load(std::memory_order_acquire) )
                return;
            continue;
        }

        RioAcceptContext* context = static_cast<RioAcceptContext*>(overlapped);

        ProcessAccept(context, success != FALSE);

        // self-stop 재진입 시나리오 방어: ProcessAccept() 도중 Stop()이 이미
        // _acceptIocp를 닫았다면 여기서 즉시 종료하고, 절대 다음 루프에서
        // 닫힌 핸들로 GQCS를 호출하지 않는다.
        if( !_isListening.load(std::memory_order_acquire) )
            return;
    }
}

//***************************************************************************
// @brief AcceptEx 완료 처리
// @param context 완료된 AcceptContext
// @param succeeded GetQueuedCompletionStatus가 보고한 이 I/O의 성공 여부
//***************************************************************************
void CRioListener::ProcessAccept(RioAcceptContext* context, bool succeeded)
{
    SOCKET clientSocket = context->acceptSocket;
    context->acceptSocket = INVALID_SOCKET;

    // 정지 절차 중 취소된 완료(주로 ERROR_OPERATION_ABORTED)이거나 그 외
    // 실패라면 소켓만 정리하고 재게시 여부를 정지 상태에 따라 결정한다.
    if( !succeeded )
    {
        if( clientSocket != INVALID_SOCKET )
            CSocketUtils::Close(clientSocket);

        if( !_isListening.load(std::memory_order_acquire) )
            return; // Stop() 진행 중 — 이 슬롯은 더 이상 재게시하지 않는다.

        if( ++context->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - AcceptEx 완료 실패 반복, 이 슬롯의 Accept 재등록 포기
            return;
        }

        PostAccept(context);
        return;
    }

    if( !_isListening.load(std::memory_order_acquire) )
    {
        // 성공 완료가 왔지만 이미 정지 절차 중 — 새 연결을 받지 않고 정리한다.
        if( clientSocket != INVALID_SOCKET )
            CSocketUtils::Close(clientSocket);
        return;
    }

    const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);

    // Listen 소켓의 컨텍스트를 클라이언트 소켓에 동기화 (AcceptEx 사용 시 필수)
    if( listenSocket == INVALID_SOCKET || !CSocketUtils::SetUpdateAcceptContext(clientSocket, listenSocket) )
    {
        LOG_ERROR(_T("[Error] SetUpdateAcceptContext failed! WSAError: %d"), ::WSAGetLastError());
        CSocketUtils::Close(clientSocket);

        if( ++context->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - SetUpdateAcceptContext 반복 실패, 이 슬롯의 Accept 재등록 포기
            return;
        }
        PostAccept(context);
        return;
    }

    // 클라이언트 주소 정보 파싱
    SOCKADDR* localSockAddr = nullptr;
    INT localSockAddrLen = 0;
    SOCKADDR* remoteSockAddr = nullptr;
    INT remoteSockAddrLen = 0;

    CSocketUtils::GetAcceptExSockaddrs(
        context->acceptBuffer,
        0,
        RioAcceptContext::kAddrLen,
        RioAcceptContext::kAddrLen,
        &localSockAddr,
        &localSockAddrLen,
        &remoteSockAddr,
        &remoteSockAddrLen
    );

    sockaddr_in clientAddr{};
    if( remoteSockAddr != nullptr && remoteSockAddrLen >= sizeof(sockaddr_in) )
    {
        ::memcpy(&clientAddr, remoteSockAddr, sizeof(sockaddr_in));
    }

    // RIO Request Queue 생성 (이제 clientSocket이 WSA_FLAG_REGISTERED_IO 플래그를 가짐)
    RIO_RQ requestQueue = CreateRequestQueueForSocket(clientSocket);

    if( requestQueue == RIO_INVALID_RQ )
    {
        LOG_ERROR(_T("[Error] CreateRequestQueueForSocket failed! WSAError: %d"), ::WSAGetLastError());
        CSocketUtils::Close(clientSocket);

        if( ++context->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - CreateRequestQueueForSocket 반복 실패, 이 슬롯의 Accept 재등록 포기
            return;
        }
        PostAccept(context);
        return;
    }

    // 세션 생성 팩토리 호출. _sessionFactory()는 사용자(상위 서비스) 코드이므로
    // 예외가 발생할 수 있다 — Accept Worker 스레드 함수 밖으로 예외가 빠져나가면
    // std::terminate()로 프로세스 전체가 죽으므로 try/catch로 감싼다.
    CRioSessionRef session;

    try
    {
        session = _sessionFactory();
    }
    catch( ... )
    {
        CSocketUtils::Close(clientSocket);
        // RQ는 clientSocket이 closesocket()되면서 커널이 함께 정리한다.
        context->retryCount = 0; // 세션 팩토리 자체의 일시적 예외로 이 슬롯을 계속 소모하지 않도록 리셋
        PostAccept(context);
        return;
    }

    if( session == nullptr )
    {
        CSocketUtils::Close(clientSocket);
        context->retryCount = 0;
        PostAccept(context);
        return;
    }

    // 외부로 Accept 완료 통보 (콜백 호출 시 clientSocket/requestQueue의
    // ownership이 콜백 쪽으로 이전된다고 간주한다). 콜백도 사용자 코드이므로
    // 위와 동일한 이유로 try/catch로 감싼다 — 예외 발생 시 여기서 직접 정리한다.
    if( _onAcceptCallback )
    {
        try
        {
            _onAcceptCallback(session, clientSocket, requestQueue, clientAddr);
        }
        catch( ... )
        {
            CSocketUtils::Close(clientSocket);
        }
    }
    else
    {
        // 콜백이 없으면 아무도 ownership을 가져가지 않으므로 여기서 정리한다.
        CSocketUtils::Close(clientSocket);
    }

    // 이번 연결은 (성공/실패와 무관하게 콜백까지) 완전히 처리되었으므로
    // 누적 실패 카운트를 리셋하고, 같은 슬롯으로 다음 AcceptEx를 재게시한다.
    context->retryCount = 0;
    PostAccept(context);
}

//***************************************************************************
// @brief 지정된 클라이언트 소켓용 RIO Request Queue를 생성합니다.
// @param clientSocket 바인딩할 클라이언트 소켓
// @return 생성된 RIO_RQ 핸들 (실패 시 RIO_INVALID_RQ)
//***************************************************************************
RIO_RQ CRioListener::CreateRequestQueueForSocket(SOCKET clientSocket)
{
    if( clientSocket == INVALID_SOCKET )
        return RIO_INVALID_RQ;

    if( !_isListening.load(std::memory_order_acquire) )
        return RIO_INVALID_RQ;

    // CRioCore 객체 lifetime을 이 호출 동안 보장하기 위해 로컬 shared_ptr로 붙잡습니다.
    CRioCoreRef rioCore = _rioCore;
    if( rioCore == nullptr )
        return RIO_INVALID_RQ;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = rioCore->GetRioTable();
    if( rioTable.RIOCreateRequestQueue == nullptr )
        return RIO_INVALID_RQ;

    const RIO_CQ receiveCq = rioCore->GetReceiveQueue();
    const RIO_CQ sendCq = rioCore->GetSendQueue();

    if( receiveCq == RIO_INVALID_CQ || sendCq == RIO_INVALID_CQ )
        return RIO_INVALID_RQ;

    return rioTable.RIOCreateRequestQueue(
        clientSocket,
        Rio::kRequestQueueMaxReceiveOutstanding,
        Rio::kRequestQueueMaxReceiveDataBuffers,
        Rio::kRequestQueueMaxSendOutstanding,
        Rio::kRequestQueueMaxSendDataBuffers,
        receiveCq,
        sendCq,
        nullptr
    );
}