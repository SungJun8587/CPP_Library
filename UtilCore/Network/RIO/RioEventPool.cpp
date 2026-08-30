
//***************************************************************************
// RioEventPool.cpp : implementation of the CRioEventPool class.
//
//***************************************************************************

#include "pch.h"
#include "RioEventPool.h"

//***************************************************************************
// @brief 지정된 수량만큼 이벤트를 미리 메모리에 연속 할당하고 Free List로 연결합니다.
// @param capacity 추가로 할당할 이벤트 개수
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioEventPool::Initialize(size_t capacity)
{
    if( capacity == 0 )
        return false;

    PLockGuard guard(_lock, __FUNCTION__);

    if( capacity > (static_cast<size_t>(-1) / sizeof(CRioEvent)) )
        return false;

    void* rawBuffer = ::operator new(sizeof(CRioEvent) * capacity, std::nothrow);

    if( rawBuffer == nullptr )
        return false;

    CRioEvent* newBlock = static_cast<CRioEvent*>(rawBuffer);
    size_t constructedCount = 0;

    for( size_t i = 0; i < capacity; ++i )
    {
        try
        {
            // 생성자가 이미 _state를 Free로 기본 초기화하므로 별도 초기 상태
            // 설정 호출이 필요 없습니다.
            ::new (&newBlock[i]) CRioEvent();
            ++constructedCount;
        }
        catch( ... )
        {
            for( size_t j = constructedCount; j > 0; --j )
                newBlock[j - 1].~CRioEvent();

            ::operator delete(rawBuffer);
            return false;
        }
    }

    MemoryBlock block;
    block.ptr = rawBuffer;
    block.count = capacity;

    try
    {
        _memoryBlocks.push_back(block);
    }
    catch( ... )
    {
        for( size_t j = constructedCount; j > 0; --j )
            newBlock[j - 1].~CRioEvent();

        ::operator delete(rawBuffer);
        return false;
    }

    for( size_t i = 0; i < capacity; ++i )
    {
        newBlock[i].SetNextFree(_head);
        _head = &newBlock[i];
    }

    _capacity += capacity;

    return true;
}

