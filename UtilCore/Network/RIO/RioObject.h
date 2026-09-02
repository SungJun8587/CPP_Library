
//***************************************************************************
// RioObject.h : interface for the CRioObject class.
//
//***************************************************************************

#ifndef UC_RIOOBJECT_H
#define UC_RIOOBJECT_H

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioEvent.h>

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
//
// [_ioCount lifecycle]
//      _ioCount는 IncrementIoCount() -> RIO submission -> completion ->
//      DecrementIoCount() 경로로만 관리됩니다.
//      외부에서 임의로 값을 재설정하는 API(예: ResetIoCount())는 존재하지
//      않으며, 이는 의도적인 설계입니다. 그런 API가 존재하면 진행 중인
//      submission/completion과 경쟁하여 lifecycle invariant를 깨뜨릴 수
//      있습니다.
//***************************************************************************
class CRioObject
{
public:
    CRioObject() noexcept;
    virtual ~CRioObject() noexcept;

    virtual CRioObjectRef GetRioObjectPtr() = 0;

    CRioObject(const CRioObject&) = delete;
    CRioObject& operator=(const CRioObject&) = delete;

    CRioObject(CRioObject&&) = delete;
    CRioObject& operator=(CRioObject&&) = delete;

    bool IncrementIoCount() noexcept;

    //***************************************************************************
    // @brief Outstanding I/O reference를 하나 감소시킵니다.
    // @param outReachedZero [out, 선택] 감소 성공 시, 이 감소로 카운트가 정확히
    //        0이 됐는지 여부를 받습니다(true=0이 됨). null이면 무시됩니다.
    //        CAS 성공 시점에 계산한 값이라 별도 재조회 없이 정확합니다 —
    //        "이 스레드의 감소가 카운트를 0으로 만든 유일한 감소였는지"를
    //        레이스 없이 판별할 수 있습니다(원자적 CAS는 선형화 가능하므로
    //        1->0 전이는 오직 하나의 스레드만 관측함).
    // @return bool 정상적으로 1 감소했으면 true, underflow면 false.
    //***************************************************************************
    bool DecrementIoCount(bool* outReachedZero = nullptr) noexcept;

    uint32 GetIoCount() const noexcept;
    bool HasOutstandingIo() const noexcept;

    virtual void Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) = 0;

    //***************************************************************************
    // @brief Outstanding I/O 카운트가 0에 도달했을 때 호출되는 훅(기본은 no-op).
    // @details CRioCore::ProcessRioResult()의 ObjectIoCountGuard가 Dispatch()
    //          반환 이후, DecrementIoCount()의 이 감소가 카운트를 정확히 0으로
    //          만들었을 때만 호출합니다(레이스 없이 유일하게 보장됨 — 위
    //          DecrementIoCount() 설명 참고). 이 시점은 이미 RIO 워커 스레드
    //          위에서 실행 중이므로, 구현체는 여기서 절대 블로킹 대기를 하면
    //          안 됩니다(워커를 막으면 다른 세션/객체의 completion 처리까지
    //          지연됨 — 이 코드베이스 전반의 "워커는 절대 블로킹하지 않는다"
    //          원칙과 동일). CRioSession은 이 훅으로 "종료 진행 중이었는데
    //          바로 이 completion이 마지막 outstanding이었던 경우"를 감지해
    //          FinalizeClose()를 대신 호출한다.
    //***************************************************************************
    virtual void OnIoCountReachedZero() noexcept {}

private:
    std::atomic<uint32> _ioCount{ 0 };        // 현재 진행 중인 비동기 RIO I/O 카운터
};

#endif // ndef UC_RIOOBJECT_H