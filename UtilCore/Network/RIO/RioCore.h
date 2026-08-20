
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

#include <vector>

class CRioEvent;
class CRioEventPool;

//***************************************************************************
// @class CRioCore
// @brief RIO(Registered I/O) 코어 Engine 및 Multi-Worker Completion Dispatcher
//
// @details
//      CRioCore는 Windows RIO(Registered I/O) API의 Completion Queue(CQ) 및
//      IOCP 알림(Notification) 메커니즘을 총괄 관리하는 핵심 코어 클래스입니다.
//
// [주요 기능]
//      1. RIO 함수 테이블(RIO_EXTENSION_FUNCTION_TABLE) 바인딩 및 관리
//      2. Send/Receive Completion Queue(RIO_CQ) 생성 및 파기
//      3. IOCP 기반 Completion 알림 연동 및 Dequeue/Notify 루프 수행
//      4. Multi-Worker Thread 제어 및 Dispatch 동기화
//      5. Outstanding I/O 카운팅 및 안전한 Shutdown/Drain 시퀀스 보장
//
// [멀티 워커 스레드 정책 및 동기화]
//      1. N개의 멀티 워커 스레드 지원.
//         - Initialize(): IOCP Concurrency를 시스템 논리 코어 수로 내부에서
//           자동 계산합니다(별도 파라미터 없음). StartWorkers()가 만드는 실제
//           워커 개수와는 완전히 독립적인 값이며, 워커 수가 이 값보다 많아도
//           안전합니다(초과분은 커널 스케줄러가 컨텍스트 스위칭으로 조절).
//         - StartWorkers(N): N개의 워커 스레드 생성(N==0이면 hardware_concurrency()/2로 자동 지정).
//      2. Recv CQ / Send CQ 락 분리(_recvCqMutex, _sendCqMutex)를 통한 경합 최소화
//      3. WakeWorkers(): 멀티 워커 전체에 IOCP Wake-up(PostQueuedCompletionStatus) 브로드캐스팅.
//         StopInternal()/MarkFaulted() 둘 다 항상 _targetWorkerCount(최종적으로
//         존재할 전체 워커 수) 기준으로 포스팅합니다 — StartWorkers(N) 직후 일부
//         워커가 아직 자기 람다에 진입 전이라 _activeWorkerCount가 실제보다
//         적게 관측되는 좁은 시간대에도, 부족한 wake로 인한 Shutdown() 데드락을
//         방지하기 위함입니다.
//      4. TLS 기반 워커 스레드 주체 검증(_tlsWorkerCore) 및 Dispatcher 콜백 검증(_tlsDispatchCore)
//      5. 데드락 방지 2단계 Shutdown: Lock 밖에서 전체 워커 Join 후 _dispatchGate 독점 획득
//
// [동기화 요약]
//      - Submission Gate: _submissionMutex를 통해 Shutdown 진입 시 신규 I/O 제출 차단
//      - Dispatch Gate: _dispatchGate를 통해 워커 스레드들의 DispatchBatch()와
//        Shutdown()의 리소스 파괴 구간이 서로 겹치지 않도록 상호 배제
//      - CQ Consumer: _recvCqMutex/_sendCqMutex를 각각 사용하여 CQ별 Dequeue/Notify
//        동시 접근을 보호(멀티 워커가 서로 다른 CQ를 동시에 소비할 수 있게 함)
//***************************************************************************
class CRioCore
{
public:
    //***************************************************************************
    // @brief CRioCore 기본 생성자
    // @details 내부 RIO/IOCP 관련 구조체를 0으로 초기화합니다.
    //***************************************************************************
    CRioCore();

    //***************************************************************************
    // @brief CRioCore 소멸자
    // @details 소멸 시 내부적으로 Shutdown()을 호출하여 자원을 안전하게 정리합니다.
    //          최종 상태가 Closed가 아니면 정리가 안 됐다는 뜻이므로 assert 후
    //          std::terminate()로 강제 종료합니다.
    //***************************************************************************
    ~CRioCore();

