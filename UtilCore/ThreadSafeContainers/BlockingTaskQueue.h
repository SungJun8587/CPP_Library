
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

//***************************************************************************
// @class CBlockingTaskQueue
// @brief 조건 변수 기반의 블로킹 태스크 큐.
//
// @details
// 내부적으로 std::mutex와 std::condition_variable을 사용하여 멀티스레드 환경에서
// 안전하게 Push/Pop을 수행합니다. 프로듀서가 데이터를 넣으면 컨슈머는 Pop 시
// 자동으로 대기(wait)하다가 데이터가 들어오면 깨어납니다. 또한 프로듀서 종료 신호를
// 지원하여 graceful shutdown이 가능합니다.
//
// 주요 사용처 및 이점:
//  - 멀티스레드 환경에서 생산자-소비자 패턴 구현
//  - 컨슈머 스레드가 데이터가 들어올 때까지 블로킹 대기 가능
//  - 프로듀서 종료 신호를 통해 안전한 종료 처리
//  - 단순하고 직관적인 인터페이스 제공
//
// 패턴 최적화:
//  - **MPMC(Multi Producer, Multi Consumer)** 환경에 적합
//    → 여러 프로듀서가 데이터를 넣고, 여러 컨슈머가 안전하게 Pop 가능
//***************************************************************************
template<typename T>
class CBlockingTaskQueue
{
public:
    //***************************************************************************
    // @brief 큐에 새로운 데이터를 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동 가능)
    void Push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(item));
        }
        _cv.notify_one();
    }

    //***************************************************************************
    // @brief 여러 데이터를 벡터 단위로 일괄 삽입합니다.
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시 내부 비워짐)
    void PushBatch(std::vector<T>& items)
    {
        if( items.empty() )
            return;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            for( auto& item : items )
                _queue.push(std::move(item));
        }
        _cv.notify_all();
        items.clear();
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 하나 꺼냅니다. 데이터가 없으면 블로킹 대기합니다.
    // @param out 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 꺼냄, false: 프로듀서 종료 신호로 더 이상 데이터 없음
    bool Pop(T& out)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        _cv.wait(lock, [this]() {
            return !_queue.empty() || _producerDone.load();
            });

        if( _queue.empty() && _producerDone.load() )
            return false;

        out = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    //***************************************************************************
    // @brief 프로듀서 종료 신호를 설정합니다. 모든 컨슈머를 깨워 안전하게 종료합니다.
    void SetProducerDone()
    {
        _producerDone.store(true);
        _cv.notify_all();
    }

    CBlockingTaskQueue() = default;
    CBlockingTaskQueue(const CBlockingTaskQueue&) = delete;
    CBlockingTaskQueue& operator=(const CBlockingTaskQueue&) = delete;

private:
    CQueue<T>                   _queue;                     // 내부 큐 버퍼
    std::mutex                  _mutex;                     // 동기화용 뮤텍스
    std::condition_variable     _cv;                        // 데이터 대기용 조건 변수
    std::atomic<bool>           _producerDone{ false };     // 프로듀서 종료 플래그
};

#endif // ndef __BLOCKINGTASKQUEUE_H__