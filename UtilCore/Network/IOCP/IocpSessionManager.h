
//***************************************************************************
// IocpSessionManager.h : interface for the CIocpSessionManager class.
//
//***************************************************************************

#ifndef UC_IOCPSESSIONMANAGER_H
#define UC_IOCPSESSIONMANAGER_H

#include <Network/IOCP/IocpCommon.h>
#include <Containers/Map/ClusterSpinUnorderedMap.h>

#include <memory>
#include <atomic>
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

    CIocpSessionManager(CIocpSessionManager&&) noexcept = default;
    CIocpSessionManager& operator=(CIocpSessionManager&&) noexcept = default;

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
    CClusterSpinUnorderedMap<uint64, CIocpSessionRef, Iocp::kSessionClusterCnt> _sessions;    // SessionId를 키로 하고, 16개의 클러스터로 분산 처리하여 락 경합을 최소화하는 고성능 해시맵
    std::atomic<uint64> _nextSessionId{ 0 };                                                  // 세션 ID 자동 증가 카운터
};

#endif // ndef UC_IOCPSESSIONMANAGER_H