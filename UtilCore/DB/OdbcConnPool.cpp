
//***************************************************************************
// OdbcConnPool.cpp : implementation of the COdbcConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcConnPool.h"
#include <random>
#include <algorithm>

constexpr int32 WAIT_TIMEOUT_MS = 100;          // 슬롯 교체 시 사용 중 스레드 이탈 대기 상한
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000; // 격리 큐 정체 경고 로그 최소 주기 (5분)

//***************************************************************************
// Construction/Destruction
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
	, _nNextSlotHint(0)
	, _bStopReconnectWorkers(false)
	, _nCurrentWorkerCount(0)
	, _nDesiredWorkerCount(0)
{
	// unique_ptr 배열은 생성자에서 딱 1회만 할당 (주소 안정성 보장)
	_pOdbcConns = std::make_unique<std::atomic<CBaseODBC*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<std::atomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize); // 캐시라인 정렬로 false sharing 방지

	_pReconnecting = std::make_unique<std::atomic<bool>[]>(_nMaxPoolSize);
	_pNextRetryAllowedMs = std::make_unique<std::atomic<int64>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<std::atomic<int32>[]>(_nMaxPoolSize);

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pOdbcConns[i].store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].store(0, std::memory_order_relaxed);
		_pReconnecting[i].store(false, std::memory_order_relaxed);
		_pNextRetryAllowedMs[i].store(0, std::memory_order_relaxed);
		_pRetryFailCount[i].store(0, std::memory_order_relaxed);
	}
	memset(&_tszDSN[0], 0, sizeof(_tszDSN));
}

COdbcConnPool::~COdbcConnPool(void)
{
	// 백그라운드 스레드를 먼저 종료해 레이스컨디션을 방지한 뒤 자원 정리
	StopHealthCheckThread();
	StopReconnectWorkers();
	Clear();
}

//***************************************************************************
// TReconnectConfig 값 유효성 검사 (Init / SetReconnectConfig 공용)
//***************************************************************************
bool COdbcConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	// base가 너무 짧으면 DB 다운 시 재연결이 사실상 무한루프처럼 돌아 CPU/연결 폭주로 이어짐
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	// shift가 너무 크면 (BaseMs << shift)에서 오버플로 발생 가능
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

//***************************************************************************
// 초기화
//***************************************************************************
bool COdbcConnPool::Init(const EDBClass dbClass, const TCHAR* ptszDSN,
	const TReconnectConfig& reconnectConfig)
{
	// 재초기화 시나리오 대비, 기존 백그라운드 스레드/자원 정리 선행
	StopHealthCheckThread();
	StopReconnectWorkers();
	Clear();

	_dbClass = dbClass;
	_tcsncpy_s(_tszDSN, _countof(_tszDSN), ptszDSN, _TRUNCATE);

	// 유효하지 않은 설정은 부분 적용하지 않고 통째로 기본값 대체
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

	// 최대 크기만큼 커넥션을 동기적으로 미리 채움
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

		_pOdbcConns[i].store(pConn, std::memory_order_release); // release로 게시
	}

	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

//***************************************************************************
// 새 커넥션 생성 및 연결 시도 (순수 재연결 로직, 블로킹 I/O)
//***************************************************************************
CBaseODBC* COdbcConnPool::TryReconnect(int32 nType)
{
	// 슬롯 획득/락과 분리된 상태에서 새 커넥션을 별도로 생성해 대기 시간과 경쟁을 최소화
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
// 슬롯 참조 획득
//***************************************************************************
CBaseODBC* COdbcConnPool::GetOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetOdbcConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	// 1. 참조 카운트를 낙관적으로 먼저 증가시켜, 재연결 워커가 삭제/재할당하지 못하도록 선점
	_pRefCount[nType].fetch_add(1, std::memory_order_relaxed);

	// 2. acquire로 최신 포인터 로드
	CBaseODBC* pOdbcConn = _pOdbcConns[nType].load(std::memory_order_acquire);

	// 3. 무효 상태면 선점 취소 후 nullptr 반환 (헬스체크가 감지해 재연결 위임)
	if( pOdbcConn == nullptr || !pOdbcConn->IsConnected() )
	{
		_pRefCount[nType].fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetOdbcConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	return pOdbcConn; // 호출자는 사용 후 반드시 ReleaseOdbcConn 필요
}

//***************************************************************************
// PopFreeSlotIndex 이후 refcount 변경 없이 조회 (OdbcConnGuard 전용)
//***************************************************************************
CBaseODBC* COdbcConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;
	return _pOdbcConns[nType].load(std::memory_order_acquire);
}

//***************************************************************************
// 자원 반환
//***************************************************************************
void COdbcConnPool::ReleaseOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;
	// 카운트가 0이 되면 헬스체크/재연결 워커가 감지해 재활용 가능
	_pRefCount[nType].fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// 빈 슬롯 검색 및 즉시 선점 (실패 시 -1). 힌트 회전으로 경합 분산.
