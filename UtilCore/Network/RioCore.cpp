
//***************************************************************************
// RioCore.cpp : implementation of the CRioCore class.
//
//***************************************************************************

#include "pch.h"
#include "RioCore.h"

thread_local CRioCore* CRioCore::_tlsDispatchCore = nullptr;

//***************************************************************************
// @brief CRioCore 기본 생성자
// @details 멤버 변수들을 안전한 초기값 및 무효 상태로 초기화합니다.
//***************************************************************************
CRioCore::CRioCore()
    : _cq(RIO_INVALID_CQ), _iocpHandle(NULL), _cqIdentifier(0), _eventPool(nullptr)
    , _state(State::Uninitialized), _workerRunning(false), _workerFaulted(false)
    , _outstandingIo(0), _cqCorrupted(false)
{
    ZeroMemory(&_rioTable, sizeof(_rioTable));
    ZeroMemory(&_rioNotification, sizeof(_rioNotification));
    ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));
}

//***************************************************************************
// @brief CRioCore 소멸자
// @details 객체 소멸 전 안전한 종료 처리를 위해 Shutdown을 보장합니다.
//***************************************************************************
CRioCore::~CRioCore()
{
    const ShutdownResult result = Shutdown();

    const State state = _state.load(std::memory_order_acquire);

    assert(state == State::Closed && "CRioCore failed to release all resources");

    (void)result;
}

//***************************************************************************
// @brief RIO 함수 테이블 로드 및 CQ/IOCP 오브젝트 초기화
// @param socket RIO 함수 포인터 바인딩을 위한 소켓 핸들
// @param maxCompletionResults CQ가 수용할 수 있는 최대 완료 엔트리 수
// @param cqIdentifier 무결성 검증용 고유 CQ 키
// @param eventPool 이벤트 객체를 관리하는 메모리풀 포인터
// @return 초기화 성공 시 true, 실패 시 false 반환
//***************************************************************************
bool CRioCore::Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool)
{
    if( eventPool == nullptr )
        return false;

    if( cqIdentifier == 0 )
        return false;

    if( socket == INVALID_SOCKET )
        return false;

    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE )
    {
        return false;
    }

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    if( _state.load(std::memory_order_acquire) != State::Uninitialized )
    {
        return false;
    }

    _state.store(State::Initializing, std::memory_order_release);

    RIO_EXTENSION_FUNCTION_TABLE tempRioTable{};
    GUID guid = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;

    int result = ::WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &tempRioTable, sizeof(tempRioTable), &bytes, nullptr, nullptr);
    if( result == SOCKET_ERROR )
    {
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    if( tempRioTable.RIOCreateCompletionQueue == nullptr ||
        tempRioTable.RIOCloseCompletionQueue == nullptr ||
        tempRioTable.RIODequeueCompletion == nullptr ||
        tempRioTable.RIONotify == nullptr )
    {
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    HANDLE tempIocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if( tempIocp == nullptr )
    {
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    /*
        * IMPORTANT:
        *   - 절대로 지역 OVERLAPPED를 사용하지 않는다.
        *   - RIO CQ가 살아있는 동안 유지되어야 하는 CRioCore 멤버 OVERLAPPED를 직접 사용한다.
    */
    ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));
    ZeroMemory(&_rioNotification, sizeof(_rioNotification));

    _rioNotification.Type = RIO_IOCP_COMPLETION;
    _rioNotification.Iocp.IocpHandle = tempIocp;
    _rioNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier);
    _rioNotification.Iocp.Overlapped = &_rioOverlapped;

    RIO_CQ tempCq = tempRioTable.RIOCreateCompletionQueue(maxCompletionResults, &_rioNotification);
    if( tempCq == RIO_INVALID_CQ )
    {
        ::CloseHandle(tempIocp);

        ZeroMemory(&_rioNotification, sizeof(_rioNotification));

        ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));

        _state.store(State::Uninitialized, std::memory_order_release);

        return false;
    }

    _rioTable = tempRioTable;
    _cq = tempCq;
    _iocpHandle = tempIocp;
    _cqIdentifier = cqIdentifier;
    _eventPool = eventPool;

    _outstandingIo.store(0, std::memory_order_relaxed);
    _cqCorrupted.store(false, std::memory_order_relaxed);
    _workerFaulted.store(false, std::memory_order_relaxed);
    _workerRunning.store(false, std::memory_order_relaxed);
    _workerThreadId.store(std::thread::id{}, std::memory_order_relaxed);

    _shutdownInProgress = false;
    _shutdownDone = false;

    _lastShutdownResult = ShutdownResult::Success;

    _state.store(State::Initialized, std::memory_order_release);

    return true;
}

