
//***************************************************************************
// IocpSessionManager.cpp : implementation of the CIocpSessionManager class.
//
//***************************************************************************

#include "pch.h"
#include "IocpSessionManager.h"
#include "IocpSession.h"

#include <utility>

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

    return _sessions.FindObject(sessionId);
}

//***************************************************************************
// @brief 현재 관리 중인 활성 세션 총 개수를 반환합니다.
// @return size_t 활성 세션 수량
//***************************************************************************
size_t CIocpSessionManager::GetSessionCount() const
{
    return static_cast<size_t>(_sessions.getSize());
}

//***************************************************************************
// @brief 모든 클러스터를 순회하며 스냅샷을 수집한 뒤 락 밖에서 안전하게 전체 세션에게 패킷 전송을 브로드캐스트합니다.
// @param data 전송할 데이터 포인터
// @param size 전송할 데이터 크기
// @note 맵은 ReadLock으로 스냅샷만 수집하고 락 밖에서 전송한다(CRioSessionManager::Broadcast()와 동일).
//       전송 데이터는 SendBuffer로 한 번만 복사해 모든 세션의 송신 큐가 공유한다.
//***************************************************************************
void CIocpSessionManager::Broadcast(const void* data, uint16 size)
{
    if( data == nullptr || size == 0 )
        return;

    CVector<CIocpSessionRef> sessionsToSend;

    // 1. 각 클러스터를 ReadLock으로 순회하며 활성 세션들의 스냅샷만 수집
    for( int32 i = 0; i < _sessions.GetClusterCnt(); ++i )
    {
        SessionMap::ReadLockGuardByIdx guard(_sessions, i, __FUNCTION__);

        auto& sessionMap = _sessions.GetClusterMapByIdx(i);
        for( auto& pair : sessionMap )
        {
            if( pair.second && pair.second->IsConnected() )
            {
                sessionsToSend.push_back(pair.second);
            }
        }
    }

    if( sessionsToSend.empty() )
        return;

    // 2. 같은 데이터를 세션마다 복사하지 않고 SendBuffer를 한 번만 만들어 모든 세션이 공유한다.
    CIocpSession::SendPieces pieces;
    if( CIocpSession::PrepareSend(data, size, pieces) == false )
        return;

    // 3. 락 외부에서 각 세션의 송신 큐에 넣는다 (다른 Add/Remove/Find와 경합하지 않음)
    for( const auto& session : sessionsToSend )
    {
        if( session && session->IsConnected() )
        {
            session->SendPrepared(pieces);
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
        SessionMap::ReadLockGuardByIdx guard(_sessions, i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second )
            {
                sessionsToClose.push_back(pair.second);
            }
        }
    }

    for( const auto& session : sessionsToClose )
    {
        if( session )
        {
            session->Disconnect(_T("SessionManager Clear"));
        }
    }
}

//***************************************************************************
// @brief 관리 중인 모든 세션이 완전히 연결 종료(Disconnected) 되었는지 검사합니다.
// @return bool 전부 끊어졌으면 true, 아니면 false
//***************************************************************************
bool CIocpSessionManager::AreAllSessionsClosed() const
{
    INT32 clusterCnt = _sessions.GetClusterCnt();

    for( INT32 i = 0; i < clusterCnt; ++i )
    {
        SessionMap::ReadLockGuardByIdx guard(_sessions, i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto const& pair : objMap )
        {
            if( pair.second && pair.second->IsConnected() )
                return false;
        }
    }

    return true;
}

//***************************************************************************
// @brief 맵 내부에 누적된 연결 해제 상태의 세션들을 클러스터별로 안전하게 일괄 제거합니다.
// @details 종료 사유(CloseReason)가 기록된 해제 세션만 제거 대상입니다. 아직 연결 완료 전인
//          신규 세션(_connected == false, 종료 사유 없음)은 제거하지 않습니다 — 서비스는
//          세션을 매니저에 등록한 뒤 ProcessConnect()를 호출하므로, 그 사이에 이 함수가
//          실행돼도 새 세션이 맵에서 빠지지 않아야 Broadcast/FindSession에서 누락되지 않습니다.
//***************************************************************************
void CIocpSessionManager::RemoveClosedSessions()
{
    INT32 clusterCnt = _sessions.GetClusterCnt();

    // 락을 쥔 채 세션 참조를 놓으면 마지막 참조일 때 세션 소멸자(소켓 닫기, 버퍼 해제)가 스핀락 구간에서
    // 실행된다. 제거한 참조는 여기에 모아 두었다가 함수가 끝나 모든 락이 풀린 뒤에 놓는다.
    CVector<CIocpSessionRef> removedSessions;

    for( INT32 i = 0; i < clusterCnt; ++i )
    {
        SessionMap::WriteLockGuardByIdx guard(_sessions, i, __FUNCTION__);
        auto& objMap = _sessions.GetClusterMapByIdx(i);

        for( auto it = objMap.begin(); it != objMap.end(); )
        {
            auto& session = it->second;
            if( !session || (!session->IsConnected() && session->GetCloseReason() != Iocp::CloseReason::None) )
            {
                if( session )
                    removedSessions.push_back(std::move(session));

                it = objMap.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}