
//***************************************************************************
// IocpListener.cpp: implementation of the CIocpListener class.
//
//***************************************************************************

#include "pch.h"
#include "IocpListener.h"

static_assert(sizeof(AcceptEvent::acceptBuffer) >= 2 * CSocketUtils::kAcceptExAddrLen,
    "acceptBuffer must hold the local and remote address blocks written by AcceptEx");

namespace
{
    //***************************************************************************
    // @brief 수락 중이던 연결 하나가 상대 쪽 사정으로 끊어진 것을 뜻하는 완료 에러인지 판별합니다.
    // @details 접속 직후 RST/취소를 보내는 클라이언트나 포트 스캔 때문에 서버가 정상이어도 흔히 발생한다.
    //          리스너 자체의 문제가 아니므로 백오프 없이 같은 슬롯을 바로 다시 게시한다. 이 에러들은 실제
    //          연결 시도 하나당 한 번씩만 완료되므로 재게시가 스스로 폭주하지 않는다.
    //***************************************************************************
    bool IsPerConnectionAcceptError(DWORD errorCode) noexcept
    {
        return errorCode == ERROR_NETNAME_DELETED
            || errorCode == ERROR_CONNECTION_ABORTED
            || errorCode == WSAECONNRESET
            || errorCode == WSAECONNABORTED;
    }
}

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

    // 2~5. 소켓 옵션(주소 바인딩 방식, Linger) → Bind → Listen → IOCP 코어 등록.
    //      하나라도 실패하면 이미 저장한 Listen 소켓을 닫고 실패를 반환한다.
    if( CSocketUtils::ApplyListenAddressMode(listenSocket, _addressMode) == false ||
        CSocketUtils::SetLinger(listenSocket, 0, 0) == false ||
        CSocketUtils::Bind(listenSocket, netAddr) == false ||
        CSocketUtils::Listen(listenSocket, SOMAXCONN) == false ||
        _iocpCore->Register(GetIocpObjectPtr()) == false )
    {
        CloseSocket();
        return false;
    }

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
// @note exchange로 핸들을 원자적으로 INVALID_SOCKET으로 바꾼 뒤 그 "이전 값"만 닫는다.
//       동시에 CloseSocket()이 여러 스레드에서 호출돼도 closesocket()이 중복
//       호출되지 않는다(idempotent).
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
//          1. _closing = true 후 대기 중인 재시도 스레드를 깨운다 — 이 시점 이후
//             RegisterAccept()/ScheduleRetry()의 모든 진입점이 새 재시도를 시작하지 않고
//             즉시 포기하며, 지연 대기 중이던 재시도 스레드는 바로 종료한다.
//          2. CloseSocket() — pending 상태였던 AcceptEx들이 취소되며, 그
//             완료 통지가 IOCP로 들어와도 ProcessAccept()가 _closing을 보고
//             더 이상 RegisterAccept()를 재호출하지 않는다.
//          3. _pendingRetries가 0이 될 때까지 대기 — 깨어난 재시도 스레드들이
//             _closing을 확인하고 즉시 반환할 때까지 결정론적으로 기다린다.
//          이미 정지된 상태에서 재호출해도 안전하다(idempotent) — 두 번째
//          호출은 _closing이 이미 true이므로 대기만 하고 즉시 반환한다.
//***************************************************************************
void CIocpListener::Stop()
{
    {
        // 재시도 스레드의 wait_for 조건 검사와 직렬화해 깨우기 신호를 놓치지 않게 한다.
        std::lock_guard<std::mutex> lock(_retryDrainMutex);
        _closing.store(true, std::memory_order_release);
    }
    _retryDrainCv.notify_all();

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
//       _listenSocket은 지역 변수로 한 번만 load()해서 사용한다 — Stop()이 동시에
//       CloseSocket()을 호출해도(atomic exchange) 이 함수 안에서는 항상 일관된
//       스냅샷 값을 쓴다.
//***************************************************************************
void CIocpListener::RegisterAccept(AcceptEvent* acceptEvent)
{
    if( _closing.load(std::memory_order_acquire) )
        return;

    const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
    if( listenSocket == INVALID_SOCKET )
        return;

    // 1. 세션 생성 팩터리 호출. 팩토리가 nullptr을 반환하는 경우(예: 세션 풀 순간 고갈)도
    //    실패 경로로 취급해 재시도한다 — 재시도 없이 반환하면 이 슬롯이 영구히 죽는다.
    CIocpObjectRef session = _sessionFactory();
    if( session == nullptr )
    {
        HandleAcceptFailure(acceptEvent, _T("SessionFactory"), 0);
        return;
    }

    // 2. CIocpObject::GetHandle()로 소켓 핸들 추출 (CSession 의존성 없음)
    SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));
    if( sessionSocket == INVALID_SOCKET )
    {
        // session은 이 스코프를 벗어나며 자체 소멸자에게 정리를 위임한다.
        HandleAcceptFailure(acceptEvent, _T("SessionSocket"), 0);
        return;
    }

    // 3. AcceptEvent 초기화 및 소유권 설정
    //    주의: Init()은 OVERLAPPED 필드(와 errorCode)만 되돌리고 retryCount는 건드리지
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

            HandleAcceptFailure(acceptEvent, _T("AcceptEx"), errorCode);
            return;
        }
    }

    // WSA_IO_PENDING(정상 대기) 또는 즉시 성공 → 종료
    // (retryCount 리셋은 Accept가 실제로 완료·성공했을 때인 ProcessAccept()에서
    // 수행한다 — 여기는 아직 "게시"만 됐을 뿐 성공을 의미하지 않는다.)
}