    CRioCore(const CRioCore&) = delete;
    CRioCore& operator=(const CRioCore&) = delete;
    CRioCore(CRioCore&&) = delete;
    CRioCore& operator=(CRioCore&&) = delete;

public:
    //***************************************************************************
    // @brief RIO Core Engine을 초기화합니다 (RIO 함수 테이블 로드 + CQ/IOCP 생성).
    // @details IOCP Concurrency는 시스템 논리 코어 수로 내부에서 자동 계산되며,
    //          StartWorkers()가 만드는 실제 워커 스레드 개수와는 완전히 독립적인
    //          값입니다(워커 수가 더 많아도 안전).
    // @param socket RIO 함수 테이블 로드 및 바인딩을 위한 소켓
    // @param maxCompletionResults Completion Queue에서 한 번에 처리할 최대 결과 수
    //        (RIO_MAX_CQ_SIZE 이하)
    // @param cqIdentifier CQ 구분 식별자 Tag (하위 2비트는 Receive/Send 태그용으로
    //        예약되어 있으므로 0이어야 함)
    // @param eventPool I/O에 사용될 RIO Event Pool 객체 Pointer
    // @return bool 성공 시 true. 성공 시 상태가 Uninitialized -> Initialized로 전이됨.
    //***************************************************************************
    bool Initialize(SOCKET socket, ULONG maxCompletionResults, ULONG_PTR cqIdentifier, CRioEventPool* eventPool);

    //***************************************************************************
    // @brief 외부에서 RIO Engine 정지를 요청합니다 (비동기 — 즉시 정지를 보장하지
    //        않으며, 워커들이 다음 IOCP wake-up 때 정지를 인지함).
    // @note Dispatch 콜백(CRioObject::Dispatch) 내부에서 호출하면 안 됩니다
    //       (assert 후 무시됨).
    //***************************************************************************
    void RequestStop();

    //***************************************************************************
    // @brief Multi-Worker 스레드 그룹을 생성하고 실행합니다.
    // @details 워커 루프 함수 실행 중 예외가 발생하면 해당 스레드만 FaultInternal()로
    //          결함 처리 후 정상 종료합니다(예외를 스레드 밖으로 내보내지 않음).
    //          스레드 "생성" 자체가 도중에 실패하면(리소스 부족 등) 예외 안전
    //          롤백 시퀀스가 작동하여 이미 생성된 스레드까지 전부 안전하게 정리합니다.
    // @tparam F 실행할 워커 루프 함수 객체 타입
    // @param workerCount 생성할 워커 스레드 수 (0 전달 시 논리 코어 수의 절반으로 자동 지정)
    // @param workerFunc 실행할 워커 루프 람다 또는 Callable 객체
    // @return 스레드 생성 및 실행 성공 시 true, 실패 시 false
    //***************************************************************************
    template<typename F>
    bool StartWorkers(uint32_t workerCount, F&& workerFunc)
    {
        std::unique_lock<std::mutex> lifecycleLock(_lifecycleMutex);

        // 1. 상태 전제조건 확인: Initialize() 완료 상태여야 하고, 이미 워커가
        //    실행 중이거나 남아있으면 거부합니다(재시작/중복 시작 방지).
        if( _state.load(std::memory_order_acquire) != Rio::State::Initialized ) return false;

        if( !_workerThreads.empty() || _activeWorkerCount.load(std::memory_order_acquire) != 0 )
        {
            return false;
        }

        // 2. workerCount가 0일 경우 논리 코어 수의 절반으로 자동 지정
        if( workerCount == 0 )
        {
            workerCount = (std::max)(1u, std::thread::hardware_concurrency() / 2);
        }

        // 3. 워커 시작 전 상태 초기화 및 Initialized -> Running 선전이
        //    (스레드 생성이 실패하면 아래 catch에서 Initialized로 되돌립니다).
        _workerFaulted.store(false, std::memory_order_release);
        _targetWorkerCount.store(workerCount, std::memory_order_release);
        _activeWorkerCount.store(0, std::memory_order_release);

        _workerThreads.clear();
        _workerThreads.reserve(workerCount);

        _state.store(Rio::State::Running, std::memory_order_release);
        _workerRunning.store(true, std::memory_order_release);

        // rvalue callable의 반복적인 move 문제 방지 및 Multi-Worker 스레드 간
        // 안전한 공유를 위해 std::shared_ptr로 감쌉니다.
        using WorkerFunction = std::decay_t<F>;
        auto sharedWorkerFunc = std::make_shared<WorkerFunction>(std::forward<F>(workerFunc));

        try
        {
            // 4. 실제 워커 스레드 N개 생성. 각 스레드는 자신을 TLS에 워커
            //    소속으로 등록한 뒤 사용자 루프(func)를 실행하고, 예외 발생 시
            //    FaultInternal()로 결함 상태 전이 후 정상적으로 스레드를 종료합니다.
            //    마지막 워커가 종료될 때만 _workerRunning을 false로 내립니다.
            for( uint32_t i = 0; i < workerCount; ++i )
            {
                _workerThreads.emplace_back([this, sharedWorkerFunc]() noexcept
                    {
                        CRioCore* previousWorkerCore = _tlsWorkerCore;
                        _tlsWorkerCore = this;

                        _activeWorkerCount.fetch_add(1, std::memory_order_acq_rel);

                        try
                        {
                            (*sharedWorkerFunc)();
                        }
                        catch( ... )
                        {
                            FaultInternal();
                        }

                        _tlsWorkerCore = previousWorkerCore;

                        const uint32_t remaining = _activeWorkerCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
                        if( remaining == 0 )
                        {
                            _workerRunning.store(false, std::memory_order_release);
                            _shutdownCv.notify_all();
                        }
                    });
            }

            return true;
        }
        catch( ... )
        {
            // 5. 스레드 생성 자체가 실패한 경우(리소스 부족 등) — 실제로 생성에
            //    성공한 스레드 수만큼만 Wake-up 포스팅하여 orphan 패킷을 방지하고,
            //    그 스레드들을 Join한 뒤 상태를 Running 이전(Initialized)으로
            //    되돌려 재시도 가능하게 합니다.
            _state.store(Rio::State::Stopping, std::memory_order_release);

            const uint32_t createdWorkerCount = static_cast<uint32_t>(_workerThreads.size());
            WakeWorkers(createdWorkerCount);

            std::vector<std::thread> threadsToJoin = std::move(_workerThreads);

            lifecycleLock.unlock();

            for( auto& thread : threadsToJoin )
            {
                if( thread.joinable() ) thread.join();
            }

            lifecycleLock.lock();

            _workerThreads.clear();
            _activeWorkerCount.store(0, std::memory_order_release);
            _targetWorkerCount.store(0, std::memory_order_release);
            _workerRunning.store(false, std::memory_order_release);
            _state.store(Rio::State::Initialized, std::memory_order_release);

            return false;
        }
    }

