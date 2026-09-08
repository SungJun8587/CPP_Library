
//***************************************************************************
// OdbcConnPool.cpp : implementation of the COdbcConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcConnPool.h"

constexpr int32 WAIT_TIMEOUT_MS = 100;				// 슬롯 교체 시 기존 사용 중인 스레드의 이탈 대기 상한 시간 (ms)
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000;		// 격리 큐 정체 경고 로그 최소 출력 주기 (5분)
constexpr int64 FORCE_CLEANUP_TIMEOUT_MS = 600000;	// 격리 큐 최대 체류 허용 시간 (10분)

//***************************************************************************
// @brief COdbcConnPool 클래스 생성자입니다.
// @param nMaxPoolSize 관리할 최대 커넥션 슬롯 개수
//***************************************************************************
COdbcConnPool::COdbcConnPool(int32 nMaxPoolSize)
	: _dbClass(EDBClass::NONE)
	, _nMaxPoolSize(nMaxPoolSize)
	, _nBackoffBaseMs(500)
	, _nBackoffMaxMs(30000)
	, _nBackoffMaxShift(6)
	, _nBackoffJitterMs(250)
	, _bStopHealthCheck(false)
	, _nHealthCheckIntervalMs(500)
	, _bStopReconnectWorkers(false)
	, _nCurrentWorkerCount(0)
	, _nDesiredWorkerCount(0)
{
	_pOdbcConns = std::make_unique<CachePaddedAtomic<CBaseODBC*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<PLock[]>(_nMaxPoolSize);

	_pReconnecting = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);
	_pInFreeSlotQueue = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pOdbcConns[i].value.store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].value.store(0, std::memory_order_relaxed);
		_pReconnecting[i].value.store(false, std::memory_order_relaxed);
		_pRetryFailCount[i].value.store(0, std::memory_order_relaxed);
		_pInFreeSlotQueue[i].value.store(false, std::memory_order_relaxed);
	}
	memset(&_tszDSN[0], 0, sizeof(_tszDSN));
}

//***************************************************************************
// @brief COdbcConnPool 클래스 소멸자입니다.
//***************************************************************************
COdbcConnPool::~COdbcConnPool(void)
{
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();
}

//***************************************************************************
// @brief 재연결 설정 값들의 유효성을 검사합니다.
// @param cfg 검사할 TReconnectConfig 설정 구조체 레퍼런스
// @return 유효한 설정이면 true, 아니면 false
//***************************************************************************
bool COdbcConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

