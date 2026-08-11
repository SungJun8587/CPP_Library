
//**********************************************************************************************************************
// RioCore.cpp : implementation of the CRioCore class.
//
//**********************************************************************************************************************

#include "pch.h"
#include "RioCore.h"

#include <algorithm>

thread_local CRioCore* CRioCore::_tlsDispatchCore = nullptr;

//**********************************************************************************************************************
// @brief CRioCore 기본 생성자
// @details 멤버 변수들을 초기 상태 값으로 명시적 초기화합니다.
//**********************************************************************************************************************
CRioCore::CRioCore()
    : _cq(RIO_INVALID_CQ)
    , _iocpHandle(NULL)
    , _cqIdentifier(0)
    , _eventPool(nullptr)
    , _state(Rio::State::Uninitialized)
    , _workerRunning(false)
    , _workerFaulted(false)
    , _outstandingIo(0)
    , _cqCorrupted(false)
{
    ZeroMemory(&_rioTable, sizeof(_rioTable));
    ZeroMemory(&_rioNotification, sizeof(_rioNotification));
    ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));
}

//**********************************************************************************************************************
// @brief CRioCore 소멸자
// @details
//      - 소멸 전 Shutdown()을 명시적으로 호출하여 잔여 I/O 및 자원을 드레인합니다.
//      - 최종 State가 State::Closed가 아닌 경우 파괴 안전성 위반으로 단정문(assert)을 발생시키고 프로세스를 종료합니다.
//**********************************************************************************************************************
CRioCore::~CRioCore()
{
    const Rio::ShutdownResult result = Shutdown();

    const Rio::State state = _state.load(std::memory_order_acquire);

    if( state != Rio::State::Closed )
    {
        assert(false && "CRioCore destruction attempted before all RIO I/O drained");
        std::terminate();
    }

    (void)result;
}

//**********************************************************************************************************************
// @brief RIO 함수 테이블 로드 및 CQ/IOCP 오브젝트 초기화
// @param socket RIO 함수 테이블 로드용 대표 소켓
// @param maxCompletionResults RIO CQ 대기열 크기
// @param cqIdentifier IOCP 완료 키용 식별값
// @param eventPool 이벤트 객체 풀 포인터
// @return 초기화 성공 시 true, 실패 시 false
//
// @details
//      1. 유효성 검사 (eventPool, cqIdentifier, socket, maxCompletionResults)
//      2. 라이프사이클 뮤텍스를 확보한 후 중복 초기화 여부를 검증합니다.
//      3. WSAIoctl을 통해 Winsock RIO 8개 필수 확장 함수 포인터를 동적으로 로드합니다.
//      4. 통지 수신용 IOCP 핸들 및 RIO_CQ를 바인딩하여 생성합니다.
//**********************************************************************************************************************
bool CRioCore::Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool)
{
    if( eventPool == nullptr )
    {
        return false;
    }

    if( cqIdentifier == 0 )
    {
        return false;
    }

    if( socket == INVALID_SOCKET )
    {
        return false;
    }

    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE )
    {
        return false;
    }

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    if( _state.load(std::memory_order_acquire) != Rio::State::Uninitialized )
    {
        return false;
    }

    _state.store(Rio::State::Initializing, std::memory_order_release);

    RIO_EXTENSION_FUNCTION_TABLE tempRioTable{};
    GUID guid = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;

    int result = ::WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &tempRioTable, sizeof(tempRioTable), &bytes, nullptr, nullptr);

    if( result == SOCKET_ERROR )
    {
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

    // RIO 필수 8개 확장 함수 포인터 검증
    if( tempRioTable.RIOCreateCompletionQueue == nullptr ||
        tempRioTable.RIOCloseCompletionQueue == nullptr ||
        tempRioTable.RIODequeueCompletion == nullptr ||
        tempRioTable.RIONotify == nullptr ||
        tempRioTable.RIOSend == nullptr ||
        tempRioTable.RIOSendEx == nullptr ||
        tempRioTable.RIOReceive == nullptr ||
        tempRioTable.RIOReceiveEx == nullptr )
    {
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

    HANDLE tempIocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);

    if( tempIocp == nullptr )
    {
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

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

        _state.store(Rio::State::Uninitialized, std::memory_order_release);

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
    _lastShutdownResult = Rio::ShutdownResult::Success;

    _state.store(Rio::State::Initialized, std::memory_order_release);

    return true;
}

//**********************************************************************************************************************
// @brief 외부에서 명시적인 정지를 호출할 때 사용하는 인터페이스 함수
//**********************************************************************************************************************
void CRioCore::RequestStop()
{
    //******************************************************************************************************************
    // @brief Dispatch callback 내부에서는 RequestStop()을 호출할 수 없습니다.
    //
    // Dispatch callback은 현재 CRioCore의 dispatch domain 내부에서 실행되며,
    // RequestStop() -> StopInternal()은 submission/lifecycle domain에 진입합니다.
    //
    // 따라서 callback 내부에서 RequestStop()을 호출하면 lock domain 전환 및
    // lifecycle state 변경으로 인한 재진입/교착 가능성을 차단하기 위해
    // 명시적으로 거부합니다.
    //******************************************************************************************************************
    if( _tlsDispatchCore == this )
    {
        assert(false && "CRioCore::RequestStop() must not be called from Dispatch callback");
        return;
    }

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);
    StopInternal();
}

