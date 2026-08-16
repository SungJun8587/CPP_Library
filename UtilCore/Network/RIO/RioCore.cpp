//***************************************************************************
// RioCore.cpp : implementation of the CRioCore class.
//
//***************************************************************************

#include "pch.h"
#include "RioCore.h"

#include <algorithm>
#include <limits>

thread_local CRioCore* CRioCore::_tlsDispatchCore = nullptr;

//***************************************************************************
// @brief 기본 생성자
//***************************************************************************
CRioCore::CRioCore()
{
    ZeroMemory(&_rioTable, sizeof(_rioTable));
    ZeroMemory(&_receiveOverlapped, sizeof(_receiveOverlapped));
    ZeroMemory(&_sendOverlapped, sizeof(_sendOverlapped));
    ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
    ZeroMemory(&_sendNotification, sizeof(_sendNotification));
}

//***************************************************************************
// @brief 소멸자
//***************************************************************************
CRioCore::~CRioCore()
{
    const Rio::ShutdownResult result = Shutdown();
    const Rio::State state = _state.load(std::memory_order_acquire);

    if( state != Rio::State::Closed )
    {
        assert(false && "CRioCore destroyed before successful shutdown");
        std::terminate();
    }

    (void)result;
}

//***************************************************************************
// @brief RIO Core Engine을 초기화합니다.
// @param socket RIO 함수 테이블 로드 및 바인딩을 위한 소켓
// @param maxCompletionResults Completion Queue에서 한 번에 처리할 최대 결과 수
// @param cqIdentifier CQ 구분 식별자 Tag
// @param eventPool I/O에 사용될 RIO Event Pool 객체 Pointer
// @return 초기화 성공 여부
//***************************************************************************
bool CRioCore::Initialize(
    SOCKET socket,
    ULONG maxCompletionResults,
    ULONG_PTR cqIdentifier,
    CRioEventPool* eventPool)
{
    if( eventPool == nullptr || socket == INVALID_SOCKET || cqIdentifier == 0 ) return false;
    if( (cqIdentifier & Rio::kCompletionTagMask) != 0 ) return false;
    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE ) return false;

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    if( _state.load(std::memory_order_acquire) != Rio::State::Uninitialized ) return false;

    _state.store(Rio::State::Initializing, std::memory_order_release);

    RIO_EXTENSION_FUNCTION_TABLE tempRioTable{};
    GUID guid = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;

    const int ioctlResult = ::WSAIoctl(
        socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid), &tempRioTable, sizeof(tempRioTable),
        &bytes, nullptr, nullptr);

    if( ioctlResult == SOCKET_ERROR )
    {
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

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

    ZeroMemory(&_receiveOverlapped, sizeof(_receiveOverlapped));
    ZeroMemory(&_sendOverlapped, sizeof(_sendOverlapped));
    ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
    ZeroMemory(&_sendNotification, sizeof(_sendNotification));

    _receiveNotification.Type = RIO_IOCP_COMPLETION;
    _receiveNotification.Iocp.IocpHandle = tempIocp;
    _receiveNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier | Rio::kReceiveCompletionTag);
    _receiveNotification.Iocp.Overlapped = &_receiveOverlapped;

    _sendNotification.Type = RIO_IOCP_COMPLETION;
    _sendNotification.Iocp.IocpHandle = tempIocp;
    _sendNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier | Rio::kSendCompletionTag);
    _sendNotification.Iocp.Overlapped = &_sendOverlapped;

    RIO_CQ tempReceiveCq = tempRioTable.RIOCreateCompletionQueue(maxCompletionResults, &_receiveNotification);
    if( tempReceiveCq == RIO_INVALID_CQ )
    {
        ::CloseHandle(tempIocp);
        ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
        ZeroMemory(&_sendNotification, sizeof(_sendNotification));
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

    RIO_CQ tempSendCq = tempRioTable.RIOCreateCompletionQueue(maxCompletionResults, &_sendNotification);
    if( tempSendCq == RIO_INVALID_CQ )
    {
        tempRioTable.RIOCloseCompletionQueue(tempReceiveCq);
        ::CloseHandle(tempIocp);
        ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
        ZeroMemory(&_sendNotification, sizeof(_sendNotification));
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

    _rioTable = tempRioTable;
    _receiveCq = tempReceiveCq;
    _sendCq = tempSendCq;
    _iocpHandle = tempIocp;
    _cqIdentifier = cqIdentifier;
    _eventPool = eventPool;

    _outstandingIo.store(0, std::memory_order_relaxed);
    _receiveCqCorrupted.store(false, std::memory_order_relaxed);
    _sendCqCorrupted.store(false, std::memory_order_relaxed);
    _workerFaulted.store(false, std::memory_order_relaxed);
    _workerRunning.store(false, std::memory_order_relaxed);
    _workerThreadId.store(std::thread::id{}, std::memory_order_relaxed);

    _shutdownInProgress = false;
    _shutdownDone = false;
    _lastShutdownResult = Rio::ShutdownResult::Success;

    _state.store(Rio::State::Initialized, std::memory_order_release);
    return true;
}

//***************************************************************************
// @brief 외부에서 RIO Engine 정지를 요청합니다.
//***************************************************************************
void CRioCore::RequestStop()
{
    if( _tlsDispatchCore == this )
    {
        assert(false && "RequestStop() must not be called from Dispatch callback");
        return;
    }

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);
    StopInternal();
}