//***************************************************************************
int32 COdbcConnPool::PopFreeSlotIndex(void) {
	uint32 nStart = _nNextSlotHint.fetch_add(1, std::memory_order_relaxed);

	for( int32 k = 0; k < _nMaxPoolSize; ++k ) {
		int32 i = static_cast<int32>((nStart + k) % _nMaxPoolSize);

		int32 expected = 0;
		if( _pRefCount[i].compare_exchange_strong(expected, 1, std::memory_order_acq_rel) ) {
			CBaseODBC* pConn = _pOdbcConns[i].load(std::memory_order_acquire);
			if( pConn && pConn->IsConnected() ) return i;

			_pRefCount[i].store(0, std::memory_order_release); // 잘못 선점한 슬롯 되돌림
		}
	}
	return -1;
}

//***************************************************************************
// 재연결 워커가 새 커넥션 확보 후 호출하는 슬롯 교체 마무리
//***************************************************************************
void COdbcConnPool::ApplyReconnectedConn(int32 nType, CBaseODBC* pNewConn)
{
	// 대기 중 슬롯이 다시 사용 중으로 바뀌었다면 새 자원 폐기
	if( _pRefCount[nType].load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	// 슬롯 스핀락 하에서 교체(Swap)
	CBaseODBC* pOldConn = nullptr;
	{
		SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[nType]);
		pOldConn = _pOdbcConns[nType].load(std::memory_order_acquire);
		_pOdbcConns[nType].store(pNewConn, std::memory_order_release);
	}

	if( pOldConn == nullptr ) return;

	// 교체 직후 낡은 자원을 참조 중이던 스레드가 빠져나갈 때까지 짧게 대기
	auto startTime = std::chrono::steady_clock::now();
	bool bTimeout = false;

	while( _pRefCount[nType].load(std::memory_order_acquire) > 0 )
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - startTime).count();

		if( elapsed >= WAIT_TIMEOUT_MS )
		{
			bTimeout = true;
			break;
		}
		std::this_thread::yield();
	}

	if( bTimeout )
	{
		// 타임아웃 초과 시 강제 삭제하면 UAF 위험 → 격리 큐로 편입
		LOG_ERROR(_T("ReconnectWorker: Slot(%d) refcount high during swap. Moving to quarantine."), nType);

		auto now = std::chrono::steady_clock::now();
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType], now });
	}
	else
	{
		xdelete(pOldConn); // 참조 없음 확인 후 즉시 삭제
	}
}

//***************************************************************************
// steady_clock 기준 현재 시각 (밀리초, 백오프 비교용)
//***************************************************************************
int64 COdbcConnPool::NowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

//***************************************************************************
// 슬롯이 지금 재연결 시도 가능한지 검사 (백오프 대기 중이면 false)
//***************************************************************************
bool COdbcConnPool::IsRetryAllowed(int32 nType) const
{
	int64 nNextAllowed = _pNextRetryAllowedMs[nType].load(std::memory_order_acquire);
	return NowMs() >= nNextAllowed;
}

//***************************************************************************
// 재연결 실패 처리: 실패 횟수 증가, 다음 허용 시각을 지수적으로(+지터) 연기
// (DB 전체 다운 시 모든 슬롯이 동시에 재시도해 connection storm이 되는 것을 방지)
//***************************************************************************
void COdbcConnPool::OnReconnectFailed(int32 nType)
{
	int32 nFailCount = _pRetryFailCount[nType].fetch_add(1, std::memory_order_acq_rel) + 1;

	int32 nMaxShift = _nBackoffMaxShift.load(std::memory_order_relaxed);
	int32 nShift = std::min(nFailCount, nMaxShift);

	int64 nBaseMs = _nBackoffBaseMs.load(std::memory_order_relaxed);
	int64 nMaxMs = _nBackoffMaxMs.load(std::memory_order_relaxed);

	int64 nDelayMs = nBaseMs << nShift; // BASE * 2^nShift
	nDelayMs = std::min<int64>(nDelayMs, nMaxMs);

	// 슬롯 간 재시도 타이밍이 겹치지 않도록 지터 추가
	thread_local std::mt19937 rng(std::random_device{}());
	int32 nJitterMax = _nBackoffJitterMs.load(std::memory_order_relaxed);
	std::uniform_int_distribution<int32> jitterDist(0, std::max(nJitterMax, 0));
	nDelayMs += jitterDist(rng);

	_pNextRetryAllowedMs[nType].store(NowMs() + nDelayMs, std::memory_order_release);

	LOG_DEBUG(_T("OnReconnectFailed: slot(%d) failCount(%d), next retry in %lldms"),
		nType, nFailCount, static_cast<long long>(nDelayMs));
}

