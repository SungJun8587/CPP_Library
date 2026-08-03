
//***************************************************************************
// SwapQueue.h : interface for the CSwapQueue class.
//
//***************************************************************************

#ifndef __SWAPQUEUE_H__
#define __SWAPQUEUE_H__

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __SPINLOCK_H__
#include <Thread/SpinLock.h>
#endif

#include <queue>
#include <vector>
#include <atomic>
#include <utility>

//***************************************************************************
// @class CSwapQueue
// @brief 락 경쟁(Contention)을 최소화하기 위한 더블 버퍼링 기반의 스레드 세이프 큐.
// @tparam T 큐에 저장할 데이터 타입
// @note 멀티스레드 환경에서 여러 스레드가 동시에 데이터를 집어넣고(Producer), 
//       단일 또는 소수의 스레드가 주기적으로 모아서 처리(Consumer)하는 
//       로그 수집, 패킷 처리, 이벤트 큐 등의 구조에 사용하면 좋습니다.
//          - IOCP 서버의 네트워크 I / O 스레드 → 로직 스레드 간 패킷 전달 큐(프레임마다 로직 스레드가 한 번의 Swap으로 대량 패킷을 일괄 수거)
//          - CAdoAsyncSrv류 비동기 DB 서비스에서 요청 / 응답 결과를 워커 스레드가 일괄 수거해 콜백 디스패치하는 결과 큐
//          - FcmPushAgent 같은 대량 푸시 스케줄러에서 생산자(스케줄 트리거)와 소비자(배치 전송 워커) 사이의 배치 전달 버퍼
//          - 로깅 시스템에서 다중 스레드가 남기는 로그 라인을 로거 스레드가 주기적으로 일괄 flush하는 용도
template<typename T>
class CSwapQueue
{
public:
    CSwapQueue() = default;
    ~CSwapQueue() = default;

    //***************************************************************************
    // @brief 단일 아이템을 입력 큐에 안전하게 삽입합니다.
    // @param item 삽입할 데이터 항목
    void Push(T item)
    {
        SPIN_LOCK;

        _inQueue.push(std::move(item));
        _size.fetch_add(1, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 락 안에서 푸시와 크기 증가를 원자적으로 처리하여 갱신된 전체 크기를 반환합니다.
    // @param item 삽입할 데이터 항목
    // @return 푸시 후의 전체 큐 크기
    int64_t PushAndGetSize(T item)
    {
        SPIN_LOCK;

        _inQueue.push(std::move(item));
        return _size.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    //***************************************************************************
    // @brief 여러 아이템을 벡터 단위로 일괄 삽입합니다.
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시 내부 비워짐)
    void PushBatch(std::vector<T>& items)
    {
        if( items.empty() )
            return;

        SPIN_LOCK;

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
    void Swap(std::queue<T>& outQueue)
    {
        SPIN_LOCK;

        if( _inQueue.empty() )
            return;

        if( outQueue.empty() )
        {
            _inQueue.swap(outQueue);
        }
        else
        {
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
    void SwapChunk(std::queue<T>& outQueue, size_t maxCount)
    {
        SPIN_LOCK;

        if( _inQueue.empty() )
            return;

        size_t movedCount = 0;
        while( !_inQueue.empty() && movedCount < maxCount )
        {
            outQueue.push(std::move(_inQueue.front()));
            _inQueue.pop();
            ++movedCount;
        }

        // 이동시킨 만큼 전체 크기 카운터를 원자적으로 차감
        _size.fetch_sub(static_cast<int64_t>(movedCount), std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 여부를 반환합니다.
    bool IsEmpty() const
    {
        return _size.load(std::memory_order_relaxed) == 0;
    }

    //***************************************************************************
    // @brief 현재 큐에 대기 중인 전체 아이템 개수를 반환합니다.
    int64_t GetSize() const
    {
        return _size.load(std::memory_order_relaxed);
    }

    CSwapQueue(const CSwapQueue&) = delete;
    CSwapQueue& operator=(const CSwapQueue&) = delete;
    CSwapQueue(CSwapQueue&&) = delete;
    CSwapQueue& operator=(CSwapQueue&&) = delete;

private:
    SPIN_USE_LOCK;
    std::queue<T>          _inQueue;      // 내부 입력을 받는 큐 버퍼
    std::atomic<int64_t>   _size{ 0 };    // 락 경합 없는 빠른 크기 조회를 위한 아토믹 카운터
};

#endif // ndef __SWAPQUEUE_H__