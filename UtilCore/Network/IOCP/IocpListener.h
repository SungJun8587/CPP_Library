
//***************************************************************************
// IocpListener.h : interface for the CIocpListener class.
//
//***************************************************************************

#ifndef UC_IOCPLISTENER_H
#define UC_IOCPLISTENER_H

#include <Network/NetAddress.h>
#include <Network/SocketUtils.h>
#include <Network/IOCP/IocpCommon.h>
#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpEvent.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

//***************************************************************************
// @brief IOCP 전용 세션 생성 팩터리 함수 타입
// @return CIocpObjectRef 생성된 IOCP 객체 포인터 (CSession과 디커플링 유지)
//***************************************************************************
using IocpSessionFactory = std::function<CIocpObjectRef()>;

//***************************************************************************
// @brief Accept 완료 후 외부 후속 처리를 담당할 콜백 함수 타입
// @param session 연결이 완료된 세션 객체 (CIocpObjectRef)
// @param netAddr 추출된 클라이언트의 IP/Port 주소 정보
//***************************************************************************
using OnAcceptCallback = std::function<void(CIocpObjectRef session, CNetAddress netAddr)>;

//***************************************************************************
// @class CIocpListener
// @brief 특정 세션 구현체에 종속되지 않는 완전 추상화된 IOCP Accept 처리 클래스.
//
// @details
// [Accept 아키텍처 및 스레드 설계]
//      - IOCP 환경에서는 Windows 커널의 비동기 확장 함수인 `AcceptEx`를 활용합니다.
//      - 비동기 완료 및 후속 처리는 글로벌 워커 스레드 풀이 담당하므로, RIO와 달리 리스너 내부에 
//        별도의 전용 `_acceptThread`나 루프 제어 플래그(`_isListening`)가 필요 없습니다.
//
// 역할:
//      1. Listen 소켓 생성, 바인딩(Bind) 및 리슨(Listen) 수행
//      2. AcceptEx 비동기 수락 요청을 풀(Pool) 단위로 사전 등록
//      3. IOCP 완료 통지 수신 시 Accept 처리 후 세션 연결 후속 작업 위임
//      4. AcceptEx 실패/완료 후 자동 재등록으로 Accept Pool 개수 영구 유지
//
// 디커플링 설계:
//      CSession 클래스를 직접 참조하지 않으며, CIocpObjectRef 인터페이스와
//      OnAcceptCallback을 활용하여 상위 네트워크 레이어로 이벤트를 전파합니다.
//
// 재시도 정책:
//      세션 생성 실패 / AcceptEx 즉시 실패 / AcceptEx 실패 완료 /
//      SetUpdateAcceptContext 실패 / IOCP Register 실패 — 이 실패 경로 전부가
//      AcceptEvent::retryCount 하나를 공유해 누적 증가시킵니다. 재시도는 포기하지
//      않습니다: 지연이 Iocp::kAcceptRetryBaseDelayMs에서 두 배씩 늘어 Iocp::kAcceptRetryMaxDelayMs에서 멈추는
//      백오프로 계속 재시도하므로, 일시적인 자원 고갈 뒤에도 Accept Pool 크기가 줄어들지
//      않고 복구됩니다. 연속 실패는 첫 실패와 Iocp::kAcceptRetryLogInterval회마다 로그로 남기며,
//      Accept가 최종 성공하면 retryCount는 0으로 리셋됩니다.
//
//      RegisterAccept()는 실패 시 자신을 루프+Sleep으로 블로킹하지 않습니다.
//      IOCP 워커 스레드에서 호출되는 경로(Dispatch → ProcessAccept →
//      RegisterAccept)이므로, 여기서 블로킹하면 같은 워커가 처리해야 할 다른
//      완료 이벤트(Recv/Send 등)가 지연되기 때문입니다. 실패 시 ScheduleRetry()가
//      백오프 지연 후 별도의 detached 스레드에서 1회만 RegisterAccept()를
//      재호출하도록 위임합니다.
//
//      Accept가 성공하면 사용자 콜백(OnAcceptCallback)을 호출하기 전에 해당 슬롯의
//      AcceptEx를 먼저 재게시합니다. 콜백(ProcessConnect()/OnConnected() 포함)이
//      느려도 Accept Pool 슬롯이 그동안 비지 않습니다.
//
// [재시도 스레드 생명주기 관리]
//      _pendingRetries(원자 카운터)로 진행 중인 detached retry를 추적하고,
//      _closing 플래그로 종료 중에는 재시도를 차단합니다. 재시도 스레드는 Sleep이 아니라
//      조건 변수로 대기하므로 Stop()이 즉시 깨울 수 있고, Stop()은 모든 재시도가 끝날
//      때까지 결정론적으로 대기한 뒤 반환합니다. 소멸자는 _acceptEvents를 delete하기 전에
//      반드시 Stop()을 호출해, 백그라운드 재시도 스레드가 전부 정리된 뒤에만 메모리를
//      해제합니다.
//      재시도 스레드는 카운트를 내린 "뒤에" Listener 참조를 놓습니다. 그 스레드가 마지막
//      소유자라 ~CIocpListener()가 스레드 안에서 실행되더라도, 소멸자의 Stop()이 그
//      스레드 자신의 카운트를 기다리며 교착되지 않게 하기 위함입니다. 카운트 감소와
//      notify는 _retryDrainMutex 아래에서 수행해 Stop()이 반환(→ 멤버 소멸)한 뒤에
//      뮤텍스를 건드리지 않습니다.
//
// [_listenSocket]
//      Stop()(임의의 호출 스레드)의 CloseSocket()과 IOCP 워커 스레드의
//      RegisterAccept()/ProcessAccept()가 동시에 접근하므로 std::atomic<SOCKET>입니다
//      (CRioListener::_listenSocket과 동일). 동기화 없는 동시 read/write는 C++ 표준상
//      data race이기 때문이며, lock-free라 비용은 일반 정수 접근과 사실상 같습니다.
//***************************************************************************
class CIocpListener : public CIocpObject, public std::enable_shared_from_this<CIocpListener>
{
public:
    CIocpListener();
    virtual ~CIocpListener();

public:
    //***************************************************************************
    // @brief IOCP에 등록할 리스닝 소켓 핸들을 반환합니다.
    // @return HANDLE Listen 소켓의 HANDLE 캐스팅 값
    //***************************************************************************
    virtual HANDLE  GetHandle() override { return reinterpret_cast<HANDLE>(_listenSocket.load(std::memory_order_acquire)); }

