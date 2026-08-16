
//***************************************************************************
// IocpListener.cpp: implementation of the CIocpListener class.
//
//***************************************************************************

#include "pch.h"
#include "IocpListener.h"

//***************************************************************************
// @brief CIocpListener 생성자
//***************************************************************************
CIocpListener::CIocpListener()
{
}

//***************************************************************************
// @brief CIocpListener 소멸자
// @details 소켓을 닫고 관리 중인 모든 AcceptEvent의 메모리를 해제합니다.
//***************************************************************************
CIocpListener::~CIocpListener()
{
    CloseSocket();

    for( AcceptEvent* acceptEvent : _acceptEvents )
    {
        xdelete(acceptEvent);
    }
    _acceptEvents.clear();
}

//***************************************************************************
// @brief 리스너 초기화 및 AcceptEx 대기 등록
// @param iocpCore IOCP 코어 참조 객체
// @param netAddr 리슨할 네트워크 주소 (IP/Port)
// @param sessionFactory 세션 생성 람다/함수 포인터
// @param acceptCount 동시 대기할 AcceptEx 수 (Accept Pool 크기)
// @param onAccept Accept 완료 시 호출될 외부 후속 처리 콜백
// @return bool 성공 여부
//***************************************************************************
bool CIocpListener::StartAccept(CIocpCoreRef iocpCore, CNetAddress netAddr, IocpSessionFactory sessionFactory,
    int32 acceptCount, OnAcceptCallback onAccept)
{
    _iocpCore = iocpCore;
    _sessionFactory = sessionFactory;
    _onAcceptCallback = onAccept;

    if( _iocpCore == nullptr || _sessionFactory == nullptr )
        return false;

    // 1. TCP Listen 소켓 생성
    _listenSocket = CSocketUtils::CreateSocket();
    if( _listenSocket == INVALID_SOCKET )
        return false;

    // 2. 소켓 옵션 설정 (주소 재사용, Linger 설정)
    if( CSocketUtils::SetReuseAddress(_listenSocket, true) == false )
        return false;

    if( CSocketUtils::SetLinger(_listenSocket, 0, 0) == false )
        return false;

    // 3. 주소 바인딩 (Bind)
    if( CSocketUtils::Bind(_listenSocket, netAddr) == false )
        return false;

    // 4. 연결 대기 상태 전환 (Listen)
    if( CSocketUtils::Listen(_listenSocket, SOMAXCONN) == false )
        return false;

    // 5. Listen 소켓을 IOCP 코어에 등록
    if( _iocpCore->Register(GetIocpObjectPtr()) == false )
        return false;

    // 6. 설정된 개수만큼 AcceptEvent 생성 및 AcceptEx 사전 등록 (Accept Pool)
    for( int32 i = 0; i < acceptCount; i++ )
    {
        AcceptEvent* acceptEvent = xnew<AcceptEvent>();
        _acceptEvents.push_back(acceptEvent);

        RegisterAccept(acceptEvent);
    }

    return true;
}

//***************************************************************************
// @brief Listen 소켓 닫기
//***************************************************************************
void CIocpListener::CloseSocket()
{
    if( _listenSocket != INVALID_SOCKET )
    {
        CSocketUtils::Close(_listenSocket);
        _listenSocket = INVALID_SOCKET;
    }
}

//***************************************************************************
// @brief IOCP Dispatch 함수 구현
// @param iocpEvent 완료 통지된 IOCP 이벤트 (AcceptEvent)
// @param numOfBytes 전송된 바이트 수
//***************************************************************************
void CIocpListener::Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes)
{
    ASSERT_CRASH(iocpEvent->eventType == Iocp::EventType::Accept);

    AcceptEvent* acceptEvent = static_cast<AcceptEvent*>(iocpEvent);
    ProcessAccept(acceptEvent);
}

