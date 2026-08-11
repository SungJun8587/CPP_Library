
//***************************************************************************
// RioEvent.cpp : implementation of the CRioEvent class.
//
//***************************************************************************

#include "pch.h"
#include "RioEvent.h"

//***************************************************************************
// @brief CRioEvent 기본 생성자
// @details
//      Pool에 의해 생성된 직후에는 아직 사용 중인 I/O가 없으므로
//      Free 상태로 초기화합니다.
//
//      모든 초기화 표현식은 예외를 발생시키지 않는 noexcept 조건으로 수행됩니다.
//***************************************************************************
CRioEvent::CRioEvent() noexcept
    : _eventType(Rio::EventType::Receive)
    , _owner(nullptr)
    , _bufferBindings()
    , _nextFree(nullptr)
#ifdef _DEBUG
    , _debugState(Rio::EEventState::Free)
#endif
{
}

//***************************************************************************
// @brief CRioEvent 소멸자
//
// @details
//      정상적인 lifecycle에서는 CRioEventPool::Release()가
//      outstanding I/O가 모두 종료된 이후 호출되므로
//      Owner가 남아 있지 않아야 합니다.
//
//      다만 방어적으로 shared_ptr을 여기서 해제합니다.
//
//      _nextFree 포인터 역시 dangling reference를 방지하기 위해 null로 초기화합니다.
//***************************************************************************
CRioEvent::~CRioEvent() noexcept
{
    _owner.reset();

    // 실제 CRioBuffer::FreeSlot()은 여기서 호출하지 않습니다.
    //
    // 정상 lifecycle에서는:
    //
    //     RIO completion
    //          ↓
    //     CRioCore::ProcessRioResult()
    //          ↓
    //     CRioBuffer::FreeSlot()
    //          ↓
    //     CRioEventPool::Free()
    //          ↓
    //     CRioEvent::Reset()
    //
    // 순서로 slot이 먼저 반환되어야 합니다.
    _bufferBindings.clear();

    _nextFree = nullptr;

#ifdef _DEBUG
    _debugState = Rio::EEventState::Free;
#endif
}

//***************************************************************************
// @brief 이벤트 객체를 새로운 RIO 작업에 사용할 수 있도록 초기화합니다.
// @param type RIO Receive 또는 Send 이벤트 타입
// @param ownerObj 이벤트를 소유하는 CRioObject shared_ptr
// @note
//      Buffer binding은 이 함수 이후 BindBufferSlot()을 통해 추가합니다.
//***************************************************************************
void CRioEvent::Initialize(Rio::EventType type, const CRioObjectRef& ownerObj) noexcept
{
    // 기존 Owner가 남아 있는 경우를 방어적으로 제거합니다.
    _owner.reset();

    // 이전 I/O의 Buffer binding이 남아 있어서는 안 됩니다.
    _bufferBindings.clear();

    // 새로운 이벤트 타입 설정
    _eventType = type;

    // 새로운 Owner shared_ptr 획득
    _owner = ownerObj;

    // InUse 상태에서는 Free List 연결이 없어야 합니다.
    _nextFree = nullptr;

#ifdef _DEBUG
    assert(_debugState == Rio::EEventState::InUse && "CRioEvent must be InUse before Initialize()");
#endif
}

