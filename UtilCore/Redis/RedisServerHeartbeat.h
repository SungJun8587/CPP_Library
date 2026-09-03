
//***************************************************************************
// RedisServerHeartbeat.h : interface for the CRedisServerHeartbeat class.
//
//***************************************************************************

#ifndef UC_REDISSERVERHEARTBEAT_H
#define UC_REDISSERVERHEARTBEAT_H

#include <Redis/RedisService.h>

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

//***************************************************************************
// @class CRedisServerHeartbeat
// @brief 서버 시작 시 Redis에 자신의 실행 상태를 등록(HSET)하고, 주기적으로
//        TTL을 갱신(EXPIRE)하여 "살아있음"을 알리는 클래스.
//
// @details
// 등록 키: "Server:{serverType}:{serverId}" (Redis Hash)
//     필드: serverType, serverId, port, pid, startedAt, updatedAt
//
// TTL 기반 자동 정리:
//     kTtlSec(살아있음으로 간주할 시간)을 걸어두고, 그보다 충분히 짧은
//     주기(kHeartbeatIntervalSec)마다 EXPIRE로 TTL을 갱신한다. 프로세스가
//     비정상 종료(크래시 등)되면 heartbeat가 끊기고 TTL 만료로 Redis에서
//     해당 키가 자동 소멸 — 디스커버리/모니터링 쪽에서 별도 정리 배치 없이도
//     "죽은 서버" 항목이 자연스럽게 사라진다.
//
// 정상 종료 시:
//     Stop()이 heartbeat 스레드를 즉시 멈춘 뒤 DEL로 키를 바로 지워, TTL
//     만료를 기다리지 않고 디스커버리 목록에서 즉시 빠지도록 한다.
//
// 스레드 모델:
//     전용 스레드 하나가 condition_variable::wait_for로 인터벌만큼 대기 후
//     깨어나 EXPIRE 명령을 게시한다. Stop()은 wait_for를 즉시 깨워 sleep
//     경과를 기다리지 않고 스레드를 종료시킨다.
//
// CRedisService::SendCommand()의 콜백은 IOCP 워커 스레드에서 CJobQueue로
// 이관되어 실행되므로, 이 클래스 내부 상태는 heartbeat 스레드/콜백 양쪽에서
// 손대지 않는다(콜백은 결과를 읽기만 함) — 별도 락 없이 안전.
//
// @code
//	// 사용 예시:
//	CRedisService redisService(pIocpCore, pJobQueue);
//	redisService.Init("127.0.0.1", 6379, 5);
//
//	CRedisServerHeartbeat heartbeat(&redisService, "GameServer", "1", 7777);
//	heartbeat.Start(15, 5);   // TTL 15초, 5초마다 갱신
//	...
//	heartbeat.Stop();         // 서버 종료 시
// @endcode
//***************************************************************************
class CRedisServerHeartbeat
{
public:
	CRedisServerHeartbeat(CRedisService* redisService, std::string serverType, std::string serverId, uint16 port);
	~CRedisServerHeartbeat();

	CRedisServerHeartbeat(const CRedisServerHeartbeat&) = delete;
	CRedisServerHeartbeat& operator=(const CRedisServerHeartbeat&) = delete;

	//***************************************************************************
	// @brief Redis에 최초 등록(HSET) 후 주기적 EXPIRE 갱신 스레드를 시작합니다.
	// @param ttlSec 살아있음으로 간주할 TTL(초).
	// @param heartbeatIntervalSec heartbeat(EXPIRE 갱신) 주기(초). ttlSec보다
	//        충분히 작아야 합니다(같거나 크면 갱신 전에 TTL이 만료되는 창이 생김).
	// @return 파라미터가 유효하고 스레드 시작에 성공하면 true.
	//***************************************************************************
	bool Start(int32 ttlSec = 15, int32 heartbeatIntervalSec = 5);

	//***************************************************************************
	// @brief heartbeat 스레드를 정지시키고 Redis에서 등록 키를 즉시 삭제합니다.
	// @details 중복 호출에 안전(idempotent)합니다.
	//***************************************************************************
	void Stop();

private:
	void			RegisterInitial();
	void			SendHeartbeat();
	void			HeartbeatLoop();
	std::string		BuildKey() const;

private:
	//***************************************************************************
	// @brief Redis 및 서버 식별 정보
	//***************************************************************************
	CRedisService* _redisService = nullptr;			// Redis 커넥션 관리 서비스 포인터
	std::string			_serverType;				// 서버 종류 (예: GameServer, MasterServer)
	std::string			_serverId;					// 서버 ID (예: 1, 2, 101)
	uint16				_port = 0;					// 서버 바인딩 포트 번호

	//***************************************************************************
	// @brief 주기 및 TTL 설정
	//***************************************************************************
	int32				_ttlSec = 15;				// 살아있음으로 간주할 시간 (초 단위)
	int32				_heartbeatIntervalSec = 5;	// Heartbeat(EXPIRE) 갱신 주기 (초 단위)

	//***************************************************************************
	// @brief 스레드 및 동기화 객체
	//***************************************************************************
	std::thread				_thread;				// Heartbeat 주기적 갱신을 담당하는 전용 스레드
	std::mutex				_lock;                  // _cv 조건 변수의 대기/신호 전달 전용 뮤텍스
	std::condition_variable	_cv;					// Heartbeat 주기 대기 및 즉시 종료 신호 수신용 조건 변수
	std::atomic<bool>		_stopping{ false };		// 스레드 정지 요청 여부 플래그 (스레드 간 원자적 공유)
};

#endif // ndef UC_REDISSERVERHEARTBEAT_H
