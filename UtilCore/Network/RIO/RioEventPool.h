//***************************************************************************
// RioEventPool.h : interface for the CRioEventPool class.
//
//***************************************************************************

#ifndef __RIOEVENTPOOL_H__
#define __RIOEVENTPOOL_H__

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __RIOCOMMON_H__
#include <Network/RIO/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/RIO/RioEvent.h>
#endif

#ifndef __RIOOBJECT_H__
#include <Network/RIO/RioObject.h>
#endif

//***************************************************************************
// @class CRioEventPool
// @brief 매번 heap에 동적 할당/해제하는 비용을 제거하기 위한 프리 리스트 기반 풀.
//
// @details
//      Alloc()은 I/O를 등록하는 로직 스레드에서,
//      Free()는 CRioCore::ProcessRioResult()가 실행되는 RIO 워커 스레드에서
//      호출될 수 있으므로 서로 다른 스레드가 동시에 Free List head를 조작합니다.
//
//      본 구현은 기존 프로젝트의 PLock을 이용하여 Free List 조작 구간만 보호합니다.
//
// [IMPORTANT LIFETIME RULE]
//      Alloc()/Free()가 실행 중인 동안 Release()가 동시에 호출되어서는 안 됩니다.
//***************************************************************************
class CRioEventPool
{
public:
    //***************************************************************************
    // @brief CRioEventPool 생성자
    //***************************************************************************
    CRioEventPool() noexcept
        : _head(nullptr)
        , _capacity(0)
        , _exhaustionCount(0)
        , _lastLogTick(0)
        , _inUseCount(0)
    {
    }

    //***************************************************************************
    // @brief CRioEventPool 소멸자
    //***************************************************************************
    ~CRioEventPool() noexcept
    {
        Release();
    }

    CRioEventPool(const CRioEventPool&) = delete;
    CRioEventPool& operator=(const CRioEventPool&) = delete;

public:
    bool Initialize(size_t capacity);

    // Alloc은 순수 메모리 슬롯만 제공하고 초기화 책임은 호출부로 분리
    CRioEvent* Alloc() noexcept;

    void Free(CRioEvent* evt) noexcept;
    void Release() noexcept;
    size_t GetCapacity() const;

private:
    void RecordExhaustion();

    //***************************************************************************
    // @brief 현재 사용 중인(할당된) 이벤트 개수를 반환합니다.
    // @return size_t 현재 외부에서 사용 중인 CRioEvent의 수량
    //***************************************************************************
    size_t GetInUseCount() const noexcept
    {
        return _inUseCount.load(std::memory_order_acquire);
    }

#ifdef _DEBUG
    void _DEBUG_SET_INITIAL_STATE(CRioEvent* evt);
#else
    void _DEBUG_SET_INITIAL_STATE(CRioEvent*) noexcept;
#endif

private:
    //***************************************************************************
    // @struct MemoryBlock
    // @brief OS로부터 청크 단위로 할당받은 메모리 블록 관리 구조체
    // @details 할당받은 raw 메모리 포인터와 해당 블록에 포함된 CRioEvent 객체 수량을 보관합니다.
    //***************************************************************************
    struct MemoryBlock
    {
        void* ptr{ nullptr }; // OS로부터 할당받은 메모리 블록 포인터
        size_t count{ 0 };   // 해당 블록 내 포함된 이벤트 개수
    };

private:
    mutable PLock _lock;                        // Free List 조작 및 멤버 변수 보호용 플랫폼 락
    CRioEvent* _head;                           // 현재 Free List의 Head 이벤트 포인터 (LIFO)
    size_t _capacity;                           // 풀의 전체 이벤트 용량
    CVector<MemoryBlock> _memoryBlocks;         // OS로부터 할당받은 메모리 블록 목록
    std::atomic<ULONGLONG> _exhaustionCount;     // 풀 고갈 발생 누적 횟수
    std::atomic<ULONGLONG> _lastLogTick;         // 마지막 고갈 로그 출력 타임스탬프 (ms)
    std::atomic<size_t> _inUseCount;             // 현재 외부에서 할당되어 사용 중인 이벤트 개수
};

#endif // ndef __RIOEVENTPOOL_H__