
//***************************************************************************
// SpinLockQueue.h : interface for the CSpinLockQueue class.
//
//***************************************************************************

#ifndef UC_SPINLOCKQUEUE_H
#define UC_SPINLOCKQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>
#include <Thread/PlatformLock.h>

#include <atomic>
#include <type_traits>
#include <utility>

//***************************************************************************
// @class CSpinLockQueue
// @brief 스레드 세이프 큐 (내부 락은 플랫폼에 따라 다름).
//
// @details
// 내부적으로 커스텀 CQueue<T>와 PLock(플랫폼 통합 락)을 사용하여 멀티스레드
// 환경에서 안전하게 Push/Pop을 수행합니다. 
// PLock은 Windows 환경에서는 네이티브 SRWLock을, 그 외 플랫폼에서는 스핀락을 
// 사용하므로, 클래스명과 달리 Windows 빌드에서는 실제로 스핀락이 아닌 SRWLock 
// 기반으로 동작합니다. 
// 아토믹 카운터를 통해 GetSize/IsEmpty 조회 시 락 경합을 피할 수 있습니다. 단순하고 직관적인
// 인터페이스를 제공하면서도 MPMC(Multi Producer, Multi Consumer) 패턴을 지원합니다.
//
// 주요 사용처 및 이점:
//  - 글로벌 작업 큐, JobQueue 관리 등 멀티스레드 환경에서 단순한 작업 큐로 활용
//  - 락 경합을 최소화하면서도 직관적인 Push/Pop 인터페이스 제공
//  - IsEmpty()/GetSize() 조회 시 락이 필요 없어 빠른 상태 확인 가능
//  - MPMC(Multi Producer, Multi Consumer) 환경에서 안전하게 사용할 수 있는 Thread-Safe Queue
//
// 패턴 최적화:
//  - MPMC(Multi Producer, Multi Consumer) 환경에 최적화
//    → 여러 프로듀서가 데이터를 넣고, 여러 컨슈머가 동시에 안전하게 꺼낼 수 있음
//
// 타입 요구사항:
//  - T는 noexcept move-assignable해야 합니다. TryPop()은 락 보유 중에
//    outItem = std::move(_items.front()) 형태로 원소를 꺼내는데, 이 이동
//    대입이 예외를 던지면 front() 원소가 일부만 이동된 상태로 큐에 남아
//    이후 호출에서 손상된 값을 반환할 수 있습니다. noexcept 이동 대입을
//    갖는 타입(포인터, 핸들, 작은 POD/구조체 등)을 사용하는 것을 전제로
//    설계되었습니다.
//***************************************************************************
template<typename T>
class CSpinLockQueue
{
public:
    //***************************************************************************
    // @brief 큐에 새로운 데이터를 삽입합니다.
    // @param item 삽입할 데이터 항목 (복사 또는 이동 가능)
    // @return true: 삽입 성공, false: 이미 Stop()된 상태라 삽입되지 않음
    //***************************************************************************
    bool Push(T item)
    {
        if( _stopped.load(std::memory_order_relaxed) )
            return false;

        PLockGuard lock(_lock, __FUNCTION__);

        // Stop()도 동일한 _lock을 거치므로, 이 지점에서 재확인하면
        // Stop() 호출(락 안에서의 상태 변경) 이후 데이터가 유입되는 것을 완전히 차단
        if( _stopped.load(std::memory_order_relaxed) )
            return false;

        // 인자로 받은 item을 rvalue로 전환하여 큐에 효율적으로 삽입
        _items.push(std::move(item));
        _size.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 하나 꺼냅니다.
    // @param outItem [out] 꺼낸 데이터를 담을 변수
    // @return true: 데이터를 꺼냄, false: 큐가 비어있어 outItem이 변경되지 않음
    // @note T는 noexcept move-assignable해야 합니다. 이동 대입 도중 예외가
    //       발생하지 않는다는 전제 하에 front() 원소를 안전하게 꺼낼 수 있습니다.
    //***************************************************************************
    [[nodiscard]] bool TryPop(OUT T& outItem)
    {
        static_assert(std::is_nothrow_move_assignable_v<T>,
            "CSpinLockQueue::TryPop requires T to be noexcept move-assignable");

        PLockGuard lock(_lock, __FUNCTION__);

        if( _items.empty() )
            return false;

        outItem = std::move(_items.front());
        _items.pop();
        _size.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    //***************************************************************************
    // @brief 큐의 모든 데이터를 한 번에 꺼내 외부 컨테이너에 담습니다.
    // @param items 데이터를 담을 외부 컨테이너 (CVector<T>)
    // @note 내부 큐(_items/_size)는 락 안에서 즉시 비워지므로 내부 상태는 항상 일관됩니다.
    //       다만 락 밖에서 수행되는 items.reserve()/push_back() 또는 T의 이동 과정에서
    //       예외가 발생하면, 아직 items로 옮기지 못한 나머지 항목은 소멸되어 유실될 수
    //       있습니다(내부 정합성은 보장되지만 데이터 보존까지 보장하지는 않음).
    //       T의 이동 연산과 대상 컨테이너 삽입이 예외를 던지지 않는 타입에서 사용을
    //       권장합니다.
    //***************************************************************************
    void PopAll(OUT CVector<T>& items)
    {
        // Clear()와 동일한 철학: 락 안에서는 포인터 교환(swap)만 수행하고,
        // 실제 원소 이동(및 그 과정에서 발생 가능한 예외/재할당 비용)은 락 밖에서 처리합니다.
        // 이렇게 하면 락 보유 시간이 줄어들 뿐 아니라, 원소 이동 중 예외가 발생하더라도
        // _items/_size는 이미 "비어있음"으로 확정된 뒤라 내부 상태가 깨지지 않습니다.
        // CQueue<T>는 std::queue<T, CDeque<T>>이므로 std::swap(queue&, queue&)이
        // 선택되어 내부적으로 CDeque(=std::deque)의 포인터/블록 맵만 교환하는 O(1) 연산이며,
        // StlAllocator가 항상 동일하다고 비교되므로(operator== 상시 true) 예외 없이 안전합니다.
        CQueue<T> localQueue;
        {
            PLockGuard lock(_lock, __FUNCTION__);
            std::swap(_items, localQueue);
            _size.store(0, std::memory_order_relaxed);
        }

        // 메모리 재할당 비용을 줄이기 위해 컨테이너 크기 미리 확보
        items.reserve(items.size() + localQueue.size());
        while( !localQueue.empty() )
        {
            items.push_back(std::move(localQueue.front()));
            localQueue.pop();
        }
    }

    //***************************************************************************
    // @brief 큐를 완전히 비웁니다.
    //***************************************************************************
    void Clear()
    {
        // 스왑으로 비워낸 구 데이터(emptyQueue)는 이 스코프가 끝나는 시점,
        // 즉 락 해제 이후에 소멸되도록 락보다 먼저 선언(선언의 역순으로 소멸)
        CQueue<T> emptyQueue;
        {
            PLockGuard lock(_lock, __FUNCTION__);
            std::swap(_items, emptyQueue);
            _size.store(0, std::memory_order_relaxed);
        }
        // emptyQueue는 여기서(락 해제 후) 소멸 → T의 소멸 비용이 락 보유 시간에 포함되지 않음
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 여부를 반환합니다.
    // @return true: 비어있음, false: 데이터 있음
    // @note 멀티스레드 환경에서는 반환 직후 상태가 변경될 수 있습니다.
    //       동시성 제어가 필요한 경우 TryPop()의 반환값을 사용해야 합니다.
    //***************************************************************************
    bool IsEmpty() const
    {
        return _size.load(std::memory_order_relaxed) == 0;
    }

    //***************************************************************************
    // @brief 현재 큐에 대기 중인 전체 아이템 개수를 반환합니다.
    // @return 큐 크기 (size_t)
    // @note 멀티스레드 환경에서는 순간적인 관찰값이며, 반환 직후 실제 큐 크기가 
    //       변경될 수 있습니다.
    //***************************************************************************
    size_t GetSize() const
    {
        return static_cast<size_t>(_size.load(std::memory_order_relaxed));
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 추가 푸시를 차단합니다.
    // @note 기존에 큐에 쌓여 있던 항목은 제거되지 않으며, TryPop()/PopAll()/Clear()로
    //       계속 처리할 수 있습니다. Stop()은 오직 이후의 Push()만 차단합니다.
    //***************************************************************************
    void Stop()
    {
        PLockGuard lock(_lock, __FUNCTION__);
        _stopped.store(true, std::memory_order_relaxed);
    }

private:
    PLock                   _lock;              // 플랫폼 통합 단독 락 객체
    CQueue<T>               _items;             // 커스텀 CQueue<T> 기반 내부 큐
    std::atomic<int64>      _size{ 0 };         // IsEmpty()/GetSize()를 락 없이 조회하기 위한 카운터
    std::atomic<bool>       _stopped{ false };  // 종료 플래그 추가
};

#endif // ndef UC_SPINLOCKQUEUE_H