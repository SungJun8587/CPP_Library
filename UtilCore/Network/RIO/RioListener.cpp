
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
// @brief 리스너 초기화 및 Accept 스레드 구동
// @param rioCore RIO 코어 참조 객체
// @param netAddr 리슨할 네트워크 주소 (IP/Port)
// @param sessionFactory 세션 생성 팩터리
// @param onAccept Accept 완료 시 호출될 콜백
// @return bool 성공 여부
//***************************************************************************
bool CRioListener::Start(CRioCoreRef rioCore, CNetAddress netAddr, RioSessionFactory sessionFactory, OnRioAcceptCallback onAccept)
{
    if( _isListening.load(std::memory_order_acquire) )
        return false;

    // 필수 인자는 멤버에 대입하기 전에 먼저 검증합니다.
    if( rioCore == nullptr || sessionFactory == nullptr )
        return false;

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

    // 4. 리스닝 상태 전환 및 Accept 스레드 시작
    _isListening.store(true, std::memory_order_release);

    try
    {
        _acceptThread = std::thread(&CRioListener::AcceptLoop, this);
    }
    catch( ... )
    {
        _isListening.store(false, std::memory_order_release);

        SOCKET socketToClose = _listenSocket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
        if( socketToClose != INVALID_SOCKET )
            CSocketUtils::Close(socketToClose);

        _rioCore.reset();
        _sessionFactory = nullptr;
        _onAcceptCallback = nullptr;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 리스너 정지 및 소켓 닫기
// @note 이미 정지된 상태에서 재호출해도 안전합니다. Accept 스레드 자신이
//       (콜백 내부 경유로) 이 함수를 호출한 경우 self-join 데드락을 피하기
//       위해 join을 생략합니다 — 이 경우 std::thread 객체는 joinable 상태로
//       남으며, 이후 다른 스레드가 Stop()을 한 번 더 호출하면 그때 join됩니다.
//***************************************************************************
void CRioListener::Stop()
{
    if( !_isListening.exchange(false, std::memory_order_acq_rel) )
        return;

    SOCKET listenSocket = _listenSocket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);

    if( listenSocket != INVALID_SOCKET )
    {
        // AcceptEx가 pending 상태일 수 있으므로 먼저 취소한 뒤 닫습니다.
        ::CancelIoEx(reinterpret_cast<HANDLE>(listenSocket), nullptr);
        CSocketUtils::Close(listenSocket);
    }

    // Accept 스레드 자신이 이 함수를 호출한 경우(콜백 내부에서 Stop()을 부르는
    // 시나리오) join()하면 self-join으로 std::system_error가 발생합니다.
    // 이 경우 join을 건너뛰고 반환합니다 — 스레드는 위 소켓 close로 인해
    // AcceptEx가 깨어나고 _isListening==false를 확인하는 즉시 자연 종료됩니다.
    if( _acceptThread.joinable() && _acceptThread.get_id() != std::this_thread::get_id() )
    {
        _acceptThread.join();
    }
}

//***************************************************************************
// @brief 클라이언트의 신규 연결 요청을 AcceptEx로 대기하고 수락하는 루프 함수
//***************************************************************************
void CRioListener::AcceptLoop()
{
    while( _isListening.load(std::memory_order_acquire) )
    {
        const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
        if( listenSocket == INVALID_SOCKET )
            break;

        // AcceptEx는 소켓이 미리 생성되어 있어야 하며, RIO 전용 플래그가 필수입니다.
        SOCKET clientSocket = CSocketUtils::CreateRioSocket();
        if( clientSocket == INVALID_SOCKET )
        {
            if( _isListening.load(std::memory_order_acquire) )
                ::Sleep(100);
            continue;
        }

        // AcceptEx를 위한 주소 버퍼 크기 (SOCKADDR_IN 기준 + 16바이트 여유)
        constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;
        char acceptBuf[(addrLen) * 2]{};
        DWORD bytesReceived = 0;
        OVERLAPPED overlapped{};

        BOOL result = CSocketUtils::AcceptEx(
            listenSocket,
            clientSocket,
            acceptBuf,
            0, // 첫 데이터 수신 안 함 (0바이트)
            addrLen,
            addrLen,
            &bytesReceived,
            &overlapped
        );

        if( !result )
        {
            const int err = ::WSAGetLastError();

            if( err == ERROR_IO_PENDING )
            {
                DWORD transferred = 0;
                DWORD flags = 0;

                if( !::WSAGetOverlappedResult(listenSocket, &overlapped, &transferred, TRUE, &flags) )
                {
                    const int resultError = ::WSAGetLastError();

                    CSocketUtils::Close(clientSocket);

                    if( !_isListening.load(std::memory_order_acquire) )
                        break;

                    // Stop()의 CancelIoEx()/closesocket()으로 인한 취소는
                    // 정상 종료 경로이므로 조용히 루프를 빠져나갑니다.
                    if( resultError == WSA_OPERATION_ABORTED || resultError == ERROR_OPERATION_ABORTED )
                        break;

                    continue;
                }
            }
            else
            {
                CSocketUtils::Close(clientSocket);

                if( !_isListening.load(std::memory_order_acquire) )
                    break;

                ::Sleep(100);
                continue;
            }
        }

        if( !_isListening.load(std::memory_order_acquire) )
        {
            CSocketUtils::Close(clientSocket);
            break;
        }

        // Listen 소켓의 컨텍스트를 클라이언트 소켓에 동기화 (AcceptEx 사용 시 필수)
        if( !CSocketUtils::SetUpdateAcceptContext(clientSocket, listenSocket) )
        {
            LOG_ERROR(_T("[Error] SetUpdateAcceptContext failed! WSAError: %d"), ::WSAGetLastError());
            CSocketUtils::Close(clientSocket);
            continue;
        }

        // 클라이언트 주소 정보 파싱
        SOCKADDR* localSockAddr = nullptr;
        INT localSockAddrLen = 0;
        SOCKADDR* remoteSockAddr = nullptr;
        INT remoteSockAddrLen = 0;

        CSocketUtils::GetAcceptExSockaddrs(
            acceptBuf,
            0,
            addrLen,
            addrLen,
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
            continue;
        }

        // Stop이 Request Queue 생성 직후 발생할 수 있으므로 ownership을 외부로
        // 넘기기 전에 다시 확인합니다. RIO_RQ는 clientSocket이 closesocket()되면서
        // 커널이 함께 정리하므로 별도로 닫을 API가 없습니다.
        if( !_isListening.load(std::memory_order_acquire) )
        {
            CSocketUtils::Close(clientSocket);
            break;
        }

        // 세션 생성 팩토리 호출. _sessionFactory()는 사용자(상위 서비스) 코드이므로
        // 예외가 발생할 수 있습니다 — 이 함수는 std::thread 위에서 실행되므로,
        // 예외가 여기를 벗어나면 std::terminate()로 프로세스 전체가 죽습니다.
        CRioSessionRef session;

        try
        {
            session = _sessionFactory();
        }
        catch( ... )
        {
            CSocketUtils::Close(clientSocket);
            continue;
        }

        if( session == nullptr )
        {
            CSocketUtils::Close(clientSocket);
            continue;
        }

        // Stop이 Session 생성 이후 발생할 수 있으므로 callback 호출 전에 다시 확인합니다.
        if( !_isListening.load(std::memory_order_acquire) )
        {
            CSocketUtils::Close(clientSocket);
            break;
        }

        // 외부로 Accept 완료 통보 (콜백 호출 시 clientSocket/requestQueue의
        // ownership이 콜백 쪽으로 이전된다고 간주합니다). 콜백도 사용자 코드이므로
        // 위와 동일한 이유로 try/catch로 감쌉니다 — 예외 발생 시 여기서 직접 정리합니다.
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
            // 콜백이 없으면 아무도 ownership을 가져가지 않으므로 여기서 정리합니다.
            CSocketUtils::Close(clientSocket);
        }
    }
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