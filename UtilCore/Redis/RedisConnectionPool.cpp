
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
	// [수정 — 주석 정확도] 이 값들은 "이 시각이 지나기 전엔 재시도 자체를
	// 안 함"이라는 최소 대기 시간일 뿐이다 — 실제 재시도 시점은 재연결
	// 스윕 주기(_nReconnectIntervalSec, 기본 3초)에 맞춰서만 확인되므로,
	// 정확히 500ms/1초/2초 뒤에 재시도한다는 뜻이 아니라 "다음 스윕에서
	// 재시도할지 말지를 가르는 기준선"으로 보는 게 정확하다.
	constexpr int64 RECONNECT_BACKOFF_BASE_MS = 500;		// 최초 재시도 최소 대기 시간
	constexpr int64 RECONNECT_BACKOFF_MAX_MS = 30000;		// 재시도 최소 대기 시간 상한선
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
	// [추가 — 버그 수정] nPoolSize가 0 이하면 즉시 실패시킨다. 예전엔
	// 이 값을 검증 안 해서, 0을 넘기면 for 루프가 한 번도 안 돌고
	// _bInitialized=true로 "성공" 반환해버렸다 — 그러면 커넥션이 0개인
	// 채로 SendCommand()가 전부 대기 큐(_queuePending)에만 쌓이고
	// 영원히 아무도 처리해주지 않는(꺼내줄 커넥션 자체가 없으므로)
	// 상태가 됐다.
	if( nPoolSize <= 0 )
	{
		LOG_ERROR(_T("CRedisConnectionPool::Init: nPoolSize(%d)는 0보다 커야 합니다"), nPoolSize);
		return false;
	}

	// 이미 초기화된 풀에 Init()이 다시 호출되는 경우(재초기화)에 대비해
	// 먼저 완전히 정리한다. 처음 호출되는 경우엔 재연결 스레드가 시작된
	// 적이 없고 큐도 비어있어 사실상 아무 일도 하지 않는다. 이 선행 정리가
	// 없으면, 이미 돌고 있는 _reconnectThread 위에 StartReconnectLoop()이
	// 새 std::thread를 그대로 대입하려다 std::terminate()로 죽는다
	// (joinable한 std::thread에 대한 이동 대입은 표준상 terminate 유발).
	// [참고] Clear()가 _generation도 증가시키므로, 여기서 재초기화될 때마다
	// 새 세대로 넘어간다 — 옛 세대의 콜백이 이 새 풀 상태에 섞여 들어오지
	// 못하게 막는 가드(RedisConnectionPool.h "세대 간 콜백 유입" 설명 참고).
	Clear();

	// strIP/_nPort/_nDbIndex/_nReconnectIntervalSec 설정과 _vecAllClients/
	// _queueFree에 대한 반영만 _lock으로 짧게 감싸고, 실제 블로킹 작업인
	// Connect()(소켓 연결 + 필요 시 SELECT 응답 대기, 클라이언트당 최대
	// 수 초)는 락 밖에서 수행한다. ReconnectLoop()과 동일한 패턴 —
	// 그렇지 않으면 풀을 채우는 동안 다른 스레드의 SendCommand()/
	// PushConnection()이 전부 이 락에 막혀 대기하게 된다.
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
// @details [수정 — 버그 수정: 데드락] 외부 리뷰로 발견된 문제 — 예전엔
//          _lock을 쥔 채로 pClient->Disconnect()를 호출했다. Disconnect()는
//          대기 중이던 콜백을 그 자리에서 실행할 수 있는데, 그 콜백
//          (CRedisConnectionPool::DispatchOnClient()가 감싼 fnWrappedCallback)의
//          마지막 동작이 pPool->PushConnection()이고, PushConnection()은
//          다시 이 _lock을 잡으려 한다 — 같은 스레드가 이미 쥐고 있는
//          non-recursive std::mutex를 다시 잡으려는 것이므로 100% 데드락
//          이었다. 이제 _vecAllClients를 락 밖으로 옮겨온 뒤, 락을 놓은
//          상태에서 Disconnect()를 호출한다.
//
//          [수정 — 버그 수정: 세대] _generation을 증가시킨다 — 이 시점
//          이후 도착하는(옛 세대에서 대여해간 커넥션의) 콜백은
//          PushConnection()에서 세대 불일치로 걸러진다.
//***************************************************************************
void CRedisConnectionPool::Clear()
{
	// 재연결 스레드가 _lock을 필요로 하므로, _lock을 잡기 전에 먼저 스레드를
	// 정지/조인해야 한다(그렇지 않으면 여기서 _lock을 쥔 채로 join()이 스레드가
	// _lock을 얻을 때까지 서로 기다리는 데드락이 생긴다).
	StopReconnectLoop();

	CVector<CRedisClientRef> clientsToDisconnect;
	{
		std::lock_guard<std::mutex> lock(_lock);

		// [수정] generation 증가를 이 _lock 구간 안으로 옮겼다 — "세대
		// 변경 + 큐 비우기 + _bInitialized=false"가 하나의 풀 상태 전이로
		// 보이도록 명확히 하기 위함이다. PushConnection()이 이제 generation
		// 검사와 큐 변경을 같은 _lock으로 묶고 있으므로(위 버그 수정
		// 참고), 이 둘의 상대적 순서 자체가 정확성에 필수는 아니지만,
		// 같은 구간에 두는 편이 상태 전이를 읽기 쉽다.
		_generation.fetch_add(1, std::memory_order_acq_rel);

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

		// [수정 — 버그 수정: 데드락] _lock을 쥔 채로 Disconnect()를 호출하지
		// 않도록, 클라이언트 목록을 로컬 벡터로 옮겨온다.
		clientsToDisconnect = std::move(_vecAllClients);
		_vecAllClients.clear();

		_bInitialized = false;
	}

	// _lock을 완전히 놓은 뒤에 Disconnect()를 호출한다 — Disconnect()가
	// 트리거하는 pending 콜백(및 그 안의 PushConnection())이 다시 _lock을
	// 잡으려 할 수 있으므로, 락을 쥔 채로 부르면 데드락이다.
	for( auto& pClient : clientsToDisconnect )
	{
		if( pClient )
			pClient->Disconnect();
	}
}