//**********************************************************************************************************************
// @brief 내부 상태를 Stopping으로 변경하고 대기 중인 IOCP 워커를 깨우는 함수
//**********************************************************************************************************************
void CRioCore::StopInternal()
{
    std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

    const Rio::State current = _state.load(std::memory_order_acquire);

    if( current == Rio::State::Running || current == Rio::State::Initialized )
    {
        _state.store(Rio::State::Stopping, std::memory_order_release);
    }
    else if( current != Rio::State::Stopping && current != Rio::State::Faulted )
    {
        return;
    }

    if( _iocpHandle != NULL && _workerRunning.load(std::memory_order_acquire) )
    {
        if( !::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr) )
        {
            MarkFaulted(false);
        }
    }
}

//**********************************************************************************************************************
// @brief 워커 스레드 내부 예외 발생 시 결함(Fault) 상태로 전환
//**********************************************************************************************************************
void CRioCore::FaultInternal() noexcept
{
    MarkFaulted(false);
}

//**********************************************************************************************************************
// @brief 완료 큐 배치 처리를 수행하는 퍼블릭 진입점 함수
// @param mode 디스패치 모드 (Wait/Drain)
// @return 처리된 이벤트 수 또는 에러 코드
//**********************************************************************************************************************
int32 CRioCore::DispatchBatch(Rio::DispatchMode mode)
{
    std::shared_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    CRioCore* previousDispatchCore = _tlsDispatchCore;
    _tlsDispatchCore = this;

    TlsDispatchGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

    return _DispatchBatchImpl(mode);
}

