
//***************************************************************************
// RioSubmissionHelper.h : interface for the CRioSubmissionHelper class.
//
//***************************************************************************

#ifndef __RIOSUBMISSIONHELPER_H__
#define __RIOSUBMISSIONHELPER_H__

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

#ifndef __RIOBUFFER_H__
#include <Network/Rio/RioBuffer.h>
#endif

#include <cassert>
#include <functional>

class CRioCore;
class CRioEvent;
class CRioObject;
class CRioBuffer;

//***************************************************************************
// @class CRioSubmissionHelper
// @brief RIO Submission 공통 lifecycle 및 rollback을 담당하는 헬퍼 클래스
//
// @details
//      - CRioSend / CRioReceive에서 공통으로 사용하는 Submission 로직을 통합합니다.
//      - Owner shared_ptr 생성
//      - CRioObject I/O Count 증가
//      - CRioEvent Owner 연결
//      - Buffer Slot Binding
//      - RIO Submit
//      - Submission 실패 시 완전한 Rollback
//      의 책임을 하나의 코드 경로로 통합합니다.
//
//      Submission 성공 후에는 CRioEvent가 Owner shared_ptr을 보유하므로
//      RIO completion이 처리될 때까지 CRioObject의 lifetime이 유지됩니다.
//
//      Submission 실패 시 다음 순서로 rollback합니다.
//
//          Buffer Slot 반환
//          -> Event Owner 반환
//          -> CRioObject IoCount 감소
//          -> EventPool 반환
//
//      중요:
//          CRioSession에서 별도로 IncrementIoCountNoLock()을 호출하면 안 됩니다.
//          IoCount의 증가/감소 책임은 이 Helper가 단독으로 담당합니다.
//***************************************************************************
class CRioSubmissionHelper final
{
public:
    CRioSubmissionHelper() = delete;
    ~CRioSubmissionHelper() = delete;

    CRioSubmissionHelper(const CRioSubmissionHelper&) = delete;
    CRioSubmissionHelper& operator=(const CRioSubmissionHelper&) = delete;

public:

    //***************************************************************************
    // @brief 단일 RIO Buffer Submission을 수행합니다.
    //***************************************************************************
    static bool SubmitSingle(
        CRioCore& core,
        Rio::EventType eventType,
        RIO_RQ requestQueue,
        const RIO_BUF& buffer,
        CRioBuffer* bufferOwner,
        uint32_t slotIndex,
        CRioEvent* rioEvent,
        CRioObject* owner,
        const std::function<bool()>& submit) noexcept;

    //***************************************************************************
    // @brief 다중 RIO Buffer Submission을 수행합니다.
    //***************************************************************************
    static bool SubmitMulti(
        CRioCore& core,
        Rio::EventType eventType,
        RIO_RQ requestQueue,
        const RIO_BUF* data,
        ULONG dataBufferCount,
        const CRioEvent::BufferBinding* dataBindings,
        CRioEvent* rioEvent,
        CRioObject* owner,
        const std::function<bool()>& submit) noexcept;

private:

    //***************************************************************************
    // @brief CRioEvent에 바인딩된 슬롯 자원을 즉시 반환합니다.
    //***************************************************************************
    static void RollbackBindings(CRioEvent* rioEvent) noexcept;

    //***************************************************************************
    // @brief Submission 실패시 완벽한 역순 롤백 트랜잭션을 수행합니다.
    //***************************************************************************
    static void RollbackSubmission(CRioCore& core, CRioEvent* rioEvent, CRioObject* owner) noexcept;

private:
    static CRioObjectRef AcquireOwner(CRioObject* owner) noexcept;
};

#endif // ndef __RIOSUBMISSIONHELPER_H__