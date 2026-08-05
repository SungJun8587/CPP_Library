
//***************************************************************************
// ChunkedSwapQueue.h : interface for the CChunkedSwapQueue class.
//
//***************************************************************************

#ifndef __CHUNKED_SWAPQUEUE_H__
#define __CHUNKED_SWAPQUEUE_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __PLATFORMLOCK_H__
#include <Thread/PlatformLock.h>
#endif

#ifndef	__QUEUECOMMON_H__
#include <Containers/Queue/QueueCommon.h>
#endif

//***************************************************************************
// @class CChunkedSwapQueue
// @brief 단일 큐와 청킹(Chunking) 기능을 지원하는 스레드 세이프 스왑 큐.
// 
// @details 
// 데이터 양이 폭발적이거나 일시적으로 몰릴 때, 컨슈머가 한 번에 처리하는 양을 
// 세밀하게 조절(청킹)하여 프레임 드랍이나 과부하를 방지해야 하는 상황에 적합합니다.
// 
// 주요 사용처 및 이점:
//  - IOCP 서버의 네트워크 I/O 스레드 → 로직 스레드 간 패킷 전달 (SwapChunk로 처리량 제어)
//  - 대량의 요청이 몰릴 때 컨슈머의 부하 분산 및 스파이크 현상 방지
//  - 큐 크기(_size)를 아토믹으로 실시간 모니터링해야 하는 경우
// 
// 패턴 최적화:
//  - **SPMC(Single Producer, Multiple Consumer)** 환경에 최적화
//    → 단일 프로듀서가 데이터를 넣고, 여러 컨슈머가 청킹 단위로 나눠 가져가며 부하를 분산
//***************************************************************************
template<typename T>
class CChunkedSwapQueue
{
public:
    CChunkedSwapQueue() = default;
    ~CChunkedSwapQueue() = default;

    //***************************************************************************
    // @brief 단일 아이템을 입력 큐에 안전하게 삽입합니다.
    // @param item 삽입할 데이터 항목
    //***************************************************************************
    void Push(T item)
    {
        if( _stopped.load(std::memory_order_relaxed) )
            return;

        PLockGuard lock(_lock, __FUNCTION__);

        _inQueue.push(std::move(item));
        _size.fetch_add(1, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 락 안에서 푸시와 크기 증가를 원자적으로 처리하여 갱신된 전체 크기를 반환합니다.
    // @param item 삽입할 데이터 항목
    // @return 푸시 후의 전체 큐 크기
    //***************************************************************************
    int64_t PushAndGetSize(T item)
    {
        if( _stopped.load(std::memory_order_relaxed) )
            return _size.load(std::memory_order_relaxed);

        PLockGuard lock(_lock, __FUNCTION__);

        _inQueue.push(std::move(item));
        return _size.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    //***************************************************************************
    // @brief 여러 아이템을 벡터 단위로 일괄 삽입합니다.
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시 내부 비워짐)
    //***************************************************************************
    void PushBatch(CVector<T>& items)
    {
        if( items.empty() || _stopped.load(std::memory_order_relaxed) )
            return;

        PLockGuard lock(_lock, __FUNCTION__);

        for( auto& item : items )
        {
            _inQueue.push(std::move(item));
        }
        _size.fetch_add(static_cast<int64_t>(items.size()), std::memory_order_relaxed);
        items.clear();
    }

    //***************************************************************************
    // @brief 입력 큐의 모든 요소를 출력 큐로 통째로 스왑(이동)합니다.
    // @param outQueue 데이터를 전달받을 대상 큐
    //***************************************************************************
    void Swap(CQueue<T>& outQueue)
    {
        PLockGuard lock(_lock, __FUNCTION__);

        if( _inQueue.empty() )
            return;

        // outQueue가 비어있다면 컨테이너 자체를 O(1)로 통째로 스왑
        if( outQueue.empty() )
        {
            _inQueue.swap(outQueue);
        }
        else
        {
            // outQueue에 잔여물이 있는 경우에만 개별 이동
            while( !_inQueue.empty() )
            {
                outQueue.push(std::move(_inQueue.front()));
                _inQueue.pop();
            }
        }

        _size.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 입력 큐에서 지정한 최대 개수(maxCount)만큼만 떼어와 출력 큐로 이동합니다. (청킹 스왑)
    // @note 멀티 스레드 환경에서 하나의 스레드가 백로그 전체를 독점하는 현상을 방지합니다.
    // @param outQueue 데이터를 전달받을 대상 큐
    // @param maxCount 한 번에 가져올 최대 아이템 개수
    //***************************************************************************
    void SwapChunk(CQueue<T>& outQueue, size_t maxCount)
    {
        PLockGuard lock(_lock, __FUNCTION__);

        if( _inQueue.empty() )
            return;

        size_t movedCount = 0;
        while( !_inQueue.empty() && movedCount < maxCount )
        {
            // unique_ptr 소유권을 안전하게 outQueue로 이동
            outQueue.push(std::move(_inQueue.front()));
            _inQueue.pop();
            ++movedCount;
        }

        // 이동시킨 만큼 전체 크기 카운터를 원자적으로 차감
        _size.fetch_sub(static_cast<int64_t>(movedCount), std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 여부를 반환합니다.
    //***************************************************************************
    bool IsEmpty() const
    {
        return _size.load(std::memory_order_relaxed) == 0;
    }

    //***************************************************************************
    // @brief 현재 큐에 대기 중인 전체 아이템 개수를 반환합니다.
    //***************************************************************************
    int64_t GetSize() const
    {
        return _size.load(std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 추가 푸시를 차단합니다.
    //***************************************************************************
    void Stop()
    {
        _stopped.store(true, std::memory_order_relaxed);
    }

    CChunkedSwapQueue(const CChunkedSwapQueue&) = delete;
    CChunkedSwapQueue& operator=(const CChunkedSwapQueue&) = delete;
    CChunkedSwapQueue(CChunkedSwapQueue&&) = delete;
    CChunkedSwapQueue& operator=(CChunkedSwapQueue&&) = delete;

private:
    PLock                   _lock;              // 플랫폼 통합 단독 락 객체
    CQueue<T>               _inQueue;           // 내부 입력을 받는 큐 버퍼
    std::atomic<int64_t>    _size{ 0 };         // 락 경합 없는 빠른 크기 조회를 위한 아토믹 카운터
    std::atomic<bool>       _stopped{ false };  // 종료 플래그 추가
};

#endif // ndef __CHUNKED_SWAPQUEUE_H__