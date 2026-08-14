
//***************************************************************************
// RioEvent.cpp : implementation of the CRioEvent class.
//
//***************************************************************************

#include "pch.h"
#include "RioEvent.h"

//***************************************************************************
// @brief CRioEvent 기본 생성자
// @details Pool 생성 직후 기본값(Free 상태)으로 초기화합니다.
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
// @details residual shared_ptr 및 바인딩 정보를 해제합니다.
//***************************************************************************
CRioEvent::~CRioEvent() noexcept
{
    _owner.reset();
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
//***************************************************************************
void CRioEvent::Initialize(Rio::EventType type, const CRioObjectRef& ownerObj) noexcept
{
#ifdef _DEBUG
    assert(_debugState == Rio::EEventState::InUse && "CRioEvent must be InUse before Initialize()");
#endif

    _eventType = type;
    _owner = ownerObj;
    _bufferBindings.clear();
    _nextFree = nullptr;
}

//***************************************************************************
// @brief Buffer-slot ownership을 CRioEvent에 연결합니다.
// @param buffer slot을 소유하는 CRioBuffer
// @param slotIndex AllocSlot()으로 확보한 slot index
// @return 성공 시 true, 실패 시 false
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
        _bufferBindings.push_back({ buffer, slotIndex });
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
// @return const CRioEvent::BufferBinding* 해당 바인딩 객체의 포인터 (유효하지 않을 시 nullptr)
//***************************************************************************
const CRioEvent::BufferBinding* CRioEvent::GetBufferBinding(size_t index) const noexcept
{
    if( index >= _bufferBindings.size() )
        return nullptr;

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
//***************************************************************************
void CRioEvent::Reset() noexcept
{
    _owner.reset();
    _bufferBindings.clear();
    _eventType = Rio::EventType::Receive;
    _nextFree = nullptr;
}

//***************************************************************************
// @brief 이벤트 Owner를 회수합니다.
// @return CRioObjectRef 소유권이 이전된 CRioObject shared_ptr
//***************************************************************************
CRioObjectRef CRioEvent::TakeOwner() noexcept
{
    return std::move(_owner);
}

//***************************************************************************
// @brief 이벤트 Owner를 shared_ptr 형태로 조회합니다.
// @return CRioObjectRef 참조 카운트가 증가된 Owner shared_ptr
//***************************************************************************
CRioObjectRef CRioEvent::GetOwnerShared() const noexcept
{
    return _owner;
}