
//***************************************************************************
// RedisConnectionPool.cpp: implementation of the CRedisConnectionPool class.
//
//***************************************************************************

#include "pch.h"
#include "RedisConnectionPool.h"

namespace
{
	// 클라이언트별 재연결 지수 백오프 기본값. COdbcConnPool::TReconnectConfig의
	// 기본값과 동일한 값을 사용해 두 풀의 재연결 체감 동작을 맞춘다.
	constexpr int64 RECONNECT_BACKOFF_BASE_MS = 500;		// 최초 재시도 대기 기본 시간
	constexpr int64 RECONNECT_BACKOFF_MAX_MS = 30000;		// 재시도 대기 시간 상한선
	constexpr int32 RECONNECT_BACKOFF_MAX_SHIFT = 6;		// 백오프 지수 증가 횟수 상한
	constexpr int32 RECONNECT_BACKOFF_JITTER_MS = 250;		// 동시 재시도 충돌 방지를 위한 지터 최대값
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisConnectionPool 생성자
// @param iocpCore IOCP 코어 참조 객체
//***************************************************************************
CRedisConnectionPool::CRedisConnectionPool(CIocpCoreRef iocpCore)
	: _iocpCore(iocpCore)
{
}

//***************************************************************************
// @brief CRedisConnectionPool 소멸자
//***************************************************************************
CRedisConnectionPool::~CRedisConnectionPool()
{
	Clear();
}

//***************************************************************************
// @brief 지정된 풀 크기만큼 CRedisClient를 생성 및 연결하고, 백그라운드
//        재연결 스레드를 시작함
// @param strIP 서버 IP
// @param nPort 서버 포트
// @param nDbIndex 각 커넥션이 접속 직후 SELECT로 고정할 Redis 논리 DB 인덱스
// @param nPoolSize 생성할 커넥션 개수
// @param nReconnectIntervalSec 격리 큐를 훑어 재연결을 시도하는 주기(초)
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisConnectionPool::Init(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nPoolSize, const int32 nReconnectIntervalSec)
{
	// 이미 초기화된 풀에 Init()이 다시 호출되는 경우(재초기화)에 대비해
	// 먼저 완전히 정리한다. 처음 호출되는 경우엔 재연결 스레드가 시작된
	// 적이 없고 큐도 비어있어 사실상 아무 일도 하지 않는다. 이 선행 정리가
	// 없으면, 이미 돌고 있는 _reconnectThread 위에 StartReconnectLoop()이
	// 새 std::thread를 그대로 대입하려다 std::terminate()로 죽는다
	// (joinable한 std::thread에 대한 이동 대입은 표준상 terminate 유발).
	Clear();

	// strIP/_nPort/_nDbIndex/_nReconnectIntervalSec 설정과 _vecAllClients/
	// _queueFree에 대한 반영만 _lock으로 짧게 감싸고, 실제 블로킹 작업인
	// Connect()(소켓 연결 + 필요 시 SELECT 응답 대기, 클라이언트당 최대
	// 수 초)는 락 밖에서 수행한다. ReconnectLoop()과 동일한 패턴 —
	// 그렇지 않으면 풀을 채우는 동안 다른 스레드의 PopConnection()/
	// PushConnection()/SendCommand()가 전부 이 락에 막혀 대기하게 된다.
	{
		std::lock_guard<std::mutex> lock(_lock);
		_strIP = strIP;
		_nPort = nPort;
		_nDbIndex = nDbIndex;
		_nReconnectIntervalSec = (nReconnectIntervalSec > 0) ? nReconnectIntervalSec : 3;
	}

	for( int32 i = 0; i < nPoolSize; ++i )
	{
		auto pClient = std::make_shared<CRedisClient>(_iocpCore);
		if( !pClient->Connect(strIP, nPort, nDbIndex) )
		{
			Clear();
			return false;
		}

		std::lock_guard<std::mutex> lock(_lock);
		_vecAllClients.push_back(pClient);
		_queueFree.push(pClient);
	}

	_bInitialized = true;

	StartReconnectLoop();
	return true;
}

//***************************************************************************
// @brief 재연결 스레드를 정지시키고 모든 커넥션을 끊어 풀 리소스를 정리함
//***************************************************************************
void CRedisConnectionPool::Clear()
{
	// 재연결 스레드가 _lock을 필요로 하므로, _lock을 잡기 전에 먼저 스레드를
	// 정지/조인해야 한다(그렇지 않으면 여기서 _lock을 쥔 채로 join()이 스레드가
	// _lock을 얻을 때까지 서로 기다리는 데드락이 생긴다).
	StopReconnectLoop();

	std::lock_guard<std::mutex> lock(_lock);

	while( !_queueFree.empty() )
		_queueFree.pop();

	while( !_queueBroken.empty() )
		_queueBroken.pop();

	// [추가] 대기 중이던 요청들도 정리한다 — 풀 자체가 사라지는 상황이라
	// 콜백을 나중에 부를 방법이 없다(이 콜백들이 기대하는 커넥션/풀
	// 자체가 없어짐). 조용히 버린다 — 호출부들은 이미 서버 종료/재초기화
	// 같은 특수 상황에서만 이 경로를 타므로, 남은 요청에 대한 응답을
	// 더 이상 기다리지 않는다고 가정한다.
	while( !_queuePending.empty() )
		_queuePending.pop();

	// 재연결 스레드는 이미 위에서 정지·조인되었으므로(StopReconnectLoop()),
	// 이 시점에는 아무도 이 맵을 건드리지 않는다 — 락 없이 바로 비워도 안전하다.
	_reconnectBackoffMap.clear();

	for( auto& pClient : _vecAllClients )
	{
		if( pClient )
			pClient->Disconnect();
	}

	_vecAllClients.clear();
	_bInitialized = false;
}

//***************************************************************************
// @brief 풀에서 놀고 있는(Free) 커넥션을 팝함
// @return 클라이언트 객체 포인터 (없을 시 nullptr)
//***************************************************************************
CRedisClientRef CRedisConnectionPool::PopConnection()
{
	std::lock_guard<std::mutex> lock(_lock);

	if( _queueFree.empty() )
		return nullptr;

	auto pClient = _queueFree.front();
	_queueFree.pop();
	return pClient;
}

//***************************************************************************
// @brief 사용이 끝난 커넥션을 반납함
// @details 반납 시점에 연결이 살아있는지 확인해, 끊어져 있으면 Free 큐가
//          아니라 격리 큐로 보낸다. 죽은 커넥션이 Free 큐에 섞여 이후의
//          모든 대여 요청을 실패시키는 것을 막기 위함이며, 격리된 커넥션은
//          재연결 스레드가 주기적으로 복구를 시도한다.
// @details [수정 — 버그 수정] 커넥션이 살아있는 채로 반납되면, Free 큐에
//          넣기 전에 대기 큐(_queuePending)부터 확인한다 — 기다리던 요청이
//          있으면 그 커넥션을 곧바로 그 요청에 태워 보낸다(Free 큐에
//          넣었다가 바로 다시 꺼내는 왕복을 생략). RedisConnectionPool.h의
//          "버그 수정" 설명 참고.
// @param pClient 반납할 클라이언트 객체 포인터
//***************************************************************************
void CRedisConnectionPool::PushConnection(CRedisClientRef pClient)
{
	if( !pClient ) return;

	if( !pClient->IsConnected() )
	{
		std::lock_guard<std::mutex> lock(_lock);
		_queueBroken.push(pClient);
		return;
	}

	TPendingCommand pending;
	bool hasPending = false;
	{
		std::lock_guard<std::mutex> lock(_lock);
		if( !_queuePending.empty() )
		{
			pending = std::move(_queuePending.front());
			_queuePending.pop();
			hasPending = true;
		}
		else
		{
			_queueFree.push(pClient);
		}
	}

	// _lock을 놓은 뒤에 실제 전송(DispatchOnClient() -> CRedisClient::SendCommand())을
	// 한다 — 전송 자체는 블로킹 작업이 아니지만(IOCP 비동기), 락을 쥔 채로
	// 콜백 체인을 시작하는 습관을 들이지 않기 위한 방어적 조치다.
	if( hasPending )
		DispatchOnClient(pClient, pending.vecArgs, pending.fnCallback);
}

//***************************************************************************
// @brief 풀에서 커넥션을 대여하여 명령을 전송하고 완료 시 자동 반납함
// @param vecArgs Redis 명령어 및 인자
// @param fnCallback 결과 처리 콜백 함수
// @return 전송 요청 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisConnectionPool::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	if( !_bInitialized ) return false;

