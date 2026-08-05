
//***************************************************************************
// AdoConnPool.cpp : implementation of the CAdoConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "AdoConnPool.h"
#include <random>

constexpr int32 WAIT_TIMEOUT_MS = 100;              // 커넥션 스왑 시 기존 객체 참조 해제 대기 타임아웃 (밀리초)
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000;     // 격리 큐 잔류 지속 경고 로그 출력 주미 (5분)
constexpr int64 FORCE_CLEANUP_TIMEOUT_MS = 600000;  // 격리 큐 강제 정리 대기 시간 상한선 (10분)

//***************************************************************************
// CAdoConnPool::CAdoConnPool
// @brief CAdoConnPool 클래스의 생성자입니다. 지정된 최대 풀 크기로 관리 배열들을 초기화합니다.
// @param nMaxPoolSize 생성할 커넥션 풀의 최대 슬롯 크기
//***************************************************************************
CAdoConnPool::CAdoConnPool(int32 nMaxPoolSize)
	: _dbClass(EDBClass::NONE)
	, _nTimeOut(5)
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
	// 풀 크기에 맞춰 각 관리 배열을 동적 할당
	_pAdoConns = std::make_unique<CachePaddedAtomic<CAdoDB*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<PLock[]>(_nMaxPoolSize);

	_pReconnecting = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);

	// 모든 슬롯의 상태 초기화
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pAdoConns[i].value.store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].value.store(0, std::memory_order_relaxed);
		_pReconnecting[i].value.store(false, std::memory_order_relaxed);
		_pRetryFailCount[i].value.store(0, std::memory_order_relaxed);
	}
	memset(&_tszConnStr[0], 0, sizeof(_tszConnStr));
}

//***************************************************************************
// CAdoConnPool::~CAdoConnPool
// @brief CAdoConnPool 클래스의 소멸자입니다. 백그라운드 스레드를 중지하고 자원을 해제합니다.
//***************************************************************************
CAdoConnPool::~CAdoConnPool(void)
{
	// 백그라운드 스레드들을 안전하게 중지시킨 뒤 풀 자원 해제
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();
}

//***************************************************************************
// CAdoConnPool::ValidateReconnectConfig
// @brief 재접속 및 백오프 설정값의 유효성을 검증합니다.
// @param cfg 검증할 재접속 및 백오프 설정 구조체
// @return 설정값이 유효하면 true, 유효하지 않으면 false
//***************************************************************************
bool CAdoConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

