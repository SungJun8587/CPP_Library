
//***************************************************************************
// RioReceive.h : interface for the CRioReceive class.
//
//***************************************************************************

#ifndef UC_RIORECEIVE_H
#define UC_RIORECEIVE_H

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioEvent.h>
#include <Network/RIO/RioObject.h>
#include <Network/RIO/RioCore.h>
#include <Network/RIO/RioSubmissionHelper.h>

class CRioCore;
class CRioEvent;
class CRioObject;
class CRioBuffer;

//***************************************************************************
// @brief RIO 수신 Submission을 담당하는 정적 유틸리티 클래스
//
// @details
//      - Windows RIO(Registered I/O) API를 사용하여 네트워크 데이터 수신 요청을
//        커널에 제출합니다.
//      - 모든 메서드는 static noexcept로 구성되어 인스턴스화 없이 사용됩니다.
//      - 실제 Submission lifecycle은 CRioSubmissionHelper에 위임합니다.
//      - Owner shared_ptr, IoCount, Buffer Binding 및 Rollback은
//        CRioSubmissionHelper에서 일관되게 관리됩니다.
//***************************************************************************
class CRioReceive final
{
public:

    CRioReceive() = delete;
    ~CRioReceive() = delete;

    CRioReceive(const CRioReceive&) = delete;
    CRioReceive& operator=(const CRioReceive&) = delete;

    static bool Receive(
        CRioCore& core,
        RIO_RQ requestQueue,
        const RIO_BUF& buffer,
        CRioBuffer* bufferOwner,
        uint32 slotIndex,
        CRioEvent* rioEvent,
        CRioObject* owner,
        DWORD flags = 0) noexcept;

    static bool ReceiveEx(
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

#endif // ndef UC_RIORECEIVE_H