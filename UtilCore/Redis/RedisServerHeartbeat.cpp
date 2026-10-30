
//***************************************************************************
// RedisServerHeartbeat.cpp: implementation of the CRedisServerHeartbeat class.
//
//***************************************************************************

#include "pch.h"
#include "RedisServerHeartbeat.h"
#include <Redis/RedisHelper.h>
#include <Util/DateTimeUtil.h>

namespace
{
	// 등록과 TTL 갱신을 한 번에 — HSET만 성공하고 EXPIRE가 빠져 키가 TTL 없이 남는 순간이 없다.
	// KEYS[1]=등록 키, ARGV[1]=TTL(초), ARGV[2..]=필드/값 쌍. EVALSHA로 전송된다(CRedisScript).
	const CRedisScript kRegisterScript(
		"redis.call('HSET', KEYS[1], unpack(ARGV, 2))\n"
		"redis.call('EXPIRE', KEYS[1], ARGV[1])\n"
		"return 1");

}

//***************************************************************************
// @brief CRedisServerHeartbeat 생성자
// @param redisService 명령 전송에 사용할 Redis 서비스 (nullptr이면 Start()가 실패)
// @param serverName 서버 이름 (예: "ChatServer")
// @param serverGroupId 서버 그룹 식별자 (예: "GameServer", "LoginServer")
// @param serverChannelId 서버 채널(인스턴스) 식별자 (예: 서버 번호, 호스트명 등)
// @param port 클라이언트/내부 연동이 접속할 포트 (등록 정보용)
//***************************************************************************
CRedisServerHeartbeat::CRedisServerHeartbeat(CRedisService* redisService, std::string serverName, std::string serverGroupId, std::string serverChannelId, uint16 port)
	: _redisService(redisService)
	, _serverName(std::move(serverName))
	, _serverGroupId(std::move(serverGroupId))
	, _serverChannelId(std::move(serverChannelId))
	, _port(port)
{
}

//***************************************************************************
// @brief 소멸자 — 아직 실행 중이면 Stop()으로 정리합니다.
//***************************************************************************
CRedisServerHeartbeat::~CRedisServerHeartbeat()
{
	Stop();
}

//***************************************************************************
// @brief Redis 등록 키를 생성합니다.
// @return "{serverName}:{serverGroupId}:{serverChannelId}" 형식의 키 문자열
//***************************************************************************
std::string CRedisServerHeartbeat::BuildKey() const
{
	return _serverName + ":" + _serverGroupId + ":" + _serverChannelId;
}

//***************************************************************************
// @brief 등록 스크립트의 ARGV(TTL + 전체 필드/값 쌍)를 만듭니다.
// @details 최초 등록과 매 주기 갱신이 같은 필드 집합을 쓰므로, 키가 사라진 뒤
//          갱신이 키를 다시 만들더라도 항상 완전한 등록 정보가 된다.
//***************************************************************************
void CRedisServerHeartbeat::BuildRegistrationArgv(CVector<std::string>& argv, const int64 nowMs) const
{
	argv.reserve(argv.size() + 17);
	argv.push_back(std::to_string(_ttlSec));
	argv.push_back("serverName");	argv.push_back(_serverName);
	argv.push_back("serverGroupId");	argv.push_back(_serverGroupId);
	argv.push_back("serverChannelId");	argv.push_back(_serverChannelId);
	argv.push_back("port");		argv.push_back(std::to_string(_port));
	argv.push_back("pid");			argv.push_back(std::to_string(static_cast<int64>(::GetCurrentProcessId())));
	argv.push_back("sessionCount");	argv.push_back(std::to_string(_sessionCountProvider ? _sessionCountProvider() : 0));
	argv.push_back("startedAt");	argv.push_back(std::to_string(_startedAtMs));
	argv.push_back("updatedAt");	argv.push_back(std::to_string(nowMs));
}

//***************************************************************************
// @brief 최초 등록(HSET) 후 주기적 EXPIRE 갱신 스레드를 시작합니다.
//***************************************************************************
bool CRedisServerHeartbeat::Start(int32 ttlSec, int32 heartbeatIntervalSec)
{
	if( _redisService == nullptr )
		return false;

	// [추가 — 버그 수정] 이미 실행 중인(joinable한) 스레드가 있는 채로
	// 아래 "_thread = std::thread(...)"를 또 실행하면 std::terminate()로
	// 즉시 죽는다(joinable한 std::thread에 대한 이동 대입은 표준상
	// terminate 유발) — Stop()을 먼저 호출해 정지시키지 않은 채 Start()를
	// 두 번 부르는 실수를 방지한다.
	if( _thread.joinable() )
	{
		ASSERT_CRASH(false);
		return false;
	}

	// heartbeat 주기가 TTL보다 같거나 크면, 다음 갱신이 오기 전에 TTL이
	// 만료되어 실제로는 살아있는데 죽은 것처럼 보이는 창이 생긴다.
	if( heartbeatIntervalSec <= 0 || ttlSec <= heartbeatIntervalSec )
	{
		ASSERT_CRASH(false);
		return false;
	}

	_ttlSec = ttlSec;
	_heartbeatIntervalSec = heartbeatIntervalSec;
	_stopping.store(false);
	_startedAtMs = ptime::NowMillis();

	RegisterInitial();

	_thread = std::thread([this]() { HeartbeatLoop(); });
	_bStarted = true;
	return true;
}