//***************************************************************************
// CAdoConnPool::Init
// @brief 커넥션 풀을 초기화하고, 지정된 설정으로 초기 커넥션을 미리 생성하여 연결합니다.
// @param dbClass 데이터베이스 종류 식별자
// @param ptszConnStr 데이터베이스 접속 문자열 (Connection String) 포인터
// @param nTimeOut 접속 및 쿼리 타임아웃 제한 시간 (초)
// @param reconnectConfig 초기 재연결 및 백오프 정책 설정 구조체
// @return 초기화 및 전체 연결 성공 시 true, 실패 시 false
//***************************************************************************
bool CAdoConnPool::Init(const EDBClass dbClass, const TCHAR* ptszConnStr, const int nTimeOut,
	const TReconnectConfig& reconnectConfig)
{
	// 기존에 구동 중인 스레드와 자원을 우선 정리
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();

	_dbClass = dbClass;
	_nTimeOut = nTimeOut;
	_tcsncpy_s(_tszConnStr, _countof(_tszConnStr), ptszConnStr, _TRUNCATE);

	TReconnectConfig cfgToApply = reconnectConfig;
	if( !ValidateReconnectConfig(reconnectConfig) )
	{
		LOG_ERROR(_T("Init (ADO): invalid TReconnectConfig, falling back to default"));
		cfgToApply = TReconnectConfig{};
	}

	// 백오프 설정값 반영
	_nBackoffBaseMs.store(cfgToApply.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(cfgToApply.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(cfgToApply.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(cfgToApply.nBackoffJitterMs, std::memory_order_relaxed);

	// 초기 설정된 최대 풀 크기만큼 커넥션을 생성 및 DB 접속 수행
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CAdoDB* pConn = xnew<CAdoDB>();
		if( pConn == nullptr )
		{
			LOG_ERROR(_T("Init (ADO): xnew<CAdoDB> failed (index=%d)"), i);
			Clear();
			return false;
		}

		if( pConn->Connect(_dbClass, _tszConnStr, _nTimeOut) < 0 )
		{
			xdelete(pConn);
			Clear();
			return false;
		}

		_pAdoConns[i].value.store(pConn, std::memory_order_release);
	}

	// 백그라운드 관리 스레드 및 워커 구동
	StartDelayedTaskThread();
	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

//***************************************************************************
// CAdoConnPool::TryReconnect
// @brief 지정된 슬롯 인덱스에 대해 새로운 ADO 커넥션 객체를 생성하고 DB 재접속을 시도합니다.
// @param nType 재접속을 시도할 풀 슬롯 인덱스
// @return 재접속에 성공한 CAdoDB 객체 포인터, 실패 시 nullptr
//***************************************************************************
CAdoDB* CAdoConnPool::TryReconnect(int32 nType)
{
	CAdoDB* pNewConn = xnew<CAdoDB>();
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect (ADO): xnew<CAdoDB> failed (index=%d)"), nType);
		return nullptr;
	}

	// 새로운 커넥션 객체로 DB 접속 시도
	if( pNewConn->Connect(_dbClass, _tszConnStr, _nTimeOut) < 0 )
	{
		xdelete(pNewConn);
		return nullptr;
	}

	return pNewConn;
}

//***************************************************************************
// CAdoConnPool::GetAdoConn
// @brief 특정 풀 슬롯 인덱스의 ADO 커넥션을 대여합니다 (참조 카운트 증가).
// @param nType 대여하고자 하는 특정 풀 슬롯 인덱스
// @return 유효한 CAdoDB 객체 포인터, 준비되지 않았거나 실패 시 nullptr
//***************************************************************************
CAdoDB* CAdoConnPool::GetAdoConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetAdoConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	// 사용을 위해 참조 카운트 선제 증가
	_pRefCount[nType].value.fetch_add(1, std::memory_order_relaxed);
	CAdoDB* pAdoConn = _pAdoConns[nType].value.load(std::memory_order_acquire);

	// 커넥션이 비정상이거나 준비되지 않은 경우 참조 카운트 복원 후 실패 처리
	if( pAdoConn == nullptr || !pAdoConn->GetDBCon() )
	{
		_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetAdoConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	return pAdoConn;
}

//***************************************************************************
// CAdoConnPool::GetPooledConnUnsafe
// @brief 락이나 참조 카운트 조절 없이 특정 슬롯의 풀링된 커넥션 포인터를 직접 조회합니다.
// @param nType 조회할 풀 슬롯 인덱스
// @return CAdoDB 객체 포인터, 인덱스가 유효하지 않으면 nullptr
//***************************************************************************
CAdoDB* CAdoConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;
	return _pAdoConns[nType].value.load(std::memory_order_acquire);
}

//***************************************************************************
// CAdoConnPool::ReleaseAdoConn
// @brief 사용을 완료한 커넥션 슬롯의 참조 카운트를 감소시킵니다.
// @param nType 사용을 완료한 커넥션 슬롯 인덱스
//***************************************************************************
void CAdoConnPool::ReleaseAdoConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;
	// 사용 완료에 따른 참조 카운트 감소
	_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// CAdoConnPool::PopFreeSlotIndex
// @brief 라운드 로빈 방식으로 사용 가능하고 즉시 대여할 수 있는 프리 슬롯의 인덱스를 탐색하여 반환합니다.
// @return 사용 가능한 슬롯 인덱스, 없거나 실패 시 -1
//***************************************************************************
int32 CAdoConnPool::PopFreeSlotIndex(void) {
	uint32 nStart = _nNextSlotHint.fetch_add(1, std::memory_order_relaxed);

	// 라운드 로빈 방식으로 사용 가능한 프리 슬롯 탐색
	for( int32 k = 0; k < _nMaxPoolSize; ++k ) {
		int32 i = static_cast<int32>((nStart + k) % _nMaxPoolSize);

		int32 expected = 0;
		if( _pRefCount[i].value.compare_exchange_strong(expected, 1, std::memory_order_acq_rel) ) {
			CAdoDB* pConn = _pAdoConns[i].value.load(std::memory_order_acquire);
			if( pConn && pConn->GetDBCon() ) return i;

			_pRefCount[i].value.store(0, std::memory_order_release);
		}
	}
	return -1;
}

//***************************************************************************
// CAdoConnPool::ApplyReconnectedConn
// @brief 재접속된 새로운 커넥션을 해당 슬롯에 반영하고 기존 커넥션을 안전하게 교체 또는 격리합니다.
// @param nType 새로 연결된 커넥션을 반영할 슬롯 인덱스
// @param pNewConn 새로 생성 및 접속 완료된 ADO 커넥션 객체 포인터
//***************************************************************************
void CAdoConnPool::ApplyReconnectedConn(int32 nType, CAdoDB* pNewConn)
{
	// 재연결을 완료하는 동안 슬롯이 다시 대여 상태가 되었다면 새로 만든 커넥션을 폐기
	if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	CAdoDB* pOldConn = nullptr;
	{
		PLockGuard guard(_slotLocks[nType]);
		pOldConn = _pAdoConns[nType].value.load(std::memory_order_acquire);
		_pAdoConns[nType].value.store(pNewConn, std::memory_order_release);
	}

	if( pOldConn == nullptr ) return;

	auto startTime = std::chrono::steady_clock::now();
	bool bTimeout = false;
	int spinCount = 0;

	// 기존 커넥션을 참조 중인 스레드들이 모두 빠져나갈 때까지 대기
	while( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
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

	// 타임아웃 발생 시 즉시 해제하지 못하므로 안전하게 격리(Quarantine) 큐로 이관
	if( bTimeout )
	{
		LOG_ERROR(_T("ReconnectWorker (ADO): Slot(%d) refcount high during swap. Moving to quarantine."), nType);

		auto now = std::chrono::steady_clock::now();
		PLockGuard qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType].value, now });
	}
	else
	{
		xdelete(pOldConn);
	}
}