    //***************************************************************************
    // @brief Completion Queue에서 완료 이벤트를 하나(또는 대기 후 하나) 처리합니다.
    // @param mode Wait: 처리할 게 없으면 IOCP에서 블로킹 대기(이 CRioCore 소속
    //             워커 스레드 전용). Drain: 블로킹 대기 없이 잔여 이벤트만 즉시
    //             처리하고 반환(Shutdown()의 드레인 루프 전용).
    // @return int32 처리된 completion 개수(>=0), 또는 Rio::k* 에러 코드(<0)
    //***************************************************************************
    int32 DispatchBatch(Rio::DispatchMode mode = Rio::DispatchMode::Wait);

    //***************************************************************************
    // @brief RIO Core Engine을 안전하게 종료합니다 (워커 전체 Join → outstanding
    //        I/O 드레인 → CQ/IOCP 파괴).
    // @param drainTimeout 드레인 대기 제한시간
    // @return Rio::ShutdownResult 종료 결과(Success면 이후 상태는 Closed)
    // @note Dispatch 콜백 내부에서 호출할 수 없습니다.
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
    // @tparam F I/O 제출 실행 함수 타입
    // @param submitFunc 실행할 I/O 제출 함수
    // @return 제출 성공 시 true, 실패 시 false
    //***************************************************************************
    template<typename F>
    bool TrySubmit(F&& submitFunc)
    {
        return SubmitIo(std::forward<F>(submitFunc));
    }

    //***************************************************************************
    // @brief 로드된 RIO 함수 테이블의 참조를 반환합니다.
    // @return const RIO_EXTENSION_FUNCTION_TABLE& RIO 함수 테이블 참조
    //***************************************************************************
    const RIO_EXTENSION_FUNCTION_TABLE& GetRioTable() const noexcept { return _rioTable; }

