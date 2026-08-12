
//***************************************************************************
// RioReceive.cpp : implementation of the CRioReceive class.
//
//***************************************************************************

#include "pch.h"
#include "RioReceive.h"

namespace
{
    //***************************************************************************
    // @brief CRioEvent가 이미 보유한(=BindBufferSlot에 성공해 기록된) Buffer slot
    //        ownership을 모두 반환합니다.
    // @param rioEvent  버퍼 바인딩 롤백을 수행할 CRioEvent 객체 포인터
    // @note
    //      이 함수는 RIO submission 실패 시에만 호출됩니다.
    //      정상 completion에서는 CRioCore가 동일한 작업을 수행합니다.
    //      FreeSlot()을 먼저 수행하고 ClearBufferBindings()를 호출해야 합니다.
    //      ClearBufferBindings()만 호출하면 실제 slot ownership이 유실됩니다.
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
}

//***************************************************************************
// @brief 단일 RIO_BUF 기반 RIOReceive
// @param core          RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue  I/O 수신 요청을 제출할 RIO Request Queue 핸들
// @param buffer        수신 데이터를 전달받을 RIO_BUF
// @param bufferOwner   buffer가 속한 CRioBuffer
// @param slotIndex     AllocSlot()으로 확보한 slot index
// @param rioEvent      Completion 발생 시 식별자로 사용될 CRioEvent
// @param owner         I/O lifetime을 관리하는 CRioObject
// @param flags         RIO 수신 옵션 플래그
// @return 성공 시 true
// @note
//      성공적으로 Submission된 이후 slot ownership은 CRioEvent로 이전됩니다.
//      호출자는 FreeSlot()을 직접 호출해서는 안 됩니다.
//      실패 시에는 이 함수가 (bufferOwner, slotIndex)에 대한 FreeSlot()까지
//      책임지고 수행합니다. 호출자가 다시 FreeSlot()을 호출하면 double-free입니다.
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
    rioEvent->Initialize(Rio::EventType::Receive, ownerRef);

    //***********************************************************************
    // Buffer-slot ownership binding
    //***********************************************************************
    if( !rioEvent->BindBufferSlot(bufferOwner, slotIndex) )
    {
        //*******************************************************************
        // BindBufferSlot() 자체가 실패했으므로 event에는 아직 이 slot이
        // 기록되지 않았습니다. 여기서 직접 FreeSlot()해야 leak이 발생하지
        // 않습니다.
        //*******************************************************************
        bufferOwner->FreeSlot(slotIndex);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // RIOReceive Submission
    //***********************************************************************
    const bool submitted = core.SubmitIo(
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
        //*******************************************************************
        // Submission 실패 Rollback
        //
        // RIO completion은 발생하지 않으므로
        // Buffer slot을 먼저 반환한 후 Event/Owner를 rollback합니다.
        //*******************************************************************
        RollbackBufferBindings(rioEvent);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // Submission 성공
    //
    // 이후 slot ownership은 CRioEvent가 보유합니다.
    // 호출자는 FreeSlot()을 호출해서는 안 됩니다.
    //***********************************************************************
    return true;
}

//***************************************************************************
// @brief 확장 RIOReceiveEx
// @param core             RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue     I/O 수신 요청을 제출할 RIO Request Queue 핸들
// @param data             수신 데이터를 전달받을 RIO_BUF 포인터 (nullptr 가능)
// @param dataBufferCount  data 버퍼 개수 (0 또는 1)
// @param dataBinding      data 버퍼의 CRioBuffer 및 slotIndex 바인딩 정보 포인터
// @param localAddress     로컬 주소 정보를 저장할 RIO_BUF 포인터 (선택적)
// @param remoteAddress    원격 주소 정보를 저장할 RIO_BUF 포인터 (선택적)
// @param control          제어 정보를 저장할 RIO_BUF 포인터 (선택적)
// @param rioEvent         Completion 발생 시 식별자로 사용될 CRioEvent
// @param owner            I/O lifetime을 관리하는 CRioObject
// @param flags            RIO 수신 옵션 플래그
// @return 성공 시 true, 실패 시 false
// @note
//      Windows RIOReceiveEx의 pData는 DataBufferCount가 0 또는 1이어야
//      하므로 현재 abstraction에서는 dataBinding 하나만 관리합니다.
//      localAddress / remoteAddress / control은 선택적 auxiliary buffer이며
//      별도의 CRioBuffer slot ownership을 전달받지 않는 경우 호출자가
//      해당 buffer lifetime을 보장해야 합니다.
//      실패 시 이미 event에 바인딩된 slot은 RollbackBufferBindings()가,
//      BindBufferSlot() 자체가 실패해 아직 기록되지 못한 slot은 이 함수가
//      직접 FreeSlot()으로 회수합니다.
//***************************************************************************
bool CRioReceive::ReceiveEx(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF* data,
    ULONG dataBufferCount,
    const CRioEvent::BufferBinding* dataBinding,
    const RIO_BUF* localAddress,
    const RIO_BUF* remoteAddress,
    const RIO_BUF* control,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr )
    {
        return false;
    }

    //***********************************************************************
    // RIOReceiveEx pData/DataBufferCount validation
    //
    // pData == nullptr  -> count must be 0
    // pData != nullptr  -> count must be 1
    //***********************************************************************
    if( data == nullptr )
    {
        if( dataBufferCount != 0 )
        {
            return false;
        }

        if( dataBinding != nullptr )
        {
            assert(false && "RIOReceiveEx dataBinding must be null when data is null");
            return false;
        }
    }
    else
    {
        if( dataBufferCount != 1 || dataBinding == nullptr )
        {
            return false;
        }

        if( data->BufferId == RIO_INVALID_BUFFERID || data->Length == 0 )
        {
            return false;
        }

        if( dataBinding->buffer == nullptr || dataBinding->slotIndex == Rio::kInvalidSlotIndex )
        {
            assert(false && "CRioReceive::ReceiveEx invalid data binding");
            return false;
        }
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
    rioEvent->Initialize(
        Rio::EventType::Receive,
        ownerRef);

    //***********************************************************************
    // Data Buffer-slot ownership binding
    //***********************************************************************
    if( data != nullptr )
    {
        if( !rioEvent->BindBufferSlot(dataBinding->buffer, dataBinding->slotIndex) )
        {
            //*******************************************************************
            // BindBufferSlot() 자체가 실패했으므로 event에는 아직 이 slot이
            // 기록되지 않았습니다. 직접 FreeSlot()해야 leak이 발생하지 않습니다.
            //*******************************************************************
            dataBinding->buffer->FreeSlot(dataBinding->slotIndex);

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }
    }

    //***********************************************************************
    // RIOReceiveEx Submission
    //***********************************************************************
    const bool submitted = core.SubmitIo(
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
        //*******************************************************************
        // Submission 실패 Rollback
        //
        // RIO completion은 발생하지 않으므로
        // Buffer slot을 먼저 반환한 후 Event/Owner를 rollback합니다.
        //*******************************************************************
        RollbackBufferBindings(rioEvent);

        (void)rioEvent->TakeOwner();

        owner->DecrementIoCount();

        return false;
    }

    //***********************************************************************
    // Submission 성공
    //
    // data slot ownership은 CRioEvent가 보유합니다.
    //***********************************************************************
    return true;
}