//***************************************************************************
// CAdoConnPool::ScheduleRetry
// @brief 재접속 실패 시 지수 백오프(Exponential Backoff) 및 지터(Jitter)를 계산하여 재시도 작업을 예약합니다.
// @param nType 재시도를 예약할 슬롯 인덱스
//***************************************************************************
void CAdoConnPool::ScheduleRetry(int32 nType)
{
	// 실패 횟수 증가 및 지수 백오프 대기 시간 계산
	int32 nFailCount = _pRetryFailCount[nType].value.fetch_add(1, std::memory_order_acq_rel) + 1;

	int32 nMaxShift = _nBackoffMaxShift.load(std::memory_order_relaxed);
	int32 nShift = std::min(nFailCount, nMaxShift);

	int64 nBaseMs = _nBackoffBaseMs.load(std::memory_order_relaxed);
	int64 nMaxMs = _nBackoffMaxMs.load(std::memory_order_relaxed);

	int64 nDelayMs = nBaseMs << nShift;
	nDelayMs = std::min<int64>(nDelayMs, nMaxMs);

	// Jitter(무작위 지연) 추가
	thread_local std::mt19937 rng(std::random_device{}());
	int32 nJitterMax = _nBackoffJitterMs.load(std::memory_order_relaxed);
	std::uniform_int_distribution<int32> jitterDist(0, std::max(nJitterMax, 0));
	nDelayMs += jitterDist(rng);

	LOG_DEBUG(_T("ScheduleRetry (ADO): slot(%d) failCount(%d), retrying in %lldms"),
		nType, nFailCount, static_cast<long long>(nDelayMs));

	// 지연 작업 큐에 재시도 작업 등록
	_delayedTaskQueue.Reserve(static_cast<int>(nDelayMs), [this, nType]() {
		CAdoDB* pCur = _pAdoConns[nType].value.load(std::memory_order_acquire);
		if( _pRefCount[nType].value.load(std::memory_order_acquire) == 0 &&
			(pCur == nullptr || !pCur->GetDBCon()) )
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
// CAdoConnPool::OnReconnectFailed
// @brief 재접속 시도가 실패했을 때 호출되어 후속 재시도 스케줄링을 트리거합니다.
// @param nType 재접속 시도가 실패한 슬롯 인덱스
//***************************************************************************
void CAdoConnPool::OnReconnectFailed(int32 nType)
{
	ScheduleRetry(nType);
}

//***************************************************************************
// CAdoConnPool::OnReconnectSucceeded
// @brief 재접속 시도가 성공했을 때 호출되어 슬롯의 재시도 실패 카운트를 초기화합니다.
// @param nType 재접속 시도가 성공한 슬롯 인덱스
//***************************************************************************
void CAdoConnPool::OnReconnectSucceeded(int32 nType)
{
	// 재연결 성공 시 실패 카운트 초기화
	_pRetryFailCount[nType].value.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// CAdoConnPool::HealthCheckLoop
// @brief 헬스체크 백그라운드 스레드의 메인 루프입니다. 격리 큐 정리 및 끊어진 커넥션 감지 후 재접속을 유도합니다.
//***************************************************************************
void CAdoConnPool::HealthCheckLoop(void)
{
	CVector<CAdoDB*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		vDeletes.clear();

		// 격리 큐에 있는 좀비 커넥션들의 참조가 풀렸거나 만료되었는지 검사
		{
			PLockGuard qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size();

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				auto now = std::chrono::steady_clock::now();
				auto elapsedTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - item.lastLogTime).count();

				if( item.pRefCount->load(std::memory_order_acquire) == 0 || elapsedTotal >= FORCE_CLEANUP_TIMEOUT_MS )
				{
					if( item.pConn != nullptr )
					{
						if( elapsedTotal >= FORCE_CLEANUP_TIMEOUT_MS )
						{
							LOG_ERROR(_T("Quarantine Force Cleanup (ADO): Connection exceeded max quarantine time. Forcibly deleting."));
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
						LOG_ERROR(_T("Quarantine Persistent Warning (ADO): Connection is still stuck in quarantine!"));
						item.lastLogTime = now;
					}

					_quarantineQueue.push(item);
				}
			}
		}

		// 락 외부에서 안전하게 자원 해제
		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine (ADO): Safely deleted stalled connection outside the lock."));
		}

		// 전체 슬롯을 순회하며 끊어진 커넥션 감지 및 재접속 트리거
		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			if( _pRefCount[i].value.load(std::memory_order_acquire) > 0 ) continue;

			CAdoDB* pCur = _pAdoConns[i].value.load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->GetDBCon() ) continue;

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
// CAdoConnPool::StartHealthCheckThread
// @brief 헬스체크 백그라운드 스레드를 생성하고 구동합니다.
//***************************************************************************
void CAdoConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

//***************************************************************************
// CAdoConnPool::StopHealthCheckThread
// @brief 헬스체크 백그라운드 스레드를 중지시키고 종료를 대기합니다.
//***************************************************************************
void CAdoConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads();
}

//***************************************************************************
// CAdoConnPool::DelayedTaskLoop
// @brief 지연 작업 처리 스레드의 메인 루프입니다. 만료된 예약 작업을 처리합니다.
//***************************************************************************
void CAdoConnPool::DelayedTaskLoop(void)
{
	_delayedTaskQueue.ProcessExpiredTasks();
}

//***************************************************************************
// CAdoConnPool::StartDelayedTaskThread
// @brief 지연 작업 처리 스레드를 생성하고 구동합니다.
//***************************************************************************
void CAdoConnPool::StartDelayedTaskThread(void)
{
	_delayedTaskThreadMgr.CreateThread([this]() { DelayedTaskLoop(); });
}

//***************************************************************************
// CAdoConnPool::StopDelayedTaskThread
// @brief 지연 작업 처리 스레드를 중지시키고 종료를 대기합니다.
//***************************************************************************
void CAdoConnPool::StopDelayedTaskThread(void)
{
	_delayedTaskQueue.Stop();
	_delayedTaskThreadMgr.JoinThreads();
}

//***************************************************************************
// CAdoConnPool::EnqueueReconnect
// @brief 재접속이 필요한 슬롯 인덱스를 재접속 대기 큐에 등록하고 워커 스레드에 알립니다.
// @param nType 재접속 대기열에 등록할 슬롯 인덱스
//***************************************************************************
void CAdoConnPool::EnqueueReconnect(int32 nType)
{
	{
		std::lock_guard<std::mutex> lock(_reconnectQueueMutex);
		_reconnectPendingSlots.push(nType);
	}
	_reconnectQueueCv.notify_one();
}

//***************************************************************************
// CAdoConnPool::TryExitIfExcess
// @brief 초과된 워커 스레드일 경우 스스로 종료 처리를 수행합니다.
// @return 초과 워커여서 종료하는 경우 true, 아니면 false
//***************************************************************************
bool CAdoConnPool::TryExitIfExcess(void)
{
	int32 nCur = _nCurrentWorkerCount.load(std::memory_order_acquire);
	while( nCur > _nDesiredWorkerCount.load(std::memory_order_acquire) )
	{
		if( _nCurrentWorkerCount.compare_exchange_weak(nCur, nCur - 1, std::memory_order_acq_rel) )
		{
			LOG_DEBUG(_T("ReconnectWorker (ADO): self-exiting as excess worker"));
			return true;
		}
	}
	return false;
}

//***************************************************************************
// CAdoConnPool::ReconnectWorkerLoop
// @brief 재접속 전용 백그라운드 워커 스레드의 메인 루프입니다. 큐에서 슬롯을 꺼내 재접속을 수행합니다.
//***************************************************************************
void CAdoConnPool::ReconnectWorkerLoop(void)
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
					|| _nCurrentWorkerCount.load(std::memory_order_acquire) > _nDesiredWorkerCount.load(std::memory_order_acquire);
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

		// 재접속 시도
		CAdoDB* pNewConn = TryReconnect(nType);
		if( pNewConn == nullptr )
		{
			OnReconnectFailed(nType);
			_pReconnecting[nType].value.store(false, std::memory_order_release);
			continue;
		}

		// 재접속 성공 시 슬롯 교체 및 상태 초기화
		ApplyReconnectedConn(nType, pNewConn);
		OnReconnectSucceeded(nType);
		_pReconnecting[nType].value.store(false, std::memory_order_release);
	}
}

