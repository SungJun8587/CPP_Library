
//***************************************************************************
// IocpSessionManager.h : interface for the CIocpSessionManager class.
//
//***************************************************************************

#ifndef __IOCPSESSIONMANAGER_H__
#define __IOCPSESSIONMANAGER_H__

#ifndef __CLUSTERSPINMAP_H__
#include <Containers/Map/ClusterSpinMap.h>
#endif

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
    uint64_t GenerateSessionId();

    bool AddSession(uint64_t sessionId, CIocpSessionRef session);
    void RemoveSession(uint64_t sessionId);
    CIocpSessionRef FindSession(uint64_t sessionId) const;

    size_t GetSessionCount() const;
    void Broadcast(const void* data, uint16_t size);

    void BeginCloseAllSessions();
    bool AreAllSessionsClosed() const;
    void RemoveClosedSessions();

private:
    CClusterSpinMap<uint64_t, CIocpSessionRef, Iocp::kSessionClusterCnt> _sessions;         // SessionId를 키로 하고, 16개의 클러스터로 분산 처리하여 락 경합을 최소화하는 고성능 해시맵
    std::atomic<uint64_t> _nextSessionId{ 0 };                                              // 세션 ID 자동 증가 카운터
};

#endif // ndef __IOCPSESSIONMANAGER_H__