	auto pClient = PopConnection();
	if( !pClient )
	{
		// [수정 — 버그 수정] 예전엔 여기서 그냥 false를 돌려주고 끝이었다 —
		// 풀의 모든 커넥션이 사용 중일 때 새 요청이 아무 알림도 없이
		// 조용히 사라지는 버그였다. 대부분의 호출부(CChatServerMain의
		// 여러 Request*() 등)가 이 반환값을 확인하지 않아서, 콜백이
		// 영원히 안 불리는데도 에러 로그 하나 없이 "응답이 안 오는" 것
		// 처럼 보였다 — 짧은 시간에 Redis 요청이 몰리면(예: 여러 방에
		// 거의 동시에 입장) 재현되는 문제였다.
		//
		// 이제 대기 큐에 넣어두고 true를 반환한다 — PushConnection()이
		// 커넥션을 돌려받는 즉시 이 큐에서 꺼내 처리한다(RedisConnectionPool.h
		// 상단 설명 참고). 대기 큐 자체가 무한정 쌓이는 것만은 막아야
		// 하므로 상한을 둔다.
		std::lock_guard<std::mutex> lock(_lock);

		if( _queuePending.size() >= kMaxPendingCommands )
		{
			LOG_ERROR(_T("CRedisConnectionPool::SendCommand: 대기 큐 포화(상한 %d) — 요청 거부"), static_cast<int>(kMaxPendingCommands));
			return false;
		}

		_queuePending.push(TPendingCommand{ vecArgs, fnCallback });
		return true;
	}

