
//**********************************************************************************************************************
// RioCore.h : interface for the CRioCore class.
//
//**********************************************************************************************************************

#ifndef __RIOCORE_H__
#define __RIOCORE_H__

#ifndef __RIOCOMMON_H__
#include <Network/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

#ifndef __RIOEVENTPOOL_H__
#include <Network/RioEventPool.h>
#endif

#ifndef __RIOOBJECT_H__
#include <Network/RioObject.h>
#endif

class CRioEvent;
class CRioEventPool;
class CRioObject;

using CRioObjectRef = std::shared_ptr<CRioObject>;

//**********************************************************************************************************************
// @brief RIO 코어 엔진 핵심 클래스
// @details 완료 큐(CQ), IOCP 바인딩, 워커 스레드 제어 및 I/O 동기화를 관리합니다.
//
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
//     _lifecycleMutex
//         -> _dispatchGate     // FORBIDDEN
//
//     _dispatchGate
//         -> _lifecycleMutex   // FORBIDDEN
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
//**********************************************************************************************************************
class CRioCore
{
public:
    CRioCore();
    ~CRioCore();

    CRioCore(const CRioCore&) = delete;
    CRioCore& operator=(const CRioCore&) = delete;

    CRioCore(CRioCore&&) = delete;
    CRioCore& operator=(CRioCore&&) = delete;

