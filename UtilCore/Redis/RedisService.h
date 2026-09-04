
//***************************************************************************
// RedisService.h : interface for the CRedisService class.
//
//***************************************************************************

#ifndef UC_REDISSERVICE_H
#define UC_REDISSERVICE_H

#include <ServerConnectInfo.h>
#include <Redis/RedisRedefineDataType.h>
#include <Redis/RedisConnectionPool.h>
#include <Job/JobQueue.h>
#include <Containers/Map/ClusterSpinUnorderedMap.h>
#include <Util/EncodingConvert.h>

#include <atomic>
#include <utility>

//***************************************************************************
// @brief 외부 모듈에 노출되는 최상위 Redis 네트워크 서비스 파사드(Facade) 클래스
// @details IOCP 스레드에서 수신한 응답을 지정된 CJobQueue로 이관시켜 주므로
//          메인/로직 스레드에서 멀티스레드 동기화 문제 없이 안전하게 콜백 결과를 받을 수 있습니다.
//
//          Redis 노드 하나당 커넥션 풀을 하나씩 두고, CRedisNode::_nID를 키로
//          CClusterSpinUnorderedMap(_poolMap)에서 관리한다. 노드가 하나뿐인
//          목록으로 초기화한 경우에도 동일하게 그 하나의 풀만 맵에 들어가며,
//          SendCommand()는 노드를 지정하지 않아도 항상 첫 번째로 등록된
//          노드로 동작한다. _poolMap 자체가 클러스터별 락으로 조회/삽입을
//          스레드 세이프하게 처리하므로, 별도의 뮤텍스를 두지 않는다.
//
// @code
//	// 서버 설정 파일에서 읽어온 CRedisNode 목록으로 초기화(노드별로 풀이 하나씩 생김):
//	auto pJobQueue = std::make_shared<CJobQueue>();
//	CRedisService redisService(pIocpCore, pJobQueue);
//	redisService.Init(CServerConfig::GetInstance()->GetRedisNodeVec(), 5);
//	redisService.SendCommand({"HGETALL", "User:1"}, [](const RedisValue& res) {
//     // 로직/메인 스레드 안전 구역
//	});                                                                  // 첫 번째 노드로 감
//	redisService.SendCommand(nodeId, {"HGETALL", "User:1"}, ...);        // 지정한 노드로 감
// @endcode
//***************************************************************************
class CRedisService
{
public:
	CRedisService(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue);
	~CRedisService();

	//***************************************************************************
	// @brief 서버 설정 파일에서 읽어온 CRedisNode 목록으로 노드별 Redis 커넥션
	//        풀을 각각 초기화함
	// @details redisNodeVec의 각 노드마다 별도의 CRedisConnectionPool을 먼저
	//          전부 만들어본 뒤(하나라도 연결 실패하면 로컬 벡터가 스코프를
	//          벗어나며 그때까지 만든 풀들이 자동 정리되고 즉시 실패 반환 —
	//          일부 노드만 연결된 애매한 상태로 남기지 않기 위함), 전부 성공한
	//          경우에만 _poolMap에 CRedisNode::_nID를 키로 반영한다. 단,
	//          _poolMap의 초기화(ClearObjectMap)와 각 노드 삽입은 클러스터
	//          단위로 순차 수행되는 별개의 락 구간이라, 이 반영 과정 자체는
	//          원자적이지 않다(재초기화를 트래픽이 흐르는 도중에 호출하면
	//          아주 짧은 순간 맵이 비어있거나 일부만 채워진 상태로 보일 수
	//          있음). 보통 트래픽이 시작되기 전 한 번만 호출하는 용도이므로
	//          문제되지 않는다. 첫 번째 노드(redisNodeVec[0])는 SendCommand()를
	//          노드 지정 없이 호출했을 때 쓰이는 기본 노드로 등록된다.
	// @param redisNodeVec 서버 설정에서 읽어온 Redis 노드 목록(비어있으면 실패).
	//        서버 1대만 쓰려면 노드 하나짜리 목록을 넘기면 된다.
	// @param nPoolSize 노드별로 생성할 커넥션 풀 개수(모든 노드에 동일하게 적용)
	// @return 성공 여부 (true: 모든 노드 성공, false: 빈 목록이거나 하나라도 실패)
	//***************************************************************************
	bool Init(CVector<CRedisNode> redisNodeVec, const int32 nPoolSize);

	//***************************************************************************
	// @brief 기본 노드(Init()에 넘긴 목록의 첫 번째 노드)로 Redis 명령을 비동기 실행함
	// @param vecArgs Redis 명령어 인자 목록
	// @param fnMainThreadCallback 대상 스레드에서 안전하게 실행될 콜백 함수
	// @return 전송 성공 여부 (true: 성공, false: 실패 — 초기화되지 않은 경우 포함)
	//***************************************************************************
	bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback);

	//***************************************************************************
	// @brief 지정한 노드로 Redis 명령을 비동기 실행함(다중 노드 초기화 시 사용)
	// @param nNodeId Init(CVector<CRedisNode>, ...)로 등록된 CRedisNode::_nID
	// @param vecArgs Redis 명령어 인자 목록
	// @param fnMainThreadCallback 대상 스레드에서 안전하게 실행될 콜백 함수
	// @return 전송 성공 여부 (true: 성공, false: 실패 — 해당 노드가 없는 경우 포함)
	//***************************************************************************
	bool SendCommand(const int16 nNodeId, const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback);

private:
	CRedisConnectionPoolRef	FindPool(const int16 nNodeId);

private:
	CIocpCoreRef				_iocpCore;			// 노드별 풀을 만들 때 넘겨줄 IOCP 코어 참조
	CJobQueueRef				_jobQueue;			// 스레드 디스패칭용 JobQueue 참조

	// Redis 노드 개수는 보통 한 자릿수 수준이라, 클러스터 4개면 락 경합을
	// 분산하기에 충분하다. bInnerLock=true(기본값)라 조회/삽입/전체삭제가
	// 내부적으로 자동 락을 사용하므로 별도의 뮤텍스가 필요 없다.
	using TPoolMap = CClusterSpinUnorderedMap<int16, CRedisConnectionPoolRef, 4>;
	TPoolMap					_poolMap;			// 노드 ID → 커넥션 풀

	std::atomic<int16>			_nDefaultNodeId{ 0 };	// SendCommand()를 노드 지정 없이 호출했을 때 쓰이는 기본 노드 ID
};

#endif // ndef UC_REDISSERVICE_H