//***************************************************************************
// @brief 풀에서 사용 가능한 CRioEvent 객체 하나를 할당받습니다.
// @return CRioEvent* 할당된 CRioEvent 포인터 (풀 고갈 시 nullptr)
//***************************************************************************
CRioEvent* CRioEventPool::Alloc() noexcept
{
    CRioEvent* evt = nullptr;

    {
        PLockGuard guard(_lock, __FUNCTION__);

        if( _head != nullptr )
        {
            evt = _head;
            _head = _head->GetNextFree();

            // 이중 Alloc/Free 탐지 - release 빌드에서도 항상 검사합니다.
            // Free List에는 Free 상태인 이벤트만 있어야 하므로, 여기서
            // Free가 아니라면 free list 자체가 이미 오염된 것입니다.
            ASSERT_CRASH(evt->GetState() == Rio::EEventState::Free && "Double Allocation or Corrupted Event State Detected!");
            evt->SetState(Rio::EEventState::InUse);

            evt->SetNextFree(nullptr);

            _inUseCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    if( evt == nullptr )
    {
        RecordExhaustion();
        return nullptr;
    }

    return evt;
}

//***************************************************************************
// @brief 사용이 끝난 CRioEvent 객체를 풀에 반환합니다.
// @param evt 반환할 CRioEvent 포인터
//***************************************************************************
void CRioEventPool::Free(CRioEvent* evt) noexcept
{
    if( evt == nullptr )
        return;

    PLockGuard guard(_lock, __FUNCTION__);

    //***********************************************************************
    // 이중 Free 탐지 - release 빌드에서도 항상 검사합니다.
    //
    // 여기서 걸러내지 못하면 아래 evt->SetNextFree(_head); _head = evt;가
    // 그대로 실행되어, 이미 Free List에 들어있는 evt를 다시 head로 밀어넣게
    // 됩니다. 그 시점의 _head가 바로 evt 자신이라면(가장 흔한 이중 반납
    // 패턴) evt->Next가 자기 자신을 가리키는 self-loop가 만들어지고, 그
    // 앞에 있던 나머지 Free List 전체가 유실됩니다 - 이후 Alloc()은 같은
    // evt 하나만 영원히 반복 반환하고 풀은 크래시 없이 조용히 무너집니다.
    // 그래서 반드시 Free List를 건드리기 전에 먼저 걸러내야 합니다.
    //***********************************************************************
    if( evt->GetState() != Rio::EEventState::InUse )
    {
        ASSERT_CRASH(false && "CRioEventPool::Free double-free or invalid state detected");
        return;
    }

    //***********************************************************************
    // Event 내부 상태 정리
    //
    // 반드시 Free List publish 이전에 수행합니다.
    //***********************************************************************
    evt->Reset();
    evt->SetState(Rio::EEventState::Free);

    //***********************************************************************
    // Free List에 publish (단일 락 내부에서 atomic transaction 보장)
    //***********************************************************************
    evt->SetNextFree(_head);
    _head = evt;

    //***********************************************************************
    // InUse count 감소
    //***********************************************************************
    const size_t previous =
        _inUseCount.fetch_sub(1, std::memory_order_release);

    if( previous == 0 )
    {
        ASSERT_CRASH(false && "CRioEventPool::Free inUseCount underflow");

        // 방어적으로 복구
        _inUseCount.fetch_add(1, std::memory_order_relaxed);

        // 여기서는 이미 Free List에 넣었으므로
        // 더 이상의 rollback은 하지 않습니다.
        //
        // 정상적인 lifecycle에서는 절대 발생하지 않아야 합니다.
    }
}

//***************************************************************************
// @brief 풀이 소멸될 때 할당된 모든 메모리를 해제합니다.
//***************************************************************************
void CRioEventPool::Release() noexcept
{
    PLockGuard guard(_lock, __FUNCTION__);

    const size_t inUseCount = _inUseCount.load(std::memory_order_acquire);

    if( inUseCount != 0 )
    {
        ASSERT_CRASH(false && "CRioEventPool::Release() called while events are still InUse");
        return;
    }

    for( size_t i = 0; i < _memoryBlocks.size(); ++i )
    {
        void* rawBuffer = _memoryBlocks[i].ptr;
        size_t count = _memoryBlocks[i].count;

        if( rawBuffer == nullptr )
            continue;

        CRioEvent* blockPtr = static_cast<CRioEvent*>(rawBuffer);

        for( size_t j = 0; j < count; ++j )
        {
            if( blockPtr[j].GetState() != Rio::EEventState::Free )
            {
                ASSERT_CRASH(false && "Releasing CRioEventPool while an event is still InUse!");
                return;
            }
        }
    }

    for( size_t i = 0; i < _memoryBlocks.size(); ++i )
    {
        void* rawBuffer = _memoryBlocks[i].ptr;
        size_t count = _memoryBlocks[i].count;

        if( rawBuffer == nullptr )
            continue;

        CRioEvent* blockPtr = static_cast<CRioEvent*>(rawBuffer);

        for( size_t j = 0; j < count; ++j )
            blockPtr[j].~CRioEvent();

        ::operator delete(rawBuffer);
    }

    _memoryBlocks.clear();
    _head = nullptr;
    _capacity = 0;

    _exhaustionCount.store(0, std::memory_order_relaxed);
    _lastLogTick.store(0, std::memory_order_relaxed);
    _inUseCount.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 현재 Pool의 전체 이벤트 용량을 반환합니다.
// @return size_t Pool의 총 이벤트 개수
//***************************************************************************
size_t CRioEventPool::GetCapacity() const
{
    PLockGuard guard(_lock, __FUNCTION__);
    return _capacity;
}

//***************************************************************************
// @brief Pool 고갈 시 통계 누적 및 쓰로틀링 로그 처리
//***************************************************************************
void CRioEventPool::RecordExhaustion()
{
    _exhaustionCount.fetch_add(1, std::memory_order_relaxed);

    const ULONGLONG currentTick = ::GetTickCount64();
    ULONGLONG lastTick = _lastLogTick.load(std::memory_order_relaxed);

    if( currentTick - lastTick >= 5000 )
    {
        if( _lastLogTick.compare_exchange_weak(lastTick, currentTick, std::memory_order_relaxed, std::memory_order_relaxed) )
        {
            const ULONGLONG missedCount = _exhaustionCount.exchange(0, std::memory_order_relaxed);
            const size_t currentCapacity = GetCapacity();

            // 이전에는 missedCount/currentCapacity를 계산만 해두고 실제 로그를
            // 남기지 않아(그냥 (void) 캐스트로 버림) 풀 고갈이 운영 중 완전히
            // 무음으로 삼켜졌습니다. 풀 고갈은 반드시 알아야 하는 이벤트이므로
            // 실제로 로그를 남깁니다. (LOG_WARNING의 실제 시그니처/포맷 지정자는
            // 프로젝트 로깅 매크로에 맞게 조정하십시오 - 여기서는 RioService.cpp에서
            // 이미 쓰고 있는 LOG_WARNING(_T("...")) 관례를 그대로 따랐습니다.)
            LOG_WARNING(_T("[CRioEventPool] Pool exhausted %llu time(s) in the last 5s (capacity=%zu)"),
                missedCount, currentCapacity);
        }
    }
}
