
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
// @note 정상 라이프사이클에서는 호출자가 이미 Shutdown()을 호출해 Closed
//       상태였어야 합니다. 여기서 다시 한번 Shutdown()을 호출하는 건
//       방어적 안전장치입니다(Uninitialized 상태였다면 여기서 바로
//       Closed로 전이됨, 그 외 상태라면 원래 이미 처리됐어야 정상).
//       최종 상태가 Closed가 아니면 리소스 정리가 안 됐다는 뜻이므로
//       assert 후 강제 종료합니다.
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
bool CRioCore::Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool)
{
    // 1. 파라미터 사전 검증 (락 밖에서, 상태를 건드리기 전에 먼저 확인)
    if( eventPool == nullptr || socket == INVALID_SOCKET || cqIdentifier == 0 ) return false;
    if( (cqIdentifier & Rio::kCompletionTagMask) != 0 ) return false;
    // MS 문서: RIOCreateCompletionQueue의 QueueSize 파라미터는 RIO_MAX_CQ_SIZE를
    // 초과할 수 없습니다(초과 시 WSAEINVAL). 0도 유효하지 않은 크기입니다.
    if( maxCompletionResults == 0 || maxCompletionResults > RIO_MAX_CQ_SIZE ) return false;

    std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

    // 2. 중복 초기화 방지 — Uninitialized 상태에서만 진행
    if( _state.load(std::memory_order_acquire) != Rio::State::Uninitialized ) return false;

    _state.store(Rio::State::Initializing, std::memory_order_release);

    // 3. WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER)로 RIO 확장 함수
    //    테이블을 로드합니다. MS 문서(RIO 개요/WSAIoctl reference) 기준:
    //    - RIO 함수들은 일반 DLL 익스포트가 아니라 WSAIoctl을 통해서만 얻을 수
    //      있는 소켓 확장 함수이며, ConnectEx/AcceptEx 등과 같은 계열의
    //      "provider별 확장 함수" 메커니즘을 씁니다.
    //    - WSAID_MULTIPLE_RIO GUID를 넘기면 RIO_EXTENSION_FUNCTION_TABLE
    //      구조체 전체(모든 RIO* 함수 포인터)를 한 번에 채워 돌려줍니다
    //      (SIO_GET_EXTENSION_FUNCTION_POINTER처럼 함수 하나씩 조회하지 않아도 됨).
    //    - 이 함수 포인터들은 특정 소켓 인스턴스가 아니라 그 소켓이 속한
    //      Winsock provider에 종속되므로, 이후 다른 소켓(RQ 생성 대상 등)에도
    //      재사용할 수 있습니다(MS 문서에 명시된 지원 패턴).
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

    // 4. 이 클래스가 실제로 사용하는 함수 포인터들이 전부 유효한지 검증
    //    (드라이버/스택이 RIO를 지원하지 않는 경우 일부가 null일 수 있음).
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

    // 5. Completion 알림 수신용 내부 전용 IOCP 생성. MS 문서(RIOCreateCompletionQueue,
    //    RIO_NOTIFICATION_COMPLETION) 기준 RIO CQ의 완료 알림 방식은 두 가지
    //    (RIO_EVENT_COMPLETION 또는 RIO_IOCP_COMPLETION) 중 선택인데, 이
    //    클래스는 IOCP 기반(RIO_IOCP_COMPLETION)을 사용합니다 — 이 방식에서는
    //    RIONotify() 호출 시점에 지정한 OVERLAPPED가 GetQueuedCompletionStatus()로
    //    완료 통지됩니다. 마지막 파라미터 1은 동시성 값(NumberOfConcurrentThreads)으로,
    //    이 IOCP는 이 CRioCore 전용 단일 워커 스레드로만 소비되므로 1로 고정합니다.
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

    // 6. Receive/Send 각각의 RIO_NOTIFICATION_COMPLETION(Type=RIO_IOCP_COMPLETION)을
    //    구성합니다. CompletionKey에 Receive/Send 구분 태그를 섞어 넣는 이유는,
    //    MS 문서상 GetQueuedCompletionStatus()가 돌려주는 CompletionKey/Overlapped
    //    조합만으로는 "어느 RIO_CQ의 알림인지" 구분할 표준 방법이 없기 때문에,
    //    자체적으로 CompletionKey에 식별 태그를 실어 보내는 패턴입니다.
    _receiveNotification.Type = RIO_IOCP_COMPLETION;
    _receiveNotification.Iocp.IocpHandle = tempIocp;
    _receiveNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier | Rio::kReceiveCompletionTag);
    _receiveNotification.Iocp.Overlapped = &_receiveOverlapped;

    _sendNotification.Type = RIO_IOCP_COMPLETION;
    _sendNotification.Iocp.IocpHandle = tempIocp;
    _sendNotification.Iocp.CompletionKey = reinterpret_cast<void*>(cqIdentifier | Rio::kSendCompletionTag);
    _sendNotification.Iocp.Overlapped = &_sendOverlapped;

    // 7. Receive CQ 생성. MS 문서: RIOCreateCompletionQueue(QueueSize, NotificationCompletion)는
    //    성공 시 RIO_CQ 핸들을, 실패 시 RIO_INVALID_CQ를 반환하며 WSAGetLastError()로
    //    원인을 확인할 수 있습니다(예: QueueSize가 RIO_MAX_CQ_SIZE 초과 시 WSAEINVAL,
    //    리소스 부족 시 WSAENOBUFS). 실패 시 IOCP까지 함께 정리합니다.
    RIO_CQ tempReceiveCq = tempRioTable.RIOCreateCompletionQueue(maxCompletionResults, &_receiveNotification);
    if( tempReceiveCq == RIO_INVALID_CQ )
    {
        ::CloseHandle(tempIocp);
        ZeroMemory(&_receiveNotification, sizeof(_receiveNotification));
        ZeroMemory(&_sendNotification, sizeof(_sendNotification));
        _state.store(Rio::State::Uninitialized, std::memory_order_release);
        return false;
    }

    // 8. Send CQ 생성 (실패 시 앞서 만든 Receive CQ와 IOCP까지 역순 정리)
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

    // 9. 모든 단계 성공 — 실제 멤버에 커밋하고 런타임 상태 초기화
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

    // 10. Initialized로 전이 — 이제 StartWorker() 호출이 가능한 상태
    _state.store(Rio::State::Initialized, std::memory_order_release);
    return true;
}