//***************************************************************************
// @brief 커넥션 풀을 초기화하고 지정된 DSN으로 초기 커넥션을 미리 생성 및 연결합니다.
// @param dbClass 데이터베이스 종류 식별자
// @param ptszDSN 연결에 사용할 DSN 문자열 포인터
// @param reconnectConfig 초기 재연결 및 백오프 정책 설정 구조체
// @return 초기화 및 전체 연결 성공 시 true, 실패 시 false
//***************************************************************************
bool COdbcConnPool::Init(const EDBClass dbClass, const TCHAR* ptszDSN,
	const TReconnectConfig& reconnectConfig)
{
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();

	_dbClass = dbClass;
	_tcsncpy_s(_tszDSN, _countof(_tszDSN), ptszDSN, _TRUNCATE);

	TReconnectConfig cfgToApply = reconnectConfig;
	if( !ValidateReconnectConfig(reconnectConfig) )
	{
		LOG_ERROR(_T("Init: invalid TReconnectConfig, falling back to default"));
		cfgToApply = TReconnectConfig{};
	}

	_nBackoffBaseMs.store(cfgToApply.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(cfgToApply.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(cfgToApply.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(cfgToApply.nBackoffJitterMs, std::memory_order_relaxed);

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseODBC* pConn = xnew<CBaseODBC>(dbClass, ptszDSN);
		if( pConn == nullptr )
		{
			LOG_ERROR(_T("Init: xnew<CBaseODBC> failed (index=%d)"), i);
			Clear();
			return false;
		}

		if( !pConn->Connect() )
		{
			xdelete(pConn);
			Clear();
			return false;
		}

		_pOdbcConns[i].value.store(pConn, std::memory_order_release);

		// [수정 — 프리 큐] 초기 연결에 성공한 슬롯을 즉시 프리 큐에 넣어둔다.
		// PopFreeSlotIndex()는 이제 이 큐만 보고 O(1)에 가깝게 슬롯을 찾는다.
		EnqueueFreeSlot(i);
	}

	StartDelayedTaskThread();
	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

//***************************************************************************
// @brief 새로운 데이터베이스 연결 객체를 생성하고 실제 연결을 시도합니다.
// @param nType 대상 슬롯 인덱스 번호 (로깅 및 추적용)
// @return 연결에 성공한 CBaseODBC 포인터, 실패 시 nullptr
//***************************************************************************
CBaseODBC* COdbcConnPool::TryReconnect(int32 nType)
{
	CBaseODBC* pNewConn = xnew<CBaseODBC>(_dbClass, _tszDSN);
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect: xnew<CBaseODBC> failed (index=%d)"), nType);
		return nullptr;
	}

	if( !pNewConn->Connect() )
	{
		xdelete(pNewConn);
		return nullptr;
	}

	return pNewConn;
}

//***************************************************************************
// @brief 지정한 슬롯 번호의 커넥션 참조를 획득합니다.
// @param nType 가져올 커넥션 슬롯 인덱스 번호
// @return 유효한 CBaseODBC 포인터, 실패 시 nullptr
//***************************************************************************
CBaseODBC* COdbcConnPool::GetOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetOdbcConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	_pRefCount[nType].value.fetch_add(1, std::memory_order_relaxed);
	CBaseODBC* pOdbcConn = _pOdbcConns[nType].value.load(std::memory_order_acquire);

	if( pOdbcConn == nullptr || !pOdbcConn->IsConnected() )
	{
		_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetOdbcConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	return pOdbcConn;
}

//***************************************************************************
// @brief 참조 카운트 변경 없이 슬롯의 커넥션을 조회합니다.
// @param nType 조회할 슬롯 인덱스 번호
// @return CBaseODBC 포인터, 유효하지 않은 경우 nullptr
//***************************************************************************
CBaseODBC* COdbcConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;
	return _pOdbcConns[nType].value.load(std::memory_order_acquire);
}

//***************************************************************************
// @brief 사용이 끝난 커넥션 슬롯의 참조 카운트를 감소시킵니다.
// @details [수정 — use-after-free 방지] 연결 상태 확인(IsConnected())을
// 반드시 참조 카운트를 감소시키기 "전에" 수행한다. 아직 내 참조가
// 남아있는 동안은(fetch_sub 이전) ApplyReconnectedConn()의 진입 게이트가
// refcount>0을 보고 이 슬롯을 건드리지 않으므로 커넥션 객체가 안전하게
// 보장된다. 반대로 감소를 먼저 하고 나서 커넥션을 들여다보면, 그
// 감소로 참조가 정확히 0이 되는 순간 다른 스레드(ApplyReconnectedConn의
// 대기 루프)가 곧바로 xdelete()할 수 있어 그 틈에 접근하면
// use-after-free가 된다.
// @param nType 반환할 커넥션 슬롯 인덱스 번호
//***************************************************************************
void COdbcConnPool::ReleaseOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;

	CBaseODBC* pConn = _pOdbcConns[nType].value.load(std::memory_order_acquire);
	bool bStillConnected = (pConn != nullptr && pConn->IsConnected());

	int32 prev = _pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
	if( prev != 1 ) return; // 아직 다른 참조가 남아 있음 — 프리 큐에 넣지 않는다

	if( bStillConnected )
		EnqueueFreeSlot(nType);
}

