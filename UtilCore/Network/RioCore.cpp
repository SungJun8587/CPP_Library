
//***************************************************************************
// RioCore.cpp: implementation of the CRioCore class.
//
//***************************************************************************

#include "pch.h"
#include "RioCore.h"

//***************************************************************************
// @brief CRioCore 생성자 구현
//***************************************************************************
CRioCore::CRioCore()
    : _cq(RIO_INVALID_CQ), _iocpHandle(NULL), _cqIdentifier(0), _eventPool(nullptr),
    _state(State::Uninitialized), _workerRunning(false), _workerFaulted(false), _outstandingIo(0)
{
    ZeroMemory(&_rioTable, sizeof(_rioTable));
    ZeroMemory(&_rioNotification, sizeof(_rioNotification));
    ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));
}

//***************************************************************************
// @brief CRioCore 소멸자 구현
//***************************************************************************
CRioCore::~CRioCore()
{
    ShutdownResult result = Shutdown();
    if( result != ShutdownResult::Success )
    {
        // 로깅 시스템이 있다면 여기에 에러 로그를 남기세요.
        // 예: LOG_ERROR("CRioCore shutdown failed with code: %d", (int)result);

        // 만약 디버그 환경에서 무조건 잡고 싶다면:
        assert(result == ShutdownResult::Success && "CRioCore failed to shut down cleanly");
    }
}

//***************************************************************************
// @brief Initialize 메서드 구현
// @param socket 함수 테이블 바인딩 및 RIO 초기화에 사용할 대표 소켓 핸들
// @param maxCompletionResults CQ가 수용할 수 있는 최대 완료 이벤트 결과 수
// @param cqIdentifier 디버깅 및 무결성 검증을 위한 고유 CQ 식별 키
// @param eventPool 완료된 이벤트를 반환할 대상 CRioEventPool 풀 포인터
// @return 초기화 성공 시 true, 실패 시 false 반환
//***************************************************************************
bool CRioCore::Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool)
{
    if( eventPool == nullptr )
        return false;

    if( cqIdentifier == 0 )
        return false;

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    State current = _state.load(std::memory_order_acquire);
    if( current != State::Uninitialized )
    {
        return false;
    }

    _state.store(State::Initializing, std::memory_order_release);

    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE || socket == INVALID_SOCKET )
    {
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    RIO_EXTENSION_FUNCTION_TABLE tempRioTable{};
    GUID guid = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;

    int result = ::WSAIoctl(
        socket,
        SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
        &guid,
        sizeof(guid),
        &tempRioTable,
        sizeof(tempRioTable),
        &bytes,
        NULL,
        NULL
    );

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

    HANDLE tempIocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if( tempIocp == NULL )
    {
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));

    RIO_NOTIFICATION_COMPLETION tempNotification{};
    tempNotification.Type = RIO_IOCP_COMPLETION;
    tempNotification.Iocp.IocpHandle = tempIocp;
    tempNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier);
    tempNotification.Iocp.Overlapped = &_rioOverlapped;

    RIO_CQ tempCq = tempRioTable.RIOCreateCompletionQueue(maxCompletionResults, &tempNotification);
    if( tempCq == RIO_INVALID_CQ )
    {
        ::CloseHandle(tempIocp);
        _state.store(State::Uninitialized, std::memory_order_release);
        return false;
    }

    _rioTable = tempRioTable;
    _rioNotification = tempNotification;
    _cq = tempCq;
    _iocpHandle = tempIocp;
    _cqIdentifier = cqIdentifier;
    _eventPool = eventPool;
    _outstandingIo.store(0, std::memory_order_relaxed);

    _state.store(State::Initialized, std::memory_order_release);
    return true;
}

//***************************************************************************
// @brief Stop 외부 공개 메서드 구현
//***************************************************************************
void CRioCore::RequestStop()
{
    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);
    StopInternal();
}

