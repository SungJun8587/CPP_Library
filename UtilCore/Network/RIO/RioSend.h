
//***************************************************************************
// RioSend.h : interface for the CRioSend class.
//
//***************************************************************************

#ifndef __RIO_SEND_H__
#define __RIO_SEND_H__

#ifndef __RIOCOMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/Rio/RioEvent.h>
#endif

#ifndef __RIOOBJECT_H__
#include <Network/Rio/RioObject.h>
#endif

#ifndef __RIOCORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __RIOSUBMISSIONHELPER_H__
#include <Network/Rio/RioSubmissionHelper.h>
#endif

class CRioCore;
class CRioEvent;
class CRioObject;
class CRioBuffer;

//***************************************************************************
// @brief RIO 송신 Submission을 담당하는 정적 유틸리티 클래스
//
// @details
//      - RIO(Registered I/O) API를 사용하여 네트워크 데이터 송신 요청을 커널에 제출합니다.
//      - 모든 메서드는 static noexcept로 구성되어 인스턴스화 없이 사용됩니다.
//      - 실제 Submission lifecycle은 CRioSubmissionHelper에 위임합니다.
//      - Owner shared_ptr, IoCount, Buffer Binding 및 Rollback은
//        CRioSubmissionHelper에서 일관되게 관리됩니다.
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

#endif // ndef __RIO_SEND_H__