
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
// @struct IocpSessionEntry
// @brief 세션 관리 맵에서 세션의 고유 식별자(ID)와 스마트 포인터를 함께 묶어 관리하기 위한 엔트리 구조체
// @details 소켓 재사용 시 발생할 수 있는 레이스 컨디션(동일 소켓에 대한 이전 세션 잔재 처리)을 방지하기 위해 sessionId를 동반합니다.
//***************************************************************************
struct IocpSessionEntry
{
    uint64_t sessionId{ 0 };                // 세션 고유 ID (재사용 검증용)
    CIocpSessionRef session;  // 세션 객체의 공유 스마트 포인터
};

//***************************************************************************
// @class CIocpSessionManager
// @brief CClusterSpinMap을 활용하여 락 경합을 최소화한 IOCP 세션 매니저 클래스
//***************************************************************************
class CIocpSessionManager
{
public:
    //***************************************************************************
    // @brief 싱글톤 패턴을 적용하여 전역에서 단 하나의 CIocpSessionManager 인스턴스에 접근하도록 반환합니다.
    // @return CIocpSessionManager& 매니저 싱글톤 인스턴스 참조
    //***************************************************************************
    static CIocpSessionManager& Instance()
    {
        static CIocpSessionManager instance;
        return instance;
    }

    CIocpSessionManager();
    ~CIocpSessionManager();

    CIocpSessionManager(const CIocpSessionManager&) = delete;
    CIocpSessionManager& operator=(const CIocpSessionManager&) = delete;

public:
    uint64_t GenerateSessionId();

    bool AddSession(SOCKET socket, uint64_t sessionId, CIocpSessionRef session);
    void RemoveSession(SOCKET socket, uint64_t sessionId);
    CIocpSessionRef FindSession(SOCKET socket) const;

    size_t GetSessionCount() const;
    void Broadcast(const void* data, uint16_t size);

    void BeginCloseAllSessions();
    bool AreAllSessionsClosed() const;
    void RemoveClosedSessions();

private:
    CClusterSpinMap<SOCKET, IocpSessionEntry, Iocp::kSessionClusterCnt> _sessions;      // SOCKET을 키로 하고, 16개의 클러스터(nClusterCnt = 16)로 분산 처리하여 락 경합을 최소화하는 고성능 해시맵
    std::atomic<uint64_t> _nextSessionId{ 0 };                                          // 세션 ID 자동 증가 카운터
};

#endif // ndef __IOCPSESSIONMANAGER_H__