//***************************************************************************
// @brief 내부 전용 정지 처리 헬퍼 구현
//***************************************************************************
void CRioCore::StopInternal()
{
    std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

    State current = _state.load(std::memory_order_acquire);
    while( current == State::Running || current == State::Initialized )
    {
        if( _state.compare_exchange_weak(current, State::Stopping, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            if( _iocpHandle != NULL )
            {
                ::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
            }
            break;
        }
        current = _state.load(std::memory_order_acquire);
    }
}

//***************************************************************************
// @brief DispatchBatch 메서드 구현
// @param mode DispatchMode::Wait 또는 DispatchMode::Drain
// @return 처리된 이벤트 수 또는 상태 코드
//***************************************************************************
int32 CRioCore::DispatchBatch(DispatchMode mode)
{
    if( mode == DispatchMode::Wait )
    {
        std::thread::id workerId = _workerThreadId.load(std::memory_order_acquire);
        if( std::this_thread::get_id() != workerId )
        {
            assert(false && "DispatchBatch(Wait) must only be called from the worker thread");
            return kCorruptCq;
        }
    }

    std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

    if( _cq == RIO_INVALID_CQ )
        return kCorruptCq;

    RIORESULT results[BATCH_SIZE];

    ULONG numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);

    if( numResults == RIO_CORRUPT_CQ )
        return kCorruptCq;

    if( numResults > 0 )
    {
        DispatchResults(results, numResults);
        return static_cast<int32>(numResults);
    }

    State s = _state.load(std::memory_order_acquire);
    if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == State::Stopping )
        {
            TryTransitionState(State::Stopping, State::Stopped);
        }
        return kStopped;
    }

    if( mode == DispatchMode::Drain )
    {
        return 0;
    }

    int notifyResult = _rioTable.RIONotify(_cq);
    if( notifyResult != ERROR_SUCCESS && notifyResult != WSAEALREADY )
    {
        return kCorruptCq;
    }

    numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);
    if( numResults == RIO_CORRUPT_CQ )
        return kCorruptCq;

    if( numResults > 0 )
    {
        DispatchResults(results, numResults);
        return static_cast<int32>(numResults);
    }

    s = _state.load(std::memory_order_acquire);
    if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( s == State::Stopping )
        {
            TryTransitionState(State::Stopping, State::Stopped);
        }
        return kStopped;
    }

    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        DWORD err = ::GetLastError();
        if( err == ERROR_INVALID_HANDLE || err == ERROR_ABANDONED_WAIT_0 )
        {
            return kCorruptCq;
        }
        return kCorruptCq;
    }

    if( completionKey == 0 && overlapped == nullptr )
    {
        s = _state.load(std::memory_order_acquire);
        if( (s == State::Stopping || s == State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
        {
            if( s == State::Stopping )
            {
                TryTransitionState(State::Stopping, State::Stopped);
            }
            return kStopped;
        }
        return 0;
    }

    if( completionKey != _cqIdentifier || overlapped != &_rioOverlapped )
    {
        return kCorruptCq;
    }

    numResults = _rioTable.RIODequeueCompletion(_cq, results, BATCH_SIZE);
    if( numResults == RIO_CORRUPT_CQ )
        return kCorruptCq;

    if( numResults == 0 )
        return 0;

    DispatchResults(results, numResults);
    return static_cast<int32>(numResults);
}

//***************************************************************************
// @brief DispatchResults 메서드 구현
// @param results RIO 결과 배열 포인터
// @param numResults 수거된 결과 개수
//***************************************************************************
void CRioCore::DispatchResults(RIORESULT* results, ULONG numResults) noexcept
{
    for( ULONG i = 0; i < numResults; ++i )
    {
        CRioEvent* rioEvent = reinterpret_cast<CRioEvent*>(results[i].RequestContext);
        ProcessRioResult(
            static_cast<LONG>(results[i].Status),
            results[i].BytesTransferred,
            rioEvent);
    }
}

//***************************************************************************
// @brief Shutdown 메서드 최종 완결 버전 (Faulted 강제 해제 위험성 제거)
// @param drainTimeout Drain 루프의 최대 대기 시간 (밀리초)
// @return ShutdownResult 결과 반환 (Success, DrainTimeout, CorruptCq)
//***************************************************************************
ShutdownResult CRioCore::Shutdown(std::chrono::milliseconds drainTimeout)
{
    std::thread threadToJoin;

    // 1. 상태 전이 및 스레드 정리 (최소 범위 lifecycleMutex 락)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        State currentState = _state.load(std::memory_order_acquire);

        // 이미 완전히 닫힌 경우
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
            return _lastShutdownResult;
        }

        if( _workerThread.joinable() && _workerThread.get_id() == std::this_thread::get_id() )
        {
            assert(false && "CRioCore::Shutdown() must not be called from the worker thread itself (self-join)");
            _lastShutdownResult = ShutdownResult::CorruptCq;
            return _lastShutdownResult;
        }

        _shutdownInProgress = true;

        // Faulted 상태가 아닌 경우에만 정상적으로 StopInternal 수행
        if( currentState != State::Faulted )
        {
            StopInternal();
        }

        // 스레드가 존재한다면 이동 후 락 외부에서 조인 준비
        if( _workerThread.joinable() )
        {
            threadToJoin = std::move(_workerThread);
        }
    }

    // 워커 스레드 조인 (락 외부에서 수행하여 데드락 방지)
    if( threadToJoin.joinable() )
    {
        threadToJoin.join();
    }

    // 2. 드레인 단계 (락 없음 - 논블로킹으로 잔여 I/O 처리)
    ShutdownResult finalResult = ShutdownResult::Success;
    {
        const auto deadline = std::chrono::steady_clock::now() + drainTimeout;
        while( _outstandingIo.load(std::memory_order_acquire) > 0 )
        {
            if( std::chrono::steady_clock::now() >= deadline )
            {
                finalResult = ShutdownResult::DrainTimeout;
                break;
            }

            int32 res = DispatchBatch(DispatchMode::Drain);
            if( res == kCorruptCq )
            {
                finalResult = ShutdownResult::CorruptCq;
                break;
            }

            if( _outstandingIo.load(std::memory_order_acquire) > 0 )
            {
                std::this_thread::yield();
            }
        }
    }

    // 드레인 루프 탈출 후 잔여 I/O 재확인
    if( finalResult == ShutdownResult::Success )
    {
        const uint32 outstanding = _outstandingIo.load(std::memory_order_acquire);
        if( outstanding != 0 )
        {
            finalResult = ShutdownResult::DrainTimeout;
        }
    }

    // 3. 최종 자원 해제 및 상태 정리 (lifecycleMutex + cqLock 동시 보호)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        const uint32 outstanding = _outstandingIo.load(std::memory_order_acquire);

        // 오직 결과가 Success이고 잔여 I/O가 0일 때만 안전하게 자원을 해제합니다.
        // (Faulted 상태였더라도 드레인을 통해 outstanding이 0이 되었다면 정상 해제 가능)
        if( finalResult == ShutdownResult::Success && outstanding == 0 )
        {
            if( _cq != RIO_INVALID_CQ )
            {
                _rioTable.RIOCloseCompletionQueue(_cq);
                _cq = RIO_INVALID_CQ;
            }

            if( _iocpHandle != NULL )
            {
                ::CloseHandle(_iocpHandle);
                _iocpHandle = NULL;
            }

            _state.store(State::Closed, std::memory_order_release);

            _eventPool = nullptr;
            _cqIdentifier = 0;
            _outstandingIo.store(0, std::memory_order_relaxed);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);

            ZeroMemory(&_rioTable, sizeof(_rioTable));
            ZeroMemory(&_rioNotification, sizeof(_rioNotification));
            ZeroMemory(&_rioOverlapped, sizeof(_rioOverlapped));

            _shutdownDone = true;
            finalResult = ShutdownResult::Success;
        }
        else
        {
            // 드레인 타임아웃이거나, 잔여 I/O가 남아있거나, CQ가 손상된 경우
            if( finalResult == ShutdownResult::CorruptCq )
            {
                _state.store(State::Faulted, std::memory_order_release);
            }
            else
            {
                // DrainTimeout 또는 I/O가 남아있는 경우 Stopping 상태 유지하여 재시도 허용
                _state.store(State::Stopping, std::memory_order_release);
            }

            _shutdownDone = false;
        }

        _shutdownInProgress = false;
        _lastShutdownResult = finalResult;
    }

    _shutdownCv.notify_all();
    return finalResult;
}

