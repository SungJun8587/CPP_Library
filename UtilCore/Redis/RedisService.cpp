
//***************************************************************************
// RedisService.cpp: implementation of the CRedisService class.
//
//***************************************************************************

#include "pch.h"
#include "RedisService.h"

//***************************************************************************
// @brief TCHAR 문자열을 std::string으로 변환함(UNICODE 빌드 대응)
//***************************************************************************
namespace
{
	std::string TCharToString(const TCHAR* ptsz)
	{
		if( ptsz == nullptr ) return std::string();

		return TStringToString(ptsz);
	}
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisService 생성자
// @param iocpCore IOCP 코어 참조 객체. Init()이 노드별 풀을 만들 때 사용됨
// @param pJobQueue 결과를 전달받을 메인/대상 스레드의 JobQueue 객체 포인터
//***************************************************************************
CRedisService::CRedisService(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue)
	: _iocpCore(iocpCore)
	, _jobQueue(pJobQueue)
{
}

//***************************************************************************
// @brief CRedisService 소멸자
// @details _poolMap이 소멸되며 각 CRedisConnectionPool의 소멸자가 순서대로
//          호출되어 재연결 스레드 정지 및 전체 커넥션 정리가 이루어진다.
//***************************************************************************
CRedisService::~CRedisService()
{
}

//***************************************************************************
// @brief 서버 설정 파일에서 읽어온 CRedisNode 목록으로 노드별 Redis 커넥션 풀을 각각 초기화함
//***************************************************************************
bool CRedisService::Init(CVector<CRedisNode> redisNodeVec, const int32 nPoolSize)
{
	if( redisNodeVec.empty() ) return false;

	// 먼저 각 노드에 대한 풀을 전부 만들어 로컬 벡터에 쌓는다 — 하나라도
	// 실패하면 로컬 벡터가 스코프를 벗어나며 그때까지 만든 풀들이 자동으로
	// 정리되고, 멤버 _poolMap은 손대지 않은 채 그대로 남는다.
	CVector<std::pair<int16, CRedisConnectionPoolRef>> vecPools;
	vecPools.reserve(redisNodeVec.size());

	for( const auto& node : redisNodeVec )
	{
		auto pPool = std::make_shared<CRedisConnectionPool>(_iocpCore);
		if( !pPool->Init(TCharToString(node._tszDBHost), node._nPort, static_cast<int32>(node._nDbIndex), nPoolSize) )
			return false;

		vecPools.push_back({ node._nID, pPool });
	}

	// 여기까지 왔으면 전부 연결 성공 — 이제 멤버 맵에 반영한다.
	_poolMap.ClearObjectMap();
	for( auto& item : vecPools )
	{
		if( !_poolMap.InsertObject(item.first, item.second) )
		{
			// InsertObject는 이미 존재하는 키에는 실패한다 — redisNodeVec에
			// 같은 _nID가 중복돼 있다는 뜻이므로 설정 오류로 로그를 남긴다.
			LOG_ERROR(_T("CRedisService::Init: duplicate node ID(%d) in redisNodeVec, skipped"), item.first);
		}
	}

	_nDefaultNodeId.store(redisNodeVec[0]._nID, std::memory_order_relaxed);
	return true;
}

//***************************************************************************
// @brief 기본 노드로 Redis 명령을 비동기 실행함
//***************************************************************************
bool CRedisService::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	return SendCommand(_nDefaultNodeId.load(std::memory_order_relaxed), vecArgs, fnMainThreadCallback);
}

//***************************************************************************
// @brief 지정한 노드로 Redis 명령을 비동기 실행함
//***************************************************************************
bool CRedisService::SendCommand(const int16 nNodeId, const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	CRedisConnectionPoolRef pPool = FindPool(nNodeId);
	if( !pPool ) return false;

	std::weak_ptr<CJobQueue> weakJobQueue = _jobQueue;

	auto fnIocpCallback = [weakJobQueue, fnMainThreadCallback](const RedisValue& res) {
		if( auto pJobQueue = weakJobQueue.lock() )
		{
			// CJobQueue의 DoAsync를 활용하여 로직 스레드 큐로 작업 이관 및 복사 최적화
			pJobQueue->DoAsync([fnMainThreadCallback, res = std::move(res)]() mutable {
				if( fnMainThreadCallback )
					fnMainThreadCallback(res);
				});
		}
		};

	return pPool->SendCommand(vecArgs, fnIocpCallback);
}

//***************************************************************************
// @brief 노드 ID로 커넥션 풀을 찾아 반환함
// @param nNodeId 찾을 노드 ID
// @return 등록된 풀(shared_ptr). 없으면 빈 shared_ptr
//***************************************************************************
CRedisConnectionPoolRef CRedisService::FindPool(const int16 nNodeId)
{
	CRedisConnectionPoolRef pPool;
	(void)_poolMap.FindObject(nNodeId, pPool);
	return pPool;
}