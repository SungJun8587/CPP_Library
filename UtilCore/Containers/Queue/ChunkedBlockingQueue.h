
//***************************************************************************
// ChunkedBlockingQueue.h : interface for the CChunkedBlockingQueue class.
//
//***************************************************************************

#ifndef UC_CHUNKEDBLOCKINGQUEUE_H
#define UC_CHUNKEDBLOCKINGQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>
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
//  - PushBatch()에 전달하는 배치 크기는 maxQueueSize를 넘을 수 없습니다(넘으면 거부).
//
// 예외 안전성:
//  - PushBatch()는 T의 move 생성이 예외를 던지지 않는다는 전제 하에
//    배치 단위 원자성(모두 삽입되거나, 전혀 삽입되지 않음)을 제공합니다.
//    이를 컴파일 타임에 강제하기 위해 T는 nothrow move constructible이어야 합니다.
//***************************************************************************
template<typename T>
class CChunkedBlockingQueue
{
    static_assert(std::is_nothrow_move_constructible_v<T>,
        "CChunkedBlockingQueue<T>: T must be nothrow move constructible "
        "for PushBatch() to provide batch-level exception safety.");

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
    //          블로킹됩니다. (단일 프로듀서 가정 하에 배치 단위 원자성 유지)
    //          배치 크기가 maxQueueSize보다 크면 절대 공간이 생기지 않으므로
    //          아무 동작도 하지 않고 반환합니다(items도 비우지 않음).
    //          T의 move 생성이 noexcept이므로 T 이동 과정에서는 예외가 발생하지 않습니다.
    //          따라서 정상적인 컨테이너 삽입이 완료되는 경우 배치 단위로 처리됩니다.
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
                    // _maxQueueSize - _inQueue.size() 형태는 만약 어떤 이유로든
                    // _inQueue.size()가 _maxQueueSize를 초과하는 상황이 생기면(정상 흐름상
                    // 발생하지 않아야 하지만) size_t 뺄셈이 언더플로우되어 거대한 값이 되고,
                    // predicate가 항상 참이 되어 용량 제한이 무력화된다. 덧셈 비교로 바꿔
                    // 그런 불변식 위반에도 안전하게 동작하도록 한다.
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
    // @brief 큐에서 지정한 최대 개수(maxCount)만큼 데이터를 떼어와 출력 큐로 이동합니다. (청킹 스왑)
    // @param outQueue 데이터를 전달받을 대상 큐
    // @param maxCount 한 번에 가져올 최대 아이템 개수 (0은 잘못된 인자로 간주하여 false 반환)
    // @return true: 정상적으로 데이터를 가져왔거나 대기 후 깨어남, false: 정지(Stop) 호출 시 또는 종료 상태
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