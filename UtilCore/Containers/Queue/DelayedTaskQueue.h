
//***************************************************************************
// DelayedTaskQueue.h : interface for the CDelayedTaskQueue class.
//
//***************************************************************************

#ifndef UC_DELAYEDTASKQUEUE_H
#define UC_DELAYEDTASKQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>

//***************************************************************************
// @struct DelayedTask
// @brief 지연 실행될 작업과 실행 시점을 담는 구조체
// @note 일정 시간 뒤에 패킷을 재전송하거나 타임아웃을 처리해야 하는 네트워크 세션 관리, 
//       게임 내 예약 이벤트(예: 아이템 소멸, 쿨타임 종료), 주기적인 헬스 체크나 타이머 기반 백그라운드 작업 등 
//       지연 실행(Delayed Execution)이 필요한 곳에 사용하면 좋습니다.
//          - FcmPushAgent 등 예약 발송이 필요한 푸시 스케줄러에서 특정 시각에 발송 작업을 실행하는 트리거
//          - CAdoConnPool, COdbcConnPool의 백그라운드 재연결 로직에서 지수 백오프(exponential backoff) 재시도 지연 실행
//          - 게임 서버에서 버프 / 디버프 만료, 스킬 쿨다운 종료, 리스폰 타이머 등 시간 기반 이벤트 처리
//          - 세션, 커넥션 idle timeout 감지(일정 시간 뒤에도 갱신되지 않으면 종료 처리 작업을 실행)
//***************************************************************************
struct DelayedTask
{
    std::chrono::steady_clock::time_point ExecuteTime; // 작업이 실행되어야 할 절대 시간
    std::function<void()> Task;                        // 실행할 콜백 함수

    //***************************************************************************
    // @brief 최소 힙(Min-Heap) 구성을 위한 비교 연산자 (실행 시간이 빠른 것이 우선순위가 높음)
    // @note 실행 시간이 동일한 task 간의 상대적 순서는 보장하지 않습니다.
    //       순서 보장이 필요하다면 시퀀스 번호를 추가해야 합니다.
    //***************************************************************************
    bool operator>(const DelayedTask& other) const
    {
        return ExecuteTime > other.ExecuteTime;
    }
};

// DelayedTask의 move 생성자가 예외를 던지지 않음을 보장합니다.
// ProcessExpiredTasks()에서 _queue.top()으로부터 move하는 도중 예외가 발생하면
// 원본 객체(큐에 남아있는 슬롯)가 부분적으로 move된 상태로 오염될 수 있으므로,
// 이 타입은 반드시 nothrow move 가능해야 합니다.
static_assert(std::is_nothrow_move_constructible_v<DelayedTask>,
    "DelayedTask must be nothrow move constructible: ProcessExpiredTasks() moves "
    "out of the priority_queue's top element, and a throwing move would leave "
    "that slot in a corrupted state before pop() removes it.");

//***************************************************************************
// @class CDelayedTaskQueue
// @brief 특정 시간 이후에 실행되어야 하는 작업을 관리하는 타임머신 형태의 지연 예약 큐.
//
// @details
// 내부적으로 std::priority_queue(최소 힙)와 조건 변수를 사용하여,
// 예약된 시각에 도달한 작업을 효율적으로 실행합니다.
// 네트워크 세션 관리, 게임 서버 이벤트, 주기적 헬스 체크 등
// 지연 실행(Delayed Execution)이 필요한 곳에 적합합니다.
//
// 주요 사용처 및 이점:
//  - 네트워크 패킷 재전송, 타임아웃 처리
//  - 게임 서버의 버프/디버프 만료, 스킬 쿨다운 종료, 리스폰 타이머
//  - 예약 발송이 필요한 푸시 스케줄러(FcmPushAgent 등)
//  - DB 커넥션 풀의 재연결 로직(지수 백오프 재시도)
//  - 세션/커넥션 idle timeout 감지
//
// 동시성 모델:
//  - **MPMC(Multiple Producer, Multiple Consumer)**: Reserve()는 mutex로 보호되므로
//    여러 스레드가 동시에 예약을 넣을 수 있고, ProcessExpiredTasks() 역시 여러 스레드에서
//    동시에 호출할 수 있습니다.
//  - 다만 callback은 lock 밖에서 실행되므로, 여러 consumer가 동시에 실행될 경우 dequeue
//    순서는 ExecuteTime 순이더라도 실제 callback 시작/완료 순서는 ExecuteTime 순서를 보장하지
//    않습니다. ExecuteTime 순서대로 callback이 실행되어야 하는 용도라면 ProcessExpiredTasks()
//    를 단일 스레드(Single Consumer)에서만 호출하십시오.
//
// Lifetime 계약:
//  - 이 클래스는 워커 스레드를 소유하지 않습니다. ProcessExpiredTasks()를 실행 중인 스레드가
//    있다면, 호출자는 반드시 Stop() 호출 후 해당 스레드를 join()한 뒤에 이 객체를 파괴해야
//    합니다. Stop()은 루프 종료 신호만 보낼 뿐 스레드 종료를 기다리지 않으므로(Stop() != Join),
//    Stop() 호출 직후 객체를 파괴하면 실행 중이던 ProcessExpiredTasks()가 이미 소멸된 멤버에
//    접근하는 미정의 동작(UB)이 발생할 수 있습니다.
//
// Stop() 정책:
//  - Stop()은 pending task(만료 여부 무관)를 전부 폐기합니다. graceful drain이
//    아닙니다. 종료 시 예약된 task를 반드시 실행해야 하는 용도라면 이 클래스를
//    그대로 쓰지 말고 별도 drain 로직을 추가하십시오.
//***************************************************************************
class CDelayedTaskQueue
{
public:
    CDelayedTaskQueue() = default;
    ~CDelayedTaskQueue() = default;

