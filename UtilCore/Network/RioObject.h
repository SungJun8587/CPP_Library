
//***************************************************************************
// RioObject.h : interface for the CRioObject class.
//
//***************************************************************************

#ifndef __RIOOBJECT_H__
#define __RIOOBJECT_H__

#ifndef __RIOCOMMON_H__
#include <Network/RioCommon.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

class CRioEvent;

//***************************************************************************
// @brief RIO 비동기 I/O의 논리적 Owner 객체
//
// @details
//      CRioSend / CRioReceive에서 RIO RequestContext에 연결되는
//      CRioEvent가 CRioObject의 shared_ptr을 보유합니다.
//
//      따라서 RIO completion이 도착하기 전까지 CRioObject의
//      객체 lifetime은 안전하게 유지됩니다.
//
// [스레드 안전성 및 Lock-Free 동기화]
//      CRioObject는 다수의 워커 스레드(Worker Thread)에서 동시에 I/O 카운트를
//      조작할 수 있으므로, std::atomic 카운터와 Lock-Free CAS(Compare-And-Swap) Loop를
//      사용하여 Mutex 락 없이 빠른 동기화를 제공합니다.
//
// [enable_shared_from_this 상속]
//      I/O 요청 등록 시 자기 자신의 std::shared_ptr을 안전하게 생성하여
//      CRioEvent에 소유권을 넘겨주기 위해 enable_shared_from_this를 상속받습니다.
//
// [추상 클래스 및 다형성]
//      Dispatch() 순수 가상 함수를 통해 RIO 완료 알림을 세션/소켓 등
//      구체적인 비즈니스 로직 클래스로 디스패치하는 추상 기반 클래스입니다.
//***************************************************************************
class CRioObject : public std::enable_shared_from_this<CRioObject>
{
public:
    CRioObject() noexcept;
    virtual ~CRioObject() noexcept;

    CRioObject(const CRioObject&) = delete;
    CRioObject& operator=(const CRioObject&) = delete;

    CRioObject(CRioObject&&) = delete;
    CRioObject& operator=(CRioObject&&) = delete;

    bool IncrementIoCount() noexcept;
    void DecrementIoCount() noexcept;
    uint32_t GetIoCount() const noexcept;
    bool HasOutstandingIo() const noexcept;

    virtual void Dispatch(
        CRioEvent* rioEvent,
        ULONG bytesTransferred,
        LONG status) noexcept = 0;

protected:
    void ResetIoCount() noexcept;

private:
    std::atomic<uint32_t> _ioCount{ 0 };        // 현재 진행 중인 비동기 RIO I/O 카운터
};

#endif // __RIOOBJECT_H__