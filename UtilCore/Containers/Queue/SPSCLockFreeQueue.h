
//***************************************************************************
// SPSCLockFreeQueue.h : interface for the SPSCLockFreeQueue class.
//
//***************************************************************************

#ifndef UC_SPSCLOCKFREEQUEUE_H
#define UC_SPSCLOCKFREEQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

//***************************************************************************
// @class SPSCLockFreeQueue
// @brief Bounded Single-Producer / Single-Consumer lock-free 큐
//
// @details
// 가장 단순하고 빠른 lock-free 큐. producer와 consumer가 각각 하나씩만
// 존재하므로 CAS가 전혀 필요 없고, 인덱스 증가만으로 동작합니다.
//
// 주요 사용처:
//  - 두 쓰레드 간 파이프라인 (예: 네트워크 수신 → 처리)
//  - 오디오/비디오 스트리밍 버퍼
//  - 고성능 데이터 전달 (저지연)
//
// 특징:
//  - CAS 불필요 → 성능 최고
//  - 구조 단순, 구현 간결
//  - blocking 정책은 동일하게 제공
//  - Close()를 통한 종료 지원
//
// 종료:
//  - Close() 호출 후 새로운 Push는 실패합니다.
//  - Close() 이전에 삽입된 데이터는 계속 Pop할 수 있습니다.
//  - Close() 이후 큐가 비어 있으면 Pop은 false를 반환합니다.
//  - Close()는 대기 중인 producer/consumer를 모두 깨웁니다.
//  - Close()는 여러 번 호출해도 안전합니다(idempotent).
//***************************************************************************
template <typename T, std::size_t Capacity>
class SPSCLockFreeQueue
{
    static_assert(Capacity >= 2,
        "Capacity must be at least 2");

    static_assert((Capacity& (Capacity - 1)) == 0,
        "Capacity must be a power of two");

    //***************************************************************************
    // @brief 큐 내부에서 객체를 저장하기 위한 슬롯입니다.
    //
    // @details
    // SPSC에서는 별도의 Sequence가 필요하지 않습니다.
    // producer는 m_EnqueuePos를 소유하고 consumer는 m_DequeuePos를 소유합니다.
    //
    // Storage는 객체의 실제 lifetime과 별도로 raw storage만 제공합니다.
    //***************************************************************************
    struct Cell
    {
        alignas(T) unsigned char Storage[sizeof(T)];

        //***************************************************************************
        // @brief 셀 내부에 저장된 데이터의 포인터를 반환합니다.
        // @return T* 데이터 포인터
        //***************************************************************************
        T* GetDataPtr() noexcept
        {
            return reinterpret_cast<T*>(Storage);
        }

        //***************************************************************************
        // @brief 셀 내부에 저장된 데이터의 const 포인터를 반환합니다.
        // @return const T* 데이터 포인터
        //***************************************************************************
        const T* GetDataPtr() const noexcept
        {
            return reinterpret_cast<const T*>(Storage);
        }
    };

    //***************************************************************************
    // @brief cache line 간격으로 분리된 atomic position입니다.
    //
    // @details
    // producer와 consumer가 각각 서로 다른 position을 지속적으로 수정하므로
    // false sharing을 방지하기 위해 cache line 정렬을 유지합니다.
    //***************************************************************************
    struct alignas(LFQ_CACHE_LINE_SIZE) AlignedAtomicPosition
    {
        std::atomic<std::size_t> Value{ 0 };
    };

public:
    // SPSC queue의 Pop은 데이터를 반드시 outValue로 이동시켜야 합니다.
    // move assignment가 예외를 발생시키면 queue의 slot을 안전하게 release할 수
    // 없으므로 noexcept를 요구합니다.
    static_assert(
        std::is_nothrow_move_assignable_v<T>,
        "T must be nothrow move assignable");

    // queue가 소유한 객체를 정리하는 과정에서 destructor가 예외를 발생시키면
    // queue의 lifetime 자체를 안전하게 종료할 수 없으므로 noexcept를 요구합니다.
    static_assert(
        std::is_nothrow_destructible_v<T>,
        "T must be nothrow destructible");