//***************************************************************************
// @brief ProcessRioResult 메서드 구현
// @param status RIO 완료 상태 코드
// @param bytesTransferred 전송된 바이트 수
// @param rioEvent 완료된 RIO 이벤트 포인터
//***************************************************************************
void CRioCore::ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept
{
    if( rioEvent == nullptr )
    {
        assert(false && "Invalid RIO RequestContext (nullptr)");
        DecrementIoCount();
        return;
    }

    CRioObjectRef rioObject;
    try
    {
        rioObject = rioEvent->TakeOwner();
    }
    catch( ... )
    {
        assert(false && "Exception caught during rioEvent->TakeOwner()");
    }

    if( rioObject != nullptr )
    {
        try
        {
            rioObject->DecrementIoCount();
        }
        catch( ... )
        {
            assert(false && "Exception caught during rioObject->DecrementIoCount()");
        }

        try
        {
            if( status == NO_ERROR )
            {
                rioObject->Dispatch(rioEvent, static_cast<ULONG>(bytesTransferred), NO_ERROR);
            }
            else
            {
                rioObject->Dispatch(rioEvent, 0, status);
            }
        }
        catch( ... )
        {
            assert(false && "Exception caught during rioObject->Dispatch()");
        }
    }

    if( _eventPool != nullptr )
    {
        try
        {
            _eventPool->Free(rioEvent);
        }
        catch( ... )
        {
            assert(false && "Exception caught during _eventPool->Free()");
        }
    }

    DecrementIoCount();
}

//***************************************************************************
// @brief IncrementIoCount 메서드 구현
// @return 증가 성공 시 true, 오버플로우 발생 시 false
//***************************************************************************
bool CRioCore::IncrementIoCount() noexcept
{
    uint32 current = _outstandingIo.load(std::memory_order_acquire);
    for( ;;)
    {
        if( current == UINT32_MAX )
        {
            assert(false && "Outstanding I/O counter overflow");
            return false;
        }

        if( _outstandingIo.compare_exchange_weak(
            current,
            current + 1,
            std::memory_order_release,
            std::memory_order_relaxed) )
        {
            return true;
        }
    }
}

//***************************************************************************
// @brief DecrementIoCount 메서드 구현
//***************************************************************************
void CRioCore::DecrementIoCount() noexcept
{
    uint32 current = _outstandingIo.load(std::memory_order_acquire);
    for( ;;)
    {
        if( current == 0 )
        {
            assert(false && "Outstanding I/O counter underflow");
            return;
        }

        if( _outstandingIo.compare_exchange_weak(
            current,
            current - 1,
            std::memory_order_release,
            std::memory_order_relaxed) )
        {
            break;
        }
    }
}