    bool Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool);
    void RequestStop();

    //**********************************************************************************************************************
    // @brief 워커 스레드를 생성하고 실행 상태로 진입합니다.
    // @tparam F 워커 스레드 메인 루프 함수/람다 타입
    // @param workerFunc 실행할 워커 스레드 함수
    // @return 워커 스레드 구동 성공 시 true, 이미 구동 중이거나 상태 불일치 시 false
    //**********************************************************************************************************************
    template<typename F>
    bool StartWorker(F&& workerFunc)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        if( _state.load(std::memory_order_acquire) != Rio::State::Initialized )
        {
            return false;
        }

        if( _workerRunning.load(std::memory_order_acquire) || _workerThread.joinable() )
        {
            return false;
        }

        _workerFaulted.store(false, std::memory_order_release);
        _workerThreadId.store(std::thread::id{}, std::memory_order_release);
        _state.store(Rio::State::Running, std::memory_order_release);
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

                    _workerRunning.store(false, std::memory_order_release);
                });

            return true;
        }
        catch( ... )
        {
            _workerRunning.store(false, std::memory_order_release);
            _workerThreadId.store(std::thread::id{}, std::memory_order_release);
            _state.store(Rio::State::Initialized, std::memory_order_release);
            return false;
        }
    }

    int32 DispatchBatch(Rio::DispatchMode mode = Rio::DispatchMode::Wait);
    Rio::ShutdownResult Shutdown(std::chrono::milliseconds drainTimeout = Rio::kDefaultDrainTimeout);

    //**********************************************************************************************************************
    // @brief I/O 작업을 안전하게 등록(Submission)합니다.
    //
    // @details
    // Submission lock을 획득한 상태에서 현재 State를 검사하고
    // CRioCore outstanding I/O count를 증가시킨 후 실제 RIO API를 호출합니다.
    //
    // submitFunc()가 false를 반환하거나 예외가 발생하면
    // outstanding I/O count를 자동으로 Rollback합니다.
    //
    // 중요:
    // submitFunc() 내부에서는 RIO API 호출만 수행하고
    // owner/event lifetime 관리는 호출자가 담당합니다.
    //
    // @tparam F RIO API 제출을 수행하는 람다/함수 객체 타입
    // @param submitFunc 실행할 제출 함수 (bool 반환)
    // @return 제출 성공 시 true, 제출 실패 또는 State 불일치 시 false
    //**********************************************************************************************************************
    template<typename F>
    bool SubmitIo(F&& submitFunc)
    {
        std::shared_lock<std::shared_mutex> submissionLock(_submissionMutex);

        if( _state.load(std::memory_order_acquire) != Rio::State::Running )
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

    //**********************************************************************************************************************
    // @brief 기존 TrySubmit 호환 인터페이스
    // @tparam F RIO API 제출을 수행하는 람다/함수 객체 타입
    // @param submitFunc 실행할 제출 함수
    // @return 제출 성공 여부
    //**********************************************************************************************************************
    template<typename F>
    bool TrySubmit(F&& submitFunc)
    {
        return SubmitIo(std::forward<F>(submitFunc));
    }

    //**********************************************************************************************************************
    // @brief RIO 확장 함수 테이블을 반환합니다.
    // @return RIO_EXTENSION_FUNCTION_TABLE 참조
    //**********************************************************************************************************************
    const RIO_EXTENSION_FUNCTION_TABLE& GetRioTable() const noexcept
    {
        return _rioTable;
    }

    //**********************************************************************************************************************
    // @brief RIO 완료 큐 핸들을 반환합니다.
    // @return RIO_CQ 핸들
    //**********************************************************************************************************************
    RIO_CQ GetCompletionQueue() const noexcept
    {
        return _cq;
    }

    //**********************************************************************************************************************
    // @brief 현재 Core가 새로운 I/O Submission을 허용하는지 확인합니다.
    // @return 제출 가능 상태(State::Running)일 경우 true, 그 외 false
    //**********************************************************************************************************************
    bool CanSubmitIo() const noexcept
    {
        return _state.load(std::memory_order_acquire) == Rio::State::Running;
    }

    //**********************************************************************************************************************
    // @brief I/O Admission이 차단되었는지 여부를 반환합니다.
    // @return Stopping, Stopped, Faulted, Closed 상태일 경우 true
    //**********************************************************************************************************************
    bool IsAdmissionClosed() const noexcept
    {
        Rio::State s = _state.load(std::memory_order_acquire);
        return s == Rio::State::Stopping || s == Rio::State::Stopped || s == Rio::State::Faulted || s == Rio::State::Closed;
    }

    //**********************************************************************************************************************
    // @brief 현재 코어의 상태를 반환합니다.
    // @return CRioCore::State 원자적 상태 값
    //**********************************************************************************************************************
    Rio::State GetState() const noexcept
    {
        return _state.load(std::memory_order_acquire);
    }

    //**********************************************************************************************************************
    // @brief 워커 스레드 오류 발생 여부를 반환합니다.
    // @return 예외 및 결함 발생 시 true
    //**********************************************************************************************************************
    bool IsWorkerFaulted() const noexcept
    {
        return _workerFaulted.load(std::memory_order_acquire);
    }

    //**********************************************************************************************************************
    // @brief 마지막 종료 작업 결과를 반환합니다.
    // @return ShutdownResult 결과 상태
    //**********************************************************************************************************************
    Rio::ShutdownResult GetLastShutdownResult() const
    {
        std::lock_guard<std::mutex> lock(_lifecycleMutex);
        return _lastShutdownResult;
    }

private:
    int32 _DispatchBatchImpl(Rio::DispatchMode mode);

    void StopInternal();
    void FaultInternal() noexcept;
    int32 DispatchResults(RIORESULT* results, ULONG numResults) noexcept;
    void ProcessRioResult(LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept;
    bool IncrementIoCount() noexcept;
    void DecrementIoCount() noexcept;
    void MarkFaulted(bool corruptCq) noexcept;

    //**********************************************************************************************************************
    // @brief CAS 연산을 통해 코어 라이프사이클 상태 전이를 시도합니다.
    // @param from 변경 전 기대 상태
    // @param to 변경할 목표 상태
    // @return 전이 성공 시 true, 실패 시 false
    //**********************************************************************************************************************
    bool TryTransitionState(Rio::State from, Rio::State to) noexcept
    {
        Rio::State expected = from;
        return _state.compare_exchange_strong(expected, to, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    //**********************************************************************************************************************
    // @brief DispatchBatch()/Shutdown() 드레인 루프 실행 중 스레드 로컬
    //        dispatch core 포인터를 관리하는 RAII Guard
    //**********************************************************************************************************************
    struct TlsDispatchGuard
    {
        CRioCore*& slot; // 복구 대상 스레드 로컬 디스패치 슬롯 참조
        CRioCore* previous; // 이전 디스패치 코어 인스턴스 주소

        ~TlsDispatchGuard() noexcept
        {
            slot = previous;
        }
    };

    //**********************************************************************************************************************
    // @brief ProcessRioResult()가 어떤 경로로 종료되더라도 CRioCore의
    //        outstanding I/O 카운터가 정확히 한 번 감소하도록 보장하는 RAII Guard
    //**********************************************************************************************************************
    struct OutstandingIoGuard
    {
        CRioCore* core; // I/O 카운트를 감축할 대상 CRioCore 주소

        ~OutstandingIoGuard() noexcept
        {
            if( core != nullptr )
            {
                core->DecrementIoCount();
            }
        }
    };

    //**********************************************************************************************************************
    // @brief CRioObject::Dispatch() 반환 이후에만 해당 객체의 IoCount가
    //        감소하도록 스코프를 제한하는 RAII 가드.
    //
    // @note
    // CRioObject::DecrementIoCount()가 void를 반환하므로
    // 반환값을 bool로 저장하지 않는다.
    //**********************************************************************************************************************
    struct ObjectIoCountGuard
    {
        CRioObject* object; // I/O 완료 알림을 수행할 대상 CRioObject 주소

        ~ObjectIoCountGuard() noexcept
        {
            if( object == nullptr )
            {
                return;
            }

            object->DecrementIoCount();
        }
    };

private:
    static thread_local CRioCore* _tlsDispatchCore; // 현재 스레드에서 디스패치 루프를 수행 중인 CRioCore 객체 포인터

    friend class CRioObject;

private:
    RIO_EXTENSION_FUNCTION_TABLE _rioTable{};           // Winsock RIO 함수 포인터 모음 테이블
    RIO_CQ _cq{ RIO_INVALID_CQ };                       // RIO 완료 큐(Completion Queue) 핸들
    HANDLE _iocpHandle{ NULL };                         // RIO_NOTIFICATION_COMPLETION과 연동되는 IOCP 핸들
    OVERLAPPED _rioOverlapped{};                        // RIONotify에 인자로 전달되는 비동기 OVERLAPPED 구조체
    RIO_NOTIFICATION_COMPLETION _rioNotification{};     // IOCP 통지 바인딩 정보 구조체
    ULONG_PTR _cqIdentifier{ 0 };                       // IOCP 패킷 검증에 활용되는 완료 키 식별자
    CRioEventPool* _eventPool{ nullptr };               // 완료 이벤트를 반납/재사용하기 위한 풀 포인터

    std::atomic<Rio::State>  _state{ Rio::State::Uninitialized }; // 코어 라이프사이클 원자적 상태
    std::atomic<bool>   _workerRunning{ false };        // 워커 스레드 구동 여부 플래그

    std::atomic<bool>   _workerFaulted{ false };        // 디스패치/콜백 처리 중 오류 발생 플래그
    std::atomic<uint32> _outstandingIo{ 0 };            // 진행 중인(미완료된) RIO I/O 카운트
    std::atomic<bool>   _cqCorrupted{ false };          // CQ 오염(RIO_CORRUPT_CQ) 발생 여부 플래그

    std::thread _workerThread; // CQ 완료 수거 및 디스패치를 구동하는 워커 스레드 객체

    mutable std::mutex  _lifecycleMutex;    // 초기화, 정지, 종료 등 제어 상태 상호 배타 락
    std::shared_mutex   _submissionMutex;   // I/O 제출과 정지 단계 간 동기화를 위한 공유 락
    std::mutex          _cqConsumerMutex;   // CQ Dequeue 및 Notify 호출 독점 보장 뮤텍스
    std::shared_mutex   _dispatchGate;      // 디스패치 수행과 Shutdown 드레인 간 구획 락

    std::atomic<std::thread::id> _workerThreadId{}; // 디스패치를 수행하는 워커 스레드의 고유 ID

    std::condition_variable _shutdownCv; // Shutdown 완수 대기를 위한 동기화 조건 변수

    bool _shutdownInProgress{ false }; // 현재 종료 수순이 진행 중인지 여부
    bool _shutdownDone{ false }; // 종료 수순 및 드레인이 최종 완료되었는지 여부

    Rio::ShutdownResult _lastShutdownResult{ Rio::ShutdownResult::Success }; // 마지막으로 수행된 Shutdown 결과 저장용
};

#endif // __RIOCORE_H__