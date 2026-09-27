
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
// @details [수정 — 버그 수정: 중복 Node ID / 방어적 검증] 외부 리뷰로 발견된
//          문제들을 반영:
//          1) 예전엔 redisNodeVec에 같은 _nID가 중복돼 있으면 InsertObject()
//             실패를 로그만 남기고 그냥 true를 반환했다 — "모든 노드 성공"
//             이라는 이 함수의 계약과 실제 동작(중복 노드 하나가 조용히
//             누락됨)이 어긋났고, 게다가 그 시점엔 이미 중복 노드의 Redis
//             연결까지 전부 만들어놓은 뒤였다. 이제 실제 연결을 시도하기
//             전에(풀을 하나도 만들기 전에) 먼저 ID 중복을 O(N^2)로
//             검사한다 — 노드 개수가 보통 한 자릿수라 이 정도로 충분하다.
//          2) nPoolSize/_iocpCore/_jobQueue에 대한 방어적 검증을 추가했다 —
//             예전엔 이 값들이 잘못됐을 때 훨씬 더 아래 단계
//             (CRedisConnectionPool::Init() 내부, 또는 그보다도 더 깊은
//             곳)에서야 실패하거나, 심하면 null 포인터 역참조로 크래시가
//             날 수 있었다.
//          3) _poolMap.InsertObject() 자체가 실패하는(이론적으로 위 1번
//             검증을 통과했다면 발생하지 않아야 하지만, 방어적으로) 경우도
//             더 이상 로그만 남기고 넘어가지 않고 Init() 전체를 실패로
//             처리한다.
//***************************************************************************
bool CRedisService::Init(CVector<CRedisNode> redisNodeVec, const int32 nPoolSize)
{
	if( redisNodeVec.empty() )
	{
		LOG_ERROR(_T("CRedisService::Init: redis node list is empty"));
		return false;
	}

	if( nPoolSize <= 0 )
	{
		LOG_ERROR(_T("CRedisService::Init: invalid pool size(%d)"), nPoolSize);
		return false;
	}

	if( !_iocpCore )
	{
		LOG_ERROR(_T("CRedisService::Init: IOCP core is null"));
		return false;
	}

	if( !_jobQueue )
	{
		LOG_ERROR(_T("CRedisService::Init: JobQueue is null"));
		return false;
	}

	// [추가 — 버그 수정] Node ID 중복 검사 — 실제 Redis 연결(블로킹, 노드당
	// 최대 수 초)을 만들기 전에 설정 오류부터 걸러낸다.
	for( size_t i = 0; i < redisNodeVec.size(); ++i )
	{
		for( size_t j = i + 1; j < redisNodeVec.size(); ++j )
		{
			if( redisNodeVec[i]._nID == redisNodeVec[j]._nID )
			{
				LOG_ERROR(_T("CRedisService::Init: duplicate node ID(%d)"), redisNodeVec[i]._nID);
				return false;
			}
		}
	}

	// 먼저 각 노드에 대한 풀을 전부 만들어 로컬 벡터에 쌓는다 — 하나라도
	// 실패하면 로컬 벡터가 스코프를 벗어나며 그때까지 만든 풀들이 자동으로
	// 정리되고, 멤버 _poolMap은 손대지 않은 채 그대로 남는다(기존 상태
	// 유지 — 예를 들어 3개 노드 중 3번째만 연결 실패해도 기존에 정상
	// 동작하던 _poolMap은 그대로 살아있다).
	CVector<std::pair<int16, CRedisConnectionPoolRef>> vecPools;
	vecPools.reserve(redisNodeVec.size());

	for( const auto& node : redisNodeVec )
	{
		auto pPool = std::make_shared<CRedisConnectionPool>(_iocpCore);
		if( !pPool->Init(TCharToString(node._tszDBHost), node._nPort, static_cast<int32>(node._nDbIndex), nPoolSize) )
		{
			// [추가] 실패 원인을 바로 알 수 있도록 노드 정보를 로그에 남긴다.
			LOG_ERROR(_T("CRedisService::Init: failed to initialize Redis pool. nodeId(%d), host(%s), port(%d), db(%d)"),
				node._nID, node._tszDBHost, node._nPort, node._nDbIndex);
			return false;
		}

		vecPools.push_back({ node._nID, pPool });
	}

	// 여기까지 왔으면 전부 연결 성공 — 이제 멤버 맵에 반영한다.
	_poolMap.ClearObjectMap();
	for( auto& item : vecPools )
	{
		if( !_poolMap.InsertObject(item.first, item.second) )
		{
			// [수정 — 버그 수정] 위에서 이미 ID 중복을 검사했으므로 정상
			// 흐름이라면 여기 도달하지 않아야 한다 — 그래도 방어적으로,
			// 예전처럼 로그만 남기고 넘어가는 대신 Init() 전체를 실패로
			// 처리한다("모든 노드 성공"이라는 계약을 실제로 지킨다).
			LOG_ERROR(_T("CRedisService::Init: failed to insert Redis pool. nodeId(%d)"), item.first);
			_poolMap.ClearObjectMap();
			return false;
		}
	}

	// [수정] release로 — _poolMap 반영이 끝난 뒤 이 값을 세운다는 순서
	// 자체는 원래도 지켜지고 있었지만(_poolMap은 이미 스스로 스레드 세이프),
	// 다른 스레드가 이 store를 관찰하면 그 이전의 _poolMap 반영도 함께
	// 관찰된다는 의도를 relaxed보다 release/acquire 페어로 더 명확히
	// 드러낸다. _poolMap 자체의 재초기화 원자성(클래스 문서 상단 "lifecycle
	// 계약" 참고)과는 별개의 문제다.
	_nDefaultNodeId.store(redisNodeVec[0]._nID, std::memory_order_release);
	return true;
}

//***************************************************************************
// @brief 기본 노드로 Redis 명령을 비동기 실행함
//***************************************************************************
bool CRedisService::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	return SendCommand(_nDefaultNodeId.load(std::memory_order_acquire), vecArgs, fnMainThreadCallback);
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
// @details shared_ptr을 반환하므로, 호출부가 이 반환값을 받아 쥔 순간부터는
//          그 이후 Init() 재호출로 _poolMap이 교체되더라도(클래스 문서 상단
//          "lifecycle 계약" 참고 — 정상 운영 중에는 발생하지 않아야 함) 이미
//          받아 쥔 풀 자체는 계속 유효하다.
//***************************************************************************
CRedisConnectionPoolRef CRedisService::FindPool(const int16 nNodeId)
{
	CRedisConnectionPoolRef pPool;
	if( !_poolMap.FindObject(nNodeId, pPool) )
		return nullptr;

	return pPool;
}