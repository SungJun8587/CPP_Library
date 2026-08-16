
//***************************************************************************
// RioSend.cpp : implementation of the CRioSend class.
//
//***************************************************************************

#include "pch.h"
#include "RioSend.h"

//***************************************************************************
// @brief 단일 버퍼 기반 RIO Send 요청을 등록합니다.
// @param core RIO 코어 객체
// @param requestQueue RIO 요청 큐 핸들
// @param buffer 전송 대상 RIO_BUF 구조체
// @param bufferOwner 버퍼 소유자 객체
// @param slotIndex 버퍼 슬롯 인덱스
// @param rioEvent 작업에 사용할 RIO 이벤트 객체
// @param owner 비동기 완료 결과를 디스패치할 RIO 객체
// @param flags RIOSend 옵션 플래그
// @return bool 제출 성공 여부
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
    return CRioSubmissionHelper::SubmitSingle(
        core,
        Rio::EventType::Send,
        requestQueue,
        buffer,
        bufferOwner,
        slotIndex,
        rioEvent,
        owner,
        [&]() noexcept -> bool
        {
            return core.TrySubmit(
                [&]() noexcept -> bool
                {
                    return core.GetRioTable().RIOSend(
                        requestQueue,
                        const_cast<PRIO_BUF>(&buffer),
                        1,
                        flags,
                        reinterpret_cast<PVOID>(rioEvent)) != FALSE;
                });
        });
}

//***************************************************************************
// @brief 다중 버퍼 및 확장 옵션 기반의 RIOSendEx 요청을 등록합니다.
// @param core RIO 코어 객체
// @param requestQueue RIO 요청 큐 핸들
// @param data 전송할 RIO_BUF 배열
// @param dataBufferCount 데이터 버퍼 개수
// @param dataBindings 각 데이터 버퍼에 대한 바인딩 정보 배열 (nullptr 허용: 사전 등록 버퍼 등)
// @param localAddress 로컬 주소 RIO_BUF (선택 사항)
// @param remoteAddress 원격 주소 RIO_BUF (선택 사항)
// @param control 제어 데이터 RIO_BUF (선택 사항)
// @param rioEvent 작업에 사용할 RIO 이벤트 객체
// @param owner 비동기 완료 결과를 디스패치할 RIO 객체
// @param flags RIOSendEx 옵션 플래그
// @return bool 제출 성공 여부
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
    return CRioSubmissionHelper::SubmitMulti(
        core,
        Rio::EventType::Send,
        requestQueue,
        data,
        dataBufferCount,
        dataBindings,
        rioEvent,
        owner,
        [&]() noexcept -> bool
        {
            return core.TrySubmit(
                [&]() noexcept -> bool
                {
                    const BOOL result = core.GetRioTable().RIOSendEx(
                        requestQueue,
                        const_cast<PRIO_BUF>(data),
                        dataBufferCount,
                        const_cast<PRIO_BUF>(localAddress),
                        const_cast<PRIO_BUF>(remoteAddress),
                        const_cast<PRIO_BUF>(control),
                        nullptr,
                        flags,
                        reinterpret_cast<PVOID>(rioEvent)) != FALSE;

                    if( result == FALSE )
                    {
                        DWORD err = WSAGetLastError();
                        std::cout << "[Error] RIOSendEx failed with WSA Error Code: " << err << "\n";
                        return false;
                    }
                });
        });
}