//***************************************************************************
// 재연결 성공 처리: 백오프 상태 초기화
//***************************************************************************
void COdbcConnPool::OnReconnectSucceeded(int32 nType)
{
	_pRetryFailCount[nType].store(0, std::memory_order_relaxed);
	_pNextRetryAllowedMs[nType].store(0, std::memory_order_release);
}

//***************************************************************************
// 헬스체크 루프: 격리 큐 청소 + 죽은 슬롯 스캔/디스패치 (실제 재연결 I/O는 워커 풀에 위임, 논블로킹)
//***************************************************************************
void COdbcConnPool::HealthCheckLoop(void)
{
	// 매 주기 재할당 비용을 없애기 위해 루프 밖에서 한 번만 생성, capacity 재사용
	CVector<CBaseODBC*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		vDeletes.clear();

		// PART A: 격리 큐 청소 (락 안에서는 분류만, 삭제는 락 밖에서)
		{
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size(); // 이번 순회에서 새로 들어온 항목은 제외

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				if( item.pRefCount->load(std::memory_order_acquire) == 0 )
				{
					if( item.pConn != nullptr )
					{
						vDeletes.push_back(item.pConn); // 락 밖에서 삭제하도록 적재
					}
				}
				else
				{
					// 5분 주기로만 경고 로그 (반복 로그 남발 방지)
					auto now = std::chrono::steady_clock::now();
					auto elapsedFromLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - item.lastLogTime).count();

					if( elapsedFromLastLog >= LOG_ALERT_INTERVAL_MS )
					{
						LOG_ERROR(_T("Quarantine Persistent Warning: Connection is still stuck in quarantine! Potential leak in application logic."));
						item.lastLogTime = now;
					}

					_quarantineQueue.push(item); // 아직 해제 불가, 재삽입
				}
			}
		} // qGuard 해제

		// PART A-1: 락 바깥에서 실제 삭제
		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine: Safely deleted stalled connection outside the lock."));
		}

		// PART B: 죽은 슬롯 스캔 및 재연결 디스패치 (블로킹 없음)
		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;      // 사용 중이면 패스

			CBaseODBC* pCur = _pOdbcConns[i].load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->IsConnected() ) continue;                 // 정상 연결이면 패스

			if( !IsRetryAllowed(i) ) continue;                                     // 백오프 대기 중이면 패스

			bool bExpected = false;
			if( !_pReconnecting[i].compare_exchange_strong(bExpected, true, std::memory_order_acq_rel) )
				continue;                                                          // 이미 처리 중이면 패스

			EnqueueReconnect(i); // 실제 재연결은 워커 풀에 위임
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_nHealthCheckIntervalMs));
	}
}

void COdbcConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

void COdbcConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads(); // 완전 종료 확인
}

//***************************************************************************
// 재연결 대기열에 슬롯을 넣고 워커 하나를 깨움
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
// 이 워커가 초과 인원인지 CAS로 판정, 초과면 카운트 감소 후 true 반환
// (동시에 여러 워커가 판정해도 목표치 초과분만 정확히 빠짐)
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
		// CAS 실패 시 nCur가 최신값으로 갱신되어 재평가됨
	}
	return false;
}

//***************************************************************************
// 재연결 워커 루프: 대기열에서 슬롯을 꺼내 TryReconnect(I/O) + 스왑을 병렬 수행
//***************************************************************************
void COdbcConnPool::ReconnectWorkerLoop(void)
{
	while( true )
	{
		// 매 순회 시작 시 스스로 초과 인원인지 확인 (SetWorkerCount 축소 요청 즉시 반영)
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

			if( _reconnectPendingSlots.empty() ) continue; // TryExitIfExcess 재확인으로 복귀

			nType = _reconnectPendingSlots.front();
			_reconnectPendingSlots.pop();
		}

		// 대기 중 슬롯이 이미 다시 사용 중이면 재연결 불필요
		if( _pRefCount[nType].load(std::memory_order_acquire) > 0 )
		{
			_pReconnecting[nType].store(false, std::memory_order_release);
			continue;
		}

		// 블로킹 I/O 구간 — 다른 워커는 각자 슬롯을 병렬 처리
		CBaseODBC* pNewConn = TryReconnect(nType);
		if( pNewConn == nullptr )
		{
			OnReconnectFailed(nType); // 실패 시 백오프 적용, 다음 헬스체크 순회에서 재큐잉
			_pReconnecting[nType].store(false, std::memory_order_release);
			continue;
		}

		ApplyReconnectedConn(nType, pNewConn);
		OnReconnectSucceeded(nType);

		_pReconnecting[nType].store(false, std::memory_order_release);
	}
}