//***************************************************************************
// @brief 내부 정지 로직을 수행합니다.
//***************************************************************************
void CRioCore::StopInternal()
{
    std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

    const Rio::State current = _state.load(std::memory_order_acquire);

    if( current == Rio::State::Running || current == Rio::State::Initialized )
    {
        _state.store(Rio::State::Stopping, std::memory_order_release);
    }
    else if( current == Rio::State::Stopping || current == Rio::State::Faulted )
    {
        // Already stopping/faulted.
    }
    else
    {
        return;
    }

    if( _iocpHandle != NULL && _workerRunning.load(std::memory_order_acquire) )
    {
        if( !::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr) )
        {
            MarkFaulted(false, false);
        }
    }
}

//***************************************************************************
// @brief 내부 예외/결함 상태를 설정합니다.
//***************************************************************************
void CRioCore::FaultInternal() noexcept
{
    MarkFaulted(false, false);
}

//***************************************************************************
// @brief Batch 단위로 완료 이벤트를 디스패치합니다.
// @param mode 디스패치 모드 (Wait/Drain)
// @return 처리된 완료 이벤트 개수 또는 에러 코드
//***************************************************************************
int32 CRioCore::DispatchBatch(Rio::DispatchMode mode)
{
    std::shared_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    CRioCore* previousDispatchCore = _tlsDispatchCore;
    _tlsDispatchCore = this;

    TlsDispatchGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

    return _DispatchBatchImpl(mode);
}

//***************************************************************************
// @brief Completion Queue에서 완료 결과를 꺼내옵니다.
// @param cq 타겟 Completion Queue Handle
// @param results 결과를 저장할 RIORESULT 배열 Pointer
// @param numResults [out] 디큐된 결과 개수
// @return 성공 여부
//***************************************************************************
bool CRioCore::DrainCompletionQueue(RIO_CQ cq, RIORESULT* results, ULONG& numResults) noexcept
{
    numResults = 0;

    if( cq == RIO_INVALID_CQ || results == nullptr || _rioTable.RIODequeueCompletion == nullptr ) return false;

    const ULONG resultCount = _rioTable.RIODequeueCompletion(cq, results, Rio::kBatchSize);
    if( resultCount == RIO_CORRUPT_CQ ) return false;

    numResults = resultCount;
    return true;
}

//***************************************************************************
// @brief Completion Queue의 알림을 요청합니다.
// @param cq 타겟 Completion Queue Handle
// @return 성공 여부
//***************************************************************************
bool CRioCore::NotifyCompletionQueue(RIO_CQ cq) noexcept
{
    if( cq == RIO_INVALID_CQ || _rioTable.RIONotify == nullptr ) return false;

    const int result = _rioTable.RIONotify(cq);
    return result == ERROR_SUCCESS || result == WSAEALREADY;
}