    virtual void    Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) override;

    // CIocpObject의 순수 가상 함수 구현 추가
    virtual CIocpObjectRef GetIocpObjectPtr() override
    {
        return CIocpObjectRef(shared_from_this(), static_cast<CIocpObject*>(this));
    }

public:
    bool    StartAccept(CIocpCoreRef iocpCore, CNetAddress netAddr, IocpSessionFactory sessionFactory, uint32 acceptPoolSize = Iocp::kDefaultAcceptPoolSize, OnAcceptCallback onAccept = nullptr);

    //***************************************************************************
    // @brief Listen 소켓을 닫고, 진행 중인 모든 detached 재시도 스레드가 끝날
    //        때까지 결정론적으로 대기한 뒤 반환합니다.
    // @details 이미 정지된 상태에서 재호출해도 안전합니다(idempotent). 호출
    //          이후에는 이 Listener에 대해 더 이상 어떤 AcceptEx도 재게시되지
    //          않는다는 것이 보장됩니다 — 소멸자가 안전하게 _acceptEvents를
    //          해제할 수 있는 것은 이 보장 덕분입니다.
    //***************************************************************************
    void    Stop();

    //***************************************************************************
    // @brief Listen 소켓의 주소 바인딩 방식을 지정합니다 (기본값: ReuseAddress, 기존 동작).
    // @details StartAccept() 이전에 호출해야 한다. 자세한 내용은 ListenAddressMode 참고.
    //***************************************************************************
    void    SetAddressMode(ListenAddressMode mode) noexcept { _addressMode = mode; }

    //***************************************************************************
    // @brief Listen 소켓만 닫습니다. 진행 중인 재시도 스레드를
    //        기다리지 않으므로, 완전한 종료가 필요하면 Stop()을 사용하십시오.
    //***************************************************************************
    void    CloseSocket();

    //***************************************************************************
    // @brief 현재 Listen 소켓 핸들을 반환합니다.
    // @return SOCKET 현재 리슨 중인 소켓 핸들 (정지 상태면 INVALID_SOCKET)
    //***************************************************************************
    SOCKET  GetListenSocket() const noexcept { return _listenSocket.load(std::memory_order_acquire); }

