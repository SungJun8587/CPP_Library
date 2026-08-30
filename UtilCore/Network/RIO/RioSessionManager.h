
//***************************************************************************
// RioSessionManager.h : interface for the CRioSessionManager class.
//
//***************************************************************************

#ifndef UC_RIOSESSIONMANAGER_H
#define UC_RIOSESSIONMANAGER_H

#include <Network/RIO/RioCommon.h>
#include <Containers/Map/ClusterSpinUnorderedMap.h>

#include <vector>
#include <memory>
#include <atomic>
#include <functional>

class CRioSession;

//***************************************************************************
// @class CRioSessionManager
// @brief CClusterSpinMap을 활용하여 락 경합을 최소화한 RIO 세션 매니저 클래스
// @details 소켓 재사용 시 발생할 수 있는 레이스 컨디션을 방지하기 위해 SOCKET 
//			대신 SessionId를 키로 관리합니다.
//***************************************************************************
class CRioSessionManager
{
public:
	CRioSessionManager();
	~CRioSessionManager();

	CRioSessionManager(const CRioSessionManager&) = delete;
	CRioSessionManager& operator=(const CRioSessionManager&) = delete;

	CRioSessionManager(CRioSessionManager&&) noexcept = default;
	CRioSessionManager& operator=(CRioSessionManager&&) noexcept = default;

public:
	uint64 GenerateSessionId();

	bool AddSession(uint64 sessionId, CRioSessionRef session);
	void RemoveSession(uint64 sessionId);
	CRioSessionRef FindSession(uint64 sessionId) const;

	size_t GetSessionCount() const;
	void Broadcast(const void* data, uint16 size);

	void BeginCloseAllSessions();
	bool AreAllSessionsClosed() const;
	void RemoveClosedSessions();

private:
	CClusterSpinUnorderedMap<uint64, CRioSessionRef, Rio::kSessionClusterCnt, true> _sessions;	// SessionId를 키로 하고, 클러스터별로 분산 처리하여 락 경합을 최소화하는 고성능 해시맵
	std::atomic<uint64> _nextSessionId{ 0 };														// 세션 ID 자동 증가 카운터
};

#endif // ndef UC_RIOSESSIONMANAGER_H