
//***************************************************************************
// SpinLockQueue.h : interface for the CSpinLockQueue class.
//
//***************************************************************************

#ifndef __SPINLOCKQUEUE_H__
#define __SPINLOCKQUEUE_H__

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
// @class CSpinLockQueue
// @brief 스핀락 기반의 스레드 세이프 큐.
//
// @details
// 내부적으로 커스텀 CQueue<T>와 스핀락을 사용하여 멀티스레드 환경에서 안전하게
// Push/Pop을 수행합니다. 아토믹 카운터를 통해 Size/Empty 조회 시 락 경합을 피할 수 있습니다.
// 단순하고 직관적인 인터페이스를 제공하면서도 MPMC(Multi Producer, Multi Consumer)
// 패턴을 지원합니다.
//
// 주요 사용처 및 이점:
//  - 글로벌 작업 큐, JobQueue 관리 등 멀티스레드 환경에서 단순한 작업 큐로 활용
//  - 락 경합을 최소화하면서도 직관적인 Push/Pop 인터페이스 제공
//  - Empty()/Size() 조회 시 락이 필요 없어 빠른 상태 확인 가능
//  - MPMC(Multi Producer, Multi Consumer) 환경에 최적화
//
// 패턴 최적화:
//  - **MPMC(Multi Producer, Multi Consumer)** 환경에 최적화
//    → 여러 프로듀서가 데이터를 넣고, 여러 컨슈머가 동시에 안전하게 꺼낼 수 있음
//***************************************************************************
template<typename T>
class CSpinLockQueue
{
public:
    //***************************************************************************
    // @brief 큐에 새로운 데이터를 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동 가능)
    //***************************************************************************
    void Push(T item)
    {
        if( _stopped.load(std::memory_order_relaxed) )
            return;

        PLockGuard lock(_lock, __FUNCTION__);

        // 인자로 받은 item을 rvalue로 전환하여 큐에 효율적으로 삽입
        _items.push(std::move(item));
        _size.fetch_add(1, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 하나 꺼냅니다.
    // @return 꺼낸 데이터 항목. 큐가 비어있으면 기본 생성된 T 반환.
    //***************************************************************************
    T Pop()
    {
        PLockGuard lock(_lock, __FUNCTION__);

        if( _items.empty() )
            return T();
        T ret = std::move(_items.front());
        _items.pop();
        _size.fetch_sub(1, std::memory_order_relaxed);
        return ret;
    }

    //***************************************************************************
    // @brief 큐의 모든 데이터를 한 번에 꺼내 외부 컨테이너에 담습니다.
    // @param items 데이터를 담을 외부 컨테이너 (CVector<T>)
    //***************************************************************************
    void PopAll(OUT CVector<T>& items)
    {
        PLockGuard lock(_lock, __FUNCTION__); // 단 한 번만 락을 잡고 내부에서 루프를 돌려 쏟아냅니다.

        // 메모리 재할당 비용을 줄이기 위해 컨테이너 크기 미리 확보
        items.reserve(items.size() + _items.size());
        while( !_items.empty() )
        {
            items.push_back(std::move(_items.front()));
            _items.pop();
        }
        _size.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐를 완전히 비웁니다.
    //***************************************************************************
    void Clear()
    {
        PLockGuard lock(_lock, __FUNCTION__);

        // Containers.h의 CQueue<T>를 사용하여 스왑 처리를 수행합니다.
        CQueue<T> emptyQueue;
        std::swap(_items, emptyQueue);
        _size.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 여부를 반환합니다.
    // @return true: 큐가 비어있음, false: 데이터 있음
    //***************************************************************************
    bool Empty() const
    {
        return _size.load(std::memory_order_relaxed) == 0;
    }

    //***************************************************************************
    // @brief 현재 큐에 대기 중인 전체 아이템 개수를 반환합니다.
    // @return 큐 크기 (size_t)
    //***************************************************************************
    size_t Size() const
    {
        return static_cast<size_t>(_size.load(std::memory_order_relaxed));
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 추가 푸시를 차단합니다.
    //***************************************************************************
    void Stop()
    {
        _stopped.store(true, std::memory_order_relaxed);
    }

private:
    PLock                   _lock;              // 플랫폼 통합 단독 락 객체
    CQueue<T>               _items;             // 커스텀 CQueue<T> 기반 내부 큐
    std::atomic<int64_t>    _size{ 0 };         // Empty()/Size()를 락 없이 조회하기 위한 카운터
    std::atomic<bool>       _stopped{ false };  // 종료 플래그 추가
};

#endif // ndef __SPINLOCKQUEUE_H__