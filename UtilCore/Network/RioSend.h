
//***************************************************************************
// RioSend.h : interface for the CRioSend class.
//
//***************************************************************************

#ifndef __RIO_SEND_H__
#define __RIO_SEND_H__

#ifndef __RIOCOMMON_H__
#include <Network/RioCommon.h>
#endif

#ifndef	__RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

#ifndef	__RIOOBJECT_H__
#include <Network/RioObject.h>
#endif

#ifndef __RIOCORE_H__
#include <Network/RioCore.h>
#endif

class CRioCore;
class CRioEvent;
class CRioObject;

//***************************************************************************
// @brief RIO 송신 Submission을 담당하는 정적 유틸리티 클래스
//
// @details
//      - RIO(Registered I/O) API를 사용하여 네트워크 데이터 송신 요청을 커널에 제출합니다.
//      - 모든 메서드는 static noexcept로 구성되어 인스턴스화 없이 사용됩니다.
//      - I/O 요청 등록 성공 시 CRioObject의 I/O 카운트를 증가시키고
//        CRioEvent에 shared_ptr 소유권을 설정하여 완료 시점까지 객체 수명을 유지합니다.
//      - 커널 Submission 실패 시 I/O 카운트 차감 및 소유권 해제(Rollback)를 원자적으로 수행합니다.
//***************************************************************************
class CRioSend final
{
public:

    CRioSend() = delete;
    ~CRioSend() = delete;

    CRioSend(const CRioSend&) = delete;
    CRioSend& operator=(const CRioSend&) = delete;

    static bool Send(
        CRioCore& core,
        RIO_RQ requestQueue,
        const RIO_BUF& buffer,
        CRioBuffer* bufferOwner,
        uint32_t slotIndex,
        CRioEvent* rioEvent,
        CRioObject* owner,
        DWORD flags = 0) noexcept;

    static bool SendEx(
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
        DWORD flags = 0) noexcept;
};

#endif // __RIO_SEND_H__