//***************************************************************************
// @brief 슬롯을 프리 큐에 등록합니다(중복 삽입 방지 포함).
// @param nType 등록할 슬롯 인덱스
//***************************************************************************
void COdbcConnPool::EnqueueFreeSlot(int32 nType)
{
	bool expected = false;
	if( !_pInFreeSlotQueue[nType].value.compare_exchange_strong(expected, true, std::memory_order_acq_rel) )
		return; // 이미 큐에 들어가 있음 — 중복 삽입 방지

	std::lock_guard<std::mutex> lock(_freeSlotMutex);
	_freeSlotQueue.push(nType);
}

//***************************************************************************
// @brief 빈 슬롯을 탐색하고 즉시 선점합니다.
// @details [수정 — O(n) 스캔 제거] 예전에는 라운드로빈으로 풀 전체를
// 스캔했다(최악의 경우 O(n)). 이제는 "비어 있고 연결된 것으로 확인된"
// 슬롯 인덱스만 담긴 _freeSlotQueue에서 후보를 하나씩 꺼내 검증한다.
// 큐에는 원칙적으로 참조 카운트 0인 슬롯만 들어오므로 CAS는 거의 항상
// 성공하지만, 큐에 있는 동안 연결이 끊긴 경우(재연결 워커가 아직 갱신
// 전)나 GetOdbcConn(명시적 인덱스)로 직접 선점된 경우처럼 CAS가 실패할
// 수 있는 경우에는 그 후보를 버리고 다음 후보로 넘어간다 — 재귀 대신
// 루프로 처리해 스택 오버플로 위험이 없다. 큐가 비어 있으면 즉시 -1을
// 반환한다(예전과 동일한 실패 시맨틱).
// @return 선점 성공한 슬롯 인덱스 번호, 실패 시 -1
//***************************************************************************
int32 COdbcConnPool::PopFreeSlotIndex(void) {
	for( ;; ) {
		int32 candidate = -1;
		{
			std::lock_guard<std::mutex> lock(_freeSlotMutex);
			if( _freeSlotQueue.empty() )
				return -1;
			candidate = _freeSlotQueue.front();
			_freeSlotQueue.pop();
		}

		// [수정] 큐에서 빠져나온 즉시 "큐에 있음" 플래그를 내린다 — 이
		// 후보를 최종적으로 못 쓰게 되더라도(아래에서 버려지더라도),
		// 이후 EnqueueFreeSlot()가 이 슬롯을 다시 정상적으로 큐에
		// 넣을 수 있어야 하기 때문이다.
		_pInFreeSlotQueue[candidate].value.store(false, std::memory_order_release);

		int32 expected = 0;
		if( !_pRefCount[candidate].value.compare_exchange_strong(expected, 1, std::memory_order_acq_rel) ) {
			// GetOdbcConn(명시적 인덱스)이 큐를 거치지 않고 먼저 선점했을
			// 수 있다 — 정상적인 레이스이므로 이 후보는 버리고 다음
			// 후보를 시도한다.
			continue;
		}

		CBaseODBC* pConn = _pOdbcConns[candidate].value.load(std::memory_order_acquire);
		if( pConn && pConn->IsConnected() ) return candidate;

		// 큐에 있는 동안 연결이 끊겼다 — 선점을 되돌리고 다음 후보를 시도한다.
		// 이 슬롯은 헬스체크가 알아서 감지해 재연결 큐로 옮긴다.
		_pRefCount[candidate].value.store(0, std::memory_order_release);
	}
}

