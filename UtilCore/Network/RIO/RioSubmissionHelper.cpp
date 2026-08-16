
//***************************************************************************
// RioSubmissionHelper.cpp : implementation of the CRioSubmissionHelper class.
//
//***************************************************************************

#include "pch.h"
#include "RioSubmissionHelper.h"

namespace
{
    //***************************************************************************
    // @brief CRioObject의 Owner shared_ptr을 안전하게 획득합니다.
    // 
    // @details
    //      - CRioObject 포인터로부터 `GetRioObjectPtr()`을 호출하여
    //        안전하게 `CRioObjectRef`(shared_ptr)를 획득합니다.
    //      - 예외 발생 시 빈 스마트 포인터를 반환합니다.
    // 
    // @param owner 대상 CRioObject 포인터
    // @return CRioObjectRef 획득된 shared_ptr 객체
    //***************************************************************************
    CRioObjectRef AcquireRioObjectOwner(CRioObject* owner) noexcept
    {
        if( owner == nullptr ) return {};

        try
        {
            return owner->GetRioObjectPtr();
        }
        catch( ... )
        {
            return {};
        }
    }
}

//***************************************************************************
// @brief CRioObject Owner shared_ptr을 획득합니다.
// 
// @details
//      - 내부 익명 namespace의 `AcquireRioObjectOwner` 유틸리티 함수를 호출하여
//        CRioObject의 shared_ptr 레퍼런스를 안전하게 가져옵니다.
// 
// @param owner 대상 CRioObject 포인터
// @return CRioObjectRef 획득된 shared_ptr 객체
//***************************************************************************
CRioObjectRef CRioSubmissionHelper::AcquireOwner(CRioObject* owner) noexcept
{
    return AcquireRioObjectOwner(owner);
}

//***************************************************************************
// @brief CRioEvent에 바인딩된 슬롯 자원을 즉시 반환합니다.
// 
// @details
//      - RIO 이벤트에 등록되어 있던 모든 BufferBinding 정보를 순회하며
//        각 버퍼의 슬롯을 강제로 반환(`FreeSlot`)합니다.
//      - 반환 완료 후 이벤트 내의 바인딩 목록을 초기화합니다.
// 
// @param rioEvent 바인딩 정보를 가지고 있는 이벤트 객체 포인터
//***************************************************************************
void CRioSubmissionHelper::RollbackBindings(CRioEvent* rioEvent) noexcept
{
    // 1. 전달받은 RIO 이벤트 객체 포인터가 유효한지 확인 (유효하지 않다면 즉시 종료)
    if( rioEvent == nullptr ) return;

    // 2. 이벤트 객체로부터 현재 바인딩되어 있는 모든 버퍼 바인딩 목록(BufferBinding) 참조 획득
    const CVector<CRioEvent::BufferBinding>& bindings = rioEvent->GetBufferBindings();

    // 3. 등록된 모든 버퍼 바인딩 항목을 순회하며 슬롯 반환 작업 수행
    for( const CRioEvent::BufferBinding& binding : bindings )
    {
        // 4. 바인딩된 버퍼 포인터나 슬롯 인덱스가 비정상적인 값인지 검증
        if( binding.buffer == nullptr || binding.slotIndex == Rio::kInvalidSlotIndex )
        {
            // 비정상적인 바인딩이 발견된 경우 어설션 발생 후 해당 항목은 건너뜀
            assert(false && "CRioSubmissionHelper rollback contains invalid BufferBinding");
            continue;
        }

        // 5. 해당 버퍼의 지정된 슬롯 인덱스를 원래 풀로 반환(FreeSlot) 시도
        const bool freed = binding.buffer->FreeSlot(binding.slotIndex);

        // 6. 슬롯 반환에 실패한 경우 어설션 발생 (자원 누수 방지 경고)
        if( !freed )
        {
            assert(false && "CRioSubmissionHelper rollback FreeSlot failed");
        }
    }

    // 7. 모든 슬롯 반환 처리가 완료된 후, 이벤트 객체 내부의 버퍼 바인딩 목록 초기화
    rioEvent->ClearBufferBindings();
}

//***************************************************************************
// @brief Submission 실패시 완벽한 역순 롤백 트랜잭션을 수행합니다.
//        Buffer Slot 반환 -> Event Owner 반환 -> Object IoCount-- -> EventPool Free
// 
// @details
//      - RIO 전송 요청(Submit) 등록 과정 중 실패가 발생했을 때 호출됩니다.
//      - 할당되었던 버퍼 슬롯, 이벤트 소유권, 객체 I/O 카운트 및 이벤트 풀 객체를
//        정확한 역순으로 안전하게 복구(Rollback)합니다.
// 
// @param core RIO 코어 객체 참조
// @param rioEvent 롤백할 RIO 이벤트 객체 포인터
// @param owner 대상 RIO 객체 포인터
//***************************************************************************
void CRioSubmissionHelper::RollbackSubmission(CRioCore& core, CRioEvent* rioEvent, CRioObject* owner) noexcept
{
    // 1. 롤백할 RIO 이벤트 객체가 존재하지 않는 경우 처리
    if( rioEvent == nullptr )
    {
        // 이벤트는 없지만 대상 RIO 객체(owner)가 유효하다면 I/O 카운트만 감소 후 종료
        if( owner != nullptr )
        {
            owner->DecrementIoCount();
        }

        return;
    }

    // 2. 이벤트에 바인딩되어 있던 모든 버퍼 슬롯 자원을 원래 풀로 반환
    RollbackBindings(rioEvent);

    // 3. 이벤트 객체 내부에 소유하고 있던 오너의 shared_ptr 레퍼런스 안전하게 추출(`TakeOwner`)
    CRioObjectRef rollbackOwner = rioEvent->TakeOwner();

    // 4. 추출한 오너 또는 인자로 전달받은 오너의 진행 중인 I/O 카운트(IoCount) 감소
    if( rollbackOwner )
    {
        rollbackOwner->DecrementIoCount();
    }
    else if( owner != nullptr )
    {
        owner->DecrementIoCount();
    }

    // 5. 코어 객체로부터 이벤트 풀(Event Pool)을 획득하여 사용이 끝난 RIO 이벤트 객체 반환
    if( CRioEventPool* eventPool = core.GetEventPool(); eventPool != nullptr )
    {
        eventPool->Free(rioEvent);
    }
}

