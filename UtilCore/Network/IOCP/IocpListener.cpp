
//***************************************************************************
// IocpListener.cpp: implementation of the CIocpListener class.
//
//***************************************************************************

#include "pch.h"
#include "IocpListener.h"

#include <thread>

//***************************************************************************
// @brief CIocpListener 생성자
//***************************************************************************
CIocpListener::CIocpListener()
{
}

//***************************************************************************
// @brief CIocpListener 소멸자
// @details Stop()으로 진행 중인 모든 재시도 스레드가 끝난 뒤에만
//          _acceptEvents를 해제합니다(생명주기 안전성 보장).
//***************************************************************************
CIocpListener::~CIocpListener()
{
    Stop();

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
// @param acceptPoolSize 동시 대기할 AcceptEx 개수 (기본 kDefaultAcceptPoolSize)
// @param onAccept Accept 완료 시 호출될 외부 후속 처리 콜백
// @return bool 성공 여부
//***************************************************************************
bool CIocpListener::StartAccept(CIocpCoreRef iocpCore, CNetAddress netAddr, IocpSessionFactory sessionFactory,
    uint32 acceptPoolSize, OnAcceptCallback onAccept)
{
    _iocpCore = iocpCore;
    _sessionFactory = sessionFactory;
    _onAcceptCallback = onAccept;

    if( _iocpCore == nullptr || _sessionFactory == nullptr )
        return false;

    // 1. TCP Listen 소켓 생성
    SOCKET listenSocket = CSocketUtils::CreateSocket();
    if( listenSocket == INVALID_SOCKET )
        return false;

    _listenSocket.store(listenSocket, std::memory_order_release);

    // 2. 소켓 옵션 설정 (주소 재사용, Linger 설정)
    if( CSocketUtils::SetReuseAddress(listenSocket, true) == false )
        return false;

    if( CSocketUtils::SetLinger(listenSocket, 0, 0) == false )
        return false;

    // 3. 주소 바인딩 (Bind)
    if( CSocketUtils::Bind(listenSocket, netAddr) == false )
        return false;

    // 4. 연결 대기 상태 전환 (Listen)
    if( CSocketUtils::Listen(listenSocket, SOMAXCONN) == false )
        return false;

    // 5. Listen 소켓을 IOCP 코어에 등록
    if( _iocpCore->Register(GetIocpObjectPtr()) == false )
        return false;

    // 6. 설정된 개수만큼 AcceptEvent 생성 및 AcceptEx 사전 등록 (Accept Pool)
    _acceptEvents.reserve(static_cast<size_t>(acceptPoolSize));

    for( uint32 i = 0; i < acceptPoolSize; i++ )
    {
        AcceptEvent* acceptEvent = xnew<AcceptEvent>();
        _acceptEvents.push_back(acceptEvent);
        RegisterAccept(acceptEvent);
    }

    return true;
}

//***************************************************************************
// @brief Listen 소켓 닫기 (재시도 스레드는 기다리지 않음 — 완전 종료는 Stop() 사용)
// @note [수정] exchange로 핸들을 원자적으로 INVALID_SOCKET으로 바꾼 뒤 그
//       "이전 값"만 닫는다. 동시에 CloseSocket()이 여러 스레드에서 호출돼도
//       closesocket()이 중복 호출되지 않는다(idempotent).
//***************************************************************************
void CIocpListener::CloseSocket()
{
    SOCKET socketToClose = _listenSocket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
    if( socketToClose != INVALID_SOCKET )
    {
        CSocketUtils::Close(socketToClose);
    }
}

//***************************************************************************
// @brief 리스너를 완전히 정지합니다.
// @details 순서:
//          1. _closing = true — 이 시점 이후 RegisterAccept()/ScheduleRetry()의
//             모든 진입점이 새 재시도를 시작하지 않고 즉시 포기한다.
//          2. CloseSocket() — pending 상태였던 AcceptEx들이 취소되며, 그
//             완료 통지가 IOCP로 들어와도 ProcessAccept()가 _closing을 보고
//             더 이상 RegisterAccept()를 재호출하지 않는다.
//          3. _pendingRetries가 0이 될 때까지 대기 — 이미 Sleep(10) 중이던
//             detached 스레드들이 깨어나 _closing을 확인하고 즉시 반환할
//             때까지 결정론적으로 기다린다(최대 대기 시간은 사실상 10ms 남짓).
//          이미 정지된 상태에서 재호출해도 안전하다(idempotent) — 두 번째
//          호출은 _closing이 이미 true이므로 대기만 하고 즉시 반환한다.
//***************************************************************************
void CIocpListener::Stop()
{
    _closing.store(true, std::memory_order_release);

    CloseSocket();

    std::unique_lock<std::mutex> lock(_retryDrainMutex);
    _retryDrainCv.wait(lock, [this]() { return _pendingRetries.load(std::memory_order_acquire) == 0; });
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
// @brief 비동기 AcceptEx I/O 요청 등록 (실패 시 즉시 반환 — 재시도는 ScheduleRetry()에 위임)
// @param acceptEvent AcceptEx 호출에 사용될 이벤트 포인터. 누적 실패 횟수는
//        acceptEvent->retryCount에 보관되어 RegisterAccept()/ProcessAccept()의
//        모든 실패 경로가 공유한다.
// @note Stop() 진행 중(_closing==true)이면 세션 생성이나 AcceptEx 게시를
//       전혀 시도하지 않고 즉시 반환한다 — 이미 닫힌 _listenSocket에 대고
//       무의미한 재시도를 반복하며 세션을 만들었다 버리는 낭비를 막는다.
//       [수정] _listenSocket을 지역 변수로 한 번만 load()해서 사용한다 —
//       Stop()이 동시에 CloseSocket()을 호출해도(atomic exchange) 이 함수
//       안에서는 항상 일관된 스냅샷 값을 쓰게 된다.
//***************************************************************************
void CIocpListener::RegisterAccept(AcceptEvent* acceptEvent)
{
    if( _closing.load(std::memory_order_acquire) )
        return;

    const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
    if( listenSocket == INVALID_SOCKET )
        return;

    // 1. 세션 생성 팩터리 호출
    // [수정] 팩토리가 nullptr을 반환하는 경우(예: 세션 풀 순간 고갈)도 이
    // 함수가 공유하는 4가지 실패 경로 중 하나로 취급해야 한다. 기존에는
    // retryCount 증가/ScheduleRetry() 없이 그냥 반환해, 이 AcceptEvent
    // 슬롯이 재등록되지 않고 영구히 죽었다 — 여러 슬롯에서 누적되면 Accept
    // Pool이 조용히 줄어들다 결국 신규 연결을 못 받는 상태로 갈 수 있었다.
    CIocpObjectRef session = _sessionFactory();
    if( session == nullptr )
    {
        if( ++acceptEvent->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - 세션 팩토리 반복 실패, Accept 재등록 포기
            return;
        }
        ScheduleRetry(acceptEvent);
        return;
    }

    // 2. CIocpObject::GetHandle()로 소켓 핸들 추출 (CSession 의존성 없음)
    SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));
    if( sessionSocket == INVALID_SOCKET )
    {
        // session은 이 스코프를 벗어나며 자체 소멸자에게 정리를 위임한다.
        if( ++acceptEvent->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - 세션 소켓 생성 반복 실패, Accept 재등록 포기
            return;
        }
        ScheduleRetry(acceptEvent);
        return;
    }

    // 3. AcceptEvent 초기화 및 소유권 설정
    //    주의: Init()은 OVERLAPPED 필드만 0으로 되돌리고 retryCount는 건드리지
    //    않는다 — retryCount는 실패 경로 간 공유 상태이므로 여기서 리셋되면 안 된다.
    acceptEvent->Init();
    acceptEvent->owner = shared_from_this(); // I/O 완료 시까지 Listener 수명 보장 (ref count +1)
    acceptEvent->session = session;

    DWORD bytesReceived = 0;

    // 4. AcceptEx 호출 (dwReceiveDataLength = 0: 접속 즉시 완료 통지 받음)
    BOOL result = CSocketUtils::AcceptEx(
        listenSocket,
        sessionSocket,
        acceptEvent->acceptBuffer,
        0,
        CSocketUtils::kAcceptExAddrLen,
        CSocketUtils::kAcceptExAddrLen,
        OUT & bytesReceived,
        static_cast<LPOVERLAPPED>(acceptEvent)
    );

    if( result == FALSE )
    {
        const int32 errorCode = ::WSAGetLastError();
        if( errorCode != WSA_IO_PENDING )
        {
            // AcceptEx 즉시 실패: 소유권만 해제하고 재시도.
            // session(shared_ptr)이 이번 스코프를 벗어나며
            // 자체 소멸자에서 소켓을 정리하도록 위임한다.
            // → 여기서 CSocketUtils::Close(sessionSocket)를 직접 호출하지 않는다.
            acceptEvent->owner = nullptr;
            acceptEvent->session = nullptr;

            if( ++acceptEvent->retryCount > kMaxAcceptRetry )
            {
                // TODO: 로그 - AcceptEx 반복 실패, Accept 재등록 포기
                return;
            }
            ScheduleRetry(acceptEvent);
            return;
        }
    }

    // WSA_IO_PENDING(정상 대기) 또는 즉시 성공 → 종료
    // (retryCount 리셋은 Accept가 실제로 완료·성공했을 때인 ProcessAccept()의
    // 4단계에서 수행한다 — 여기는 아직 "게시"만 됐을 뿐 성공을 의미하지 않는다.)
}