	DispatchOnClient(pClient, vecArgs, fnCallback);
	return true;
}

//***************************************************************************
// @brief [추가] pClient 하나에 실제로 명령을 실어 보낸다.
// @details SendCommand()의 즉시 전송 경로와 PushConnection()의 "대기 중이던
//          요청을 반납받은 커넥션에 바로 태우는" 경로가 공유하는 공통
//          로직이다.
//***************************************************************************
void CRedisConnectionPool::DispatchOnClient(CRedisClientRef pClient, const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	std::weak_ptr<CRedisConnectionPool> weakSelf = shared_from_this();

	auto fnWrappedCallback = [weakSelf, pClient, fnCallback](const RedisValue& res) {
		if( fnCallback )
			fnCallback(res);

		if( auto pPool = weakSelf.lock() )
		{
			pPool->PushConnection(pClient);
		}
		};

	if( !pClient->SendCommand(vecArgs, fnWrappedCallback) )
	{
		// [수정] 전송 자체가 실패(끊긴 커넥션 등)했을 때도 콜백을 한 번은
		// 불러준다 — 예전엔 여기서 PushConnection()만 하고 끝이라, 호출부가
		// 응답을 영원히 못 받는 채로 남는 동일한 부류의 문제가 있었다.
		// 실패를 나타내는 빈 RedisValue로 호출한다(CRedisResultSet이 이를
		// "빈 응답"으로 안전하게 해석함 — IsEmpty()==true).
		PushConnection(pClient);
		if( fnCallback )
			fnCallback(RedisValue{});
	}
}

//***************************************************************************
// @brief 재연결 스레드를 시작함
//***************************************************************************
void CRedisConnectionPool::StartReconnectLoop()
{
	_stopping.store(false);
	_reconnectThread = std::thread([this]() { ReconnectLoop(); });
}

//***************************************************************************
// @brief 재연결 스레드를 정지시킴 (중복 호출에 안전)
//***************************************************************************
void CRedisConnectionPool::StopReconnectLoop()
{
	if( _stopping.exchange(true) )
		return; // 이미 정지 중이거나 정지됨

	_reconnectCv.notify_all();

	if( _reconnectThread.joinable() )
		_reconnectThread.join();
}

