
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