//***************************************************************************
// @brief 재연결 워커가 새로 확보한 커넥션을 슬롯에 교체(Swap) 적용합니다.
// @param nType 교체 대상 슬롯 인덱스 번호
// @param pNewConn 새로 연결된 CBaseODBC 객체 포인터
//***************************************************************************
void COdbcConnPool::ApplyReconnectedConn(int32 nType, CBaseODBC* pNewConn)
{
	if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	CBaseODBC* pOldConn = nullptr;
	{
		PLockGuard guard(_slotLocks[nType]);
		pOldConn = _pOdbcConns[nType].value.load(std::memory_order_acquire);
		_pOdbcConns[nType].value.store(pNewConn, std::memory_order_release);
	}

	if( pOldConn == nullptr )
	{
		// [수정 — 프리 큐] 최초 슬롯 생성 이후 첫 재연결처럼 이전 커넥션이
		// 없던 경우도, 스왑이 끝난 지금 이 슬롯은 참조 0(함수 진입 시
		// 이미 확인됨) & 연결됨 상태이므로 프리 큐에 등록해야 한다.
		EnqueueFreeSlot(nType);
		return;
	}

	auto startTime = std::chrono::steady_clock::now();
	bool bTimeout = false;
	int spinCount = 0;

	while( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - startTime).count();

		if( elapsed >= WAIT_TIMEOUT_MS )
		{
			bTimeout = true;
			break;
		}

		// CPU 공회전 방지를 위해 스핀 횟수가 넘어가면 짧게 슬립 수행
		if( ++spinCount > 1000 )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			std::this_thread::yield();
		}
	}

	if( bTimeout )
	{
		LOG_ERROR(_T("ReconnectWorker: Slot(%d) refcount high during swap. Moving to quarantine."), nType);

		// [수정] enqueueTime/lastLogTime 모두 격리 시작 시각(now)으로
		// 초기화한다 — enqueueTime은 이후 절대 갱신되지 않고 강제 정리
		// 타임아웃 판단에만 쓰인다(OdbcConnPool.h TQuarantineItem 참고).
		// [주의 — 프리 큐] 이 시점엔 refcount가 아직 0으로 안 떨어졌으므로
		// (그래서 타임아웃난 것) 이 슬롯을 프리 큐에 넣지 않는다 — 나중에
		// 그 참조가 실제로 ReleaseOdbcConn()을 호출해 0이 되는 순간, 그
		// 함수가 알아서 프리 큐에 등록해 준다.
		auto now = std::chrono::steady_clock::now();
		PLockGuard qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType].value, now, now });
	}
	else
	{
		xdelete(pOldConn);

		// [수정 — 프리 큐] 대기 루프가 정상 종료됐다는 것은 이 시점에
		// refcount가 확실히 0이라는 뜻 — 이제 이 슬롯은 새 커넥션으로
		// 완전히 사용 가능한 상태이므로 프리 큐에 등록한다.
		EnqueueFreeSlot(nType);
	}
}

//***************************************************************************
// @brief 재연결 실패 시 지수 백오프 및 지터를 계산하여 재시도를 예약합니다.
// @param nType 재시도가 실패한 슬롯 인덱스 번호
//***************************************************************************
void COdbcConnPool::ScheduleRetry(int32 nType)
{
	int32 nFailCount = _pRetryFailCount[nType].value.fetch_add(1, std::memory_order_acq_rel) + 1;

	int32 nMaxShift = _nBackoffMaxShift.load(std::memory_order_relaxed);
	int32 nShift = std::min(nFailCount, nMaxShift);

	int64 nBaseMs = _nBackoffBaseMs.load(std::memory_order_relaxed);
	int64 nMaxMs = _nBackoffMaxMs.load(std::memory_order_relaxed);

	int64 nDelayMs = nBaseMs << nShift;
	nDelayMs = std::min<int64>(nDelayMs, nMaxMs);

	thread_local std::mt19937 rng(std::random_device{}());
	int32 nJitterMax = _nBackoffJitterMs.load(std::memory_order_relaxed);
	std::uniform_int_distribution<int32> jitterDist(0, std::max(nJitterMax, 0));
	nDelayMs += jitterDist(rng);

	LOG_DEBUG(_T("ScheduleRetry: slot(%d) failCount(%d), retrying in %lldms"),
		nType, nFailCount, static_cast<long long>(nDelayMs));

	_delayedTaskQueue.Reserve(std::chrono::milliseconds(nDelayMs), [this, nType]() {
		CBaseODBC* pCur = _pOdbcConns[nType].value.load(std::memory_order_acquire);
		if( _pRefCount[nType].value.load(std::memory_order_acquire) == 0 &&
			(pCur == nullptr || !pCur->IsConnected()) )
		{
			EnqueueReconnect(nType);
		}
		else
		{
			_pReconnecting[nType].value.store(false, std::memory_order_release);
		}
		});
}

