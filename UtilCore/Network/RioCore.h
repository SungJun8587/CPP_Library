
//***************************************************************************
// RioCore.h : interface for the CRioCore class.
//
//***************************************************************************

#ifndef __RIOCORE_H__
#define __RIOCORE_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#ifndef RIOEVENT_H
#include <Network/RioEvent.h>
#endif

#ifndef RIOEVENTPOOL_H
#include <Network/RioEventPool.h>
#endif

#ifndef RIOOBJECT_H
#include <Network/RioObject.h>
#endif

#include <winsock2.h>
#include <mswsock.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <utility>

class CRioEvent;
class CRioEventPool;
class CRioObject;

using CRioObjectRef = std::shared_ptr<CRioObject>;

//***************************************************************************
// @brief Shutdown 작업 수행 시 반환되는 결과 상태 코드
//***************************************************************************
enum class ShutdownResult : int32
{
    Success = 0,          // 정상적으로 모든 종료 및 드레인 작업 완료
    DrainTimeout = 1,     // 지정된 시간 내에 미완료 I/O가 드레인되지 않음
    CorruptCq = 2,        // RIO 완료 큐(CQ) 손상 감지
    InvalidCall = 3,      // 유효하지 않은 스레드 컨텍스트에서의 잘못된 호출 (예: 자체 조인 등)
    DispatchError = 4     // 디스패치 및 폴링 과정 중 하위 에러 발생
};

//***************************************************************************
// @brief RIO 코어 엔진 핵심 클래스
// @details 완료 큐(CQ), IOCP 바인딩, 워커 스레드 제어 및 I/O 동기화를 관리합니다.
// @note
// [락 정렬 정책(Lock Ordering Policy)]
//
// [Admission / Lifecycle Domain]
//
//     _lifecycleMutex
//          |
//          +--> _submissionMutex
//
// [Dispatch / CQ Domain]
//
//     _dispatchGate
//          |
//          +--> _cqConsumerMutex
//
// [CRITICAL]
//
//     _lifecycleMutex와 _dispatchGate는 서로 중첩하여 획득하지 않는다.
//
//     즉 다음 두 패턴 모두 금지한다.
//
//         _lifecycleMutex
//             -> _dispatchGate     // FORBIDDEN
//
//         _dispatchGate
//             -> _lifecycleMutex   // FORBIDDEN
//
// 두 lock domain 사이의 전환이 필요한 경우 반드시
// 상위 lock을 먼저 release한 후 다른 domain의 lock을 획득한다.
//
// [Shutdown Ordering]
//
//     Phase 1:
//         _lifecycleMutex
//             -> _submissionMutex (exclusive)
//             -> State = Stopping
//             -> worker wake-up
//
//         _lifecycleMutex RELEASE
//
//     Phase 2:
//         _dispatchGate (exclusive)
//             -> 기존 DispatchBatch 종료 대기
//             -> worker join
//
//     Phase 3:
//         _dispatchGate (exclusive)
//             -> _cqConsumerMutex
//             -> CQ drain / resource destruction
//
// [IMPORTANT]
//  1. StopInternal()은 호출자가 _lifecycleMutex를 보유한 상태에서만 호출한다.
//  2. StopInternal()은 _dispatchGate를 절대로 획득하지 않는다.
//  3. Shutdown은 _lifecycleMutex를 보유한 상태에서 _dispatchGate를 기다리지 않는다.
//  4. Shutdown은 _dispatchGate를 획득한 상태에서 _lifecycleMutex를 기다리지 않는다.
//  5. _cqConsumerMutex는 반드시 _dispatchGate가 exclusive인 상태에서 획득한다.
//  6. DispatchBatch()는 _dispatchGate(shared) 획득 후 _cqConsumerMutex를 획득한다.
//  7. Worker wake-up은 _dispatchGate 획득 전에 수행한다.
//***************************************************************************
class CRioCore
{
    enum { BATCH_SIZE = 64 };       // 한 번의 배치 처리를 통해 가져올 최대 RIO 완료 결과 개수

public:

    //***************************************************************************
    // @brief CRioCore 엔진의 라이프사이클 상태 정의
    //***************************************************************************
    enum class State : int32
    {
        Uninitialized = 0,    // 초기화되지 않은 기본 상태
        Initializing = 1,     // 초기화 작업이 진행 중인 상태
        Initialized = 2,      // 초기화가 완료되어 실행 대기 중인 상태
        Running = 3,          // 워커 및 I/O 제출이 정상적으로 구동 중인 상태
        Stopping = 4,         // 정지가 요청되어 잔여 I/O를 정리하는 상태
        Stopped = 5,          // 정지가 완전히 완료된 상태
        Faulted = 6,          // 예외 또는 내부 오류로 인해 결함이 발생한 상태
        Closed = 7            // 모든 리소스가 해제되고 완전히 마감된 상태
    };

