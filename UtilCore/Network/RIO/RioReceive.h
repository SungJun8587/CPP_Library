
//***************************************************************************
// RioReceive.h : interface for the CRioReceive class.
//
//***************************************************************************

#ifndef __RIO_RECEIVE_H__
#define __RIO_RECEIVE_H__

#ifndef __RIOCOMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef	__RIOEVENT_H__
#include <Network/Rio/RioEvent.h>
#endif

#ifndef	__RIOOBJECT_H__
#include <Network/Rio/RioObject.h>
#endif

#ifndef __RIOCORE_H__
#include <Network/Rio/RioCore.h>
#endif

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
//      - I/O 요청 등록 성공 시 CRioObject의 I/O 카운트를 증가시키고
//        CRioEvent에 Owner shared_ptr을 연결하여 completion까지 객체 수명을 유지합니다.
//      - RIO Buffer slot을 사용하는 경우 해당 slot ownership도 CRioEvent로 이전합니다.
//      - Submission 실패 시 Buffer slot을 먼저 FreeSlot()으로 반환한 뒤
//        Event binding 및 Owner를 rollback합니다.
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
        uint32_t slotIndex,
        CRioEvent* rioEvent,
        CRioObject* owner,
        DWORD flags = 0) noexcept;

    static bool ReceiveEx(
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
        DWORD flags = 0) noexcept;
};

#endif // ndef __RIO_RECEIVE_H__