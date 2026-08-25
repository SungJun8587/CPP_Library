
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
#include <type_traits>
#include <utility>

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
//
//      [수정] SubmitSingle/SubmitMulti는 기존 std::function<bool()> 파라미터
//      대신 템플릿 Callable(F&&)을 받도록 변경했습니다. CRioSend/CRioReceive는
//      매 Send/Receive 호출마다(패킷 단위 핫패스) 다수의 변수를 캡처한 람다를
//      넘기는데, std::function으로 감싸면 캡처 크기가 SSO 한도를 넘어 힙 할당이
//      발생할 수 있었습니다. 템플릿화로 이 할당을 제거합니다(동작/계약은 동일).
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
    // @tparam F 실제 RIO 제출을 실행하는 Callable 타입 (bool() 형태, noexcept 권장)
    //***************************************************************************
    template<typename F>
    static bool SubmitSingle(
        CRioCore& core,
        Rio::EventType eventType,
        RIO_RQ requestQueue,
        const RIO_BUF& buffer,
        CRioBuffer* bufferOwner,
        uint32_t slotIndex,
        CRioEvent* rioEvent,
        CRioObject* owner,
        F&& submit) noexcept
    {
        static_assert(std::is_invocable_r_v<bool, F>, "submit must be callable as bool()");

        // 1. 필수 인자(Request Queue, 이벤트, 오너, 버퍼 오너 등)의 유효성 검사
        if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr || bufferOwner == nullptr )
        {
            return false;
        }

        // 2. RIO 버퍼 식별자(BufferId) 및 데이터 길이 유효성 검사
        if( buffer.BufferId == RIO_INVALID_BUFFERID || buffer.Length == 0 ) return false;

        // 3. 버퍼 슬롯 인덱스가 유효한 값인지 검사 (유효하지 않다면 어설션 발생 후 실패 반환)
        if( slotIndex == Rio::kInvalidSlotIndex )
        {
            assert(false && "CRioSubmissionHelper::SubmitSingle invalid slot index");
            return false;
        }

        // 4. 안전하게 소유자(Owner)의 shared_ptr 레퍼런스(`CRioObjectRef`) 획득
        CRioObjectRef ownerRef = AcquireOwner(owner);

        if( ownerRef == nullptr ) return false;

        // 5. 대상 RIO 객체의 진행 중인 I/O 카운트(IoCount) 증가 시도 (실패 시 중단)
        if( !owner->IncrementIoCount() ) return false;

        // 6. 획득한 소유자 레퍼런스를 이용해 RIO 이벤트 객체 초기화 (이벤트 타입 및 오너 바인딩)
        rioEvent->Initialize(eventType, ownerRef);

        // 7. 이벤트 객체에 버퍼 슬롯 바인딩 시도
        if( !rioEvent->BindBufferSlot(bufferOwner, slotIndex) )
        {
            // 바인딩 실패 시 앞서 할당받았던 버퍼 슬롯을 즉시 반환하고, 전체 롤백 트랜잭션 수행
            bufferOwner->FreeSlot(slotIndex);
            RollbackSubmission(core, rioEvent, owner);
            return false;
        }

        // 8. 전달받은 Callable(`submit`)을 통해 실제 RIO 제출(Send/Receive 등) 수행
        const bool submitted = submit();

        // 9. RIO 제출이 실패한 경우, 롤백 트랜잭션을 수행하여 모든 리소스 원상 복구
        if( !submitted )
        {
            RollbackSubmission(core, rioEvent, owner);
            return false;
        }

        // 10. 모든 준비 및 제출 과정 성공
        return true;
    }

    //***************************************************************************
    // @brief 다중 RIO Buffer Submission을 수행합니다.
    // @tparam F 실제 RIO 제출을 실행하는 Callable 타입 (bool() 형태, noexcept 권장)
    //***************************************************************************
    template<typename F>
    static bool SubmitMulti(
        CRioCore& core,
        Rio::EventType eventType,
        RIO_RQ requestQueue,
        const RIO_BUF* data,
        ULONG dataBufferCount,
        const CRioEvent::BufferBinding* dataBindings,
        CRioEvent* rioEvent,
        CRioObject* owner,
        F&& submit) noexcept
    {
        static_assert(std::is_invocable_r_v<bool, F>, "submit must be callable as bool()");

        // 1. 기본 필수 인자(Request Queue, 데이터 포인터, 버퍼 개수, 이벤트, 오너 등)의 유효성 검사
        if( requestQueue == RIO_INVALID_RQ || data == nullptr || dataBufferCount == 0 || rioEvent == nullptr || owner == nullptr )
        {
            return false;
        }

        // 2. 본격적인 작업 수행 전, 다중 버퍼 배열 및 바인딩 정보의 유효성 사전 검사
        if( dataBindings != nullptr )
        {
            // 바인딩 정보가 존재하는 경우: 각 버퍼의 바인딩 상태(버퍼 포인터, 슬롯 인덱스)와 RIO_BUF 유효성 검증
            for( ULONG i = 0; i < dataBufferCount; ++i )
            {
                const CRioEvent::BufferBinding& binding = dataBindings[i];

                if( binding.buffer == nullptr || binding.slotIndex == Rio::kInvalidSlotIndex )
                {
                    return false;
                }

                if( data[i].BufferId == RIO_INVALID_BUFFERID || data[i].Length == 0 )
                {
                    return false;
                }
            }
        }
        else
        {
            // 바인딩 정보가 없는 경우: RIO_BUF 배열의 식별자(BufferId) 및 데이터 길이(Length) 검증
            for( ULONG i = 0; i < dataBufferCount; ++i )
            {
                if( data[i].BufferId == RIO_INVALID_BUFFERID || data[i].Length == 0 )
                {
                    return false;
                }
            }
        }

        // 3. 안전하게 소유자(Owner)의 shared_ptr 레퍼런스(`CRioObjectRef`) 획득
        CRioObjectRef ownerRef = AcquireOwner(owner);

        if( ownerRef == nullptr ) return false;

        // 4. 대상 RIO 객체의 진행 중인 I/O 카운트(IoCount) 증가 시도 (실패 시 중단)
        if( !owner->IncrementIoCount() ) return false;

        // 5. 획득한 소유자 레퍼런스를 이용해 RIO 이벤트 객체 초기화 (이벤트 타입 및 오너 바인딩)
        rioEvent->Initialize(eventType, ownerRef);

        // 6. 버퍼 바인딩 정보가 존재할 경우, 각 슬롯을 이벤트 객체에 순차적으로 바인딩 수행
        if( dataBindings != nullptr )
        {
            for( ULONG i = 0; i < dataBufferCount; ++i )
            {
                const CRioEvent::BufferBinding& binding = dataBindings[i];

                // 바인딩 중 하나라도 실패하면 롤백 트랜잭션을 호출하여 지금까지 할당/바인딩된 자원 원상 복구
                if( !rioEvent->BindBufferSlot(binding.buffer, binding.slotIndex) )
                {
                    RollbackSubmission(core, rioEvent, owner);
                    return false;
                }
            }
        }

        // 7. 전달받은 Callable(`submit`)을 통해 실제 RIO 다중 제출(Scatter/Gather 등) 수행
        const bool submitted = submit();

        // 8. RIO 제출이 실패한 경우, 롤백 트랜잭션을 수행하여 모든 리소스 원상 복구
        if( !submitted )
        {
            RollbackSubmission(core, rioEvent, owner);
            return false;
        }

        // 9. 모든 준비 및 다중 제출 과정 성공
        return true;
    }

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