//***************************************************************************
// @brief DispatchBatch의 실제 내부 구현부입니다.
// @param mode 디스패치 모드
// @return 처리된 완료 이벤트 개수 또는 에러 코드
//
// @note
//      [수정: _cqConsumerMutex 범위 축소]
//      기존에는 "CQ Lock 획득 -> Dequeue -> DispatchResults()(=Dispatch 콜백까지 포함)"
//      전체가 하나의 lock scope 안에 있어, 무거운 사용자 Dispatch()가 실행되는 동안
//      다른 CQ(Receive/Send)를 소비하려는 스레드까지 차단되는 문제가 있었습니다.
//      이는 "무거운 Dispatch는 CQ consumer lock 밖에서 수행한다"는 기존 invariant와
//      맞지 않으므로, Drain(Dequeue)만 lock 안에서 수행하고 그 결과를 lock 밖에서
//      DispatchResults()로 넘기도록 모든 CQ 접근 지점(6곳)을 수정했습니다.
//***************************************************************************
int32 CRioCore::_DispatchBatchImpl(Rio::DispatchMode mode)
{
    if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) )
    {
        return Rio::kCorruptCq;
    }

    if( mode == Rio::DispatchMode::Wait )
    {
        const std::thread::id workerId = _workerThreadId.load(std::memory_order_acquire);
        if( workerId == std::thread::id{} || std::this_thread::get_id() != workerId )
        {
            assert(false && "DispatchBatch(Wait) must only be called from worker thread");
            return Rio::kInvalidCompletion;
        }
    }

    RIORESULT results[Rio::kBatchSize]{};

    // 1. Receive CQ Drain
    {
        ULONG numResults = 0;
        bool drainSucceeded = false;

        {
            std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
            drainSucceeded = DrainCompletionQueue(_receiveCq, results, numResults);
        }

        if( !drainSucceeded )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kCorruptCq;
        }

        // Dispatch는 CQ Consumer Lock을 해제한 이후 수행합니다.
        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Receive, results, numResults);
    }

    // 2. Send CQ Drain
    {
        ULONG numResults = 0;
        bool drainSucceeded = false;

        {
            std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
            drainSucceeded = DrainCompletionQueue(_sendCq, results, numResults);
        }

        if( !drainSucceeded )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kCorruptCq;
        }

        // Dispatch는 CQ Consumer Lock을 해제한 이후 수행합니다.
        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    Rio::State state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    if( mode == Rio::DispatchMode::Drain ) return 0;

    // 3. Receive CQ Notify
    {
        ULONG numResults = 0;
        bool notifySucceeded = false;
        bool drainSucceeded = false;

        {
            std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

            notifySucceeded = NotifyCompletionQueue(_receiveCq);

            if( notifySucceeded ) drainSucceeded = DrainCompletionQueue(_receiveCq, results, numResults);
        }

        if( !notifySucceeded )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kNotifyError;
        }

        if( !drainSucceeded )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kCorruptCq;
        }

        // Dispatch는 CQ Consumer Lock을 해제한 이후 수행합니다.
        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Receive, results, numResults);
    }

    // 4. Send CQ Notify
    {
        ULONG numResults = 0;
        bool notifySucceeded = false;
        bool drainSucceeded = false;

        {
            std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

            notifySucceeded = NotifyCompletionQueue(_sendCq);

            if( notifySucceeded ) drainSucceeded = DrainCompletionQueue(_sendCq, results, numResults);
        }

        if( !notifySucceeded )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kNotifyError;
        }

        if( !drainSucceeded )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kCorruptCq;
        }

        // Dispatch는 CQ Consumer Lock을 해제한 이후 수행합니다.
        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    // 5. IOCP Wait
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    const BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        // [수정: Faulted 전이 누락] GQCS 실패 시에도 다른 에러 경로와 동일하게
        // MarkFaulted()로 워커 결함 상태를 통일합니다.
        MarkFaulted(false, false);

        if( overlapped != nullptr )
        {
            assert(false && "GetQueuedCompletionStatus failed with completion packet");
            return Rio::kIocpError;
        }

        assert(false && "GetQueuedCompletionStatus failed without completion packet");
        return Rio::kIocpError;
    }

    // Stop packet
    if( IsStopPacket(completionKey, overlapped) )
    {
        state = _state.load(std::memory_order_acquire);

        if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
        {
            if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
            return Rio::kStopped;
        }

        return 0;
    }

    // Invalid completion packet
    if( !IsValidCompletionPacket(completionKey, overlapped) )
    {
        // [수정: Faulted 전이 누락] 잘못된 completion packet도 다른 corrupt 경로와
        // 동일하게 MarkFaulted()로 통일합니다.
        MarkFaulted(false, false);

        assert(false && "Unexpected IOCP completion packet");
        return Rio::kInvalidCompletion;
    }

    RIO_CQ targetCq = RIO_INVALID_CQ;
    Rio::RioCqType targetCqType = Rio::RioCqType::Receive;

    if( IsReceiveCompletionPacket(completionKey, overlapped) )
    {
        targetCq = _receiveCq;
        targetCqType = Rio::RioCqType::Receive;
    }
    else if( IsSendCompletionPacket(completionKey, overlapped) )
    {
        targetCq = _sendCq;
        targetCqType = Rio::RioCqType::Send;
    }
    else
    {
        // [수정: Faulted 전이 누락]
        MarkFaulted(false, false);

        assert(false && "Invalid CQ completion packet");
        return Rio::kInvalidCompletion;
    }

    // 6. Target CQ Drain
    {
        ULONG numResults = 0;
        bool drainSucceeded = false;

        {
            std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);
            drainSucceeded = DrainCompletionQueue(targetCq, results, numResults);
        }

        if( !drainSucceeded )
        {
            if( targetCqType == Rio::RioCqType::Receive )
            {
                _receiveCqCorrupted.store(true, std::memory_order_release);
                MarkFaulted(true, false);
            }
            else
            {
                _sendCqCorrupted.store(true, std::memory_order_release);
                MarkFaulted(false, true);
            }

            return Rio::kCorruptCq;
        }

        if( numResults == 0 ) return 0;

        // Dispatch는 CQ Consumer Lock을 해제한 이후 수행합니다.
        return DispatchResults(targetCqType, results, numResults);
    }
}

