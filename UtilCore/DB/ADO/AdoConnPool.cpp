
//***************************************************************************
// AdoConnPool.cpp : implementation of the CAdoConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "AdoConnPool.h"

constexpr int32 WAIT_TIMEOUT_MS = 100;
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000;

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
	_pAdoConns = std::make_unique<CachePaddedAtomic<CAdoDB*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize);

	_pReconnecting = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);
	_pNextRetryAllowedMs = std::make_unique<CachePaddedAtomic<int64>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pAdoConns[i].value.store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].value.store(0, std::memory_order_relaxed);
		_pReconnecting[i].value.store(false, std::memory_order_relaxed);
		_pNextRetryAllowedMs[i].value.store(0, std::memory_order_relaxed);
		_pRetryFailCount[i].value.store(0, std::memory_order_relaxed);
	}
	memset(&_tszConnStr[0], 0, sizeof(_tszConnStr));
}

CAdoConnPool::~CAdoConnPool(void)
{
	StopHealthCheckThread();
	StopReconnectWorkers();
	Clear();
}

bool CAdoConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

bool CAdoConnPool::Init(const EDBClass dbClass, const TCHAR* ptszConnStr, const int nTimeOut,
	const TReconnectConfig& reconnectConfig)
{
	StopHealthCheckThread();
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

	_nBackoffBaseMs.store(cfgToApply.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(cfgToApply.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(cfgToApply.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(cfgToApply.nBackoffJitterMs, std::memory_order_relaxed);

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

	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

CAdoDB* CAdoConnPool::TryReconnect(int32 nType)
{
	CAdoDB* pNewConn = xnew<CAdoDB>();
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect (ADO): xnew<CAdoDB> failed (index=%d)"), nType);
		return nullptr;
	}

	if( pNewConn->Connect(_dbClass, _tszConnStr, _nTimeOut) < 0 )
	{
		xdelete(pNewConn);
		return nullptr;
	}

	return pNewConn;
}

CAdoDB* CAdoConnPool::GetAdoConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetAdoConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	_pRefCount[nType].value.fetch_add(1, std::memory_order_relaxed);
	CAdoDB* pAdoConn = _pAdoConns[nType].value.load(std::memory_order_acquire);

	if( pAdoConn == nullptr || !pAdoConn->GetDBCon() )
	{
		_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
		return nullptr;
	}

	return pAdoConn;
}

CAdoDB* CAdoConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;
	return _pAdoConns[nType].value.load(std::memory_order_acquire);
}

void CAdoConnPool::ReleaseAdoConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;
	_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
}

int32 CAdoConnPool::PopFreeSlotIndex(void) {
	uint32 nStart = _nNextSlotHint.fetch_add(1, std::memory_order_relaxed);

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

void CAdoConnPool::ApplyReconnectedConn(int32 nType, CAdoDB* pNewConn)
{
	if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	CAdoDB* pOldConn = nullptr;
	{
		SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[nType]);
		pOldConn = _pAdoConns[nType].value.load(std::memory_order_acquire);
		_pAdoConns[nType].value.store(pNewConn, std::memory_order_release);
	}

	if( pOldConn == nullptr ) return;

	auto startTime = std::chrono::steady_clock::now();
	bool bTimeout = false;

	while( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
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
		LOG_ERROR(_T("ReconnectWorker (ADO): Slot(%d) refcount high during swap. Moving to quarantine."), nType);
		auto now = std::chrono::steady_clock::now();
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType].value, now });
	}
	else
	{
		xdelete(pOldConn);
	}
}

int64 CAdoConnPool::NowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool CAdoConnPool::IsRetryAllowed(int32 nType) const
{
	int64 nNextAllowed = _pNextRetryAllowedMs[nType].value.load(std::memory_order_acquire);
	return NowMs() >= nNextAllowed;
}

void CAdoConnPool::OnReconnectFailed(int32 nType)
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

	_pNextRetryAllowedMs[nType].value.store(NowMs() + nDelayMs, std::memory_order_release);
}

