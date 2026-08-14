
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
            ::new (&newBlock[i]) CRioEvent();
            _DEBUG_SET_INITIAL_STATE(&newBlock[i]);
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

#ifdef _DEBUG
            assert(evt->GetDebugState() == Rio::EEventState::Free &&"Double Allocation or Corrupted Event State Detected!");
            evt->SetDebugState(Rio::EEventState::InUse);
#endif

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

#ifdef _DEBUG

    if( evt->GetDebugState() != Rio::EEventState::InUse )
    {
        assert(false && "CRioEventPool::Free double-free or invalid state detected");
        return;
    }

#endif

    //***********************************************************************
    // Event 내부 상태 정리
    //
    // 반드시 Free List publish 이전에 수행합니다.
    //***********************************************************************
    evt->Reset();

#ifdef _DEBUG

    evt->SetDebugState(Rio::EEventState::Free);

#endif

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
        assert(false && "CRioEventPool::Free inUseCount underflow");

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
        assert(false && "CRioEventPool::Release() called while events are still InUse");
        return;
    }

#ifdef _DEBUG
    for( size_t i = 0; i < _memoryBlocks.size(); ++i )
    {
        void* rawBuffer = _memoryBlocks[i].ptr;
        size_t count = _memoryBlocks[i].count;

        if( rawBuffer == nullptr )
            continue;

        CRioEvent* blockPtr = static_cast<CRioEvent*>(rawBuffer);

        for( size_t j = 0; j < count; ++j )
        {
            if( blockPtr[j].GetDebugState() != Rio::EEventState::Free )
            {
                assert(false && "Releasing CRioEventPool while an event is still InUse!");
                return;
            }
        }
    }
#endif

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

            (void)missedCount;
            (void)currentCapacity;
        }
    }
}

#ifdef _DEBUG

//***************************************************************************
// @brief 디버그 빌드 전용 초기 상태 설정
// @param evt 상태를 설정할 CRioEvent 객체 포인터
//***************************************************************************
void CRioEventPool::_DEBUG_SET_INITIAL_STATE(CRioEvent* evt)
{
    if( evt != nullptr )
        evt->SetDebugState(Rio::EEventState::Free);
}

#else

//***************************************************************************
// @brief 릴리즈 빌드 전용 인라인 stub
//***************************************************************************
void CRioEventPool::_DEBUG_SET_INITIAL_STATE(CRioEvent*) noexcept
{
}

#endif