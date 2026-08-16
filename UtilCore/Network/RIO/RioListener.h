
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

class CRioSession;
using CRioSessionRef = std::shared_ptr<CRioSession>;

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
//      - RIO 환경에서는 블로킹 방식의 동기 `accept()` 모델을 사용하여 클라이언트 접속을 대기합니다.
//      - 클라이언트 접속 요청을 대기하기 위한 전용 스레드(`_acceptThread`)와 안전한 정지를 위한 
//        원자적 상태 플래그(`_isListening`)를 내부적으로 보유하고 관리합니다.
// 
// 역할:
//      1. Listen 소켓 생성, 바인딩(Bind) 및 리슨(Listen) 수행
//      2. 백그라운드 Accept 스레드를 통해 클라이언트 접속(`accept`) 대기
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
    bool Start(CRioCoreRef rioCore, CNetAddress netAddr, RioSessionFactory sessionFactory, OnRioAcceptCallback onAccept = nullptr);
    void Stop();

    SOCKET GetListenSocket() const noexcept { return _listenSocket; }

private:
    void AcceptLoop();
    RIO_RQ CreateRequestQueueForSocket(SOCKET clientSocket);

private:
    SOCKET                  _listenSocket = INVALID_SOCKET;     // 리스닝 소켓 핸들
    CRioCoreRef             _rioCore = nullptr;                 // 연동할 RIO 코어 참조
    RioSessionFactory       _sessionFactory = nullptr;          // 세션 생성 팩터리
    OnRioAcceptCallback     _onAcceptCallback = nullptr;        // Accept 완료 알림 콜백

    std::atomic<bool>       _isListening{ false };              // 리스닝 상태 플래그
    std::thread             _acceptThread;                      // Accept 수신 전용 스레드
};

#endif // ndef __RIOLISTENER_H__