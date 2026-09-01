
//***************************************************************************
// BlockingTaskQueue.h : interface for the CBlockingTaskQueue class.
//
//***************************************************************************

#ifndef UC_BLOCKINGTASKQUEUE_H
#define UC_BLOCKINGTASKQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>

#include <condition_variable>
#include <mutex>
#include <utility>

//***************************************************************************
// @class CBlockingTaskQueue
// @brief 멀티 스레드 환경에서 블로킹 되는 타스크 큐.
//
// @details
// 내부적으로 std::mutex와 std::condition_variable을 사용하여 멀티스레드 환경에서
// 안전하게 Push/Pop을 수행합니다. 컨슈머는 데이터가 없으면 자동응답으로 대기(wait)하다가 데이터가 들어오면 깨어납니다. 또한 프로듀서 종료 신호를
// 사용하여 graceful shutdown을 지원합니다.
//
// 주요 처리 및 특징:
//  - 멀티스레드 환경에서 생산자-소비자 패턴 구현
//  - 소비자가 스레드가 데이터가 없을 때까지 블로킹 대기 상태 유지
//  - 프로듀서 종료 신호를 통한 안전한 종료 처리
//  - 단순하고 직관적인 인터페이스 제공
//
// 패턴 최적화:
//  - **MPMC(Multi Producer, Multi Consumer)** 환경에 최적화
//    → 여러 프로듀서가 데이터를 넣고, 여러 컨슈머가 안전하게 Pop 수행
//***************************************************************************
template<typename T>
class CBlockingTaskQueue
{
public:
    //***************************************************************************
    // @brief 큐의 내부에 데이터를 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동 가능)
    // @return true: 삽입 성공, false: Stop() 또는 SetProducerDone() 이후라 거부됨
    //***************************************************************************
    bool Push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped || _producerDone )
                return false;
            _queue.push(std::move(item));
        }
        _cv.notify_one();
        return true;
    }

    //***************************************************************************
    // @brief 여러 데이터를 묶어서 한 번에 넣고 알림을 보냅니다.
    // @param items 삽입할 데이터 항목들의 참조 벡터 (성공 시 내부 비워짐)
    // @return true: 삽입 성공, false: Stop() 또는 SetProducerDone() 이후라 거부됨(items는 최대한 복원됨)
    // @note temp.push()/items.push_back() 등 락 밖 이동 과정에서 예외가 발생하면
    //       일부 원소만 옮겨진 상태로 함수가 종료될 수 있습니다. _queue 자체의 컨테이너
    //       invariant는 깨지지 않지만, "예외 발생 시 items가 호출 전 상태로 완전히
    //       보존된다"는 보장은 아닙니다. T의 이동 연산이 예외를 던지지 않는 타입에서
    //       사용을 권장합니다.
    //***************************************************************************
    bool PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return true;

        // 할당이 발생할 수 있는 각 원소의 큐 삽입을 락 밖에서 먼저 임시 큐에 수행합니다.
        // 이렇게 하면 락을 잡지 않은 채로 처리되므로 락 보유 시간이 줄어듭니다.
        CQueue<T> temp;
        for( auto& item : items )
            temp.push(std::move(item));

        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped || _producerDone )
            {
                // 이미 정지/종료된 상태라면 큐에 넣지 않고, temp로 옮겨둔 원소를
                // items로 최대한 되돌립니다(위 @note 참고: 완전한 예외 안전성은 아님).
                items.clear();
                while( !temp.empty() )
                {
                    items.push_back(std::move(temp.front()));
                    temp.pop();
                }
                return false;
            }

            while( !temp.empty() )
            {
                _queue.push(std::move(temp.front()));
                temp.pop();
            }
        }
        _cv.notify_all();
        items.clear();
        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 하나 꺼냅니다. 데이터가 없으면 블로킹 대기합니다.
    // @param out 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터가 정상적으로 꺼내짐, false: 프로듀서 종료 신호 또는 Stop 호출 후 더이상 데이터가 없음
    //***************************************************************************
    bool Pop(T& out)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        _cv.wait(lock, [this]() {
            return _stopped || !_queue.empty() || _producerDone;
            });

        if( _stopped )
            return false;

        if( _queue.empty() )
            return false; // producerDone && no more data (wait 조건상 이 지점에서만 empty일 수 있음)

        out = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    //***************************************************************************
    // @brief 프로듀서 종료 신호를 설정합니다. 남은 데이터만 마저 처리하고 종료합니다.
    //***************************************************************************
    void SetProducerDone()
    {
        {
            // _mutex 없이 상태만 바꾸면 Pop()의 wait predicate 검사와 경합하여
            // lost wakeup(알림 유실로 인한 무한 대기)이 발생할 수 있으므로 락 안에서 변경
            std::lock_guard<std::mutex> lock(_mutex);
            _producerDone = true;
        }
        _cv.notify_all();
    }

    //***************************************************************************
    // @brief 큐를 강제 종료하고 대기 중인 모든 스레드를 깨웁니다.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
    }

    CBlockingTaskQueue() = default;
    CBlockingTaskQueue(const CBlockingTaskQueue&) = delete;
    CBlockingTaskQueue& operator=(const CBlockingTaskQueue&) = delete;

private:
    CQueue<T>                   _queue;                 // 내부 큐 컨테이너
    std::mutex                  _mutex;                 // 동기화를 위한 뮤텍스
    std::condition_variable     _cv;                    // 소비자 대기 제어 조건 변수
    bool                        _producerDone{ false }; // 프로듀서 종료 플래그 (항상 _mutex 보유 상태에서만 접근)
    bool                        _stopped{ false };      // 강제 종료 플래그 (항상 _mutex 보유 상태에서만 접근)
};

#endif // ndef UC_BLOCKINGTASKQUEUE_H