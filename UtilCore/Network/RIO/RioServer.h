
//***************************************************************************
// RioServer.h : interface for the CRioServer class.
//
//***************************************************************************

#ifndef __RIOSERVER_H__
#define __RIOSERVER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIO_CORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __RIO_EVENT_POOL_H__
#include <Network/Rio/RioEventPool.h>
#endif

#ifndef __RIO_BUFFER_H__
#include <Network/Rio/RioBuffer.h>
#endif

#ifndef __RIO_SESSION_H__
#include <Network/Rio/RioSession.h>
#endif

#ifndef __RIO_SESSION_MANAGER_H__
#include <Network/Rio/RioSessionManager.h>
#endif

#include <atomic>
#include <thread>
#include <memory>
#include <limits>

class CRioSession;

//***************************************************************************
// @class CRioServer
// @brief RIO(Registered I/O) 엔진 기반의 네트워크 서버 메인 클래스
//***************************************************************************
class CRioServer
{
public:
    CRioServer();
    virtual ~CRioServer();

    CRioServer(const CRioServer&) = delete;
    CRioServer& operator=(const CRioServer&) = delete;

public:
    bool Start(uint16_t port, uint32_t maxSessions);
    void Stop();

    virtual CRioSessionRef CreateSession() = 0;

    CRioSessionManager& GetSessionManager() noexcept { return _sessionManager; }
    CRioCore& GetCore() noexcept { return _rioCore; }
    bool IsRunning() const noexcept { return _serverState.load(std::memory_order_acquire) == Rio::ServerState::Running; }

private:
    void AcceptLoop();
    RIO_RQ CreateRequestQueueForSocket(SOCKET clientSocket);

private:
    std::atomic<Rio::ServerState> _serverState{ Rio::ServerState::Created };    // 서버 라이프사이클 상태 관리용 원자적 변수

    CRioCore _rioCore;                                  // RIO Core 엔진
    CRioEventPool _eventPool;                           // RIO 이벤트 풀
    CRioBuffer _globalRecvBufferPool;                   // 글로벌 수신 버퍼 풀
    CRioBuffer _globalSendBufferPool;                   // 글로벌 송신 버퍼 풀
    CRioSessionManager _sessionManager;                 // 세션 매니저

    SOCKET _listenSocket{ INVALID_SOCKET };             // 리슨 소켓
    RIO_BUFFERID _sendBufferId{ RIO_INVALID_BUFFERID }; // 송신 RIO 버퍼 ID

    std::thread _acceptThread;                          // Accept 수신 전용 스레드
};

#endif // ndef __RIOSERVER_H__