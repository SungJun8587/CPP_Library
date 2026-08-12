
//***************************************************************************
// RioSessionManager.h : interface for the CRioSessionManager class.
//
//***************************************************************************

#ifndef __RIOSESSIONMANAGER_H__
#define __RIOSESSIONMANAGER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIO_OBJECT_H__
#include <Network/Rio/RioObject.h>
#endif

#ifndef __RIO_SESSION_H__
#include <Network/Rio/RioSession.h>
#endif

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class CRioCore;
class CRioSession;

//***************************************************************************
// @class CRioSessionManager
// @brief RIO Session의 생성, 등록, 조회, 종료 및 제거 등 전체 라이프사이클을 총괄 관리하는 매니저 클래스입니다.
//
// @details
//      CRioSessionManager는 세션의 네트워크 I/O 전송 자체를 직접 제어하지 않으며,
//      세션 객체의 등록 및 소유권, 상태 변화에 따른 라이프사이클 관리를 전담합니다.
//
//      [주요 정책 및 특성]
//      - Ownership: 세션은 std::shared_ptr로 레지스트리에 관리됩니다.
//      - Lifetime: 진행 중인 I/O(Outstanding I/O)가 존재하는 동안은 레지스트리에서 제거되어도
//        비동기 이벤트(CRioEvent)가 shared_ptr을 보유하므로 즉시 파괴되지 않고 안전하게 유지됩니다.
//      - Safe Closing: CloseAll() 및 CloseSession()은 매니저 락을 해제한 상태에서 진행하여
//        상태 변경 콜백 함수에서의 재진입 및 데드락을 방지합니다.
//      - Safe Removal: Closed 상태이면서 Outstanding I/O가 0인 세션만 최종 레지스트리에서 제거됩니다.
//      - Thread Safety: std::shared_mutex를 활용하여 Read(조회) 작업 시 shared_lock, Write(등록/제거) 작업 시 unique_lock을 사용합니다.
//***************************************************************************
class CRioSessionManager final
{
public:
    using SessionId = uint64_t;
    using SessionPtr = std::shared_ptr<CRioSession>;
    using ConstSessionPtr = std::shared_ptr<const CRioSession>;

public:
    explicit CRioSessionManager(CRioCore& core) noexcept;
    ~CRioSessionManager() noexcept;

    CRioSessionManager(const CRioSessionManager&) = delete;
    CRioSessionManager& operator=(const CRioSessionManager&) = delete;

    CRioSessionManager(CRioSessionManager&&) = delete;
    CRioSessionManager& operator=(CRioSessionManager&&) = delete;

public:
    template <typename T, typename... Args>
    std::shared_ptr<T> CreateSession(SessionId sessionId, SOCKET socket, RIO_RQ requestQueue, Args&&... args) noexcept;

    bool RegisterSession(SessionId sessionId, const SessionPtr& session) noexcept;

    bool RemoveSession(SessionId sessionId) noexcept;

public:
    SessionPtr FindSession(SessionId sessionId) noexcept;
    ConstSessionPtr FindSession(SessionId sessionId) const noexcept;

    bool Contains(SessionId sessionId) const noexcept;

public:
    bool CloseSession(SessionId sessionId) noexcept;
    bool CloseSession(const SessionPtr& session) noexcept;

    void CloseAll() noexcept;
    void RemoveClosedSessions() noexcept;

public:
    size_t GetSessionCount() const noexcept;
    size_t GetActiveSessionCount() const noexcept;
    size_t GetClosingSessionCount() const noexcept;
    size_t GetClosedSessionCount() const noexcept;
    bool HasSession(SessionId sessionId) const noexcept;

public:
    CRioCore* GetCore() const noexcept;

public:
    static bool IsValidSessionId(SessionId sessionId) noexcept;

private:
    using SessionMap = std::unordered_map<SessionId, SessionPtr>;

private:
    CRioCore* _core;                         // RIO Core 객체 참조 포인터 (매니저가 소유권을 가지지 않음)
    mutable std::shared_mutex _sessionMutex; // 세션 레지스트리 접근 동기화를 위한 shared_mutex
    SessionMap _sessions;                    // SessionId를 키로 사용하는 RIO 세션 관리 맵
    std::atomic<bool> _closing;              // 매니저 종료 절차 진행 여부 플래그
};

//***************************************************************************
// @brief 템플릿 기반 세션 생성 및 초기화 함수
// @tparam T CRioSession을 상속받은 세션 클래스 타입
// @tparam Args 세션 생성자에 전달할 인자 패킷 타입
// @param sessionId 세션에 부여할 고유 식별자 (0 제외)
// @param socket 세션에 바인딩할 네트워크 소켓 핸들
// @param requestQueue RIO 요청 큐 핸들
// @param args 세션 인스턴스 생성자 전달 인자
// @return 생성 및 초기화, 등록에 성공한 세션의 shared_ptr (실패 시 nullptr)
// @note
//      1. T는 반드시 CRioSession의 파생 클래스여야 합니다.
//      2. std::make_shared를 사용하여 객체를 생성하므로 shared_from_this() 사용이 안전합니다.
//      3. Initialize() 실패 또는 RegisterSession() 실패 시 생성된 세션은 즉시 폐기됩니다.
//***************************************************************************
template <typename T, typename... Args>
std::shared_ptr<T> CRioSessionManager::CreateSession(SessionId sessionId, SOCKET socket, RIO_RQ requestQueue, Args&&... args) noexcept
{
    static_assert(std::is_base_of_v<CRioSession, T>, "T must derive from CRioSession");

    if( !IsValidSessionId(sessionId) )
    {
        return nullptr;
    }

    if( socket == INVALID_SOCKET || requestQueue == RIO_INVALID_RQ )
    {
        return nullptr;
    }

    if( _closing.load(std::memory_order_acquire) )
    {
        return nullptr;
    }

    std::shared_ptr<T> session;

    try
    {
        session = std::make_shared<T>(std::forward<Args>(args)...);
    }
    catch( ... )
    {
        return nullptr;
    }

    if( !session->Initialize(*_core, socket, requestQueue) )
    {
        return nullptr;
    }

    if( !RegisterSession(sessionId, session) )
    {
        session->Close();
        return nullptr;
    }

    return session;
}

#endif // __RIOSESSIONMANAGER_H__