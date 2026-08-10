
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
    : _eventType(EventType::Receive)
    , _owner(nullptr)
    , _nextFree(nullptr)
#ifdef _DEBUG
    , _debugState(EEventState::Free)
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
    _nextFree = nullptr;

#ifdef _DEBUG
    _debugState = EEventState::Free;
#endif
}

//***************************************************************************
// @brief 이벤트 객체를 새로운 RIO 작업에 사용할 수 있도록 초기화합니다.
//
// @param type RIO Receive 또는 Send 이벤트 타입
// @param ownerObj 이벤트를 소유하는 CRioObject shared_ptr
//
// @details
//      CRioEventPool::Alloc()에서 Free List에서 Pop한 직후 호출됩니다.
//
//      Owner는 shared_ptr로 저장되므로 RIO completion이 도착한 이후
//      Dispatch()가 완료될 때까지 CRioObject의 lifetime이 유지됩니다.
//
// [메모리 안전성 보장]
//      기존 _owner가 예외 상황으로 인해 남아있더라도 안전하게 reset()을 먼저 호출하여
//      이전 세션/소유자의 참조 카운트를 즉시 차감시킵니다.
//***************************************************************************
void CRioEvent::Initialize(EventType type, const CRioObjectRef& ownerObj) noexcept
{
    // 기존 Owner가 남아 있는 경우를 방어적으로 제거합니다.
    _owner.reset();

    // 새로운 이벤트 타입 설정
    _eventType = type;

    // 새로운 Owner shared_ptr 획득
    _owner = ownerObj;

    // InUse 상태에서는 Free List 연결이 없어야 합니다.
    _nextFree = nullptr;

#ifdef _DEBUG
    assert(_debugState == EEventState::InUse && "CRioEvent must be InUse before Initialize()");
#endif
}

//***************************************************************************
// @brief 이벤트의 I/O 상태 및 Owner를 초기화합니다.
//
// @details
//      CRioEventPool::Free()에서 호출되며,
//      EventPool의 Free List에 다시 반환되기 전에 실행됩니다.
//
//      중요:
//          Reset()은 Free List link를 초기화합니다.
//          이후 CRioEventPool::Free()가 SetNextFree()를 호출하여
//          새로운 Free List link를 설정합니다.
//
// [재사용 안전성 및 부작용 방지]
//      _owner.reset()을 명시적으로 실행함으로써 반환 즉시 소유 객체의 레퍼런스 카운트를 낮춥니다.
//      만약 이 작업이 이뤄지지 않을 경우, 사용되지도 않는 이벤트를 풀에 보유하는 동안
//      CRioObject 메모리가 해제되지 않는 Memory Leak(상주) 문제가 생길 수 있습니다.
//***************************************************************************
void CRioEvent::Reset() noexcept
{
    // RIO completion 처리에 사용했던 Owner shared_ptr 해제
    _owner.reset();

    // 기본 이벤트 타입으로 복원
    _eventType = EventType::Receive;

    // 기존 Free List link 제거
    _nextFree = nullptr;
}

//***************************************************************************
// @brief 이벤트 Owner shared_ptr을 설정합니다.
//
// @param ownerObj 새로운 CRioObject shared_ptr
//
// @note
//      일반적인 lifecycle에서는 Initialize()가 이 기능을 수행합니다.
//      이 함수는 명시적으로 Owner를 설정해야 하는 외부 코드 및
//      기존 설계와의 호환성을 위해 제공합니다.
//
// @details
//      전달받은 shared_ptr의 레퍼런스 카운터가 1 증가하여 객체 생존 기간을 원자적으로 연장합니다.
//***************************************************************************
void CRioEvent::SetOwnerShared(const CRioObjectRef& ownerObj) noexcept
{
    _owner = ownerObj;
}

//***************************************************************************
// @brief 이벤트 Owner를 회수합니다.
//
// @return CRioObject shared_ptr
//
// @details
//      CRioCore::ProcessRioResult()에서 호출합니다.
//
//      Owner를 std::move하여 CRioEvent 내부의 Owner를 비운 뒤
//      호출자에게 소유권을 전달합니다.
//
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
//
// @return Owner shared_ptr
//
// @details
//      반환된 shared_ptr은 호출자의 scope 동안 CRioObject lifetime을
//      추가로 유지합니다.
//
//      TakeOwner()와 달리 복사(Copy)가 일어나므로 원자적 참조 카운트가 1 증가합니다.
//***************************************************************************
CRioObjectRef CRioEvent::GetOwnerShared() const noexcept
{
    return _owner;
}