//***************************************************************************
// @brief 꺼내온 RIO 결과를 순회하며 각 이벤트를 처리(Dispatch)합니다.
// @param cqType CQ 타입 (Receive/Send)
// @param results RIORESULT 배열
// @param numResults 결과 개수
// @return 성공적으로 처리된 개수 또는 에러 코드
//***************************************************************************
int32 CRioCore::DispatchResults(Rio::RioCqType cqType, RIORESULT* results, ULONG numResults) noexcept
{
    if( results == nullptr || numResults == 0 ) return 0;

    for( ULONG i = 0; i < numResults; ++i )
    {
        if( results[i].RequestContext == 0 )
        {
            if( cqType == Rio::RioCqType::Receive )
            {
                _receiveCqCorrupted.store(true, std::memory_order_release);
                MarkFaulted(true, false);
            }
            else
            {
                _sendCqCorrupted.store(true, std::memory_order_release);
                MarkFaulted(false, true);
            }

            assert(false && "RIO completion contains null RequestContext");
            return Rio::kCorruptCq;
        }

        CRioEvent* rioEvent = reinterpret_cast<CRioEvent*>(static_cast<ULONG_PTR>(results[i].RequestContext));

        ProcessRioResult(cqType, results[i].Status, results[i].BytesTransferred, rioEvent);

        if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) )
        {
            return Rio::kCorruptCq;
        }
    }

    return static_cast<int32>(numResults);
}

//***************************************************************************
// @brief 올바른 Completion 패킷인지 검증합니다.
// @param completionKey Completion Key
// @param overlapped Overlapped 구조체 Pointer
// @return 유효성 여부
//***************************************************************************
bool CRioCore::IsValidCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return IsReceiveCompletionPacket(completionKey, overlapped) || IsSendCompletionPacket(completionKey, overlapped);
}