//***************************************************************************
// @brief 사용이 끝난 커넥션을 반납함
// @details 반납 시점에 연결이 살아있는지 확인해, 끊어져 있으면 Free 큐가
//          아니라 격리 큐로 보낸다. 죽은 커넥션이 Free 큐에 섞여 이후의
//          모든 대여 요청을 실패시키는 것을 막기 위함이며, 격리된 커넥션은
//          재연결 스레드가 주기적으로 복구를 시도한다.
// @details 커넥션이 살아있는 채로 반납되면, Free 큐에 넣기 전에 대기 큐
//          (_queuePending)부터 확인한다 — 기다리던 요청이 있으면 그
//          커넥션을 곧바로 그 요청에 태워 보낸다(Free 큐에 넣었다가 바로
//          다시 꺼내는 왕복을 생략).
// @details [수정 — 버그 수정: 세대] generation이 지금 풀의 세대와 다르면
//          (Clear()/재Init()이 그 사이 있었다는 뜻) 이 커넥션은 이미 옛
//          세대의 Clear()가 Disconnect()까지 마쳤을 것이므로, 지금 풀의
//          큐/상태에 아무 것도 반영하지 않고 조용히 버린다 —
//          RedisConnectionPool.h "세대 간 콜백 유입" 설명 참고.
// @param pClient 반납할 클라이언트 객체 포인터
// @param generation 이 커넥션을 대여해갔던 시점의 풀 세대 번호
//***************************************************************************
void CRedisConnectionPool::PushConnection(CRedisClientRef pClient, uint64 generation)
{
	if( !pClient ) return;

	TPendingCommand pending;
	bool hasPending = false;
	{
		std::lock_guard<std::mutex> lock(_lock);

		// [수정 — 버그 수정: TOCTOU] 예전엔 generation 검사가 이 _lock
		// 밖에서(그리고 IsConnected() 검사도 별도 _lock 구간에서) 이뤄졌다.
		// 그 사이에 다른 스레드의 Clear()가 세대를 올리고 큐를 비워버리면,
		// 방금 통과한 세대 검사가 무의미해지고 이미 지나간 세대의 클라이언트가
		// 새로 초기화된 풀의 큐에 그대로 들어갈 수 있었다(외부 리뷰로 발견
		// — 세대 가드를 도입한 목적 자체를 무력화하는 경합이었다). 이제
		// generation 검사, IsConnected() 검사, 큐 변경을 전부 하나의 _lock
		// 구간으로 묶어서 Clear()의 "세대 증가 + 큐 비우기"와 원자적으로
		// 순서가 정해지게 한다.
		if( generation != _generation.load(std::memory_order_acquire) )
			return;

		if( !pClient->IsConnected() )
		{
			_queueBroken.push(pClient);
			return;
		}

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
// @return 요청이 풀에 접수(즉시 전송 또는 대기 큐 등록)됐는지 여부 (true:
//         접수됨, false: 접수 자체가 거부됨 — 풀 미초기화 또는 대기 큐
//         포화 — 이 경우 fnCallback은 호출되지 않는다).
// @details [추가 — 계약 명확화] true를 반환한 뒤의 콜백 호출 계약: 정상
//          운영 중에는 fnCallback이 정확히 한 번 호출된다(성공 응답,
//          또는 연결 끊김/전송 실패로 인한 에러 값). 단,
//          CRedisConnectionPool::Clear()가 실행되면 그 시점에 대기 큐
//          (_queuePending)에 남아있던 미전송 명령은 lifecycle 종료를
//          위해 조용히 폐기되며 fnCallback은 호출되지 않는다 — Clear()는
//          서버 종료/재초기화 같은 특수 상황에서만 호출된다는 전제
//          (클래스 문서 상단 "lifecycle 계약" 참고) 하의 설계 선택이다.
// @details [수정 — 버그 수정: TOCTOU] 외부 리뷰로 발견된 문제 — 예전엔
//          "Free 큐에서 꺼내기"(PopConnection())와 "없으면 대기 큐에 넣기"가
//          서로 다른 _lock 구간이었다. 그 사이에 다른 스레드의
//          PushConnection()이 끼어들어 커넥션을 Free 큐에 반납해버리면
//          (그 시점엔 대기 큐가 비어있어 PushConnection()이 Free로 보냄),
//          바로 뒤이어 이 함수가 대기 큐에 넣은 요청을 아무도 깨우지
//          못하고 방치되는 경합이 있었다. 이제 "Free에서 꺼내기 OR 대기
//          큐에 넣기"를 하나의 _lock 구간에서 원자적으로 결정한다.
//***************************************************************************
bool CRedisConnectionPool::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	if( !_bInitialized ) return false;

	CRedisClientRef pClient;
	{
		std::lock_guard<std::mutex> lock(_lock);

		if( !_queueFree.empty() )
		{
			pClient = _queueFree.front();
			_queueFree.pop();
		}
		else
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
			// 커넥션을 돌려받는 즉시 이 큐에서 꺼내 처리한다. 대기 큐
			// 자체가 무한정 쌓이는 것만은 막아야 하므로 상한을 둔다.
			if( _queuePending.size() >= kMaxPendingCommands )
			{
				LOG_ERROR(_T("CRedisConnectionPool::SendCommand: 대기 큐 포화(상한 %d) — 요청 거부"), static_cast<int>(kMaxPendingCommands));
				return false;
			}

			_queuePending.push(TPendingCommand{ vecArgs, fnCallback });
			return true;
		}
	}

	DispatchOnClient(pClient, vecArgs, fnCallback);
	return true;
}