//***************************************************************************
// @brief 재연결 실패 후속 처리를 수행합니다.
// @param nType 실패한 슬롯 인덱스 번호
//***************************************************************************
void COdbcConnPool::OnReconnectFailed(int32 nType)
{
	ScheduleRetry(nType);
}

//***************************************************************************
// @brief 재연결 성공 시 백오프 실패 카운트를 초기화합니다.
// @param nType 성공한 슬롯 인덱스 번호
//***************************************************************************
void COdbcConnPool::OnReconnectSucceeded(int32 nType)
{
	_pRetryFailCount[nType].value.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 격리 큐 정리 및 끊긴 슬롯을 주기적으로 스캔하여 재연결 워커에 디스패치합니다.
//***************************************************************************
void COdbcConnPool::HealthCheckLoop(void)
{
	CVector<CBaseODBC*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		vDeletes.clear();

		{
			PLockGuard qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size();

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				auto now = std::chrono::steady_clock::now();

				// [수정 — 강제 정리 타임아웃 버그] 전체 격리 경과 시간은
				// enqueueTime(최초 격리 시각, 절대 갱신되지 않음) 기준으로
				// 계산한다. 예전에는 lastLogTime을 여기와 아래 경고
				// 쿨다운 판단 양쪽에 같이 썼는데, 경고를 남길 때마다
				// lastLogTime이 now로 리셋되면서 이 경과 시간도 함께
				// 리셋되어 FORCE_CLEANUP_TIMEOUT_MS(10분) 조건에 절대
				// 도달하지 못하는 버그가 있었다.
				auto elapsedTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - item.enqueueTime).count();

				// 참조 카운트가 0이 되었거나, 허용 체류 시간을 초과한 경우 강제 회수
				if( item.pRefCount->load(std::memory_order_acquire) == 0 || elapsedTotal >= FORCE_CLEANUP_TIMEOUT_MS )
				{
					if( item.pConn != nullptr )
					{
						if( elapsedTotal >= FORCE_CLEANUP_TIMEOUT_MS )
						{
							LOG_ERROR(_T("Quarantine Force Cleanup: Connection exceeded max quarantine time. Forcibly deleting to prevent memory leak."));
						}
						vDeletes.push_back(item.pConn);
					}
				}
				else
				{
					auto elapsedFromLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - item.lastLogTime).count();

					if( elapsedFromLastLog >= LOG_ALERT_INTERVAL_MS )
					{
						LOG_ERROR(_T("Quarantine Persistent Warning: Connection is still stuck in quarantine! Potential leak in application logic."));
						// [수정] lastLogTime만 갱신한다 — enqueueTime은 절대
						// 건드리지 않아야 강제 정리 타임아웃이 정상 동작한다.
						item.lastLogTime = now;
					}

					_quarantineQueue.push(item);
				}
			}
		}

		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine: Safely deleted stalled connection outside the lock."));
		}

		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			if( _pRefCount[i].value.load(std::memory_order_acquire) > 0 ) continue;

			CBaseODBC* pCur = _pOdbcConns[i].value.load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->IsConnected() ) continue;

			bool bExpected = false;
			if( !_pReconnecting[i].value.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel) )
				continue;

			if( _pRetryFailCount[i].value.load(std::memory_order_relaxed) == 0 )
			{
				EnqueueReconnect(i);
			}
			else
			{
				_pReconnecting[i].value.store(false, std::memory_order_release);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_nHealthCheckIntervalMs));
	}
}

