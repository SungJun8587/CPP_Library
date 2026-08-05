
//***************************************************************************
// DoubleBufferQueue.h : interface for the CDoubleBufferQueue class.
//
//***************************************************************************

#ifndef __DOUBLEBUFFERQUEUE_H__
#define __DOUBLEBUFFERQUEUE_H__

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
// @class CDoubleBufferQueue
// @brief 락 분할과 더블 버퍼링을 통해 메모리 할당 오버헤드를 없앤 고성능 큐.
// 
// @details 
// 수많은 프로듀서 스레드가 동시에 무차별적으로 데이터를 쏟아붓고, 컨슈머가 
// 이를 최대한 빠르게 통째로 긁어와 배치(Batch) 처리하며 힙 메모리 재할당 비용을 
// 극한까지 없애야 하는 핫패스(Hot Path)에 최적화되어 있습니다.
// 
// 주요 사용처 및 이점:
//  - 초고성능 로깅 시스템 (다중 스레드의 로그를 모아 일괄 플러시)
//  - 초당 수만 건 발생하는 통계, 지표 수집, DB 비동기 결과 수거
//  - 프로듀서 간 락 경합 최소화 및 제로나이트(Zero-Allocation) 메모리 유지
// 
// 패턴 최적화:
//  - **MPSC(Multiple Producer, Single Consumer)** 환경에 최적화
//    → 여러 프로듀서가 동시에 데이터를 밀어 넣고, 단일 컨슈머가 전체 버퍼를 스왑하여
//      초고속으로 배치 처리
//***************************************************************************
template <typename T, typename Preset = SpinLockPreset::LightWeight>
class CDoubleBufferQueue
{
public:
    CDoubleBufferQueue() = default;
    ~CDoubleBufferQueue() = default;

    CDoubleBufferQueue(const CDoubleBufferQueue&) = delete;
    CDoubleBufferQueue& operator=(const CDoubleBufferQueue&) = delete;
    CDoubleBufferQueue(CDoubleBufferQueue&&) = delete;
    CDoubleBufferQueue& operator=(CDoubleBufferQueue&&) = delete;

    //***************************************************************************
    // @brief 큐에 새로운 데이터를 추가합니다 (L-value 복사).
    // @param item 추가할 데이터 객체
    void Push(const T& item)
    {
        if( m_stopped.load(std::memory_order_relaxed) )
            return;
        PushInternal(item);
    }

    //***************************************************************************
    // @brief 큐에 새로운 데이터를 추가합니다 (R-value 이동).
    // @param item 추가할 데이터 객체 (이동语义)
    void Push(T&& item)
    {
        if( m_stopped.load(std::memory_order_relaxed) )
            return;
        PushInternal(std::move(item));
    }

    //***************************************************************************
    // @brief 전달받은 인자들로 큐 내부에서 데이터를 직접 생성(Emplace)합니다.
    // @tparam Args 생성자에 전달할 인자 타입들
    // @param args 생성자에 전달할 인자들
    template <typename... Args>
    void Emplace(Args&&... args)
    {
        if( m_stopped.load(std::memory_order_relaxed) )
            return;
        PushInternal(T(std::forward<Args>(args)...));
    }

    //***************************************************************************
    // @brief 현재 활성 버퍼를 스왑하고, 이전 버퍼의 모든 데이터를 컨테이너로 반환합니다.
    // @return 이전 버퍼에 쌓여있던 데이터들의 벡터
    CVector<T> Swap()
    {
        CVector<T> result;
        SwapInto(result);
        return result;
    }

    //***************************************************************************
    // @brief 활성 버퍼를 스왑하고, 이전 버퍼의 내용을 외부 벡터에 효율적으로 옮겨 담습니다.
    // @param out 이전 버퍼의 데이터가 채워질 대상 벡터 
    void SwapInto(CVector<T>& out)
    {
        // 다중 컨슈머 진입 차단 (릴리즈 빌드에서도 Fatal로 안전하게 감지)
        bool expectedSwap = false;
        if( !m_isSwapping.compare_exchange_strong(expectedSwap, true, std::memory_order_acq_rel) )
        {
            SPINLOCK_FATAL("CDoubleBufferQueue::SwapInto - Concurrent multi-consumer access detected!");
        }

        // 스왑 작업 전반의 예외 안전성 보장 (스코프 이탈 시 소멸자에서 자동으로 m_isSwapping 해제)
        SwapGuard swapGuard(m_isSwapping);

        // 활성 인덱스를 원자적으로 뒤집고 이전 인덱스를 얻는다.
        const int oldIdx = m_writeIdx.fetch_xor(1, std::memory_order_acq_rel);

        // 이전 버퍼에 아직 기록 중인 Producer가 모두 빠질 때까지 대기.
        SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
            [this, oldIdx]() noexcept
            {
                return m_inFlight[oldIdx].load(std::memory_order_acquire) != 0;
            }
        );