//***************************************************************************
// @brief IOCP 워커 스레드를 블로킹하지 않도록, 재시도를 짧은 지연 후
//        별도의 detached 스레드에서 1회 수행합니다.
// @param acceptEvent 재시도할 AcceptEvent 포인터. retryCount는 이벤트 자신이
//        보유하고 있으므로 별도로 전달하지 않는다.
// @details acceptEvent는 Listener가 소유(_acceptEvents)하는 고정 메모리이므로
//          여기서 별도로 수명 관리할 필요가 없다. Listener 자신은 weak_ptr로
//          캡처하여, 지연 대기 중 Listener가 먼저 소멸되어도 안전하게(lock()
//          실패로) 재시도를 건너뛴다.
// @note _pendingRetries를 스레드 시작 시 증가시키고, 어떤 경로로 빠져나가든
//       종료 시 반드시 감소시킨 뒤 Stop()이 대기 중인 조건 변수를 notify한다.
//***************************************************************************
void CIocpListener::ScheduleRetry(AcceptEvent* acceptEvent)
{
    std::weak_ptr<CIocpListener> weakSelf = weak_from_this();

    _pendingRetries.fetch_add(1, std::memory_order_acq_rel);

    try
    {
        std::thread([weakSelf, acceptEvent, this]()
            {
                // Stop()이 이미 진행 중이면 즉시 포기(불필요한 Sleep도 생략).
                if( !_closing.load(std::memory_order_acquire) )
                {
                    ::Sleep(10);

                    if( !_closing.load(std::memory_order_acquire) )
                    {
                        if( CIocpListenerRef self = weakSelf.lock() )
                        {
                            self->RegisterAccept(acceptEvent);
                        }
                    }
                }

                if( _pendingRetries.fetch_sub(1, std::memory_order_acq_rel) == 1 )
                {
                    // 마지막 pending retry였다면 Stop()에서 대기 중일 수 있는
                    // 조건 변수를 깨운다.
                    std::lock_guard<std::mutex> lock(_retryDrainMutex);
                    _retryDrainCv.notify_all();
                }
            }).detach();
    }
    catch( ... )
    {
        // std::thread 생성 자체가 실패한 경우(리소스 고갈 등) — IOCP 워커
        // 스레드로 예외가 전파되어 std::terminate()로 이어지는 것을 막기
        // 위해 여기서 흡수하고, 증가시켰던 카운트를 되돌린다.
        if( _pendingRetries.fetch_sub(1, std::memory_order_acq_rel) == 1 )
        {
            std::lock_guard<std::mutex> lock(_retryDrainMutex);
            _retryDrainCv.notify_all();
        }
        // TODO: 로그 - 재시도 스레드 생성 실패, 이 슬롯의 Accept 재등록 포기
    }
}