void CAdoConnPool::OnReconnectSucceeded(int32 nType)
{
	_pRetryFailCount[nType].value.store(0, std::memory_order_relaxed);
	_pNextRetryAllowedMs[nType].value.store(0, std::memory_order_release);
}

void CAdoConnPool::HealthCheckLoop(void)
{
	CVector<CAdoDB*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		vDeletes.clear();

		{
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size();

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				if( item.pRefCount->load(std::memory_order_acquire) == 0 )
				{
					if( item.pConn != nullptr ) vDeletes.push_back(item.pConn);
				}
				else
				{
					auto now = std::chrono::steady_clock::now();
					auto elapsedFromLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - item.lastLogTime).count();

					if( elapsedFromLastLog >= LOG_ALERT_INTERVAL_MS )
					{
						LOG_ERROR(_T("Quarantine Persistent Warning (ADO): Connection stuck!"));
						item.lastLogTime = now;
					}
					_quarantineQueue.push(item);
				}
			}
		}

		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
		}

		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			if( _pRefCount[i].value.load(std::memory_order_acquire) > 0 ) continue;

			CAdoDB* pCur = _pAdoConns[i].value.load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->GetDBCon() ) continue;

			if( !IsRetryAllowed(i) ) continue;

			bool bExpected = false;
			if( !_pReconnecting[i].value.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel) )
				continue;

			EnqueueReconnect(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_nHealthCheckIntervalMs));
	}
}

void CAdoConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

void CAdoConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads();
}

void CAdoConnPool::EnqueueReconnect(int32 nType)
{
	{
		std::lock_guard<std::mutex> lock(_reconnectQueueMutex);
		_reconnectPendingSlots.push(nType);
	}
	_reconnectQueueCv.notify_one();
}

bool CAdoConnPool::TryExitIfExcess(void)
{
	int32 nCur = _nCurrentWorkerCount.load(std::memory_order_acquire);
	while( nCur > _nDesiredWorkerCount.load(std::memory_order_acquire) )
	{
		if( _nCurrentWorkerCount.compare_exchange_weak(nCur, nCur - 1, std::memory_order_acq_rel) )
			return true;
	}
	return false;
}

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

		CAdoDB* pNewConn = TryReconnect(nType);
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

void CAdoConnPool::StopReconnectWorkers(void)
{
	_bStopReconnectWorkers.store(true, std::memory_order_relaxed);
	_reconnectQueueCv.notify_all();
	_reconnectWorkerMgr.JoinThreads();
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);
	_nDesiredWorkerCount.store(0, std::memory_order_relaxed);
}

void CAdoConnPool::SetWorkerCount(int32 nNewCount)
{
	nNewCount = std::max(nNewCount, 1);
	_nDesiredWorkerCount.store(nNewCount, std::memory_order_release);

	int32 nExpected = _nCurrentWorkerCount.load(std::memory_order_acquire);
	if( nExpected < nNewCount )
	{
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
		}
	}
	else
	{
		_reconnectQueueCv.notify_all();
	}
}

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

void CAdoConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CAdoDB*> vShutdownDeletes;

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CAdoDB* pConn = _pAdoConns[i].value.load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;

		while( _pRefCount[i].value.load(std::memory_order_acquire) > 0 )
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
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i].value, now });
		}
		else
		{
			xdelete(pConn);
		}

		_pAdoConns[i].value.store(nullptr, std::memory_order_relaxed);
	}

	{
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		while( !_quarantineQueue.empty() )
		{
			TQuarantineItem item = _quarantineQueue.front();
			_quarantineQueue.pop();

			if( item.pRefCount->load(std::memory_order_acquire) == 0 )
			{
				if( item.pConn != nullptr ) vShutdownDeletes.push_back(item.pConn);
			}
		}
	}

	for( int32 i = 0; i < vShutdownDeletes.size(); ++i )
	{
		xdelete(vShutdownDeletes[i]);
	}
}