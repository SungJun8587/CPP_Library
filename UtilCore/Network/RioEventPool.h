
//***************************************************************************
// RioEventPool.h : interface for the CRioEventPool class.
//
//***************************************************************************

#ifndef __RIOEVENTPOOL_H__
#define __RIOEVENTPOOL_H__

#ifndef CONTAINERS_H
#include <Memory/Containers.h>
#endif

#ifndef __RIOCOMMON_H__
#include <Network/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

#ifndef __RIOOBJECT_H__
#include <Network/RioObject.h>
#endif

//***************************************************************************
// @class CRioEventPool
// @brief 매번 heap에 동적 할당/해제하는 비용을 제거하기 위한 프리 리스트 기반 풀.
//
// @details
//      Alloc()은 I/O를 등록하는 로직 스레드에서,
//      Free()는 CRioCore::ProcessRioResult()가 실행되는 RIO 워커 스레드에서
//      호출될 수 있으므로 서로 다른 스레드가 동시에 Free List head를
//      조작합니다.
//
//      본 구현은 기존 프로젝트의 PLock을 이용하여 Free List 조작 구간만
//      보호합니다.
//
//      중요:
//
//          Alloc()
//              |
//              +-- lock
//              |      Pop
//              +-- unlock
//              |
//              +-- Initialize()
//
//          Free()
//              |
//              +-- Reset()
//              |
//              +-- lock
//              |      Push
//              +-- unlock
//
//      따라서 사용자 Dispatch 또는 기타 무거운 작업은 Pool lock을
//      보유한 상태에서 수행되지 않습니다.
//
// [Lock 경합 최소화 최적화]
//      CRioEvent::Initialize() 및 CRioEvent::Reset()과 같이 shared_ptr 레퍼런스
//      카운트 제어나 메모리 설정 작업은 Lock 경계 '외부'에서 수행됩니다.
//      따라서 임계 영역(Critical Section) 내부에서는 단순 포인터 교환(Push/Pop)만
//      수행되어 스레드 간 Lock 경합(Contention)을 최소화합니다.
//
// [메모리 단편화 방지]
//      CVector<MemoryBlock>을 통해 청크(Chunk) 단위 대량 할당을 관리하므로
//      C-Runtime Heap 단편화를 방지하고 캐시 지역성(Cache Locality)을 향상시킵니다.
//***************************************************************************
class CRioEventPool
{
public:
    //***************************************************************************
    // @brief CRioEventPool 생성자
    // @details 멤버 변수들을 기본값(nullptr, 0)으로 명시적 초기화합니다.
    //***************************************************************************
    CRioEventPool() noexcept
        : _head(nullptr)
        , _capacity(0)
        , _exhaustionCount(0)
        , _lastLogTick(0)
    {
    }

    //***************************************************************************
    // @brief CRioEventPool 소멸자
    // @details Release()를 자동 호출하여 할당된 모든 청크 메모리를 안전하게 정리합니다.
    //***************************************************************************
    ~CRioEventPool() noexcept
    {
        Release();
    }

    CRioEventPool(const CRioEventPool&) = delete;
    CRioEventPool& operator=(const CRioEventPool&) = delete;

public:
    bool Initialize(size_t capacity);
    CRioEvent* Alloc(CRioEvent::EventType type, CRioObjectRef ownerObj);
    void Free(CRioEvent* evt);
    void Release() noexcept;
    size_t GetCapacity() const;

private:
    void RecordExhaustion();

#ifdef _DEBUG
    void _DEBUG_SET_INITIAL_STATE(CRioEvent* evt);
#else
    void _DEBUG_SET_INITIAL_STATE(CRioEvent*) noexcept;
#endif

private:
    struct MemoryBlock
    {
        void* ptr{ nullptr }; // OS에서 할당받은 원본 메모리 버퍼 포인터
        size_t count{ 0 };    // 블록 내 CRioEvent 객체 개수
    };

private:
    mutable PLock _lock;                        // Free List 조작 및 멤버 변수 보호용 플랫폼 락
    CRioEvent* _head;                           // 현재 Free List의 Head 이벤트 포인터 (LIFO)
    size_t _capacity;                           // 풀의 전체 이벤트 용량
    CVector<MemoryBlock> _memoryBlocks;         // OS로부터 할당받은 메모리 블록 목록
    std::atomic<ULONGLONG> _exhaustionCount;    // 풀 고갈 발생 누적 횟수
    std::atomic<ULONGLONG> _lastLogTick;        // 마지막 고갈 로그 출력 타임스탬프 (ms)
};

#endif // __RIOEVENTPOOL_H__