    //***************************************************************************
    // @brief 일정 시간 뒤에 실행될 작업을 예약합니다.
    // @tparam F 람다식 또는 함수 객체 타입
    // @param delay 현재 시점부터 경과해야 할 시간
    // @param task 시간이 되었을 때 실행할 작업 함수
    // @return true: 예약 성공, false: Stop() 이후라 거부됨
    // @note DelayedTask(std::function 포함) 구성은 락 밖에서 수행합니다. std::function이
    //       캡처 크기에 따라 힙 할당을 일으킬 수 있는데, 이를 _mutex 보유 구간 밖으로
    //       빼내어 락 보유 시간을 최소화합니다. 락 안에서는 완성된 DelayedTask를
    //       move만 합니다(nothrow 보장은 위 static_assert 참고).
    //***************************************************************************
    template<typename F>
    bool Reserve(std::chrono::milliseconds delay, F&& task)
    {
        DelayedTask newTask{ std::chrono::steady_clock::now() + delay, std::forward<F>(task) };

        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped )
                return false;
            _queue.push(std::move(newTask));
        }
        _cv.notify_one(); // 대기 중인 스레드 깨우기
        return true;
    }

    //***************************************************************************
    // @brief 시간에 도달한 작업들을 순차적으로 꺼내어 실행합니다.
    // @note 아직 시간이 되지 않은 작업은 남은 시간만큼 조건 변수로 효율적으로 대기합니다.
    //       Stop()이 호출되면 대기/처리 루프를 종료하고 반환합니다. 단, 이미 큐에서 꺼내어
    //       실행 중인 callback은 중단되지 않고 끝까지 실행됩니다.
    //***************************************************************************
    void ProcessExpiredTasks()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        while( !_stopped )
        {
            if( _queue.empty() )
            {
                // 큐가 빌 경우 중지 신호나 새로운 예약이 들어올 때까지 대기
                _cv.wait(lock, [this] {
                    return _stopped || !_queue.empty();
                    });
                if( _stopped )
                    return;
            }

            auto now = std::chrono::steady_clock::now();
            const auto capturedExecuteTime = _queue.top().ExecuteTime;

            if( capturedExecuteTime > now )
            {
                // predicate가 _stopped만 검사하면, 대기 도중 더 이른 마감시간을 가진
                // task가 Reserve()로 새로 들어와도 notify_one()이 predicate를 깨우지
                // 못해(계속 false) 원래 capturedExecuteTime까지 그대로 재대기하게 된다.
                // 즉 새로 예약된 task가 기존 top보다 빨리 실행되어야 함에도 늦게 처리되는
                // 버그가 생긴다. top이 바뀌었는지(=더 이른 task가 들어왔는지)도 함께
                // predicate에 포함시켜, 그 경우 즉시 깨어나 while 루프에서 top을 다시
                // 평가하도록 한다.
                _cv.wait_until(lock, capturedExecuteTime, [this, capturedExecuteTime] {
                    return _stopped
                        || _queue.empty()
                        || _queue.top().ExecuteTime != capturedExecuteTime;
                    });
                if( _stopped )
                    return;
                continue;
            }

            // 만료된 작업을 하나만 꺼내어 언락 후 실행합니다.
            // 이전에는 만료된 task들을 CVector에 일괄 수집(batch drain)한 뒤 락을 풀고
            // 순회 실행했으나, "pop 이후 push_back()"의 순서 특성상 push_back()의 메모리
            // 할당 실패(bad_alloc) 시 이미 queue에서 제거된 task가 유실될 수 있는 문제가
            // 있었습니다. task를 큐에서 꺼내는 시점과 실행하는 시점 사이에 별도의 컨테이너로
            // 옮겨 담는 단계 자체가 없으면 이 문제가 원천적으로 발생하지 않으므로, 아래처럼
            // task 하나를 이동시켜 즉시 pop하고 바로 실행하는 방식으로 단순화했습니다.
            // (lock/unlock 횟수는 늘지만, std::function의 move 자체는 일반적으로 저렴하고
            //  timer queue 규모에서는 안전성/단순성이 batch로 인한 lock 절약보다 우선합니다.
            //  만료 task가 tick당 수천~수만 개 이상으로 크다면 별도 측정 후 batch 재도입을
            //  검토하십시오.)
            //
            // top()은 const_reference를 반환하지만, priority_queue의 내부 저장소인
            // std::vector<DelayedTask>의 해당 슬롯 자체는 const 객체가 아니므로
            // const_cast 후 move하는 것 자체는 UB가 아닙니다. 다만 DelayedTask가
            // nothrow move 가능함을 static_assert로 보장하고 있으므로(위 참고),
            // 이 move는 예외를 던지지 않고 안전하게 완료됩니다.
            //
            // 뒤이어 호출되는 pop()은 내부적으로 pop_heap() + pop_back()으로 구성되며,
            // pop_heap()은 front(=이미 move-out된 슬롯)를 back과 swap한 뒤
            // [first, last-1) 범위, 즉 방금 move-out된 슬롯을 제외한 범위에서만
            // comparator(operator>)를 호출해 재정렬합니다. 따라서 moved-from 상태의
            // DelayedTask가 heap 비교 대상이 되는 일은 없으며, ExecuteTime 자체도
            // move 후 그대로 유지되는 값이므로 이 패턴은 안전합니다.
            DelayedTask task = std::move(const_cast<DelayedTask&>(_queue.top()));
            _queue.pop();

            lock.unlock();
            try
            {
                task.Task();
            }
            catch( const std::exception& e )
            {
                // 표준 예외 메시지 로깅
                LOG_ERROR(_T("CDelayedTaskQueue: Task execution failed with std::exception: %hs"), e.what());
            }
            catch( ... )
            {
                // 알 수 없는 예외 로깅
                LOG_ERROR(_T("CDelayedTaskQueue: Task execution failed with unknown exception."));
            }
            lock.lock();
        }
    }

    //***************************************************************************
    // @brief 대기 중인 처리 루프를 정지시킵니다.
    // @note 정지 플래그를 설정한 뒤 대기 중인 스레드를 깨웁니다.
    //       ProcessExpiredTasks()는 이후 새로운 대기/작업 처리를 시작하지 않고 루프를
    //       종료합니다. 이미 실행 중인 워커 스레드의 종료를 기다리려면(join) 호출자가
    //       별도로 처리해야 합니다.
    // @warning Stop() 호출 시점에 큐에 남아 있던 task(이미 만료되었으나 아직 꺼내지
    //          못한 것 포함, 아직 만료되지 않은 예약도 포함)는 실행되지 않고 그대로
    //          버려집니다. Stop()은 "남은 예약을 마저 처리하고 종료"하는 graceful
    //          drain이 아니라 즉시 중단(discard)입니다. 종료 전 반드시 실행되어야
    //          하는 task가 있다면, 호출자가 Stop() 전에 별도로 처리하거나 이 클래스를
    //          graceful-drain이 필요 없는 용도(세션 timeout, 재시도 backoff 등)에만
    //          사용하십시오.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
    }

    CDelayedTaskQueue(const CDelayedTaskQueue&) = delete;
    CDelayedTaskQueue& operator=(const CDelayedTaskQueue&) = delete;

private:
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<DelayedTask>> _queue;   // 최소 힙 우선순위 큐
    std::mutex                  _mutex;                     // 큐 및 _stopped 보호용 뮤텍스
    std::condition_variable     _cv;                        // 타이머 대기용 조건 변수
    bool                        _stopped{ false };          // ProcessExpiredTasks() 루프 정지 플래그 (_mutex로 보호)
};

#endif // ndef UC_DELAYEDTASKQUEUE_H