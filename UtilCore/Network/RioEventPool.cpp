
//***************************************************************************
// RioEventPool.cpp : implementation of the CRioEventPool class.
//
//***************************************************************************

#include "pch.h"
#include "RioEventPool.h"

//***************************************************************************
// @brief 지정된 수량만큼 이벤트를 미리 메모리에 연속 할당하고
//        Free List로 연결합니다.
//
// @param capacity 추가로 할당할 이벤트 개수
//
// @return 성공 시 true, 실패 시 false
//
// @details
//      - std::nothrow를 사용하여 raw memory allocation 예외를 차단합니다.
//      - Placement New를 통해 CRioEvent를 생성합니다.
//      - 일부 객체 생성 실패 시 이미 생성된 객체를 역순으로 파괴합니다.
//      - MemoryBlock vector push 실패 시 전체 rollback합니다.
//
// [롤백(Rollback) 및 Strong Exception Guarantee]
//      Placement New 생성 루프나 CVector push_back 중 예외 발생 시,
//      기할당된 Raw Buffer 해제 및 이미 생성된 CRioEvent 객체들의 역순(Reverse)
//      소멸자를 명시적으로 호출하여 메모리/자원 누수 없는 강력한 예외 안전성을 보장합니다.
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

    // ---------------------------------------------------------------------
    // CRioEvent 객체 생성
    // ---------------------------------------------------------------------
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

    // ---------------------------------------------------------------------
    // 메모리 블록 관리 정보 저장
    // ---------------------------------------------------------------------
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

    // ---------------------------------------------------------------------
    // 새로 생성된 이벤트를 Free List에 연결
    //
    // LIFO 방식:
    //
    //      newBlock[0] -> ...
    //      newBlock[1] -> newBlock[0]
    //      newBlock[2] -> newBlock[1]
    //      ...
    // ---------------------------------------------------------------------
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
//
// @param type 비동기 작업 이벤트 타입
// @param ownerObj 이벤트를 소유하는 CRioObject shared_ptr
//
// @return 할당된 CRioEvent 포인터
//         풀 고갈 시 nullptr
//
// @details
//      Free List Pop은 lock 내부에서 수행하지만
//      CRioEvent::Initialize()는 lock 외부에서 수행합니다.
//
//      따라서 Initialize()가 상대적으로 오래 걸리더라도
//      다른 Alloc()/Free() 호출이 Pool lock 때문에 대기하지 않습니다.
//
// [Lock 영역 최소화 분리 기술]
//      - Critical Section: 오직 포인터 변경(_head = _head->GetNextFree())만 빠르게 완료
//      - Non-Critical Section: Lock 해제 후 _owner(shared_ptr 복사/레퍼런스 카운팅) 설정 수행
//***************************************************************************
CRioEvent* CRioEventPool::Alloc(Rio::EventType type, CRioObjectRef ownerObj)
{
    CRioEvent* evt = nullptr;

    {
        PLockGuard guard(_lock, __FUNCTION__);

        if( _head != nullptr )
        {
            evt = _head;
            _head = _head->GetNextFree();

            // InUse 객체에서는 stale Free List link를 제거합니다.
            evt->SetNextFree(nullptr);
            _inUseCount.fetch_add(1, std::memory_order_relaxed);

#ifdef _DEBUG
            assert(evt->GetDebugState() == Rio::EEventState::Free && "Double Allocation or Corrupted Event State Detected!");
            evt->SetDebugState(Rio::EEventState::InUse);
#endif
        }
    }

    // ---------------------------------------------------------------------
    // Pool exhaustion은 lock 외부에서 처리합니다.
    // ---------------------------------------------------------------------
    if( evt == nullptr )
    {
        RecordExhaustion();
        return nullptr;
    }

    // ---------------------------------------------------------------------
    // 실제 이벤트 초기화
    // ---------------------------------------------------------------------
    evt->Initialize(type, ownerObj);

    return evt;
}

//***************************************************************************
// @brief 사용이 끝난 CRioEvent 객체를 풀에 반환합니다.
//
// @details
//      Lifecycle:
//
//          Dispatch()
//              |
//              v
//          ObjectIoCount--
//              |
//              v
//          EventPool::Free()
//              |
//              v
//          Reset()
//              |
//              v
//          Free List Push
//
//      Reset()은 Owner shared_ptr을 해제하고 이벤트 내부 상태를
//      초기화합니다.
//
// @param evt 반환할 CRioEvent 포인터
//
// [스레드 안전한 반환 처리]
//      Alloc과 마찬가지로 Lock 외부에서 Reset()을 호출해 _owner shared_ptr의
//      레퍼런스 감소 및 객체 해제 작업을 먼저 수행하여 Lock 보유 시간을 최소화합니다.
//***************************************************************************
void CRioEventPool::Free(CRioEvent* evt)
{
    if( evt == nullptr )
        return;

#ifdef _DEBUG
    assert(evt->GetDebugState() == Rio::EEventState::InUse && "Double Free Detected!");
#endif

    // ---------------------------------------------------------------------
    // Payload / Owner 상태 초기화
    // ---------------------------------------------------------------------
    evt->Reset();

    {
        PLockGuard guard(_lock, __FUNCTION__);

#ifdef _DEBUG
        assert(evt->GetDebugState() == Rio::EEventState::InUse && "CRioEvent state changed unexpectedly before Free List publish!");
        evt->SetDebugState(Rio::EEventState::Free);
#endif

        // -----------------------------------------------------------------
        // Free List Push
        // -----------------------------------------------------------------
        evt->SetNextFree(_head);
        _head = evt;
        _inUseCount.fetch_sub(1, std::memory_order_relaxed);
    }
}

