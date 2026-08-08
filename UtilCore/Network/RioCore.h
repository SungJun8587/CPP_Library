#ifndef __RIOCORE_H__
#define __RIOCORE_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#ifndef	__RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

#ifndef	__RIOEVENTPOOL_H__
#include <Network/RioEventPool.h>
#endif

#ifndef	__RIOOBJECT_H__
#include <Network/RioObject.h>
#endif

#include <winsock2.h>
#include <mswsock.h>
#include <memory>
#include <utility>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>

class CRioEvent;
class CRioEventPool;
class CRioObject;

using CRioObjectRef = std::shared_ptr<CRioObject>;

enum class ShutdownResult : int32
{
    Success = 0,
    DrainTimeout = 1,
    CorruptCq = 2
};

class CRioCore
{
    enum { BATCH_SIZE = 64 };   // RIODequeueCompletion 1회 최대 수거 개수

public:
    // Lifecycle State Machine 정의 (Faulted 상태 포함)
    enum class State : int32
    {
        Uninitialized = 0,
        Initializing = 1,
        Initialized = 2,
        Running = 3,
        Stopping = 4,
        Stopped = 5,
        Faulted = 6, // 워커 내부 예외 발생 시 진입하는 결함 상태
        Closed = 7
    };

    // CQ 디스패치 모드 정의 (블로킹 대기 vs 논블로킹 드레인 전용)
    enum class DispatchMode : int32
    {
        Wait = 0, // 대기형 (RIONotify 및 GQCS INFINITE 블로킹 포함)
        Drain = 1  // 논블로킹 드레인 전용
    };

    static constexpr int32 kCorruptCq = -1; // CQ 손상 또는 치명적 오류 코드
    static constexpr int32 kStopped = -2; // 정지 신호 및 잔여 I/O 드레인 완료 코드

    // Shutdown Drain 기본 timeout
    static constexpr std::chrono::milliseconds kDefaultDrainTimeout{ 5000 };

public:
    CRioCore();
    ~CRioCore();

    CRioCore(const CRioCore&) = delete;
    CRioCore& operator=(const CRioCore&) = delete;
    CRioCore(CRioCore&&) = delete;
    CRioCore& operator=(CRioCore&&) = delete;