//***************************************************************************
// @brief 단일 RIO Buffer Submission을 수행합니다.
// 
// @details
//      - 단일 버퍼를 사용하는 RIO I/O(Send/Receive 등) 요청을 안전하게 준비하고 제출합니다.
//      - 검증 단계: 인자 유효성 검사, BufferId/Length 검사, Slot Index 검사
//      - 획득 단계: Owner shared_ptr 획득, Object IoCount 증가, Event 초기화 및 Buffer Slot 바인딩
//      - 실행 단계: 전달받은 람다(`submit`)를 통해 실제 RIO Submit 수행
//      - 실패 시: `RollbackSubmission`을 호출하여 트랜잭션 원상 복구
// 
// @param core RIO Core 객체 참조
// @param eventType RIO 이벤트 타입 (Send / Receive 등)
// @param requestQueue RIO Request Queue 핸들
// @param buffer 전송/수신용 RIO_BUF 구조체
// @param bufferOwner 버퍼를 소유하고 있는 CRioBuffer 포인터
// @param slotIndex 버퍼 슬롯 인덱스
// @param rioEvent 작업을 처리할 CRioEvent 포인터
// @param owner 완료 통지를 받을 CRioObject 포인터
// @param submit 실제 RIO 제출을 실행할 람다 함수 (`std::function<bool()>`)
// @return bool Submission 및 모든 준비 과정 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSubmissionHelper::SubmitSingle(
    CRioCore& core,
    Rio::EventType eventType,
    RIO_RQ requestQueue,
    const RIO_BUF& buffer,
    CRioBuffer* bufferOwner,
    uint32_t slotIndex,
    CRioEvent* rioEvent,
    CRioObject* owner,
    const std::function<bool()>& submit) noexcept
{
    // 1. 필수 인자(Request Queue, 이벤트, 오너, 버퍼 오너, 제출 람다 등)의 유효성 검사
    if( requestQueue == RIO_INVALID_RQ || rioEvent == nullptr || owner == nullptr || bufferOwner == nullptr || !submit )
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

    // 8. 전달받은 람다(`submit`)를 통해 실제 RIO 제출(Send/Receive 등) 수행
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
// 
// @details
//      - 다중 버퍼(Scatter/Gather 등)를 사용하는 RIO I/O 요청을 안전하게 준비하고 제출합니다.
//      - 검증 단계: 전체 버퍼 배열 및 바인딩 배열 유효성 사전 검사
//      - 획득 단계: Owner shared_ptr 획득, Object IoCount 증가, Event 초기화 및 다중 Buffer Slot 바인딩
//      - 실행 단계: 전달받은 람다(`submit`)를 통해 실제 RIO 다중 Submit 수행
//      - 실패 시: `RollbackSubmission`을 호출하여 트랜잭션 원상 복구
// 
// @param core RIO Core 객체 참조
// @param eventType RIO 이벤트 타입 (Send / Receive 등)
// @param requestQueue RIO Request Queue 핸들
// @param data RIO_BUF 배열 포인터
// @param dataBufferCount 배열 내 버퍼 개수
// @param dataBindings 각 버퍼 슬롯에 대한 바인딩 정보 배열 포인터 (선택 사항)
// @param rioEvent 작업을 처리할 CRioEvent 포인터
// @param owner 완료 통지를 받을 CRioObject 포인터
// @param submit 실제 RIO 다중 제출을 실행할 람다 함수 (`std::function<bool()>`)
// @return bool Submission 및 모든 준비 과정 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSubmissionHelper::SubmitMulti(
    CRioCore& core,
    Rio::EventType eventType,
    RIO_RQ requestQueue,
    const RIO_BUF* data,
    ULONG dataBufferCount,
    const CRioEvent::BufferBinding* dataBindings,
    CRioEvent* rioEvent,
    CRioObject* owner,
    const std::function<bool()>& submit) noexcept
{
    // 1. 기본 필수 인자(Request Queue, 데이터 포인터, 버퍼 개수, 이벤트, 오너, 제출 람다 등)의 유효성 검사
    if( requestQueue == RIO_INVALID_RQ || data == nullptr || dataBufferCount == 0 || rioEvent == nullptr || owner == nullptr || !submit )
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

    // 7. 전달받은 람다(`submit`)를 통해 실제 RIO 다중 제출(Scatter/Gather 등) 수행
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