//***************************************************************************
// @brief Accept 실패 경로 공통 처리: retryCount 증가, 샘플링 로그, 백오프 재시도 위임.
//***************************************************************************
void CIocpListener::HandleAcceptFailure(AcceptEvent* acceptEvent, const TCHAR* stage, int32 errorCode)
{
    const int32 failures = ++acceptEvent->retryCount;

    if( failures == 1 || failures % Iocp::kAcceptRetryLogInterval == 0 )
    {
        LOG_WARNING(_T("[CIocpListener] accept failure: stage=%s, error=%d, consecutive=%d - retrying with backoff"),
            stage, errorCode, failures);
    }

    ScheduleRetry(acceptEvent);
}

//***************************************************************************
// @brief 연속 실패 횟수에 대한 재시도 지연(ms)을 계산합니다.
// @return 1회째 Iocp::kAcceptRetryBaseDelayMs, 이후 두 배씩 늘어 Iocp::kAcceptRetryMaxDelayMs에서 멈춥니다.
//***************************************************************************
uint32 CIocpListener::CalcRetryDelayMs(int32 retryCount) noexcept
{
    uint32 delay = Iocp::kAcceptRetryBaseDelayMs;
    for( int32 i = 1; i < retryCount && delay < Iocp::kAcceptRetryMaxDelayMs; ++i )
        delay *= 2;

    return (std::min)(delay, Iocp::kAcceptRetryMaxDelayMs);
}

//***************************************************************************
// @brief 재시도 스레드 종료 처리: _pendingRetries를 감소시키고 마지막이면 Stop()을 깨웁니다.
// @details 감소와 notify를 _retryDrainMutex 아래에서 수행한다. Stop()의 대기 조건 검사도
//          같은 뮤텍스 아래에서 이뤄지므로, Stop()은 이 함수가 뮤텍스를 놓은 뒤에야 0을
//          관측하고 반환한다. 호출 스레드는 이 함수 반환 후 멤버를 건드리지 않는다.
//***************************************************************************
void CIocpListener::FinishRetry() noexcept
{
    std::lock_guard<std::mutex> lock(_retryDrainMutex);
    if( _pendingRetries.fetch_sub(1, std::memory_order_acq_rel) == 1 )
    {
        _retryDrainCv.notify_all();
    }
}

//***************************************************************************
// @brief IOCP 워커 스레드를 블로킹하지 않도록, 재시도를 백오프 지연 후
//        별도의 detached 스레드에서 1회 수행합니다.
// @param acceptEvent 재시도할 AcceptEvent 포인터. retryCount는 이벤트 자신이
//        보유하고 있으므로 별도로 전달하지 않는다.
// @details acceptEvent는 Listener가 소유(_acceptEvents)하는 고정 메모리이므로
//          여기서 별도로 수명 관리할 필요가 없다. Listener 자신은 weak_ptr로
//          캡처하여, 지연 대기 중 Listener가 먼저 소멸되어도 안전하게(lock()
//          실패로) 재시도를 건너뛴다.
// @note _pendingRetries를 스레드 시작 시 증가시키고, 어떤 경로로 빠져나가든
//       종료 시 FinishRetry()로 반드시 감소시킨다. 스레드는 'self' 참조를 쥔 채
//       FinishRetry()를 호출하므로, 마지막 소유자라서 ~CIocpListener()가 이 스레드에서
//       실행되더라도 소멸자의 Stop()은 이미 0이 된 카운트를 보고 즉시 반환한다.
//***************************************************************************
void CIocpListener::ScheduleRetry(AcceptEvent* acceptEvent)
{
    if( _closing.load(std::memory_order_acquire) )
        return;

    const uint32 delayMs = CalcRetryDelayMs(acceptEvent->retryCount);
    std::weak_ptr<CIocpListener> weakSelf = weak_from_this();

    _pendingRetries.fetch_add(1, std::memory_order_acq_rel);

    try
    {
        std::thread([weakSelf, acceptEvent, this, delayMs]()
            {
                // 지연은 Sleep이 아니라 조건 변수로 대기한다 — Stop()이 _closing을 세우고
                // notify하면 남은 지연과 무관하게 즉시 깨어난다.
                {
                    std::unique_lock<std::mutex> lock(_retryDrainMutex);
                    _retryDrainCv.wait_for(lock, std::chrono::milliseconds(delayMs),
                        [this]() { return _closing.load(std::memory_order_acquire); });
                }

                CIocpListenerRef self = weakSelf.lock();
                if( self && !_closing.load(std::memory_order_acquire) )
                {
                    self->RegisterAccept(acceptEvent);
                }

                FinishRetry(); // 'self'는 이 호출 뒤, 람다가 끝날 때 해제된다.
            }).detach();
    }
    catch( ... )
    {
        // std::thread 생성 자체가 실패한 경우(리소스 고갈 등) — IOCP 워커
        // 스레드로 예외가 전파되어 std::terminate()로 이어지는 것을 막기
        // 위해 여기서 흡수하고, 증가시켰던 카운트를 되돌린다.
        FinishRetry();
        LOG_ERROR(_T("[CIocpListener] failed to start accept retry thread - this accept slot is not re-armed"));
    }
}

