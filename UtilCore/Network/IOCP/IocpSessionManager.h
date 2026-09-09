
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
    // [수정] AddSession()/RemoveSession()/FindSession()이 전부 sessionId==0을
    // "무효한 값"으로 취급해 거부한다(0을 sentinel로 쓰는 관례). 그런데
    // GenerateSessionId()가 fetch_add(1)의 반환값(증가 *전* 값)을 그대로
    // 돌려주므로, 이 카운터가 0에서 시작하면 맨 처음 발급되는 ID가 정확히
    // 0이 되어 AddSession()에서 거부당한다 — 그 결과 첫 번째로 접속한
    // 세션이 세션 맵에 아예 등록되지 못해 Broadcast() 등 맵을 순회하는
    // 모든 기능에서 조용히 빠지는 사고로 이어졌다(실제 재현 사례:
    // 맨 처음 접속한 클라이언트만 다른 사람 메시지를 못 받음). 1부터
    // 시작하면 fetch_add(1)이 절대 0을 반환하지 않는다.
    std::atomic<uint64> _nextSessionId{ 1 };                                                  // 세션 ID 자동 증가 카운터 — 0은 "무효 ID"로 예약되어 있으므로 1부터 시작
};

#endif // ndef UC_IOCPSESSIONMANAGER_H