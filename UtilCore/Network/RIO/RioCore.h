
//***************************************************************************
// RioCore.h : interface for the CRioCore class.
//
//***************************************************************************

#ifndef __RIOCORE_H__
#define __RIOCORE_H__

#ifndef __RIOCOMMON_H__
#include <Network/RIO/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/RIO/RioEvent.h>
#endif

#ifndef __RIOEVENTPOOL_H__
#include <Network/RIO/RioEventPool.h>
#endif

class CRioEvent;
class CRioEventPool;

//***************************************************************************
// @class CRioCore
// @brief RIO(Registered I/O) 코어 Engine 및 Completion Dispatcher
//
// @details
//      CRioCore는 Windows RIO(Registered I/O) API의 Completion Queue(CQ) 및
//      IOCP 알림(Notification) 메커니즘을 총괄 관리하는 핵심 코어 클래스입니다.
//
// [주요 기능]
//      1. RIO 함수 테이블(RIO_EXTENSION_FUNCTION_TABLE) 바인딩 및 관리
//      2. Send/Receive Completion Queue(RIO_CQ) 생성 및 파기
//      3. IOCP 기반 Completion 알림 연동 및 Dequeue/Notify 루프 수행
//      4. Worker Thread 제어 및 Dispatch Multi-threading 동기화
//      5. Outstanding I/O 카운팅 및 안전한 Shutdown/Drain 시퀀스 보장
//
// [동기화 및 Lifecycle]
//      - Submission Gate: _submissionMutex를 통해 Shutdown 진입 시 신규 I/O 제출 차단
//      - Dispatch Gate: _dispatchGate를 통해 워커 스레드의 DispatchBatch()와
//        Shutdown()의 리소스 파괴 구간이 서로 겹치지 않도록 상호 배제
//      - CQ Consumer: _cqConsumerMutex를 사용하여 Dequeue / Notify 동시 접근 보호
//
// [워커 스레드 정책]
//      CRioCore 인스턴스당 워커 스레드는 정확히 1개만 존재합니다(재시작 없음,
//      StartWorker() 1회 성공 후 재호출은 거부됨). Shutdown()은 이 단일
//      워커가 완전히 정지했음을 확인한 뒤에만 CQ Drain 및 리소스 파괴 단계로
//      진입합니다.
//***************************************************************************
class CRioCore
{
public:
    CRioCore();
    ~CRioCore();