//***************************************************************************
// @brief Buffer-slot ownership을 CRioEvent에 연결합니다.
// @param buffer     slot을 소유하는 CRioBuffer
// @param slotIndex  AllocSlot()으로 확보한 slot index
// @return 성공 시 true
// @details
//      이 함수는 RIO submission 전에만 호출해야 합니다.
//      CRioEvent는 completion이 발생할 때까지 다음 정보를 유지합니다.
//          CRioBuffer*
//          slotIndex
//      이후 CRioCore::ProcessRioResult()에서 해당 정보를 이용하여
//      CRioBuffer::FreeSlot()을 호출합니다.
//***************************************************************************
bool CRioEvent::BindBufferSlot(CRioBuffer* buffer, uint32_t slotIndex) noexcept
{
    if( buffer == nullptr )
    {
        assert(false && "CRioEvent::BindBufferSlot buffer is null");
        return false;
    }

    if( slotIndex == Rio::kInvalidSlotIndex )
    {
        assert(false && "CRioEvent::BindBufferSlot invalid slot index");
        return false;
    }

#ifdef _DEBUG
    assert(_debugState == Rio::EEventState::InUse && "CRioEvent must be InUse before BindBufferSlot()");
#endif

    try
    {
        _bufferBindings.push_back(
            BufferBinding
            {
                buffer,
                slotIndex
            });
    }
    catch( ... )
    {
        assert(false && "CRioEvent::BindBufferSlot allocation failed");
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 특정 Buffer-slot binding을 조회합니다.
// @param index 조회할 바인딩 인덱스
// @return const CRioEvent::BufferBinding* 해당 바인딩 객체의 포인터 (인덱스 유효하지 않을 시 nullptr)
//***************************************************************************
const CRioEvent::BufferBinding* CRioEvent::GetBufferBinding(size_t index) const noexcept
{
    if( index >= _bufferBindings.size() )
    {
        return nullptr;
    }

    return &_bufferBindings[index];
}

//***************************************************************************
// @brief 전체 Buffer-slot binding 목록의 참조를 반환합니다.
// @return const CVector<CRioEvent::BufferBinding>& 전체 바인딩 벡터 참조
//***************************************************************************
const CVector<CRioEvent::BufferBinding>& CRioEvent::GetBufferBindings() const noexcept
{
    return _bufferBindings;
}

//***************************************************************************
// @brief 이벤트의 I/O 상태 및 Owner/Buffer binding을 초기화합니다.
// @note
//      실제 CRioBuffer::FreeSlot()은 여기서 수행하지 않습니다.
//      FreeSlot()은 반드시 CRioCore의 completion 처리 단계에서
//      먼저 수행되어야 합니다.
//***************************************************************************
void CRioEvent::Reset() noexcept
{
    // RIO completion 처리에 사용했던 Owner shared_ptr 해제
    _owner.reset();

    // Buffer binding 제거
    //
    // 주의:
    // 여기서는 실제 slot을 Free하지 않습니다.
    // CRioCore가 먼저 FreeSlot()을 수행해야 합니다.
    _bufferBindings.clear();

    // 기본 이벤트 타입으로 복원
    _eventType = Rio::EventType::Receive;

    // 기존 Free List link 제거
    _nextFree = nullptr;
}

//***************************************************************************
// @brief 이벤트 Owner를 회수합니다.
// @return CRioObject shared_ptr
// @details
//      CRioCore::ProcessRioResult()에서 호출합니다.
//      Owner를 std::move하여 CRioEvent 내부의 Owner를 비운 뒤
//      호출자에게 소유권을 전달합니다.
//      호출자는 반환된 shared_ptr을 Dispatch() 완료 시점까지 유지합니다.
//
// [성능 최적화 기법]
//      std::move를 활용한 rvalue 이전(Zero-Copy) 방식을 사용하여
//      shared_ptr의 atomic reference count 증가/감소 연산 비용을 통째로 생략시킵니다.
//***************************************************************************
CRioObjectRef CRioEvent::TakeOwner() noexcept
{
    return std::move(_owner);
}

//***************************************************************************
// @brief 이벤트 Owner를 shared_ptr 형태로 조회합니다.
// @return Owner shared_ptr
// @details
//      반환된 shared_ptr은 호출자의 scope 동안 CRioObject lifetime을
//      추가로 유지합니다.
//      TakeOwner()와 달리 복사(Copy)가 일어나므로 원자적 참조 카운트가 1 증가합니다.
//***************************************************************************
CRioObjectRef CRioEvent::GetOwnerShared() const noexcept
{
    return _owner;
}