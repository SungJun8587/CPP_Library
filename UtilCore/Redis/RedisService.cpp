
//***************************************************************************
// RedisService.cpp: implementation of the CRedisService class.
//
//***************************************************************************

#include "pch.h"
#include "RedisService.h"
#include <Redis/RedisHelper.h>
#include <Util/CommonUtil.h>

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
	ShutdownSubscribers();
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
		if( !pPool->Init(TCharToString(node._tszDBHost), node._nPort, static_cast<int32>(node._nDbIndex), nPoolSize, Redis::kDefaultReconnectIntervalSec,
			TCharToString(node._tszDBUserId), TCharToString(node._tszDBPasswd)) )
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

	// 이전 Init()에서 만든 구독 연결은 노드 정보가 바뀔 수 있으므로 닫고, 새 노드 목록을 보관한다.
	// 구독 연결은 첫 Subscribe() 호출 때 새로 만들어진다.
	const int16 nDefaultNodeId = redisNodeVec[0]._nID;
	ShutdownSubscribers();
	{
		std::lock_guard<std::mutex> lock(_subscriberLock);
		_redisNodeVec = std::move(redisNodeVec);
	}

	// release — _poolMap 반영이 끝난 뒤 이 값을 세운다. 이 store를 acquire로 읽은 스레드는 그 이전의
	// _poolMap 반영도 함께 관찰한다.
	_nDefaultNodeId.store(nDefaultNodeId, std::memory_order_release);
	return true;
}

//***************************************************************************
// @brief 기본 노드로 Redis 명령을 비동기 실행함
//***************************************************************************
bool CRedisService::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	return SendCommand(_nDefaultNodeId.load(std::memory_order_acquire), vecArgs, std::move(fnMainThreadCallback));
}

//***************************************************************************
// @brief 지정한 노드로 Redis 명령을 비동기 실행함
//***************************************************************************
bool CRedisService::SendCommand(const int16 nNodeId, const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)
{
	CRedisConnectionPoolRef pPool = FindPool(nNodeId);
	if( !pPool ) return false;

	std::weak_ptr<CJobQueue> weakJobQueue = _jobQueue;

	// 이 콜백은 클라이언트 계약상 정확히 한 번만 호출되므로, 사용자 콜백을 복사하지 않고 move한다
	// (콜백이 캡처한 문자열/shared_ptr을 명령마다 여러 번 깊은 복사하지 않기 위함).
	// 응답 값도 rvalue로 받아 JobQueue 작업에 move한다(클라이언트가 파싱한 값이 복사 없이 사용자 콜백까지 간다).
	auto fnIocpCallback = [weakJobQueue, fnUserCallback = std::move(fnMainThreadCallback)](RedisValue&& res) mutable {
		if( auto pJobQueue = weakJobQueue.lock() )
		{
			pJobQueue->DoAsync([fnUserCallback = std::move(fnUserCallback), res = std::move(res)]() mutable {
				if( fnUserCallback )
					fnUserCallback(res);
				});
		}
		};

	return pPool->SendCommand(vecArgs, std::move(fnIocpCallback));
}

//***************************************************************************
// @brief 기본 노드의 채널로 메시지를 발행함
//***************************************************************************
bool CRedisService::Publish(const std::string& strChannel, const std::string& strMessage, RedisCallback fnMainThreadCallback)
{
	return Publish(_nDefaultNodeId.load(std::memory_order_acquire), strChannel, strMessage, std::move(fnMainThreadCallback));
}

//***************************************************************************
// @brief 지정한 노드의 채널로 메시지를 발행함(PUBLISH 명령을 일반 풀로 전송)
//***************************************************************************
bool CRedisService::Publish(const int16 nNodeId, const std::string& strChannel, const std::string& strMessage, RedisCallback fnMainThreadCallback)
{
	if( strChannel.empty() )
		return false;

	return SendCommand(nNodeId, Redis::MakeArgs("PUBLISH", strChannel, strMessage), std::move(fnMainThreadCallback));
}

//***************************************************************************
// @brief 채널 구독(기본 노드)
//***************************************************************************
bool CRedisService::Subscribe(const std::string& strChannel, RedisMessageCallback fnCallback)
{
	return Subscribe(_nDefaultNodeId.load(std::memory_order_acquire), strChannel, std::move(fnCallback));
}

//***************************************************************************
// @brief 채널 구독(지정 노드). 노드의 구독 연결이 없으면 만든다.
//***************************************************************************
bool CRedisService::Subscribe(const int16 nNodeId, const std::string& strChannel, RedisMessageCallback fnCallback)
{
	CRedisSubscriberRef pSubscriber = FindSubscriber(nNodeId, true);
	if( !pSubscriber )
		return false;

	return pSubscriber->Subscribe(strChannel, std::move(fnCallback));
}

