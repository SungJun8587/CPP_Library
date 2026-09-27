
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
//          [추가 — lifecycle 계약] Init()은 서버 시작(또는 재시작) 시,
//          트래픽이 흐르기 시작하기 전에 호출하는 것을 전제로 한다. 정상
//          서비스 운영 중에는 Init()을 재호출하지 않는다 — 재호출하면
//          _poolMap을 비우고 다시 채우는 짧은 구간 동안 동시에 실행 중인
//          SendCommand()가 일부만 채워진 맵을 볼 수 있다(_poolMap 자체의
//          개별 연산은 스레드 세이프하지만, "비우기+채우기" 전체가 하나의
//          원자적 전환은 아니다). 이 계약 위에서는 이 부분을 별도로
//          잠그지 않는다 — 그렇게 하려면 풀 전체를 감싸는 락이 필요해져
//          평상시 SendCommand()의 락프리 조회 이점이 사라진다.
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
	//          경우에만 _poolMap에 CRedisNode::_nID를 키로 반영한다. 첫 번째
	//          노드(redisNodeVec[0])는 SendCommand()를 노드 지정 없이 호출했을
	//          때 쓰이는 기본 노드로 등록된다.
	// @details [수정 — 버그 수정: 중복 Node ID] 예전엔 redisNodeVec에 같은
	//          _nID가 중복돼 있어도 InsertObject() 실패를 로그만 남기고
	//          그냥 true를 반환했다 — "모든 노드 성공"이라는 이 함수의
	//          계약과 실제 동작(중복된 노드 하나가 조용히 누락됨)이
	//          어긋났고, 게다가 그 시점엔 이미 중복 노드의 Redis 연결까지
	//          전부 만들어놓은 뒤였다(외부 리뷰로 발견). 이제 실제 연결을
	//          시도하기 전에 먼저 ID 중복을 검사해서, 있으면 즉시
	//          실패시킨다.
	// @details [추가] nPoolSize/_iocpCore/_jobQueue에 대한 방어적 검증을
	//          추가했다 — 예전엔 이 값들이 잘못됐을 때 훨씬 더 아래
	//          단계(CRedisConnectionPool::Init() 내부, 또는 그보다도 더
	//          깊은 곳)에서야 실패하거나, 심하면 null 포인터 역참조로
	//          크래시가 날 수 있었다. 여기서 미리 걸러 원인을 바로 알 수
	//          있게 한다.
	// @param redisNodeVec 서버 설정에서 읽어온 Redis 노드 목록(비어있으면 실패).
	//        서버 1대만 쓰려면 노드 하나짜리 목록을 넘기면 된다.
	// @param nPoolSize 노드별로 생성할 커넥션 풀 개수(모든 노드에 동일하게 적용,
	//        0 이하면 실패)
	// @return 성공 여부 (true: 모든 노드 성공, false: 빈 목록/잘못된 인자/
	//         중복 ID/하나라도 연결 실패)
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
	// @brief 지정한 노드로 Redis 명령을 비동기 실행함
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