//***************************************************************************
// @brief Stop 요청 패킷인지 확인합니다.
// @param completionKey Completion Key
// @param overlapped Overlapped 구조체 Pointer
// @return Stop 패킷 여부
//***************************************************************************
bool CRioCore::IsStopPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == 0 && overlapped == nullptr;
}

//***************************************************************************
// @brief Receive 완료 패킷인지 확인합니다.
// @param completionKey Completion Key
// @param overlapped Overlapped 구조체 Pointer
// @return Receive 패킷 여부
//***************************************************************************
bool CRioCore::IsReceiveCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == (_cqIdentifier | Rio::kReceiveCompletionTag) && overlapped == &_receiveOverlapped;
}

//***************************************************************************
// @brief Send 완료 패킷인지 확인합니다.
// @param completionKey Completion Key
// @param overlapped Overlapped 구조체 Pointer
// @return Send 패킷 여부
//***************************************************************************
bool CRioCore::IsSendCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == (_cqIdentifier | Rio::kSendCompletionTag) && overlapped == &_sendOverlapped;
}

//***************************************************************************
// @brief RIO Core Engine을 안전하게 종료하고 리소스를 해제합니다.
// @param drainTimeout 잔여 I/O 처리 대기 제한시간
// @return Shutdown 수행 결과
//***************************************************************************
Rio::ShutdownResult CRioCore::Shutdown(std::chrono::milliseconds drainTimeout)
{
    if( _tlsDispatchCore == this )
    {
        assert(false && "Shutdown() must not be called from Dispatch callback");
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
        _lastShutdownResult = Rio::ShutdownResult::InvalidCall;
        return _lastShutdownResult;
    }

    if( drainTimeout < std::chrono::milliseconds::zero() ) drainTimeout = std::chrono::milliseconds::zero();

    std::thread threadToJoin;

    // Phase 1. Admission Close
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        const Rio::State currentState = _state.load(std::memory_order_acquire);

        if( currentState == Rio::State::Closed || _shutdownDone ) return _lastShutdownResult;

        if( _shutdownInProgress )
        {
            _shutdownCv.wait(lifecycleLock, [this] { return _shutdownDone || !_shutdownInProgress; });
            return _lastShutdownResult;
        }

        if( currentState == Rio::State::Uninitialized )
        {
            // [수정: Uninitialized -> Closed 누락]
            // Initialize()를 한 번도 하지 않은 상태에서 Shutdown()이 호출되면
            // state를 Closed로 전이하지 않고 그대로 반환하고 있었습니다.
            // 소멸자는 최종 state가 Closed가 아니면 std::terminate()하므로,
            // Initialize() 없이 생성만 하고 소멸시키는 것만으로 프로세스가
            // 죽는 문제가 있었습니다. 여기서 명시적으로 Closed로 전이합니다.
            _state.store(Rio::State::Closed, std::memory_order_release);
            _lastShutdownResult = Rio::ShutdownResult::Success;
            _shutdownDone = true;
            return _lastShutdownResult;
        }

        if( _workerThread.joinable() && _workerThread.get_id() == std::this_thread::get_id() )
        {
            assert(false && "Shutdown() self-join");
            _lastShutdownResult = Rio::ShutdownResult::InvalidCall;
            return _lastShutdownResult;
        }

        _shutdownInProgress = true;
        StopInternal();

        if( _workerThread.joinable() ) threadToJoin = std::move(_workerThread);
    }

    // Phase 2. Dispatch Domain Exclusive
    std::unique_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    // Phase 3. Worker Join
    if( threadToJoin.joinable() ) threadToJoin.join();

    _workerRunning.store(false, std::memory_order_release);
    _workerThreadId.store(std::thread::id{}, std::memory_order_release);

    // Phase 4. CQ Drain
    Rio::ShutdownResult finalResult = Rio::ShutdownResult::Success;

    {
        const auto deadline = std::chrono::steady_clock::now() + drainTimeout;
        constexpr auto kDrainPollInterval = std::chrono::milliseconds(1);

        CRioCore* previousDispatchCore = _tlsDispatchCore;
        _tlsDispatchCore = this;

        TlsDispatchGuard tlsGuard{ _tlsDispatchCore, previousDispatchCore };

        while( _outstandingIo.load(std::memory_order_acquire) > 0 )
        {
            if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) )
            {
                finalResult = Rio::ShutdownResult::CorruptCq;
                break;
            }

            if( std::chrono::steady_clock::now() >= deadline )
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

            if( _outstandingIo.load(std::memory_order_acquire) == 0 ) break;

            const auto now = std::chrono::steady_clock::now();
            if( now >= deadline )
            {
                finalResult = Rio::ShutdownResult::DrainTimeout;
                break;
            }

            const auto remaining = deadline - now;
            const auto sleepDuration = (std::min)(remaining, std::chrono::duration_cast<std::chrono::steady_clock::duration>(kDrainPollInterval));

            if( sleepDuration > std::chrono::steady_clock::duration::zero() )
            {
                std::this_thread::sleep_for(sleepDuration);
            }
        }
    }

    // Phase 5. Drain Validation
    if( finalResult == Rio::ShutdownResult::Success )
    {
        if( _outstandingIo.load(std::memory_order_acquire) != 0 ) finalResult = Rio::ShutdownResult::DrainTimeout;
        else if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) ) finalResult = Rio::ShutdownResult::CorruptCq;
        else if( _workerFaulted.load(std::memory_order_acquire) ) finalResult = Rio::ShutdownResult::DispatchError;
    }

    // Phase 6. Resource Destruction
    bool resourceDestroySucceeded = false;

    if( _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( _outstandingIo.load(std::memory_order_acquire) != 0 )
        {
            finalResult = Rio::ShutdownResult::DrainTimeout;
        }
        else
        {
            if( _receiveCq != RIO_INVALID_CQ )
            {
                if( _rioTable.RIOCloseCompletionQueue != nullptr ) _rioTable.RIOCloseCompletionQueue(_receiveCq);
                _receiveCq = RIO_INVALID_CQ;
            }

            if( _sendCq != RIO_INVALID_CQ )
            {
                if( _rioTable.RIOCloseCompletionQueue != nullptr ) _rioTable.RIOCloseCompletionQueue(_sendCq);
                _sendCq = RIO_INVALID_CQ;
            }

            if( _iocpHandle != NULL )
            {
                ::CloseHandle(_iocpHandle);
                _iocpHandle = NULL;
            }

            _eventPool = nullptr;
            _cqIdentifier = 0;

            _outstandingIo.store(0, std::memory_order_relaxed);
            _workerRunning.store(false, std::memory_order_release);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);

            ZeroMemory(&_rioTable, sizeof(_rioTable));
            ZeroMemory(&_receiveOverlapped, sizeof(_receiveOverlapped));
            ZeroMemory(&_sendOverlapped, sizeof(_sendOverlapped));
            ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
            ZeroMemory(&_sendNotification, sizeof(_sendNotification));

            resourceDestroySucceeded = true;
        }
    }

    // Phase 7. Dispatch Domain Release
    dispatchGateLock.unlock();

    // Phase 8. Lifecycle Commit
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( resourceDestroySucceeded )
        {
            _state.store(Rio::State::Closed, std::memory_order_release);
            _shutdownDone = true;
        }
        else
        {
            if( finalResult == Rio::ShutdownResult::CorruptCq ) _state.store(Rio::State::Faulted, std::memory_order_release);
            else _state.store(Rio::State::Stopping, std::memory_order_release);
            _shutdownDone = false;
        }

        _shutdownInProgress = false;
        _lastShutdownResult = finalResult;
    }

    // Phase 9. Notify Waiters
    _shutdownCv.notify_all();

    return finalResult;
}

