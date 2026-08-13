
//***************************************************************************
// RioCore.cpp : implementation of the CRioCore class.
//
//***************************************************************************

#include "pch.h"
#include "RioCore.h"

#include <algorithm>
#include <limits>

thread_local CRioCore* CRioCore::_tlsDispatchCore = nullptr;    // 현재 스레드의 Dispatch Core TLS 포인터 정의 및 초기화

//***************************************************************************
// @brief CRioCore 기본 생성자
// @details
//      멤버 변수 및 RIO/IOCP 관련 구조체들을 ZeroMemory로 초기화합니다.
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
// @brief CRioCore 소멸자
// @details
//      객체 파기 전 명시적인 Shutdown()이 완료되었는지 검증하며,
//      Closed 상태가 아닐 경우 std::terminate()를 통해 프로세스를 보호합니다.
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
// @brief RIO 함수 테이블 및 Receive/Send CQ와 IOCP를 초기화합니다.
// @param socket RIO 함수 확장 로드에 사용할 소켓
// @param maxCompletionResults Completion Queue의 최대 결과 수
// @param cqIdentifier CQ 구분용 고유 태그
// @param eventPool 완료 후 반환할 EventPool 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioCore::Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool)
{
    if( eventPool == nullptr ) return false;
    if( socket == INVALID_SOCKET ) return false;
    if( cqIdentifier == 0 ) return false;
    if( (cqIdentifier & Rio::kCompletionTagMask) != 0 ) return false;
    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE ) return false;

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    if( _state.load(std::memory_order_acquire) != Rio::State::Uninitialized ) return false;

    _state.store(Rio::State::Initializing, std::memory_order_release);

    RIO_EXTENSION_FUNCTION_TABLE tempRioTable{};
    GUID guid = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;

    const int ioctlResult = ::WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &tempRioTable, sizeof(tempRioTable), &bytes, nullptr, nullptr);

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
// @brief 외부에서 명시적인 정지를 호출합니다.
// @details Dispatch 콜백 내부 호출 방지용 단성 검증 후 StopInternal을 수행합니다.
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
// @brief Admission을 차단하고 Worker 스레드를 깨웁니다.
//***************************************************************************
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
            MarkFaulted(false, false);
        }
    }
}

//***************************************************************************
// @brief Worker 내부 예외 발생 시 Fault 상태로 전환합니다.
//***************************************************************************
void CRioCore::FaultInternal() noexcept
{
    MarkFaulted(false, false);
}

//***************************************************************************
// @brief Dispatch 진입점 함수입니다.
// @param mode 디스패치 모드
// @return 처리 결과
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
// @brief CQ 하나에서 완료 결과를 non-blocking 방식으로 수거합니다.
// @param cq 수거할 RIO_CQ 핸들
// @param results 결과를 담을 RIORESULT 배열
// @param numResults [out] 수거된 결과 개수
// @return 성공 시 true, CQ 손상 등 에러 발생 시 false
//***************************************************************************
bool CRioCore::DrainCompletionQueue(RIO_CQ cq, RIORESULT* results, ULONG& numResults) noexcept
{
    numResults = 0;

    if( cq == RIO_INVALID_CQ || results == nullptr ) return false;
    if( _rioTable.RIODequeueCompletion == nullptr ) return false;

    const ULONG resultCount = _rioTable.RIODequeueCompletion(cq, results, Rio::kBatchSize);

    if( resultCount == RIO_CORRUPT_CQ ) return false;

    numResults = resultCount;
    return true;
}

//***************************************************************************
// @brief CQ notification을 다시 등록합니다.
// @param cq 등록할 RIO_CQ 핸들
// @return 성공 또는 WSAEALREADY 상태일 경우 true
//***************************************************************************
bool CRioCore::NotifyCompletionQueue(RIO_CQ cq) noexcept
{
    if( cq == RIO_INVALID_CQ ) return false;
    if( _rioTable.RIONotify == nullptr ) return false;

    const int result = _rioTable.RIONotify(cq);

    return result == ERROR_SUCCESS || result == WSAEALREADY;
}