    CRioCore(const CRioCore&) = delete;
    CRioCore& operator=(const CRioCore&) = delete;
    CRioCore(CRioCore&&) = delete;
    CRioCore& operator=(CRioCore&&) = delete;

public:
    //***************************************************************************
    // @brief RIO Core Engine을 초기화합니다 (RIO 함수 테이블 로드 + CQ/IOCP 생성).
    // @param socket RIO 함수 테이블 로드 및 바인딩을 위한 소켓
    // @param maxCompletionResults Completion Queue에서 한 번에 처리할 최대 결과 수
    // @param cqIdentifier CQ 구분 식별자 Tag (하위 2비트는 Receive/Send 태그용으로
    //        예약되어 있으므로 0이어야 함)
    // @param eventPool I/O에 사용될 RIO Event Pool 객체 Pointer
    // @return bool 성공 시 true. 성공 시 상태가 Uninitialized -> Initialized로 전이됨.
    //***************************************************************************
    bool Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool);

    //***************************************************************************
    // @brief 외부에서 RIO Engine 정지를 요청합니다 (비동기 — 즉시 정지를 보장하지
    //        않으며, 워커가 다음 IOCP wake-up 때 정지를 인지함).
    // @note Dispatch 콜백(CRioObject::Dispatch) 내부에서 호출하면 안 됩니다
    //       (assert 후 무시됨).
    //***************************************************************************
    void RequestStop();

    //***************************************************************************
    // @brief RIO Completion 처리용 Worker 스레드를 시작합니다.
    // @tparam F 워커 루프 함수 타입
    // @param workerFunc 실행할 워커 루프 람다 또는 함수 객체
    // @return 성공 시 true, 실패 시 false
    //***************************************************************************
    template<typename F>
    bool StartWorker(F&& workerFunc)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        // 1. 상태 전제조건 확인: Initialize() 완료 상태여야 하고, 이미 워커가
        //    실행 중이거나 남아있으면 거부합니다(재시작/중복 시작 방지).
        if( _state.load(std::memory_order_acquire) != Rio::State::Initialized ) return false;
        if( _workerRunning.load(std::memory_order_acquire) || _workerThread.joinable() ) return false;

        // 2. 워커 시작 전 상태 초기화 및 Initialized -> Running 선전이
        //    (스레드 생성이 실패하면 아래 catch에서 Initialized로 되돌립니다).
        _workerFaulted.store(false, std::memory_order_release);
        _workerThreadId.store(std::thread::id{}, std::memory_order_release);
        _state.store(Rio::State::Running, std::memory_order_release);
        _workerRunning.store(true, std::memory_order_release);

        try
        {
            // 3. 실제 워커 스레드 생성. 스레드 내부에서 자신의 thread id를
            //    저장한 뒤 사용자 루프(func)를 실행하고, 예외 발생 시
            //    FaultInternal()로 결함 상태 전이 후 정상적으로 스레드를 종료합니다
            //    (예외를 스레드 밖으로 내보내지 않음 — std::terminate() 방지).
            _workerThread = std::thread([this, func = std::forward<F>(workerFunc)]() mutable noexcept
                {
                    _workerThreadId.store(std::this_thread::get_id(), std::memory_order_release);

                    try
                    {
                        func();
                    }
                    catch( ... )
                    {
                        FaultInternal();
                    }

                    _workerRunning.store(false, std::memory_order_release);
                });

            return true;
        }
        catch( ... )
        {
            // 4. 스레드 생성 자체가 실패한 경우(리소스 부족 등) — 상태를
            //    Running 이전(Initialized)으로 되돌려 재시도 가능하게 합니다.
            _workerRunning.store(false, std::memory_order_release);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);
            _state.store(Rio::State::Initialized, std::memory_order_release);
            return false;
        }
    }

    //***************************************************************************
    // @brief Completion Queue에서 완료 이벤트를 하나(또는 대기 후 하나) 처리합니다.
    // @param mode Wait: 처리할 게 없으면 IOCP에서 블로킹 대기(워커 스레드 전용).
    //             Drain: 블로킹 대기 없이 잔여 이벤트만 즉시 처리하고 반환
    //             (Shutdown()의 드레인 루프 전용).
    // @return int32 처리된 completion 개수(>=0), 또는 Rio::k* 에러 코드(<0)
    //***************************************************************************
    int32 DispatchBatch(Rio::DispatchMode mode = Rio::DispatchMode::Wait);

    //***************************************************************************
    // @brief RIO Core Engine을 안전하게 종료합니다 (워커 Join → outstanding I/O
    //        드레인 → CQ/IOCP 파괴).
    // @param drainTimeout 드레인 대기 제한시간
    // @return Rio::ShutdownResult 종료 결과(Success면 이후 상태는 Closed)
    //***************************************************************************
    Rio::ShutdownResult Shutdown(std::chrono::milliseconds drainTimeout = Rio::kDefaultDrainTimeout);

    //***************************************************************************
    // @brief RIO I/O 요청 제출을 동기화 보호 하에 실행합니다.
    // @tparam F I/O 제출 함수 타입
    // @param submitFunc 실제 RIOSendEx/RIOReceiveEx를 호출하는 람다
    // @return 제출 성공 시 true, 실패 시 false
    //***************************************************************************
    template<typename F>
    bool SubmitIo(F&& submitFunc)
    {
        // 1. Shared lock으로 진입 — StopInternal()의 unique_lock과 상호 배제되므로,
        //    이 lock을 통과했다는 것 자체가 "이 시점엔 Stopping 전이가 진행 중이
        //    아니다"를 보장합니다.
        std::shared_lock<std::shared_mutex> submissionLock(_submissionMutex);

        // 2. 상태 확인 및 outstanding I/O 카운트 선증가(admission).
        if( _state.load(std::memory_order_acquire) != Rio::State::Running ) return false;
        if( !IncrementIoCount() ) return false;

        // 3. 실제 제출 함수 실행. 실패(또는 예외) 시 증가시켰던 카운트를 롤백합니다.
        try
        {
            if( !submitFunc() )
            {
                DecrementIoCount();
                return false;
            }
        }
        catch( ... )
        {
            DecrementIoCount();
            return false;
        }

        return true;
    }

    //***************************************************************************
    // @brief RIO I/O 제출 시도 (SubmitIo의 별칭)
    //***************************************************************************
    template<typename F>
    bool TrySubmit(F&& submitFunc)
    {
        return SubmitIo(std::forward<F>(submitFunc));
    }

    //***************************************************************************
    // @brief 로드된 RIO 함수 테이블의 참조를 반환합니다.
    //***************************************************************************
    const RIO_EXTENSION_FUNCTION_TABLE& GetRioTable() const noexcept
    {
        return _rioTable;
    }

    //***************************************************************************
    // @brief Receive Completion Queue 핸들을 반환합니다.
    //***************************************************************************
    RIO_CQ GetReceiveQueue() const noexcept
    {
        return _receiveCq;
    }

    //***************************************************************************
    // @brief Send Completion Queue 핸들을 반환합니다.
    //***************************************************************************
    RIO_CQ GetSendQueue() const noexcept
    {
        return _sendCq;
    }

    //***************************************************************************
    // @brief 바인딩된 이벤트 풀 포인터를 반환합니다.
    // @return CRioEventPool* 이벤트 풀 포인터
    //***************************************************************************
    CRioEventPool* GetEventPool() const noexcept
    {
        return _eventPool;
    }

    //***************************************************************************
    // @brief 현재 신규 I/O 제출이 가능한 상태인지 확인합니다.
    //***************************************************************************
    bool CanSubmitIo() const noexcept
    {
        return _state.load(std::memory_order_acquire) == Rio::State::Running;
    }

    //***************************************************************************
    // @brief 신규 I/O 진입(Admission)이 차단되었는지 확인합니다.
    //***************************************************************************
    bool IsAdmissionClosed() const noexcept
    {
        const Rio::State state = _state.load(std::memory_order_acquire);
        return state == Rio::State::Stopping || state == Rio::State::Stopped || state == Rio::State::Faulted || state == Rio::State::Closed;
    }

    //***************************************************************************
    // @brief 현재 CRioCore의 상태를 반환합니다.
    //***************************************************************************
    Rio::State GetState() const noexcept
    {
        return _state.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Worker 스레드에 결함(Fault)이 발생했는지 확인합니다.
    //***************************************************************************
    bool IsWorkerFaulted() const noexcept
    {
        return _workerFaulted.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Receive CQ 손상 여부를 반환합니다.
    //***************************************************************************
    bool IsReceiveCqCorrupted() const noexcept
    {
        return _receiveCqCorrupted.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Send CQ 손상 여부를 반환합니다.
    //***************************************************************************
    bool IsSendCqCorrupted() const noexcept
    {
        return _sendCqCorrupted.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 어느 하나라도 CQ 손상이 발생했는지 확인합니다.
    //***************************************************************************
    bool IsCqCorrupted() const noexcept
    {
        return _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 현재 처리 중인 Outstanding I/O 개수를 반환합니다.
    //***************************************************************************
    uint32_t GetOutstandingIoCount() const noexcept
    {
        return _outstandingIo.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 마지막 Shutdown 결과를 반환합니다.
    //***************************************************************************
    Rio::ShutdownResult GetLastShutdownResult() const
    {
        std::lock_guard<std::mutex> lock(_lifecycleMutex);
        return _lastShutdownResult;
    }

private:
    int32 _DispatchBatchImpl(Rio::DispatchMode mode);
    void StopInternal();
    void FaultInternal() noexcept;

    int32 DispatchResults(Rio::RioCqType cqType, RIORESULT* results, ULONG numResults) noexcept;
    void ProcessRioResult(Rio::RioCqType cqType, LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept;

    bool IncrementIoCount() noexcept;
    void DecrementIoCount() noexcept;

    void MarkFaulted(bool receiveCqCorrupt, bool sendCqCorrupt) noexcept;

    //***************************************************************************
    // @brief _state를 from -> to로 원자적으로 전이 시도합니다(CAS 1회).
    //***************************************************************************
    bool TryTransitionState(Rio::State from, Rio::State to) noexcept
    {
        Rio::State expected = from;
        return _state.compare_exchange_strong(expected, to, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    bool DrainCompletionQueue(RIO_CQ cq, RIORESULT* results, ULONG& numResults) noexcept;
    bool NotifyCompletionQueue(RIO_CQ cq) noexcept;

    bool IsValidCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;
    bool IsStopPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;
    bool IsReceiveCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;
    bool IsSendCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;

    //***************************************************************************
    // @struct TlsDispatchGuard
    // @brief TLS Dispatch Context 복원용 RAII 가드. 스코프 종료 시 이전 값으로
    //        되돌려 중첩 호출(예: Shutdown()의 드레인 루프가 자기 자신을
    //        Dispatch Core로 다시 표시하는 경우)에도 안전합니다.
    //***************************************************************************
    struct TlsDispatchGuard
    {
        CRioCore*& slot;    // TLS 슬롯 참조
        CRioCore* previous; // 이전 Dispatch Core 포인터

        ~TlsDispatchGuard() noexcept
        {
            slot = previous;
        }
    };

    //***************************************************************************
    // @struct OutstandingIoGuard
    // @brief CRioCore 자신의 outstanding I/O 카운트를 스코프 종료 시 자동 감축합니다.
    //***************************************************************************
    struct OutstandingIoGuard
    {
        CRioCore* core;

        ~OutstandingIoGuard() noexcept
        {
            if( core != nullptr ) core->DecrementIoCount();
        }
    };

    //***************************************************************************
    // @struct ObjectIoCountGuard
    // @brief CRioObject(개별 세션 등)의 I/O 카운트를 Dispatch() 반환 이후
    //        스코프 종료 시 자동 감축합니다. 호출자(CRioObject 구현체)는
    //        이 카운트를 직접 건드리면 안 됩니다.
    //***************************************************************************
    struct ObjectIoCountGuard
    {
        CRioObject* object;

        ~ObjectIoCountGuard() noexcept
        {
            if( object != nullptr ) object->DecrementIoCount();
        }
    };

private:
    static thread_local CRioCore* _tlsDispatchCore; // 현재 스레드의 Dispatch Core TLS 포인터

    friend class CRioObject;

private:
    RIO_EXTENSION_FUNCTION_TABLE _rioTable{}; // WSAIoctl로 로드한 RIO API 함수 포인터 테이블

    RIO_CQ _receiveCq{ RIO_INVALID_CQ }; // Receive 전용 RIO Completion Queue 핸들
    RIO_CQ _sendCq{ RIO_INVALID_CQ };    // Send 전용 RIO Completion Queue 핸들

    HANDLE _iocpHandle{ NULL }; // RIO completion 알림을 받기 위한 internal IOCP 핸들

    OVERLAPPED _receiveOverlapped{}; // Receive RIONotify용 OVERLAPPED 구조체
    OVERLAPPED _sendOverlapped{};    // Send RIONotify용 OVERLAPPED 구조체

    RIO_NOTIFICATION_COMPLETION _receiveNotification{}; // Receive Notification 구조체
    RIO_NOTIFICATION_COMPLETION _sendNotification{};    // Send Notification 구조체

    ULONG_PTR _cqIdentifier{ 0 }; // CQ Completion Key 바인딩용 고유 식별 태그

    CRioEventPool* _eventPool{ nullptr }; // Completion 처리 후 이벤트 반환용 EventPool

    std::atomic<Rio::State> _state{ Rio::State::Uninitialized }; // Core Lifecycle 상태
    std::atomic<bool> _workerRunning{ false };                   // Worker 실행 여부
    std::atomic<bool> _workerFaulted{ false };                   // Worker Fault 상태

    std::atomic<uint32_t> _outstandingIo{ 0 }; // 현재 진행 중인 Outstanding I/O 개수

    std::atomic<bool> _receiveCqCorrupted{ false }; // Receive CQ Corrupt 손상 플래그
    std::atomic<bool> _sendCqCorrupted{ false };    // Send CQ Corrupt 손상 플래그

    std::thread _workerThread;                      // Worker Thread (인스턴스당 1개)
    std::atomic<std::thread::id> _workerThreadId{}; // Worker Thread ID 정보

    mutable std::mutex _lifecycleMutex; // Lifecycle 동기화 Mutex
    std::shared_mutex _submissionMutex; // Submission 동기화 Shared Mutex
    std::mutex _cqConsumerMutex;        // CQ Consumer 동기화 Mutex
    std::shared_mutex _dispatchGate;    // Dispatch Gate Shared Mutex

    std::condition_variable _shutdownCv; // Shutdown 대기 조건 변수

    bool _shutdownInProgress{ false }; // Shutdown 진행 중 플래그
    bool _shutdownDone{ false };       // Shutdown 완료 플래그

    Rio::ShutdownResult _lastShutdownResult{ Rio::ShutdownResult::Success }; // 마지막 Shutdown 결과
};

#endif // ndef __RIOCORE_H__