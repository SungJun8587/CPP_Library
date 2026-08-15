
//***************************************************************************
// RioSessionManager.h : interface for the CRioSessionManager class.
//
//***************************************************************************

#ifndef __RIOSESSIONMANAGER_H__
#define __RIOSESSIONMANAGER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __CLUSTERSPINMAP_H__
#include <Network/ClusterSpinMap.h>
#endif

#ifndef __RIO_SESSION_H__
#include <Network/Rio/RioSession.h>
#endif

#include <vector>
#include <memory>
#include <atomic>
#include <functional>

class CRioSession;

//***************************************************************************
// @struct RioSessionEntry
// @brief 세션 관리 맵에 저장될 엔트리 구조체
// @details
//      - 소켓 재사용 레이스 컨디션을 방지하기 위한 고유 세션 ID(`sessionId`)와
//        실제 세션 객체를 가리키는 `CRioSessionRef`을 함께 보관합니다.
//***************************************************************************
struct RioSessionEntry
{
    uint64_t sessionId{ 0 };                 // 세션 고유 식별자 (소켓 재사용 레이스 방지용)
    CRioSessionRef session;    // 세션 객체 스마트 포인터
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

    bool AddSession(SOCKET socket, uint64_t sessionId, CRioSessionRef session);
    void RemoveSession(SOCKET socket, uint64_t sessionId);
    CRioSessionRef FindSession(SOCKET socket) const;

    size_t GetSessionCount() const;
    void Broadcast(const void* data, uint16_t size);

    void BeginCloseAllSessions();
    bool AreAllSessionsClosed() const;
    void RemoveClosedSessions();

private:
    CClusterSpinMap<SOCKET, RioSessionEntry, Rio::kSessionClusterCnt, true> _sessions; // 클러스터 맵 기반 활성 세션 저장소 (Socket 키 기준)
    std::atomic<uint64_t> _nextSessionId{ 0 };                 // 세션 ID 자동 증가 카운터
};

#endif // ndef __RIOSESSIONMANAGER_H__