//***************************************************************************
// @brief 외부에서 명시적인 정지를 호출할 때 사용하는 인터페이스 함수
//***************************************************************************
void CRioCore::RequestStop()
{
    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);
    StopInternal();
}

//***************************************************************************
// @brief 내부 상태를 Stopping으로 변경하고 대기 중인 IOCP 워커를 깨우는 함수
// @note
//      PRECONDITION: _lifecycleMutex is held by caller.
//      ACQUIRES: _submissionMutex (exclusive)
//      DOES NOT ACQUIRE: _dispatchGate
//***************************************************************************
void CRioCore::StopInternal()
{
    // IMPORTANT:
    // 호출자는 _lifecycleMutex를 보유한 상태여야 한다.
    //
    // 이 함수는 submissionMutex를 획득하여
    // 신규 I/O submission과 정지 요청 사이의
    // admission race를 차단한다.

    std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

    State current = _state.load(std::memory_order_acquire);
    if( current != State::Running && current != State::Initialized )
    {
        return;
    }

    _state.store(State::Stopping, std::memory_order_release);

    // Worker가 GQCS(INFINITE)에서 대기 중일 수 있으므로
    // 반드시 wake-up 한다.
    if( _iocpHandle != NULL && _workerRunning.load(std::memory_order_acquire) )
    {
        if( !::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr) )
        {
            _workerFaulted.store(true, std::memory_order_release);
            _state.store(State::Faulted, std::memory_order_release);
        }
    }
}

//***************************************************************************
// @brief 워커 스레드 내부 예외 발생 시 결함(Fault) 상태로 전환하는 내부 함수
//***************************************************************************
void CRioCore::FaultInternal() noexcept
{
    try
    {
        std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

        State current = _state.load(std::memory_order_acquire);
        if( current == State::Running || current == State::Stopping )
        {
            _state.store(State::Faulted, std::memory_order_release);
        }

        _workerFaulted.store(true, std::memory_order_release);

        if( _iocpHandle != NULL )
        {
            BOOL posted = ::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
            if( !posted )
            {
                DWORD err = ::GetLastError();
                (void)err;
                assert(false && "PostQueuedCompletionStatus failed during FaultInternal");
            }
        }
    }
    catch( ... )
    {
        assert(false && "CRioCore::FaultInternal unexpected exception");
    }
}

//***************************************************************************
// @brief 완료 큐 배치 처리를 수행하는 퍼블릭 진입점 함수
// @param mode 대기 모드 (Wait 또는 Drain)
// @return 처리된 완료 건수 또는 에러 코드 반환
//***************************************************************************
int32 CRioCore::DispatchBatch(DispatchMode mode)
{
    std::shared_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    CRioCore* previousDispatchCore = _tlsDispatchCore;
    _tlsDispatchCore = this;

    struct DispatchTlsGuard
    {
        CRioCore*& slot;
        CRioCore* previous;
        ~DispatchTlsGuard() noexcept { slot = previous; }
    };

    DispatchTlsGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

    return _DispatchBatchImpl(mode);
}