//***************************************************************************
// @brief 개별 RIO 결과를 처리하고 해당 이벤트와 리소스를 반환/디스패치합니다.
// @param cqType CQ 타입
// @param status RIO 완료 상태
// @param bytesTransferred 전송된 바이트 수
// @param rioEvent 디스패치할 RIO Event 객체 Pointer
//***************************************************************************
void CRioCore::ProcessRioResult(
    Rio::RioCqType cqType,
    LONG status,
    ULONG bytesTransferred,
    CRioEvent* rioEvent) noexcept
{
    OutstandingIoGuard ioGuard{ this };

    if( rioEvent == nullptr )
    {
        if( cqType == Rio::RioCqType::Receive )
            MarkFaulted(true, false);
        else
            MarkFaulted(false, true);

        assert(false && "Invalid RIO RequestContext");
        return;
    }

    CRioObjectRef rioObject = rioEvent->TakeOwner();

    if( rioObject == nullptr )
    {
        if( cqType == Rio::RioCqType::Receive )
            MarkFaulted(true, false);
        else
            MarkFaulted(false, true);

        assert(false && "RIO completion has no owner");
        return;
    }

    // 반드시 전체 completion cleanup 동안 Object lifetime 보호
    ObjectIoCountGuard objectIoGuard{ rioObject.get() };

    try
    {
        if( status == NO_ERROR )
            rioObject->Dispatch(rioEvent, bytesTransferred, NO_ERROR);
        else
            rioObject->Dispatch(rioEvent, 0, status);
    }
    catch( ... )
    {
        MarkFaulted(false, false);
        assert(false && "CRioObject::Dispatch() threw an exception");
    }

    const CVector<CRioEvent::BufferBinding>& bufferBindings =
        rioEvent->GetBufferBindings();

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

    if( bufferReleaseFailed )
    {
        if( cqType == Rio::RioCqType::Receive )
            MarkFaulted(true, false);
        else
            MarkFaulted(false, true);
    }

    if( _eventPool != nullptr )
    {
        _eventPool->Free(rioEvent);
    }
    else
    {
        if( cqType == Rio::RioCqType::Receive )
            MarkFaulted(true, false);
        else
            MarkFaulted(false, true);

        assert(false && "EventPool became null while completion was outstanding");
    }
}

