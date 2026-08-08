
//***************************************************************************
// RioObject.h : RIO 비동기 오브젝트 인터페이스 (I/O Drain 보장)
//
//***************************************************************************

#ifndef __RIOOBJECT_H__
#define __RIOOBJECT_H__

#include <winsock2.h>
#include <memory>
#include <atomic>
#include <cstdint>

class CRioEvent;

//===========================================================================
// @class CRioObject
// @brief RIO 비동기 처리를 수행하는 대상(예: CSession)의 추상 기반 클래스.
// @details 원자적 I/O 카운터를 통해 Shutdown 시점의 완벽한 Outstanding I/O Drain을 보장합니다.
//===========================================================================
class CRioObject : public std::enable_shared_from_this<CRioObject>
{
public:
    //***************************************************************************
    // @brief CRioObject 생성자
    // @details 진행 중인 I/O 카운터를 0으로 초기화합니다.
    //***************************************************************************
    CRioObject() : _outstandingIoCount(0) {}

    //***************************************************************************
    // @brief 가상 소멸자
    //***************************************************************************
    virtual ~CRioObject() = default;

    //***************************************************************************
    // @brief 세션이 보유한 소켓 핸들을 반환합니다.
    // @return SOCKET 소켓 핸들
    //***************************************************************************
    virtual SOCKET GetSocket() const = 0;

    //***************************************************************************
    // @brief RIO 비동기 I/O 완료 시 호출되는 핵심 디스패치 메서드
    // @param rioEvent 완료된 RIO 이벤트 포인터
    // @param numOfBytes 실제 전송 혹은 수신된 바이트 수
    // @param status Winsock 에러 코드 (NO_ERROR: 성공, 그 외 WSAE* 에러 코드)
    //
    // @warning 호출 계약 (CRioCore::ProcessRioResult 기준):
    //     이 함수가 반환된 직후 CRioCore가 rioEvent를 즉시 이벤트 풀로 반환합니다.
    //     따라서 구현체는 이 함수 안에서:
    //       - rioEvent 포인터를 보관하거나 재사용해서는 안 됩니다.
    //       - 후속 RIOReceive/RIOSend를 등록할 때는 반드시 CRioEventPool::Alloc()으로
    //         새 이벤트를 새로 발급받아 사용해야 합니다.
    //     이 계약을 어기면 풀로 반환된 이벤트를 다른 Alloc() 호출이 동시에 가져가는
    //     이중 사용(use-after-free) 상황이 발생할 수 있습니다.
    //***************************************************************************
    virtual void Dispatch(CRioEvent* rioEvent, ULONG numOfBytes = 0, LONG status = NO_ERROR) noexcept = 0;

    //***************************************************************************
    // @brief 비동기 I/O 요청이 시작될 때 카운터를 원자적으로 증가시킵니다.
    //***************************************************************************
    void IncrementIoCount() { _outstandingIoCount.fetch_add(1, std::memory_order_relaxed); }

    //***************************************************************************
    // @brief 비동기 I/O 완료가 처리될 때 카운터를 원자적으로 감소시킵니다.
    //***************************************************************************
    void DecrementIoCount() noexcept { _outstandingIoCount.fetch_sub(1, std::memory_order_relaxed); }

    //***************************************************************************
    // @brief 현재 진행 중인 Outstanding I/O 총 개수를 조회합니다.
    // @return int32 진행 중인 I/O 카운터 값
    //***************************************************************************
    int32 GetIoCount() const { return _outstandingIoCount.load(std::memory_order_acquire); }

private:
    std::atomic<int32> _outstandingIoCount; // 현재 진행 중인 비동기 I/O 요청 총 개수
};

#endif // __RIOOBJECT_H__