    bool Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool);

    void RequestStop();

    //***************************************************************************
    // @brief StartWorker 중복 실행 방어 및 스레드 안정성이 보장된 워커 구동 메서드
    // @tparam F 워커 루프 함수 타입
    // @param[in] workerFunc 워커 스레드가 실행할 람다 또는 함수 객체
    // @return 성공 시 true, 이미 실행 중이거나 실패 시 false
    //***************************************************************************
    template<typename F>
    bool StartWorker(F&& workerFunc)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( _state.load(std::memory_order_acquire) != State::Initialized )
            return false;

        if( _workerRunning.load(std::memory_order_acquire) || _workerThread.joinable() )
            return false;

        _state.store(State::Running, std::memory_order_release);
        _workerRunning.store(true, std::memory_order_release);
        _workerFaulted.store(false, std::memory_order_release);

        try
        {
            _workerThread = std::thread([this, func = std::forward<F>(workerFunc)]() mutable noexcept {
                _workerThreadId.store(std::this_thread::get_id(), std::memory_order_release);

                try
                {
                    func();
                    StopInternal();
                }
                catch( ... )
                {
                    _workerFaulted.store(true, std::memory_order_release);
                    TryTransitionState(State::Running, State::Faulted);

                    if( _iocpHandle != NULL )
                    {
                        ::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
                    }
                }

                _workerRunning.store(false, std::memory_order_release);
                });
            return true;
        }
        catch( ... )
        {
            _workerFaulted.store(true, std::memory_order_release);
            _state.store(State::Faulted, std::memory_order_release);
            _workerRunning.store(false, std::memory_order_release);
            return false;
        }
    }

    int32 DispatchBatch(DispatchMode mode = DispatchMode::Wait);

    ShutdownResult Shutdown(std::chrono::milliseconds drainTimeout = kDefaultDrainTimeout);

    //***************************************************************************
    // @brief std::shared_mutex 기반 Admission Gate와 Fallible Increment 원자적 결합
    // @tparam F 실제 RIO 제출 수행 람다 또는 함수 객체 타입
    // @param[in] submitFunc 실제 RIO 제출을 수행하는 함수 객체
    // @return 성공 시 true, 오버플로우 발생, 스톱 상태이거나 제출 실패 시 false
    //***************************************************************************
    template<typename F>
    bool TrySubmit(F&& submitFunc)
    {
        std::shared_lock<std::shared_mutex> submissionLock(_submissionMutex);

        if( _state.load(std::memory_order_acquire) != State::Running )
            return false;

        if( !IncrementIoCount() )
            return false;

        try
        {
            // TrySubmit()의 submitFunc는 CRioCore의 lifecycle API (RequestStop, Shutdown, Initialize, StartWorker)를 호출하지 않아야 한다.
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
    // @brief 신규 I/O 접수 차단 여부 확인 (Admission Gate 인터페이스)
    // @return true인 경우 신규 RIOSend / RIORecv 제출을 즉시 중단해야 함
    //***************************************************************************
    bool IsAdmissionClosed() const
    {
        State s = _state.load(std::memory_order_acquire);
        return s == State::Stopping || s == State::Stopped || s == State::Faulted || s == State::Closed;
    }

    //***************************************************************************
    // @brief 현재 Lifecycle 상태 반환
    //***************************************************************************
    State GetState() const { return _state.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 워커 내부 예외 발생 여부 반환
    //***************************************************************************
    bool IsWorkerFaulted() const { return _workerFaulted.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief RIO 확장 함수 테이블 참조 반환
    //***************************************************************************
    const RIO_EXTENSION_FUNCTION_TABLE& GetRioTable() const { return _rioTable; }

    //***************************************************************************
    // @brief RIO Completion Queue 핸들 반환
    //***************************************************************************
    RIO_CQ GetCompletionQueue() const { return _cq; }

    ShutdownResult _lastShutdownResult { ShutdownResult::Success };

private:
    void StopInternal();

    void DispatchResults(RIORESULT* results, ULONG numResults) noexcept;
    void ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept;

    bool IncrementIoCount() noexcept;
    void DecrementIoCount() noexcept;

    bool TryTransitionState(State from, State to) noexcept
    {
        State expected = from;
        return _state.compare_exchange_strong(expected, to, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    friend class CRioObject;

private:
    RIO_EXTENSION_FUNCTION_TABLE    _rioTable;         // RIO 확장 함수 포인터 집합
    RIO_CQ                          _cq;               // RIO Completion Queue 핸들
    HANDLE                          _iocpHandle;       // 스핀 방지용 전용 IOCP 핸들
    OVERLAPPED                      _rioOverlapped{};  // RIO 알림용 오버랩 구조체
    RIO_NOTIFICATION_COMPLETION     _rioNotification;  // RIO 알림 설정 구조체
    ULONG_PTR                       _cqIdentifier;     // CQ 식별 키
    CRioEventPool*                  _eventPool;        // 이벤트 객체 프리 리스트 풀 포인터

    std::atomic<State>              _state{ State::Uninitialized }; // Lifecycle State Machine 상태
    std::atomic<bool>               _workerRunning{ false };        // 워커 실행 중복 방어 플래그
    std::atomic<bool>               _workerFaulted{ false };        // 워커 내부 예외 발생 여부 추적 플래그
    std::atomic<uint32>             _outstandingIo{ 0 };            // 잔여 I/O 카운터

    std::thread                     _workerThread;     // 코어가 직접 소유하는 워커 스레드
    std::mutex                      _lifecycleMutex;   // 직렬화 뮤텍스
    std::shared_mutex               _submissionMutex;  // 동기화 락
    std::mutex                      _cqConsumerMutex;  // 단일 consumer 보장 뮤텍스
    std::atomic<std::thread::id>    _workerThreadId{}; // 워커 스레드 식별자

    std::condition_variable         _shutdownCv;                    // 셧다운 조건 변수
    bool                            _shutdownInProgress{ false };   // 셧다운 진행 중 플래그
    bool                            _shutdownDone{ false };         // 셧다운 완료 플래그
};

#endif // __RIOCORE_H__