
//***************************************************************************
// ChunkedBlockingQueue.h : interface for the CChunkedBlockingQueue class.
//
//***************************************************************************

#ifndef UC_CHUNKEDBLOCKINGQUEUE_H
#define UC_CHUNKEDBLOCKINGQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <type_traits>

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
//  - 대량의 태스크가 유입될 때 프로듀서 측 경합을 제거하고, 소비자는 청크 단위로 가져가 mutex 획득 횟수를 줄여 부하를 분산
//  - 큐가 비었을 때 불필요한 CPU 점유(Busy-Waiting) 없이 안전한 대기 및 휴식 지원
// 
// 패턴 최적화:
//  - **SPMC (Single Producer, Multiple Consumer)** 환경에 최적화
//    → 단일 프로듀서는 락 경합을 최소화하며 데이터를 공급하고, 멀티 컨슈머는 청크 단위로 가져가 락 횟수를 극적으로 절감
//
// 용량 제한:
//  - 생성자에 maxQueueSize(기본값 0 = 무제한)를 지정하면, 큐가 가득 찼을 때
//    Push/PushBatch가 공간이 생길 때까지 블로킹되어 백프레셔를 제공합니다.
//  - PushBatch()에 전달하는 배치 크기는 maxQueueSize를 넘을 수 없습니다(넘으면 거부).
//
// 예외 안전성:
//  - T는 nothrow move constructible이어야 합니다.
//  - PushBatch()는 T의 이동 과정에서 예외가 발생하지 않는 것을 전제로 합니다.
//  - 단, 내부 deque의 메모리 할당 실패 등으로 예외가 발생할 경우
//    배치 전체의 강한 예외 보장 및 items의 원상 복구는 보장하지 않습니다.
//***************************************************************************
template<typename T>
class CChunkedBlockingQueue
{
    static_assert(std::is_nothrow_move_constructible_v<T>,
        "CChunkedBlockingQueue<T>: T must be nothrow move constructible.");

public:
    explicit CChunkedBlockingQueue(size_t maxQueueSize = 0)
        : _maxQueueSize(maxQueueSize)
    {
    }