//***************************************************************************
// CAdoConnPool::StartReconnectWorkers
// @brief 지정된 개수만큼 재접속 워커 스레드를 생성하고 구동합니다.
// @param nWorkerCount 구동할 백그라운드 재접속 워커 스레드 수
//***************************************************************************
void CAdoConnPool::StartReconnectWorkers(int32 nWorkerCount)
{
	_bStopReconnectWorkers.store(false, std::memory_order_relaxed);
	int32 nClamped = std::max(nWorkerCount, 1);
	_nDesiredWorkerCount.store(nClamped, std::memory_order_relaxed);
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);

	for( int32 i = 0; i < nClamped; ++i )
	{
		_reconnectWorkerMgr.CreateThread([this]() { ReconnectWorkerLoop(); });
	}
	_nCurrentWorkerCount.store(nClamped, std::memory_order_release);
}

//***************************************************************************
// CAdoConnPool::StopReconnectWorkers
// @brief 재접속 워커 스레드들을 중지시키고 종료를 대기합니다.
//***************************************************************************
void CAdoConnPool::StopReconnectWorkers(void)
{
	_bStopReconnectWorkers.store(true, std::memory_order_relaxed);
	_reconnectQueueCv.notify_all();
	_reconnectWorkerMgr.JoinThreads();
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);
	_nDesiredWorkerCount.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// CAdoConnPool::SetWorkerCount