//***************************************************************************
// @brief 헬스체크 스레드를 생성하고 구동을 시작합니다.
//***************************************************************************
void COdbcConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

//***************************************************************************
// @brief 헬스체크 스레드를 안전하게 종료하고 대기합니다.
//***************************************************************************
void COdbcConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads();
}

//***************************************************************************
// @brief CDelayedTaskQueue의 만료 작업 처리 루프 함수입니다.
//***************************************************************************
void COdbcConnPool::DelayedTaskLoop(void)
{
	_delayedTaskQueue.ProcessExpiredTasks();
}

//***************************************************************************
// @brief 지연 타이머 큐 전용 스레드를 생성하고 구동을 시작합니다.
//***************************************************************************
void COdbcConnPool::StartDelayedTaskThread(void)
{
	_delayedTaskThreadMgr.CreateThread([this]() { DelayedTaskLoop(); });
}

//***************************************************************************
// @brief 지연 타이머 큐 스레드를 안전하게 중지하고 종료를 대기합니다.
//***************************************************************************
void COdbcConnPool::StopDelayedTaskThread(void)
{
	_delayedTaskQueue.Stop();
	_delayedTaskThreadMgr.JoinThreads();
}

//***************************************************************************
// @brief 재연결이 필요한 슬롯 인덱스를 대기열에 넣고 워커 스레드를 깨웁니다.
// @param nType 재연결 대상 슬롯 인덱스 번호
//***************************************************************************
void COdbcConnPool::EnqueueReconnect(int32 nType)
{
	{
		std::lock_guard<std::mutex> lock(_reconnectQueueMutex);
		_reconnectPendingSlots.push(nType);
	}
	_reconnectQueueCv.notify_one();
}

//***************************************************************************
// @brief 현재 워커 스레드가 목표치를 초과하는지 검사하고 초과 시 스스로 감축합니다.
// @return 초과 워커로 판정되어 자가 종료하는 경우 true, 아니면 false
//***************************************************************************
bool COdbcConnPool::TryExitIfExcess(void)
{
	int32 nCur = _nCurrentWorkerCount.load(std::memory_order_acquire);
	while( nCur > _nDesiredWorkerCount.load(std::memory_order_acquire) )
	{
		if( _nCurrentWorkerCount.compare_exchange_weak(nCur, nCur - 1, std::memory_order_acq_rel) )
		{
			LOG_DEBUG(_T("ReconnectWorker: self-exiting as excess worker (target reached)"));
			return true;
		}
	}
	return false;
}

//***************************************************************************
// @brief 재연결 워커 스레드의 메인 루프 함수입니다.
//***************************************************************************
void COdbcConnPool::ReconnectWorkerLoop(void)
{
	while( true )
	{
		if( TryExitIfExcess() ) return;

		int32 nType;
		{
			std::unique_lock<std::mutex> lock(_reconnectQueueMutex);
			_reconnectQueueCv.wait(lock, [this]() {
				return _bStopReconnectWorkers.load(std::memory_order_relaxed)
					|| !_reconnectPendingSlots.empty()
					|| _nCurrentWorkerCount.load(std::memory_order_acquire)
			> _nDesiredWorkerCount.load(std::memory_order_acquire);
				});

			if( _bStopReconnectWorkers.load(std::memory_order_relaxed) && _reconnectPendingSlots.empty() )
				return;

			if( _reconnectPendingSlots.empty() ) continue;

			nType = _reconnectPendingSlots.front();
			_reconnectPendingSlots.pop();
		}

		if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
		{
			_pReconnecting[nType].value.store(false, std::memory_order_release);
			continue;
		}

		CBaseODBC* pNewConn = TryReconnect(nType);
		if( pNewConn == nullptr )
		{
			OnReconnectFailed(nType);
			_pReconnecting[nType].value.store(false, std::memory_order_release);
			continue;
		}

		ApplyReconnectedConn(nType, pNewConn);
		OnReconnectSucceeded(nType);

		_pReconnecting[nType].value.store(false, std::memory_order_release);
	}
}