//***************************************************************************
// @brief 격리 큐(_queueBroken)를 주기적으로 훑어 재연결을 시도하는 스레드 루프
// @details condition_variable::wait_for로 주기만큼 대기하되, StopReconnectLoop()이
//          notify하면 대기를 즉시 끝내고 루프를 탈출한다. 재연결 시도(connect)는
//          풀 락(_lock) 밖에서 수행해 다른 스레드의 대여/반납을 막지 않는다.
//***************************************************************************
void CRedisConnectionPool::ReconnectLoop()
{
	while( !_stopping.load() )
	{
		{
			std::unique_lock<std::mutex> guard(_reconnectLock);
			const bool stoppedDuringWait = _reconnectCv.wait_for(
				guard,
				std::chrono::seconds(_nReconnectIntervalSec),
				[this] { return _stopping.load(); });

			if( stoppedDuringWait )
				break; // StopReconnectLoop() 요청으로 깨어남
		}

		// _reconnectLock은 오직 주기 대기(wait_for)를 위한 것이다. 실제
		// 재연결 시도(connect, 클라이언트당 최대 connectTimeoutMs 블로킹)는
		// 락을 놓은 채로 수행해, 서버 장애 등으로 대량 재연결이 진행 중일
		// 때도 StopReconnectLoop()이 다음 클라이언트 처리 전 시점에서 곧바로
		// 빠져나올 수 있게 한다(그렇지 않으면 종료 자체가 풀 크기만큼 지연됨).
		CVector<CRedisClientRef> vecBroken;
		{
			std::lock_guard<std::mutex> lock(_lock);
			while( !_queueBroken.empty() )
			{
				vecBroken.push_back(_queueBroken.front());
				_queueBroken.pop();
			}
		}

		for( auto& pClient : vecBroken )
		{
			if( _stopping.load() )
			{
				// 종료 요청이 들어온 경우 남은 클라이언트는 다시 격리 큐로
				// 되돌려 다음 기회에 재시도되게 하고 이번 순회는 즉시 접는다.
				std::lock_guard<std::mutex> lock(_lock);
				_queueBroken.push(pClient);
				continue;
			}

			auto& backoffState = _reconnectBackoffMap[pClient.get()];
			const auto now = std::chrono::steady_clock::now();

			if( backoffState.nFailCount > 0 && now < backoffState.nextRetryTime )
			{
				// 아직 백오프 대기 시간이 지나지 않음 — 이번 스윕에서는
				// 건너뛰고 격리 큐로 그대로 되돌린다.
				std::lock_guard<std::mutex> lock(_lock);
				_queueBroken.push(pClient);
				continue;
			}

			if( pClient->Connect(_strIP, _nPort, _nDbIndex) )
			{
				_reconnectBackoffMap.erase(pClient.get());

				std::lock_guard<std::mutex> lock(_lock);
				_queueFree.push(pClient);
			}
			else
			{
				// COdbcConnPool::ScheduleRetry()와 동일한 방식의 지수 백오프 +
				// 지터: 실패할수록 다음 시도까지 더 오래 기다려, 서버가 오래
				// 죽어있는 동안 스윕 주기마다 무의미하게 두드리는 것을 막는다.
				backoffState.nFailCount++;

				const int32 nShift = std::min(backoffState.nFailCount, RECONNECT_BACKOFF_MAX_SHIFT);
				int64 nDelayMs = std::min<int64>(RECONNECT_BACKOFF_BASE_MS << nShift, RECONNECT_BACKOFF_MAX_MS);

				thread_local std::mt19937 rng(std::random_device{}());
				std::uniform_int_distribution<int32> jitterDist(0, RECONNECT_BACKOFF_JITTER_MS);
				nDelayMs += jitterDist(rng);

				backoffState.nextRetryTime = now + std::chrono::milliseconds(nDelayMs);

				std::lock_guard<std::mutex> lock(_lock);
				_queueBroken.push(pClient);
			}
		}
	}
}