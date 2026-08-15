
//***************************************************************************
// RioReceive.cpp : implementation of the CRioReceive class.
//
//***************************************************************************

#include "pch.h"
#include "RioReceive.h"

namespace
{
    //***************************************************************************
    // @brief CRioEvent에 바인딩된 슬롯 자원을 즉시 반환합니다.
    // @param rioEvent 바인딩 정보를 가지고 있는 이벤트 객체
    //***************************************************************************
    void RollbackBufferBindings(CRioEvent* rioEvent) noexcept
    {
        if( rioEvent == nullptr )
        {
            return;
        }

        const CVector<CRioEvent::BufferBinding>& bindings = rioEvent->GetBufferBindings();

        for( const CRioEvent::BufferBinding& binding : bindings )
        {
            if( binding.buffer == nullptr || binding.slotIndex == Rio::kInvalidSlotIndex )
            {
                assert(false && "CRioReceive rollback contains invalid BufferBinding");
                continue;
            }

            const bool freed = binding.buffer->FreeSlot(binding.slotIndex);

            if( !freed )
            {
                assert(false && "CRioReceive rollback FreeSlot failed");
            }
        }

        rioEvent->ClearBufferBindings();
    }

    //***************************************************************************
    // @brief Submission 실패시 완벽한 역순 롤백 트랜잭션을 수행합니다.
    //        Buffer Slot 반환 -> Event Owner 반환 -> Object IoCount-- -> EventPool Free
    // @param core RIO 코어 객체 참조
    // @param rioEvent 롤백할 RIO 이벤트 객체
    // @param owner 대상 RIO 객체 포인터
    //***************************************************************************
    void RollbackSubmission(CRioCore& core, CRioEvent* rioEvent, CRioObject* owner) noexcept
    {
        if( rioEvent == nullptr )
        {
            if( owner != nullptr )
            {
                owner->DecrementIoCount();
            }
            return;
        }

        RollbackBufferBindings(rioEvent);

        CRioObjectRef rollbackOwner = rioEvent->TakeOwner();

        if( rollbackOwner )
        {
            rollbackOwner->DecrementIoCount();
        }
        else if( owner != nullptr )
        {
            owner->DecrementIoCount();
        }

        if( core.GetEventPool() != nullptr )
        {
            core.GetEventPool()->Free(rioEvent);
        }
    }
}

//***************************************************************************
// @brief 단일 버퍼 기반 RIO Receive 요청을 등록합니다.
// @param core RIO 코어 객체
// @param requestQueue RIO 요청 큐 핸들
// @param buffer 수신받을 RIO_BUF 구조체
// @param bufferOwner 버퍼 소유자 객체
// @param slotIndex 버퍼 슬롯 인덱스
// @param rioEvent 작업에 사용할 RIO 이벤트 객체
// @param owner 비동기 완료 결과를 디스패치할 RIO 객체
// @param flags RIOReceive 옵션 플래그
// @return bool 제출 성공 여부
//***************************************************************************
bool CRioReceive::Receive(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF& buffer,
    CRioBuffer* bufferOwner,
    uint32_t slotIndex,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr || bufferOwner == nullptr )
    {
        return false;
    }

    if( buffer.BufferId == RIO_INVALID_BUFFERID || buffer.Length == 0 )
    {
        return false;
    }

    if( slotIndex == Rio::kInvalidSlotIndex )
    {
        assert(false && "CRioReceive::Receive invalid slot index");
        return false;
    }

    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->GetRioObjectPtr();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr )
    {
        return false;
    }

    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    rioEvent->Initialize(Rio::EventType::Receive, ownerRef);

    if( !rioEvent->BindBufferSlot(bufferOwner, slotIndex) )
    {
        bufferOwner->FreeSlot(slotIndex);
        RollbackSubmission(core, rioEvent, owner);
        return false;
    }

    const bool submitted = core.TrySubmit(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOReceive(
                requestQueue,
                const_cast<PRIO_BUF>(&buffer),
                1,
                flags,
                reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        RollbackSubmission(core, rioEvent, owner);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 다중 버퍼 및 확장 옵션 기반의 RIOReceiveEx 요청을 등록합니다.
// @param core RIO 코어 객체
// @param requestQueue RIO 요청 큐 핸들
// @param data 수신할 RIO_BUF 포인터
// @param dataBufferCount 데이터 버퍼 개수
// @param dataBindings 각 데이터 버퍼에 대한 바인딩 정보 배열 (nullptr 허용: 사전 등록 버퍼 등)
// @param localAddress 로컬 주소 RIO_BUF (선택 사항)
// @param remoteAddress 원격 주소 RIO_BUF (선택 사항)
// @param control 제어 데이터 RIO_BUF (선택 사항)
// @param rioEvent 작업에 사용할 RIO 이벤트 객체
// @param owner 비동기 완료 결과를 디스패치할 RIO 객체
// @param flags RIOReceiveEx 옵션 플래그
// @return bool 제출 성공 여부
//***************************************************************************
bool CRioReceive::ReceiveEx(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF* data,
    ULONG dataBufferCount,
    const CRioEvent::BufferBinding* dataBindings,
    const RIO_BUF* localAddress,
    const RIO_BUF* remoteAddress,
    const RIO_BUF* control,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || data == nullptr || dataBufferCount == 0 )
    {
        return false;
    }

    if( rioEvent == nullptr || owner == nullptr )
    {
        return false;
    }

    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->GetRioObjectPtr();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr )
    {
        return false;
    }

    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    rioEvent->Initialize(Rio::EventType::Receive, ownerRef);

    if( dataBindings != nullptr )
    {
        for( ULONG i = 0; i < dataBufferCount; ++i )
        {
            const CRioEvent::BufferBinding& binding = dataBindings[i];

            if( binding.buffer == nullptr || binding.slotIndex == Rio::kInvalidSlotIndex )
            {
                RollbackSubmission(core, rioEvent, owner);
                return false;
            }

            if( data[i].BufferId == RIO_INVALID_BUFFERID || data[i].Length == 0 )
            {
                binding.buffer->FreeSlot(binding.slotIndex);
                RollbackSubmission(core, rioEvent, owner);
                return false;
            }

            if( !rioEvent->BindBufferSlot(binding.buffer, binding.slotIndex) )
            {
                binding.buffer->FreeSlot(binding.slotIndex);
                RollbackSubmission(core, rioEvent, owner);
                return false;
            }
        }
    }
    else
    {
        for( ULONG i = 0; i < dataBufferCount; ++i )
        {
            if( data[i].BufferId == RIO_INVALID_BUFFERID || data[i].Length == 0 )
            {
                RollbackSubmission(core, rioEvent, owner);
                return false;
            }
        }
    }

    const bool submitted = core.TrySubmit(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOReceiveEx(
                requestQueue,
                const_cast<PRIO_BUF>(data),
                dataBufferCount,
                const_cast<PRIO_BUF>(localAddress),
                const_cast<PRIO_BUF>(remoteAddress),
                const_cast<PRIO_BUF>(control),
                nullptr,
                flags,
                reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        RollbackSubmission(core, rioEvent, owner);
        return false;
    }

    return true;
}