//***************************************************************************
// @brief Receive/Send CQ 실제 Dispatch 내부 로직을 수행합니다.
// @param mode 디스패치 실행 모드
// @return 처리된 I/O 개수 또는 오류 코드
//***************************************************************************
int32 CRioCore::_DispatchBatchImpl(Rio::DispatchMode mode)
{
    if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) ) return Rio::kCorruptCq;

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

    //***************************************************************************
    // 1. Receive CQ Drain
    //***************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        ULONG numResults = 0;

        if( !DrainCompletionQueue(_receiveCq, results, numResults) )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kCorruptCq;
        }

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Receive, results, numResults);
    }

    //***************************************************************************
    // 2. Send CQ Drain
    //***************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        ULONG numResults = 0;

        if( !DrainCompletionQueue(_sendCq, results, numResults) )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kCorruptCq;
        }

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    Rio::State state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    if( mode == Rio::DispatchMode::Drain ) return 0;

    //***************************************************************************
    // 3. Receive CQ Notify
    //***************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( !NotifyCompletionQueue(_receiveCq) )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kNotifyError;
        }

        ULONG numResults = 0;

        if( !DrainCompletionQueue(_receiveCq, results, numResults) )
        {
            _receiveCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(true, false);
            return Rio::kCorruptCq;
        }

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Receive, results, numResults);
    }

    //***************************************************************************
    // 4. Send CQ Notify
    //***************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        if( !NotifyCompletionQueue(_sendCq) )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kNotifyError;
        }

        ULONG numResults = 0;

        if( !DrainCompletionQueue(_sendCq, results, numResults) )
        {
            _sendCqCorrupted.store(true, std::memory_order_release);
            MarkFaulted(false, true);
            return Rio::kCorruptCq;
        }

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    //***************************************************************************
    // 5. Shared IOCP Wait
    //***************************************************************************
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    const BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        if( overlapped != nullptr )
        {
            assert(false && "GetQueuedCompletionStatus failed with completion packet");
            return Rio::kIocpError;
        }

        assert(false && "GetQueuedCompletionStatus failed without completion packet");
        return Rio::kIocpError;
    }

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

    if( !IsValidCompletionPacket(completionKey, overlapped) )
    {
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
        assert(false && "Invalid CQ completion packet");
        return Rio::kInvalidCompletion;
    }

    //***************************************************************************
    // 6. Target CQ Drain
    //***************************************************************************
    {
        std::unique_lock<std::mutex> cqLock(_cqConsumerMutex);

        ULONG numResults = 0;

        if( !DrainCompletionQueue(targetCq, results, numResults) )
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

        return DispatchResults(targetCqType, results, numResults);
    }
}

//***************************************************************************
// @brief CQ별 Fault 격리가 반영된 DispatchResults
// @param cqType 처리 대상 CQ 열거형
// @param results Completion 이벤트 배열
// @param numResults 처리할 완료 건수
// @return int32 처리 완료 건수 또는 오류 코드
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
                MarkFaulted(true, false);
            }
            else
            {
                MarkFaulted(false, true);
            }

            assert(false && "RIO completion contains null RequestContext");
            return Rio::kCorruptCq;
        }

        CRioEvent* rioEvent = reinterpret_cast<CRioEvent*>(static_cast<ULONG_PTR>(results[i].RequestContext));

        ProcessRioResult(results[i].Status, results[i].BytesTransferred, rioEvent);

        if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) ) return Rio::kCorruptCq;
    }

    return static_cast<int32>(numResults);
}

//***************************************************************************
// @brief 수신된 IOCP 완료 패킷이 유효한 completion 패킷인지 검증합니다.
//***************************************************************************
bool CRioCore::IsValidCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return IsReceiveCompletionPacket(completionKey, overlapped) || IsSendCompletionPacket(completionKey, overlapped);
}

//***************************************************************************
// @brief Stop 요청용 IOCP 완료 패킷인지 확인합니다.
//***************************************************************************
bool CRioCore::IsStopPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == 0 && overlapped == nullptr;
}

//***************************************************************************
// @brief Receive CQ 전용 Completion Packet인지 검증합니다.
//***************************************************************************
bool CRioCore::IsReceiveCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == (_cqIdentifier | Rio::kReceiveCompletionTag) && overlapped == &_receiveOverlapped;
}

//***************************************************************************
// @brief Send CQ 전용 Completion Packet인지 검증합니다.
//***************************************************************************
bool CRioCore::IsSendCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept
{
    return completionKey == (_cqIdentifier | Rio::kSendCompletionTag) && overlapped == &_sendOverlapped;
}

