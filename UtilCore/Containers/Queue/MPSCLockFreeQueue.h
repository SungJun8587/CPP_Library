
//***************************************************************************
// MPSCLockFreeQueue.h : interface for the MPSCLockFreeQueue class.
//
//***************************************************************************

#ifndef __MPSCLOCKFREEQUEUE_H__
#define __MPSCLOCKFREEQUEUE_H__

#ifndef	__QUEUECOMMON_H__
#include <Containers/Queue/QueueCommon.h>
#endif

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
        T* GetDataPtr() noexcept { return reinterpret_cast<T*>(Storage); }
    };

public:
    //***************************************************************************
    // @brief MPSCLockFreeQueue 객체를 생성합니다.
    MPSCLockFreeQueue()
    {
        for( std::size_t i = 0; i < Capacity; ++i )
            m_Buffer[i].Sequence.store(i, std::memory_order_relaxed);
        m_EnqueuePos.store(0, std::memory_order_relaxed);
        m_DequeuePos.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 소멸자. 남은 데이터를 모두 비웁니다.
    ~MPSCLockFreeQueue()
    {
        T temp;
        for( ;;)
        {
            bool popped;
            try { popped = TryPop(temp); }
            catch( ... ) { continue; }
            if( !popped ) break;
        }
    }

    MPSCLockFreeQueue(const MPSCLockFreeQueue&) = delete;
    MPSCLockFreeQueue& operator=(const MPSCLockFreeQueue&) = delete;

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참
    bool TryPush(const T& value) { return EmplacePush(value); }

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참
    bool TryPush(T&& value) { return EmplacePush(std::move(value)); }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    void Push(const T& value) { BlockingEmplacePush(value); }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    void Push(T&& value) { BlockingEmplacePush(std::move(value)); }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음
    bool TryPop(T& outValue)
    {
        // 단일 consumer 전용: CAS 없이 relaxed 원자적 load/store만 사용.
        std::size_t pos = m_DequeuePos.load(std::memory_order_relaxed);
        Cell* cell = &m_Buffer[pos & kIndexMask];
        const std::size_t seq = cell->Sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if( diff != 0 ) return false;

        m_DequeuePos.store(pos + 1, std::memory_order_relaxed);
        outValue = std::move(*cell->GetDataPtr());
        cell->GetDataPtr()->~T();
        cell->Sequence.store(pos + Capacity, std::memory_order_release);
        NotifyNotFull();
        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    void Pop(T& outValue)
    {
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( TryPop(outValue) ) return;
            LFQ_CPU_PAUSE();
        }
        for( ;;)
        {
            std::unique_lock<std::mutex> lock(m_NotEmptyMutex);
            m_WaitingPoppers.fetch_add(1, std::memory_order_relaxed);
            m_NotEmptyCv.wait_for(lock, std::chrono::milliseconds(1));
            m_WaitingPoppers.fetch_sub(1, std::memory_order_relaxed);
            lock.unlock();
            if( TryPop(outValue) ) return;
        }
    }

    //***************************************************************************
    // @brief 대략적인 현재 큐 크기를 반환합니다.
    // @return std::size_t 대략적인 요소 개수
    std::size_t SizeApprox() const noexcept
    {
        const std::size_t enq = m_EnqueuePos.load(std::memory_order_relaxed);
        const std::size_t deq = m_DequeuePos.load(std::memory_order_relaxed);
        return (enq >= deq) ? (enq - deq) : 0;
    }

    //***************************************************************************
    // @brief 큐의 최대 용량을 반환합니다.
    // @return constexpr std::size_t 큐 용량
    constexpr std::size_t GetCapacity() const noexcept { return Capacity; }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 대기 중인 소비자(popper)에게 데이터가 있음을 알립니다.
    void NotifyNotEmpty() { if( m_WaitingPoppers.load(std::memory_order_relaxed) > 0 ) m_NotEmptyCv.notify_one(); }

    //***************************************************************************
    // @brief 대기 중인 생산자(pusher)에게 빈 공간이 생겼음을 알립니다.
    void NotifyNotFull() { if( m_WaitingPushers.load(std::memory_order_relaxed) > 0 ) m_NotFullCv.notify_one(); }

    //***************************************************************************
    // @brief 블로킹 방식으로 데이터를 큐에 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    template <typename U>
    void BlockingEmplacePush(U&& value)
    {
        for( int i = 0; i < kSpinBeforeSleepCount; ++i )
        {
            if( EmplacePush(std::forward<U>(value)) ) return;
            LFQ_CPU_PAUSE();
        }
        for( ;;)
        {
            std::unique_lock<std::mutex> lock(m_NotFullMutex);
            m_WaitingPushers.fetch_add(1, std::memory_order_relaxed);
            m_NotFullCv.wait_for(lock, std::chrono::milliseconds(1));
            m_WaitingPushers.fetch_sub(1, std::memory_order_relaxed);
            lock.unlock();
            if( EmplacePush(std::forward<U>(value)) ) return;
        }
    }

    //***************************************************************************
    // @brief 논블로킹 방식으로 데이터를 큐에 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    // @return true: 삽입 성공, false: 큐가 가득 참
    template <typename U>
    bool EmplacePush(U&& value)
    {
        Cell* cell;
        std::size_t pos = m_EnqueuePos.load(std::memory_order_relaxed);
        for( ;;)
        {
            cell = &m_Buffer[pos & kIndexMask];
            const std::size_t seq = cell->Sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if( diff == 0 )
            {
                if( m_EnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed) )
                    break;
            }
            else if( diff < 0 ) { return false; }
            else { pos = m_EnqueuePos.load(std::memory_order_relaxed); }
        }

        new (cell->GetDataPtr()) T(std::forward<U>(value));
        cell->Sequence.store(pos + 1, std::memory_order_release);
        NotifyNotEmpty();
        return true;
    }

    static constexpr std::size_t kIndexMask = Capacity - 1;
    alignas(LFQ_CACHE_LINE_SIZE) Cell m_Buffer[Capacity];
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_EnqueuePos;
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_DequeuePos;
    std::mutex m_NotEmptyMutex, m_NotFullMutex;
    std::condition_variable m_NotEmptyCv, m_NotFullCv;
    std::atomic<int> m_WaitingPoppers{ 0 }, m_WaitingPushers{ 0 };
};

#endif // __MPSCLOCKFREEQUEUE_H__