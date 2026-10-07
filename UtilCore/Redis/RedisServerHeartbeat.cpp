
//***************************************************************************
// RedisServerHeartbeat.cpp: implementation of the CRedisServerHeartbeat class.
//
//***************************************************************************

#include "pch.h"
#include "RedisServerHeartbeat.h"

namespace
{
	int64 NowMs()
	{
		return static_cast<int64>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
	}
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
// @return "Server:{serverGroupId}:{serverChannelId}" 형식의 키 문자열
//***************************************************************************
std::string CRedisServerHeartbeat::BuildKey() const
{
	return _serverName + ":" + _serverGroupId + ":" + _serverChannelId;
}

//***************************************************************************
// @brief 등록 키의 전체 필드를 쓰는 HSET 명령 인자를 만듭니다.
// @details 최초 등록과 매 주기 갱신이 같은 필드 집합을 쓰므로, 키가 사라진 뒤
//          갱신이 키를 다시 만들더라도 항상 완전한 등록 정보가 된다.
//***************************************************************************
void CRedisServerHeartbeat::BuildRegistrationArgs(CVector<std::string>& args, const int64 nowMs) const
{
	args.push_back("HSET");
	args.push_back(BuildKey());
	args.push_back("serverName");	args.push_back(_serverName);
	args.push_back("serverGroupId");	args.push_back(_serverGroupId);
	args.push_back("serverChannelId");	args.push_back(_serverChannelId);
	args.push_back("port");		args.push_back(std::to_string(_port));
	args.push_back("pid");			args.push_back(std::to_string(static_cast<int64>(::GetCurrentProcessId())));
	args.push_back("sessionCount");	args.push_back(std::to_string(_sessionCountProvider ? _sessionCountProvider() : 0));
	args.push_back("startedAt");	args.push_back(std::to_string(_startedAtMs));
	args.push_back("updatedAt");	args.push_back(std::to_string(nowMs));
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
	_startedAtMs = NowMs();

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
// @details [수정 — 버그 수정: use-after-free 위험] 예전엔 HSET 완료 콜백이
//          [this, key, ttlSec]를 캡처해서, 콜백 안에서 EXPIRE를 걸 때
//          "_redisService->SendCommand(...)"라고 썼다 — 이건 암묵적으로
//          this->_redisService에 접근하는 것과 같다. 이 HSET은 Start()가
//          호출되는 시점에 곧바로 비동기로 걸리는데, 그 응답이 오기 전에
//          이 CRedisServerHeartbeat 객체 자체가 소멸되면(예: 서버 시작
//          직후 어떤 이유로 바로 종료되는 경우) 콜백이 이미 죽은 this를
//          통해 멤버에 접근하게 된다 — ~CRedisServerHeartbeat()가 부르는
//          Stop()은 heartbeat *스레드*만 join할 뿐, 이렇게 이미 날아간
//          개별 Redis 요청까지 기다려주지는 않는다(SendHeartbeat()의
//          콜백들은 애초에 아무것도 캡처하지 않아 이 문제가 없었는데,
//          이 함수만 그랬다). _redisService(비소유 raw pointer)를 로컬
//          변수로 복사해서 그 값 자체를 캡처하도록 바꿨다 — 이러면
//          콜백이 this가 아니라 포인터 값 하나에만 의존하므로, this가
//          먼저 소멸돼도 안전하다(다만 _redisService가 가리키는 객체
//          자체의 수명은 여전히 호출부가 보장해야 한다 — 이 클래스의
//          원래 raw pointer 계약 그대로).
//***************************************************************************
void CRedisServerHeartbeat::RegisterInitial()
{
	const std::string key = BuildKey();

	CVector<std::string> args;
	BuildRegistrationArgs(args, NowMs());

	const int32 ttlSec = _ttlSec;
	CRedisService* redisService = _redisService; // [수정] this 대신 이 값 자체를 캡처

	// HSET 완료 콜백 안에서 EXPIRE를 게시 — HSET이 실패한 채로 TTL만 걸려
	// "내용 없는" 키가 남는 것을 피하기 위함. (RedisValue의 성공/에러 판별
	// API가 이 헤더만으론 확인이 안 돼, 여기선 HSET 콜백이 왔다는 것 자체를
	// "완료됨"으로 보고 무조건 EXPIRE를 건다 — 프로젝트에 에러 체크 메서드가
	// 있다면 이 콜백 안에서 감싸는 것을 권장한다. TODO)
	redisService->SendCommand(args, [redisService, key, ttlSec](const RedisValue& /*res*/)
		{
			CVector<std::string> expireArgs;
			expireArgs.push_back("EXPIRE");
			expireArgs.push_back(key);
			expireArgs.push_back(std::to_string(ttlSec));

			redisService->SendCommand(expireArgs, [](const RedisValue& /*res*/) {});
		});
}

//***************************************************************************
// @brief 등록 정보 전체 재기록(HSET) + TTL 갱신(EXPIRE)을 게시합니다.
// @details 두 커맨드는 서로 다른 왕복(RTT)이라 완전한 원자성은 없다(HSET
//          성공, EXPIRE 실패 시 TTL이 이번 틱엔 안 갱신될 수 있음). 다만
//          heartbeat 자체가 짧은 주기로 반복되므로 다음 틱에서 자연히
//          복구된다 — Lua 스크립트/MULTI로 원자화할 실익이 낮다고 판단해
//          단순한 2회 SendCommand()로 구현했다.
//***************************************************************************
void CRedisServerHeartbeat::SendHeartbeat()
{
	const std::string key = BuildKey();

	// 갱신마다 등록 필드 전체를 다시 쓴다 — 키가 TTL 만료나 Redis 재시작으로 사라졌더라도
	// 이 HSET이 완전한 등록 정보로 복원한다.
	CVector<std::string> hsetArgs;
	BuildRegistrationArgs(hsetArgs, NowMs());
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