//***************************************************************************
// @brief heartbeat 스레드를 정지시키고 Redis에서 등록 키를 즉시 삭제합니다.
//***************************************************************************
void CRedisServerHeartbeat::Stop()
{
	if( _stopping.exchange(true) )
		return; // 이미 정지 중이거나 정지됨 — 중복 호출 안전

	// 대기 뮤텍스를 한 번 거친 뒤 알린다. 스레드가 조건(_stopping)을 검사하고 대기에 들어가기
	// 직전에 알림이 끼면 유실되어, join이 heartbeat 주기만큼 지연될 수 있기 때문이다.
	{
		std::lock_guard<std::mutex> lock(_lock);
	}
	_cv.notify_all();

	if( _thread.joinable() )
		_thread.join();

	// Start()가 성공한 적이 없으면 등록한 키가 없다 — 같은 이름의 다른 인스턴스 키를 지우지 않도록 건너뛴다.
	if( !_bStarted )
		return;
	_bStarted = false;

	// 정상 종료 시 등록 키를 즉시 지워 TTL 만료를 기다리지 않고 디스커버리
	// 목록에서 바로 빠지도록 한다. 이 호출은 응답을 기다리지 않는다 — 서비스(풀)를 곧바로
	// 해제하면 전송되기 전에 끊길 수 있고, 그 경우에도 TTL이 결국 정리해주므로 치명적이지 않다.
	if( _redisService != nullptr )
	{
		_redisService->SendCommand(Redis::MakeArgs("DEL", BuildKey()), [](const RedisValue& /*res*/) {});
	}
}

//***************************************************************************
// @brief 서버 정보를 Redis Hash로 최초 등록하고 TTL을 설정합니다.
// @details 등록(HSET)과 TTL(EXPIRE)을 하나의 스크립트로 보내므로, 응답을 기다렸다가
//          EXPIRE를 따로 거는 콜백이 없다 — 객체가 먼저 소멸해도 건드릴 멤버가 없다.
//***************************************************************************
void CRedisServerHeartbeat::RegisterInitial()
{
	PublishRegistration();
}

//***************************************************************************
// @brief 등록 정보 전체 재기록(HSET) + TTL 갱신(EXPIRE)을 스크립트 하나로 게시합니다.
// @details 키가 TTL 만료나 Redis 재시작으로 사라졌더라도 이 스크립트가 완전한 등록
//          정보로 복원한다. 두 명령이 한 스크립트 안에서 원자적으로 실행된다.
//***************************************************************************
void CRedisServerHeartbeat::SendHeartbeat()
{
	PublishRegistration();
}

//***************************************************************************
// @brief 등록 스크립트를 실행합니다(RegisterInitial/SendHeartbeat 공통).
//***************************************************************************
void CRedisServerHeartbeat::PublishRegistration()
{
	CVector<std::string> argv;
	BuildRegistrationArgv(argv, ptime::NowMillis());

	kRegisterScript.Run(*_redisService, { BuildKey() }, argv);
}

//***************************************************************************
// @brief heartbeat 전용 스레드 루프.
// @details condition_variable::wait_for로 인터벌만큼 대기하되, Stop()이
//          notify하면 대기를 즉시 끝내고 루프를 탈출한다(sleep_for와 달리
//          정지 요청에 즉각 반응).
//***************************************************************************
void CRedisServerHeartbeat::HeartbeatLoop()
{
	std::unique_lock<std::mutex> guard(_lock);

	while( !_stopping.load() )
	{
		const bool stoppedDuringWait = _cv.wait_for(
			guard,
			std::chrono::seconds(_heartbeatIntervalSec),
			[this] { return _stopping.load(); });

		if( stoppedDuringWait )
			break; // Stop() 요청으로 깨어남

		// 타임아웃으로 깨어남 — heartbeat 게시. SendCommand()는 락이 필요
		// 없으므로 unlock/lock 왕복 없이 그대로 호출한다(락은 오직
		// condition_variable::wait_for 계약을 위한 것).
		SendHeartbeat();
	}
}