//***************************************************************************
// @brief 비동기 AcceptEx I/O 요청 등록 (루프 기반 재시도, 최대 kMaxAcceptRetry회)
// @param acceptEvent AcceptEx 호출에 사용될 이벤트 포인터
// @param retryCount 누적 연속 실패 횟수
//***************************************************************************
void CIocpListener::RegisterAccept(AcceptEvent* acceptEvent, int32 retryCount)
{
    for( ;;)
    {
        // 1. 세션 생성 팩터리 호출
        CIocpObjectRef session = _sessionFactory();
        if( session == nullptr )
            return;

        // 2. CIocpObject::GetHandle()로 소켓 핸들 추출 (CSession 의존성 없음)
        SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));
        if( sessionSocket == INVALID_SOCKET )
        {
            // session은 이 스코프를 벗어나며 자체 소멸자에게 정리를 위임한다.
            if( ++retryCount > kMaxAcceptRetry )
            {
                // TODO: 로그 - 세션 소켓 생성 반복 실패, Accept 재등록 포기
                return;
            }
            ::Sleep(10);
            continue;
        }

        // 3. AcceptEvent 초기화 및 소유권 설정
        acceptEvent->Init();
        acceptEvent->owner = shared_from_this(); // I/O 완료 시까지 Listener 수명 보장 (ref count +1)
        acceptEvent->session = session;

        DWORD bytesReceived = 0;

        // 4. AcceptEx 호출 (dwReceiveDataLength = 0: 접속 즉시 완료 통지 받음)
        BOOL result = CSocketUtils::AcceptEx(
            _listenSocket,
            sessionSocket,
            acceptEvent->acceptBuffer,
            0,
            sizeof(SOCKADDR_IN) + 16,
            sizeof(SOCKADDR_IN) + 16,
            OUT & bytesReceived,
            static_cast<LPOVERLAPPED>(acceptEvent)
        );

        if( result == FALSE )
        {
            const int32 errorCode = ::WSAGetLastError();
            if( errorCode != WSA_IO_PENDING )
            {
                // AcceptEx 즉시 실패: 소유권만 해제하고 재시도.
                // session(shared_ptr)이 이번 루프 스코프를 벗어나며
                // 자체 소멸자에서 소켓을 정리하도록 위임한다.
                // → 여기서 CSocketUtils::Close(sessionSocket)를 직접 호출하지 않는다.
                acceptEvent->owner = nullptr;
                acceptEvent->session = nullptr;

                if( ++retryCount > kMaxAcceptRetry )
                {
                    // TODO: 로그 - AcceptEx 반복 실패, Accept 재등록 포기
                    return;
                }
                ::Sleep(10);
                continue;
            }
        }

        return;  // WSA_IO_PENDING(정상 대기) 또는 즉시 성공 → 종료
    }
}

//***************************************************************************
// @brief AcceptEx 완료 처리
// @param acceptEvent 완료 통지된 AcceptEvent 포인터
//***************************************************************************
void CIocpListener::ProcessAccept(AcceptEvent* acceptEvent)
{
    CIocpObjectRef session = acceptEvent->session;

    // 수명 관리 해제 (Ref Count -1)
    acceptEvent->owner = nullptr;
    acceptEvent->session = nullptr;

    SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));

    // 1. SO_UPDATE_ACCEPT_CONTEXT 설정 (getpeername 및 소켓 옵션 정상 작동에 필수)
    if( CSocketUtils::SetUpdateAcceptContext(sessionSocket, _listenSocket) == false )
    {
        // session이 함수를 벗어나며 자체 소멸자에서 소켓을 정리하도록 위임
        // (직접 Close 시 session 자체 정리 로직과 이중 Close될 위험이 있었음)
        RegisterAccept(acceptEvent);
        return;
    }

    // 2. 클라이언트 IP/Port 주소 추출
    CNetAddress netAddr;
    SOCKADDR_IN sockAddr;
    if( CSocketUtils::GetPeerAddress(sessionSocket, sockAddr) )
    {
        netAddr = CNetAddress(sockAddr);
    }

    // 3. 수락된 세션을 IOCP 코어에 등록
    if( _iocpCore->Register(session) == false )
    {
        // 동일하게 직접 Close하지 않고 session 소멸에 위임
        RegisterAccept(acceptEvent);
        return;
    }

    // 4. 콜백 호출 (외부에서 CSession 캐스팅 후 SetNetAddress/ProcessConnect 실행)
    if( _onAcceptCallback )
    {
        _onAcceptCallback(session, netAddr);
    }

    // 5. 다음 클라이언트를 받기 위해 AcceptEvent 재등록
    RegisterAccept(acceptEvent);
}