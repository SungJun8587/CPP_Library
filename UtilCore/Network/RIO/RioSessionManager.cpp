//***************************************************************************
// RioSessionManager.cpp : implementation of the CRioSessionManager class.
//
//***************************************************************************

#include "pch.h"
#include "RioSessionManager.h"

//***************************************************************************
// @brief CRioSessionManager 생성자
//***************************************************************************
CRioSessionManager::CRioSessionManager()
    : _nextSessionId(0)
{
}

//***************************************************************************
// @brief CRioSessionManager 소멸자
//***************************************************************************
CRioSessionManager::~CRioSessionManager()
{
}

//***************************************************************************
// @brief 원자적으로 새로운 고유 SessionId를 발급합니다.
// @return uint64_t 고유 세션 ID
//***************************************************************************
uint64_t CRioSessionManager::GenerateSessionId()
{
    return _nextSessionId.fetch_add(1, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 세션을 매니저에 등록합니다.
// @param socket 소켓 키
// @param sessionId 고유 세션 ID
// @param session 세션 shared_ptr
// @return 등록 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSessionManager::AddSession(SOCKET socket, uint64_t sessionId, std::shared_ptr<CRioSession> session)
{
    if( socket == INVALID_SOCKET || session == nullptr ) return false;

    PLockGuard guard(_lock, "CRioSessionManager_AddSession");

    // [수정] 구조적 바인딩 컴파일 오류 방지를 위해 전통적 pair 인서트 결과 처리로 변경
    auto insertResult = _sessions.emplace(socket, SessionEntry{ sessionId, session });
    return insertResult.second;
}

//***************************************************************************
// @brief 소켓 재사용 레이스 방지를 위해 세션 ID를 함께 검증하여 매니저에서 제거합니다.
// @param socket 제거할 소켓 키
// @param sessionId 검증할 고유 세션 ID
//***************************************************************************
void CRioSessionManager::RemoveSession(SOCKET socket, uint64_t sessionId)
{
    if( socket == INVALID_SOCKET ) return;

    PLockGuard guard(_lock, "CRioSessionManager_RemoveSession");
    auto it = _sessions.find(socket);
    if( it != _sessions.end() && it->second.sessionId == sessionId )
    {
        _sessions.erase(it);
    }
}

//***************************************************************************
// @brief 소켓 키 기반으로 유효한 세션을 검색합니다.
// @param socket 찾을 소켓 키
// @return 세션 shared_ptr (존재하지 않을 경우 nullptr)
//***************************************************************************
std::shared_ptr<CRioSession> CRioSessionManager::FindSession(SOCKET socket) const
{
    if( socket == INVALID_SOCKET ) return nullptr;

    PLockGuard guard(_lock, "CRioSessionManager_FindSession");
    auto it = _sessions.find(socket);
    if( it != _sessions.end() )
    {
        return it->second.session;
    }
    return nullptr;
}

//***************************************************************************
// @brief 락 내부에서 스냅샷을 수집한 뒤 락 밖에서 안전하게 Close를 브로드캐스트합니다.
//***************************************************************************
void CRioSessionManager::BeginCloseAllSessions()
{
    std::vector<std::shared_ptr<CRioSession>> sessionsToClose;

    {
        PLockGuard guard(_lock, "CRioSessionManager_BeginCloseAllSessions");
        sessionsToClose.reserve(_sessions.size());

        // [수정] 구조적 바인딩 대신 일반 순회 및 .first/.second 참조 방식 사용
        for( auto const& pair : _sessions )
        {
            if( pair.second.session )
            {
                sessionsToClose.push_back(pair.second.session);
            }
        }
    }

    for( const auto& session : sessionsToClose )
    {
        if( session )
        {
            session->Close(CloseReason::ForcedClose);
        }
    }
}

//***************************************************************************
// @brief 관리 중인 모든 세션이 완전히 Closed 되었는지 검사합니다.
// @return bool 전부 닫혔으면 true, 아니면 false
//***************************************************************************
bool CRioSessionManager::AreAllSessionsClosed() const
{
    PLockGuard guard(_lock, "CRioSessionManager_AreAllSessionsClosed");

    for( auto const& pair : _sessions )
    {
        if( pair.second.session && !pair.second.session->IsClosed() )
        {
            return false;
        }
    }

    return true;
}

//***************************************************************************
// @brief 맵 내부에 누적된 Closed 상태의 세션들을 안전하게 일괄 제거합니다.
//***************************************************************************
void CRioSessionManager::RemoveClosedSessions()
{
    PLockGuard guard(_lock, "CRioSessionManager_RemoveClosedSessions");

    for( auto it = _sessions.begin(); it != _sessions.end(); )
    {
        const auto& session = it->second.session;
        if( !session || session->IsClosed() )
        {
            it = _sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

//***************************************************************************
// @brief 현재 관리 중인 활성 세션 개수를 반환합니다.
// @return size_t 활성 세션 수량
//***************************************************************************
size_t CRioSessionManager::GetSessionCount() const
{
    PLockGuard guard(_lock, "CRioSessionManager_GetSessionCount");
    return _sessions.size();
}