//**********************************************************************************************************************
// @brief 실제 이벤트 수거, 알림 등록, IOCP 대기를 수행하는 핵심 로직
// @param mode 디스패치 모드 (Wait / Drain)
// @return 수거/처리된 이벤트 카운트 또는 오류 상태 상수 (kCorruptCq, kStopped 등)
//**********************************************************************************************************************
int32 CRioCore::_DispatchBatchImpl(Rio::DispatchMode mode)
{
    if( _cqCorrupted.load(std::memory_order_acquire) )
    {
        return Rio::kCorruptCq;
    }

    if( mode == Rio::DispatchMode::Wait )
    {
        std::thread::id workerId = _workerThreadId.load(std::memory_order_acquire);

        if( workerId == std::thread::id{} || std::this_thread::get_id() != workerId )
        {
            assert(false && "DispatchBatch(Wait) must only be called from worker thread");
            return Rio::kInvalidCompletion;
        }
    }

    RIORESULT results[Rio::kBatchSize]{};
    ULONG numResults = 0;

    //******************************************************************************************************************
    // 1단계:
    // RIONotify 없이 이미 완료되어 대기 중인 이벤트 우선 수거
    //******************************************************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( _cq == RIO_INVALID_CQ )
        {
            return Rio::kInvalidCompletion;
        }

        numResults = _rioTable.RIODequeueCompletion(_cq, results, Rio::kBatchSize);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);
        return Rio::kCorruptCq;
    }

    if( numResults > 0 )
    {
        const int32 dispatchResult = DispatchResults(results, numResults);

        if( dispatchResult == Rio::kCorruptCq )
        {
            return Rio::kCorruptCq;
        }

        return dispatchResult;
    }

    Rio::State s = _state.load(std::memory_order_acquire);

    if( (s == Rio::State::Stopping || s == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == Rio::State::Stopping )
        {
            TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        }

        return Rio::kStopped;
    }

    if( mode == Rio::DispatchMode::Drain )
    {
        return 0;
    }

    //******************************************************************************************************************
    // 2단계:
    // RIONotify를 등록하여 다음 완료 이벤트를 통지받도록 설정 후 재확인
    //******************************************************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( _cq == RIO_INVALID_CQ )
        {
            return Rio::kInvalidCompletion;
        }

        int notifyResult = _rioTable.RIONotify(_cq);

        if( notifyResult != ERROR_SUCCESS && notifyResult != WSAEALREADY )
        {
            assert(false && "RIONotify failed");
            return Rio::kNotifyError;
        }

        numResults = _rioTable.RIODequeueCompletion(_cq, results, Rio::kBatchSize);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);
        return Rio::kCorruptCq;
    }

    if( numResults > 0 )
    {
        const int32 dispatchResult = DispatchResults(results, numResults);

        if( dispatchResult == Rio::kCorruptCq )
        {
            return Rio::kCorruptCq;
        }

        return dispatchResult;
    }

    s = _state.load(std::memory_order_acquire);

    if( (s == Rio::State::Stopping || s == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == Rio::State::Stopping )
        {
            TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        }

        return Rio::kStopped;
    }

    //******************************************************************************************************************
    // 3단계:
    // 이벤트가 없다면 IOCP 대기 상태로 전환
    //******************************************************************************************************************
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        if( overlapped != nullptr )
        {
            assert(false && "GetQueuedCompletionStatus returned failed completion packet");
            return Rio::kIocpError;
        }

        assert(false && "GetQueuedCompletionStatus failed without completion packet");
        return Rio::kIocpError;
    }

    if( completionKey == 0 && overlapped == nullptr )
    {
        s = _state.load(std::memory_order_acquire);

        if( (s == Rio::State::Stopping || s == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
        {
            if( s == Rio::State::Stopping )
            {
                TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
            }

            return Rio::kStopped;
        }

        return 0;
    }

    if( completionKey != _cqIdentifier || overlapped != &_rioOverlapped )
    {
        assert(false && "Unexpected IOCP completion packet");
        return Rio::kInvalidCompletion;
    }

    //******************************************************************************************************************
    // 4단계:
    // IOCP 깨어남 수신 후 CQ에서 최종 완료 이벤트 일괄 수거
    //******************************************************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( _cq == RIO_INVALID_CQ )
        {
            return Rio::kInvalidCompletion;
        }

        numResults = _rioTable.RIODequeueCompletion(_cq, results, Rio::kBatchSize);
    }

    if( numResults == RIO_CORRUPT_CQ )
    {
        _cqCorrupted.store(true, std::memory_order_release);
        _workerFaulted.store(true, std::memory_order_release);
        return Rio::kCorruptCq;
    }

    if( numResults == 0 )
    {
        return 0;
    }

    const int32 dispatchResult = DispatchResults(results, numResults);

    if( dispatchResult == Rio::kCorruptCq )
    {
        return Rio::kCorruptCq;
    }

    return dispatchResult;
}

