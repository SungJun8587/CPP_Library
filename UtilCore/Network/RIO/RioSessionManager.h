
//***************************************************************************
// RioSessionManager.h : interface for the CRioSessionManager class.
//
//***************************************************************************

#ifndef __RIOSESSIONMANAGER_H__
#define __RIOSESSIONMANAGER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __PLATFORMLOCK_H__
#include <Thread/PlatformLock.h>
#endif

#ifndef __RIO_SESSION_H__
#include <Network/Rio/RioSession.h>
#endif

#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>

class CRioSession;

struct SessionEntry
{
    uint64_t sessionId{ 0 };
    std::shared_ptr<CRioSession> session;
};

//***************************************************************************
// @class CRioSessionManager
// @brief 네트워크 세션 생성, 관리, 검색 및 전체 종료를 총괄하는 매니저 클래스
//***************************************************************************
class CRioSessionManager
{
public:
    CRioSessionManager();
    ~CRioSessionManager();

    CRioSessionManager(const CRioSessionManager&) = delete;
    CRioSessionManager& operator=(const CRioSessionManager&) = delete;

public:
    uint64_t GenerateSessionId();

    bool AddSession(SOCKET socket, uint64_t sessionId, std::shared_ptr<CRioSession> session);
    void RemoveSession(SOCKET socket, uint64_t sessionId);
    std::shared_ptr<CRioSession> FindSession(SOCKET socket) const;

    void BeginCloseAllSessions();
    bool AreAllSessionsClosed() const;
    void RemoveClosedSessions();
    size_t GetSessionCount() const;

private:
    mutable PLock _lock;                                        // 동기화 플랫폼 락
    std::unordered_map<SOCKET, SessionEntry> _sessions;         // 활성 세션 해시맵 (Socket 키 기준)
    std::atomic<uint64_t> _nextSessionId{ 0 };                  // 세션 ID 자동 증가 카운터
};

#endif // ndef __RIOSESSIONMANAGER_H__