    //***************************************************************************
    // @brief SPSCLockFreeQueue 객체를 생성합니다.
    //***************************************************************************
    SPSCLockFreeQueue()
    {
        m_EnqueuePos.Value.store(0, std::memory_order_relaxed);
        m_DequeuePos.Value.store(0, std::memory_order_relaxed);
        m_Closed.store(false, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 소멸자. 남은 데이터를 모두 비웁니다.
    //
    // @details
    // 소멸자는 다른 producer/consumer가 더 이상 queue를 사용하지 않는다는
    // 전제하에 호출되어야 합니다.
    //
    // TryPop()을 사용하지 않고 남아 있는 객체를 직접 destroy하므로
    // T가 default constructible일 필요가 없습니다.
    //***************************************************************************
    ~SPSCLockFreeQueue()
    {
        const std::size_t dequeuePos =
            m_DequeuePos.Value.load(std::memory_order_relaxed);

        const std::size_t enqueuePos =
            m_EnqueuePos.Value.load(std::memory_order_relaxed);

        std::size_t pos = dequeuePos;

        while( pos != enqueuePos )
        {
            Cell& cell = m_Buffer[pos & kIndexMask];

            cell.GetDataPtr()->~T();

            ++pos;
        }
    }

    SPSCLockFreeQueue(const SPSCLockFreeQueue&) = delete;
    SPSCLockFreeQueue& operator=(const SPSCLockFreeQueue&) = delete;

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참 또는 Close 상태
    //***************************************************************************
    bool TryPush(const T& value)
    {
        return EmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참 또는 Close 상태
    //***************************************************************************
    bool TryPush(T&& value)
    {
        return EmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: Close 상태로 인해 삽입되지 않음
    //***************************************************************************
    bool Push(const T& value)
    {
        return BlockingEmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: Close 상태로 인해 삽입되지 않음
    //***************************************************************************
    bool Push(T&& value)
    {
        return BlockingEmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음
    //***************************************************************************
    bool TryPop(T& outValue)
    {
        // consumer만 m_DequeuePos를 수정하므로 relaxed load로 충분합니다.
        const std::size_t pos =
            m_DequeuePos.Value.load(std::memory_order_relaxed);

        // producer가 publish한 enqueue position을 acquire로 읽습니다.
        //
        // producer:
        //     T 생성
        //     m_EnqueuePos.store(..., release)
        //
        // consumer:
        //     m_EnqueuePos.load(..., acquire)
        //
        // 따라서 consumer가 객체의 완전한 초기화 이후 데이터를 읽을 수 있습니다.
        const std::size_t enqueuePos =
            m_EnqueuePos.Value.load(std::memory_order_acquire);

        if( pos == enqueuePos )
            return false;

        Cell& cell = m_Buffer[pos & kIndexMask];

        // m_DequeuePos를 증가시키기 전에 데이터를 outValue로 이동합니다.
        //
        // T는 nothrow move assignable이므로 여기에서 예외가 발생하지 않습니다.
        outValue = std::move(*cell.GetDataPtr());

        // 객체의 lifetime을 종료합니다.
        cell.GetDataPtr()->~T();

        // consumer가 해당 slot을 완전히 처리했음을 producer에게 publish합니다.
        //
        // producer:
        //     m_DequeuePos.load(acquire)
        //
        // 이 acquire와 대응하여 consumer의 release store 이전 작업이
        // producer에게 올바르게 관찰됩니다.
        m_DequeuePos.Value.store(
            pos + 1,
            std::memory_order_release);

        NotifyNotFull();

        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: Close 상태이며 큐가 비어있어
    //         더 이상 꺼낼 데이터가 없음
    //
    // @details
    // Close() 호출 후 큐에 남아있는 데이터는 먼저 drain할 수 있도록
    // 구현되어 있습니다. drain이 끝난 이후에는 false를 반환합니다.
    //***************************************************************************
    bool Pop(T& outValue)
    {
        // 짧은 대기는 context switch보다 spin이 유리할 수 있습니다.
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( TryPop(outValue) )
                return true;

            if( IsClosed() && !HasData() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;;)
        {
            std::unique_lock<std::mutex> lock(m_NotEmptyMutex);

            // waiter 등록과 predicate 확인을 같은 mutex 영역에서 수행합니다.
            //
            // producer는 waiter가 존재할 경우 같은 mutex를 잡은 뒤 notify하기
            // 때문에 notify와 wait 사이의 lost wakeup을 방지할 수 있습니다.
            //
            // Close()도 동일한 mutex를 잡은 뒤 notify하므로 predicate 평가
            // 직후, wait() 등록 직전 구간에서의 lost wakeup을 방지합니다.
            m_WaitingPoppers.fetch_add(
                1,
                std::memory_order_relaxed);

            m_NotEmptyCv.wait(
                lock,
                [this]() noexcept
                {
                    return IsClosed() || HasData();
                });

            m_WaitingPoppers.fetch_sub(
                1,
                std::memory_order_relaxed);

            lock.unlock();

            // SPSC이므로 predicate가 true가 된 이후 다른 consumer가 데이터를
            // 가져갈 수 없습니다. 따라서 데이터가 있었다면 정상적으로
            // 성공해야 합니다.
            if( TryPop(outValue) )
                return true;

            // Close 상태이고 큐가 완전히 drain되었다면 종료합니다.
            if( IsClosed() && !HasData() )
                return false;
        }
    }

    //***************************************************************************
    // @brief 대략적인 현재 큐 크기를 반환합니다.
    // @return std::size_t 대략적인 요소 개수
    //
    // @details
    // 정상 동작 중에는 dequeuePos <= enqueuePos가 항상 성립하지만,
    // 다른 lock-free 큐 구현체들과의 일관성 및 향후 변경에 대한 방어적
    // 안전장치로 클램프를 유지합니다.
    //***************************************************************************
    std::size_t SizeApprox() const noexcept
    {
        const std::size_t enqueuePos =
            m_EnqueuePos.Value.load(std::memory_order_relaxed);

        const std::size_t dequeuePos =
            m_DequeuePos.Value.load(std::memory_order_relaxed);

        return (enqueuePos >= dequeuePos)
            ? (enqueuePos - dequeuePos)
            : 0;
    }

    //***************************************************************************
    // @brief 큐의 최대 용량을 반환합니다.
    // @return constexpr std::size_t 큐 용량
    //***************************************************************************
    constexpr std::size_t GetCapacity() const noexcept
    {
        return Capacity;
    }

    //***************************************************************************
    // @brief 큐를 종료 상태로 전환합니다.
    //
    // @details
    // Close() 이후 새로운 Push는 실패합니다.
    // 이미 큐에 저장된 데이터는 계속 Pop할 수 있으며,
    // 모든 데이터가 소비된 이후 Pop()은 false를 반환합니다.
    //
    // 대기 중인 producer/consumer는 모두 깨워 종료 상태를 확인할 수 있습니다.
    //
    // Close()는 여러 번 호출해도 안전합니다.
    //***************************************************************************
    void Close() noexcept
    {
        const bool wasClosed =
            m_Closed.exchange(true, std::memory_order_acq_rel);

        if( wasClosed )
            return;

        // waiter가 predicate 평가 직후, 아직 wait()에 등록되기 전인
        // 구간에서 notify가 유실되지 않도록 각 mutex를 잡은 뒤 통보합니다.
        //
        // Close는 lifecycle 이벤트이므로 빈번한 hot path가 아니며,
        // mutex를 잡는 비용은 무시할 수 있습니다.
        {
            std::lock_guard<std::mutex> lock(m_NotEmptyMutex);
            m_NotEmptyCv.notify_all();
        }
        {
            std::lock_guard<std::mutex> lock(m_NotFullMutex);
            m_NotFullCv.notify_all();
        }
    }

    //***************************************************************************
    // @brief 큐가 종료 상태인지 확인합니다.
    // @return true: 종료됨, false: 동작 중
    //***************************************************************************
    bool IsClosed() const noexcept
    {
        return m_Closed.load(std::memory_order_acquire);
    }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 큐에 데이터가 존재하는지 확인합니다.
    // @return true: 데이터가 존재함, false: 큐가 비어있음
    //***************************************************************************
    bool HasData() const noexcept
    {
        const std::size_t dequeuePos =
            m_DequeuePos.Value.load(std::memory_order_relaxed);

        const std::size_t enqueuePos =
            m_EnqueuePos.Value.load(std::memory_order_acquire);

        return dequeuePos != enqueuePos;
    }

    //***************************************************************************
    // @brief 큐에 빈 공간이 존재하는지 확인합니다.
    // @return true: 빈 공간 존재, false: 큐가 가득 참
    //***************************************************************************
    bool HasFreeSlot() const noexcept
    {
        const std::size_t enqueuePos =
            m_EnqueuePos.Value.load(std::memory_order_relaxed);

        const std::size_t dequeuePos =
            m_DequeuePos.Value.load(std::memory_order_acquire);

        return (enqueuePos - dequeuePos) < Capacity;
    }

    //***************************************************************************
    // @brief 대기 중인 소비자(popper)에게 데이터가 있음을 알립니다.
    //
    // @details
    // waiter가 없는 일반적인 hot path에서는 mutex를 잡지 않습니다.
    //
    // waiter가 존재하는 경우에는 wait와 동일한 mutex를 사용하여
    // notification과 sleep 사이의 lost wakeup을 방지합니다.
    //***************************************************************************
    void NotifyNotEmpty()
    {
        if( m_WaitingPoppers.load(std::memory_order_acquire) == 0 )
            return;

        std::lock_guard<std::mutex> lock(m_NotEmptyMutex);

        m_NotEmptyCv.notify_one();
    }

    //***************************************************************************
    // @brief 대기 중인 생산자(pusher)에게 빈 공간이 생겼음을 알립니다.
    //
    // @details
    // waiter가 없는 일반적인 hot path에서는 mutex를 잡지 않습니다.
    //
    // waiter가 존재하는 경우에는 wait와 동일한 mutex를 사용하여
    // notification과 sleep 사이의 lost wakeup을 방지합니다.
    //***************************************************************************
    void NotifyNotFull()
    {
        if( m_WaitingPushers.load(std::memory_order_acquire) == 0 )
            return;

        std::lock_guard<std::mutex> lock(m_NotFullMutex);

        m_NotFullCv.notify_one();
    }

    //***************************************************************************
    // @brief 블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: Close 상태로 인해 삽입되지 않음
    //***************************************************************************
    template <typename U>
    bool BlockingEmplacePush(U&& value)
    {
        // 짧은 대기는 context switch보다 spin이 유리할 수 있습니다.
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( EmplacePush(std::forward<U>(value)) )
                return true;

            if( IsClosed() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;;)
        {
            std::unique_lock<std::mutex> lock(m_NotFullMutex);

            // waiter 등록과 predicate 확인을 같은 mutex 영역에서 수행합니다.
            //
            // Close()도 동일한 mutex를 잡은 뒤 notify하므로 lost wakeup을
            // 방지합니다.
            m_WaitingPushers.fetch_add(
                1,
                std::memory_order_relaxed);

            m_NotFullCv.wait(
                lock,
                [this]() noexcept
                {
                    return IsClosed() || HasFreeSlot();
                });

            m_WaitingPushers.fetch_sub(
                1,
                std::memory_order_relaxed);

            lock.unlock();

            if( IsClosed() )
                return false;

            // SPSC이므로 predicate가 true가 된 이후 다른 producer가
            // 해당 slot을 선점할 수 없습니다.
            if( EmplacePush(std::forward<U>(value)) )
                return true;
        }
    }

    //***************************************************************************
    // @brief 논블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 가득 참 또는 Close 상태
    //***************************************************************************
    template <typename U>
    bool EmplacePush(U&& value)
    {
        // Close 이후 새로운 데이터를 추가하지 않습니다.
        //
        // producer가 하나뿐이므로 이 검사와 실제 슬롯 예약 사이에
        // 다른 producer가 끼어들 여지가 없어 별도의 재확인이 필요 없습니다.
        if( m_Closed.load(std::memory_order_acquire) )
            return false;

        // producer만 m_EnqueuePos를 수정하므로 relaxed load로 충분합니다.
        const std::size_t pos =
            m_EnqueuePos.Value.load(std::memory_order_relaxed);

        // consumer가 release store한 dequeue position을 acquire로 읽습니다.
        //
        // consumer:
        //     데이터 이동
        //     destructor
        //     m_DequeuePos.store(..., release)
        //
        // producer:
        //     m_DequeuePos.load(..., acquire)
        //
        // 따라서 producer가 해당 slot을 재사용하기 전에 이전 객체의
        // lifetime 종료가 관찰되도록 합니다.
        const std::size_t dequeuePos =
            m_DequeuePos.Value.load(std::memory_order_acquire);

        // producer와 consumer position의 차이가 Capacity 이상이면 full입니다.
        if( (pos - dequeuePos) >= Capacity )
            return false;

        Cell& cell = m_Buffer[pos & kIndexMask];

        // 매우 중요:
        // 객체 생성이 성공하기 전에 m_EnqueuePos를 변경하지 않습니다.
        //
        // T의 constructor/copy/move constructor가 예외를 발생시키더라도
        // queue의 logical state는 그대로 유지됩니다.
        new (cell.GetDataPtr()) T(std::forward<U>(value));

        // 객체 생성이 성공한 이후에만 producer의 position을 publish합니다.
        //
        // consumer의 acquire load와 synchronize되어 객체가 완전히 생성된
        // 이후 consumer가 해당 객체를 읽을 수 있도록 합니다.
        m_EnqueuePos.Value.store(
            pos + 1,
            std::memory_order_release);

        NotifyNotEmpty();

        return true;
    }

private:
    static constexpr std::size_t kIndexMask = Capacity - 1;

    //***************************************************************************
    // 큐의 실제 데이터를 저장하는 링 버퍼 배열입니다.
    //
    // SPSC에서는 각 Cell에 Sequence가 필요하지 않으므로 Cell을 단순화하여
    // 불필요한 cache-line padding과 메모리 사용량을 제거했습니다.
    //***************************************************************************
    Cell m_Buffer[Capacity];

    //***************************************************************************
    // producer 전용 enqueue position.
    //
    // producer는 이 값을 지속적으로 수정하고 consumer는 acquire로 읽습니다.
    //***************************************************************************
    AlignedAtomicPosition m_EnqueuePos;

    //***************************************************************************
    // consumer 전용 dequeue position.
    //
    // consumer는 이 값을 지속적으로 수정하고 producer는 acquire로 읽습니다.
    //***************************************************************************
    AlignedAtomicPosition m_DequeuePos;

    //***************************************************************************
    // 큐가 비어있을 때 blocking consumer를 위한 동기화 객체입니다.
    //
    // 일반적인 lock-free hot path에서는 접근하지 않습니다.
    //***************************************************************************
    std::mutex m_NotEmptyMutex;
    std::condition_variable m_NotEmptyCv;

    //***************************************************************************
    // 큐가 가득 찼을 때 blocking producer를 위한 동기화 객체입니다.
    //
    // 일반적인 lock-free hot path에서는 접근하지 않습니다.
    //***************************************************************************
    std::mutex m_NotFullMutex;
    std::condition_variable m_NotFullCv;

    //***************************************************************************
    // 현재 대기 중인 소비자 및 생산자의 수입니다.
    //
    // Notify 함수에서는 mutex를 획득하기 전에 이 값을 확인하여
    // waiter가 없는 hot path에서는 불필요한 mutex lock을 피합니다.
    //
    // producer와 consumer가 각각 다른 thread에서 갱신하므로, 같은 캐시라인에
    // 놓여 false sharing이 발생하지 않도록 서로 분리합니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<int> m_WaitingPoppers{ 0 };
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<int> m_WaitingPushers{ 0 };

    //***************************************************************************
    // 큐 종료 여부입니다.
    //
    // false -> 정상 동작
    // true  -> 새로운 Push 금지, 기존 데이터 drain 허용
    //
    // producer/consumer 모두가 읽으므로 별도의 cache line에 배치하여
    // false sharing을 줄입니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<bool> m_Closed{ false };
};

#endif // UC_SPSCLOCKFREEQUEUE_H
