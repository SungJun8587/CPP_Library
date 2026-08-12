
//***************************************************************************
// RioSend.cpp : implementation of the CRioSend class.
//
//***************************************************************************

#include "pch.h"
#include "RioSend.h"

namespace
{
    //***************************************************************************
    // @brief CRioEvent가 이미 보유한(=BindBufferSlot에 성공해 기록된) Buffer slot
    //        ownership을 모두 반환합니다.
    // @param rioEvent  버퍼 바인딩 롤백을 수행할 CRioEvent 객체 포인터
    // @note
    //      RIO submission 실패 시에만 호출됩니다. 정상 completion 경로에서는
    //      CRioCore::ProcessRioResult()가 동일한 작업을 수행합니다.
    //      FreeSlot()을 먼저 수행한 뒤 ClearBufferBindings()를 호출해야 합니다.
    //      ClearBufferBindings()만 호출하면 실제 slot ownership이 유실(leak)됩니다.
    //      이 함수는 CRioEvent에 이미 "기록된" binding만 처리합니다.
    //      BindBufferSlot() 호출 자체가 실패해 아직 event에 기록되지 못한
    //      slot은 호출자가 별도로 FreeSlot()해야 합니다.
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
                assert(false && "CRioSend rollback contains invalid BufferBinding");
                continue;
            }

            const bool freed = binding.buffer->FreeSlot(binding.slotIndex);

            if( !freed )
            {
                assert(false && "CRioSend rollback FreeSlot failed");
            }
        }

        rioEvent->ClearBufferBindings();
    }
}

