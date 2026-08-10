
//***************************************************************************
// RioSend.cpp : implementation of the CRioSend class.
//
//***************************************************************************

#include "pch.h"
#include "RioSend.h"

//***************************************************************************
// @brief 단일 RIO_BUF 기반 RIOSend
//
// @param core RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue I/O 요청을 제출할 RIO Request Queue 핸들
// @param buffer 송신할 데이터가 담긴 RIO 버퍼 정보
// @param rioEvent 완료 처리 시 식별자로 사용될 RIO 이벤트 객체
// @param owner I/O 수명 주기를 관리하는 CRioObject 소유자 포인터
// @param flags RIO 송신 옵션 플래그 (기본값: 0)
//
// @return 성공 시 true, 검증 실패 또는 제출 실패 시 false
//
// @details
//      1. 파라미터 및 RIO_BUF(BufferId, Length)의 유효성을 검사합니다.
//      2. owner->shared_from_this()를 통해 스마트 포인터를 안전하게 획득합니다.
//      3. IncrementIoCount()로 Outstanding I/O 카운터를 증가시킵니다.
//      4. rioEvent->SetOwnerShared()를 호출하여 비동기 완료 전까지 소유자 객체가 파괴되지 않도록 바인딩합니다.
//      5. core.SubmitIo()를 통해 Windows RIO API(RIOSend)를 호출합니다.
//      6. 제출 실패 시 TakeOwner() 및 DecrementIoCount()를 호출하여 획득한 소유권과 카운터를 롤백(Rollback)합니다.
//***************************************************************************
bool CRioSend::Send(
    CRioCore& core,
    RIO_RQ requestQueue,
    const RIO_BUF& buffer,
    CRioEvent* rioEvent,
    CRioObject* owner,
    DWORD flags) noexcept
{
    if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr )
    {
        return false;
    }

    if( buffer.BufferId == RIO_INVALID_BUFFERID || buffer.Length == 0 )
    {
        return false;
    }

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

    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    try
    {
        rioEvent->SetOwnerShared(ownerRef);
    }
    catch( ... )
    {
        owner->DecrementIoCount();
        return false;
    }

    const bool submitted = core.SubmitIo(
        [&]() noexcept -> bool
        {
            return core.GetRioTable().RIOSend(requestQueue, const_cast<PRIO_BUF>(&buffer), 1, flags, reinterpret_cast<PVOID>(rioEvent)) != FALSE;
        });

    if( !submitted )
    {
        rioEvent->TakeOwner();
        owner->DecrementIoCount();
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 확장 RIOSendEx
//
// @param core RIO 핵심 처리를 담당하는 CRioCore 참조
// @param requestQueue I/O 요청을 제출할 RIO Request Queue 핸들
// @param data 송신할 RIO_BUF 버퍼 배열 포인터 (Scatter-Gather 송신 지원)
// @param dataBufferCount 송신 버퍼 배열의 개수
// @param localAddress 로컬 주소 정보 버퍼 포인터 (선택 사항)
// @param remoteAddress 원격 주소 정보 버퍼 포인터 (선택 사항)
// @param control 제어 메시지 버퍼 포인터 (선택 사항)
// @param rioEvent 완료 처리 시 식별자로 사용될 RIO 이벤트 객체
// @param owner I/O 수명 주기를 관리하는 CRioObject 소유자 포인터
// @param flags RIO 송신 옵션 플래그 (기본값: 0)
//
// @return 성공 시 true, 검증 실패 또는 제출 실패 시 false
//
// @details
//      - Scatter-Gather I/O 및 주소/제어 버퍼 지정을 지원하는 RIOSendEx 커널 API를 호출합니다.
//      - 다단계 롤백 처리:
//        * shared_from_this() 실패 시 -> 즉시 false 반환
//        * IncrementIoCount() 실패 시 -> 즉시 false 반환
//        * SetOwnerShared() 예외 발생 시 -> DecrementIoCount() 수행 후 false 반환
//        * SubmitIo() 커널 제출 실패 시 -> rioEvent->TakeOwner()로 참조 해제 및 DecrementIoCount() 수행 후 false 반환
//***************************************************************************
bool CRioSend::SendEx(
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

    if( !owner->IncrementIoCount() )
    {
        return false;
    }

    try
    {
        rioEvent->SetOwnerShared(ownerRef);
    }
    catch( ... )
    {
        owner->DecrementIoCount();
        return false;
    }

    const bool submitted = core.SubmitIo([&]() noexcept -> bool { return core.GetRioTable().RIOSendEx(requestQueue, const_cast<PRIO_BUF>(data), dataBufferCount, const_cast<PRIO_BUF>(localAddress), const_cast<PRIO_BUF>(remoteAddress), const_cast<PRIO_BUF>(control), nullptr, flags, reinterpret_cast<PVOID>(rioEvent)) != FALSE; });

    if( !submitted )
    {
        rioEvent->TakeOwner();
        owner->DecrementIoCount();
        return false;
    }

    return true;
}