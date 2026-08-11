
//***************************************************************************
// RioSend.cpp : implementation of the CRioSend class.
//
//***************************************************************************

#include "pch.h"
#include "RioSend.h"

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
// @return 성공 시 true
// @note
//      성공적으로 Submission된 이후 slot의 소유권은
//      CRioEvent로 이전됩니다.
//      호출자는 FreeSlot()을 직접 호출해서는 안 됩니다.
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
    if( requestQueue == RIO_INVALID_RQ ||
        rioEvent == nullptr ||
        owner == nullptr ||
        bufferOwner == nullptr )
    {
        return false;
    }

    if( buffer.BufferId == RIO_INVALID_BUFFERID ||
        buffer.Length == 0 )
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
    rioEvent->Initialize(
        Rio::EventType::Send,
        ownerRef);

    //***********************************************************************
    // Buffer-slot ownership binding
    //
    // 성공적인 RIO submission 이후 completion이 처리될 때까지
    // CRioEvent가 해당 slot 정보를 유지합니다.
    //***********************************************************************
    if( !rioEvent->BindBufferSlot(
        bufferOwner,
        slotIndex) )
    {
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
        // 아직 RIO completion이 발생하지 않았으므로
        // Event에 등록했던 Buffer binding을 제거합니다.
        //*******************************************************************
        rioEvent->ClearBufferBindings();

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
// @param core             RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue     I/O 요청을 제출할 RIO Request Queue
// @param data              송신 데이터 RIO_BUF 배열
// @param dataBufferCount   송신 데이터 RIO_BUF 개수
// @param dataBindings      각 data RIO_BUF에 대응하는 BufferBinding 배열
// @param localAddress      로컬 주소 정보 버퍼
// @param remoteAddress     원격 주소 정보 버퍼
// @param control           제어 메시지 버퍼
// @param rioEvent          완료 처리 시 식별자로 사용될 RIO 이벤트
// @param owner             I/O lifetime을 관리하는 CRioObject
// @param flags             RIO 송신 옵션 플래그
// @return 성공 시 true
// @note
//      dataBindings[i]는 data[i]와 1:1 대응합니다.
//      성공적으로 Submission된 모든 slot의 소유권은
//      CRioEvent로 이전됩니다.
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
    if( requestQueue == RIO_INVALID_RQ ||
        data == nullptr ||
        dataBufferCount == 0 ||
        dataBindings == nullptr )
    {
        return false;
    }

    if( rioEvent == nullptr ||
        owner == nullptr )
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
    rioEvent->Initialize(
        Rio::EventType::Send,
        ownerRef);

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

        if( binding.buffer == nullptr ||
            binding.slotIndex == Rio::kInvalidSlotIndex )
        {
            rioEvent->ClearBufferBindings();

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }

        if( data[i].BufferId == RIO_INVALID_BUFFERID ||
            data[i].Length == 0 )
        {
            rioEvent->ClearBufferBindings();

            (void)rioEvent->TakeOwner();

            owner->DecrementIoCount();

            return false;
        }

        if( !rioEvent->BindBufferSlot(
            binding.buffer,
            binding.slotIndex) )
        {
            rioEvent->ClearBufferBindings();

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
        // RIO completion은 발생하지 않으므로
        // 모든 Buffer binding을 즉시 제거합니다.
        //*******************************************************************
        rioEvent->ClearBufferBindings();

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