    //***************************************************************************
    // @brief Receive Completion Queue 핸들을 반환합니다.
    // @return RIO_CQ 수신 CQ 핸들
    //***************************************************************************
    RIO_CQ GetReceiveQueue() const noexcept { return _receiveCq; }

    //***************************************************************************
    // @brief Send Completion Queue 핸들을 반환합니다.
    // @return RIO_CQ 송신 CQ 핸들
    //***************************************************************************
    RIO_CQ GetSendQueue() const noexcept { return _sendCq; }

    //***************************************************************************
    // @brief 바인딩된 이벤트 풀 포인터를 반환합니다.
    // @return CRioEventPool* 이벤트 풀 포인터
    //***************************************************************************
    CRioEventPool* GetEventPool() const noexcept { return _eventPool; }

    //***************************************************************************
    // @brief 현재 신규 I/O 제출이 가능한 상태인지 확인합니다.
    // @return bool Running 상태이면 true
    //***************************************************************************
    bool CanSubmitIo() const noexcept { return _state.load(std::memory_order_acquire) == Rio::State::Running; }

    //***************************************************************************
    // @brief 신규 I/O 진입(Admission)이 차단되었는지 확인합니다.
    // @return bool Stopping/Stopped/Faulted/Closed 중 하나이면 true
    //***************************************************************************
    bool IsAdmissionClosed() const noexcept
    {
        const Rio::State state = _state.load(std::memory_order_acquire);
        return state == Rio::State::Stopping || state == Rio::State::Stopped || state == Rio::State::Faulted || state == Rio::State::Closed;
    }

