
//***************************************************************************
// DelayedTaskQueue.h : interface for the CDelayedTaskQueue class.
//
//***************************************************************************

#ifndef __DELAYEDTASKQUEUE_H__
#define __DELAYEDTASKQUEUE_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

#ifndef	__QUEUECOMMON_H__
#include <Containers/Queue/QueueCommon.h>
#endif

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
    //***************************************************************************
    bool operator>(const DelayedTask& other) const
    {
        return ExecuteTime > other.ExecuteTime;
    }
};

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
// 패턴 최적화:
//  - **SPMC(Single Producer, Multiple Consumer)** 또는 단일 소비자 루프에 적합
//    → 하나의 스레드가 예약 작업을 넣고, 하나 이상의 소비자 스레드가
//      ProcessExpiredTasks()를 통해 실행 가능
//***************************************************************************
class CDelayedTaskQueue
{
public:
    CDelayedTaskQueue() = default;
    ~CDelayedTaskQueue() = default;

    //***************************************************************************
    // @brief 일정 시간(밀리초) 뒤에 실행될 작업을 예약합니다.
    // @tparam F 람다식 또는 함수 객체 타입
    // @param milliseconds 현재 시점부터 경과해야 할 시간 (밀리초 단위)
    // @param task 시간이 되었을 때 실행할 작업 함수
    //***************************************************************************
    template<typename F>
    void Reserve(int milliseconds, F&& task)
    {
        if( _stopped.load(std::memory_order_relaxed) )
            return;

        auto executeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);

        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped.load(std::memory_order_relaxed) )
                return;
            _queue.push(DelayedTask{ executeTime, std::forward<F>(task) });
        }
        _cv.notify_one(); // 대기 중인 스레드 깨우기
    }

    //***************************************************************************
    // @brief 시간에 도달한 작업들을 순차적으로 꺼내어 실행합니다.
    // @note 아직 시간이 되지 않은 작업은 남은 시간만큼 조건 변수로 효율적으로 대기합니다.
    //       Stop()이 호출되면 즉시 반환합니다.
    //***************************************************************************
    void ProcessExpiredTasks()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        while( !_stopped.load(std::memory_order_relaxed) )
        {
            if( _queue.empty() )
            {
                // 큐가 빌 경우 중지 신호나 새로운 예약이 들어올 때까지 대기
                _cv.wait(lock, [this] {
                    return _stopped.load(std::memory_order_relaxed) || !_queue.empty();
                    });
                if( _stopped.load(std::memory_order_relaxed) )
                    return;
            }

            auto now = std::chrono::steady_clock::now();
            const auto& top = _queue.top();

            if( top.ExecuteTime > now )
            {
                _cv.wait_until(lock, top.ExecuteTime, [this] {
                    return _stopped.load(std::memory_order_relaxed);
                    });
                if( _stopped.load(std::memory_order_relaxed) )
                    return;
                continue;
            }

            // 시간이 된 작업들을 일괄 수집(Drain)하여 락 점유 시간 최소화
            CVector<std::function<void()>> expiredTasks;
            while( !_queue.empty() && _queue.top().ExecuteTime <= now )
            {
                expiredTasks.push_back(std::move(const_cast<DelayedTask&>(_queue.top())).Task);
                _queue.pop();
            }

            // 락 해제 후 안전하게 일괄 콜백 실행
            lock.unlock();
            for( auto& task : expiredTasks )
            {
                try
                {
                    task();
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
            }
            lock.lock();
        }
    }

    //***************************************************************************
    // @brief 대기 중인 처리 루프를 정지시킵니다.
    // @note 정지 플래그를 설정한 뒤 대기 중인 스레드를 깨웁니다.
    //       ProcessExpiredTasks()는 큐에 남은 작업이 있어도 이 호출 이후 즉시 반환합니다.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped.store(true, std::memory_order_relaxed);
        }
        _cv.notify_all();
    }

    CDelayedTaskQueue(const CDelayedTaskQueue&) = delete;
    CDelayedTaskQueue& operator=(const CDelayedTaskQueue&) = delete;

private:
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<DelayedTask>> _queue;   // 최소 힙 우선순위 큐
    std::mutex              _mutex;                     // 큐 보호용 뮤텍스
    std::condition_variable _cv;                        // 타이머 대기용 조건 변수
    std::atomic<bool>       _stopped{ false };          // ProcessExpiredTasks() 루프 정지 플래그
};

#endif // ndef __DELAYEDTASKQUEUE_H__