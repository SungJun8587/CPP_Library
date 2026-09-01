
//***************************************************************************
// MPSCLockFreeQueue.h : interface for the MPSCLockFreeQueue class.
//
//***************************************************************************

#ifndef UC_MPSCLOCKFREEQUEUE_H
#define UC_MPSCLOCKFREEQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>

#include <atomic>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

//***************************************************************************
// @class MPSCLockFreeQueue
// @brief Bounded Multi-Producer / Single-Consumer lock-free 큐
//
// @details
// 여러 producer가 동시에 데이터를 넣고, 단일 consumer가 데이터를
// 소비하는 구조입니다. producer는 CAS로 경쟁하고, consumer는
// 단순히 인덱스를 증가시킵니다.
//
// 주요 사용처:
//  - 로깅 시스템 (여러 쓰레드가 로그 기록 → 단일 쓰레드 출력)
//  - 이벤트 큐 (여러 producer → 단일 처리 루프)
//  - 네트워크 수신 큐 (여러 수신 스레드 → 단일 처리 스레드)
//
// 특징:
//  - producer는 CAS 필요
//  - consumer는 단순 인덱스 증가
//  - 단일 소비자 환경에서 효율적
//  - C++20 atomic wait/notify를 사용하여 blocking 대기 가능
//
// 종료:
//  - Close() 호출 후 새로운 Push는 실패합니다.
//  - Close() 이전에 삽입된 데이터는 계속 Pop할 수 있습니다.
//  - Close() 이후 큐가 비어 있으면 Pop은 false를 반환합니다.
//
// 주의:
//  - T의 생성자는 noexcept여야 합니다.
//  - T의 move assignment는 noexcept여야 합니다.
//  - T의 destructor는 noexcept여야 합니다.
//  - 객체가 소멸되는 동안 다른 producer/consumer가 큐에 접근해서는 안 됩니다.
//  - C++20 std::atomic::wait/notify 기능을 사용합니다.
//***************************************************************************
template <typename T, std::size_t Capacity>
class MPSCLockFreeQueue
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

        const T* GetDataPtr() const noexcept
        {
            return reinterpret_cast<const T*>(Storage);
        }
    };

