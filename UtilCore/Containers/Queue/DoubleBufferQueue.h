
//***************************************************************************
// DoubleBufferQueue.h : interface for the CDoubleBufferQueue class.
//
//***************************************************************************

#ifndef UC_DOUBLEBUFFERQUEUE_H
#define UC_DOUBLEBUFFERQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>
#include <Thread/PlatformLock.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>
#include <atomic>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

//***************************************************************************
// @brief CDoubleBufferQueue 내부 대기 보조 함수 모음. (SpinLock 라이브러리에 의존하지 않는다.)
//***************************************************************************
namespace DoubleBufferDetail
{
    //***************************************************************************
    // @brief 스핀 대기 중 CPU에 양보 힌트(pause/yield 명령)를 줍니다.
    //***************************************************************************
    inline void CpuRelax() noexcept
    {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        _mm_pause();
#elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
        __yield();
#elif defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield");
#endif
    }

    //***************************************************************************
    // @brief 조건이 참인 동안 대기합니다. (지수 백오프 스핀 -> 스레드 양보 -> 짧은 슬립)
    // @details 1) CPU pause를 1, 2, 4 ... 회(최대 Preset::MaxPauseBackoff 회)로 늘려 가며 재검사합니다.
    //          2) 백오프가 상한에 도달하면 스레드를 양보하며 재검사하고, 양보가 Preset::MaxYieldCount 회를
    //             넘으면 50us씩 슬립하며 재검사합니다. (기록 중인 Producer가 선점되어 오래 걸리는 경우에
    //             CPU를 태우거나 같은 코어의 Producer를 굶기지 않기 위함)
    // @tparam Preset MaxPauseBackoff / MaxYieldCount 정적 상수를 가진 대기 정책
    // @tparam Pred   bool() 형태의 호출 가능 객체. true인 동안 대기
    // @param stillWaiting 대기를 계속해야 하면 true를 반환하는 조건
    //***************************************************************************
    template <typename Preset, typename Pred>
    inline void SpinWait(Pred&& stillWaiting) noexcept(noexcept(stillWaiting()))
    {
        int backoff = 1;
        int yields = 0;

        while( stillWaiting() )
        {
            if( backoff < static_cast<int>(Preset::MaxPauseBackoff) )
            {
                for( int i = 0; i < backoff; ++i )
                    CpuRelax();
                backoff <<= 1;
            }
            else if( yields < static_cast<int>(Preset::MaxYieldCount) )
            {
                ++yields;
                std::this_thread::yield();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    //***************************************************************************
    // @brief 단일 컨슈머 계약 위반을 릴리즈 빌드에서도 즉시 중단시킵니다.
    //***************************************************************************
    [[noreturn]] inline void FatalMultiConsumer() noexcept
    {
        assert(false && "CDoubleBufferQueue::SwapInto - Concurrent multi-consumer access detected!");
        std::abort();
    }
}

//***************************************************************************
// @brief CDoubleBufferQueue 기본 대기 정책.
// @details Preset 타입은 MaxPauseBackoff(스핀 백오프 상한)와 MaxYieldCount(양보 횟수 상한) 정적 상수만
//          가지면 됩니다. 따라서 SpinLockPreset::LightWeight 등 기존 프리셋도 그대로 넘길 수 있습니다.
//***************************************************************************
namespace DoubleBufferWaitPreset
{
    struct Default
    {
        static constexpr int MaxPauseBackoff = 64;      // 한 번의 재검사 전에 반복하는 CPU pause 횟수의 상한
        static constexpr int MaxYieldCount = 16;        // 스레드 양보 횟수 상한 (이후에는 짧은 슬립)
    };
}

//***************************************************************************
// @class CDoubleBufferQueue
// @brief 락 분할과 더블 버퍼링을 통해 메모리 재할당 오버헤드를 크게 줄인 고성능 큐.
// 
// @details 
// 수많은 프로듀서 스레드가 동시에 무차별적으로 데이터를 쏟아붓고, 컨슈머가 
// 이를 최대한 빠르게 통째로 긁어와 배치(Batch) 처리하며 힙 메모리 재할당 비용을 
// 최소화해야 하는 핫패스(Hot Path)에 최적화되어 있습니다.
// 
// 주요 사용처 및 이점:
//  - 초고성능 로깅 시스템 (다중 스레드의 로그를 모아 일괄 플러시)
//  - 초당 수만 건 발생하는 통계, 지표 수집, DB 비동기 결과 수거
//  - 프로듀서 간 락 경합 최소화 (버퍼별 락 분할) 및 batch 재할당 최소화
// 
// 락과 대기 정책:
//  - 버퍼별 락은 플랫폼 통합 락(PLock)이며 PLockGuard로 보호합니다.
//    Windows에서는 SRWLock(독점), 그 외 플랫폼에서는 SpinLock으로 동작합니다.
//  - Preset은 SwapInto()가 이전 버퍼의 Producer 퇴장을 기다릴 때의 대기 정책
//    (스핀 백오프/양보 횟수)만 결정합니다. 버퍼 락의 종류와는 무관하며, SpinLock 라이브러리에
//    의존하지 않습니다. (MaxPauseBackoff / MaxYieldCount 상수만 있으면 어떤 프리셋이든 사용 가능)
// 
// 패턴 최적화:
//  - **MPSC(Multiple Producer, Single Consumer)** 환경에 최적화
//    → 여러 프로듀서가 동시에 데이터를 밀어 넣고, 단일 컨슈머가 전체 버퍼를 스왑하여
//      초고속으로 배치 처리
// 
// 동시성 설계 (Producer admission ↔ Consumer buffer flip):
//  - Producer는 현재 활성 버퍼 인덱스를 읽고 inFlight 카운터를 증가시킨 뒤,
//    버퍼 인덱스를 재검증하는 "optimistic admission" 프로토콜을 사용합니다.
//  - Consumer는 버퍼 인덱스를 뒤집은 뒤 이전 버퍼의 inFlight가 0이 될 때까지
//    대기합니다.
//  - 이 admission(inFlight 등록 + 재검증)과 flip(인덱스 전환 + inFlight 관찰)은
//    서로 다른 atomic 변수에 걸친 Dekker류 상호 배제 관계이므로, 관련된 네
//    연산(Producer의 inFlight.fetch_add·writeIdx 재검증 load, Consumer의
//    writeIdx.fetch_xor·inFlight.load)은 모두 memory_order_seq_cst로 통일되어
//    있습니다. 이 네 연산이 하나의 순차 일관(SC) 전체 순서를 공유해야만
//    "Producer는 예전 버퍼로 착각하고 Consumer는 inFlight==0으로 착각하는"
//    상호 관찰 누락이 원천적으로 배제됩니다. acquire/release만으로는 이
//    보장이 성립하지 않으므로, 이 네 곳의 seq_cst는 성능 여유가 아니라
//    correctness 요구사항입니다. (참고: Producer가 최초로 writeIdx를 읽는
//    지점과 InFlightGuard 해제 시의 fetch_sub는 이 SC 관계에 참여하지 않으므로
//    각각 acquire / release로 충분합니다.)
//***************************************************************************
template <typename T, typename Preset = DoubleBufferWaitPreset::Default>
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
    //***************************************************************************
    void Push(const T& item)
    {
        if( m_stopped.load(std::memory_order_relaxed) )
            return;
        PushInternal(item);
    }

    //***************************************************************************
    // @brief 큐에 새로운 데이터를 추가합니다 (R-value 이동).
    // @param item 추가할 데이터 객체 (이동 의미론, move semantics)
    //***************************************************************************
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
    //***************************************************************************
    template <typename... Args>
    void Emplace(Args&&... args)
    {
        if( m_stopped.load(std::memory_order_relaxed) )
            return;
        EmplaceInternal(std::forward<Args>(args)...);
    }

    //***************************************************************************
    // @brief 현재 활성 버퍼를 스왑하고, 이전 버퍼의 모든 데이터를 컨테이너로 반환합니다.
    // @return 이전 버퍼에 쌓여있던 데이터들의 벡터
    //***************************************************************************
    CVector<T> Swap()
    {
        CVector<T> result;
        SwapInto(result);
        return result;
    }

    //***************************************************************************
    // @brief 활성 버퍼를 스왑하고, 이전 버퍼의 내용을 외부 벡터에 효율적으로 옮겨 담습니다.
    // @param out 이전 버퍼의 데이터가 채워질 대상 벡터 
    // @note out.clear() 자체는 스왑과 무관하게 out에 남아있던 이전 batch의 원소들을
    //       파괴하는 비용(O(N), T의 소멸자 비용에 비례)이 들 수 있습니다. 버퍼
    //       스왑 자체는 O(1)이지만 SwapInto() 전체 비용을 "항상 O(1)"로 보면 안 됩니다.
    //***************************************************************************
    void SwapInto(CVector<T>& out)
    {
        // 다중 컨슈머 진입 차단 (릴리즈 빌드에서도 Fatal로 안전하게 감지)
        bool expectedSwap = false;
        if( !m_isSwapping.compare_exchange_strong(expectedSwap, true, std::memory_order_acq_rel) )
        {
            DoubleBufferDetail::FatalMultiConsumer();
        }

        // 스왑 작업 전반의 예외 안전성 보장 (스코프 이탈 시 소멸자에서 자동으로 m_isSwapping 해제)
        SwapGuard swapGuard(m_isSwapping);

        // 활성 인덱스를 원자적으로 뒤집고 이전 인덱스를 얻는다.
        // Producer의 admission 재검증 load와 SC 전체 순서를 공유해야 하므로 seq_cst.
        const int oldIdx = m_writeIdx.fetch_xor(1, std::memory_order_seq_cst);

        // 이전 버퍼에 아직 기록 중인 Producer가 모두 빠질 때까지 대기.
        // 이 관찰 역시 위 admission SC 프로토콜의 일부이므로 seq_cst.
        DoubleBufferDetail::SpinWait<Preset>(
            [this, oldIdx]() noexcept
            {
                return m_inFlight[oldIdx].load(std::memory_order_seq_cst) != 0;
            }
        );

        // 핫패스 성능 유지를 위한 순수 clear 및 swap
        out.clear();
        out.swap(m_buffer[oldIdx]);
    }

    //***************************************************************************
    // @brief 양쪽 버퍼에 쌓인 데이터 총 개수의 근사치(Approximate Size)를 반환합니다.
    // @return 두 버퍼에 있는 데이터 수의 합
    // @warning Consumer 스레드 단독 환경 또는 데이터 경합이 없는 디버깅용으로만 호출해야 합니다.
    //          Producer가 동시에 emplace_back() 중이거나 Consumer가 SwapInto() 중인
    //          멀티스레드 상황에서 호출 시, std::vector 내부 접근과의 data race로 인해
    //          단순히 "근사값이 부정확한" 수준이 아니라 C++ 메모리 모델 상 UB입니다.
    //***************************************************************************
    size_t ApproxSize() const
    {
        return m_buffer[0].size() + m_buffer[1].size();
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 새로운 데이터의 유입을 차단합니다.
    // @note "Stop 호출을 관찰한 이후의 Producer"부터 Push/Emplace를 거부하는
    //       admission 플래그입니다. Stop() 호출 시점에 이미 admission을 통과해
    //       진행 중인 Producer가 있다면 해당 호출은 계속 완료됩니다. 즉 이
    //       함수는 "새 작업 접수 중단"이며, 진행 중인 작업의 drain을 기다리는
    //       shutdown/flush 동작이 아닙니다. 완전한 drain이 필요하다면 Stop()
    //       이후 SwapInto()를 호출해 남은 데이터를 마저 수거해야 합니다.
    //***************************************************************************
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
    //
    // fetch_add는 Consumer의 writeIdx.fetch_xor / inFlight.load와 하나의 SC
    // 전체 순서를 공유해야 하는 admission 프로토콜의 일부이므로 seq_cst를
    // 사용합니다. 반면 소멸자의 fetch_sub는 이 프로토콜에 참여하지 않고
    // 단순히 "이 Producer가 버퍼 접근을 마쳤음"을 release로 공표하는
    // 역할이며, Consumer 측 SC load가 acquire보다 강하므로 release로
    // 충분합니다.
    //***************************************************************************
    class InFlightGuard
    {
    public:
        explicit InFlightGuard(std::atomic<int>& inFlight) noexcept
            : m_inFlight(inFlight)
        {
            m_inFlight.fetch_add(1, std::memory_order_seq_cst);
        }

        ~InFlightGuard() noexcept
        {
            m_inFlight.fetch_sub(1, std::memory_order_release);
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
    // (Producer admission 프로토콜과는 무관하게, 단일 Consumer라는 클래스
    // 계약을 어긴 잘못된 API 사용을 탐지하는 용도입니다.)
    //***************************************************************************
    class SwapGuard
    {
    public:
        explicit SwapGuard(std::atomic<bool>& isSwapping) noexcept
            : m_isSwapping(isSwapping)
        {
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
    // - PLockGuard로 버퍼 락 보호
    //***************************************************************************
    template <typename U>
    void PushInternal(U&& item)
    {
        EmplaceInternal(std::forward<U>(item));
    }

    //***************************************************************************
    // @brief 내부 Emplace 구현 함수. Producer가 인자들을 통해 데이터를 버퍼에 직접 제자리 생성합니다.
    // @tparam Args 생성자에 전달할 인자 타입들
    // @param args 생성자에 전달할 인자들
    //
    // @details
    // idx를 읽는 최초 load는 단순히 현재 버퍼 후보를 고르는 optimistic read이므로
    // acquire로 충분합니다. 실제 correctness는 InFlightGuard로 inFlight를 등록한
    // 뒤 writeIdx를 재검증하는 두 번째 load에서 결정되며, 이 재검증 load는
    // Consumer의 writeIdx.fetch_xor / inFlight.load와 SC 전체 순서를 공유해야
    // 하므로 seq_cst를 사용합니다. (클래스 상단 "동시성 설계" 설명 참고.)
    //***************************************************************************
    template <typename... Args>
    void EmplaceInternal(Args&&... args)
    {
        int idx;

        for( ;;)
        {
            if( m_stopped.load(std::memory_order_relaxed) )
                return;

            idx = m_writeIdx.load(std::memory_order_acquire);

            InFlightGuard guard(m_inFlight[idx]);

            if( m_writeIdx.load(std::memory_order_seq_cst) != idx )
                continue;

            {
                PLockGuard lockGuard(m_bufferLock[idx], __FUNCTION__);

                m_buffer[idx].emplace_back(std::forward<Args>(args)...);
            }

            break;
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
    std::atomic<bool> m_stopped{ false };       // 종료(신규 접수 차단) 플래그
};

#endif // ndef UC_DOUBLEBUFFERQUEUE_H