//***************************************************************************
// @brief AcceptEx 완료 처리
// @param acceptEvent 완료 통지된 AcceptEvent 포인터
// @note AcceptEx가 실패로 완료된 경우(acceptEvent->errorCode != 0)에는 수락된 연결이
//       없으므로 SetUpdateAcceptContext가 실패하고, 그 경로에서 실패 원인으로
//       acceptEvent->errorCode가 로그에 남는다. SetUpdateAcceptContext / IOCP Register
//       실패도 AcceptEx 게시 이후의 실패이므로 동일하게 HandleAcceptFailure()로 처리한다.
//       Stop() 진행 중(_closing==true)이면 session 정리만 하고 어떤
//       재시도/재등록도 하지 않는다 — CloseSocket() 이후 도착하는 취소
//       완료 통지에 대한 정상적인 처리 경로다.
//       성공 시에는 사용자 콜백을 호출하기 전에 이 슬롯의 AcceptEx를 먼저 재게시한다.
//***************************************************************************
void CIocpListener::ProcessAccept(AcceptEvent* acceptEvent)
{
    CIocpObjectRef session = acceptEvent->session;
    const DWORD completionError = acceptEvent->errorCode;

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

    if( session == nullptr )
    {
        HandleAcceptFailure(acceptEvent, _T("NullSession"), 0);
        return;
    }

    // 수락 도중 상대가 연결을 끊은 경우: 이 세션은 버리고(소멸자가 소켓 정리) 슬롯만 바로 다시 게시한다.
    // 백오프 재시도 스레드를 만들면 그 시간 동안 슬롯이 비고, 스캔/RST 폭주 시 스레드가 쌓인다.
    if( completionError != 0 && IsPerConnectionAcceptError(completionError) )
    {
        RegisterAccept(acceptEvent);
        return;
    }

    const SOCKET listenSocket = _listenSocket.load(std::memory_order_acquire);
    SOCKET sessionSocket = static_cast<SOCKET>(reinterpret_cast<ULONG_PTR>(session->GetHandle()));

    // 1. SO_UPDATE_ACCEPT_CONTEXT 설정 (getpeername 및 소켓 옵션 정상 작동에 필수)
    if( listenSocket == INVALID_SOCKET || CSocketUtils::SetUpdateAcceptContext(sessionSocket, listenSocket) == false )
    {
        // session이 함수를 벗어나며 자체 소멸자에서 소켓을 정리하도록 위임
        // (직접 Close 시 session 자체 정리 로직과 이중 Close될 위험이 있다)
        const int32 errorCode = (completionError != 0) ? static_cast<int32>(completionError) : ::WSAGetLastError();

        // RegisterAccept() 즉시 재귀 대신 ScheduleRetry()로 지연 오프로딩 — 실패가
        // 지속돼도(리소스 고갈 등) 워커가 세션 생성까지 포함한 무거운 재시도를 백오프 없이
        // 반복하며 다른 완료 이벤트 처리를 지연시키지 않는다.
        HandleAcceptFailure(acceptEvent, _T("SetUpdateAcceptContext"), errorCode);
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
        HandleAcceptFailure(acceptEvent, _T("RegisterIocp"), static_cast<int32>(::GetLastError()));
        return;
    }

    // 4. Accept 최종 성공 — 누적 실패 카운트를 리셋하고, 사용자 콜백보다 먼저 이 슬롯의
    //    AcceptEx를 재게시한다. 콜백(ProcessConnect()/OnConnected() 포함)이 느려도
    //    그동안 Accept Pool 슬롯이 비지 않는다. acceptEvent는 위에서 이미 비워졌고
    //    콜백은 지역 session만 사용하므로 재게시와 겹쳐도 안전하다.
    acceptEvent->retryCount = 0;
    RegisterAccept(acceptEvent);

    // 5. 콜백 호출 (외부에서 CSession 캐스팅 후 SetNetAddress/ProcessConnect 실행)
    if( _onAcceptCallback )
    {
        _onAcceptCallback(session, netAddr);
    }
}