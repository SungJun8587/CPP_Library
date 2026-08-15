
//***************************************************************************
// RioEvent.h : interface for the CRioEvent class.
//
//***************************************************************************

#ifndef __RIOEVENT_H__
#define __RIOEVENT_H__

#ifndef __RIOCOMMON_H__
#include <Network/RIO/RioCommon.h>
#endif

#ifndef __RIOBUFFER_H__
#include <Network/RIO/RioBuffer.h>
#endif

class CRioBuffer;

//***************************************************************************
// @class CRioEvent
// @brief RIO Completion RequestContext로 사용되는 이벤트 객체
//
// @details
//      CRioEvent는 RIO I/O 하나의 lifecycle을 표현합니다.
//      RIO API의 RequestContext에는 CRioEvent의 주소가 들어가며,
//      완료 처리 시 CRioCore가 해당 주소를 다시 CRioEvent*로 복원합니다.
//      Owner는 shared_ptr로 보유합니다.
//      따라서 RIO completion이 도착한 이후에도 Dispatch가 완료될 때까지
//      CRioObject의 lifetime이 유지됩니다.
//
// [메모리 레이아웃 및 캐시 최적화]
//      CRioEvent 객체는 CRioEventPool에 의해 청크 단위(대량)로 메모리에 연속 할당됩니다.
//      따라서 멤버 변수 정렬(64비트 기준 포인터 8바이트, enum 1바이트)을 조밀하게 관리하여
//      캐시 라인 바운더리를 효율적으로 활용하도록 설계되어 있습니다.
//
// [동기화 및 Thread-Safety]
//      CRioEvent 자체는 내부 동기화 primitives(std::mutex 등)를 가지지 않는 Plain
//      구조 형태에 가깝습니다. 객체의 스레드 안전성은 CRioEventPool의 Alloc/Free 시점에
//      수행되는 락 동기화와, RIO I/O 진행 중 단 하나의 워커/제출 스레드만 이 객체에
//      접근하도록 보장하는 Ownership Transfer 원칙에 의존합니다.
//
// [Lifecycle]
//
//      CRioEventPool::Alloc()
//                |
//                v
//      CRioEvent::Initialize()
//                |
//                v
//      CRioObject::IncrementIoCount()
//                |
//                v
//      RIOReceiveEx()/RIOSendEx()
//                |
//                v
//      RIO completion
//                |
//                v
//      CRioEvent::TakeOwner()
//                |
//                v
//      CRioObject::Dispatch()
//                |
//                v
//      CRioObject::DecrementIoCount()
//                |
//                v
//      CRioEventPool::Free()
//                |
//                v
//      CRioEvent::Reset()
//
//***************************************************************************
class CRioEvent
{
public:
    //***************************************************************************
    // @struct BufferBinding
    // @brief RIO Buffer slot ownership binding 정보
    // @details CRioBuffer 포인터와 해당 버퍼에서 할당받은 slotIndex를 관리합니다.
    //***************************************************************************
    struct BufferBinding
    {
        CRioBuffer* buffer{ nullptr };                  // slot을 소유하는 CRioBuffer 포인터
        uint32_t slotIndex{ Rio::kInvalidSlotIndex };   // 할당받은 버퍼 slot 인덱스
    };

public:
    CRioEvent() noexcept;
    ~CRioEvent() noexcept;

    CRioEvent(const CRioEvent&) = delete;
    CRioEvent& operator=(const CRioEvent&) = delete;

    CRioEvent(CRioEvent&&) = delete;
    CRioEvent& operator=(CRioEvent&&) = delete;

public:
    void Initialize(Rio::EventType type, const CRioObjectRef& ownerObj) noexcept;
    void Reset() noexcept;

    //***************************************************************************
    // @brief 이벤트 타입을 반환합니다.
    // @return EventType (Receive 또는 Send)
    //***************************************************************************
    Rio::EventType GetEventType() const noexcept
    {
        return _eventType;
    }

    CRioObjectRef TakeOwner() noexcept;
    CRioObjectRef GetOwnerShared() const noexcept;

    bool BindBufferSlot(CRioBuffer* buffer, uint32_t slotIndex) noexcept;

    //***************************************************************************
    // @brief 등록된 Buffer-slot binding 개수를 반환합니다.
    // @return size_t 바인딩된 버퍼 슬롯 개수
    //***************************************************************************
    size_t GetBufferBindingCount() const noexcept
    {
        return _bufferBindings.size();
    }

    const BufferBinding* GetBufferBinding(size_t index) const noexcept;
    const CVector<BufferBinding>& GetBufferBindings() const noexcept;

    //***************************************************************************
    // @brief Buffer binding 전체를 제거합니다.
    // @note 실제 CRioBuffer::FreeSlot() 호출은 하지 않으며 단순 목록을 비웁니다.
    //***************************************************************************
    void ClearBufferBindings() noexcept
    {
        _bufferBindings.clear();
    }

    //***************************************************************************
    // @brief Free List의 다음 이벤트를 설정합니다.
    // @param next Free List 단방향 연결 리스트에서 다음 노드가 될 CRioEvent 포인터
    //***************************************************************************
    void SetNextFree(CRioEvent* next) noexcept
    {
        _nextFree = next;
    }

    //***************************************************************************
    // @brief Free List의 다음 이벤트를 반환합니다.
    // @return CRioEvent* 다음 자유 이벤트 노드의 포인터 (없을 경우 nullptr)
    //***************************************************************************
    CRioEvent* GetNextFree() const noexcept
    {
        return _nextFree;
    }

#ifdef _DEBUG
    //***************************************************************************
    // @brief 디버그 빌드에서 현재 Lifecycle 상태를 반환합니다.
    // @return EEventState (Free 또는 InUse)
    //***************************************************************************
    Rio::EEventState GetDebugState() const noexcept
    {
        return _debugState;
    }

    //***************************************************************************
    // @brief 디버그 빌드에서 Lifecycle 상태를 설정합니다.
    // @param state 설정하고자 하는 상태 값
    //***************************************************************************
    void SetDebugState(Rio::EEventState state) noexcept
    {
        _debugState = state;
    }
#endif

private:
    Rio::EventType _eventType{ Rio::EventType::Receive };   // 1바이트(uint8_t) 크기의 송/수신 구분 플래그
    CRioObjectRef _owner;                                   // completion이 처리될 때까지 lifetime을 유지하는 Owner shared_ptr
    CVector<BufferBinding> _bufferBindings;                 // Scatter-Gather 지원을 위한 Buffer-slot ownership 목록
    CRioEvent* _nextFree{ nullptr };                        // CRioEventPool Free List 연결 포인터

#ifdef _DEBUG
    Rio::EEventState _debugState{ Rio::EEventState::Free }; // 디버그 전용 Lifecycle 상태 (Double-Free 및 UAF 검출용)
#endif
};

#endif // ndef __RIOEVENT_H__