//***************************************************************************
// @brief AcceptEx 완료 처리
// @param acceptEvent 완료 통지된 AcceptEvent 포인터
// @note SetUpdateAcceptContext / IOCP Register 실패 경로도 성공적인 AcceptEx
//       게시 이후의 실패이므로 동일하게 acceptEvent->retryCount를 증가시키고
//       kMaxAcceptRetry를 넘기면 재등록을 포기한다.
//       Stop() 진행 중(_closing==true)이면 session 정리만 하고 어떤
//       재시도/재등록도 하지 않는다 — CloseSocket() 이후 도착하는 취소
//       완료 통지에 대한 정상적인 처리 경로다.
//       [수정] _listenSocket을 지역 변수로 한 번만 load()해서 사용한다.
//***************************************************************************
void CIocpListener::ProcessAccept(AcceptEvent* acceptEvent)
{
    CIocpObjectRef session = acceptEvent->session;

    // 수명 관리 해제 (Ref Count -1)
    acceptEvent->owner = nullptr;
    acceptEvent->session = nullptr;

    if( _closing.load(std::memory_order_acquire) )
    {
        // Stop() 진행 중에 도착한 완료(주로 취소/오류) — session은 이 함수
        // 스코프를 벗어나며 자체 소멸자가 소켓을 정리하도록 위임하고,
        // 더 이상 재게시하지 않는다.
        return;
    }

    const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
    SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));

    // 1. SO_UPDATE_ACCEPT_CONTEXT 설정 (getpeername 및 소켓 옵션 정상 작동에 필수)
    if( listenSocket == INVALID_SOCKET || CSocketUtils::SetUpdateAcceptContext(sessionSocket, listenSocket) == false )
    {
        // session이 함수를 벗어나며 자체 소멸자에서 소켓을 정리하도록 위임
        // (직접 Close 시 session 자체 정리 로직과 이중 Close될 위험이 있었음)
        if( ++acceptEvent->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - SetUpdateAcceptContext 반복 실패, Accept 재등록 포기
            return;
        }
        // [수정] RegisterAccept() 즉시 재귀 대신 ScheduleRetry()로 지연 오프로딩.
        // 이 실패가 지속되면(리소스 고갈 등) 워커 스레드가 세션 생성까지 포함한
        // 무거운 재시도를 백오프 없이 그 자리에서 kMaxAcceptRetry회 반복하며
        // 다른 완료 이벤트 처리를 지연시킬 수 있다 — RegisterAccept() 자신의
        // 실패 경로와 동일한 정책으로 통일한다.
        ScheduleRetry(acceptEvent);
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
        if( ++acceptEvent->retryCount > kMaxAcceptRetry )
        {
            // TODO: 로그 - IOCP Register 반복 실패, Accept 재등록 포기
            return;
        }
        // [수정] 위와 동일한 이유로 ScheduleRetry()로 통일.
        ScheduleRetry(acceptEvent);
        return;
    }

    // 4. 콜백 호출 (외부에서 CSession 캐스팅 후 SetNetAddress/ProcessConnect 실행)
    if( _onAcceptCallback )
    {
        _onAcceptCallback(session, netAddr);
    }

    // Accept가 여기까지 도달하면 최종 성공이므로 누적 실패 카운트를 리셋한다.
    acceptEvent->retryCount = 0;

    // 5. 다음 클라이언트를 받기 위해 AcceptEvent 재등록
    RegisterAccept(acceptEvent);
}