//***************************************************************************
// @brief 실제 이벤트 수거, 알림 등록, IOCP 대기를 수행하는 핵심 로직 함수
//***************************************************************************
int32 CRioCore::_DispatchBatchImpl(DispatchMode mode)
{
    if( _cqCorrupted.load(std::memory_order_acquire) ) return kCorruptCq;

    if( mode == DispatchMode::Wait )
    {
        std::thread::id workerId = _workerThreadId.load(std::memory_order_acquire);
        if( workerId == std::thread::id{} || std::this_thread::get_id() != workerId )
        {
            assert(false && "DispatchBatch(Wait) must only be called from worker thread");
            return kInvalidCompletion;
        }
    }

    RIORESULT results[BATCH_SIZE]{};
    ULONG numResults = 0;

    // 1단계: RIONotify 없이 이미 완료되어 대기 중인 이벤트 우선 수거
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
        if( _cq == RIO_INVALID_CQ ) return kInvalidCompletion;

        numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        return kCorruptCq;
    }

    if( numResults > 0 )
    {
        DispatchResults(results, numResults);
        return static_cast<int32>(numResults);
    }

    State s = _state.load(std::memory_order_acquire);
    if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == State::Stopping ) TryTransitionState(State::Stopping, State::Stopped);
        return kStopped;
    }

    if( mode == DispatchMode::Drain ) return 0;

    // 2단계: RIONotify를 등록하여 다음 완료 이벤트를 통지받도록 설정 후 재확인
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
        if( _cq == RIO_INVALID_CQ ) return kInvalidCompletion;

        int notifyResult = _rioTable.RIONotify(_cq);
        if( notifyResult != ERROR_SUCCESS && notifyResult != WSAEALREADY )
        {
            DWORD err = static_cast<DWORD>(notifyResult);
            (void)err;
            assert(false && "RIONotify failed");
            return kNotifyError;
        }

        numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        return kCorruptCq;
    }

    if( numResults > 0 )
    {
        DispatchResults(results, numResults);
        return static_cast<int32>(numResults);
    }

    s = _state.load(std::memory_order_acquire);
    if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == State::Stopping ) TryTransitionState(State::Stopping, State::Stopped);
        return kStopped;
    }

    // 3단계: 이벤트가 없다면 IOCP 대기 상태로 전환 (GetQueuedCompletionStatus)
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        DWORD err = ::GetLastError();
        if( overlapped != nullptr )
        {
            (void)err;
            assert(false && "GetQueuedCompletionStatus returned failed completion packet");
            return kIocpError;
        }
        (void)err;
        assert(false && "GetQueuedCompletionStatus failed without completion packet");
        return kIocpError;
    }

    if( completionKey == 0 && overlapped == nullptr )
    {
        s = _state.load(std::memory_order_acquire);
        if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
        {
            if( s == State::Stopping ) TryTransitionState(State::Stopping, State::Stopped);
            return kStopped;
        }
        return 0;
    }

    if( completionKey != _cqIdentifier || overlapped != &_rioOverlapped )
    {
        assert(false && "Unexpected IOCP completion packet");
        return kInvalidCompletion;
    }

    // 4단계: IOCP 깨어남 수신 후 CQ에서 최종 완료 이벤트 일괄 수거
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
        if( _cq == RIO_INVALID_CQ ) return kInvalidCompletion;

        numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        return kCorruptCq;
    }

    if( numResults == 0 ) return 0;

    DispatchResults(results, numResults);
    return static_cast<int32>(numResults);
}

//***************************************************************************
// @brief 수거된 RIO 결과 배열을 순회하며 각 이벤트 처리를 위임하는 함수
//***************************************************************************
void CRioCore::DispatchResults(RIORESULT* results, ULONG numResults) noexcept
{
    if( results == nullptr || numResults == 0 )
        return;

    for( ULONG i = 0; i < numResults; ++i )
    {
        if( results[i].RequestContext == 0 )
        {
            _cqCorrupted.store(true, std::memory_order_release);
            _workerFaulted.store(true, std::memory_order_release);

            assert(false && "RIO completion contains null RequestContext");

            continue;
        }

        CRioEvent* rioEvent = reinterpret_cast<CRioEvent*>(static_cast<ULONG_PTR>(results[i].RequestContext));

        ProcessRioResult(results[i].Status, results[i].BytesTransferred, rioEvent);
    }
}

