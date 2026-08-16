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
    : _listenSocket(INVALID_SOCKET)
    , _isListening(false)
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
    if( _isListening.load() )
        return false;

    _rioCore = rioCore;
    _sessionFactory = sessionFactory;
    _onAcceptCallback = onAccept;

    if( _rioCore == nullptr || _sessionFactory == nullptr )
        return false;

    // 1. CSocketUtils를 이용한 RIO 전용 Listen 소켓 생성 (WSA_FLAG_REGISTERED_IO 적용)
    _listenSocket = CSocketUtils::CreateRioSocket();
    if( _listenSocket == INVALID_SOCKET )
        return false;

    // 2. 소켓 옵션 설정 (주소 재사용 및 Linger 설정)
    if( CSocketUtils::SetReuseAddress(_listenSocket, true) == false ||
        CSocketUtils::SetLinger(_listenSocket, 0, 0) == false )
    {
        CSocketUtils::Close(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return false;
    }

    // 3. 주소 바인딩 (Bind) 및 Listen
    if( CSocketUtils::Bind(_listenSocket, netAddr) == false ||
        CSocketUtils::Listen(_listenSocket, SOMAXCONN) == false )
    {
        CSocketUtils::Close(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return false;
    }

    // 4. 리스닝 상태 전환 및 Accept 스레드 시작
    _isListening.store(true);

    try
    {
        _acceptThread = std::thread(&CRioListener::AcceptLoop, this);
    }
    catch( ... )
    {
        _isListening.store(false);
        CSocketUtils::Close(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 리스너 정지 및 소켓 닫기
//***************************************************************************
void CRioListener::Stop()
{
    if( !_isListening.exchange(false) )
    {
        return;
    }

    if( _listenSocket != INVALID_SOCKET )
    {
        SOCKET listenSocket = _listenSocket;
        _listenSocket = INVALID_SOCKET;
        CSocketUtils::Close(listenSocket); // accept 블로킹 해제용
    }

    if( _acceptThread.joinable() )
    {
        _acceptThread.join();
    }
}

//***************************************************************************
// @brief 클라이언트의 신규 연결 요청을 AcceptEx로 대기하고 수락하는 루프 함수
//***************************************************************************
void CRioListener::AcceptLoop()
{
    while( _isListening.load() )
    {
        // AcceptEx는 소켓이 미리 생성되어 있어야 하며, RIO 전용 플래그가 필수입니다.
        SOCKET clientSocket = CSocketUtils::CreateRioSocket();
        if( clientSocket == INVALID_SOCKET )
        {
            ::Sleep(100);
            continue;
        }

        // AcceptEx를 위한 주소 버퍼 크기 (SOCKADDR_IN 기준 + 16바이트 여유)
        constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;
        char acceptBuf[(addrLen) * 2];
        DWORD bytesReceived = 0;
        OVERLAPPED overlapped{};

        BOOL result = CSocketUtils::AcceptEx(
            _listenSocket,
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
            int err = ::WSAGetLastError();
            if( err == ERROR_IO_PENDING )
            {
                // 비동기 대기 구현 필요 시 WSAWaitForMultipleEvents 또는 IOCP 연동 가능
                // 현재 단순 블로킹 구조를 위해 대기 로직 추가 (혹은 동기적으로 대기)
                DWORD transferred = 0;
                DWORD flags = 0;
                if( !::WSAGetOverlappedResult(_listenSocket, &overlapped, &transferred, TRUE, &flags) )
                {
                    CSocketUtils::Close(clientSocket);
                    if( !_isListening.load() )
                        break;
                    continue;
                }
            }
            else
            {
                CSocketUtils::Close(clientSocket);
                if( !_isListening.load() )
                    break;
                ::Sleep(100);
                continue;
            }
        }

        if( !_isListening.load() )
        {
            CSocketUtils::Close(clientSocket);
            break;
        }

        // Listen 소켓의 컨텍스트를 클라이언트 소켓에 동기화 (AcceptEx 사용 시 필수)
        if( !CSocketUtils::SetUpdateAcceptContext(clientSocket, _listenSocket) )
        {
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
        if( remoteSockAddr && remoteSockAddrLen >= sizeof(sockaddr_in) )
        {
            ::memcpy(&clientAddr, remoteSockAddr, sizeof(sockaddr_in));
        }

        // RIO Request Queue 생성 (이제 clientSocket이 WSA_FLAG_REGISTERED_IO 플래그를 가짐)
        RIO_RQ requestQueue = CreateRequestQueueForSocket(clientSocket);
        if( requestQueue == RIO_INVALID_RQ )
        {
            std::cout << "[Error] CreateRequestQueueForSocket failed! WSAError: " << ::WSAGetLastError() << "\n";
            CSocketUtils::Close(clientSocket);
            continue;
        }

        // 세션 생성 팩토리 호출
        CRioSessionRef session = _sessionFactory();
        if( session == nullptr )
        {
            CSocketUtils::Close(clientSocket);
            continue;
        }

        // 외부로 Accept 완료 통보 (콜백 호출)
        if( _onAcceptCallback )
        {
            _onAcceptCallback(session, clientSocket, requestQueue, clientAddr);
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

    if( !_isListening.load() )
        return RIO_INVALID_RQ;

    if( _rioCore == nullptr )
        return RIO_INVALID_RQ;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _rioCore->GetRioTable();
    if( rioTable.RIOCreateRequestQueue == nullptr )
        return RIO_INVALID_RQ;

    const RIO_CQ receiveCq = _rioCore->GetReceiveQueue();
    const RIO_CQ sendCq = _rioCore->GetSendQueue();

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