//***************************************************************************
// nWorkerCount개의 워커 스레드로 재연결 풀 신규 기동 (Init 전용)
//***************************************************************************
void COdbcConnPool::StartReconnectWorkers(int32 nWorkerCount)
{
	_bStopReconnectWorkers.store(false, std::memory_order_relaxed);

	// 방어적 최소값 보정 (ValidateReconnectConfig를 거쳤다면 정상적으로는 발생하지 않음)
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

void COdbcConnPool::StopReconnectWorkers(void)
{
	_bStopReconnectWorkers.store(true, std::memory_order_relaxed);
	_reconnectQueueCv.notify_all(); // 대기 중인 모든 워커를 깨워 종료 조건 확인
	_reconnectWorkerMgr.JoinThreads();
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);
	_nDesiredWorkerCount.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// 목표 워커 수 갱신 (SetReconnectConfig 전용)
// 확대: 목표치 즉시 갱신 후 부족분만큼 CAS로 조율하며 스폰 (중복 스폰 방지)
// 축소: 스레드를 직접 건드리지 않고 목표치만 낮춘 뒤 조건변수로 깨움 →
//       각 워커가 다음 순회에서 TryExitIfExcess()로 스스로 종료 판단.
//       반복/역전 호출이 있어도 항상 최종 목표치로 정확히 수렴.
//***************************************************************************
void COdbcConnPool::SetWorkerCount(int32 nNewCount)
{
	nNewCount = std::max(nNewCount, 1);
	_nDesiredWorkerCount.store(nNewCount, std::memory_order_release);

	int32 nExpected = _nCurrentWorkerCount.load(std::memory_order_acquire);
	if( nExpected < nNewCount )
	{
		int32 nBefore = nExpected;
		// CAS로 목표치까지 원자적으로 끌어올려 실제 스폰할 정확한 개수를 결정
		while( nExpected < nNewCount &&
			!_nCurrentWorkerCount.compare_exchange_weak(nExpected, nNewCount, std::memory_order_acq_rel) )
		{
			// CAS 실패 시 nExpected 최신값으로 재평가
		}

		if( nExpected < nNewCount ) // CAS 성공 시에만 그만큼 스폰
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
		_reconnectQueueCv.notify_all(); // 축소는 워커 스스로 판단하도록 깨우기만 함
		LOG_DEBUG(_T("SetWorkerCount: target worker count decreased to %d, workers will self-exit"), nNewCount);
	}
}

//***************************************************************************
// 재연결 백오프 정책과 워커 수를 런타임에 조정
//***************************************************************************
bool COdbcConnPool::SetReconnectConfig(const TReconnectConfig& reconnectConfig)
{
	if( !ValidateReconnectConfig(reconnectConfig) )
	{
		LOG_ERROR(_T("SetReconnectConfig: invalid TReconnectConfig rejected, no change applied"));
		return false;
	}

	// 필드별 독립 원자 변수라 완전 동기 스냅샷은 아니지만, 각 값이 개별 판단에만 쓰여 문제 없음
	_nBackoffBaseMs.store(reconnectConfig.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(reconnectConfig.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(reconnectConfig.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(reconnectConfig.nBackoffJitterMs, std::memory_order_relaxed);

	// 워커 수는 스레드 기동/종료 조율이 필요해 별도 경로로 처리
	SetWorkerCount(reconnectConfig.nWorkerCount);

	return true;
}

//***************************************************************************
// 현재 재연결 설정 스냅샷 조회
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
// 풀 자원 최종 청소 (Shutdown)
//***************************************************************************
void COdbcConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CBaseODBC*> vShutdownDeletes;

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseODBC* pConn = _pOdbcConns[i].load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;

		// 사용 중인 스레드가 이탈할 때까지 대기
		while( _pRefCount[i].load(std::memory_order_acquire) > 0 )
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - startTime).count();

			if( elapsed >= WAIT_TIMEOUT_MS )
			{
				bTimeout = true;
				break;
			}
			std::this_thread::yield();
		}

		if( bTimeout )
		{
			LOG_ERROR(_T("Clear: Slot(%d) refcount is zombie (%d). Moving to quarantine."), i, _pRefCount[i].load());

			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i], now });
		}
		else
		{
			xdelete(pConn); // 참조 없음, 즉시 삭제
		}

		_pOdbcConns[i].store(nullptr, std::memory_order_relaxed);
	}

	// 셧다운 시점 격리 큐 처리 (락-밖 소멸 유지)
	{
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		while( !_quarantineQueue.empty() )
		{
			TQuarantineItem item = _quarantineQueue.front();
			_quarantineQueue.pop();

			if( item.pRefCount->load(std::memory_order_acquire) == 0 )
			{
				if( item.pConn != nullptr )
				{
					vShutdownDeletes.push_back(item.pConn); // 락 밖에서 최종 삭제
				}
			}
			else
			{
				// [Safe Leak] 셧다운 시점 UAF 크래시를 방지하기 위해 삭제를 의도적으로 방기
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