//***************************************************************************
// @brief 코어 리소스를 해제하고 모든 미완료 I/O가 처리될 때까지 드레인하는 함수
// @param drainTimeout 드레인 대기 제한 시간
// @return 종료 결과 상태 값 반환
//***************************************************************************
ShutdownResult CRioCore::Shutdown(std::chrono::milliseconds drainTimeout)
{
    // ---------------------------------------------------------------------
    // Self-call 방지
    // ---------------------------------------------------------------------
    if( _tlsDispatchCore == this )
    {
        assert(false && "CRioCore::Shutdown() must not be called from Dispatch callback");

        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);

        _lastShutdownResult = ShutdownResult::InvalidCall;
        return _lastShutdownResult;
    }

    if( drainTimeout < std::chrono::milliseconds::zero() )
    {
        drainTimeout = std::chrono::milliseconds::zero();
    }

    std::thread threadToJoin;

    // =====================================================================
    // Phase 1. Shutdown Admission 차단 및 Worker Stop 요청
    // =====================================================================
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        State currentState = _state.load(std::memory_order_acquire);

        if( currentState == State::Closed || _shutdownDone )
        {
            return _lastShutdownResult;
        }

        if( _shutdownInProgress )
        {
            _shutdownCv.wait(lifecycleLock, [this] { return _shutdownDone || !_shutdownInProgress; });

            return _lastShutdownResult;
        }

        if( currentState == State::Uninitialized )
        {
            _lastShutdownResult = ShutdownResult::Success;
            _shutdownDone = true;

            return _lastShutdownResult;
        }

        if( _workerThread.joinable() && _workerThread.get_id() == std::this_thread::get_id() )
        {
            assert(false && "CRioCore::Shutdown() self-join");

            _lastShutdownResult = ShutdownResult::InvalidCall;

            return _lastShutdownResult;
        }

        _shutdownInProgress = true;

        StopInternal();

        if( _workerThread.joinable() )
        {
            threadToJoin = std::move(_workerThread);
        }
    }

    // =====================================================================
    // Phase 2. Dispatch Domain 독점 진입
    // =====================================================================
    std::unique_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    // =====================================================================
    // Phase 3. Worker 종료 및 Join 완료
    // =====================================================================
    if( threadToJoin.joinable() )
    {
        threadToJoin.join();
    }

    _workerThreadId.store(std::thread::id{}, std::memory_order_release);

    // =====================================================================
    // Phase 4. Outstanding I/O Drain 처리
    // =====================================================================
    ShutdownResult finalResult = ShutdownResult::Success;

    {
        const auto deadline = std::chrono::steady_clock::now() + drainTimeout;

        constexpr auto kDrainPollInterval = std::chrono::milliseconds(1);

        CRioCore* previousDispatchCore = _tlsDispatchCore;

        _tlsDispatchCore = this;

        struct ShutdownDrainTlsGuard
        {
            CRioCore*& slot;
            CRioCore* previous;

            ~ShutdownDrainTlsGuard() noexcept
            {
                slot = previous;
            }
        };

        ShutdownDrainTlsGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

        while( _outstandingIo.load(std::memory_order_acquire) > 0 )
        {
            if( _cqCorrupted.load(std::memory_order_acquire) )
            {
                finalResult = ShutdownResult::CorruptCq;
                break;
            }

            const auto now = std::chrono::steady_clock::now();

            if( now >= deadline )
            {
                finalResult = ShutdownResult::DrainTimeout;
                break;
            }

            const int32 result = _DispatchBatchImpl(DispatchMode::Drain);

            if( result == kCorruptCq )
            {
                finalResult = ShutdownResult::CorruptCq;
                break;
            }

            if( result == kNotifyError || result == kIocpError || result == kInvalidCompletion )
            {
                finalResult = ShutdownResult::DispatchError;
                break;
            }

            if( _outstandingIo.load(std::memory_order_acquire) > 0 )
            {
                const auto afterDispatch = std::chrono::steady_clock::now();

                if( afterDispatch >= deadline )
                {
                    finalResult = ShutdownResult::DrainTimeout;
                    break;
                }

                const auto remaining = deadline - afterDispatch;

                const auto sleepDuration = std::min(remaining, std::chrono::duration_cast<std::chrono::steady_clock::duration>(kDrainPollInterval));

                if( sleepDuration > std::chrono::steady_clock::duration::zero() )
                {
                    std::this_thread::sleep_for(sleepDuration);
                }
            }
        }
    }

    // =====================================================================
    // Phase 5. Drain 및 Worker/CQ 상태 검증
    // =====================================================================
    if( finalResult == ShutdownResult::Success )
    {
        if( _outstandingIo.load(std::memory_order_acquire) != 0 )
        {
            finalResult = ShutdownResult::DrainTimeout;
        }
        else if( _cqCorrupted.load(std::memory_order_acquire) )
        {
            finalResult = ShutdownResult::CorruptCq;
        }
        else if( _workerFaulted.load(std::memory_order_acquire) )
        {
            finalResult = ShutdownResult::DispatchError;
        }
    }

    // =====================================================================
    // Phase 6. RIO/IOCP Resource Destruction
    //
    // Resource destruction policy:
    //
    //   ShutdownResult가 Success인지 여부는 resource destruction의
    //   직접적인 허용 조건이 아니다.
    //
    //   Faulted / DispatchError / CorruptCq 상태에서도 다음 조건을
    //   모두 만족하면 최종 리소스 해제를 수행한다.
    //
    //       1. Worker thread가 완전히 종료되었다.
    //       2. dispatchGate를 exclusive로 획득했다.
    //       3. cqConsumerMutex를 획득했다.
    //       4. outstandingIo == 0 이다.
    //
    //   반대로 outstandingIo > 0이면 ShutdownResult와 관계없이
    //   RIO CQ / IOCP를 절대로 파괴하지 않는다.
    //
    // Resource safety rule:
    //
    //       Worker stopped
    //       + Dispatch exclusive
    //       + CQ consumer exclusive
    //       + outstandingIo == 0
    //                 |
    //                 v
    //       RIO CQ destruction allowed
    //                 |
    //                 v
    //       IOCP destruction allowed
    //
    // Fault policy:
    //
    //   Faulted / CorruptCq는 "리소스를 해제하지 말라"는 의미가 아니다.
    //   해당 상태에서는 정상적인 completion processing을 더 이상
    //   신뢰하지 않으며, 안전한 drain이 불가능할 경우 즉시 중단한다.
    //
    //   단, outstandingIo == 0이고 모든 consumer가 종료된 경우에는
    //   리소스를 최종 폐기한다.
    //
    // Destruction order:
    //
    //       RIO CQ
    //           ↓
    //       IOCP
    //           ↓
    //       runtime binding reset
    //
    // Lock order:
    //
    //       dispatchGate (exclusive)
    //           ↓
    //       cqConsumerMutex
    //
    // IMPORTANT:
    //
    //   - lifecycleMutex는 절대 획득하지 않는다.
    //   - resource destruction은 이 Phase에서만 수행한다.
    //   - outstandingIo > 0인 경우 resource destruction을 수행하지 않는다.
    // =====================================================================
    bool resourceDestroySucceeded = false;

    if( _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        const uint32 outstanding = _outstandingIo.load(std::memory_order_acquire);
        if( outstanding != 0 )
        {
            finalResult = ShutdownResult::DrainTimeout;
        }
        else
        {
            // -------------------------------------------------------------
            // 여기까지 도달했다는 것은:
            //
            // 1. Worker가 join 완료
            // 2. dispatchGate exclusive
            // 3. cqConsumerMutex exclusive
            // 4. outstandingIo == 0
            //
            // 따라서 더 이상 CQ consumer가 존재하지 않는다.
            // Faulted / CorruptCq 상태라도 resource destruction은 허용한다.
            // -------------------------------------------------------------

            // -------------------------------------------------------------
            // RIO CQ destruction
            // -------------------------------------------------------------
            if( _cq != RIO_INVALID_CQ )
            {
                _rioTable.RIOCloseCompletionQueue(_cq);
                _cq = RIO_INVALID_CQ;
            }

            // -------------------------------------------------------------
            // IOCP destruction
            // -------------------------------------------------------------
            if( _iocpHandle != NULL )
            {
                ::CloseHandle(_iocpHandle);
                _iocpHandle = NULL;
            }

            // -------------------------------------------------------------
            // Runtime binding reset
            // -------------------------------------------------------------
            _eventPool = nullptr;
            _cqIdentifier = 0;

            _outstandingIo.store(0, std::memory_order_relaxed);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);

            // -------------------------------------------------------------
            // RIO configuration reset
            // -------------------------------------------------------------
            ZeroMemory(&_rioTable, sizeof(_rioTable));
            ZeroMemory(&_rioNotification, sizeof(_rioNotification));
            ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));

            resourceDestroySucceeded = true;
        }
    }

    // =====================================================================
    // Phase 7. Dispatch Domain 종료
    // =====================================================================
    dispatchGateLock.unlock();

    // =====================================================================
    // Phase 8. Shutdown 결과 및 Lifecycle State Commit
    //
    // IMPORTANT:
    //   dispatchGate는 이미 해제되었다.
    //   따라서 lifecycleMutex만 획득한다.
    //   이 단계에서는 resource destruction을 수행하지 않는다.
    // =====================================================================
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( resourceDestroySucceeded )
        {
            // -------------------------------------------------------------
            // Resource destruction이 완료되었다면
            // ShutdownResult가 Success가 아니더라도 lifecycle은 종료된다.
            //
            // 예:
            //
            //   CorruptCq + outstandingIo == 0
            //       -> CQ/IOCP destroy
            //       -> State::Closed
            //       -> ShutdownResult::CorruptCq
            //
            //   DispatchError + outstandingIo == 0
            //       -> CQ/IOCP destroy
            //       -> State::Closed
            //       -> ShutdownResult::DispatchError
            // -------------------------------------------------------------

            _state.store(State::Closed, std::memory_order_release);
            _shutdownDone = true;
        }
        else
        {
            // -------------------------------------------------------------
            // Resource destruction이 완료되지 않은 경우
            // 객체는 아직 최종 종료되지 않았다.
            // -------------------------------------------------------------

            if( finalResult == ShutdownResult::CorruptCq )
            {
                _state.store(State::Faulted, std::memory_order_release);
            }
            else
            {
                _state.store(State::Stopping, std::memory_order_release);
            }

            _shutdownDone = false;
        }

        _shutdownInProgress = false;
        _lastShutdownResult = finalResult;
    }

    // =====================================================================
    // Phase 9. Shutdown 완료 대기자 Notification
    // =====================================================================
    _shutdownCv.notify_all();

    return finalResult;
}

