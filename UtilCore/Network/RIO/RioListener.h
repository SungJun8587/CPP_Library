
//***************************************************************************
// RioListener.h : interface for the CRioListener class.
//
//***************************************************************************

#ifndef __RIOLISTENER_H__
#define __RIOLISTENER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIO_CORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __NET_ADDRESS_H__
#include <Network/NetAddress.h>
#endif

#include <functional>
#include <thread>
#include <atomic>
#include <memory>

//***************************************************************************
// @brief RIO 전용 세션 생성 팩터리 함수 타입
// @return CRioSessionRef 생성된 RIO 세션 객체 포인터
//***************************************************************************
using RioSessionFactory = std::function<CRioSessionRef()>;

//***************************************************************************
// @brief Accept 완료 후 외부 후속 처리를 담당할 콜백 함수 타입
// @param session 연결이 완료된 세션 객체
// @param clientSocket 클라이언트 소켓 핸들
// @param requestQueue 생성된 RIO_RQ 핸들
// @param netAddr 추출된 클라이언트의 IP/Port 주소 정보
//***************************************************************************
using OnRioAcceptCallback = std::function<void(CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)>;

//***************************************************************************
// @class CRioListener
// @brief RIO(Registered I/O) 기반의 클라이언트 접속 수락(Accept) 전담 클래스
//
// @details
// [Accept 아키텍처 및 스레드 설계]
//      - RIO 환경에서는 AcceptEx 기반의 Overlapped I/O를 사용하여 클라이언트 접속을 대기합니다.
//      - 클라이언트 접속 요청을 대기하기 위한 전용 스레드(`_acceptThread`)와 안전한 정지를 위한
//        원자적 상태 플래그(`_isListening`)를 내부적으로 보유하고 관리합니다.
//      - `_listenSocket`은 accept 스레드와 Stop()을 호출하는 임의의 스레드가 동시에
//        접근하므로 std::atomic<SOCKET>으로 관리합니다.
//      - Stop()이 accept 스레드 자신에게서 호출되는 경우(콜백 내부에서 Stop()을
//        부르는 시나리오)는 self-join 데드락을 방지하기 위해 join을 생략합니다.
//        이 경우에도 스레드 자체는 AcceptLoop()가 _isListening==false를 감지하는
//        즉시 자연 종료됩니다.
//      - `_sessionFactory`/`_onAcceptCallback` 호출은 사용자 코드이므로 예외가
//        발생할 수 있습니다. AcceptLoop()가 실행되는 std::thread 함수 밖으로
//        예외가 빠져나가면 std::terminate()가 호출되어 프로세스 전체가 죽으므로,
//        두 호출 모두 try/catch로 감쌉니다.
//
// 역할:
//      1. Listen 소켓 생성, 바인딩(Bind) 및 리슨(Listen) 수행
//      2. 백그라운드 Accept 스레드를 통해 클라이언트 접속(`AcceptEx`) 대기
//      3. 접속된 소켓에 대한 RIO Request Queue(`RIO_RQ`) 생성
//      4. 세션 팩토리를 통해 세션을 생성하고 콜백을 통해 상위 서비스로 위임
//***************************************************************************
class CRioListener : public std::enable_shared_from_this<CRioListener>
{
public:
    CRioListener();
    virtual ~CRioListener();

    CRioListener(const CRioListener&) = delete;
    CRioListener& operator=(const CRioListener&) = delete;

public:
    //***************************************************************************
    // @brief 리스너를 초기화하고 Accept 스레드를 구동합니다.
    // @param rioCore RIO 코어 참조 객체 (null이면 실패)
    // @param netAddr 리슨할 네트워크 주소 (IP/Port)
    // @param sessionFactory 세션 생성 팩터리 (null이면 실패)
    // @param onAccept Accept 완료 시 호출될 콜백 (null이면 콜백 없이 동작 — 이 경우
    //        clientSocket/requestQueue는 즉시 정리되고 세션은 버려집니다)
    // @return bool 소켓 생성/바인드/리슨 및 Accept 스레드 시작까지 전부 성공하면 true
    //***************************************************************************
    bool Start(CRioCoreRef rioCore, CNetAddress netAddr, RioSessionFactory sessionFactory, OnRioAcceptCallback onAccept = nullptr);

    //***************************************************************************
    // @brief 리스너를 정지하고 Listen 소켓을 닫습니다.
    // @note 이미 정지된 상태에서 다시 호출해도 안전합니다(idempotent). Accept
    //       스레드 자신이 이 함수를 호출한 경우(콜백 내부 경유) self-join을
    //       피하기 위해 join을 생략하고 반환합니다 — 스레드는 곧 자연 종료됩니다.
    //***************************************************************************
    void Stop();

    //***************************************************************************
    // @brief 현재 Listen 소켓 핸들을 반환합니다.
    // @return SOCKET 현재 리슨 중인 소켓 핸들 (정지 상태면 INVALID_SOCKET)
    //***************************************************************************
    SOCKET GetListenSocket() const noexcept { return _listenSocket.load(std::memory_order_acquire); }

private:
    //***************************************************************************
    // @brief 클라이언트의 신규 연결 요청을 AcceptEx로 대기하고 수락하는 루프 함수.
    //        _acceptThread에서 실행됩니다.
    //***************************************************************************
    void AcceptLoop();

    //***************************************************************************
    // @brief 지정된 클라이언트 소켓용 RIO Request Queue를 생성합니다.
    // @param clientSocket 바인딩할 클라이언트 소켓
    // @return 생성된 RIO_RQ 핸들 (실패 시 RIO_INVALID_RQ)
    //***************************************************************************
    RIO_RQ CreateRequestQueueForSocket(SOCKET clientSocket);

private:
    std::atomic<SOCKET>     _listenSocket{ INVALID_SOCKET };    // 리스닝 소켓 핸들 (accept 스레드와 Stop() 호출 스레드가 동시 접근)
    CRioCoreRef             _rioCore = nullptr;                 // 연동할 RIO 코어 참조
    RioSessionFactory       _sessionFactory = nullptr;          // 세션 생성 팩터리
    OnRioAcceptCallback     _onAcceptCallback = nullptr;        // Accept 완료 알림 콜백

    std::atomic<bool>       _isListening{ false };              // 리스닝 상태 플래그
    std::thread             _acceptThread;                      // Accept 수신 전용 스레드
};
#endif // ndef __RIOLISTENER_H__