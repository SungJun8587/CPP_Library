
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
	BeginCloseAllSessions();
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
// @param sessionId 고유 세션 ID (Key)
// @param session 세션 shared_ptr
// @return 등록 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSessionManager::AddSession(uint64_t sessionId, CRioSessionRef session)
{
	if( sessionId == 0 || session == nullptr )
		return false;

	return _sessions.InsertObject(sessionId, session);
}

//***************************************************************************
// @brief SessionId를 기반으로 매니저에서 세션을 제거합니다.
// @param sessionId 제거할 고유 세션 ID (Key)
//***************************************************************************
void CRioSessionManager::RemoveSession(uint64_t sessionId)
{
	if( sessionId == 0 )
		return;

	_sessions.EraseObject(sessionId);
}

//***************************************************************************
// @brief SessionId 기반으로 유효한 세션을 검색합니다.
// @param sessionId 찾을 고유 세션 ID (Key)
// @return 세션 shared_ptr (존재하지 않을 경우 nullptr)
//***************************************************************************
CRioSessionRef CRioSessionManager::FindSession(uint64_t sessionId) const
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
size_t CRioSessionManager::GetSessionCount() const
{
	auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
	return static_cast<size_t>(mutableSessions.getSize());
}

//***************************************************************************
// @brief 모든 활성 세션에 패킷 데이터를 브로드캐스트합니다.
// @param data 전송할 데이터 포인터
// @param size 전송할 데이터 크기 (바이트)
//***************************************************************************
void CRioSessionManager::Broadcast(const void* data, uint16_t size)
{
	if( data == nullptr || size == 0 )
	{
		return;
	}

	CVector<CRioSessionRef> sessionsToSend;

	// 1. 모든 클러스터를 인덱스로 순회하며 활성 세션들의 스냅샷 수집
	__int32 clusterCnt = _sessions.GetClusterCnt();
	for( __int32 i = 0; i < clusterCnt; ++i )
	{
		_sessions.ReadLockByIdx(i, __FUNCTION__);

		auto& map = _sessions.GetClusterMapByIdx(i);
		for( const auto& pair : map )
		{
			if( pair.second && pair.second->IsActive() )
			{
				sessionsToSend.push_back(pair.second);
			}
		}

		_sessions.ReadUnlockByIdx(i, __FUNCTION__);
	}

	// 2. 락 외부에서 각 세션의 Send 호출 (세션 내부에서 큐잉 및 Flush 진행, 데드락 방지)
	for( const auto& session : sessionsToSend )
	{
		if( session && session->IsActive() )
		{
			session->Send(data, size);
		}
	}
}

//***************************************************************************
// @brief 락 내부에서 스냅샷을 수집한 뒤 락 밖에서 안전하게 Close를 브로드캐스트합니다.
//***************************************************************************
void CRioSessionManager::BeginCloseAllSessions()
{
	CVector<CRioSessionRef> sessionsToClose;

	__int32 clusterCnt = _sessions.GetClusterCnt();
	for( __int32 i = 0; i < clusterCnt; ++i )
	{
		_sessions.ReadLockByIdx(i, __FUNCTION__);

		auto& map = _sessions.GetClusterMapByIdx(i);
		for( const auto& pair : map )
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
			session->Close(Rio::CloseReason::ForcedClose);
		}
	}
}

//***************************************************************************
// @brief 관리 중인 모든 세션이 완전히 Closed 되었는지 검사합니다.
// @return bool 전부 닫혔으면 true, 아니면 false
//***************************************************************************
bool CRioSessionManager::AreAllSessionsClosed() const
{
	auto& mutableSessions = const_cast<decltype(_sessions)&>(_sessions);
	__int32 clusterCnt = mutableSessions.GetClusterCnt();

	for( __int32 i = 0; i < clusterCnt; ++i )
	{
		mutableSessions.ReadLockByIdx(i, __FUNCTION__);

		auto& map = mutableSessions.GetClusterMapByIdx(i);
		for( const auto& pair : map )
		{
			if( pair.second && !pair.second->IsClosed() )
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
// @brief 맵 내부에 누적된 Closed 상태의 세션들을 안전하게 일괄 제거합니다.
//***************************************************************************
void CRioSessionManager::RemoveClosedSessions()
{
	__int32 clusterCnt = _sessions.GetClusterCnt();

	for( __int32 i = 0; i < clusterCnt; ++i )
	{
		_sessions.WriteLockByIdx(i, __FUNCTION__);

		auto& map = _sessions.GetClusterMapByIdx(i);
		for( auto it = map.begin(); it != map.end(); )
		{
			const auto& session = it->second;
			if( !session || session->IsClosed() )
			{
				it = map.erase(it);
			}
			else
			{
				++it;
			}
		}

		_sessions.WriteUnlockByIdx(i, __FUNCTION__);
	}
}