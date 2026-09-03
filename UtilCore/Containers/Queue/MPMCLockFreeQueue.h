
//***************************************************************************
// MPMCLockFreeQueue.h : interface for the MPMCLockFreeQueue class.
//
//***************************************************************************

#ifndef UC_MPMCLOCKFREEQUEUE_H
#define UC_MPMCLOCKFREEQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

//***************************************************************************
// @class MPMCLockFreeQueue
// @brief Bounded Multi-Producer / Multi-Consumer lock-free 큐
//
// @details
// Dmitry Vyukov 알고리즘 기반으로, 여러 producer와 여러 consumer가
// 동시에 접근할 수 있는 범용 큐입니다.
// 슬롯 단위 CAS로 push/pop 소유권을 조정하며, 전역 락은 사용하지 않습니다.
//
// 주요 사용처:
//  - 네트워크 패킷 처리 (여러 수신 스레드 → 여러 처리 스레드)
//  - 멀티스레드 작업 큐 (스레드 풀)
//  - 고성능 로그/메시지 큐
//
// 특징:
//  - producer/consumer 모두 CAS 경쟁
//  - 범용적이며 가장 일반적인 lock-free 큐
//  - blocking 경로는 짧은 스핀 후 condition_variable로 대기
//
// 종료:
//  - Close() 호출 후 새로운 데이터 삽입은 허용되지 않습니다.
//  - Close() 이후에도 큐에 남아있는 데이터는 정상적으로 소비할 수 있습니다.
//  - 큐가 닫히고 남은 데이터가 없으면 blocking Pop()은 false를 반환합니다.
//  - Close()는 대기 중인 모든 producer/consumer를 깨웁니다.
//  - Close()가 이미 IsClosed()==false를 확인하고 reservation 절차에
//    들어간 producer와 동시에 호출되면, 그 producer의 아이템은 정상
//    publish되어 Pop 가능합니다. 다만 이는 best-effort이며, Close()를
//    "모든 producer가 push를 끝낸 뒤"에 호출하는 것이 유일하게 보장되는
//    사용 방식입니다. 그렇지 않으면 극히 드문 스케줄링 하에서 Close()와
//    거의 동시에 시작된 push가 이미 종료를 관찰한 Pop()에 보이지 않을
//    수 있습니다.
//
// 주의:
//  - 객체의 소멸은 모든 producer/consumer가 종료된 이후에 수행해야 합니다.
//***************************************************************************
template <typename T, std::size_t Capacity>
class MPMCLockFreeQueue
{
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be a power of two");

private:
    struct alignas(LFQ_CACHE_LINE_SIZE) Cell
    {
        std::atomic<std::size_t> Sequence;
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

public:
    //***************************************************************************
    // @brief MPMCLockFreeQueue 객체를 생성합니다.
    //***************************************************************************
    MPMCLockFreeQueue()
    {
        for( std::size_t i = 0; i < Capacity; ++i )
            m_Buffer[i].Sequence.store(i, std::memory_order_relaxed);

        m_EnqueuePos.store(0, std::memory_order_relaxed);
        m_DequeuePos.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 소멸자. 남은 데이터를 모두 비웁니다.
    //***************************************************************************
    ~MPMCLockFreeQueue()
    {
        static_assert(
            std::is_nothrow_destructible_v<T>,
            "T destructor must be noexcept");

        DrainAndDestroy();
    }

    MPMCLockFreeQueue(const MPMCLockFreeQueue&) = delete;
    MPMCLockFreeQueue& operator=(const MPMCLockFreeQueue&) = delete;

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 차거나 종료됨
    //***************************************************************************
    [[nodiscard]] bool TryPush(const T& value)
    {
        static_assert(
            std::is_nothrow_copy_constructible_v<T>,
            "T copy constructor must be noexcept");

        return EmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 차거나 종료됨
    //***************************************************************************
    [[nodiscard]] bool TryPush(T&& value)
    {
        static_assert(
            std::is_nothrow_move_constructible_v<T>,
            "T move constructor must be noexcept");

        return EmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 종료됨
    //***************************************************************************
    [[nodiscard]] bool Push(const T& value)
    {
        static_assert(
            std::is_nothrow_copy_constructible_v<T>,
            "T copy constructor must be noexcept");

        return BlockingEmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 종료됨
    //***************************************************************************
    [[nodiscard]] bool Push(T&& value)
    {
        static_assert(
            std::is_nothrow_move_constructible_v<T>,
            "T move constructor must be noexcept");

        return BlockingEmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음 또는 종료 후 비어있음
    //***************************************************************************
    [[nodiscard]] bool TryPop(T& outValue)
    {
        static_assert(
            std::is_nothrow_move_assignable_v<T>,
            "T move assignment operator must be noexcept");

        Cell* cell;
        std::size_t pos = m_DequeuePos.load(std::memory_order_relaxed);

        for( ;;)
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
            }
            else if( diff < 0 )
            {
                return false;
            }
            else
            {
                pos = m_DequeuePos.load(std::memory_order_relaxed);
            }
        }

        outValue = std::move(*cell->GetDataPtr());

        cell->GetDataPtr()->~T();

        cell->Sequence.store(
            pos + Capacity,
            std::memory_order_release);

        NotifyNotFull();

        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    //***************************************************************************
    [[nodiscard]] bool Pop(T& outValue)
    {
        static_assert(
            std::is_nothrow_move_assignable_v<T>,
            "T move assignment operator must be noexcept");

        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( TryPop(outValue) )
                return true;

            if( IsClosed() && IsEmptyApprox() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;;)
        {
            {
                std::unique_lock<std::mutex> lock(m_NotEmptyMutex);

                if( IsClosed() && IsEmptyApprox() )
                    return false;

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
            }

            if( TryPop(outValue) )
                return true;

            // Close 상태이고 큐가 완전히 drain되었다면 종료합니다.
            //
            // TryPop() 실패 시점의 스냅샷이 아니라 이 시점에서 다시
            // IsEmptyApprox()를 평가하여, TryPop() 확인과 Close() 확인
            // 사이에 끼어든 push를 놓치지 않도록 합니다.
            if( IsClosed() && IsEmptyApprox() )
                return false;
        }
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
    //***************************************************************************
    void Close() noexcept
    {
        bool expected = false;

        if( m_Closed.compare_exchange_strong(
            expected,
            true,
            std::memory_order_release,
            std::memory_order_relaxed) )
        {
            // NotifyNotEmpty()/NotifyNotFull()과 동일한 이유로,
            // waiter가 predicate 평가 직후 아직 wait()에 등록되기 전인
            // 구간에서 notify가 유실되지 않도록 각 mutex를 잡은 뒤 통보합니다.
            {
                std::lock_guard<std::mutex> lock(m_NotEmptyMutex);
                m_NotEmptyCv.notify_all();
            }
            {
                std::lock_guard<std::mutex> lock(m_NotFullMutex);
                m_NotFullCv.notify_all();
            }
        }
    }

    //***************************************************************************
    // @brief 큐가 종료 상태인지 확인합니다.
    // @return true: 종료됨, false: 동작 중
    //***************************************************************************
    [[nodiscard]] bool IsClosed() const noexcept
    {
        return m_Closed.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief Close 상태를 해제하고 다시 사용할 수 있도록 합니다.
    //
    // @details
    // Reset()은 큐를 사용하는 다른 producer/consumer가 없는 상태에서
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
        m_Closed.store(false, std::memory_order_release);
    }

    //***************************************************************************
    // @brief 대략적인 현재 큐 크기를 반환합니다.
    // @return std::size_t 대략적인 요소 개수
    //***************************************************************************
    [[nodiscard]] std::size_t SizeApprox() const noexcept
    {
        const std::size_t enq =
            m_EnqueuePos.load(std::memory_order_relaxed);

        const std::size_t deq =
            m_DequeuePos.load(std::memory_order_relaxed);

        return (enq >= deq) ? (enq - deq) : 0;
    }

    //***************************************************************************
    // @brief 큐의 최대 용량을 반환합니다.
    // @return constexpr std::size_t 큐 용량
    //***************************************************************************
    [[nodiscard]] constexpr std::size_t GetCapacity() const noexcept
    {
        return Capacity;
    }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 대략적인 큐가 비어있는지 확인합니다.
    //***************************************************************************
    [[nodiscard]] bool IsEmptyApprox() const noexcept
    {
        return SizeApprox() == 0;
    }

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
        if( m_WaitingPoppers.load(std::memory_order_relaxed) == 0 )
            return;

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
        if( m_WaitingPushers.load(std::memory_order_relaxed) == 0 )
            return;

        std::lock_guard<std::mutex> lock(m_NotFullMutex);

        m_NotFullCv.notify_one();
    }

    //***************************************************************************
    // @brief 큐에 남아있는 데이터를 직접 소멸시킵니다.
    //
    // @details
    // 소멸자에서 호출되며, 이 시점에는 다른 스레드가 큐에 접근하지 않는다는
    // 전제입니다. TryPop()을 사용하지 않으므로 T의 기본 생성자나
    // move assignment 연산을 요구하지 않습니다.
    //***************************************************************************
    void DrainAndDestroy() noexcept
    {
        std::size_t pos =
            m_DequeuePos.load(std::memory_order_relaxed);

        for( ;; ++pos )
        {
            Cell& cell = m_Buffer[pos & kIndexMask];

            const std::size_t seq =
                cell.Sequence.load(std::memory_order_relaxed);

            if( seq != pos + 1 )
                break;

            cell.GetDataPtr()->~T();
        }
    }

    //***************************************************************************
    // @brief 블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    //***************************************************************************
    template <typename U>
    bool BlockingEmplacePush(U&& value)
    {
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( IsClosed() )
                return false;

            if( EmplacePush(std::forward<U>(value)) )
                return true;

            LFQ_CPU_PAUSE();
        }

        for( ;;)
        {
            {
                std::unique_lock<std::mutex> lock(m_NotFullMutex);

                if( IsClosed() )
                    return false;

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
            }

            if( IsClosed() )
                return false;

            if( EmplacePush(std::forward<U>(value)) )
                return true;
        }
    }

    //***************************************************************************
    // @brief 논블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 가득 참 또는 종료됨
    //***************************************************************************
    template <typename U>
    bool EmplacePush(U&& value)
    {
        static_assert(
            std::is_nothrow_constructible_v<T, U&&>,
            "T construction must be noexcept");

        if( IsClosed() )
            return false;

        Cell* cell;
        std::size_t pos =
            m_EnqueuePos.load(std::memory_order_relaxed);

        for( ;;)
        {
            cell = &m_Buffer[pos & kIndexMask];

            const std::size_t seq =
                cell->Sequence.load(std::memory_order_acquire);

            const intptr_t diff =
                static_cast<intptr_t>(seq) -
                static_cast<intptr_t>(pos);

            if( diff == 0 )
            {
                if( m_EnqueuePos.compare_exchange_weak(
                    pos,
                    pos + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed) )
                {
                    break;
                }
            }
            else if( diff < 0 )
            {
                return false;
            }
            else
            {
                pos = m_EnqueuePos.load(std::memory_order_relaxed);
            }

            if( IsClosed() )
                return false;
        }

        new (cell->GetDataPtr()) T(std::forward<U>(value));

        cell->Sequence.store(
            pos + 1,
            std::memory_order_release);

        NotifyNotEmpty();

        return true;
    }

    //***************************************************************************
    // @brief 대략적인 큐가 가득 찼는지 확인합니다.
    //***************************************************************************
    [[nodiscard]] bool IsFullApprox() const noexcept
    {
        return SizeApprox() >= Capacity;
    }

    static constexpr std::size_t kIndexMask = Capacity - 1;                // 큐 인덱스 계산용 마스크 (Capacity가 2의 거듭제곱일 때 모듈로 연산 대체)
    alignas(LFQ_CACHE_LINE_SIZE) Cell m_Buffer[Capacity];                  // 큐의 실제 데이터를 저장하는 링 버퍼 배열 (캐시 라인 정렬)
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_EnqueuePos;    // 데이터가 삽입될 다음 위치 (멀티 프로듀서 경쟁, 캐시 라인 정렬)
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_DequeuePos;    // 데이터가 추출될 다음 위치 (멀티 컨슈머 경쟁, 캐시 라인 정렬)
    std::mutex m_NotEmptyMutex, m_NotFullMutex;                            // 큐가 비어있거나 가득 찼을 때 블로킹을 위한 뮤텍스
    std::condition_variable m_NotEmptyCv, m_NotFullCv;                     // 큐 상태 변화(비어있지 않음 / 가득 차지 않음)를 통보하는 조건 변수

    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<int> m_WaitingPoppers{ 0 };                                // 현재 대기 중인 소비자 스레드 수

    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<int> m_WaitingPushers{ 0 };                                // 현재 대기 중인 생산자 스레드 수

    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<bool> m_Closed{ false };                                   // 큐 종료 여부 (Close() 호출 시 true)
};

#endif // ndef UC_MPMCLOCKFREEQUEUE_H