//***************************************************************************
// @brief 개별 RIO 완료 결과를 파싱하고 객체 소유권 회수 및 풀 반환을 수행하는 함수
//***************************************************************************
void CRioCore::ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept
{
    // -------------------------------------------------------------
    // Completion 하나를 소비한 이상 outstandingIo는
    // 성공/실패와 관계없이 정확히 한 번 감소되어야 한다.
    // -------------------------------------------------------------
    struct OutstandingIoGuard
    {
        CRioCore* core;

        ~OutstandingIoGuard() noexcept
        {
            if( core != nullptr )
            {
                core->DecrementIoCount();
            }
        }
    };

    OutstandingIoGuard ioGuard{ this };

    // -------------------------------------------------------------
    // RequestContext 검증
    // -------------------------------------------------------------
    if( rioEvent == nullptr )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "Invalid RIO RequestContext");
        return;
    }

    CRioObjectRef rioObject;

    // -------------------------------------------------------------
    // Owner 회수
    // -------------------------------------------------------------
    try
    {
        rioObject = rioEvent->TakeOwner();
    }
    catch( ... )
    {
        // ---------------------------------------------------------
        // 중요:
        //
        // TakeOwner()가 예외를 던졌으므로 ownership 상태를
        // 신뢰할 수 없다.
        //
        // 따라서 EventPool::Free()를 수행하지 않는다.
        //
        // 그러나 completion 자체는 이미 소비되었으므로
        // outstandingIo는 반드시 감소되어야 한다.
        //
        // OutstandingIoGuard가 이를 보장한다.
        // ---------------------------------------------------------
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "CRioEvent::TakeOwner() threw an exception");

        return;
    }

    // -------------------------------------------------------------
    // Owner가 없는 completion
    // -------------------------------------------------------------
    if( rioObject == nullptr )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "RIO completion has no owner");

        // 여기서는 rioEvent의 ownership을 신뢰할 수 없는
        // 정책이라면 Free하지 않는다.
        return;
    }

    // -------------------------------------------------------------
    // CRioObject의 I/O count 감소
    // -------------------------------------------------------------
    try
    {
        rioObject->DecrementIoCount();
    }
    catch( ... )
    {
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "Exception in CRioObject::DecrementIoCount()");
    }

    // -------------------------------------------------------------
    // 사용자 Dispatch
    // -------------------------------------------------------------
    try
    {
        if( status == NO_ERROR )
        {
            rioObject->Dispatch(rioEvent, bytesTransferred, NO_ERROR);
        }
        else
        {
            rioObject->Dispatch(rioEvent, 0, status);
        }
    }
    catch( ... )
    {
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "Exception in CRioObject::Dispatch()");
    }

    // -------------------------------------------------------------
    // EventPool 반환
    //
    // 여기까지 정상적으로 TakeOwner()가 완료되었으므로
    // rioEvent ownership이 확정된 상태에서 반환한다.
    // -------------------------------------------------------------
    if( _eventPool != nullptr )
    {
        try
        {
            _eventPool->Free(rioEvent);
        }
        catch( ... )
        {
            _workerFaulted.store(true, std::memory_order_release);
            
            assert(false && "Exception in CRioEventPool::Free()");
        }
    }
    else
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);

        assert(false && "EventPool became null while completion was outstanding");
    }
}

//***************************************************************************
// @brief 미완료 I/O 카운트를 원자적으로 증가시키는 함수
// @return 증가 성공 시 true, 오버플로우 발생 시 false 반환
//***************************************************************************
bool CRioCore::IncrementIoCount() noexcept
{
    uint32 current = _outstandingIo.load(std::memory_order_relaxed);

    for( ;; )
    {
        if( current == UINT32_MAX )
        {
            assert(false && "Outstanding I/O counter overflow");
            return false;
        }

        if( _outstandingIo.compare_exchange_weak(current, current + 1, std::memory_order_relaxed, std::memory_order_relaxed) )
        {
            return true;
        }
    }
}

//***************************************************************************
// @brief 미완료 I/O 카운트를 원자적으로 감소시키는 함수
//***************************************************************************
void CRioCore::DecrementIoCount() noexcept
{
    uint32 current = _outstandingIo.load(std::memory_order_acquire);

    for( ;; )
    {
        if( current == 0 )
        {
            assert(false && "Outstanding I/O counter underflow");
            return;
        }

        if( _outstandingIo.compare_exchange_weak(current, current - 1, std::memory_order_release, std::memory_order_relaxed) )
        {
            break;
        }
    }
}