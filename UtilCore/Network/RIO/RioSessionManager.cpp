
//***************************************************************************
// RioSessionManager.cpp : implementation of the CRioSessionManager class.
//
//***************************************************************************

#include "pch.h"
#include "RioSessionManager.h"

//***************************************************************************
// @brief CRioSessionManager 생성자
// @param core 세션 매니저가 연동될 CRioCore 인스턴스의 레퍼런스
// @note
//      CRioCore의 소유권은 매니저가 가지지 않으므로, 매니저의 수명 동안
//      유효한 CRioCore 객체임이 보장되어야 합니다.
//***************************************************************************
CRioSessionManager::CRioSessionManager(CRioCore& core) noexcept
    : _core(&core)
    , _sessionMutex()
    , _sessions()
    , _closing(false)
{
}

//***************************************************************************
// @brief CRioSessionManager 소멸자
// @note
//      소멸 시 CloseAll()을 호출하여 등록된 모든 세션에 Close()를 요청합니다.
//      Outstanding I/O가 남아있는 세션은 비동기 완료 시점까지 객체 수명이 유지될 수 있습니다.
//***************************************************************************
CRioSessionManager::~CRioSessionManager() noexcept
{
    CloseAll();
}

//***************************************************************************
// @brief 레지스트리에 새로운 세션을 등록합니다.
// @param sessionId 등록할 세션의 고유 식별자
// @param session 등록할 세션의 shared_ptr
// @return 등록 성공 시 true, 검증 실패 또는 중복 ID 존재 시 false
// @note
//      - 유효한 SessionId 및 non-null 세션 객체여야 합니다.
//      - 세션은 반드시 Active 상태여야 하며, 동일한 CRioCore 참조를 가져야 합니다.
//      - 매니저가 종료 중(_closing == true)인 경우 등록이 거부됩니다.
//***************************************************************************
bool CRioSessionManager::RegisterSession(SessionId sessionId, const SessionPtr& session) noexcept
{
    if( !IsValidSessionId(sessionId) )
    {
        return false;
    }

    if( session == nullptr )
    {
        return false;
    }

    if( _core == nullptr )
    {
        return false;
    }

    if( _closing.load(std::memory_order_acquire) )
    {
        return false;
    }

    if( !session->IsActive() )
    {
        return false;
    }

    if( session->GetCore() != _core )
    {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(_sessionMutex);

    if( _closing.load(std::memory_order_acquire) )
    {
        return false;
    }

    const auto [it, inserted] = _sessions.emplace(sessionId, session);

    return inserted;
}

//***************************************************************************
// @brief 지정된 세션을 레지스트리에서 제거합니다.
// @param sessionId 레지스트리에서 제거할 세션 식별자
// @return 세션 제거 성공 시 true, 미존재/미종료/진행중인 I/O 잔여 시 false
// @note
//      - 세션이 IsClosed() 상태여야 합니다.
//      - HasOutstandingIo()가 false여야(진행 중인 I/O가 없어야) 최종 제거됩니다.
//***************************************************************************
bool CRioSessionManager::RemoveSession(SessionId sessionId) noexcept
{
    if( !IsValidSessionId(sessionId) )
    {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(_sessionMutex);

    const auto it = _sessions.find(sessionId);

    if( it == _sessions.end() )
    {
        return false;
    }

    const SessionPtr& session = it->second;

    if( session == nullptr )
    {
        _sessions.erase(it);
        return true;
    }

    if( !session->IsClosed() )
    {
        return false;
    }

    if( session->HasOutstandingIo() )
    {
        return false;
    }

    _sessions.erase(it);

    return true;
}

//***************************************************************************
// @brief 식별자로 세션을 검색합니다.
// @param sessionId 검색할 세션 식별자
// @return 검색된 세션의 shared_ptr (존재하지 않을 경우 nullptr)
// @note
//      shared_lock을 사용하여 동시 읽기 접근을 보호합니다.
//      반환된 shared_ptr을 보유하고 있는 동안 세션 객체의 수명이 안전하게 보장됩니다.
//***************************************************************************
CRioSessionManager::SessionPtr CRioSessionManager::FindSession(SessionId sessionId) noexcept
{
    if( !IsValidSessionId(sessionId) )
    {
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    const auto it = _sessions.find(sessionId);

    if( it == _sessions.end() )
    {
        return nullptr;
    }

    return it->second;
}

//***************************************************************************
// @brief 식별자로 const 세션을 검색합니다. (const 오버로딩)
// @param sessionId 검색할 세션 식별자
// @return 검색된 const 세션의 shared_ptr (존재하지 않을 경우 nullptr)
//***************************************************************************
CRioSessionManager::ConstSessionPtr CRioSessionManager::FindSession(SessionId sessionId) const noexcept
{
    if( !IsValidSessionId(sessionId) )
    {
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    const auto it = _sessions.find(sessionId);

    if( it == _sessions.end() )
    {
        return nullptr;
    }

    return it->second;
}

//***************************************************************************
// @brief 특정 세션이 레지스트리에 존재하는지 확인합니다.
// @param sessionId 존재 여부를 확인할 세션 식별자
// @return 존재하면 true, 없으면 false
//***************************************************************************
bool CRioSessionManager::Contains(SessionId sessionId) const noexcept
{
    if( !IsValidSessionId(sessionId) )
    {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    return _sessions.find(sessionId) != _sessions.end();
}

//***************************************************************************
// @brief 식별자에 해당하는 세션의 종료(Close)를 요청합니다.
// @param sessionId 종료할 세션 식별자
// @return 세션을 찾아 Close() 요청을 성공적으로 전달했으면 true, 없으면 false
// @note
//      데드락 및 재진입 문제를 방지하기 위해 매니저 락(_sessionMutex)을 해제한 상태에서
//      세션의 Close() 함수를 호출합니다.
//***************************************************************************
bool CRioSessionManager::CloseSession(SessionId sessionId) noexcept
{
    SessionPtr session;

    {
        std::shared_lock<std::shared_mutex> lock(_sessionMutex);

        const auto it = _sessions.find(sessionId);

        if( it == _sessions.end() )
        {
            return false;
        }

        session = it->second;
    }

    if( session == nullptr )
    {
        return false;
    }

    session->Close();

    return true;
}

//***************************************************************************
// @brief 세션 shared_ptr 참조를 통해 종료(Close)를 요청합니다.
// @param session 종료 요청을 전달할 세션 객체의 shared_ptr
// @return 유효한 세션 객체여서 Close()를 호출했으면 true, nullptr이면 false
//***************************************************************************
bool CRioSessionManager::CloseSession(const SessionPtr& session) noexcept
{
    if( session == nullptr )
    {
        return false;
    }

    session->Close();

    return true;
}

//***************************************************************************
// @brief 레지스트리에 관리 중인 모든 세션에 대해 종료(Close)를 요청합니다.
// @note
//      1. _closing 플래그를 true로 변경하여 신규 세션 등록을 차단합니다.
//      2. 락 내부에서는 세션 pointer들의 스냅샷(vector)만 안전하게 복사합니다.
//      3. 락을 해제한 후 스냅샷을 순회하며 각 세션의 Close()를 호출하여 데드락을 방지합니다.
//***************************************************************************
void CRioSessionManager::CloseAll() noexcept
{
    std::vector<SessionPtr> sessions;

    {
        std::unique_lock<std::shared_mutex> lock(_sessionMutex);

        _closing.store(true, std::memory_order_release);

        sessions.reserve(_sessions.size());

        for( const auto& [sessionId, session] : _sessions )
        {
            if( session != nullptr )
            {
                sessions.push_back(session);
            }
        }
    }

    for( const SessionPtr& session : sessions )
    {
        if( session != nullptr )
        {
            session->Close();
        }
    }
}

//***************************************************************************
// @brief Closed 상태이며 Outstanding I/O가 완전히 정리된 세션들을 일괄 제거합니다.
// @note
//      레지스트리 맵에서만 제거되며, 타 객체나 완료 이벤트에서 참조 중인 shared_ptr이
//      남아있는 경우 해당 참조까지 해제되는 시점에 실제 소멸자가 호출됩니다.
//***************************************************************************
void CRioSessionManager::RemoveClosedSessions() noexcept
{
    std::unique_lock<std::shared_mutex> lock(_sessionMutex);

    for( auto it = _sessions.begin(); it != _sessions.end(); )
    {
        const SessionPtr& session = it->second;

        if( session == nullptr )
        {
            it = _sessions.erase(it);
            continue;
        }

        if( session->IsClosed() && !session->HasOutstandingIo() )
        {
            it = _sessions.erase(it);
            continue;
        }

        ++it;
    }
}

//***************************************************************************
// @brief 현재 매니저 레지스트리에 등록된 전체 세션 개수를 반환합니다.
// @return 등록된 세션의 총 개수
//***************************************************************************
size_t CRioSessionManager::GetSessionCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    return _sessions.size();
}

//***************************************************************************
// @brief 현재 등록된 세션 중 Active 상태인 세션의 개수를 반환합니다.
// @return Active 상태 세션 수
//***************************************************************************
size_t CRioSessionManager::GetActiveSessionCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    size_t count = 0;

    for( const auto& [sessionId, session] : _sessions )
    {
        if( session != nullptr && session->IsActive() )
        {
            ++count;
        }
    }

    return count;
}

//***************************************************************************
// @brief 현재 등록된 세션 중 Closing(종료 진행 중) 상태인 세션의 개수를 반환합니다.
// @return Closing 상태 세션 수
//***************************************************************************
size_t CRioSessionManager::GetClosingSessionCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    size_t count = 0;

    for( const auto& [sessionId, session] : _sessions )
    {
        if( session != nullptr && session->IsClosing() )
        {
            ++count;
        }
    }

    return count;
}

//***************************************************************************
// @brief 현재 등록된 세션 중 Closed(종료 완료) 상태인 세션의 개수를 반환합니다.
// @return Closed 상태 세션 수
//***************************************************************************
size_t CRioSessionManager::GetClosedSessionCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(_sessionMutex);

    size_t count = 0;

    for( const auto& [sessionId, session] : _sessions )
    {
        if( session != nullptr && session->IsClosed() )
        {
            ++count;
        }
    }

    return count;
}

//***************************************************************************
// @brief 특정 세션의 존재 여부를 확인합니다. (Contains의 래퍼 함수)
// @param sessionId 존재 확인할 세션 식별자
// @return 존재 시 true, 미존재 시 false
//***************************************************************************
bool CRioSessionManager::HasSession(SessionId sessionId) const noexcept
{
    return Contains(sessionId);
}

//***************************************************************************
// @brief 매니저가 참조 중인 CRioCore 포인터를 반환합니다.
// @return CRioCore 포인터
//***************************************************************************
CRioCore* CRioSessionManager::GetCore() const noexcept
{
    return _core;
}

//***************************************************************************
// @brief 전달된 SessionId가 유효한 범위인지 검증합니다.
// @param sessionId 검증할 세션 식별자
// @return 유효한 ID이면 true (0이 아닌 값)
//***************************************************************************
bool CRioSessionManager::IsValidSessionId(SessionId sessionId) noexcept
{
    return sessionId != 0;
}