private:
    //***************************************************************************
    // @brief 비동기 AcceptEx I/O 요청을 등록합니다. 실패 시 HandleAcceptFailure()로
    //        retryCount를 증가시키고 백오프 재시도를 위임합니다.
    //        _closing이 true면(Stop() 진행 중) 즉시 아무것도 하지 않고 반환합니다.
    // @param acceptEvent AcceptEx 호출에 사용될 이벤트 포인터 (재시도 횟수를 자체 보유)
    //***************************************************************************
    void    RegisterAccept(AcceptEvent* acceptEvent);

    //***************************************************************************
    // @brief IOCP 워커 스레드를 블로킹하지 않도록, 재시도를 백오프 지연 후
    //        별도의 detached 스레드에서 1회 수행합니다. Listener가 그 사이
    //        소멸되면 weak_ptr이 만료되어 안전하게 아무 일도 하지 않습니다.
    //        스레드 시작/종료 시 _pendingRetries를 증감시켜 Stop()이 이
    //        스레드의 완료를 추적/대기할 수 있게 합니다.
    // @param acceptEvent 재시도할 AcceptEvent 포인터 (Listener가 소유, _acceptEvents에 보관됨)
    //***************************************************************************
    void    ScheduleRetry(AcceptEvent* acceptEvent);

    //***************************************************************************
    // @brief Accept 실패 경로 공통 처리: retryCount 증가, 샘플링 로그, 백오프 재시도 위임.
    // @param acceptEvent 실패한 AcceptEvent
    // @param stage 실패 단계 이름 (로그용)
    // @param errorCode 실패 원인 Win32/WSA 에러 코드 (로그용)
    //***************************************************************************
    void    HandleAcceptFailure(AcceptEvent* acceptEvent, const TCHAR* stage, int32 errorCode);

    //***************************************************************************
    // @brief 연속 실패 횟수에 대한 재시도 지연(ms)을 계산합니다.
    //***************************************************************************
    static uint32 CalcRetryDelayMs(int32 retryCount) noexcept;

    //***************************************************************************
    // @brief 재시도 스레드 종료 처리: _pendingRetries를 감소시키고 마지막이면 Stop()을 깨웁니다.
    // @details 감소와 notify를 _retryDrainMutex 아래에서 수행합니다. 이 함수가 반환한 뒤에는
    //          호출 스레드가 Listener 멤버를 건드려서는 안 됩니다(Stop()이 반환해 멤버가
    //          소멸할 수 있음).
    //***************************************************************************
    void    FinishRetry() noexcept;

    void    ProcessAccept(AcceptEvent* acceptEvent);

private:
    ListenAddressMode           _addressMode = ListenAddressMode::ReuseAddress; // 주소 바인딩 옵션 (StartAccept() 이전에 설정)
    std::atomic<SOCKET>         _listenSocket{ INVALID_SOCKET };    // 리스닝 소켓 핸들 (IOCP 워커와 Stop() 호출 스레드가 동시 접근)
    CIocpCoreRef                _iocpCore = nullptr;                // 연동할 IOCP 코어 참조
    IocpSessionFactory          _sessionFactory = nullptr;          // 세션 생성 팩터리
    OnAcceptCallback            _onAcceptCallback = nullptr;        // Accept 완료 알림 콜백
    std::vector<AcceptEvent*>   _acceptEvents;                      // 생성된 AcceptEvent 관리 벡터

    // 재시도 스레드 생명주기 관리
    std::atomic<bool>          _closing{ false };        // true가 되면 RegisterAccept/ScheduleRetry가 즉시 포기
    std::atomic<int32>         _pendingRetries{ 0 };      // 현재 대기(Sleep) 중이거나 실행 중인 detached retry 스레드 수
    std::mutex                 _retryDrainMutex;          // _pendingRetries==0 대기용
    std::condition_variable    _retryDrainCv;              // 마지막 retry 스레드가 종료 시 notify
};

#endif // ndef UC_IOCPLISTENER_H