//***************************************************************************
// @brief 단일 RIO_BUF 기반 RIOSend
// @param core          RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue  I/O 요청을 제출할 RIO Request Queue
// @param buffer        송신할 RIO Buffer descriptor
// @param bufferOwner   buffer가 속한 CRioBuffer
// @param slotIndex     AllocSlot()으로 확보한 slot index
// @param rioEvent      완료 처리 시 식별자로 사용될 RIO 이벤트
// @param owner         I/O lifetime을 관리하는 CRioObject
// @param flags         RIO 송신 옵션 플래그
// @return 성공 시 true, 실패 시 false
// @note
//      성공적으로 Submission된 이후 slot의 소유권은
//      CRioEvent로 이전됩니다.
//      호출자는 FreeSlot()을 직접 호출해서는 안 됩니다.
//      실패 시에는 이 함수가 (bufferOwner, slotIndex)에 대한 FreeSlot()까지
//      책임지고 수행합니다. 호출자가 다시 FreeSlot()을 호출하면 double-free입니다.
//***************************************************************************
bool CRioSend::Send(
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
        assert(false && "CRioSend::Send invalid slot index");
        return false;
    }

    //***********************************************************************
    // Owner lifetime 확보
    //***********************************************************************
    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->shared_from_this();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr )
    {
        return false;
    }

    //***********************************************************************
    // CRioObject outstanding I/O count 확보
    //***********************************************************************
    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    //***********************************************************************
    // CRioEvent 초기화
    //***********************************************************************
    rioEvent->Initialize(Rio::EventType::Send, ownerRef);

    //***********************************************************************
    // Buffer-slot ownership binding
    //
    // 성공적인 RIO submission 이후 completion이 처리될 때까지
    // CRioEvent가 해당 slot 정보를 유지합니다.
    //***********************************************************************
    if( !rioEvent->BindBufferSlot(bufferOwner, slotIndex) )
    {
        //*******************************************************************
        // BindBufferSlot() 자체가 실패했으므로 event에는 아직 이 slot이
        // 기록되지 않았습니다. RollbackBufferBindings()로는 회수되지 않으므로
        // 여기서 직접 FreeSlot()해야 leak이 발생하지 않습니다.
        //*******************************************************************
        bufferOwner->FreeSlot(slotIndex);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // RIO Submission
    //***********************************************************************
    const bool submitted = core.SubmitIo(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOSend(
                requestQueue,
                const_cast<PRIO_BUF>(&buffer),
                1,
                flags,
                reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        //*******************************************************************
        // Submission 실패 Rollback
        //
        // 아직 RIO completion이 발생하지 않았으므로 Event에 기록된
        // Buffer binding을 FreeSlot()까지 포함해 완전히 되돌립니다.
        //*******************************************************************
        RollbackBufferBindings(rioEvent);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // Submission 성공
    //
    // 이후 slot의 소유권은 CRioEvent가 보유합니다.
    // 호출자는 FreeSlot()을 호출해서는 안 됩니다.
    //***********************************************************************
    return true;
}

//***************************************************************************
// @brief Scatter-Gather RIOSendEx
// @param core              RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue      I/O 요청을 제출할 RIO Request Queue
// @param data              송신 데이터 RIO_BUF 배열
// @param dataBufferCount   송신 데이터 RIO_BUF 개수
// @param dataBindings      각 data RIO_BUF에 대응하는 BufferBinding 배열
// @param localAddress      로컬 주소 정보 버퍼 (선택적)
// @param remoteAddress     원격 주소 정보 버퍼 (선택적)
// @param control           제어 메시지 버퍼 (선택적)
// @param rioEvent          완료 처리 시 식별자로 사용될 RIO 이벤트
// @param owner             I/O lifetime을 관리하는 CRioObject
// @param flags             RIO 송신 옵션 플래그
// @return 성공 시 true, 실패 시 false
// @note
//      dataBindings[i]는 data[i]와 1:1 대응합니다.
//      성공적으로 Submission된 모든 slot의 소유권은
//      CRioEvent로 이전됩니다.
//      실패 시 이미 event에 바인딩된 slot(0..i-1)은 RollbackBufferBindings()가,
//      이번 반복에서 실패한 slot(i)은 각 실패 분기에서 직접 FreeSlot()이
//      담당합니다. 단, binding 자체가 애초에 무효(sentinel)인 경우는
//      해제할 실체가 없으므로 FreeSlot()을 호출하지 않습니다.
//***************************************************************************
bool CRioSend::SendEx(
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
    if( requestQueue == RIO_INVALID_RQ || data == nullptr || dataBufferCount == 0 || dataBindings == nullptr )
    {
        return false;
    }

    if( rioEvent == nullptr || owner == nullptr )
    {
        return false;
    }

    //***********************************************************************
    // Owner lifetime 확보
    //***********************************************************************
    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->shared_from_this();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr )
    {
        return false;
    }

    //***********************************************************************
    // CRioObject outstanding I/O count 확보
    //***********************************************************************
    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    //***********************************************************************
    // CRioEvent 초기화
    //***********************************************************************
    rioEvent->Initialize(Rio::EventType::Send, ownerRef);

    //***********************************************************************
    // Scatter-Gather Buffer-slot binding
    //
    // data[i] <-> dataBindings[i]
    //
    // 모든 binding이 성공해야 Submission을 진행합니다.
    //***********************************************************************
    for( ULONG i = 0; i < dataBufferCount; ++i )
    {
        const CRioEvent::BufferBinding& binding = dataBindings[i];

        if( binding.buffer == nullptr || binding.slotIndex == Rio::kInvalidSlotIndex )
        {
            //*******************************************************************
            // binding 자체가 무효(sentinel)이므로 해제할 실체가 없습니다.
            // 0..i-1까지 이미 event에 기록된 slot만 되돌립니다.
            //*******************************************************************
            RollbackBufferBindings(rioEvent);

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }

        if( data[i].BufferId == RIO_INVALID_BUFFERID || data[i].Length == 0 )
        {
            //*******************************************************************
            // binding[i]는 유효한 slot이지만 아직 event에 기록되지 않았으므로
            // RollbackBufferBindings()로는 회수되지 않습니다. 직접 FreeSlot().
            //*******************************************************************
            binding.buffer->FreeSlot(binding.slotIndex);

            RollbackBufferBindings(rioEvent);

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }

        if( !rioEvent->BindBufferSlot(binding.buffer, binding.slotIndex) )
        {
            //*******************************************************************
            // BindBufferSlot() 자체가 실패해 event에 기록되지 못했으므로
            // 마찬가지로 직접 FreeSlot()해야 합니다.
            //*******************************************************************
            binding.buffer->FreeSlot(binding.slotIndex);

            RollbackBufferBindings(rioEvent);

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }
    }

    //***********************************************************************
    // RIOSendEx Submission
    //***********************************************************************
    const bool submitted = core.SubmitIo(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOSendEx(
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
        //*******************************************************************
        // Submission 실패 Rollback
        //
        // 이 시점에는 모든 slot이 이미 event에 성공적으로 기록되어 있으므로
        // RollbackBufferBindings() 하나로 전부 회수됩니다.
        //*******************************************************************
        RollbackBufferBindings(rioEvent);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // Submission 성공
    //
    // 모든 data slot의 소유권은 CRioEvent가 보유합니다.
    // 호출자는 FreeSlot()을 호출해서는 안 됩니다.
    //***********************************************************************
    return true;
}