//**********************************************************************************************************************
// @brief 수거된 RIO 결과 배열을 순회하며 각 이벤트 처리를 위임
// @param results RIORESULT 배열 주소
// @param numResults 배열 내 항목 개수
//**********************************************************************************************************************
int32 CRioCore::DispatchResults(RIORESULT* results, ULONG numResults) noexcept
{
    if( results == nullptr || numResults == 0 )
    {
        return 0;
    }

    for( ULONG i = 0; i < numResults; ++i )
    {
        //******************************************************************************************************************
        // RequestContext == 0은 정상적인 RIO completion으로 간주하지 않는다.
        //
        // RequestContext는 CRioEvent*를 식별하기 위한 필수 completion context이므로
        // null이 발견되면 해당 CQ의 무결성을 신뢰할 수 없는 것으로 판단한다.
        //
        // 따라서:
        //
        //     RequestContext == 0
        //         -> CorruptCq
        //         -> WorkerFaulted
        //         -> 현재 DispatchBatch 중단
        //
        // 이 경우 어떤 CRioEvent가 해당 completion의 소유자인지 식별할 수 없으므로
        // _outstandingIo를 임의로 감소시키지 않는다.
        //
        // 따라서 Shutdown()에서는 해당 completion의 소유권을 확인할 수 없기 때문에
        // outstanding I/O가 0이 되지 않으면 DrainTimeout으로 종료되며,
        // 안전하지 않은 resource destruction은 수행하지 않는다.
        //
        // 정상 completion으로 복구하거나 무시해서는 안 된다.
        //******************************************************************************************************************
        if( results[i].RequestContext == 0 )
        {
            _cqCorrupted.store(true, std::memory_order_release);
            _workerFaulted.store(true, std::memory_order_release);

            assert(false && "RIO completion contains null RequestContext");
            return Rio::kCorruptCq;
        }

        CRioEvent* rioEvent = reinterpret_cast<CRioEvent*>(static_cast<ULONG_PTR>(results[i].RequestContext));

        ProcessRioResult(results[i].Status, results[i].BytesTransferred, rioEvent);

        if( _cqCorrupted.load(std::memory_order_acquire) )
        {
            return Rio::kCorruptCq;
        }
    }

    return static_cast<int32>(numResults);
}