public:
    //***************************************************************************
    // @brief MPSCLockFreeQueue 객체를 생성합니다.
    //***************************************************************************
    MPSCLockFreeQueue() noexcept
        : m_EnqueuePos(0)
        , m_DequeuePos(0)
        , m_Closed(false)
    {
        for( std::size_t i = 0; i < Capacity; ++i )
            m_Buffer[i].Sequence.store(i, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 소멸자. 남은 데이터를 모두 비웁니다.
    //
    // @details
    // 소멸 시점에는 다른 producer/consumer가 큐에 접근하지 않는다는
    // 전제하에, 남아 있는 객체를 직접 파괴합니다.
    //
    // TryPop()을 이용하지 않으므로 T의 default constructor가 필요하지
    // 않습니다.
    //***************************************************************************
    ~MPSCLockFreeQueue()
    {
        static_assert(
            std::is_nothrow_destructible_v<T>,
            "T must be nothrow destructible");

        const std::size_t deq =
            m_DequeuePos.load(std::memory_order_relaxed);

        const std::size_t enq =
            m_EnqueuePos.load(std::memory_order_relaxed);

        for( std::size_t pos = deq; pos != enq; ++pos )
        {
            Cell& cell = m_Buffer[pos & kIndexMask];

            const std::size_t seq =
                cell.Sequence.load(std::memory_order_relaxed);

            // 소멸 시점에는 모든 예약된 position이 정상적으로 publish되어
            // 있어야 합니다. T 생성자가 noexcept라는 전제에 의해 보장됩니다.
            ASSERT_CRASH(seq == pos + 1);

            cell.GetDataPtr()->~T();
        }
    }

    MPSCLockFreeQueue(const MPSCLockFreeQueue&) = delete;
    MPSCLockFreeQueue& operator=(const MPSCLockFreeQueue&) = delete;

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 찼거나 종료됨
    //***************************************************************************
    bool TryPush(const T& value)
    {
        return EmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 찼거나 종료됨
    //***************************************************************************
    bool TryPush(T&& value)
    {
        return EmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 종료됨
    //***************************************************************************
    bool Push(const T& value)
    {
        return BlockingEmplacePush(value);
    }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 종료됨
    //***************************************************************************
    bool Push(T&& value)
    {
        return BlockingEmplacePush(std::move(value));
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음 또는 종료 후 비어있음
    //***************************************************************************
    bool TryPop(T& outValue)
    {
        static_assert(
            std::is_nothrow_move_assignable_v<T>,
            "T must be nothrow move assignable");

        // 단일 consumer 전용: CAS 없이 relaxed 원자적 load/store만 사용.
        const std::size_t pos =
            m_DequeuePos.load(std::memory_order_relaxed);

        Cell* cell = &m_Buffer[pos & kIndexMask];

        const std::size_t seq =
            cell->Sequence.load(std::memory_order_acquire);

        const intptr_t diff =
            static_cast<intptr_t>(seq) -
            static_cast<intptr_t>(pos + 1);

        if( diff != 0 )
            return false;

        // T의 move assignment가 noexcept라는 전제이므로,
        // 아래 상태 변경 이후 예외로 인해 Cell이 영구적으로 점유되는
        // 문제가 발생하지 않습니다.
        m_DequeuePos.store(pos + 1, std::memory_order_relaxed);

        outValue = std::move(*cell->GetDataPtr());

        cell->GetDataPtr()->~T();

        // Consumer가 Cell의 사용을 완료했음을 producer에게 publish합니다.
        cell->Sequence.store(
            pos + Capacity,
            std::memory_order_release);

        // Full 상태에서 대기 중인 producer를 깨웁니다.
        m_DequeuePos.notify_one();

        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 종료되어 더 이상 데이터가 없음
    //***************************************************************************
    bool Pop(T& outValue)
    {
        static_assert(
            std::is_nothrow_move_assignable_v<T>,
            "T must be nothrow move assignable");

        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( TryPop(outValue) )
                return true;

            if( IsClosedAndEmpty() )
                return false;

            LFQ_CPU_PAUSE();
        }

        for( ;;)
        {
            if( TryPop(outValue) )
                return true;

            if( IsClosedAndEmpty() )
                return false;

            // 현재 enqueue position을 관찰합니다.
            //
            // producer가 데이터를 publish하거나 Close()가 호출되면
            // 해당 atomic 값에 notify가 발생합니다.
            const std::size_t observed =
                m_EnqueuePos.load(std::memory_order_acquire);

            // wait 직전에 상태를 다시 확인합니다.
            // 값이 이미 변경되었다면 wait하지 않고 다시 loop합니다.
            if( TryPop(outValue) )
                return true;

            if( IsClosedAndEmpty() )
                return false;

            m_EnqueuePos.wait(
                observed,
                std::memory_order_acquire);
        }
    }

    //***************************************************************************
    // @brief 큐를 종료합니다.
    //
    // @details
    // Close 이후 새로운 Push는 실패합니다.
    // 이미 큐에 들어간 데이터는 정상적으로 Pop할 수 있습니다.
    // 대기 중인 producer/consumer는 즉시 깨워집니다.
    //
    // Close()는 idempotent입니다.
    //***************************************************************************
    void Close() noexcept
    {
        const bool wasClosed =
            m_Closed.exchange(true, std::memory_order_acq_rel);

        if( wasClosed )
            return;

        // 대기 중인 consumer를 깨웁니다.
        m_EnqueuePos.notify_all();

        // 대기 중인 producer를 깨웁니다.
        m_DequeuePos.notify_all();
    }

    //***************************************************************************
    // @brief 큐가 종료되었는지 확인합니다.
    // @return true: 종료됨, false: 동작 중
    //***************************************************************************
    bool IsClosed() const noexcept
    {
        return m_Closed.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 큐가 종료되었고 비어 있는지 확인합니다.
    // @return true: 종료되었으며 데이터가 없음
    //***************************************************************************
    bool IsClosedAndEmpty() const noexcept
    {
        if( !m_Closed.load(std::memory_order_acquire) )
            return false;

        const std::size_t enq =
            m_EnqueuePos.load(std::memory_order_acquire);

        const std::size_t deq =
            m_DequeuePos.load(std::memory_order_acquire);

        return enq == deq;
    }

    //***************************************************************************
    // @brief 대략적인 현재 큐 크기를 반환합니다.
    // @return std::size_t 대략적인 요소 개수
    //***************************************************************************
    std::size_t SizeApprox() const noexcept
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
    constexpr std::size_t GetCapacity() const noexcept
    {
        return Capacity;
    }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 종료됨
    //***************************************************************************
    template <typename U>
    bool BlockingEmplacePush(U&& value)
    {
        static_assert(
            std::is_nothrow_constructible_v<T, U&&>,
            "T must be nothrow constructible from U");

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
            if( EmplacePush(std::forward<U>(value)) )
                return true;

            if( IsClosed() )
                return false;

            // 현재 dequeue position을 관찰합니다.
            //
            // consumer가 Cell을 반환하면 dequeue position이 변경되고
            // notify_one()이 호출됩니다.
            const std::size_t observed =
                m_DequeuePos.load(std::memory_order_acquire);

            // wait 직전에 다시 시도하여 race를 제거합니다.
            if( EmplacePush(std::forward<U>(value)) )
                return true;

            if( IsClosed() )
                return false;

            m_DequeuePos.wait(
                observed,
                std::memory_order_acquire);
        }
    }

    //***************************************************************************
    // @brief 논블로킹 방식으로 데이터를 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 가득 찼거나 종료됨
    //***************************************************************************
    template <typename U>
    bool EmplacePush(U&& value)
    {
        static_assert(
            std::is_nothrow_constructible_v<T, U&&>,
            "T must be nothrow constructible from U");

        // Close와 reservation 사이의 race를 줄이기 위해
        // reservation 전에 종료 상태를 먼저 확인합니다.
        //
        // Close()와 동시에 이미 reservation을 획득한 producer는
        // 해당 item을 정상적으로 publish할 수 있습니다.
        if( m_Closed.load(std::memory_order_acquire) )
            return false;

        Cell* cell;

        std::size_t pos =
            m_EnqueuePos.load(std::memory_order_relaxed);

        for( ;;)
        {
            // CAS loop 도중 Close()가 호출되었는지 확인합니다.
            if( m_Closed.load(std::memory_order_acquire) )
                return false;

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
                pos =
                    m_EnqueuePos.load(std::memory_order_relaxed);
            }
        }

        // reservation 이후 T 생성자는 반드시 성공해야 합니다.
        // 그렇지 않으면 sequence hole이 발생하여 queue가 영구적으로
        // 진행하지 못할 수 있으므로 noexcept를 요구합니다.
        new (cell->GetDataPtr()) T(std::forward<U>(value));

        // Producer가 Cell의 데이터 생성을 완료했음을 consumer에게 publish합니다.
        cell->Sequence.store(
            pos + 1,
            std::memory_order_release);

        // 대기 중인 consumer를 깨웁니다.
        //
        // m_EnqueuePos 자체도 reservation 단계에서 이미 변경되었기
        // 때문에 wait 중인 consumer는 값 변화만으로도 깨어날 수 있지만,
        // 실제 데이터 publish 시점에 명시적으로 notify하여 불필요한
        // 지연을 줄입니다.
        m_EnqueuePos.notify_one();

        return true;
    }

    static constexpr std::size_t kIndexMask = Capacity - 1;

    // 큐의 실제 데이터를 저장하는 링 버퍼 배열입니다.
    // Cell 전체를 캐시 라인 정렬하여 producer/consumer 간 false sharing을 줄입니다.
    alignas(LFQ_CACHE_LINE_SIZE)
        Cell m_Buffer[Capacity];

    // 데이터가 삽입될 다음 위치입니다.
    // 멀티 프로듀서가 CAS로 경쟁하므로 별도의 캐시 라인에 배치합니다.
    //
    // C++20 atomic wait/notify의 consumer notification source로도 사용합니다.
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<std::size_t> m_EnqueuePos;

    // 데이터가 추출될 다음 위치입니다.
    // 단일 컨슈머 전용이므로 CAS가 필요하지 않습니다.
    //
    // C++20 atomic wait/notify의 producer notification source로도 사용합니다.
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<std::size_t> m_DequeuePos;

    // 큐 종료 여부입니다.
    //
    // false -> 정상 동작
    // true  -> 새로운 Push 금지, 기존 데이터 drain 허용
    alignas(LFQ_CACHE_LINE_SIZE)
        std::atomic<bool> m_Closed;
};

#endif // UC_MPSCLOCKFREEQUEUE_H
