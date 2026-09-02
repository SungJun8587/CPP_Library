
//***************************************************************************
// SPMCLockFreeQueue.h : interface for the SPMCLockFreeQueue class.
//
//***************************************************************************

#ifndef UC_SPMCLOCKFREEQUEUE_H
#define UC_SPMCLOCKFREEQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <utility>

//***************************************************************************
// @class SPMCLockFreeQueue
// @brief Bounded Single-Producer / Multi-Consumer lock-free 큐
//
// @details
// 단일 producer만 존재한다는 제약을 활용해, producer는 CAS 없이
// 단순히 인덱스를 증가시키고, consumer만 CAS로 경쟁합니다.
//
// 주요 사용처:
//  - 이벤트 루프 (단일 쓰레드가 이벤트를 생산, 여러 쓰레드가 소비)
//  - 게임 서버의 이벤트 브로드캐스트
//  - 로그 처리 (단일 쓰레드 기록 → 여러 쓰레드 분석)
//
// 특징:
//  - producer 경로 단순화 → 성능 향상
//  - consumer는 CAS 필요
//  - blocking 정책은 MPMC와 동일
//  - Close()를 통한 종료 지원
//  - T 생성/이동 대입 예외 발생 시 queue 구조를 최대한 보존
//
// 주의:
//  - Producer는 반드시 하나의 쓰레드만 사용해야 합니다.
//  - 객체의 소멸과 동시에 다른 쓰레드에서 queue를 사용하는 것은 허용하지 않습니다.
//***************************************************************************
template <typename T, std::size_t Capacity>
class SPMCLockFreeQueue
{
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(
        (Capacity& (Capacity - 1)) == 0,
        "Capacity must be a power of two");

private:
    //***************************************************************************
    // @struct Cell
    // @brief Queue의 개별 슬롯
    //
    // @details
    // Sequence를 이용하여 producer와 consumer 사이의 publish/release
    // 상태를 관리합니다.
    //
    // 각 Cell을 cache line 단위로 정렬하여 서로 다른 consumer가
    // 서로 다른 Cell을 처리할 때 false sharing을 줄입니다.
    //***************************************************************************
    struct alignas(LFQ_CACHE_LINE_SIZE) Cell
    {
        std::atomic<std::size_t> Sequence;

