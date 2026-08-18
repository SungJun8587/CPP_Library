
//***************************************************************************
// BlockingTaskQueue.h : interface for the CBlockingTaskQueue class.
//
//***************************************************************************

#ifndef __BLOCKINGTASKQUEUE_H__
#define __BLOCKINGTASKQUEUE_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef	__QUEUECOMMON_H__
#include <Containers/Queue/QueueCommon.h>
#endif

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
    //***************************************************************************
    void Push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped.load(std::memory_order_relaxed) )
                return;
            _queue.push(std::move(item));
        }
        _cv.notify_one();
    }

    //***************************************************************************
    // @brief 여러 데이터를 묶어서 한 번에 넣고 알림을 보냅니다.
    // @param items 삽입할 데이터 항목들의 참조 벡터 (성공 시 내부 비워짐)
    //***************************************************************************
    void PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            if( _stopped.load(std::memory_order_relaxed) )
                return;

            for( auto& item : items )
                _queue.push(std::move(item));
        }
        _cv.notify_all();
        items.clear();
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
            return _stopped.load(std::memory_order_relaxed) || !_queue.empty() || _producerDone.load();
            });

        if( _stopped.load(std::memory_order_relaxed) )
            return false;

        if( _queue.empty() && _producerDone.load() )
            return false;

        if( _queue.empty() )
            return false;

        out = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    //***************************************************************************
    // @brief 프로듀서 종료 신호를 설정합니다. 남은 데이터만 마저 처리하고 종료합니다.
    //***************************************************************************
    void SetProducerDone()
    {
        _producerDone.store(true);
        _cv.notify_all();
    }

    //***************************************************************************
    // @brief 큐를 강제 종료하고 대기 중인 모든 스레드를 깨웁니다.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped.store(true, std::memory_order_relaxed);
        }
        _cv.notify_all();
    }

    CBlockingTaskQueue() = default;
    CBlockingTaskQueue(const CBlockingTaskQueue&) = delete;
    CBlockingTaskQueue& operator=(const CBlockingTaskQueue&) = delete;

private:
    CQueue<T>               _queue;                     // 내부 큐 컨테이너
    std::mutex              _mutex;                     // 동기화를 위한 텍스
    std::condition_variable _cv;                        // 소비자 대기 제어 조건 변수
    std::atomic<bool>       _producerDone{ false };     // 프로듀서 종료 플래그
    std::atomic<bool>       _stopped{ false };          // 강제 종료 플래그
};

#endif // ndef __BLOCKINGTASKQUEUE_H__