//***************************************************************************
// @brief 채널 구독 해제(기본 노드)
//***************************************************************************
bool CRedisService::Unsubscribe(const std::string& strChannel)
{
	return Unsubscribe(_nDefaultNodeId.load(std::memory_order_acquire), strChannel);
}

//***************************************************************************
// @brief 채널 구독 해제(지정 노드). 구독 연결이 없으면 해제할 것도 없으므로 false
//***************************************************************************
bool CRedisService::Unsubscribe(const int16 nNodeId, const std::string& strChannel)
{
	CRedisSubscriberRef pSubscriber = FindSubscriber(nNodeId, false);
	if( !pSubscriber )
		return false;

	return pSubscriber->Unsubscribe(strChannel);
}

//***************************************************************************
// @brief 패턴 구독(기본 노드)
//***************************************************************************
bool CRedisService::PSubscribe(const std::string& strPattern, RedisMessageCallback fnCallback)
{
	return PSubscribe(_nDefaultNodeId.load(std::memory_order_acquire), strPattern, std::move(fnCallback));
}

//***************************************************************************
// @brief 패턴 구독(지정 노드)
//***************************************************************************
bool CRedisService::PSubscribe(const int16 nNodeId, const std::string& strPattern, RedisMessageCallback fnCallback)
{
	CRedisSubscriberRef pSubscriber = FindSubscriber(nNodeId, true);
	if( !pSubscriber )
		return false;

	return pSubscriber->PSubscribe(strPattern, std::move(fnCallback));
}

//***************************************************************************
// @brief 패턴 구독 해제(기본 노드)
//***************************************************************************
bool CRedisService::PUnsubscribe(const std::string& strPattern)
{
	return PUnsubscribe(_nDefaultNodeId.load(std::memory_order_acquire), strPattern);
}

//***************************************************************************
// @brief 패턴 구독 해제(지정 노드)
//***************************************************************************
bool CRedisService::PUnsubscribe(const int16 nNodeId, const std::string& strPattern)
{
	CRedisSubscriberRef pSubscriber = FindSubscriber(nNodeId, false);
	if( !pSubscriber )
		return false;

	return pSubscriber->PUnsubscribe(strPattern);
}

//***************************************************************************
// @brief 모든 노드의 구독 연결을 닫고 보관 목록을 비움
//***************************************************************************
void CRedisService::ShutdownSubscribers()
{
	std::unordered_map<int16, CRedisSubscriberRef> subscribers;
	{
		std::lock_guard<std::mutex> lock(_subscriberLock);
		subscribers.swap(_subscriberMap);
	}

	// Shutdown()은 재연결 스레드를 join하므로 락 밖에서 호출한다.
	for( auto& kv : subscribers )
	{
		if( kv.second )
			kv.second->Shutdown();
	}
}

//***************************************************************************
// @brief 노드의 구독 연결을 찾음. bCreate이면 없을 때 Init()에 보관한 노드 정보로 만듦
// @details 연결 생성(접속)은 블로킹 connect를 포함하므로 락을 잡은 채 하지 않는다.
//          동시에 두 스레드가 만들면 먼저 맵에 넣은 쪽이 이기고 나머지는 닫는다.
//***************************************************************************
CRedisSubscriberRef CRedisService::FindSubscriber(const int16 nNodeId, const bool bCreate)
{
	CRedisNode node;
	bool bNodeFound = false;
	{
		std::lock_guard<std::mutex> lock(_subscriberLock);

		auto it = _subscriberMap.find(nNodeId);
		if( it != _subscriberMap.end() )
			return it->second;

		if( !bCreate )
			return nullptr;

		for( const auto& item : _redisNodeVec )
		{
			if( item._nID == nNodeId )
			{
				node = item;
				bNodeFound = true;
				break;
			}
		}
	}

	if( !bNodeFound )
		return nullptr;

	auto pNew = std::make_shared<CRedisSubscriber>(_iocpCore, _jobQueue);
	if( !pNew->Init(TCharToString(node._tszDBHost), node._nPort, Redis::kSubscriberReconnectIntervalSec, TCharToString(node._tszDBUserId), TCharToString(node._tszDBPasswd)) )
	{
		LOG_ERROR(_T("CRedisService::FindSubscriber: failed to initialize subscriber. nodeId(%d), host(%s), port(%d)"),
			node._nID, node._tszDBHost, node._nPort);
		return nullptr;
	}

	CRedisSubscriberRef pExisting;
	{
		std::lock_guard<std::mutex> lock(_subscriberLock);

		auto it = _subscriberMap.find(nNodeId);
		if( it == _subscriberMap.end() )
		{
			_subscriberMap[nNodeId] = pNew;
			return pNew;
		}

		pExisting = it->second;
	}

	// 다른 스레드가 먼저 만들었다 — 이쪽은 락 밖에서 닫고 그쪽을 쓴다.
	pNew->Shutdown();
	return pExisting;
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