
//***************************************************************************
// IocpSessionManager.h : interface for the CIocpSessionManager class.
//
//***************************************************************************

#ifndef UC_IOCPSESSIONMANAGER_H
#define UC_IOCPSESSIONMANAGER_H

#include <Network/IOCP/IocpCommon.h>
#include <Network/IOCP/IocpSession.h>
#include <Containers/Map/ClusterSpinUnorderedMap.h>

#include <memory>
#include <atomic>
#include <utility>
#include <vector>

//***************************************************************************
// @class CIocpSessionManager
// @brief CClusterSpinMap을 활용하여 락 경합을 최소화한 IOCP 세션 매니저 클래스
// @details 소켓 재사용 시 발생할 수 있는 레이스 컨디션을 방지하기 위해 SOCKET 
//          대신 SessionId를 키로 관리합니다.
//***************************************************************************
class CIocpSessionManager
{
public:
    CIocpSessionManager();
    ~CIocpSessionManager();

    CIocpSessionManager(const CIocpSessionManager&) = delete;
    CIocpSessionManager& operator=(const CIocpSessionManager&) = delete;

    CIocpSessionManager(CIocpSessionManager&&) = delete;
    CIocpSessionManager& operator=(CIocpSessionManager&&) = delete;

public:
    uint64 GenerateSessionId();

    bool AddSession(uint64 sessionId, CIocpSessionRef session);
    void RemoveSession(uint64 sessionId);
    CIocpSessionRef FindSession(uint64 sessionId) const;

    size_t GetSessionCount() const;
    void Broadcast(const void* data, uint16 size);

    void BeginCloseAllSessions();
    bool AreAllSessionsClosed() const;
    void RemoveClosedSessions();

private:
    using SessionMap = CClusterSpinUnorderedMap<uint64, CIocpSessionRef, Iocp::kSessionClusterCnt>;

    mutable SessionMap _sessions;    // SessionId를 키로 하고, 16개의 클러스터로 분산 처리하여 락 경합을 최소화하는 고성능 해시맵
    // AddSession()/RemoveSession()/FindSession()은 sessionId==0을 "무효한 값"으로 취급해 거부한다
    // (0을 sentinel로 쓰는 관례). GenerateSessionId()는 fetch_add(1)의 반환값(증가 *전* 값)을
    // 돌려주므로 카운터를 1부터 시작해야 첫 발급 ID가 0이 되지 않는다.
    std::atomic<uint64> _nextSessionId{ 1 };                                                  // 세션 ID 자동 증가 카운터 — 0은 "무효 ID"로 예약되어 있으므로 1부터 시작
};

#endif // ndef UC_IOCPSESSIONMANAGER_H