//***************************************************************************
// @brief CRioCore를 종료하고 자원을 순차 파기합니다.
// @param drainTimeout Drain 대기 시간 limit
// @return ShutdownResult 처리 결과
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

    //***************************************************************************
    // Phase 1. Admission Close
    //***************************************************************************
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

    //***************************************************************************
    // Phase 2. Dispatch Domain Exclusive
    //***************************************************************************
    std::unique_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    //***************************************************************************
    // Phase 3. Worker Join
    //***************************************************************************
    if( threadToJoin.joinable() ) threadToJoin.join();

    _workerRunning.store(false, std::memory_order_release);
    _workerThreadId.store(std::thread::id{}, std::memory_order_release);

    //***************************************************************************
    // Phase 4. CQ Drain
    //***************************************************************************
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
            const auto sleepDuration = std::min(remaining, std::chrono::duration_cast<std::chrono::steady_clock::duration>(kDrainPollInterval));

            if( sleepDuration > std::chrono::steady_clock::duration::zero() ) std::this_thread::sleep_for(sleepDuration);
        }
    }

    //***************************************************************************
    // Phase 5. Drain Validation
    //***************************************************************************
    if( finalResult == Rio::ShutdownResult::Success )
    {
        if( _outstandingIo.load(std::memory_order_acquire) != 0 )
        {
            finalResult = Rio::ShutdownResult::DrainTimeout;
        }
        else if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) )
        {
            finalResult = Rio::ShutdownResult::CorruptCq;
        }
        else if( _workerFaulted.load(std::memory_order_acquire) )
        {
            finalResult = Rio::ShutdownResult::DispatchError;
        }
    }

    //***************************************************************************
    // Phase 6. Resource Destruction (Faulted 상태라도 _outstandingIo == 0일 때만 해제)
    //***************************************************************************
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

    //***************************************************************************
    // Phase 7. Dispatch Domain Release
    //***************************************************************************
    dispatchGateLock.unlock();

    //***************************************************************************
    // Phase 8. Lifecycle Commit
    //***************************************************************************
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

    //***************************************************************************
    // Phase 9. Notify Waiters
    //***************************************************************************
    _shutdownCv.notify_all();

    return finalResult;
}

//***************************************************************************
// @brief Completion 순서 엄격 고정:
//        TakeOwner() -> Dispatch() -> FreeSlot() -> EventPool::Free() -> CoreOutstandingIo--
// @param status completion 상태 코드
// @param bytesTransferred 전송 완료된 바이트 수
// @param rioEvent 디스패치할 RIO 이벤트 포인터
//***************************************************************************
void CRioCore::ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept
{
    // OutstandingIoGuard가 함수 종료 시 Core OutstandingIo--를 가장 마지막에 처리합니다.
    OutstandingIoGuard ioGuard{ this };

    if( rioEvent == nullptr )
    {
        MarkFaulted(true, true);
        assert(false && "Invalid RIO RequestContext");
        return;
    }

    const CVector<CRioEvent::BufferBinding>& bufferBindings = rioEvent->GetBufferBindings();

    // 1. TakeOwner
    CRioObjectRef rioObject = rioEvent->TakeOwner();

    if( rioObject == nullptr )
    {
        MarkFaulted(true, true);
        assert(false && "RIO completion has no owner");
        return;
    }

    // 2. Dispatch
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
                if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) ) break;
            }

            assert(false && "CRioObject::Dispatch() threw an exception");
        }
    }

    // 3. FreeSlot
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
        MarkFaulted(true, true);
    }

    // 4. EventPool Free
    if( _eventPool != nullptr )
    {
        try
        {
            _eventPool->Free(rioEvent);
        }
        catch( ... )
        {
            MarkFaulted(true, true);
            assert(false && "Exception in CRioEventPool::Free()");
        }
    }
    else
    {
        MarkFaulted(true, true);
        assert(false && "EventPool became null while completion was outstanding");
    }
}

//***************************************************************************
// @brief outstanding I/O 카운트를 원자적으로 1 증가시킵니다.
// @return 성공 시 true, overflow 발생 시 false
//***************************************************************************
bool CRioCore::IncrementIoCount() noexcept
{
    uint32_t current = _outstandingIo.load(std::memory_order_relaxed);

    for( ;;)
    {
        if( current == std::numeric_limits<uint32_t>::max() )
        {
            assert(false && "Outstanding I/O counter overflow");
            return false;
        }

        if( _outstandingIo.compare_exchange_weak(current, current + 1, std::memory_order_relaxed, std::memory_order_relaxed) ) return true;
    }
}

//***************************************************************************
// @brief outstanding I/O 카운트를 원자적으로 1 감소시킵니다.
//***************************************************************************
void CRioCore::DecrementIoCount() noexcept
{
    uint32_t current = _outstandingIo.load(std::memory_order_relaxed);

    for( ;;)
    {
        if( current == 0 )
        {
            assert(false && "Outstanding I/O counter underflow");
            return;
        }

        if( _outstandingIo.compare_exchange_weak(current, current - 1, std::memory_order_release, std::memory_order_relaxed) ) return;
    }
}

//***************************************************************************
// @brief CQ 손상 및 Worker 결함 발생 시 Faulted 상태로 전환하고 IOCP 깨움을 유도합니다.
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
        if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) ) break;
    }

    if( _iocpHandle != NULL ) ::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
}