    //***************************************************************************
    // @brief 이벤트 수거 및 디스패치 모드
    //***************************************************************************
    enum class DispatchMode : int32
    {
        Wait = 0,     // 이벤트가 없을 경우 IOCP 대기 상태(INFINITE)로 진입
        Drain = 1     // 대기 없이 즉시 남아있는 이벤트만 비블로킹 방식으로 수거
    };

    static constexpr int32 kCorruptCq = -1;         // RIO 완료 큐(CQ) 손상 오류 상태 코드
    static constexpr int32 kStopped = -2;           // 엔진이 정지된 상태 코드
    static constexpr int32 kNotifyError = -3;       // RIO 통지(Notify) 등록 실패 오류 코드
    static constexpr int32 kIocpError = -4;         // IOCP 대기 및 알림 관련 오류 코드
    static constexpr int32 kInvalidCompletion = -5; // 유효하지 않은 완료 결과 수신 오류 코드

    static constexpr std::chrono::milliseconds kDefaultDrainTimeout{ 5000 }; // 미완료 I/O 드레인 대기 기본 제한 시간 (5초)

public:

    CRioCore();
    ~CRioCore();

    CRioCore(const CRioCore&) = delete;
    CRioCore& operator=(const CRioCore&) = delete;

    CRioCore(CRioCore&&) = delete;
    CRioCore& operator=(CRioCore&&) = delete;

