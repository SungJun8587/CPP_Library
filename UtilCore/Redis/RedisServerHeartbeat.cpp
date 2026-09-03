
//***************************************************************************
// RedisServerHeartbeat.cpp: implementation of the CRedisServerHeartbeat class.
//
//***************************************************************************

#include "pch.h"
#include "RedisServerHeartbeat.h"

#include <chrono>

//***************************************************************************
// @brief CRedisServerHeartbeat 생성자
// @param redisService 명령 전송에 사용할 Redis 서비스 (nullptr이면 Start()가 실패)
// @param serverType 서버 종류 식별자 (예: "GameServer", "LoginServer")
// @param serverId 서버 인스턴스 식별자 (예: 서버 번호, 호스트명 등)
// @param port 클라이언트/내부 연동이 접속할 포트 (등록 정보용)
//***************************************************************************
CRedisServerHeartbeat::CRedisServerHeartbeat(CRedisService* redisService, std::string serverType, std::string serverId, uint16 port)
	: _redisService(redisService)
	, _serverType(std::move(serverType))
	, _serverId(std::move(serverId))
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
// @return "Server:{serverType}:{serverId}" 형식의 키 문자열
//***************************************************************************
std::string CRedisServerHeartbeat::BuildKey() const
{
	return "Server:" + _serverType + ":" + _serverId;
}

//***************************************************************************
// @brief 최초 등록(HSET) 후 주기적 EXPIRE 갱신 스레드를 시작합니다.
//***************************************************************************
bool CRedisServerHeartbeat::Start(int32 ttlSec, int32 heartbeatIntervalSec)
{
	if( _redisService == nullptr )
		return false;

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

	RegisterInitial();

	_thread = std::thread([this]() { HeartbeatLoop(); });
	return true;
}

//***************************************************************************
// @brief heartbeat 스레드를 정지시키고 Redis에서 등록 키를 즉시 삭제합니다.
//***************************************************************************
void CRedisServerHeartbeat::Stop()
{
	if( _stopping.exchange(true) )
		return; // 이미 정지 중이거나 정지됨 — 중복 호출 안전

	_cv.notify_all();

	if( _thread.joinable() )
		_thread.join();

	// 정상 종료 시 등록 키를 즉시 지워 TTL 만료를 기다리지 않고 디스커버리
	// 목록에서 바로 빠지도록 한다. 이 호출이 실패해도 TTL이 결국 정리해주므로
	// 치명적이지 않다.
	if( _redisService != nullptr )
	{
		CVector<std::string> args;
		args.push_back("DEL");
		args.push_back(BuildKey());

		_redisService->SendCommand(args, [](const RedisValue& /*res*/) {});
	}
}

//***************************************************************************
// @brief 서버 정보를 Redis Hash로 최초 등록하고 TTL을 설정합니다.
//***************************************************************************
void CRedisServerHeartbeat::RegisterInitial()
{
	const std::string key = BuildKey();

	const int64 nowMs = static_cast<int64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());

	CVector<std::string> args;
	args.push_back("HSET");
	args.push_back(key);
	args.push_back("serverType");	args.push_back(_serverType);
	args.push_back("serverId");	args.push_back(_serverId);
	args.push_back("port");		args.push_back(std::to_string(_port));
	args.push_back("pid");			args.push_back(std::to_string(static_cast<int64>(::GetCurrentProcessId())));
	args.push_back("startedAt");	args.push_back(std::to_string(nowMs));
	args.push_back("updatedAt");	args.push_back(std::to_string(nowMs));

	const int32 ttlSec = _ttlSec;

	// HSET 완료 콜백 안에서 EXPIRE를 게시 — HSET이 실패한 채로 TTL만 걸려
	// "내용 없는" 키가 남는 것을 피하기 위함. (RedisValue의 성공/에러 판별
	// API가 이 헤더만으론 확인이 안 돼, 여기선 HSET 콜백이 왔다는 것 자체를
	// "완료됨"으로 보고 무조건 EXPIRE를 건다 — 프로젝트에 에러 체크 메서드가
	// 있다면 이 콜백 안에서 감싸는 것을 권장한다. TODO)
	_redisService->SendCommand(args, [this, key, ttlSec](const RedisValue& /*res*/)
		{
			CVector<std::string> expireArgs;
			expireArgs.push_back("EXPIRE");
			expireArgs.push_back(key);
			expireArgs.push_back(std::to_string(ttlSec));

			_redisService->SendCommand(expireArgs, [](const RedisValue& /*res*/) {});
		});
}

//***************************************************************************
// @brief updatedAt 필드 갱신 + TTL 갱신을 게시합니다.
// @details 두 커맨드는 서로 다른 왕복(RTT)이라 완전한 원자성은 없다(HSET
//          성공, EXPIRE 실패 시 TTL이 이번 틱엔 안 갱신될 수 있음). 다만
//          heartbeat 자체가 짧은 주기로 반복되므로 다음 틱에서 자연히
//          복구된다 — Lua 스크립트/MULTI로 원자화할 실익이 낮다고 판단해
//          단순한 2회 SendCommand()로 구현했다.
//***************************************************************************
void CRedisServerHeartbeat::SendHeartbeat()
{
	const std::string key = BuildKey();

	const int64 nowMs = static_cast<int64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());

	CVector<std::string> hsetArgs;
	hsetArgs.push_back("HSET");
	hsetArgs.push_back(key);
	hsetArgs.push_back("updatedAt");
	hsetArgs.push_back(std::to_string(nowMs));
	_redisService->SendCommand(hsetArgs, [](const RedisValue& /*res*/) {});

	CVector<std::string> expireArgs;
	expireArgs.push_back("EXPIRE");
	expireArgs.push_back(key);
	expireArgs.push_back(std::to_string(_ttlSec));
	_redisService->SendCommand(expireArgs, [](const RedisValue& /*res*/) {});
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