//**********************************************************************************************************************
// @brief 코어 리소스를 해제하고 모든 미완료 I/O가 처리될 때까지 드레인
// @param drainTimeout 잔여 I/O 완수 대기 최대 시간
// @return ShutdownResult 결과 상태 코드
//**********************************************************************************************************************
Rio::ShutdownResult CRioCore::Shutdown(std::chrono::milliseconds drainTimeout)
{
    if( _tlsDispatchCore == this )
    {
        assert(false && "CRioCore::Shutdown() must not be called from Dispatch callback");

        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);

        _lastShutdownResult = Rio::ShutdownResult::InvalidCall;
        return _lastShutdownResult;
    }

    if( drainTimeout < std::chrono::milliseconds::zero() )
    {
        drainTimeout = std::chrono::milliseconds::zero();
    }

    std::thread threadToJoin;

    //******************************************************************************************************************
    // Phase 1.
    // Shutdown Admission 차단 및 Worker Stop 요청
    //******************************************************************************************************************
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        Rio::State currentState = _state.load(std::memory_order_acquire);

        if( currentState == Rio::State::Closed || _shutdownDone )
        {
            return _lastShutdownResult;
        }

        if( _shutdownInProgress )
        {
            _shutdownCv.wait(lifecycleLock, [this] { return _shutdownDone || !_shutdownInProgress; });
            return _lastShutdownResult;
        }

        if( currentState == Rio::State::Uninitialized )
        {
            _lastShutdownResult = Rio::ShutdownResult::Success;
            _shutdownDone = true;
            return _lastShutdownResult;
        }

        if( _workerThread.joinable() && _workerThread.get_id() == std::this_thread::get_id() )
        {
            assert(false && "CRioCore::Shutdown() self-join");

            _lastShutdownResult = Rio::ShutdownResult::InvalidCall;
            return _lastShutdownResult;
        }

        _shutdownInProgress = true;

        StopInternal();

        if( _workerThread.joinable() )
        {
            threadToJoin = std::move(_workerThread);
        }
    }

    //******************************************************************************************************************
    // Phase 2.
    // Dispatch Domain 독점 진입
    //******************************************************************************************************************
    std::unique_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    //******************************************************************************************************************
    // Phase 3.
    // Worker 종료 및 Join 완료
    //******************************************************************************************************************
    if( threadToJoin.joinable() )
    {
        threadToJoin.join();
    }

    _workerThreadId.store(std::thread::id{}, std::memory_order_release);

    //******************************************************************************************************************
    // Phase 4.
    // Outstanding I/O Drain 처리
    //******************************************************************************************************************
    Rio::ShutdownResult finalResult = Rio::ShutdownResult::Success;

    {
        const auto deadline = std::chrono::steady_clock::now() + drainTimeout;
        constexpr auto kDrainPollInterval = std::chrono::milliseconds(1);

        CRioCore* previousDispatchCore = _tlsDispatchCore;
        _tlsDispatchCore = this;

        TlsDispatchGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

        while( _outstandingIo.load(std::memory_order_acquire) > 0 )
        {
            if( _cqCorrupted.load(std::memory_order_acquire) )
            {
                finalResult = Rio::ShutdownResult::CorruptCq;
                break;
            }

            const auto now = std::chrono::steady_clock::now();

            if( now >= deadline )
            {
                finalResult = Rio::ShutdownResult::DrainTimeout;
                break;
            }

            const int32 result = _DispatchBatchImpl(Rio::DispatchMode::Drain);

            if( result == Rio::kCorruptCq )
            {
                finalResult = Rio::ShutdownResult::CorruptCq;
                break;
            }

            if( result == Rio::kNotifyError || result == Rio::kIocpError || result == Rio::kInvalidCompletion )
            {
                finalResult = Rio::ShutdownResult::DispatchError;
                break;
            }

            if( _outstandingIo.load(std::memory_order_acquire) > 0 )
            {
                const auto afterDispatch = std::chrono::steady_clock::now();

                if( afterDispatch >= deadline )
                {
                    finalResult = Rio::ShutdownResult::DrainTimeout;
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

    //******************************************************************************************************************
    // Phase 5.
    // Drain 및 Worker/CQ 상태 검증
    //******************************************************************************************************************
    if( finalResult == Rio::ShutdownResult::Success )
    {
        if( _outstandingIo.load(std::memory_order_acquire) != 0 )
        {
            finalResult = Rio::ShutdownResult::DrainTimeout;
        }
        else if( _cqCorrupted.load(std::memory_order_acquire) )
        {
            finalResult = Rio::ShutdownResult::CorruptCq;
        }
        else if( _workerFaulted.load(std::memory_order_acquire) )
        {
            finalResult = Rio::ShutdownResult::DispatchError;
        }
    }

    //******************************************************************************************************************
    // Phase 6.
    // RIO/IOCP Resource Destruction
    //
    // Resource destruction 조건:
    //
    //     Worker stopped
    //     + Dispatch exclusive
    //     + CQ consumer exclusive
    //     + outstandingIo == 0
    //
    // Faulted / CorruptCq / DispatchError 상태라도 outstandingIo == 0이면
    // completion ownership이 모두 회수된 것으로 판단하여 최종 resource destruction을 수행할 수 있다.
    //
    // 단, resource destruction 성공 여부와 ShutdownResult는 별개의 개념이다.
    // 즉, CQ corruption 또는 callback fault가 발생했더라도 자원을 안전하게 해제했다면
    // 최종 State는 Closed가 되며 ShutdownResult에는 원래의 fault 결과를 유지한다.
    //******************************************************************************************************************
    bool resourceDestroySucceeded = false;

    if( _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        const uint32 outstanding = _outstandingIo.load(std::memory_order_acquire);

        if( outstanding != 0 )
        {
            finalResult = Rio::ShutdownResult::DrainTimeout;
        }
        else
        {
            //**********************************************************************************************************
            // RIO CQ destruction
            //**********************************************************************************************************
            if( _cq != RIO_INVALID_CQ )
            {
                if( _rioTable.RIOCloseCompletionQueue != nullptr )
                {
                    _rioTable.RIOCloseCompletionQueue(_cq);
                }

                _cq = RIO_INVALID_CQ;
            }

            //**********************************************************************************************************
            // IOCP destruction
            //**********************************************************************************************************
            if( _iocpHandle != NULL )
            {
                ::CloseHandle(_iocpHandle);
                _iocpHandle = NULL;
            }

            //**********************************************************************************************************
            // Runtime binding reset
            //**********************************************************************************************************
            _eventPool = nullptr;
            _cqIdentifier = 0;

            _outstandingIo.store(0, std::memory_order_relaxed);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);

            ZeroMemory(&_rioTable, sizeof(_rioTable));
            ZeroMemory(&_rioNotification, sizeof(_rioNotification));
            ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));

            resourceDestroySucceeded = true;
        }
    }

    //******************************************************************************************************************
    // Phase 7.
    // Dispatch Domain 종료
    //******************************************************************************************************************
    dispatchGateLock.unlock();

    //******************************************************************************************************************
    // Phase 8.
    // Shutdown 결과 및 Lifecycle State Commit
    //
    // resourceDestroySucceeded == true이면 ShutdownResult가 Success가 아니더라도
    // 실제 RIO/IOCP resource는 모두 안전하게 파괴된 상태이므로 State는 Closed로 확정한다.
    //
    // 따라서:
    //
    //     State::Closed + ShutdownResult::CorruptCq
    //     State::Closed + ShutdownResult::DispatchError
    //
    // 조합이 정상적으로 발생할 수 있다.
    //
    // ShutdownResult는 "왜 종료되었는가"를 나타내고,
    // State는 "현재 resource lifecycle이 어디까지 완료되었는가"를 나타낸다.
    //******************************************************************************************************************
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( resourceDestroySucceeded )
        {
            _state.store(Rio::State::Closed, std::memory_order_release);
            _shutdownDone = true;
        }
        else
        {
            if( finalResult == Rio::ShutdownResult::CorruptCq )
            {
                _state.store(Rio::State::Faulted, std::memory_order_release);
            }
            else
            {
                _state.store(Rio::State::Stopping, std::memory_order_release);
            }

            _shutdownDone = false;
        }

        _shutdownInProgress = false;
        _lastShutdownResult = finalResult;
    }

    //******************************************************************************************************************
    // Phase 9.
    // Shutdown 완료 대기자 Notification
    //******************************************************************************************************************
    _shutdownCv.notify_all();

    return finalResult;
}