    //***************************************************************************
    // @brief 큐에 새로운 단일 아이템을 안전하게 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동)
    // @details maxQueueSize가 설정된 경우, 공간이 생길 때까지 블로킹됩니다.
    // @return true: 삽입 성공, false: Stop() 또는 SetProducerDone() 이후라 거부됨
    //***************************************************************************
    bool Push(T item)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);

            if( _maxQueueSize > 0 )
            {
                _notFullCv.wait(lock, [this]() {
                    return _stopped
                        || _producerDone
                        || _inQueue.size() < _maxQueueSize;
                    });
            }

            if( _stopped || _producerDone )
                return false;

            _inQueue.push(std::move(item));
        }
        _cv.notify_one();
        return true;
    }

    //***************************************************************************
    // @brief 여러 아이템을 벡터 단위로 일괄 삽입합니다. (프로듀서 배치 최적화)
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시 내부 비워짐)
    // @details maxQueueSize가 설정된 경우, 배치 전체를 담을 공간이 생길 때까지
    //          블로킹됩니다.
    //          배치 크기가 maxQueueSize보다 크면 절대 공간이 생기지 않으므로
    //          아무 동작도 하지 않고 반환합니다(items도 비우지 않음).
    //          T의 move 생성이 noexcept이므로 T 이동 과정에서는 예외가 발생하지 않습니다.
    //          단, 내부 deque의 메모리 할당 실패 등으로 예외가 발생할 경우
    //          items의 원상 복구는 보장하지 않습니다.
    // @return true: 삽입 성공, false: 거부됨(빈 배치 제외 — Stop()/SetProducerDone() 이후,
    //         또는 배치 크기가 maxQueueSize 초과)
    //***************************************************************************
    bool PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return true;

        if( _maxQueueSize > 0 && items.size() > _maxQueueSize )
            return false; // 이 배치는 큐 용량을 넘어 절대 들어갈 수 없음

        {
            std::unique_lock<std::mutex> lock(_mutex);

            if( _maxQueueSize > 0 )
            {
                _notFullCv.wait(lock, [this, &items]() {
                    // 배치 전체가 큐에 들어갈 수 있는 충분한 공간이 있는지 확인합니다.
                    // _maxQueueSize - _inQueue.size() 방식은 _inQueue.size()가
                    // _maxQueueSize를 초과한 비정상 상태에서 size_t 언더플로우가
                    // 발생할 수 있으므로 사용하지 않습니다.
                    //
                    // 정상적인 큐 invariant(_inQueue.size() <= _maxQueueSize)에서는
                    // 아래 덧셈 비교가 배치 전체를 수용할 수 있는지를 정확하게 판단합니다.
                    return _stopped
                        || _producerDone
                        || items.size() + _inQueue.size() <= _maxQueueSize;
                    });
            }

            if( _stopped || _producerDone )
                return false;

            for( auto& item : items )
                _inQueue.push(std::move(item));
        }
        _cv.notify_all();
        items.clear();
        return true;
    }

    //***************************************************************************
    // @brief 큐에서 최대 maxCount개의 데이터를 청크 단위로 가져와 출력 큐로 이동합니다.
    // @param outQueue 데이터를 전달받을 대상 큐
    // @param maxCount 한 번에 가져올 최대 아이템 개수 (0은 잘못된 인자로 간주하여 false 반환)
    // @return true: 하나 이상의 데이터를 정상적으로 가져옴
    //         false: maxCount == 0이거나 Stop() 호출 또는 ProducerDone 상태에서 더 이상 데이터가 없음
    //***************************************************************************
    bool PopChunk(CQueue<T>& outQueue, size_t maxCount)
    {
        if( maxCount == 0 )
            return false;

        {
            std::unique_lock<std::mutex> lock(_mutex);

            _cv.wait(lock, [this]() {
                return _stopped || !_inQueue.empty() || _producerDone;
                });

            if( _stopped )
                return false;

            if( _inQueue.empty() && _producerDone )
                return false;

            size_t movedCount = 0;
            while( !_inQueue.empty() && movedCount < maxCount )
            {
                outQueue.push(std::move(_inQueue.front()));
                _inQueue.pop();
                ++movedCount;
            }
        }

        // 공간이 생겼음을 대기 중인 프로듀서에게 알림 (maxQueueSize 설정 시에만 의미 있음)
        // 단일 프로듀서(SPMC) 가정이므로 대기자는 최대 1명 — notify_one으로 충분
        if( _maxQueueSize > 0 )
            _notFullCv.notify_one();

        return true;
    }

    //***************************************************************************
    // @brief 프로듀서의 작업 완료 신호를 설정하고 모든 소비자 스레드를 깨웁니다.
    //***************************************************************************
    void SetProducerDone()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _producerDone = true;
        }
        _cv.notify_all();
        _notFullCv.notify_all();
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 대기 중인 모든 소비자/생산자 스레드를 해제합니다.
    //***************************************************************************
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
        _notFullCv.notify_all();
    }

    CChunkedBlockingQueue(const CChunkedBlockingQueue&) = delete;
    CChunkedBlockingQueue& operator=(const CChunkedBlockingQueue&) = delete;
    CChunkedBlockingQueue(CChunkedBlockingQueue&&) = delete;
    CChunkedBlockingQueue& operator=(CChunkedBlockingQueue&&) = delete;

private:
    CQueue<T>                   _inQueue;                   // 내부 데이터를 보관하는 큐 버퍼
    std::mutex                  _mutex;                     // 동기화를 위한 뮤텍스
    std::condition_variable     _cv;                        // 소비자 대기 및 통보용 조건 변수
    std::condition_variable     _notFullCv;                 // 생산자 백프레셔 대기용 조건 변수 (단일 프로듀서 전제)
    bool                        _producerDone{ false };     // 프로듀서 탐색 완료 플래그 (_mutex로 보호)
    bool                        _stopped{ false };          // 시스템 강제 정지 플래그 (_mutex로 보호)
    size_t                      _maxQueueSize{ 0 };         // 큐 최대 크기 (0 = 무제한)
};

#endif // ndef UC_CHUNKEDBLOCKINGQUEUE_H