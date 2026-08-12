
//***************************************************************************
// RioObject.cpp : implementation of the CRioObject class.
//
//***************************************************************************

#include "pch.h"
#include "RioObject.h"

//***************************************************************************
// @brief CRioObject 기본 생성자
// @details
//      초기 I/O 카운터를 0으로 명시적 초기화합니다.
//      noexcept 키워드로 예외 미발생을 보장합니다.
//***************************************************************************
CRioObject::CRioObject() noexcept
    : _ioCount(0)
{
}

//***************************************************************************
// @brief CRioObject 소멸자
// @note
//      객체가 소멸되는 시점에는 Outstanding I/O가 남아 있어서는 안 됩니다.
//
//      CRioEvent가 Completion 전까지 shared_ptr<CRioObject>를 보유하기
//      때문에 정상적인 CRio lifecycle에서는 이 시점에 IoCount가 0이어야
//      합니다.
// @details
//      memory_order_acquire 로드를 통해 타 스레드에서 마지막 DecrementIoCount()가
//      수행한 Release 메모리 오더링과 동기화하여 검사합니다.
//      만약 남아있는 I/O가 있다면 Assert를 발생시켜 Use-After-Free 및 Dangling Callbacks를 방지합니다.
//***************************************************************************
CRioObject::~CRioObject() noexcept
{
    const uint32_t ioCount = _ioCount.load(std::memory_order_acquire);

    if( ioCount != 0 )
    {
        assert(false && "CRioObject destroyed while outstanding I/O exists");
        std::terminate();
    }
}

//***************************************************************************
// @brief Outstanding I/O reference를 하나 증가시킵니다.
// @details
//      Lock-Free 알고리즘인 compare_exchange_weak (CAS) 루프를 수행합니다.
//      Spurious Failure(의사 실패)를 고려해 루프 내에서 실패 시 자동 재시도합니다.
//      단순 카운트 증가 목적이므로 메모리 동기화 오버헤드를 최소화하기 위해
//      std::memory_order_relaxed 메모리 순서를 사용합니다.
//***************************************************************************
bool CRioObject::IncrementIoCount() noexcept
{
    uint32_t current = _ioCount.load(std::memory_order_relaxed);

    for( ;; )
    {
        if( current == std::numeric_limits<uint32_t>::max() )
        {
            assert(false && "CRioObject I/O counter overflow");
            return false;
        }

        if( _ioCount.compare_exchange_weak(
            current,
            current + 1,
            std::memory_order_relaxed,
            std::memory_order_relaxed) )
        {
            return true;
        }
    }
}

//***************************************************************************
// @brief Outstanding I/O reference를 하나 감소시킵니다.
// @note
//      이 함수는 void 반환형입니다.
//      CRioCore::ProcessRioResult()의 ObjectIoCountGuard는
//      Dispatch()가 정상적으로 반환된 이후 이 함수를 호출합니다.
// @details
//      CAS 루프 성공 시 std::memory_order_release 오더링을 적용합니다.
//      이로써 Dispatch() 로직 및 I/O 처리 과정에서 발생한 모든 메모리 변경 사항이
//      이 카운터를 관찰하는 다른 스레드(acquire 로드)에 가시성(Visibility)을 가집니다.
//***************************************************************************
void CRioObject::DecrementIoCount() noexcept
{
    // 루프 진입 전 최초 1회 읽는 값이므로 acquire가 필요 없다.
    // CAS가 실패할 경우 재시도용 값은 compare_exchange_weak의
    // failure order(relaxed)로 갱신되며, 실제 release-acquire 동기화는
    // CAS 성공 시의 release와 GetIoCount()의 acquire 페어링이 담당한다.
    uint32_t current = _ioCount.load(std::memory_order_relaxed);

    for( ;; )
    {
        if( current == 0 )
        {
            assert(false && "CRioObject I/O counter underflow");
            return;
        }

        if( _ioCount.compare_exchange_weak(
            current,
            current - 1,
            std::memory_order_release,
            std::memory_order_relaxed) )
        {
            return;
        }
    }
}

//***************************************************************************
// @brief 현재 Outstanding I/O 개수를 반환합니다.
// @details
//      std::memory_order_acquire를 사용하여 DecrementIoCount()의 Release 오더링과
//      동기화 페어(Acquire-Release semantics)를 이룹니다.
//***************************************************************************
uint32_t CRioObject::GetIoCount() const noexcept
{
    return _ioCount.load(std::memory_order_acquire);
}

//***************************************************************************
// @brief Outstanding I/O가 존재하는지 확인합니다.
// @return true: 진행 중인 I/O 존재, false: 진행 중인 I/O 없음
//***************************************************************************
bool CRioObject::HasOutstandingIo() const noexcept
{
    return GetIoCount() != 0;
}