//**********************************************************************************************************************
// @brief 개별 RIO 완료 결과를 파싱하고 객체 소유권 회수 및 풀 반환
// @param status RIO 완료 처리 결과 코드
// @param bytesTransferred 완료된 데이터 전송 바이트 수
// @param rioEvent 디스패치할 완료 이벤트 개체
//**********************************************************************************************************************
void CRioCore::ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept
{
    OutstandingIoGuard ioGuard{ this };

    //******************************************************************************************************************
    // 1. RequestContext 검증
    //******************************************************************************************************************
    if( rioEvent == nullptr )
    {
        _workerFaulted.store(true, std::memory_order_release);
        _cqCorrupted.store(true, std::memory_order_release);
        assert(false && "Invalid RIO RequestContext");
        return;
    }

    //******************************************************************************************************************
    // 2. Buffer binding 참조
    //******************************************************************************************************************
    const CVector<CRioEvent::BufferBinding>& bufferBindings = rioEvent->GetBufferBindings();

    //******************************************************************************************************************
    // 3. Owner 회수
    //******************************************************************************************************************
    CRioObjectRef rioObject = rioEvent->TakeOwner();

    if( rioObject == nullptr )
    {
        _workerFaulted.store(true, std::memory_order_release);
        _cqCorrupted.store(true, std::memory_order_release);
        assert(false && "RIO completion has no owner");
        return;
    }

    //******************************************************************************************************************
    // 4. Application Dispatch
    //******************************************************************************************************************
    {
        ObjectIoCountGuard objectIoGuard{ rioObject.get() };

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

            Rio::State current = _state.load(std::memory_order_acquire);

            while( current == Rio::State::Running || current == Rio::State::Stopping )
            {
                if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
                {
                    break;
                }
            }

            assert(false && "CRioObject::Dispatch() threw an exception");
        }
    }

    //******************************************************************************************************************
    // 5. Buffer-slot 반환
    //******************************************************************************************************************
    bool bufferReleaseFailed = false;

    for( const CRioEvent::BufferBinding& binding : bufferBindings )
    {
        if( binding.buffer == nullptr )
        {
            bufferReleaseFailed = true;
            assert(false && "CRioEvent contains null CRioBuffer binding");
            continue;
        }

        if( binding.slotIndex == Rio::kInvalidSlotIndex )
        {
            bufferReleaseFailed = true;
            assert(false && "CRioEvent contains invalid Buffer slot index");
            continue;
        }

        if( !binding.buffer->FreeSlot(binding.slotIndex) )
        {
            bufferReleaseFailed = true;
            assert(false && "CRioBuffer::FreeSlot() failed during RIO completion");
        }
    }

    //******************************************************************************************************************
    // 6. Buffer release 실패 처리
    //******************************************************************************************************************
    if( bufferReleaseFailed )
    {
        _workerFaulted.store(true, std::memory_order_release);

        Rio::State current = _state.load(std::memory_order_acquire);

        while( current == Rio::State::Running || current == Rio::State::Stopping )
        {
            if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
            {
                break;
            }
        }
    }

    //******************************************************************************************************************
    // 7. EventPool 반환
    //******************************************************************************************************************
    if( _eventPool != nullptr )
    {
        try
        {
            _eventPool->Free(rioEvent);
        }
        catch( ... )
        {
            _workerFaulted.store(true, std::memory_order_release);

            Rio::State current = _state.load(std::memory_order_acquire);

            while( current == Rio::State::Running || current == Rio::State::Stopping )
            {
                if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
                {
                    break;
                }
            }

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

//**********************************************************************************************************************
// @brief 미완료 I/O 카운트를 원자적으로 증가
// @return 증가 성공 시 true, overflow 시 false
//**********************************************************************************************************************
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

//**********************************************************************************************************************
// @brief 미완료 I/O 카운트를 원자적으로 감소
//**********************************************************************************************************************
void CRioCore::DecrementIoCount() noexcept
{
    uint32 current = _outstandingIo.load(std::memory_order_relaxed);

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

//**********************************************************************************************************************
// @brief 워커 결함(WorkerFaulted) 상태로 설정하고 내부 라이프사이클 상태를 Faulted로 전환
// @param corruptCq CQ 오염(RIO_CORRUPT_CQ) 동시 동반 여부
//**********************************************************************************************************************
void CRioCore::MarkFaulted(bool corruptCq) noexcept
{
    _workerFaulted.store(true, std::memory_order_release);

    if( corruptCq )
    {
        _cqCorrupted.store(true, std::memory_order_release);
    }

    Rio::State current = _state.load(std::memory_order_acquire);

    while( current == Rio::State::Running || current == Rio::State::Stopping )
    {
        if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            break;
        }
    }

    if( _iocpHandle != NULL )
    {
        ::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
    }
}