//***************************************************************************
// @brief 진행 중인 I/O 카운터를 원자적으로 1 증가시킵니다.
// @return 증가 성공 여부
//***************************************************************************
bool CRioCore::IncrementIoCount() noexcept
{
    uint32_t current = _outstandingIo.load(std::memory_order_relaxed);

    for( ;; )
    {
        if( current == (std::numeric_limits<uint32_t>::max)() )
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
// @brief 진행 중인 I/O 카운터를 원자적으로 1 감소시킵니다.
//***************************************************************************
void CRioCore::DecrementIoCount() noexcept
{
    uint32_t current = _outstandingIo.load(std::memory_order_relaxed);

    for( ;; )
    {
        if( current == 0 )
        {
            assert(false && "Outstanding I/O counter underflow");
            return;
        }

        if( _outstandingIo.compare_exchange_weak(current, current - 1, std::memory_order_release, std::memory_order_relaxed) )
        {
            _shutdownCv.notify_all();
            return;
        }
    }
}

//***************************************************************************
// @brief Engine의 결함 상태 및 CQ Corrupt 상태를 설정합니다.
// @param receiveCqCorrupt Receive CQ 손상 여부
// @param sendCqCorrupt Send CQ 손상 여부
//***************************************************************************
void CRioCore::MarkFaulted(bool receiveCqCorrupt, bool sendCqCorrupt) noexcept
{
    _workerFaulted.store(true, std::memory_order_release);

    if( receiveCqCorrupt ) _receiveCqCorrupted.store(true, std::memory_order_release);
    if( sendCqCorrupt ) _sendCqCorrupted.store(true, std::memory_order_release);

    Rio::State current = _state.load(std::memory_order_acquire);

    while( current == Rio::State::Running || current == Rio::State::Stopping )
    {
        if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            break;
        }
    }

    HANDLE iocp = _iocpHandle;

    if( iocp != NULL )
    {
        ::PostQueuedCompletionStatus(iocp, 0, 0, nullptr);
    }

    _shutdownCv.notify_all();
}