
//***************************************************************************
// IocpSessionManager.cpp : implementation of the CIocpSessionManager class.
//
//***************************************************************************

#include "pch.h"
#include "IocpSessionManager.h"

//***************************************************************************
// @brief CIocpSessionManager 생성자
//***************************************************************************
CIocpSessionManager::CIocpSessionManager()
    : _nextSessionId(0)
{
}

//***************************************************************************
// @brief CIocpSessionManager 소멸자
//***************************************************************************
CIocpSessionManager::~CIocpSessionManager()
{
    BeginCloseAllSessions();
}

//***************************************************************************
// @brief 원자적으로 새로운 고유 SessionId를 발급합니다.
// @return uint64_t 고유 세션 ID
//***************************************************************************
uint64_t CIocpSessionManager::GenerateSessionId()
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
bool CIocpSessionManager::AddSession(SOCKET socket, uint64_t sessionId, CIocpSessionRef session)
{
    if( socket == INVALID_SOCKET || session == nullptr )
        return false;

    return _sessions.InsertObject(socket, IocpSessionEntry{ sessionId, session });
}

//***************************************************************************
// @brief 소켓 재사용 레이스 방지를 위해 세션 ID를 함께 검증하여 매니저에서 제거합니다.
// @param socket 제거할 소켓 키
// @param sessionId 검증할 고유 세션 ID
//***************************************************************************
void CIocpSessionManager::RemoveSession(SOCKET socket, uint64_t sessionId)
{
    if( socket == INVALID_SOCKET )
        return;

    _sessions.WriteLock(socket, __FUNCTION__);

    IocpSessionEntry entry;
    if( _sessions.FindObject(socket, entry) )
    {
        if( entry.sessionId == sessionId )
        {
            _sessions.EraseObject(socket);
        }
    }

    _sessions.WriteUnlock(socket, __FUNCTION__);
}

//***************************************************************************
// @brief 소켓 키 기반으로 유효한 세션을 검색합니다.
// @param socket 찾을 소켓 키
// @return 세션 shared_ptr (존재하지 않을 경우 nullptr)
//***************************************************************************
CIocpSessionRef CIocpSessionManager::FindSession(SOCKET socket) const
{
    if( socket == INVALID_SOCKET ) return nullptr;

    auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);

    mutableSessions.ReadLock(socket, __FUNCTION__);

    IocpSessionEntry entry;
    CIocpSessionRef session = nullptr;
    if( mutableSessions.FindObject(socket, entry) )
    {
        session = entry.session;
    }

    mutableSessions.ReadUnlock(socket, __FUNCTION__);

    return session;
}

//***************************************************************************
// @brief 현재 관리 중인 활성 세션 총 개수를 반환합니다.
// @return size_t 활성 세션 수량
//***************************************************************************
size_t CIocpSessionManager::GetSessionCount() const
{
    // decltype(_sessions) 적용으로 하드코딩 템플릿 인자 제거
    auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
    return static_cast<size_t>(mutableSessions.getSize());
}

//***************************************************************************
// @brief 모든 클러스터를 순회하며 스냅샷을 수집한 뒤 락 밖에서 안전하게 전체 세션에게 패킷 전송을 브로드캐스트합니다.
// @param sendBuffer 전송할 패킷 버퍼
//***************************************************************************
void CIocpSessionManager::Broadcast(CSendBufferRef sendBuffer)
{
    if( sendBuffer == nullptr ) return;

    for( int32 i = 0; i < _sessions.GetClusterCnt(); ++i )
    {
        // 1. 인덱스로 직접 락 획득
        _sessions.WriteLockByIdx(i, __FUNCTION__);

        // 2. 인덱스로 해당 클러스터의 Map 참조 가져오기
        auto& sessionMap = _sessions.GetClusterMapByIdx(i);
        for( auto& pair : sessionMap )
        {
            const auto& session = pair.second.session;
            if( session && session->IsConnected() )
            {
                session->Send(sendBuffer);
            }
        }

        // 3. 락 해제
        _sessions.WriteUnlockByIdx(i, __FUNCTION__);
    }
}

//***************************************************************************
// @brief 각 클러스터별로 순회하며 스냅샷을 수집한 뒤 락 밖에서 안전하게 Disconnect를 브로드캐스트합니다.
//***************************************************************************
void CIocpSessionManager::BeginCloseAllSessions()
{
    CVector<CIocpSessionRef> sessionsToClose;
    INT32 clusterCnt = _sessions.GetClusterCnt();

    for( INT32 i = 0; i < clusterCnt; ++i )
    {
        // 인덱스 기반 직접 읽기 락 및 맵 참조
        _sessions.ReadLockByIdx(i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second.session )
            {
                sessionsToClose.push_back(pair.second.session);
            }
        }
        _sessions.ReadUnlockByIdx(i, __FUNCTION__);
    }

    for( const auto& session : sessionsToClose )
    {
        if( session )
        {
            session->Disconnect(L"SessionManager Clear");
        }
    }
}

//***************************************************************************
// @brief 관리 중인 모든 세션이 완전히 연결 종료(Disconnected) 되었는지 검사합니다.
// @return bool 전부 끊어졌으면 true, 아니면 false
//***************************************************************************
bool CIocpSessionManager::AreAllSessionsClosed() const
{
    auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
    INT32 clusterCnt = mutableSessions.GetClusterCnt();

    for( INT32 i = 0; i < clusterCnt; ++i )
    {
        // 인덱스 기반 직접 읽기 락 및 맵 참조
        mutableSessions.ReadLockByIdx(i, __FUNCTION__);
        auto& objMap = mutableSessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second.session && pair.second.session->IsConnected() )
            {
                mutableSessions.ReadUnlockByIdx(i, __FUNCTION__);
                return false;
            }
        }
        mutableSessions.ReadUnlockByIdx(i, __FUNCTION__);
    }

    return true;
}

//***************************************************************************
// @brief 맵 내부에 누적된 연결 해제 상태의 세션들을 클러스터별로 안전하게 일괄 제거합니다.
//***************************************************************************
void CIocpSessionManager::RemoveClosedSessions()
{
    INT32 clusterCnt = _sessions.GetClusterCnt();

    for( INT32 i = 0; i < clusterCnt; ++i )
    {
        // 인덱스 기반 직접 쓰기 락 및 맵 참조
        _sessions.WriteLockByIdx(i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto it = objMap.begin(); it != objMap.end(); )
        {
            const auto& session = it->second.session;
            if( !session || !session->IsConnected() )
            {
                it = objMap.erase(it);
            }
            else
            {
                ++it;
            }
        }
        _sessions.WriteUnlockByIdx(i, __FUNCTION__);
    }
}