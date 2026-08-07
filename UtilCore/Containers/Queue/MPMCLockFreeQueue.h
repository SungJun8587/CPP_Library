
//***************************************************************************
// MPMCLockFreeQueue.h : interface for the MPMCLockFreeQueue class.
//
//***************************************************************************

#ifndef __MPMCLOCKFREEQUEUE_H__
#define __MPMCLOCKFREEQUEUE_H__

#ifndef	__QUEUECOMMON_H__
#include <Containers/Queue/QueueCommon.h>
#endif

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
        T* GetDataPtr() noexcept { return reinterpret_cast<T*>(Storage); }
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
        T temp;
        for( ;;)
        {
            bool popped;
            try { popped = TryPop(temp); }
            catch( ... ) { continue; }
            if( !popped ) break;
        }
    }

    MPMCLockFreeQueue(const MPMCLockFreeQueue&) = delete;
    MPMCLockFreeQueue& operator=(const MPMCLockFreeQueue&) = delete;

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참
    //***************************************************************************
    bool TryPush(const T& value) { return EmplacePush(value); }

    //***************************************************************************
    // @brief 큐에 데이터를 논블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    // @return true: 삽입 성공, false: 큐가 가득 참
    //***************************************************************************
    bool TryPush(T&& value) { return EmplacePush(std::move(value)); }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Lvalue)
    // @param value 삽입할 값
    //***************************************************************************
    void Push(const T& value) { BlockingEmplacePush(value); }

    //***************************************************************************
    // @brief 큐에 데이터를 블로킹 방식으로 삽입합니다. (Rvalue)
    // @param value 삽입할 값
    //***************************************************************************
    void Push(T&& value) { BlockingEmplacePush(std::move(value)); }

    //***************************************************************************
    // @brief 큐에서 데이터를 논블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    // @return true: 데이터 추출 성공, false: 큐가 비어있음
    //***************************************************************************
    bool TryPop(T& outValue)
    {
        Cell* cell;
        std::size_t pos = m_DequeuePos.load(std::memory_order_relaxed);
        for( ;;)
        {
            cell = &m_Buffer[pos & kIndexMask];
            const std::size_t seq = cell->Sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if( diff == 0 )
            {
                if( m_DequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed) )
                    break;
            }
            else if( diff < 0 ) { return false; }
            else { pos = m_DequeuePos.load(std::memory_order_relaxed); }
        }

        outValue = std::move(*cell->GetDataPtr());
        cell->GetDataPtr()->~T();
        cell->Sequence.store(pos + Capacity, std::memory_order_release);
        NotifyNotFull();
        return true;
    }

    //***************************************************************************
    // @brief 큐에서 데이터를 블로킹 방식으로 꺼냅니다.
    // @param outValue 꺼낸 데이터가 저장될 참조 변수
    //***************************************************************************
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
    //***************************************************************************
    std::size_t SizeApprox() const noexcept
    {
        const std::size_t enq = m_EnqueuePos.load(std::memory_order_relaxed);
        const std::size_t deq = m_DequeuePos.load(std::memory_order_relaxed);
        return (enq >= deq) ? (enq - deq) : 0;
    }

    //***************************************************************************
    // @brief 큐의 최대 용량을 반환합니다.
    // @return constexpr std::size_t 큐 용량
    //***************************************************************************
    constexpr std::size_t GetCapacity() const noexcept { return Capacity; }

private:
    static constexpr int kSpinBeforeSleepCount = 1000;

    //***************************************************************************
    // @brief 대기 중인 소비자(popper)에게 데이터가 있음을 알립니다.
    //***************************************************************************
    void NotifyNotEmpty() { if( m_WaitingPoppers.load(std::memory_order_relaxed) > 0 ) m_NotEmptyCv.notify_one(); }

    //***************************************************************************
    // @brief 대기 중인 생산자(pusher)에게 빈 공간이 생겼음을 알립니다.
    //***************************************************************************
    void NotifyNotFull() { if( m_WaitingPushers.load(std::memory_order_relaxed) > 0 ) m_NotFullCv.notify_one(); }

    //***************************************************************************
    // @brief 블로킹 방식으로 데이터를 큐에 임플레이스 삽입합니다.
    // @param value 삽입할 값 (포워딩 참조)
    //***************************************************************************
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
    //***************************************************************************
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

    static constexpr std::size_t kIndexMask = Capacity - 1;                // 큐 인덱스 계산용 마스크 (Capacity가 2의 거듭제곱일 때 모듈로 연산 대체)
    alignas(LFQ_CACHE_LINE_SIZE) Cell m_Buffer[Capacity];                  // 큐의 실제 데이터를 저장하는 링 버퍼 배열 (캐시 라인 정렬)
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_EnqueuePos;    // 데이터가 삽입될 다음 위치 (멀티 프로듀서 경쟁, 캐시 라인 정렬)
    alignas(LFQ_CACHE_LINE_SIZE) std::atomic<std::size_t> m_DequeuePos;    // 데이터가 추출될 다음 위치 (멀티 컨슈머 경쟁, 캐시 라인 정렬)
    std::mutex m_NotEmptyMutex, m_NotFullMutex;                            // 큐가 비어있거나 가득 찼을 때 블로킹을 위한 뮤텍스
    std::condition_variable m_NotEmptyCv, m_NotFullCv;                     // 큐 상태 변화(비어있지 않음 / 가득 차지 않음)를 통보하는 조건 변수
    std::atomic<int> m_WaitingPoppers{ 0 }, m_WaitingPushers{ 0 };         // 현재 대기 중인 소비자 및 생산자의 스레드 수
};

#endif // __MPMCLOCKFREEQUEUE_H__