
//***************************************************************************
// RedisConnectionPool.h : interface for the RedisConnectionPool class.
//
//***************************************************************************

#ifndef UC_REDISCONNECTIONPOOL_H
#define UC_REDISCONNECTIONPOOL_H

#include <Network/IOCP/IocpCore.h>
#include <Redis/RedisRedefineDataType.h>
#include <Redis/RedisClient.h>

#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <random>

//***************************************************************************
// @brief 여러 개의 CRedisClient 연결 객체를 동시 분배/관리하는 커넥션 풀 클래스
// @details 요청 시 놀고 있는(Free) 커넥션을 대여하여 명령을 전송하고,
//          응답 콜백 시 해당 커넥션을 다시 큐로 자동 반납 처리합니다.
//
//          반납되는 커넥션은 연결 상태를 확인해 살아있으면 Free 큐로,
//          끊어져 있으면 격리 큐(_queueBroken)로 나뉘어 들어갑니다. 전용
//          백그라운드 스레드가 일정 주기로 격리 큐를 훑어 재연결을 시도하고,
//          성공한 커넥션만 다시 Free 큐로 돌려보냅니다 — 죽은 커넥션이
//          Free 큐에 섞여 매번 요청을 실패시키는 일이 없도록 합니다.
//
//          재연결 시도는 클라이언트별로 지수 백오프(+지터)를 적용합니다.
//          연속으로 실패할수록 다음 시도까지의 대기 시간이 늘어나(500ms에서
//          시작해 최대 30초까지), 서버가 오래 죽어있는 동안 스윕 주기마다
//          무의미하게 계속 두드리는 것을 막습니다.
//
// @code
// // 사용 예시:
// auto pPool = std::make_shared<CRedisConnectionPool>(pIocpCore);
// pPool->Init("127.0.0.1", 6379, /*nDbIndex*/ 0, 10); // 10개 커넥션 생성, 재연결 스레드 자동 시작
// pPool->SendCommand({"INCR", "VisitorCount"}, [](const RedisValue& res) {
//     // 완료 처리
// });
// @endcode
//***************************************************************************
class CRedisConnectionPool : public std::enable_shared_from_this<CRedisConnectionPool>
{
public:
	CRedisConnectionPool(CIocpCoreRef iocpCore);
	~CRedisConnectionPool();

	//***************************************************************************
	// @brief 지정된 풀 크기만큼 CRedisClient를 생성 및 연결하고, 백그라운드
	//        재연결 스레드를 시작함
	// @details 이미 초기화된 풀에 다시 호출해도 안전하다(재진입 시 내부적으로
	//          먼저 Clear()를 호출해 기존 재연결 스레드/커넥션을 완전히
	//          정리한 뒤 처음부터 다시 채운다).
	// @param strIP 서버 IP
	// @param nPort 서버 포트
	// @param nDbIndex 각 커넥션이 접속 직후 SELECT로 고정할 Redis 논리 DB
	//        인덱스(0이면 기본 DB, SELECT 생략). 재연결된 커넥션에도 동일하게
	//        적용된다
	// @param nPoolSize 생성할 커넥션 개수
	// @param nReconnectIntervalSec 격리 큐를 훑어 재연결을 시도하는 주기(초)
	// @return 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool        Init(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nPoolSize, const int32 nReconnectIntervalSec = 3);

	//***************************************************************************
	// @brief 재연결 스레드를 정지시키고 모든 커넥션을 끊어 풀 리소스를 정리함
	//***************************************************************************
	void        Clear();

	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);

private:
	CRedisClientRef		PopConnection();
	void                PushConnection(CRedisClientRef pClient);

	void                StartReconnectLoop();
	void                StopReconnectLoop();
	void                ReconnectLoop();

private:
	CIocpCoreRef                    _iocpCore;               // IOCP 코어 참조
	std::string                     _strIP;                  // 연결 대상 IP
	uint16                          _nPort = 0;              // 연결 대상 포트
	int32                           _nDbIndex = 0;           // 각 커넥션이 접속 직후 SELECT로 고정할 논리 DB 인덱스(0이면 생략)

	std::mutex                      _lock;					  // 풀 동기화 락 (_vecAllClients/_queueFree/_queueBroken 공용)
	CVector<CRedisClientRef>		_vecAllClients;          // 생성된 전체 클라이언트 리스트
	CQueue<CRedisClientRef>			_queueFree;              // 사용 가능한(연결된) 클라이언트 큐
	CQueue<CRedisClientRef>			_queueBroken;            // 연결이 끊겨 재연결을 기다리는 격리 큐

	std::atomic<bool>               _bInitialized{ false };  // 풀 초기화 여부 플래그

	//***************************************************************************
	// @brief 재연결 전용 스레드 및 동기화 객체
	//***************************************************************************
	int32                           _nReconnectIntervalSec = 3;	// 재연결 스레드가 격리 큐를 훑는 주기 (초 단위)
	std::thread                     _reconnectThread;				// 격리 큐 재연결을 담당하는 전용 스레드
	std::mutex                      _reconnectLock;				// _reconnectCv 대기/신호 전달 전용 뮤텍스
	std::condition_variable         _reconnectCv;					// 재연결 주기 대기 및 즉시 종료 신호 수신용 조건 변수
	std::atomic<bool>               _stopping{ false };			// 재연결 스레드 정지 요청 여부 플래그

	//***************************************************************************
	// @brief 클라이언트별 재연결 지수 백오프 상태
	// @details 격리 큐를 스윕할 때마다 무조건 재연결을 시도하면, 서버가 오래
	//          죽어있는 동안 계속 두드리게 된다(COdbcConnPool과 달리 이
	//          풀에는 별도 헬스체크/워커 스레드가 없고 재연결 스윕 자체가
	//          재시도 시점을 겸하므로, 백오프는 "이번 스윕에서 이 클라이언트를
	//          건너뛸지" 판단하는 형태로 구현한다). 오직 재연결 스레드만
	//          이 맵을 건드리므로(격리 큐에 있는 동안은 다른 누구도 해당
	//          클라이언트를 만지지 않음) 별도 락이 필요 없다.
	//***************************************************************************
	struct TReconnectBackoffState
	{
		int32                                   nFailCount = 0;		// 연속 재연결 실패 횟수
		std::chrono::steady_clock::time_point   nextRetryTime{};	// 이 시각 이전에는 재시도를 건너뜀
	};
	std::unordered_map<CRedisClient*, TReconnectBackoffState> _reconnectBackoffMap;	// 클라이언트 포인터 → 백오프 상태
};

#endif // ndef UC_REDISCONNECTIONPOOL_H