// @brief 재접속 워커 스레드의 목표 개수를 동적으로 변경합니다.
// @param nNewCount 변경할 목표 워커 스레드 수
//***************************************************************************
void CAdoConnPool::SetWorkerCount(int32 nNewCount)
{
	nNewCount = std::max(nNewCount, 1);
	_nDesiredWorkerCount.store(nNewCount, std::memory_order_release);

	int32 nExpected = _nCurrentWorkerCount.load(std::memory_order_acquire);
	if( nExpected < nNewCount )
	{
		int32 nBefore = nExpected;
		while( nExpected < nNewCount &&
			!_nCurrentWorkerCount.compare_exchange_weak(nExpected, nNewCount, std::memory_order_acq_rel) ) {
		}

		if( nExpected < nNewCount )
		{
			int32 nToAdd = nNewCount - nExpected;
			for( int32 i = 0; i < nToAdd; ++i )
			{
				_reconnectWorkerMgr.CreateThread([this]() { ReconnectWorkerLoop(); });
			}
			LOG_DEBUG(_T("SetWorkerCount (ADO): worker count increased %d -> %d"), nBefore, nNewCount);
		}
	}
	else
	{
		_reconnectQueueCv.notify_all();
		LOG_DEBUG(_T("SetWorkerCount (ADO): target worker count decreased to %d"), nNewCount);
	}
}

