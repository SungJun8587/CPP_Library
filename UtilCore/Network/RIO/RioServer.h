
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
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

//***************************************************************************
// @brief Windows RIO(Registered I/O) 기반의 서버 클래스
//
// @details
//      클라이언트 접속 수락(Accept Loop)을 전담하는 독립적인 워커 스레드를 관리하며,
//      접속된 클라이언트 소켓별로 RIO Request Queue(RIO_RQ)를 할당하고
//      세션 객체(CRioSession)의 라이프사이클을 총괄합니다.
//
// [클라이언트 연결 수락 및 세션 생성]
//      Non-blocking 모드의 Listen 소켓을 기반으로 AcceptLoop()를 구동합니다.
//      연결된 소켓은 CRioCore의 RIO Completion Queue(CQ)와 연결되는 RIO_RQ를
//      생성받고 CRioSessionManager에 관리 대상으로 등록됩니다.
//
// [스레드 안전성 및 라이프사이클 동기화]
//      서버의 상태 변화(Initialize/Start/Stop)는 _lifecycleMutex로 보호되며,
//      세션별 RIO_RQ 리소스 맵(_requestQueues)은 _sessionResourceMutex를 통해
//      스레드 세이프하게 관리됩니다.
//
// [안전한 자원 해제 및 종료 시퀀스]
//      Stop() 호출 시 Accept 스레드를 중단하고 모든 세션의 소켓을 shutdown하여
//      진행 중인 비동기 I/O를 취소(Cancel/Abort)시킵니다. 이후 CRioCore의
//      Completion Drain을 거쳐 Outstanding I/O가 0이 된 시점에 RIO_RQ를
//      안전하게 파괴합니다.
//***************************************************************************
class CRioServer final
{
public:
    using SessionId = CRioSessionManager::SessionId;
    using SessionPtr = CRioSessionManager::SessionPtr;

public:
    CRioServer(CRioCore& core, CRioEventPool& eventPool, CRioBuffer& receiveBuffer) noexcept;
    ~CRioServer() noexcept;

    CRioServer(const CRioServer&) = delete;
    CRioServer& operator=(const CRioServer&) = delete;

    CRioServer(CRioServer&&) = delete;
    CRioServer& operator=(CRioServer&&) = delete;

public:
    bool Initialize(const sockaddr_in& address, int backlog = SOMAXCONN) noexcept;
    bool Start() noexcept;
    void Stop(std::chrono::milliseconds drainTimeout = std::chrono::milliseconds(5000)) noexcept;

public:
    bool IsRunning() const noexcept;
    bool IsInitialized() const noexcept;

public:
    SOCKET GetListenSocket() const noexcept;
    CRioCore* GetCore() const noexcept;
    CRioEventPool* GetEventPool() const noexcept;
    CRioBuffer* GetReceiveBuffer() const noexcept;

    CRioSessionManager* GetSessionManager() noexcept;
    const CRioSessionManager* GetSessionManager() const noexcept;

public:
    size_t GetSessionCount() const noexcept;
    size_t GetActiveSessionCount() const noexcept;
    size_t GetClosingSessionCount() const noexcept;
    size_t GetClosedSessionCount() const noexcept;

private:
    bool CreateListenSocket(const sockaddr_in& address, int backlog) noexcept;
    bool ConfigureListenSocket() noexcept;

private:
    void AcceptLoop() noexcept;
    bool AcceptOne() noexcept;

private:
    bool CreateRequestQueue(SOCKET socket, RIO_RQ& outRequestQueue) noexcept;
    bool CreateSession(SOCKET socket, RIO_RQ requestQueue, SessionPtr& outSession) noexcept;
    bool StartInitialReceive(const SessionPtr& session) noexcept;

private:
    bool AllocateReceiveEvent(
        const SessionPtr& session,
        uint32_t& outSlotIndex,
        RIO_BUF& outBuffer,
        CRioEvent*& outEvent) noexcept;

private:
    void CleanupClosedSessions() noexcept;
    void CloseSessionSocket(const SessionPtr& session) noexcept;
    bool DestroyRequestQueue(RIO_RQ requestQueue) noexcept;

private:
    void CloseListenSocket() noexcept;
    void CloseSocket(SOCKET socket) noexcept;

private:
    static SessionId GenerateSessionId(std::atomic<SessionId>& counter) noexcept;

private:
    CRioCore* _core;                                // RIO Core 엔진 포인터
    CRioEventPool* _eventPool;                      // RIO Event 재사용 풀
    CRioBuffer* _receiveBuffer;                    // 수신 전용 등록 버퍼

    CRioSessionManager _sessionManager;             // 세션 관리자

    mutable std::mutex _lifecycleMutex;            // 서버 시작/종료 제어 뮤텍스
    mutable std::mutex _sessionResourceMutex;       // 세션별 RIO_RQ 매핑 자원 보호 뮤텍스

    SOCKET _listenSocket;                           // 연결 수락용 소켓
    sockaddr_in _listenAddress;                     // 바인딩된 바인드 주소
    int _backlog;                                   // Listen 연결 대기열 크기

    std::atomic<bool> _initialized;                 // 서버 초기화 상태 플래그
    std::atomic<bool> _running;                     // 서버 실행 상태 플래그

    std::thread _acceptThread;                      // Accept 전용 스레드

    std::atomic<SessionId> _nextSessionId;          // 세션 식별자 카운터
    std::unordered_map<SessionId, RIO_RQ> _requestQueues; // 세션 ID별 RIO_RQ 추적 맵
};

#endif // __RIOSERVER_H__