//***************************************************************************
// @brief 외부에서 RIO Engine 정지를 요청합니다.
//***************************************************************************
void CRioCore::RequestStop()
{
    // Dispatch 콜백(CRioObject::Dispatch) 내부에서 RequestStop()을 호출하면
    // 워커 스레드가 자기 자신의 정지를 기다리는 형태가 되어 문제가 되므로 차단합니다.
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
    // 1. Submission Gate를 exclusive로 잡아, 이미 진행 중인 SubmitIo() 호출이
    //    전부 끝날 때까지 대기한 뒤에만 상태를 전이합니다(진행 중인 제출과
    //    Stopping 전이가 겹치지 않도록 보장).
    std::unique_lock<std::shared_mutex> submissionLock(_submissionMutex);

    const Rio::State current = _state.load(std::memory_order_acquire);

    if( current == Rio::State::Running || current == Rio::State::Initialized )
    {
        _state.store(Rio::State::Stopping, std::memory_order_release);
    }
    else if( current == Rio::State::Stopping || current == Rio::State::Faulted )
    {
        // 이미 Stopping/Faulted 상태 — 별도 처리 없이 통과(멱등).
    }
    else
    {
        return;
    }

    // 2. 워커가 IOCP GetQueuedCompletionStatus()에서 블로킹 대기 중일 수 있으므로,
    //    completionKey=0, overlapped=nullptr인 "빈" completion packet을 하나
    //    큐잉해서 깨웁니다. MS 문서: PostQueuedCompletionStatus()로 임의의
    //    사용자 정의 패킷을 IOCP에 직접 넣을 수 있으며, 이는 실제 I/O 완료와
    //    무관하게 대기 중인 GetQueuedCompletionStatus() 호출 하나를 즉시
    //    깨우는 표준적인 "wake-up" 패턴입니다. 이 패킷은 IsStopPacket()에서
    //    completionKey==0 && overlapped==nullptr 조합으로 식별됩니다(RIO
    //    자신의 completion은 항상 유효한 CompletionKey/Overlapped 쌍을
    //    가지므로 이 조합과 충돌하지 않습니다).
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
    // Dispatch Gate를 shared로 잡아, Shutdown()의 리소스 파괴 구간(exclusive)과
    // 겹치지 않도록 보장합니다.
    std::shared_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    // 현재 스레드를 이 CRioCore의 Dispatch Context로 표시 —
    // RequestStop()/Shutdown()의 self-call 방지에 사용됩니다.
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

    // MS 문서(RIODequeueCompletion): ArraySize(=Rio::kBatchSize)를 넘지 않는
    // 범위에서 실제로 사용 가능한 completion 개수만큼만 채우고 그 개수를
    // 반환합니다(요청보다 적게 채워져도 정상 — CQ가 비었으면 0). 반환값이
    // RIO_CORRUPT_CQ이면 그 CQ는 손상되어 더 이상 사용할 수 없는 상태이며,
    // 문서상 애플리케이션은 해당 CQ를 닫고 새로 만들어야 합니다 — 이 클래스는
    // 재생성 대신 해당 방향(Receive/Send)을 손상 처리하고 Faulted로 전이시킵니다.
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

    // MS 문서(RIONotify): 이 CQ에 대한 알림은 "one-shot"입니다 — 한 번
    // RIONotify()로 등록해두면, 다음에 completion이 하나라도 발생했을 때
    // 딱 한 번만 통지되고 그 후엔 다시 RIONotify()를 불러야 재등록됩니다.
    // 반환값 ERROR_SUCCESS는 정상 등록, WSAEALREADY는 "이미 등록된 알림이
    // 아직 발생하지 않은 상태에서 다시 호출됨"을 뜻하며 이 역시 정상으로
    // 취급합니다(문서에 명시된 정상 반환 케이스).
    const int result = _rioTable.RIONotify(cq);
    return result == ERROR_SUCCESS || result == WSAEALREADY;
}