//***************************************************************************
// CAdoConnPool::SetReconnectConfig
// @brief 재접속 및 백오프 정책 설정값을 동적으로 변경합니다.
// @param reconnectConfig 새로 적용할 재접속 및 백오프 설정 구조체
// @return 설정 검증 및 적용 성공 시 true, 실패 시 false
//***************************************************************************
bool CAdoConnPool::SetReconnectConfig(const TReconnectConfig& reconnectConfig)
{
	if( !ValidateReconnectConfig(reconnectConfig) ) return false;

	_nBackoffBaseMs.store(reconnectConfig.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(reconnectConfig.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(reconnectConfig.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(reconnectConfig.nBackoffJitterMs, std::memory_order_relaxed);

	SetWorkerCount(reconnectConfig.nWorkerCount);
	return true;
}

//***************************************************************************
// CAdoConnPool::GetReconnectConfig
// @brief 현재 적용되어 있는 재접속 및 백오프 정책 설정을 조회합니다.
// @return 현재 설정이 담긴 TReconnectConfig 구조체
//***************************************************************************
CAdoConnPool::TReconnectConfig CAdoConnPool::GetReconnectConfig(void) const
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
// CAdoConnPool::Clear
// @brief 커넥션 풀에 보유 중인 모든 커넥션 자원을 해제하고 풀을 초기화 상태로 되돌립니다.
//***************************************************************************
void CAdoConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CAdoDB*> vShutdownDeletes;

	// 풀 내부의 모든 커넥션 정리 및 해제
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CAdoDB* pConn = _pAdoConns[i].value.load(std::memory_order_acquire);
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
			LOG_ERROR(_T("Clear (ADO): Slot(%d) refcount is zombie. Moving to quarantine."), i);
			PLockGuard qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i].value, now });
		}
		else
		{
			xdelete(pConn);
		}

		_pAdoConns[i].value.store(nullptr, std::memory_order_relaxed);
	}

	// 종료 시점에 격리 큐에 남아있는 자원 정리
	{
		PLockGuard qGuard(_globalQuarantineLock);
		while( !_quarantineQueue.empty() )
		{
			TQuarantineItem item = _quarantineQueue.front();
			_quarantineQueue.pop();

			if( item.pRefCount->load(std::memory_order_acquire) == 0 )
			{
				if( item.pConn != nullptr ) vShutdownDeletes.push_back(item.pConn);
			}
			else
			{
				LOG_ERROR(_T("Clear (ADO): Abandoning leaked connection to prevent Use-After-Free crash."));
			}
		}
	}

	for( int32 i = 0; i < vShutdownDeletes.size(); ++i )
	{
		xdelete(vShutdownDeletes[i]);
		LOG_DEBUG(_T("Clear (ADO): Safely deleted stalled quarantine connection during shutdown."));
	}
}