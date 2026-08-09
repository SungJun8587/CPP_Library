
//***************************************************************************
// LockFreeSlotStack.h : interface for the CLockFreeSlotStack class.
//
//***************************************************************************

#ifndef __LOCKFREESLOTSTACK_H__
#define __LOCKFREESLOTSTACK_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#include <atomic>
#include <cassert>
#include <cstdint>

//***************************************************************************
// @class CLockFreeSlotStack
// @brief 멀티스레드 환경에서 락(Lock) 없이 안전하게 슬롯 인덱스를 관리하는 스택입니다.
//
// @details
// 내부적으로 64비트 아토믹 변수(`_head`)를 사용하여 상위 32비트에는 버전(ABA 문제 방지),
// 하위 32비트에는 스택의 최상단 인덱스를 패킹하는 트라이버 스택(Treiber Stack) 구조를 활용합니다.
// 고정된 용량(`capacity`)만큼의 인덱스 링크드 리스트를 미리 구축하여 런타임 중 동적 할당 없이
// 고성능으로 슬롯의 할당(Pop) 및 반환(Push)을 처리합니다.
//
// 주요 처리 및 특징:
//  - 락 프리(Lock-free) 기반 MPMC(Multi-Producer, Multi-Consumer) 슬롯 관리
//  - 버전 카운터를 통한 32-bit version counter 기반 ABA 재출현 방어
//  - 사전 할당된 벡터를 통한 메모리 단편화 및 할당 부하 제거
//  - RIO(Registered I/O) 버퍼 풀 등의 자원 관리 최적화
//***************************************************************************
class CLockFreeSlotStack
{
public:
    //***************************************************************************
    // @brief 지정한 용량(capacity)으로 락 프리 슬롯 스택을 초기화합니다.
    // @param capacity 관리할 총 슬롯 개수
    //***************************************************************************
    explicit CLockFreeSlotStack(uint32_t capacity)
        : _capacity(capacity)
        , _nextFree(capacity)
    {
        if( capacity == 0 )
        {
            _head.store(MakeNode(0, kNullIndex), std::memory_order_relaxed);
            return;
        }

        // 0 -> 1 -> 2 -> ... -> capacity-1 -> null
        for( uint32_t i = 0; i + 1 < capacity; ++i )
        {
            _nextFree[i].store(i + 1, std::memory_order_relaxed);
        }

        _nextFree[capacity - 1].store(kNullIndex, std::memory_order_relaxed);

        _head.store(MakeNode(0, 0), std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 스택에서 사용 가능한 슬롯 인덱스를 하나 꺼냅니다.
    // @param outIndex 꺼낸 슬롯 인덱스가 저장될 참조 변수
    // @return true: 인덱스가 정상적으로 꺼내짐, false: 스택이 비어 있어 할당 가능한 슬롯이 없음
    //***************************************************************************
    bool Pop(uint32_t& outIndex)
    {
        uint64_t oldHead = _head.load(std::memory_order_acquire);

        for( ;;)
        {
            const uint32_t index = GetIndex(oldHead);

            if( index == kNullIndex )
            {
                return false;
            }

            const uint32_t nextIndex = _nextFree[index].load(std::memory_order_relaxed);
            const uint32_t version = GetVersion(oldHead);
            const uint64_t newHead = MakeNode(version + 1, nextIndex);

            if( _head.compare_exchange_weak(oldHead, newHead, std::memory_order_acq_rel, std::memory_order_acquire) )
            {
                outIndex = index;
                return true;
            }
        }
    }

    //***************************************************************************
    // @brief 사용을 마친 슬롯 인덱스를 스택에 반환합니다.
    // @param index 반환할 슬롯 인덱스
    //***************************************************************************
    void Push(uint32_t index)
    {
        assert(index < _capacity);

        uint64_t oldHead = _head.load(std::memory_order_acquire);

        for( ;;)
        {
            const uint32_t oldIndex = GetIndex(oldHead);

            _nextFree[index].store(oldIndex, std::memory_order_relaxed);

            const uint32_t version = GetVersion(oldHead);
            const uint64_t newHead = MakeNode(version + 1, index);

            if( _head.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_acquire) )
            {
                return;
            }
        }
    }

private:
    //***************************************************************************
    // @brief 유효하지 않은 인덱스를 나타내는 상수 값 (0xFFFFFFFF)
    //***************************************************************************
    static constexpr uint32_t kNullIndex = 0xFFFFFFFFu;

    //***************************************************************************
    // @brief 버전과 인덱스를 결합하여 64비트 노드 값으로 생성합니다.
    // @param version ABA 방지를 위한 버전 카운터 값
    // @param index 슬롯 인덱스 값
    // @return 결합된 64비트 노드 값
    //***************************************************************************
    static uint64_t MakeNode(uint32_t version, uint32_t index) noexcept
    {
        return (static_cast<uint64_t>(version) << 32) |
            static_cast<uint64_t>(index);
    }

    //***************************************************************************
    // @brief 64비트 노드 값에서 버전 카운터를 추출합니다.
    // @param node 64비트 노드 값
    // @return 추출된 버전 값
    //***************************************************************************
    static uint32_t GetVersion(uint64_t node) noexcept
    {
        return static_cast<uint32_t>(node >> 32);
    }

    //***************************************************************************
    // @brief 64비트 노드 값에서 슬롯 인덱스를 추출합니다.
    // @param node 64비트 노드 값
    // @return 추출된 슬롯 인덱스 값
    //***************************************************************************
    static uint32_t GetIndex(uint64_t node) noexcept
    {
        return static_cast<uint32_t>(node);
    }

private:
    uint32_t                        _capacity{ 0 };     // 관리하는 전체 슬롯 용량
    CVector<std::atomic<uint32_t>>  _nextFree;          // 각 슬롯의 다음 빈 슬롯 인덱스 링크 배열
    alignas(8) std::atomic<uint64_t>_head{ 0 };         // 스택의 최상단 헤드 (버전 + 인덱스 패킹)
};

#endif // ndef __LOCKFREESLOTSTACK_H__