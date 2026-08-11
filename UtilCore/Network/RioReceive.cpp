
//***************************************************************************
// RioReceive.cpp : implementation of the CRioReceive class.
//
//***************************************************************************

#include "pch.h"
#include "RioReceive.h"

//***************************************************************************
// @brief 단일 RIO_BUF 기반 RIOReceive
// @param core RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue I/O 수신 요청을 제출할 RIO Request Queue 핸들
// @param buffer 수신 데이터를 전달받을 메모리가 등록된 RIO_BUF 구조체
// @param rioEvent Completion 발생 시 식별자로 전달될 CRioEvent 포인터
// @param owner I/O 라이프사이클을 소유하는 CRioObject 포인터
// @param flags RIO 수신 옵션 플래그 (기본값: 0)
// @return 성공 시 true, 검증 실패 또는 RIO 커널 제출 실패 시 false
// @details
//      1. 파라미터 및 RIO_BUF의 유효성을 검사합니다.
//      2. owner->shared_from_this()로 Owner lifetime을 확보합니다.
//      3. IncrementIoCount()를 통해 Outstanding I/O 카운트를 증가시킵니다.
//      4. CRioEvent::Initialize()를 통해 Receive 타입 및 Owner를 바인딩합니다.
//      5. core.SubmitIo()를 통해 RIOReceive()를 제출합니다.
//      6. 제출 실패 시 TakeOwner() 및 DecrementIoCount()로 rollback합니다.
//***************************************************************************
bool CRioReceive::Receive(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF& buffer,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr ) return false;
    if( buffer.BufferId == RIO_INVALID_BUFFERID || buffer.Length == 0 ) return false;

    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->shared_from_this();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr ) return false;
    if( !owner->IncrementIoCount() ) return false;

    rioEvent->Initialize(Rio::EventType::Receive, ownerRef);

    const bool submitted = core.SubmitIo(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOReceive(requestQueue, const_cast<PRIO_BUF>(&buffer), 1, flags, reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        (void)rioEvent->TakeOwner();
        owner->DecrementIoCount();
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 확장 RIOReceiveEx
// @param core RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue I/O 수신 요청을 제출할 RIO Request Queue 핸들
// @param data 수신 버퍼 배열 포인터
// @param dataBufferCount 수신 버퍼 배열의 개수
// @param localAddress 로컬 주소 정보 버퍼 포인터
// @param remoteAddress 원격 주소 정보 버퍼 포인터
// @param control 제어 메시지 정보 버퍼 포인터
// @param rioEvent Completion 발생 시 식별자로 전달될 CRioEvent 포인터
// @param owner I/O 라이프사이클을 소유하는 CRioObject 포인터
// @param flags RIO 수신 옵션 플래그 (기본값: 0)
// @return 성공 시 true, 검증 실패 또는 RIO 커널 제출 실패 시 false
// @details
//      Scatter-Gather 수신 및 주소/제어 버퍼 지정을 지원합니다.
//      shared_from_this() -> IncrementIoCount() -> Initialize() -> SubmitIo()
//      순서로 ownership을 확보하며 submission 실패 시 rollback합니다.
//***************************************************************************
bool CRioReceive::ReceiveEx(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF* data,
    ULONG dataBufferCount,
    const RIO_BUF* localAddress,
    const RIO_BUF* remoteAddress,
    const RIO_BUF* control,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || data == nullptr || dataBufferCount == 0 ) return false;
    if( rioEvent == nullptr || owner == nullptr ) return false;

    CRioObjectRef ownerRef;

    try
    {
        ownerRef = owner->shared_from_this();
    }
    catch( ... )
    {
        return false;
    }

    if( ownerRef == nullptr ) return false;
    if( !owner->IncrementIoCount() ) return false;

    rioEvent->Initialize(Rio::EventType::Receive, ownerRef);

    const bool submitted = core.SubmitIo(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOReceiveEx(requestQueue, const_cast<PRIO_BUF>(data), dataBufferCount, const_cast<PRIO_BUF>(localAddress), const_cast<PRIO_BUF>(remoteAddress), const_cast<PRIO_BUF>(control), nullptr, flags, reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        (void)rioEvent->TakeOwner();
        owner->DecrementIoCount();
        return false;
    }

    return true;
}