
//***************************************************************************
// RedisServerHeartbeat.h : interface for the CRedisServerHeartbeat class.
//
//***************************************************************************

#ifndef UC_REDISSERVERHEARTBEAT_H
#define UC_REDISSERVERHEARTBEAT_H

#include <Redis/RedisService.h>
#include <Util/DateTimeUtil.h>

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>

//***************************************************************************
// @class CRedisServerHeartbeat
// @brief 서버 시작 시 Redis에 자신의 실행 상태를 등록(HSET)하고, 주기적으로
//        TTL을 갱신(EXPIRE)하여 "살아있음"을 알리는 클래스.
//
// @details
// 등록 키: "{serverName}:{serverGroupId}:{serverChannelId}" (Redis Hash)
//     필드: serverName, serverGroupId, serverChannelId, port, pid, sessionCount, startedAt, updatedAt
//
// TTL 기반 자동 정리:
//     kTtlSec(살아있음으로 간주할 시간)을 걸어두고, 그보다 충분히 짧은
//     주기(kHeartbeatIntervalSec)마다 EXPIRE로 TTL을 갱신한다. 프로세스가
//     비정상 종료(크래시 등)되면 heartbeat가 끊기고 TTL 만료로 Redis에서
//     해당 키가 자동 소멸 — 디스커버리/모니터링 쪽에서 별도 정리 배치 없이도
//     "죽은 서버" 항목이 자연스럽게 사라진다.
//
// 등록 정보 복원:
//     매 주기 갱신은 updatedAt/sessionCount만 쓰지 않고 등록 필드 전체를 HSET으로
//     다시 쓴다. Redis 장애/재시작이나 TTL 만료로 키가 사라졌다가 복구돼도
//     일부 필드만 가진 불완전한 항목이 아니라 완전한 등록 정보로 되살아난다.
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
// [추가 — lifecycle 계약] Start()는 Stop()으로 먼저 정지시키지 않은 채로
// 두 번 연달아 호출하면 안 된다 — 이미 실행 중인 스레드가 있는 상태에서
// 또 스레드를 만들려 하면 표준상 std::terminate()가 유발된다(아래
// Start() 선언부 설명 참고).
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
	CRedisServerHeartbeat(CRedisService* redisService, std::string serverName, std::string serverGroupId, std::string serverChannelId, uint16 port);
	~CRedisServerHeartbeat();

	CRedisServerHeartbeat(const CRedisServerHeartbeat&) = delete;
	CRedisServerHeartbeat& operator=(const CRedisServerHeartbeat&) = delete;

	//***************************************************************************
	// @brief 매 heartbeat 갱신마다(최초 등록 + 이후 매 주기) 호출해 "지금 이
	//        서버의 세션 수(동접자수)"를 물어볼 콜백을 등록합니다.
	// @details 콜백은 heartbeat 전용 스레드에서 호출된다(RegisterInitial()/
	//          SendHeartbeat()가 그 스레드에서 실행됨) — 세션 매니저의
	//          GetSessionCount()는 락 기반 스냅샷 카운트라 다른 스레드에서
	//          불러도 안전하다. Start() 호출 *전에* 등록해야 최초 등록
	//          (RegisterInitial())부터 값이 반영된다 — 등록하지 않으면
	//          (또는 nullptr이면) sessionCount 필드는 항상 0으로 채워진다.
	//***************************************************************************
	using SessionCountProvider = std::function<int32()>;
	void SetSessionCountProvider(SessionCountProvider provider) { _sessionCountProvider = std::move(provider); }

	//***************************************************************************
	// @brief Redis에 최초 등록(HSET) 후 주기적 EXPIRE 갱신 스레드를 시작합니다.
	// @param ttlSec 살아있음으로 간주할 TTL(초).
	// @param heartbeatIntervalSec heartbeat(EXPIRE 갱신) 주기(초). ttlSec보다
	//        충분히 작아야 합니다(같거나 크면 갱신 전에 TTL이 만료되는 창이 생김).
	// @return 파라미터가 유효하고 스레드 시작에 성공하면 true. 이미 실행
	//         중이면(Stop()을 안 부르고 재호출) false.
	// @details [수정 — 버그 수정: 중복 Start() 크래시] 이미 실행 중인(joinable한)
	//          heartbeat 스레드가 있는 상태에서 다시 이 함수를 호출하면,
	//          예전엔 그 스레드에 새 std::thread를 그대로 대입하려다
	//          std::terminate()로 죽었다(joinable한 std::thread에 대한
	//          이동 대입은 표준상 terminate 유발). 이제 이미 실행 중이면
	//          즉시 false를 반환하고 아무 것도 하지 않는다 — Stop()을 먼저
	//          호출해 완전히 정지시킨 뒤에만 재시작할 수 있다.
	//***************************************************************************
	bool Start(int32 ttlSec = 15, int32 heartbeatIntervalSec = 5);

	//***************************************************************************
	// @brief heartbeat 스레드를 정지시키고 Redis에서 등록 키를 즉시 삭제합니다.
	// @details 중복 호출에 안전(idempotent)합니다.
	//***************************************************************************
	void Stop();

private:
	void			RegisterInitial();
	void			BuildRegistrationArgs(CVector<std::string>& args, int64 nowMs) const;
	void			SendHeartbeat();
	void			HeartbeatLoop();
	std::string		BuildKey() const;

private:
	CRedisService* _redisService = nullptr;	// 명령 전송 대상(비소유) — nullptr이면 Start()가 실패
	std::string				_serverName;				// 등록 키/HSET의 serverName 필드로 그대로 쓰임(예: "ChatServer")
	std::string				_serverGroupId;				// 등록 키/HSET의 serverGroupId 필드로 그대로 쓰임(예: 서버군 번호)
	std::string				_serverChannelId;			// 등록 키/HSET의 serverChannelId 필드로 그대로 쓰임(예: 서버 인스턴스 번호)
	uint16					_port = 0;					// 클라이언트/내부 연동이 접속할 포트(등록 정보용, HSET의 port 필드)

	int32					_ttlSec = 15;				// Start()에서 넘겨받아 저장 — EXPIRE에 매번 이 값을 씀
	int32					_heartbeatIntervalSec = 5;	// Start()에서 넘겨받아 저장 — HeartbeatLoop()의 대기 주기로 씀
	int64					_startedAtMs = 0;			// Start() 시각(epoch ms) — 매 갱신의 HSET에 같은 값으로 기록됨

	SessionCountProvider	_sessionCountProvider;	// 설정 안 하면(nullptr) sessionCount 필드는 항상 0

	std::thread				_thread;					// HeartbeatLoop() 전용 워커 스레드 — Start()에서 기동, Stop()에서 join
	std::mutex				_lock;               // wait_for 전용 (heartbeat 로직 자체엔 동기화 불필요)
	std::condition_variable	_cv;					// Stop()이 notify_all()로 대기 중인 _thread를 즉시 깨워 종료시키는 용도
	std::atomic<bool>		_stopping{ false };			// Stop() 중복 호출 방지 + HeartbeatLoop() 탈출 조건
};

#endif // ndef UC_REDISSERVERHEARTBEAT_H