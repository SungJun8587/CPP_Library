
//***************************************************************************
// ChunkedBlockingQueue.h : interface for the CChunkedBlockingQueue class.
//
//***************************************************************************

#ifndef __CHUNKED_BLOCKINGQUEUE_H__
#define __CHUNKED_BLOCKINGQUEUE_H__

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
// @class CChunkedBlockingQueue
// @brief 싱글 프로듀서 - 멀티 소비자(SPMC) 환경에 최적화된 청크 기반 블로킹 큐.
// 
// @details 
// 단일 프로듀서가 대량의 데이터를 빠르게 밀어 넣고, 여러 컨슈머 스레드가 
// 조건 변수(Condition Variable)로 안전하게 대기하다가 청크(Chunk) 단위로 일괄 가져가 
// 처리할 수 있도록 설계된 고성능 블로킹 큐입니다.
// 
// 주요 사용처 및 이점:
//  - ShCopyMove와 같은 대규모 파일 탐색 및 병렬 처리 시스템 (SPMC 패턴)
//  - 대량의 태스크가 유입될 때 락 경합을 최소화하고 컨슈머 간 부하 분산
//  - 큐가 비었을 때 불필요한 CPU 점유(Busy-Waiting) 없이 안전한 대기 및 휴식 지원
// 
// 패턴 최적화:
//  - **SPMC (Single Producer, Multiple Consumer)** 환경에 최적화
//    → 단일 프로듀서는 락 경합을 최소화하며 데이터를 공급하고, 멀티 컨슈머는 청크 단위로 가져가 락 횟수를 극적으로 절감
//
// 용량 제한:
//  - 생성자에 maxQueueSize(기본값 0 = 무제한)를 지정하면, 큐가 가득 찼을 때
//    Push/PushBatch가 공간이 생길 때까지 블로킹되어 백프레셔를 제공합니다.
//***************************************************************************
template<typename T>
class CChunkedBlockingQueue
{
public:
    explicit CChunkedBlockingQueue(size_t maxQueueSize = 0)
        : _maxQueueSize(maxQueueSize)
    {
    }

    //***************************************************************************
    // @brief 큐에 새로운 단일 아이템을 안전하게 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동)
    // @details maxQueueSize가 설정된 경우, 공간이 생길 때까지 블로킹됩니다.
    //***************************************************************************
    void Push(T item)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);

            if( _maxQueueSize > 0 )
            {
                _notFullCv.wait(lock, [this]() {
                    return _stopped.load(std::memory_order_relaxed)
                        || _inQueue.size() < _maxQueueSize;
                    });
            }

            if( _stopped.load(std::memory_order_relaxed) )
                return;

            _inQueue.push(std::move(item));
        }
        _cv.notify_one();
    }

    //***************************************************************************
    // @brief 여러 아이템을 벡터 단위로 일괄 삽입합니다. (프로듀서 배치 최적화)
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시 내부 비워짐)
    // @details maxQueueSize가 설정된 경우, 배치 전체를 담을 공간이 생길 때까지
    //          블로킹됩니다. (단일 프로듀서 가정 하에 배치 단위 원자성 유지)
    //***************************************************************************
    void PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return;

        {
            std::unique_lock<std::mutex> lock(_mutex);

            if( _maxQueueSize > 0 )
            {
                _notFullCv.wait(lock, [this, &items]() {
                    return _stopped.load(std::memory_order_relaxed)
                        || _inQueue.size() + items.size() <= _maxQueueSize;
                    });
            }

            if( _stopped.load(std::memory_order_relaxed) )
                return;

            for( auto& item : items )
                _inQueue.push(std::move(item));
        }
        _cv.notify_all();
        items.clear();
    }

    //***************************************************************************
    // @brief 큐에서 지정한 최대 개수(maxCount)만큼 데이터를 떼어와 출력 큐로 이동합니다. (청킹 스왑)
    // @param outQueue 데이터를 전달받을 대상 큐
    // @param maxCount 한 번에 가져올 최대 아이템 개수
    // @return true: 정상적으로 데이터를 가져왔거나 대기 후 깨어남, false: 정지(Stop) 호출 시
    //***************************************************************************
    bool PopChunk(CQueue<T>& outQueue, size_t maxCount)
    {
        bool popped = false;
        {
            std::unique_lock<std::mutex> lock(_mutex);

            _cv.wait(lock, [this]() {
                return _stopped.load(std::memory_order_relaxed) || !_inQueue.empty() || _producerDone.load();
                });

            if( _stopped.load(std::memory_order_relaxed) )
                return false;

            if( _inQueue.empty() && _producerDone.load() )
                return false;

            if( _inQueue.empty() )
                return true; // 가짜 깨어남(Spurious wakeup) 방어용

            size_t movedCount = 0;
            while( !_inQueue.empty() && movedCount < maxCount )
            {
                outQueue.push(std::move(_inQueue.front()));
                _inQueue.pop();
                ++movedCount;
            }
            popped = movedCount > 0;
        }

        // 공간이 생겼음을 대기 중인 프로듀서에게 알림 (maxQueueSize 설정 시에만 의미 있음)
        if( popped && _maxQueueSize > 0 )
            _notFullCv.notify_all();

        return true;
    }

    //***************************************************************************
    // @brief 프로듀서의 작업 완료 신호를 설정하고 모든 소비자 스레드를 깨웁니다.
    //***************************************************************************
    void SetProducerDone()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _producerDone.store(true, std::memory_order_relaxed);
        }
        _cv.notify_all();
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 대기 중인 모든 소비자/생산자 스레드를 해제합니다.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped.store(true, std::memory_order_relaxed);
        }
        _cv.notify_all();
        _notFullCv.notify_all();
    }

    CChunkedBlockingQueue(const CChunkedBlockingQueue&) = delete;
    CChunkedBlockingQueue& operator=(const CChunkedBlockingQueue&) = delete;
    CChunkedBlockingQueue(CChunkedBlockingQueue&&) = delete;
    CChunkedBlockingQueue& operator=(CChunkedBlockingQueue&&) = delete;

private:
    CQueue<T>               _inQueue;                   // 내부 데이터를 보관하는 큐 버퍼
    std::mutex              _mutex;                     // 동기화를 위한 뮤텍스
    std::condition_variable _cv;                        // 소비자 대기 및 통보용 조건 변수
    std::condition_variable _notFullCv;                 // 생산자 백프레셔 대기용 조건 변수
    std::atomic<bool>       _producerDone{ false };     // 프로듀서 탐색 완료 플래그
    std::atomic<bool>       _stopped{ false };          // 시스템 강제 정지 플래그
    size_t                  _maxQueueSize{ 0 };         // 큐 최대 크기 (0 = 무제한)
};

#endif // __CHUNKED_BLOCKINGQUEUE_H__