//***************************************************************************
// @brief 지정한 수만큼의 재연결 워커 스레드들을 생성하고 기동합니다.
// @param nWorkerCount 생성할 워커 스레드 개수
//***************************************************************************
void COdbcConnPool::StartReconnectWorkers(int32 nWorkerCount)
{
	_bStopReconnectWorkers.store(false, std::memory_order_relaxed);

	int32 nClamped = std::max(nWorkerCount, 1);
	if( nClamped != nWorkerCount )
	{
		LOG_ERROR(_T("StartReconnectWorkers: invalid nWorkerCount(%d), clamped to %d"),
			nWorkerCount, nClamped);
	}

	_nDesiredWorkerCount.store(nClamped, std::memory_order_relaxed);
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);

	for( int32 i = 0; i < nClamped; ++i )
	{
		_reconnectWorkerMgr.CreateThread([this]() { ReconnectWorkerLoop(); });
	}

	_nCurrentWorkerCount.store(nClamped, std::memory_order_release);
}

//***************************************************************************
// @brief 모든 재연결 워커 스레드를 종료시키고 완료를 대기합니다.
//***************************************************************************
void COdbcConnPool::StopReconnectWorkers(void)
{
	_bStopReconnectWorkers.store(true, std::memory_order_relaxed);
	_reconnectQueueCv.notify_all();
	_reconnectWorkerMgr.JoinThreads();
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);
	_nDesiredWorkerCount.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 런타임에 목표 재연결 워커 스레드 개수를 동적으로 조정합니다.
// @param nNewCount 변경할 목표 워커 스레드 수
//***************************************************************************
void COdbcConnPool::SetWorkerCount(int32 nNewCount)
{
	nNewCount = std::max(nNewCount, 1);
	_nDesiredWorkerCount.store(nNewCount, std::memory_order_release);

	int32 nExpected = _nCurrentWorkerCount.load(std::memory_order_acquire);
	if( nExpected < nNewCount )
	{
		int32 nBefore = nExpected;
		while( nExpected < nNewCount &&
			!_nCurrentWorkerCount.compare_exchange_weak(nExpected, nNewCount, std::memory_order_acq_rel) )
		{
		}

		if( nExpected < nNewCount )
		{
			int32 nToAdd = nNewCount - nExpected;
			for( int32 i = 0; i < nToAdd; ++i )
			{
				_reconnectWorkerMgr.CreateThread([this]() { ReconnectWorkerLoop(); });
			}
			LOG_DEBUG(_T("SetWorkerCount: worker count increased %d -> %d"), nBefore, nNewCount);
		}
	}
	else
	{
		_reconnectQueueCv.notify_all();
		LOG_DEBUG(_T("SetWorkerCount: target worker count decreased to %d, workers will self-exit"), nNewCount);
	}
}

