//***************************************************************************
// RioEvent.h : RIO 비동기 I/O 요청 컨텍스트 및 이벤트 정의
//
//***************************************************************************

#ifndef __RIOEVENT_H__
#define __RIOEVENT_H__

#include <memory>
#include <utility>

class CRioObject;
using CRioObjectRef = std::shared_ptr<CRioObject>;

//===========================================================================
// @enum EEventState
// @brief 디버그 모드에서 이벤트 객체의 생명주기(할당/반환) 상태를 추적하기 위한 열거형
//===========================================================================
enum class EEventState : uint8_t
{
    Free,   // 풀(Pool)에 대기 중인 유휴 상태
    InUse   // 로직 스레드에서 할당받아 사용 중인 상태
};

//===========================================================================
// @class CRioEvent
// @brief 개별 RIO 비동기 I/O 요청의 식별 및 세션 수명 소유권을 담는 컨텍스트 클래스.
//===========================================================================
class CRioEvent
{
public:
    enum class EventType : uint8_t
    {
        Receive,
        Send
    };

    //***************************************************************************
    // @brief CRioEvent 생성자
    // @details 이벤트 타입을 수신(Receive)으로 설정하고 프리 리스트 포인터 및 디버그 상태를 초기화합니다.
    //***************************************************************************
    CRioEvent() noexcept
        : _type(EventType::Receive)
        , _nextFree(nullptr)
#ifdef _DEBUG
        , _debugState(EEventState::Free)
#endif
    {
    }

    //***************************************************************************
    // @brief 풀에서 재사용될 때 이벤트 속성과 소유권 대상을 초기화합니다.
    // @param type 비동기 작업 유형 (Receive 또는 Send)
    // @param ownerObj 이 I/O를 소유하는 세션 스마트 포인터 (`CRioObjectRef`)
    //***************************************************************************
    void Initialize(EventType type, CRioObjectRef ownerObj) noexcept
    {
        _type = type;
        _owner = std::move(ownerObj);
        _nextFree = nullptr;
    }

    //***************************************************************************
    // @brief 현재 이벤트의 작업 유형을 반환합니다.
    // @return EventType (Receive 또는 Send)
    //***************************************************************************
    EventType GetEventType() const noexcept { return _type; }

    //***************************************************************************
    // @brief 세션 소유권(shared_ptr)을 안전하게 이동시켜 내부 참조를 비웁니다.
    // @return CRioObjectRef 추출된 세션 스마트 포인터 (이후 내부 _owner는 nullptr로 초기화됨)
    //***************************************************************************
    CRioObjectRef TakeOwner() noexcept
    {
        return std::exchange(_owner, nullptr);
    }

    //***************************************************************************
    // @brief 프리 리스트 체이닝을 위한 다음 이벤트 포인터를 반환합니다.
    // @return CRioEvent* 다음 유휴 이벤트 포인터
    //***************************************************************************
    CRioEvent* GetNextFree() const noexcept { return _nextFree; }

    //***************************************************************************
    // @brief 프리 리스트 체이닝 포인터를 설정합니다.
    // @param next 연결할 다음 CRioEvent 포인터
    //***************************************************************************
    void SetNextFree(CRioEvent* next) noexcept { _nextFree = next; }

    //***************************************************************************
    // @brief 풀로 반환되기 전 내부 상태를 깨끗이 초기화합니다.
    //***************************************************************************
    void Reset() noexcept
    {
        _type = EventType::Receive;
        _owner.reset();
        _nextFree = nullptr;
    }

#ifdef _DEBUG
    //***************************************************************************
    // @brief 디버그 빌드 전용: 이벤트 객체의 현재 생명주기 상태를 설정합니다.
    // @param state 설정할 상태 (EEventState::Free 또는 EEventState::InUse)[cite: 8]
    //***************************************************************************
    void SetDebugState(EEventState state) { _debugState = state; }

    //***************************************************************************
    // @brief 디버그 빌드 전용: 이벤트 객체의 현재 생명주기 상태를 반환합니다.
    // @return EEventState 현재 상태
    //***************************************************************************
    EEventState GetDebugState() const { return _debugState; }
#endif

private:
    EventType       _type;     // 비동기 I/O 타입 (Receive / Send)
    CRioObjectRef   _owner;    // I/O 완료 시까지 세션의 수명을 붙들어 매는 스마트 포인터
    CRioEvent* _nextFree; // 프리 리스트(Free-list) 단방향 연결 포인터

#ifdef _DEBUG
    EEventState     _debugState; // 디버그 빌드 시 Double Free 및 이중 할당 검증을 위한 상태 플래그
#endif
};

#endif // __RIOEVENT_H__