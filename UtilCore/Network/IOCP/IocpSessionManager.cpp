
//***************************************************************************
// IocpSessionManager.cpp : implementation of the CIocpSessionManager class.
//
//***************************************************************************

#include "pch.h"
#include "IocpSessionManager.h"

//***************************************************************************
// @brief CIocpSessionManager 생성자
// @details [수정] _nextSessionId를 0이 아니라 1부터 시작한다 — 이유는
//          IocpSessionManager.h의 _nextSessionId 선언부 주석 참고. 0은
//          AddSession()/RemoveSession()/FindSession()이 "무효 ID"로 예약해
//          둔 값이라, 카운터가 0에서 시작하면 fetch_add()가 돌려주는 첫
//          값이 0이 되어 그 세션이 맵에 등록조차 못 되는 사고가 났다.
//***************************************************************************
CIocpSessionManager::CIocpSessionManager()
    : _nextSessionId(1)
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
// @return uint64 고유 세션 ID
//***************************************************************************
uint64 CIocpSessionManager::GenerateSessionId()
{
    return _nextSessionId.fetch_add(1, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 세션을 매니저에 등록합니다.
// @param sessionId 고유 세션 ID (Key)
// @param session 세션 shared_ptr
// @return 등록 성공 시 true, 실패 시 false
//***************************************************************************
bool CIocpSessionManager::AddSession(uint64 sessionId, CIocpSessionRef session)
{
    if( sessionId == 0 || session == nullptr )
        return false;

    return _sessions.InsertObject(sessionId, session);
}

//***************************************************************************
// @brief SessionId를 기반으로 매니저에서 세션을 제거합니다.
// @param sessionId 제거할 고유 세션 ID (Key)
//***************************************************************************
void CIocpSessionManager::RemoveSession(uint64 sessionId)
{
    if( sessionId == 0 )
        return;

    (void)_sessions.EraseObject(sessionId);
}

//***************************************************************************
// @brief SessionId 기반으로 유효한 세션을 검색합니다.
// @param sessionId 찾을 고유 세션 ID (Key)
// @return 세션 shared_ptr (존재하지 않을 경우 nullptr)
//***************************************************************************
CIocpSessionRef CIocpSessionManager::FindSession(uint64 sessionId) const
{
    if( sessionId == 0 )
        return nullptr;

    auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
    return mutableSessions.FindObject(sessionId);
}

//***************************************************************************
// @brief 현재 관리 중인 활성 세션 총 개수를 반환합니다.
// @return size_t 활성 세션 수량
//***************************************************************************
size_t CIocpSessionManager::GetSessionCount() const
{
    auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
    return static_cast<size_t>(mutableSessions.getSize());
}

//***************************************************************************
// @brief 모든 클러스터를 순회하며 스냅샷을 수집한 뒤 락 밖에서 안전하게 전체 세션에게 패킷 전송을 브로드캐스트합니다.
// @param data 전송할 데이터 포인터
// @param size 전송할 데이터 크기
// @note [수정] 기존에는 맵을 변경하지 않음에도 WriteLock을 잡은 채 session->Send()
//       (내부적으로 락 획득 + WSASend 게시까지 수행)를 호출해 락 경합이 컸다.
//       CRioSessionManager::Broadcast()와 동일하게 ReadLock으로 스냅샷만 수집한
//       뒤 락 밖에서 Send()를 호출하도록 수정.
//***************************************************************************
void CIocpSessionManager::Broadcast(const void* data, uint16 size)
{
    if( data == nullptr || size == 0 )
        return;

    CVector<CIocpSessionRef> sessionsToSend;

    // 1. 각 클러스터를 ReadLock으로 순회하며 활성 세션들의 스냅샷만 수집
    for( int32 i = 0; i < _sessions.GetClusterCnt(); ++i )
    {
        _sessions.ReadLockByIdx(i, __FUNCTION__);

        auto& sessionMap = _sessions.GetClusterMapByIdx(i);
        for( auto& pair : sessionMap )
        {
            if( pair.second && pair.second->IsConnected() )
            {
                sessionsToSend.push_back(pair.second);
            }
        }

        _sessions.ReadUnlockByIdx(i, __FUNCTION__);
    }

    // 2. 락 외부에서 각 세션의 Send 호출 (다른 Add/Remove/Find와 경합하지 않음)
    for( const auto& session : sessionsToSend )
    {
        if( session && session->IsConnected() )
        {
            session->Send(data, size);
        }
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
        _sessions.ReadLockByIdx(i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second )
            {
                sessionsToClose.push_back(pair.second);
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
        mutableSessions.ReadLockByIdx(i, __FUNCTION__);
        auto& objMap = mutableSessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second && pair.second->IsConnected() )
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
        _sessions.WriteLockByIdx(i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto it = objMap.begin(); it != objMap.end(); )
        {
            const auto& session = it->second;
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