        // 핫패스 성능 유지를 위한 순수 clear 및 swap
        out.clear();
        out.swap(m_buffer[oldIdx]);
    }

    //***************************************************************************
    // @brief 양쪽 버퍼에 쌓인 데이터 총 개수의 근사치(Approximate Size)를 반환합니다.
    // @return 두 버퍼에 있는 데이터 수의 합
    size_t ApproxSize() const
    {
        return m_buffer[0].size() + m_buffer[1].size();
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 새로운 데이터의 유입을 차단합니다.
    void Stop()
    {
        m_stopped.store(true, std::memory_order_relaxed);
    }

private:
    //***************************************************************************
    // @class InFlightGuard
    // @brief Producer용 예외 안전성 가드.
    //
    // @details
    // Producer가 데이터를 기록하는 동안 inFlight 카운터를 증가시켜
    // Consumer가 Swap 시점에 안전하게 대기할 수 있도록 보장합니다.
    // RAII 패턴을 사용하여 예외 발생 시에도 카운터가 자동 복구됩니다.
    //***************************************************************************
    class InFlightGuard
    {
    public:
        explicit InFlightGuard(std::atomic<int>& inFlight) noexcept
            : m_inFlight(inFlight)
        {
            m_inFlight.fetch_add(1, std::memory_order_acq_rel);
        }

        ~InFlightGuard() noexcept
        {
            m_inFlight.fetch_sub(1, std::memory_order_acq_rel);
        }

        InFlightGuard(const InFlightGuard&) = delete;
        InFlightGuard& operator=(const InFlightGuard&) = delete;

    private:
        std::atomic<int>& m_inFlight; // 현재 버퍼에 기록 중인 Producer 수
    };

    //***************************************************************************
    // @class SwapGuard
    // @brief Consumer 스왑 플래그용 예외 안전성 가드.
    //
    // @details
    // Consumer가 SwapInto에 진입했을 때 m_isSwapping 플래그를 설정하여
    // 다중 컨슈머 진입을 차단합니다. 스코프 종료 시 자동으로 플래그 해제.
    //***************************************************************************
    class SwapGuard
    {
    public:
        explicit SwapGuard(std::atomic<bool>& isSwapping) noexcept
            : m_isSwapping(isSwapping) {
        }

        ~SwapGuard() noexcept
        {
            m_isSwapping.store(false, std::memory_order_release);
        }

        SwapGuard(const SwapGuard&) = delete;
        SwapGuard& operator=(const SwapGuard&) = delete;

    private:
        std::atomic<bool>& m_isSwapping; // Consumer 진입 여부 플래그
    };

    //***************************************************************************
    // @brief 내부 Push 구현 함수. Producer가 데이터를 버퍼에 삽입합니다.
    // @tparam U 삽입할 데이터 타입 (L-value 또는 R-value)
    // @param item 삽입할 데이터 객체
    //
    // @details
    // - 현재 writeIdx를 읽어 해당 버퍼에 삽입
    // - InFlightGuard로 Producer 카운터 관리
    // - SpinLockGuard로 버퍼 락 보호
    //***************************************************************************
    template <typename U>
    void PushInternal(U&& item)
    {
        int idx;
        for( ;;)
        {
            if( m_stopped.load(std::memory_order_relaxed) )
                return;

            idx = m_writeIdx.load(std::memory_order_acquire);

            // RAII 가드를 통해 예외 발생 시에도 카운터가 안전하게 복구됨
            InFlightGuard guard(m_inFlight[idx]);

            if( m_writeIdx.load(std::memory_order_acquire) == idx )
            {
                {
                    SpinLockGuard<Preset> lockGuard(m_bufferLock[idx], "CDoubleBufferQueue::Push");
                    m_buffer[idx].push_back(std::forward<U>(item));
                }
                break;
            }
        }
    }

    //***************************************************************************
    // @brief 내부 멤버 변수들
    //***************************************************************************
    PLock             m_bufferLock[2];          // 두 버퍼 각각의 플랫폼 통합 락
    CVector<T>        m_buffer[2];              // 더블 버퍼 (데이터 저장소)

    std::atomic<int>  m_writeIdx{ 0 };          // 현재 활성 버퍼 인덱스 (0 또는 1)
    std::atomic<bool> m_isSwapping{ false };    // Consumer 스왑 중 여부 플래그
    std::atomic<int>  m_inFlight[2]{};          // 각 버퍼에 기록 중인 Producer 수
    std::atomic<bool> m_stopped{ false };       // 종료 플래그 추가
};

#endif // ndef __DOUBLEBUFFERQUEUE_H__