//***************************************************************************
// @brief DispatchBatch의 실제 내부 구현부입니다.
// @param mode 디스패치 모드
// @return 처리된 완료 이벤트 개수 또는 에러 코드
//
// @note
//      [CQ Consumer Lock 범위]
//      "CQ Lock 획득 -> Dequeue만" 하고 즉시 락을 풀어, 그 결과를 lock 밖에서
//      DispatchResults()(=무거운 사용자 Dispatch 콜백 포함)로 넘깁니다. 이렇게
//      해야 무거운 Dispatch가 실행되는 동안 다른 CQ(Receive/Send)를 소비하려는
//      스레드가 차단되지 않습니다. 아래 6개 CQ 접근 지점(Receive/Send 각각의
//      Drain, Notify+Drain, 그리고 IOCP wake-up 후 Target CQ Drain) 전부
//      동일한 패턴을 따릅니다.
//
//      [전체 흐름 개요 — MS RIO 권장 패턴 기준]
//      MS 문서(Registered I/O 개요)가 설명하는 표준 완료 처리 루프는
//      "먼저 Dequeue를 시도해 이미 쌓여있는 결과부터 소비하고, 없으면
//      RIONotify로 알림을 등록한 뒤 IOCP에서 대기한다"입니다. Dequeue를
//      먼저 시도하는 이유는, RIONotify 등록 자체에도 비용이 들고 one-shot
//      특성상 등록 시점 이후의 completion만 통지받기 때문에, 이미 도착해
//      있는 결과를 그냥 Dequeue만으로 빠르게 처리하는 게 더 효율적이기
//      때문입니다. 아래 1~2단계(우선 Dequeue) -> 4~5단계(Notify+Dequeue)
//      -> 7단계(IOCP Wait)가 이 패턴을 그대로 구현한 것입니다.
//***************************************************************************
int32 CRioCore::_DispatchBatchImpl(Rio::DispatchMode mode)
{
    // 0. 이미 CQ가 손상된 상태라면 더 진행하지 않습니다.
    if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) )
    {
        return Rio::kCorruptCq;
    }

    // Wait 모드는 워커 스레드가 IOCP에서 블로킹 대기할 수 있으므로,
    // 반드시 그 워커 스레드 자신만 호출해야 합니다.
    if( mode == Rio::DispatchMode::Wait )
    {
        const std::thread::id workerId = _workerThreadId.load(std::memory_order_acquire);
        if( workerId == std::thread::id{} || std::this_thread::get_id() != workerId )
        {
            assert(false && "DispatchBatch(Wait) must only be called from worker thread");
            return Rio::kInvalidCompletion;
        }
    }

    // DrainCompletionQueue()가 항상 [0, numResults) 구간만 채우고 그 범위만
    // 읽으므로, 매 배치마다(핫 패스) 64개 구조체를 0-초기화할 필요가 없습니다.
    RIORESULT results[Rio::kBatchSize];

    // 1. Receive CQ Drain — 알림 없이도 바로 꺼낼 게 있는지 우선 확인
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

    // 2. Send CQ Drain — 위와 동일한 이유로 우선 확인
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

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    // 3. 두 CQ 모두 비어있었고, 정지 절차 중이면서 outstanding I/O도 0이면
    //    여기서 Stopped로 전이하고 종료합니다(Drain 루프의 정상 종료 지점).
    Rio::State state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    // Drain 모드는 여기서 블로킹 대기 없이 반환합니다(Shutdown()의 드레인 루프가
    // 자체적으로 재시도 주기를 관리하기 때문).
    if( mode == Rio::DispatchMode::Drain ) return 0;

    // 4. Receive CQ Notify — one-shot 알림을 다시 등록한 뒤, 등록 직후 바로
    //    뭔가 도착해 있을 수 있으므로(등록과 도착 사이의 race) 한 번 더
    //    Drain합니다. 이 순서(Notify -> Drain, 둘 다 같은 락 안)를 지켜야
    //    "Notify 등록 -> 그 직후 completion 도착 -> 이 Drain이 놓침 -> IOCP
    //    대기에서 영원히 못 깨어남" 같은 race를 막을 수 있습니다.
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

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Receive, results, numResults);
    }

    // 5. Send CQ Notify — 위와 동일
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

        if( numResults > 0 ) return DispatchResults(Rio::RioCqType::Send, results, numResults);
    }

    // 6. Notify까지 걸었는데도 즉시 아무것도 없었다면, 다시 한번 정지 조건 확인
    state = _state.load(std::memory_order_acquire);

    if( (state == Rio::State::Stopping || state == Rio::State::Faulted) && _outstandingIo.load(std::memory_order_acquire) == 0 )
    {
        if( state == Rio::State::Stopping ) TryTransitionState(Rio::State::Stopping, Rio::State::Stopped);
        return Rio::kStopped;
    }

    // 7. 진짜로 처리할 게 없으므로 IOCP에서 블로킹 대기 (Wait 모드 전용 경로).
    //    두 CQ 모두 4~5단계에서 RIONotify()로 알림을 등록해뒀으므로, 그 뒤에
    //    어느 쪽이든 completion이 발생하면 그때 등록해둔 OVERLAPPED
    //    (_receiveOverlapped 또는 _sendOverlapped)와 CompletionKey가 실려
    //    이 GetQueuedCompletionStatus() 호출을 깨웁니다.
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    const BOOL gqcsResult = ::GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE);

    if( !gqcsResult )
    {
        // GQCS 실패도 다른 에러 경로와 동일하게 MarkFaulted()로 결함 상태를 통일합니다.
        MarkFaulted(false, false);

        if( overlapped != nullptr )
        {
            assert(false && "GetQueuedCompletionStatus failed with completion packet");
            return Rio::kIocpError;
        }

        assert(false && "GetQueuedCompletionStatus failed without completion packet");
        return Rio::kIocpError;
    }

    // 8. Stop packet(StopInternal()이 깨우기 위해 큐잉한 빈 패킷)인지 확인
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

    // 9. 우리가 등록한 Receive/Send Notification의 completion key/overlapped
    //    조합이 아니면 손상된 패킷으로 간주합니다.
    if( !IsValidCompletionPacket(completionKey, overlapped) )
    {
        MarkFaulted(false, false);

        assert(false && "Unexpected IOCP completion packet");
        return Rio::kInvalidCompletion;
    }

    // 10. 어느 CQ의 알림인지 식별
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
        MarkFaulted(false, false);

        assert(false && "Invalid CQ completion packet");
        return Rio::kInvalidCompletion;
    }

    // 11. 식별된 CQ에서 실제 completion을 꺼내 디스패치. IOCP 알림은 "이
    //     CQ에 처리할 completion이 최소 1개 이상 있다"는 신호일 뿐, MS
    //     문서상 그 정확한 개수는 알려주지 않으므로 반드시 RIODequeueCompletion()으로
    //     실제 다시 Dequeue해야 합니다(이 Drain 호출 자체가 그 처리입니다).
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
        // MS 문서(RIORESULT 구조체): RequestContext는 해당 I/O를 제출할 때
        // RIOSend/RIOSendEx/RIOReceive/RIOReceiveEx의 마지막 인자로 넘겼던
        // 값이 그대로 되돌아오는 필드입니다(이 클래스는 CRioEvent* 포인터를
        // 이 통로로 실어보냅니다). 따라서 0(null)이면 제출 경로 어딘가가
        // 손상됐다는 뜻입니다.
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

        // 방금 처리 중 CQ가 손상됐다고 표시됐으면 나머지 결과는 처리하지 않고 중단합니다.
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
    // Dispatch 콜백 내부에서 자기 자신의 Shutdown()을 부르면 워커 스레드가
    // 자신의 Join을 기다리는 셀프 데드락이 되므로 차단합니다.
    if( _tlsDispatchCore == this )
    {
        assert(false && "Shutdown() must not be called from Dispatch callback");
        std::lock_guard<std::mutex> lifecycleLock(_lifecycleMutex);
        _lastShutdownResult = Rio::ShutdownResult::InvalidCall;
        return _lastShutdownResult;
    }

    if( drainTimeout < std::chrono::milliseconds::zero() ) drainTimeout = std::chrono::milliseconds::zero();

    std::thread threadToJoin;

    // Phase 1. Admission Close — 신규 제출 차단 및 정지 요청, 중복/동시
    //          Shutdown() 호출을 단일 흐름으로 직렬화합니다.
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        const Rio::State currentState = _state.load(std::memory_order_acquire);

        if( currentState == Rio::State::Closed || _shutdownDone ) return _lastShutdownResult;

        if( _shutdownInProgress )
        {
            // 다른 스레드가 이미 Shutdown 중 — 그 완료를 기다렸다가 같은 결과를 반환합니다.
            _shutdownCv.wait(lifecycleLock, [this] { return _shutdownDone || !_shutdownInProgress; });
            return _lastShutdownResult;
        }

        if( currentState == Rio::State::Uninitialized )
        {
            // Initialize()를 한 번도 하지 않은 상태에서 Shutdown()이 호출된 경우.
            // 소멸자는 최종 상태가 Closed가 아니면 std::terminate()하므로,
            // 여기서 명시적으로 Closed로 전이해 "Initialize 없이 생성만 하고
            // 소멸시키는" 정상 케이스가 죽지 않도록 합니다.
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

    // Phase 2. Dispatch Domain Exclusive — 이 시점부터는 다른 스레드가
    //          DispatchBatch()에 새로 진입할 수 없습니다(진행 중이던 것은
    //          완료될 때까지 여기서 대기).
    std::unique_lock<std::shared_mutex> dispatchGateLock(_dispatchGate);

    // Phase 3. Worker Join — 워커가 완전히 멈췄음을 확인. 이 뒤에야 outstanding
    //          I/O를 안전하게 드레인할 수 있습니다.
    if( threadToJoin.joinable() ) threadToJoin.join();

    _workerRunning.store(false, std::memory_order_release);
    _workerThreadId.store(std::thread::id{}, std::memory_order_release);

    // Phase 4. CQ Drain — 워커가 멈췄으니 이제 이 스레드가 직접
    //          _DispatchBatchImpl(Drain)을 반복 호출해 outstanding I/O가
    //          0이 될 때까지(또는 타임아웃/손상 발생까지) 잔여 completion을 처리합니다.
    //          MS 문서상 이미 제출된 RIOSend/RIOSendEx/RIOReceive/RIOReceiveEx
    //          요청은 취소 API가 없으므로, 소켓/CQ를 닫기 전 반드시 그
    //          completion이 CQ로 돌아올 때까지 명시적으로 기다려야 합니다
    //          (그러지 않고 CQ/IOCP를 먼저 닫으면 나중에 도착할 completion이
    //          이미 해제된 자원을 참조하는 UAF로 이어질 수 있음).
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

            // 처리할 게 없어도 즉시 재시도하지 않고 짧게 쉬었다가 다시 시도합니다
            // (Drain 모드는 블로킹 대기 없이 바로 반환하므로 바쁜 대기가 되는 것을 방지).
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

    // Phase 5. Drain Validation — 루프를 빠져나온 사유를 최종 결과에 반영합니다.
    if( finalResult == Rio::ShutdownResult::Success )
    {
        if( _outstandingIo.load(std::memory_order_acquire) != 0 ) finalResult = Rio::ShutdownResult::DrainTimeout;
        else if( _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire) ) finalResult = Rio::ShutdownResult::CorruptCq;
        else if( _workerFaulted.load(std::memory_order_acquire) ) finalResult = Rio::ShutdownResult::DispatchError;
    }

    // Phase 6. Resource Destruction — outstanding I/O가 확실히 0일 때만
    //          CQ/IOCP를 실제로 파괴합니다. 0이 아니면(드레인 실패) 리소스를
    //          그대로 남겨둬서, 아직 완료 안 된 요청이 이미 해제된 자원을
    //          참조하는 UAF를 방지합니다. MS 문서(RIOCloseCompletionQueue):
    //          이 CQ를 참조하는 RIO_RQ가 아직 존재하는 상태에서 CQ를 닫는
    //          동작은 정의되어 있지 않으므로, 반드시 outstanding이 0임을
    //          먼저 확인한 뒤에만 호출해야 합니다.
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

    // Phase 8. Lifecycle Commit — 최종 상태를 확정하고 대기 중이던 다른
    //          Shutdown() 호출자들에게 통지합니다.
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
void CRioCore::ProcessRioResult(Rio::RioCqType cqType, LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept
{
    // 1. 이 completion 하나에 대응하는 CRioCore 자신의 outstanding I/O 카운트를
    //    함수 종료 시 자동 감소시킵니다.
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

    // 2. 이벤트가 보유하고 있던 Owner(CRioObject) shared_ptr을 여기로 회수 —
    //    이 함수가 끝날 때까지 그 객체의 lifetime을 로컬에서 보장합니다.
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

    // 3. Owner 객체 자신의 IoCount도 이 함수(특히 Dispatch() 호출) 종료 후
    //    자동 감소시킵니다. rioObject보다 나중에 생성되어 먼저 소멸하므로,
    //    "감소 -> 그 다음에야 rioObject 참조 해제 가능"이라는 순서가 보장됩니다.
    ObjectIoCountGuard objectIoGuard{ rioObject.get() };

    // MS 문서(RIORESULT.Status): 이 값은 해당 I/O의 Winsock 에러 코드입니다
    // (NO_ERROR가 성공). BytesTransferred는 실제 송수신된 바이트 수이며,
    // 실패 시(status != NO_ERROR)에는 유효하지 않으므로 0으로 전달합니다.
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

    // 4. Dispatch()가 반환된 이후에만 버퍼 바인딩을 반환합니다(Dispatch() 도중에
    //    사용자 코드가 그 버퍼 내용을 읽는 경우가 있으므로).
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

    // 5. 마지막으로 이벤트 자체를 풀에 반환합니다.
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
            // Shutdown()의 드레인 루프가 이 감소를 즉시 알아챌 필요는 없지만
            // (폴링 방식), 대기 중인 다른 스레드가 있을 수 있으므로 통지합니다.
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

    // Running/Stopping 상태였다면 Faulted로 전이합니다(그 외 상태는 그대로 둠).
    Rio::State current = _state.load(std::memory_order_acquire);

    while( current == Rio::State::Running || current == Rio::State::Stopping )
    {
        if( _state.compare_exchange_weak(current, Rio::State::Faulted, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            break;
        }
    }

    // 워커가 IOCP 대기 중일 수 있으므로 깨워서 Faulted 상태를 인지하게 합니다.
    HANDLE iocp = _iocpHandle;

    if( iocp != NULL )
    {
        ::PostQueuedCompletionStatus(iocp, 0, 0, nullptr);
    }

    _shutdownCv.notify_all();
}