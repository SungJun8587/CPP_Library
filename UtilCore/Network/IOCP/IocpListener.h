
//***************************************************************************
// IocpListener.h : interface for the CIocpListener class.
//
//***************************************************************************

#ifndef __IOCPLISTENER_H__
#define __IOCPLISTENER_H__

#ifndef	__NETADDRESS_H__
#include <Network/NetAddress.h>
#endif

#ifndef	__SOCKETUTILS_H__
#include <Network/SocketUtils.h>
#endif

#ifndef	__IOCPCOMMON_H__
#include <Network/IOCP/IocpCommon.h>
#endif

#ifndef __IOCPCORE_H__
#include <Network/IOCP/IocpCore.h>
#endif

#ifndef __IOCPEVENT_H__
#include <Network/IOCP/IocpEvent.h>
#endif

#include <functional>

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
// 역할:
//     1. Listen 소켓 생성, 바인딩(Bind) 및 리슨(Listen) 수행
//     2. AcceptEx 비동기 수락 요청을 풀(Pool) 단위로 사전 등록
//     3. IOCP 완료 통지 수신 시 Accept 처리 후 세션 연결 후속 작업 위임
//     4. AcceptEx 실패/완료 후 자동 재등록으로 Accept Pool 개수 영구 유지
// 
// 디커플링 설계:
//     CSession 클래스를 직접 참조하지 않으며, CIocpObjectRef 인터페이스와
//     OnAcceptCallback을 활용하여 상위 네트워크 레이어로 이벤트를 전파합니다.
//
// 재시도 정책:
//     세션 소켓 생성 실패 / AcceptEx 즉시 실패 시 루프 기반으로 재시도합니다.
//     연속 실패가 kMaxAcceptRetry를 넘으면 해당 AcceptEvent는 재등록을 포기합니다
//     (워커 스레드가 무한정 블로킹되는 것을 방지).
//***************************************************************************
class CIocpListener : public CIocpObject
{
public:
    CIocpListener();
    virtual ~CIocpListener();

public:
    //***************************************************************************
    // @brief IOCP에 등록할 리스닝 소켓 핸들을 반환합니다.
    // @return HANDLE Listen 소켓의 HANDLE 캐스팅 값
    //***************************************************************************
    virtual HANDLE  GetHandle() override { return reinterpret_cast<HANDLE>(_listenSocket); }

    virtual void    Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) override;

public:
    bool    StartAccept(CIocpCoreRef iocpCore, CNetAddress netAddr, IocpSessionFactory sessionFactory, int32 acceptCount = 10, OnAcceptCallback onAccept = nullptr);
    void    CloseSocket();

private:
    // 세션 소켓 생성 실패 / AcceptEx 즉시 실패 시 최대 연속 재시도 횟수.
    // 초과 시 해당 AcceptEvent는 재등록을 포기하고 워커 스레드 블로킹을 방지한다.
    static constexpr int32 kMaxAcceptRetry = 5;

    void    RegisterAccept(AcceptEvent* acceptEvent, int32 retryCount = 0);
    void    ProcessAccept(AcceptEvent* acceptEvent);

private:
    SOCKET                  _listenSocket = INVALID_SOCKET;     // 리스닝 소켓 핸들
    CIocpCoreRef            _iocpCore = nullptr;                // 연동할 IOCP 코어 참조
    IocpSessionFactory      _sessionFactory = nullptr;          // 세션 생성 팩터리
    OnAcceptCallback        _onAcceptCallback = nullptr;        // Accept 완료 알림 콜백
    CVector<AcceptEvent*>   _acceptEvents;                      // 생성된 AcceptEvent 관리 벡터
};

#endif // ndef __IOCPLISTENER_H__