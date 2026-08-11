
//***************************************************************************
// RioReceive.h : interface for the CRioReceive class.
//
//***************************************************************************

#ifndef __RIO_RECEIVE_H__
#define __RIO_RECEIVE_H__

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
// @brief RIO 수신 Submission을 담당하는 유틸리티 클래스
//
// @details
//      - Windows RIO(Registered I/O) API를 사용하여 네트워크 데이터 수신 요청을 커널에 제출합니다.
//      - 모든 메서드가 static noexcept로 구성된 정적 유틸리티 클래스입니다.
//      - 수신 요청 등록 성공 시 CRioObject의 I/O 카운터를 증가시키고,
//        CRioEvent에 shared_ptr 소유권을 설정하여 비동기 수신 완료 시점까지 객체 수명을 보장합니다.
//      - 커널 제출(SubmitIo) 실패 시 I/O 카운트 감소 및 이벤트 소유권 해제(Rollback)를 원자적으로 수행합니다.
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
            CRioEvent* rioEvent,
            CRioObject* owner,
            DWORD flags) noexcept;

    static bool ReceiveEx(
            CRioCore& core,
            RIO_RQ requestQueue,
            const RIO_BUF* data,
            ULONG dataBufferCount,
            const RIO_BUF* localAddress,
            const RIO_BUF* remoteAddress,
            const RIO_BUF* control,
            CRioEvent* rioEvent,
            CRioObject* owner,
            DWORD flags) noexcept;
};

#endif // __RIO_RECEIVE_H__