    //***************************************************************************
    // @brief RIO 함수 테이블 로드 및 CQ/IOCP 오브젝트 초기화
    //***************************************************************************
    bool Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool);

    //***************************************************************************
    // @brief 외부에서 명시적인 정지를 호출할 때 사용하는 인터페이스 함수
    //***************************************************************************
    void RequestStop();

    //***************************************************************************
    // @brief 워커 스레드를 생성하고 실행 상태로 진입합니다.
    //***************************************************************************
    template<typename F>
    bool StartWorker(F&& workerFunc)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( _state.load(std::memory_order_acquire) != State::Initialized )
        {
            return false;
        }

        if( _workerRunning.load(std::memory_order_acquire) || _workerThread.joinable() )
        {
            return false;
        }

        _workerFaulted.store(false, std::memory_order_release);
        _workerThreadId.store(std::thread::id{}, std::memory_order_release);
        _state.store(State::Running, std::memory_order_release);
        _workerRunning.store(true, std::memory_order_release);

        try
        {
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

                    //***************************************************************************
                    // Worker lifecycle의 종료 상태는 worker 자신이 기록한다.
                    // Shutdown()은 join()만 수행한다.
                    //***************************************************************************     
                    _workerRunning.store(false, std::memory_order_release);
                });

            return true;
        }
        catch( ... )
        {
            _workerRunning.store(false, std::memory_order_release);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);
            _state.store(State::Initialized, std::memory_order_release);
            return false;
        }
    }

    //***************************************************************************
    // @brief 완료 큐 배치 처리를 수행하는 퍼블릭 진입점 함수
    //***************************************************************************
    int32 DispatchBatch(DispatchMode mode = DispatchMode::Wait);

    //***************************************************************************
    // @brief 코어 리소스를 해제하고 모든 미완료 I/O가 처리될 때까지 드레인하는 함수
    //***************************************************************************
    ShutdownResult Shutdown(std::chrono::milliseconds drainTimeout = kDefaultDrainTimeout);

    //***************************************************************************
    // @brief I/O 작업을 안전하게 등록(Submission)을 시도합니다.
    //***************************************************************************
    template<typename F>
    bool TrySubmit(F&& submitFunc)
    {
        std::shared_lock<std::shared_mutex> submissionLock(_submissionMutex);

        if( _state.load(std::memory_order_acquire) != State::Running )
        {
            return false;
        }

        if( !IncrementIoCount() )
        {
            return false;
        }

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
    // @brief I/O 허용(Admission)이 차단되었는지 여부를 반환합니다.
    //***************************************************************************
    bool IsAdmissionClosed() const noexcept
    {
        State s = _state.load(std::memory_order_acquire);
        return s == State::Stopping || s == State::Stopped || s == State::Faulted || s == State::Closed;
    }

    //***************************************************************************
    // @brief 현재 코어의 상태를 반환합니다.
    //***************************************************************************
    State GetState() const noexcept
    {
        return _state.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 워커 스레드 오류 발생 여부를 반환합니다.
    //***************************************************************************
    bool IsWorkerFaulted() const noexcept
    {
        return _workerFaulted.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 바인딩된 RIO 확장 함수 테이블 레퍼런스를 반환합니다.
    //***************************************************************************
    const RIO_EXTENSION_FUNCTION_TABLE& GetRioTable() const noexcept
    {
        return _rioTable;
    }

    //***************************************************************************
    // @brief 생성된 RIO 완료 큐 핸들을 반환합니다.
    //***************************************************************************
    RIO_CQ GetCompletionQueue() const noexcept
    {
        return _cq;
    }

    //***************************************************************************
    // @brief 마지막 종료 작업 결과를 반환합니다.
    //***************************************************************************
    ShutdownResult GetLastShutdownResult() const
    {
        std::lock_guard<std::mutex> lock(_lifecycleMutex);
        return _lastShutdownResult;
    }

private:

    int32 _DispatchBatchImpl(DispatchMode mode);

    void StopInternal();
    void FaultInternal() noexcept;

    void DispatchResults(RIORESULT* results, ULONG numResults) noexcept;
    void ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept;

    bool IncrementIoCount() noexcept;
    void DecrementIoCount() noexcept;

    bool TryTransitionState(State from, State to) noexcept
    {
        State expected = from;
        return _state.compare_exchange_strong(expected, to, std::memory_order_acq_rel, std::memory_order_acquire);
    }

private:
    static thread_local CRioCore* _tlsDispatchCore; // 현재 실행 중인 스레드의 CRioCore 인스턴스를 가리키는 스레드 로컬 포인터
    friend class CRioObject;    // CRioObject 클래스에서 CRioCore의 비공개 멤버에 접근할 수 있도록 허용하는 프렌드 선언

private:
    RIO_EXTENSION_FUNCTION_TABLE _rioTable{};         // 마이크로소프트 RIO 확장 함수 포인터 테이블 구조체
    RIO_CQ _cq{ RIO_INVALID_CQ };                     // Windows RIO 완료 큐(Completion Queue) 핸들
    HANDLE _iocpHandle{ NULL };                       // RIO 알림 수신을 위한 IOCP 커널 객체 핸들
    OVERLAPPED _rioOverlapped{};                      // IOCP 통지 바인딩에 사용되는 중첩(Overlapped) 구조체
    RIO_NOTIFICATION_COMPLETION _rioNotification{};   // RIO 완료 알림 방식을 정의하는 설정 구조체
    ULONG_PTR _cqIdentifier{ 0 };                     // IOCP 패킷 무결성 검증을 위한 고유 식별자 키
    CRioEventPool* _eventPool{ nullptr };             // 이벤트 객체 할당 및 관리를 담당하는 메모리 풀 포인터

    std::atomic<State> _state{ State::Uninitialized };             // 코어 엔진의 현재 라이프사이클 상태 (원자적 변수)
    
    // Worker function의 실제 실행 상태.
    // thread 객체의 joinable 여부를 나타내지 않는다.
    // 값의 변경 책임은 worker thread 자신에게 있다.
    // Shutdown()은 이 값을 직접 변경하지 않고 join()만 수행한다.
    std::atomic<bool> _workerRunning{ false };                     
    
    std::atomic<bool> _workerFaulted{ false };                     // 워커 스레드 내부에서 예외나 결함이 발생했는지 여부
    std::atomic<uint32> _outstandingIo{ 0 };                       // 현재 처리 중이거나 미완료된 총 I/O 작업 카운트
    std::atomic<bool> _cqCorrupted{ false };                       // RIO 완료 큐가 손상(Corrupt) 상태에 빠졌는지 여부

    std::thread _workerThread;                                     // 실제 이벤트 폴링을 수행하는 백그라운드 워커 스레드 객체
    mutable std::mutex _lifecycleMutex;                            // 객체 생성, 소멸 및 상태 변경을 보호하는 뮤텍스
    std::shared_mutex _submissionMutex;                            // 새로운 I/O 제출(Submission)과 정지를 동기화하는 공유 뮤텍스
    std::mutex _cqConsumerMutex;                                   // CQ 소비(Dequeue/Notify) 영역을 보호하는 뮤텍스
    std::shared_mutex _dispatchGate;                               // 디스패치 진입 제어를 위한 공유 뮤텍스

    std::atomic<std::thread::id> _workerThreadId{};                // 워커 스레드의 고유 스레드 ID
    std::condition_variable _shutdownCv;                           // 종료 대기 및 통보를 위한 조건 변수
    bool _shutdownInProgress{ false };                             // 종료 프로세스가 진행 중인지 여부
    bool _shutdownDone{ false };                                   // 종료 프로세스가 최종 완료되었는지 여부
    ShutdownResult _lastShutdownResult{ ShutdownResult::Success }; // 가장 최근에 수행된 Shutdown의 결과 상태
};

#endif // __RIOCORE_H__