    //***************************************************************************
    // @brief 현재 CRioCore의 상태를 반환합니다.
    // @return Rio::State 엔진 현재 상태 값
    //***************************************************************************
    Rio::State GetState() const noexcept { return _state.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief Worker 스레드에 결함(Fault)이 발생했는지 확인합니다.
    // @return bool 결함 발생 시 true
    //***************************************************************************
    bool IsWorkerFaulted() const noexcept { return _workerFaulted.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief Receive CQ 손상 여부를 반환합니다.
    // @return bool 손상 시 true
    //***************************************************************************
    bool IsReceiveCqCorrupted() const noexcept { return _receiveCqCorrupted.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief Send CQ 손상 여부를 반환합니다.
    // @return bool 손상 시 true
    //***************************************************************************
    bool IsSendCqCorrupted() const noexcept { return _sendCqCorrupted.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 어느 하나라도 CQ 손상이 발생했는지 확인합니다.
    // @return bool 하나라도 손상 시 true
    //***************************************************************************
    bool IsCqCorrupted() const noexcept { return _receiveCqCorrupted.load(std::memory_order_acquire) || _sendCqCorrupted.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 현재 처리 중인 Outstanding I/O 개수를 반환합니다.
    // @return uint32_t 진행 중인 I/O 개수
    //***************************************************************************
    uint32_t GetOutstandingIoCount() const noexcept { return _outstandingIo.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 현재 구동 중인 활성 워커 스레드 수량을 반환합니다.
    // @return uint32_t 활성 워커 수
    //***************************************************************************
    uint32_t GetActiveWorkerCount() const noexcept { return _activeWorkerCount.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 설정된 목표 워커 스레드 수량을 반환합니다.
    // @return uint32_t 목표 워커 수
    //***************************************************************************
    uint32_t GetTargetWorkerCount() const noexcept { return _targetWorkerCount.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 마지막 Shutdown 결과를 반환합니다.
    // @return Rio::ShutdownResult 셧다운 결과 코드
    //***************************************************************************
    Rio::ShutdownResult GetLastShutdownResult() const
    {
        std::lock_guard<std::mutex> lock(_lifecycleMutex);
        return _lastShutdownResult;
    }

private:
    //***************************************************************************
    // @brief DispatchBatch의 실제 내부 구현부입니다.
    // @param mode 디스패치 모드
    // @return 처리된 완료 이벤트 개수 또는 에러 코드
    //***************************************************************************
    int32 _DispatchBatchImpl(Rio::DispatchMode mode);

    //***************************************************************************
    // @brief 내부 정지 로직을 수행합니다.
    // @details 제출 권한을 닫고 _targetWorkerCount 기준으로 WakeWorkers를 호출합니다.
    //***************************************************************************
    void StopInternal();

    //***************************************************************************
    // @brief 내부 예외/결함 상태를 설정합니다.
    //***************************************************************************
    void FaultInternal() noexcept;

    //***************************************************************************
    // @brief 대기 중인 멀티 워커 스레드 전체에 IOCP Wake-up(Stop) 패킷을 포스팅합니다.
    // @param workerCount 깨울 목표 워커 스레드 수
    //***************************************************************************
    void WakeWorkers(uint32_t workerCount) noexcept;

    //***************************************************************************
    // @brief 꺼내온 RIO 결과를 순회하며 각 이벤트를 처리(Dispatch)합니다.
    // @param cqType CQ 타입 (Receive/Send)
    // @param results RIORESULT 배열
    // @param numResults 결과 개수
    // @return 성공적으로 처리된 개수 또는 에러 코드
    //***************************************************************************
    int32 DispatchResults(Rio::RioCqType cqType, RIORESULT* results, ULONG numResults) noexcept;

    //***************************************************************************
    // @brief 개별 RIO 결과를 처리하고 해당 이벤트와 리소스를 반환/디스패치합니다.
    // @param cqType CQ 타입
    // @param status RIO 완료 상태
    // @param bytesTransferred 전송된 바이트 수
    // @param rioEvent 디스패치할 RIO Event 객체 Pointer
    //***************************************************************************
    void ProcessRioResult(Rio::RioCqType cqType, LONG status, ULONG bytesTransferred, CRioEvent* rioEvent) noexcept;

    //***************************************************************************
    // @brief 진행 중인 I/O 카운터를 원자적으로 1 증가시킵니다.
    // @return 증가 성공 여부
    //***************************************************************************
    bool IncrementIoCount() noexcept;

    //***************************************************************************
    // @brief 진행 중인 I/O 카운터를 원자적으로 1 감소시킵니다.
    //***************************************************************************
    void DecrementIoCount() noexcept;

    //***************************************************************************
    // @brief Engine의 결함 상태 및 CQ Corrupt 상태를 설정합니다.
    // @param receiveCqCorrupt Receive CQ 손상 여부
    // @param sendCqCorrupt Send CQ 손상 여부
    //***************************************************************************
    void MarkFaulted(bool receiveCqCorrupt, bool sendCqCorrupt) noexcept;

    //***************************************************************************
    // @brief _state를 from -> to로 원자적으로 전이 시도합니다(CAS 1회).
    // @param from 기존 기대 상태
    // @param to 변경할 목표 상태
    // @return 전환 성공 시 true
    //***************************************************************************
    bool TryTransitionState(Rio::State from, Rio::State to) noexcept
    {
        Rio::State expected = from;
        return _state.compare_exchange_strong(expected, to, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Completion Queue에서 완료 결과를 꺼내옵니다.
    // @param cq 타겟 Completion Queue Handle
    // @param results 결과를 저장할 RIORESULT 배열 Pointer
    // @param numResults [out] 디큐된 결과 개수
    // @return 성공 여부
    //***************************************************************************
    bool DrainCompletionQueue(RIO_CQ cq, RIORESULT* results, ULONG& numResults) noexcept;

    //***************************************************************************
    // @brief Completion Queue의 알림을 요청합니다.
    // @param cq 타겟 Completion Queue Handle
    // @return 성공 여부
    //***************************************************************************
    bool NotifyCompletionQueue(RIO_CQ cq) noexcept;

    //***************************************************************************
    // @brief 올바른 Completion 패킷인지 검증합니다.
    // @param completionKey Completion Key
    // @param overlapped Overlapped 구조체 Pointer
    // @return 유효성 여부
    //***************************************************************************
    bool IsValidCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;

    //***************************************************************************
    // @brief Stop 요청 패킷인지 확인합니다.
    // @param completionKey Completion Key
    // @param overlapped Overlapped 구조체 Pointer
    // @return Stop 패킷 여부
    //***************************************************************************
    bool IsStopPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;

    //***************************************************************************
    // @brief Receive 완료 패킷인지 확인합니다.
    // @param completionKey Completion Key
    // @param overlapped Overlapped 구조체 Pointer
    // @return Receive 패킷 여부
    //***************************************************************************
    bool IsReceiveCompletionPacket(ULONG_PTR completionKey, LPOVERLAPPED overlapped) const noexcept;

    //***************************************************************************
    // @brief Send 완료 패킷인지 확인합니다.
    // @param completionKey Completion Key
    // @param overlapped Overlapped 구조체 Pointer
    // @return Send 패킷 여부
    //***************************************************************************
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

        //***************************************************************************
        // @brief 스코프 종료 시 TLS 바인딩 포인터를 이전 상태로 복원합니다.
        //***************************************************************************
        ~TlsDispatchGuard() noexcept { slot = previous; }
    };

    //***************************************************************************
    // @struct OutstandingIoGuard
    // @brief Outstanding I/O 카운트 자동 감축용 RAII 가드
    //***************************************************************************
    struct OutstandingIoGuard
    {
        CRioCore* core;

        //***************************************************************************
        // @brief 스코프 탈출 시 core->DecrementIoCount()를 안전하게 호출합니다.
        //***************************************************************************
        ~OutstandingIoGuard() noexcept
        {
            if( core != nullptr ) core->DecrementIoCount();
        }
    };

    //***************************************************************************
    // @struct ObjectIoCountGuard
    // @brief CRioObject I/O 카운트 자동 감축용 RAII 가드. Dispatch() 반환 이후
    //        스코프 종료 시 해당 객체의 I/O 참조 카운트를 감소시킵니다. 호출자
    //        (CRioObject 구현체)는 이 카운트를 직접 건드리면 안 됩니다.
    //***************************************************************************
    struct ObjectIoCountGuard
    {
        CRioObject* object;

        //***************************************************************************
        // @brief 스코프 탈출 시 object->DecrementIoCount()를 안전하게 호출합니다.
        //***************************************************************************
        ~ObjectIoCountGuard() noexcept
        {
            if( object != nullptr ) object->DecrementIoCount();
        }
    };

private:
    static thread_local CRioCore* _tlsDispatchCore; // 현재 스레드의 Dispatch Core TLS 포인터
    static thread_local CRioCore* _tlsWorkerCore;   // 현재 스레드가 소속된 워커 Core TLS 포인터

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
    std::atomic<bool> _workerRunning{ false };                   // Worker 실행 여부(전체 워커 중 1개 이상 실행 중)
    std::atomic<bool> _workerFaulted{ false };                   // Worker Fault 상태

    std::atomic<uint32_t> _outstandingIo{ 0 }; // 현재 진행 중인 Outstanding I/O 개수

    std::atomic<bool> _receiveCqCorrupted{ false }; // Receive CQ Corrupt 손상 플래그
    std::atomic<bool> _sendCqCorrupted{ false };    // Send CQ Corrupt 손상 플래그

    std::vector<std::thread> _workerThreads;       // 생성된 멀티 워커 스레드 객체 리스트
    std::atomic<uint32_t> _activeWorkerCount{ 0 }; // 현재 구동 중인(자기 람다에 진입 완료한) 워커 스레드 수량
    std::atomic<uint32_t> _targetWorkerCount{ 0 }; // StartWorkers()가 만들기로 한 목표 워커 스레드 수량

    mutable std::mutex _lifecycleMutex; // Lifecycle 동기화 Mutex
    std::shared_mutex _submissionMutex; // Submission 동기화 Shared Mutex

    std::mutex _recvCqMutex; // Receive CQ 전용 Dequeue/Notify Mutex
    std::mutex _sendCqMutex; // Send CQ 전용 Dequeue/Notify Mutex

    std::shared_mutex _dispatchGate; // Dispatch Gate Shared Mutex

    std::condition_variable _shutdownCv; // Shutdown 대기 조건 변수

    bool _shutdownInProgress{ false }; // Shutdown 진행 중 플래그
    bool _shutdownDone{ false };       // Shutdown 완료 플래그

    Rio::ShutdownResult _lastShutdownResult{ Rio::ShutdownResult::Success }; // 마지막 Shutdown 결과
};

#endif // ndef __RIOCORE_H__