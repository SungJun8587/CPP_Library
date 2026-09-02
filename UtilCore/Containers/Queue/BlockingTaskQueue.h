
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
#include <type_traits>
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
//
// 예외 안전성:
//  - PushBatch()는 Stop()/SetProducerDone() 이후 거부되는 경우 temp에 옮겨둔 원소를
//    items로 되돌리는 롤백 경로를 가집니다. 이 롤백이 안전하려면 T의 이동 생성이
//    예외를 던지지 않아야 하므로, T는 nothrow move constructible이어야 합니다
//    (아래 static_assert로 강제).
//***************************************************************************
template<typename T>
class CBlockingTaskQueue
{
    static_assert(std::is_nothrow_move_constructible_v<T>,
        "CBlockingTaskQueue<T>: T must be nothrow move constructible "
        "for exception-safe batch insertion.");

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
    // @brief 여러 데이터를 묶어서 한 번에 큐에 넣고 대기 중인 소비자에게 알립니다.
    // @param items 삽입할 데이터 항목들의 참조 벡터 (성공 시 내부 비워짐)
    // @return true: 삽입 성공
    //         false: Stop() 또는 SetProducerDone() 이후라 삽입이 거부됨
    // @note
    //       모든 원소의 큐 삽입은 동일한 mutex 구간에서 수행됩니다.
    //       따라서 배치 삽입과 Stop()/SetProducerDone() 사이의 상태 경쟁을
    //       방지하고, 배치 전체를 하나의 논리적 삽입 작업으로 처리합니다.
    //
    //       별도의 임시 CQueue<T>를 사용하지 않습니다.
    //       CQueue<T>가 deque 기반이므로 temp를 사용하는 경우 추가적인
    //       컨테이너 메모리 할당/해제와 원소 이동이 발생합니다.
    //
    //       T는 nothrow move constructible이어야 하며,
    //       PushBatch()에서 원소를 내부 큐로 이동하는 과정에서
    //       T의 이동 생성으로 인한 예외를 방지합니다.
    //
    //       단, _queue의 내부 메모리 할당은 실패할 수 있으므로,
    //       메모리 할당 예외 발생 시 items가 호출 전 상태로
    //       완전히 보존된다는 강한 예외 보장은 제공하지 않습니다.
    //***************************************************************************    
    bool PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return true;

        size_t count = 0;

        {
            std::lock_guard<std::mutex> lock(_mutex);

            if( _stopped || _producerDone )
                return false;

            count = items.size();

            for( auto& item : items )
                _queue.push(std::move(item));
        }

        items.clear();

        // 배치 삽입이므로 대기 중인 소비자를 깨웁니다.
        // 단일 항목은 notify_one()으로 충분하지만,
        // 여러 항목은 여러 소비자가 동시에 처리할 수 있도록 notify_all()을 사용합니다.
        if( count == 1 )
            _cv.notify_one();
        else
            _cv.notify_all();

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