//***************************************************************************
// @brief 재연결 백오프 정책 및 워커 스레드 설정을 런타임에 일괄 변경합니다.
// @param reconnectConfig 적용할 새로운 TReconnectConfig 설정 구조체
// @return 설정 변경 성공 시 true, 유효성 검사 실패 시 false
//***************************************************************************
bool COdbcConnPool::SetReconnectConfig(const TReconnectConfig& reconnectConfig)
{
	if( !ValidateReconnectConfig(reconnectConfig) )
	{
		LOG_ERROR(_T("SetReconnectConfig: invalid TReconnectConfig rejected, no change applied"));
		return false;
	}

	_nBackoffBaseMs.store(reconnectConfig.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(reconnectConfig.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(reconnectConfig.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(reconnectConfig.nBackoffJitterMs, std::memory_order_relaxed);

	SetWorkerCount(reconnectConfig.nWorkerCount);

	return true;
}

//***************************************************************************
// @brief 현재 적용된 재연결 정책 설정을 조회합니다.
// @return 현재 설정값이 담긴 TReconnectConfig 구조체
//***************************************************************************
COdbcConnPool::TReconnectConfig COdbcConnPool::GetReconnectConfig(void) const
{
	TReconnectConfig cfg;
	cfg.nWorkerCount = _nDesiredWorkerCount.load(std::memory_order_acquire);
	cfg.nBackoffBaseMs = _nBackoffBaseMs.load(std::memory_order_relaxed);
	cfg.nBackoffMaxMs = _nBackoffMaxMs.load(std::memory_order_relaxed);
	cfg.nBackoffMaxShift = _nBackoffMaxShift.load(std::memory_order_relaxed);
	cfg.nBackoffJitterMs = _nBackoffJitterMs.load(std::memory_order_relaxed);
	return cfg;
}

//***************************************************************************
// @brief 풀이 소멸되거나 재초기화될 때 내부 모든 커넥션 자원과 격리 큐를 안전하게 해제합니다.
//***************************************************************************
void COdbcConnPool::Clear(void)
{
	// [수정 — 프리 큐] 아래에서 각 슬롯의 커넥션을 실제로 삭제/격리하기
	// 전에 프리 큐부터 비워둔다 — 그러지 않으면 재Init() 이후 새로 채워질
	// 프리 큐가 이번 Clear() 이전 슬롯 상태를 가리키는 오래된 인덱스와
	// 뒤섞일 수 있다(PopFreeSlotIndex()의 CAS/IsConnected() 방어 덕에
	// 즉시 크래시로 이어지진 않지만, 굳이 남겨둘 이유도 없다). 큐를 비운
	// 뒤에는 _pInFreeSlotQueue 플래그도 전부 false로 되돌려야 한다 —
	// 그러지 않으면 "큐에 있다"는 플래그만 true로 남아, 재Init() 이후
	// EnqueueFreeSlot()이 그 슬롯을 다시는 큐에 넣지 못하게 된다.
	{
		std::lock_guard<std::mutex> lock(_freeSlotMutex);
		while( !_freeSlotQueue.empty() )
			_freeSlotQueue.pop();
	}
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pInFreeSlotQueue[i].value.store(false, std::memory_order_relaxed);
	}

	auto now = std::chrono::steady_clock::now();
	CVector<CBaseODBC*> vShutdownDeletes;

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseODBC* pConn = _pOdbcConns[i].value.load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;
		int spinCount = 0;

		while( _pRefCount[i].value.load(std::memory_order_acquire) > 0 )
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - startTime).count();

			if( elapsed >= WAIT_TIMEOUT_MS )
			{
				bTimeout = true;
				break;
			}

			if( ++spinCount > 1000 )
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				std::this_thread::yield();
			}
		}

		if( bTimeout )
		{
			LOG_ERROR(_T("Clear: Slot(%d) refcount is zombie (%d). Moving to quarantine."), i, _pRefCount[i].value.load());

			// [수정] ApplyReconnectedConn()과 동일하게 enqueueTime/
			// lastLogTime을 모두 now로 초기화한다.
			PLockGuard qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i].value, now, now });
		}
		else
		{
			xdelete(pConn);
		}

		_pOdbcConns[i].value.store(nullptr, std::memory_order_relaxed);
	}

	{
		PLockGuard qGuard(_globalQuarantineLock);
		while( !_quarantineQueue.empty() )
		{
			TQuarantineItem item = _quarantineQueue.front();
			_quarantineQueue.pop();

			if( item.pRefCount->load(std::memory_order_acquire) == 0 )
			{
				if( item.pConn != nullptr )
				{
					vShutdownDeletes.push_back(item.pConn);
				}
			}
			else
			{
				LOG_ERROR(_T("Clear: Abandoning leaked connection to prevent Use-After-Free crash."));
			}
		}
	}

	for( int32 i = 0; i < vShutdownDeletes.size(); ++i )
	{
		xdelete(vShutdownDeletes[i]);
		LOG_DEBUG(_T("Clear: Safely deleted stalled quarantine connection during shutdown."));
	}
}