//***************************************************************************
// @brief [추가] pClient 하나에 실제로 명령을 실어 보낸다.
// @details SendCommand()의 즉시 전송 경로와 PushConnection()의 "대기 중이던
//          요청을 반납받은 커넥션에 바로 태우는" 경로가 공유하는 공통
//          로직이다.
// @details [수정 — 버그 수정: 세대] 명령을 보내는 이 시점의 풀 세대 번호를
//          캡처해서 콜백에 같이 담아둔다 — 이 명령이 완료되어
//          PushConnection()이 호출될 때, 그 사이 Clear()/재Init()으로
//          세대가 바뀌었으면 옛 세대의 콜백으로 간주해 걸러낸다.
//***************************************************************************
void CRedisConnectionPool::DispatchOnClient(CRedisClientRef pClient, const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	std::weak_ptr<CRedisConnectionPool> weakSelf = shared_from_this();
	const uint64 generation = _generation.load(std::memory_order_acquire);

	auto fnWrappedCallback = [weakSelf, pClient, fnCallback, generation](const RedisValue& res) {
		// [추가 — 방어적 하드닝] 사용자 콜백(fnCallback)이 예외를 던지면
		// 원래는 이 람다 자체가 중간에 끊겨서 아래 PushConnection()이
		// 실행되지 않는다 — 그러면 이 커넥션이 풀로 영원히 반납되지 않고
		// 사라지는 결과가 된다. 프로젝트 전체에서 "Redis 콜백은 절대
		// 예외를 던지지 않는다"는 계약이 확립돼 있지 않다면, 이렇게
		// 잡아서 최소한 커넥션 반납만은 항상 보장하는 게 안전하다.
		try
		{
			if( fnCallback )
				fnCallback(res);
		}
		catch( const std::exception& e )
		{
			LOG_ERROR(_T("CRedisConnectionPool: 콜백에서 예외 발생 - %hs"), e.what());
		}
		catch( ... )
		{
			LOG_ERROR(_T("CRedisConnectionPool: 콜백에서 알 수 없는 예외 발생"));
		}

		if( auto pPool = weakSelf.lock() )
		{
			pPool->PushConnection(pClient, generation);
		}
		};

	// [수정 — 버그 수정] CRedisClient::SendCommand()는 이제 반환값과 무관하게
	// fnWrappedCallback을 정확히 한 번 호출하는 것을 스스로 보장한다
	// (RedisClient.h/.cpp 상단 "버그 수정" 설명 참고) — 성공하면 실제
	// 응답으로, 실패하면(즉시 끊겨있었거나 DoSend() 실패로 Disconnect()를
	// 거쳤거나) 에러 값으로. 그래서 여기서 반환값을 보고 또 콜백이나
	// PushConnection()을 호출하면 안 된다 — 예전엔 이렇게 했었는데, 그게
	// Disconnect()가 이미 처리한 콜백/PushConnection과 경합하면서 같은
	// 커넥션이 Free 큐에 중복으로 들어가 결국 두 개의 다른 명령에 동시에
	// 대여되는 버그로 이어질 수 있었다. 반환값은 이제 참고 정보일 뿐이라
	// 무시한다.
	pClient->SendCommand(vecArgs, fnWrappedCallback);
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
// @details [수정] _nReconnectIntervalSec을 매 반복 락 없이 읽는 대신, 이
//          스레드가 시작될 때 한 번만 읽어 로컬 상수로 고정한다 — Init()이
//          _lock으로 짧게 감싸서 쓰는 값을 여기선 락 없이 읽고 있었는데,
//          지금의 lifecycle 계약(Init()/Clear()는 정상 트래픽과 동시에
//          호출하지 않음, StartReconnectLoop()도 그 계약 안에서만 호출됨)
//          에서는 실질적으로 안전하지만, 아예 락 없는 접근 자체를 없애서
//          더 명확하게 만들었다.
//***************************************************************************
void CRedisConnectionPool::ReconnectLoop()
{
	const int32 reconnectIntervalSec = _nReconnectIntervalSec;

	while( !_stopping.load() )
	{
		{
			std::unique_lock<std::mutex> guard(_reconnectLock);
			const bool stoppedDuringWait = _reconnectCv.wait_for(
				guard,
				std::chrono::seconds(reconnectIntervalSec),
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

			// closesocket() 이후에도 그 소켓에 걸려있던 WSARecv/WSASend의
			// 완료 통지가 IOCP 큐를 아직 안 빠져나왔을 수 있다 — 이 상태에서
			// Connect()를 불러 같은 _recvEvent/_sendEvent(OVERLAPPED 메모리)를
			// 재사용해버리면, 나중에 도착하는 그 stale completion이 새
			// 연결의 것으로 오인되어 처리될 위험이 있다(외부 리뷰로 발견).
			// 이번 스윕에서는 재시도 실패로 취급하지 않고(백오프를 증가시키지
			// 않고) 그냥 건너뛴다 — 다음 스윕쯤엔 그 completion이 처리돼
			// 있을 가능성이 높다.
			if( !pClient->HasNoOutstandingIo() )
			{
				std::lock_guard<std::mutex> lock(_lock);
				_queueBroken.push(pClient);
				continue;
			}

			// [추가] Connect()는 블로킹(수 초까지) 호출이라, 시작 전에
			// generation을 미리 읽어둔다 — 정상적인 lifecycle 계약에서는
			// Connect() 도중 Clear()가 동시에 실행되지 않지만, 오래 걸리는
			// Connect() 도중 세대가 바뀌는 극단적인 경우까지 방어한다
			// (Connect() 완료 후 다시 읽으면 그 사이 세대가 바뀐 값을
			// 볼 수 있음).
			const uint64 reconnectGeneration = _generation.load(std::memory_order_acquire);

			if( pClient->Connect(_strIP, _nPort, _nDbIndex) )
			{
				_reconnectBackoffMap.erase(pClient.get());

				// [수정 — 버그 수정] 예전엔 여기서 그냥 _queueFree에 바로
				// push했다 — 그 사이 대기 큐(_queuePending)에 쌓여있던
				// 요청이 있어도 아무도 그걸 꺼내 처리해주지 않는 버그였다
				// (대기 큐 소비 로직은 PushConnection()에만 있었으므로).
				// PushConnection()을 거치도록 통일해서, 재연결 성공 즉시
				// 대기 중이던 요청이 있으면 바로 처리되게 한다.
				PushConnection(pClient, reconnectGeneration);
			}
			else
			{
				// COdbcConnPool::ScheduleRetry()와 동일한 방식의 지수 백오프 +
				// 지터: 실패할수록 다음 시도까지 더 오래 기다려, 서버가 오래
				// 죽어있는 동안 스윕 주기마다 무의미하게 두드리는 것을 막는다.
				backoffState.nFailCount++;

				// [수정 — off-by-one] 예전엔 nShift에 nFailCount를 그대로
				// 써서 1차 실패부터 500<<1=1000ms가 됐다 — RECONNECT_BACKOFF_BASE_MS
				// 자체가 이미 "최초 재시도 대기 시간"이라는 이름인데, 실제로는
				// 그 두 배부터 시작하는 불일치였다(외부 리뷰로 발견). nFailCount-1을
				// 써야 1차 실패=500ms, 2차=1000ms, 3차=2000ms... 로 이름과
				// 일치한다.
				const int32 nShift = std::min(backoffState.nFailCount - 1, RECONNECT_BACKOFF_MAX_SHIFT);
				int64 nDelayMs = std::min<int64>(RECONNECT_BACKOFF_BASE_MS << nShift, RECONNECT_BACKOFF_MAX_MS);

				thread_local std::mt19937 rng(std::random_device{}());
				std::uniform_int_distribution<int32> jitterDist(0, RECONNECT_BACKOFF_JITTER_MS);
				nDelayMs += jitterDist(rng);

				// [추가] 지터를 더한 뒤 다시 한번 상한으로 클램프한다 —
				// 그렇지 않으면 지터(최대 250ms)만큼 RECONNECT_BACKOFF_MAX_MS를
				// 살짝 넘길 수 있었다(사소하지만, 상한이라는 이름과 어긋남).
				nDelayMs = std::min<int64>(nDelayMs, RECONNECT_BACKOFF_MAX_MS);

				backoffState.nextRetryTime = now + std::chrono::milliseconds(nDelayMs);

				std::lock_guard<std::mutex> lock(_lock);
				_queueBroken.push(pClient);
			}
		}
	}
}