        alignas(T)
            unsigned char Storage[sizeof(T)];

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

public:
    //***************************************************************************
    // @brief SPMCLockFreeQueue 객체를 생성합니다.
    //***************************************************************************
    SPMCLockFreeQueue() noexcept
    {
        for( std::size_t i = 0; i < Capacity; ++i )
        {
            m_Buffer[i].Sequence.store(
                i,
                std::memory_order_relaxed);
        }

        m_EnqueuePos.store(
            0,
            std::memory_order_relaxed);

        m_DequeuePos.store(
            0,
            std::memory_order_relaxed);

        m_Closed.store(
            false,
            std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 소멸자. 남은 데이터를 모두 비웁니다.
    //
    // @details
    // 소멸 시점에는 다른 쓰레드가 queue를 사용하지 않는다는 전제하에
    // TryPop()을 사용하지 않고 살아있는 객체를 직접 소멸시킵니다.
    //
    // 이를 통해 T가 default constructible 또는 move assignable일 필요가
    // 없으며, destructor에서 T의 대입 연산자 예외에 의존하지 않습니다.
    //***************************************************************************
    ~SPMCLockFreeQueue()
    {
        const std::size_t deq =
            m_DequeuePos.load(std::memory_order_relaxed);

        const std::size_t enq =
            m_EnqueuePos.load(std::memory_order_relaxed);

        for( std::size_t pos = deq; pos < enq; ++pos )
        {
            Cell& cell = m_Buffer[pos & kIndexMask];

            // 정상적으로 publish된 객체만 소멸시킵니다.
            //
            // Producer의 T 생성자가 예외를 발생시키면 EnqueuePos는
            // 증가하지 않으므로 일반적으로 이 검사는 true가 됩니다.
            const std::size_t seq =
                cell.Sequence.load(std::memory_order_relaxed);

            if( seq == (pos + 1) )
            {
                cell.GetDataPtr()->~T();
            }
        }
    }

    SPMCLockFreeQueue(const SPMCLockFreeQueue&) = delete;
    SPMCLockFreeQueue& operator=(const SPMCLockFreeQueue&) = delete;

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
    //
    // @details
    // Close()가 호출되면 더 이상 삽입하지 않고 false를 반환합니다.
    // 반환값을 통해 호출자는 Close 이후 Push가 실제로 수행되었는지
    // 확인할 수 있습니다.
    //***************************************************************************
    bool Push(const T& value)
    {
        return BlockingEmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: Close 상태로 인해 삽입되지 않음
    //
    // @details
    // Close()가 호출되면 더 이상 삽입하지 않고 false를 반환합니다.
    //***************************************************************************
    bool Push(T&& value)
    {
        return BlockingEmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음
    //
    // @details
    // outValue 대입 연산자가 예외를 발생시키더라도 해당 Cell을 반드시
    // release하여 queue가 영구적으로 막히지 않도록 합니다.
    //
    // 단, outValue 대입 자체가 실패한 경우 해당 queue element는
    // 정상적으로 반환할 수 없으므로 폐기됩니다.
    //***************************************************************************
    bool TryPop(T& outValue)
    {
        Cell* cell = nullptr;

        std::size_t pos =
            m_DequeuePos.load(std::memory_order_relaxed);

        for( ;; )
        {
            cell = &m_Buffer[pos & kIndexMask];

            const std::size_t seq =
                cell->Sequence.load(std::memory_order_acquire);

            const intptr_t diff =
                static_cast<intptr_t>(seq) -
                static_cast<intptr_t>(pos + 1);

            if( diff == 0 )
            {
                if( m_DequeuePos.compare_exchange_weak(
                    pos,
                    pos + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed) )
                {
                    break;
                }

                // CAS 실패 시 compare_exchange_weak이 pos를
                // 최신 값으로 갱신해주므로 별도의 load가 필요 없습니다.
                continue;
            }

            if( diff < 0 )
            {
                return false;
            }

            // 다른 consumer가 앞선 위치를 진행했거나 producer가
            // 현재 위치를 아직 publish하는 중인 경우 최신 dequeue
            // position을 다시 확인합니다.
            pos = m_DequeuePos.load(
                std::memory_order_relaxed);
        }

        // 여기부터 해당 Cell은 현재 consumer가 독점적으로 소유합니다.
        //
        // outValue의 move assignment가 예외를 발생시키더라도
        // Cell을 반드시 반환해야 합니다.
        try
        {
            outValue = std::move(*cell->GetDataPtr());
        }
        catch( ... )
        {
            cell->GetDataPtr()->~T();

            cell->Sequence.store(
                pos + Capacity,
                std::memory_order_release);

            NotifyNotFull();

            throw;
        }

        cell->GetDataPtr()->~T();

        // Cell을 다음 producer가 재사용할 수 있도록 publish합니다.
        cell->Sequence.store(
            pos + Capacity,
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
    // Close()가 호출되고 queue가 비어 있으면 false를 반환합니다.
    //
    // Close() 시점에 이미 queue에 남아있는 데이터는 먼저 drain할 수
    // 있도록 구현되어 있습니다.
    //
    // 반환값을 통해 호출자는 while (queue.Pop(item)) { ... } 와 같은
    // 표준적인 소비 루프를 작성할 수 있습니다. outValue는 반환값이
    // true인 경우에만 유효합니다.
    //***************************************************************************
    bool Pop(T& outValue)
    {
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( TryPop(outValue) )
                return true;

            if( IsClosed() && IsEmptyApprox() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;; )
        {
            std::unique_lock<std::mutex> lock(m_NotEmptyMutex);

            m_WaitingPoppers.fetch_add(
                1,
                std::memory_order_relaxed);

            m_NotEmptyCv.wait(
                lock,
                [this]()
                {
                    return IsClosed() || !IsEmptyApprox();
                });

            m_WaitingPoppers.fetch_sub(
                1,
                std::memory_order_relaxed);

            lock.unlock();

            if( TryPop(outValue) )
                return true;

            // Close 상태이고 queue가 완전히 drain되었다면 종료합니다.
            if( IsClosed() && IsEmptyApprox() )
                return false;
        }
    }

    //***************************************************************************
    // @brief 큐를 종료 상태로 전환합니다.
    //
    // @details
    // Close 이후 새로운 Push는 거부됩니다.
    //
    // 이미 queue에 들어간 데이터는 Pop을 통해 drain할 수 있습니다.
    // queue가 비어있는 상태에서 대기 중인 Pop은 즉시 깨워집니다.
    //
    // Close()는 여러 번 호출해도 안전합니다.
    //***************************************************************************
    void Close() noexcept
    {
        m_Closed.store(
            true,
            std::memory_order_release);

        // NotifyNotEmpty()/NotifyNotFull()과 동일한 이유로, waiter가
        // predicate 평가 직후 아직 wait()에 등록되기 전인 구간에서
        // notify가 유실되지 않도록 각 mutex를 잡은 뒤 통보합니다.
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
    // @return true: Close 상태, false: 실행 상태
    //***************************************************************************
    bool IsClosed() const noexcept
    {
        return m_Closed.load(
            std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Close 상태를 해제하고 다시 사용할 수 있도록 합니다.
    //
    // @details
    // Reset()은 queue를 사용하는 다른 producer/consumer가 없는 상태에서
    // 호출해야 합니다.
    //
    // 일반적인 생산/소비 lifecycle에서는 Close 후 새로운 queue 객체를
    // 생성하는 방식이 더 안전합니다.
    //
    // Reset()은 position이나 Cell을 초기화하지 않습니다.
    // 따라서 기존 queue가 완전히 drain된 상태에서만 사용해야 합니다.
    //***************************************************************************
    void Reset() noexcept
    {
        m_Closed.store(
            false,
            std::memory_order_release);
    }

    //***************************************************************************
    // @brief 대략적인 현재 큐 크기를 반환합니다.
    // @return std::size_t 대략적인 요소 개수
    //
    // @details
    // 다른 thread가 동시에 Push/Pop하는 동안 호출할 경우 정확한
    // snapshot을 보장하지 않습니다.
    //***************************************************************************
    std::size_t SizeApprox() const noexcept
    {
        const std::size_t enq =
            m_EnqueuePos.load(std::memory_order_relaxed);

        const std::size_t deq =
            m_DequeuePos.load(std::memory_order_relaxed);

        return (enq >= deq)
            ? (enq - deq)
            : 0;
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 대략적으로 확인합니다.
    // @return true: 비어있음, false: 데이터 존재
    //***************************************************************************
    bool IsEmptyApprox() const noexcept
    {
        return SizeApprox() == 0;
    }

    //***************************************************************************
    // @brief 큐가 가득 찼는지 대략적으로 확인합니다.
    // @return true: 가득 참, false: 빈 공간 존재
    //***************************************************************************
    bool IsFullApprox() const noexcept
    {
        return SizeApprox() >= Capacity;
    }

    //***************************************************************************
    // @brief 큐의 최대 용량을 반환합니다.
    // @return constexpr std::size_t 큐 용량
    //***************************************************************************
    constexpr std::size_t GetCapacity() const noexcept
    {
        return Capacity;
    }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 대기 중인 소비자(popper)에게 데이터가 있음을 알립니다.
    //
    // @details
    // waiter의 fetch_add + predicate 평가 + wait() 등록은 m_NotEmptyMutex를
    // 잡은 채로 이루어집니다. notify_one()도 동일한 mutex를 잡은 뒤 호출해야만
    // "predicate가 false로 평가된 직후, 아직 wait()에 등록되기 전" 구간에서
    // notify가 유실되는 lost wakeup을 막을 수 있습니다.
    //***************************************************************************
    void NotifyNotEmpty() noexcept
    {
        if( m_WaitingPoppers.load(
            std::memory_order_relaxed) == 0 )
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_NotEmptyMutex);

        m_NotEmptyCv.notify_one();
    }

    //***************************************************************************
    // @brief 대기 중인 생산자(pusher)에게 빈 공간이 생겼음을 알립니다.
    //
    // @details
    // NotifyNotEmpty()와 동일한 이유로 m_NotFullMutex를 잡은 뒤 notify합니다.
    //***************************************************************************
    void NotifyNotFull() noexcept
    {
        if( m_WaitingPushers.load(
            std::memory_order_relaxed) == 0 )
        {
            return;
        }

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
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( EmplacePush(std::forward<U>(value)) )
                return true;

            if( IsClosed() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;; )
        {
            std::unique_lock<std::mutex> lock(m_NotFullMutex);

            m_WaitingPushers.fetch_add(
                1,
                std::memory_order_relaxed);

            m_NotFullCv.wait(
                lock,
                [this]()
                {
                    return IsClosed() || !IsFullApprox();
                });

            m_WaitingPushers.fetch_sub(
                1,
                std::memory_order_relaxed);

            lock.unlock();

            if( IsClosed() )
                return false;

            if( EmplacePush(std::forward<U>(value)) )
                return true;
        }
    }

    //***************************************************************************
    // @brief 논블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 가득 참 또는 Close 상태
    //
    // @details
    // 단일 producer 전용: CAS 없이 relaxed 원자적 load/store만 사용합니다.
    //
    // 중요한 예외 안전성 규칙:
    // T의 생성자가 성공하기 전에는 m_EnqueuePos를 증가시키지 않습니다.
    // 따라서 T 생성자가 예외를 발생시켜도 producer position이 오염되지
    // 않습니다.
    //***************************************************************************
    template <typename U>
    bool EmplacePush(U&& value)
    {
        // Close 이후 새로운 데이터를 추가하지 않습니다.
        if( m_Closed.load(
            std::memory_order_acquire) )
        {
            return false;
        }

        // 단일 producer 전용: CAS 없이 relaxed atomic load만 사용합니다.
        //
        // SizeApprox가 다른 thread에서 이 값을 읽으므로 atomic이어야 하며,
        // 단일 writer이므로 CAS 없는 relaxed load/store로 충분합니다.
        const std::size_t pos =
            m_EnqueuePos.load(
                std::memory_order_relaxed);

        Cell* cell =
            &m_Buffer[pos & kIndexMask];

        const std::size_t seq =
            cell->Sequence.load(
                std::memory_order_acquire);

        const intptr_t diff =
            static_cast<intptr_t>(seq) -
            static_cast<intptr_t>(pos);

        if( diff != 0 )
        {
            return false;
        }

        // 중요:
        // m_EnqueuePos를 먼저 증가시키면 T 생성자가 예외를 발생시켰을 때
        // producer position이 실제 객체 상태보다 앞서가게 됩니다.
        //
        // 따라서 반드시 T 생성 성공 이후에 EnqueuePos를 갱신합니다.
        new (cell->GetDataPtr())
            T(std::forward<U>(value));

        // Producer가 단 하나이므로 CAS가 필요하지 않습니다.
        //
        // EnqueuePos는 통계/SizeApprox 및 다음 producer 위치에 사용되며,
        // 실제 데이터 publish는 아래 Sequence release store가 담당합니다.
        m_EnqueuePos.store(
            pos + 1,
            std::memory_order_relaxed);

        // Sequence release가 실제 데이터 publish 지점입니다.
        //
        // Consumer는 acquire load를 통해 이 store 이전에 생성된 T 객체를
        // 안전하게 관찰할 수 있습니다.
        cell->Sequence.store(
            pos + 1,
            std::memory_order_release);

        NotifyNotEmpty();

        return true;
    }

private:
    static constexpr std::size_t kIndexMask =
        Capacity - 1;

    //***************************************************************************
    // @brief 큐의 실제 데이터를 저장하는 링 버퍼 배열
    //
    // @details
    // 각 Cell을 cache line 단위로 분리하여 여러 consumer가 서로 다른
    // Cell을 처리할 때 false sharing을 줄입니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        Cell m_Buffer[Capacity];

    //***************************************************************************
    // @brief 데이터가 삽입될 다음 위치
    //
    // @details
    // 단일 producer만 변경하므로 CAS가 필요하지 않습니다.
    // cache line을 분리하여 dequeue position과의 false sharing을 방지합니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<std::size_t> m_EnqueuePos;

    //***************************************************************************
    // @brief 데이터가 추출될 다음 위치
    //
    // @details
    // 여러 consumer가 동시에 접근하므로 CAS를 사용합니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<std::size_t> m_DequeuePos;

    //***************************************************************************
    // @brief queue 종료 상태
    //
    // @details
    // Close() 호출 이후 새로운 Push를 차단하고 blocking thread를 깨웁니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<bool> m_Closed{ false };

    //***************************************************************************
    // @brief 큐가 비어있거나 가득 찼을 때 블로킹을 위한 뮤텍스
    //
    // @details
    // Hot path가 아니라 blocking path에서만 사용됩니다.
    //***************************************************************************
    std::mutex m_NotEmptyMutex;
    std::mutex m_NotFullMutex;

    //***************************************************************************
    // @brief 큐 상태 변화(비어있지 않음 / 가득 차지 않음)를 통보하는 조건 변수
    //***************************************************************************
    std::condition_variable m_NotEmptyCv;
    std::condition_variable m_NotFullCv;

    //***************************************************************************
    // @brief 현재 대기 중인 소비자 thread 수
    //
    // @details
    // 별도의 cache line에 배치하여 blocking 경로에서의
    // false sharing을 줄입니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<int> m_WaitingPoppers{ 0 };

    //***************************************************************************
    // @brief 현재 대기 중인 생산자 thread 수
    //
    // @details
    // 별도의 cache line에 배치하여 blocking 경로에서의
    // false sharing을 줄입니다.
    //***************************************************************************
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<int> m_WaitingPushers{ 0 };
};

#endif // UC_SPMCLOCKFREEQUEUE_H