//***************************************************************************
// @brief 풀이 소멸될 때 할당된 모든 메모리를 해제합니다.
//
// @details
//      반드시 다음 shutdown ordering이 보장된 이후 호출되어야 합니다.
//
//          1. Stop Accept
//          2. Stop new RIO Post
//          3. Shutdown sockets
//          4. RIO Completion Drain
//          5. Outstanding RIO I/O == 0
//          6. RIO CQ shutdown
//          7. EventPool Release
//          8. Object destruction
//
//      특히 outstanding RIO I/O가 남아있는 상태에서 Release()가 호출되면
//      RIO RequestContext가 가리키는 CRioEvent 메모리가 해제될 수 있으므로
//      절대로 허용해서는 안 됩니다.
//
// [명시적 소멸자 호출 및 메모리 해제]
//      _memoryBlocks 배열을 순회하며 Placement New로 할당되었던 각 객체의
//      명시적 소멸자(~CRioEvent())를 호출한 뒤 ::operator delete를 실행합니다.
//
// [중요]
//      InUse 이벤트가 하나라도 존재하면 Release를 수행하지 않습니다.
//      Debug 빌드에서는 assert를 발생시키며, Release 빌드에서는 안전하게
//      반환하여 UAF가 발생하는 것을 방지합니다.
//***************************************************************************
void CRioEventPool::Release() noexcept
{
    PLockGuard guard(_lock, __FUNCTION__);

    const size_t inUseCount = _inUseCount.load(std::memory_order_acquire);

    if( inUseCount != 0 )
    {
        assert(false && "CRioEventPool::Release() called while events are still InUse");
        std::terminate();
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
//
// @return Pool의 총 이벤트 개수
//***************************************************************************
size_t CRioEventPool::GetCapacity() const
{
    PLockGuard guard(_lock, __FUNCTION__);
    return _capacity;
}

//***************************************************************************
// @brief Pool 고갈 시 통계 누적 및 쓰로틀링 로그 처리
//
// @details
//      Pool Free List lock을 획득하지 않은 상태에서 호출됩니다.
//      _capacity 조회 시에는 GetCapacity()가 별도의 lock을 사용합니다.
//
// [Atomic CAS 기반 로그 쓰로틀링(Log Throttling)]
//      풀 고갈 시 순간적으로 수천/수만 번 발생하는 패킷 처리 실패 로그로 인해
//      I/O 병목이 더욱 악화되는 'Log Storm' 현상을 방지합니다.
//      compare_exchange_weak를 사용해 5초(5000ms) 간격으로 단 1개의 스레드만
//      누적 고갈 횟수(missedCount)를 인쇄하도록 스레드 안전하게 통제합니다.
//***************************************************************************
void CRioEventPool::RecordExhaustion()
{
    _exhaustionCount.fetch_add(1, std::memory_order_relaxed);

    const ULONGLONG currentTick = ::GetTickCount64();
    ULONGLONG lastTick = _lastLogTick.load(std::memory_order_relaxed);

    if( currentTick - lastTick >= 5000 )
    {
        if( _lastLogTick.compare_exchange_weak(
            lastTick,
            currentTick,
            std::memory_order_relaxed,
            std::memory_order_relaxed) )
        {
            const ULONGLONG missedCount = _exhaustionCount.exchange(0, std::memory_order_relaxed);
            const size_t currentCapacity = GetCapacity();

            // -----------------------------------------------------------------
            // 실제 프로젝트의 logging 시스템을 연결합니다.
            //
            // LOG_ERROR("CRioEventPool Exhausted! occurrences=%llu capacity=%zu", missedCount + 1, currentCapacity);
            // -----------------------------------------------------------------
            (void)missedCount;
            (void)currentCapacity;
        }
    }
}

#ifdef _DEBUG

//***************************************************************************
// @brief 디버그 빌드 전용 초기 상태 설정
//***************************************************************************
void CRioEventPool::_DEBUG_SET_INITIAL_STATE(CRioEvent* evt)
{
    if( evt != nullptr )
        evt->SetDebugState(Rio::EEventState::Free);
}

#else

void CRioEventPool::_DEBUG_SET_INITIAL_STATE(CRioEvent*) noexcept
{
}

#endif