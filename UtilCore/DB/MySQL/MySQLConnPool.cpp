
//***************************************************************************
// MySQLConnPool.cpp : implementation of the CMySQLConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLConnPool.h"

constexpr int32 WAIT_TIMEOUT_MS = 100;				// 슬롯 교체 시 기존 사용 중인 스레드의 이탈 대기 상한 시간 (ms)
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000;		// 격리 큐 정체 경고 로그 최소 출력 주기 (5분)
constexpr int64 FORCE_CLEANUP_TIMEOUT_MS = 600000;	// 격리 큐 최대 체류 허용 시간 (10분)

//***************************************************************************
// @brief CMySQLConnPool 클래스 생성자입니다.
// @param nMaxPoolSize 관리할 최대 커넥션 슬롯 개수
//***************************************************************************
CMySQLConnPool::CMySQLConnPool(int32 nMaxPoolSize)
	: _nMaxPoolSize(nMaxPoolSize)
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
	, _uiPort(0)
{
	_pMySQLConns = std::make_unique<CachePaddedAtomic<CBaseMySQL*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize);

	_pReconnecting = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		_pMySQLConns[i].value.store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].value.store(0, std::memory_order_relaxed);
		_pReconnecting[i].value.store(false, std::memory_order_relaxed);
		_pRetryFailCount[i].value.store(0, std::memory_order_relaxed);
	}

	memset(&_szDBHost[0], 0, sizeof(_szDBHost));
	memset(&_szDBUserId[0], 0, sizeof(_szDBUserId));
	memset(&_szDBPasswd[0], 0, sizeof(_szDBPasswd));
	memset(&_szDBName[0], 0, sizeof(_szDBName));
}

//***************************************************************************
// @brief CMySQLConnPool 클래스 소멸자입니다.
//***************************************************************************
CMySQLConnPool::~CMySQLConnPool(void)
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
bool CMySQLConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

//***************************************************************************
// @brief 커넥션 풀을 초기화하고 지정된 char 접속 정보로 초기 커넥션을 미리 생성합니다.
// @param pszDBHost 데이터베이스 서버 호스트 주소
// @param pszDBUserId 데이터베이스 사용자 계정 ID
// @param pszDBPasswd 데이터베이스 사용자 계정 비밀번호
// @param pszDBName 접속할 데이터베이스 이름
// @param uiPort 데이터베이스 서버 포트 번호
// @param reconnectConfig 재연결 정책 설정 구조체
// @return 초기화 및 연결 성공 시 true, 실패 시 false
//***************************************************************************
bool CMySQLConnPool::Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort, const TReconnectConfig& reconnectConfig)
{
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();

	strncpy_s(_szDBHost, DATABASE_SERVER_NAME_STRLEN, pszDBHost, _TRUNCATE);
	strncpy_s(_szDBUserId, DATABASE_DSN_USER_ID_STRLEN, pszDBUserId, _TRUNCATE);
	strncpy_s(_szDBPasswd, DATABASE_DSN_USER_PASSWORD_STRLEN, pszDBPasswd, _TRUNCATE);
	strncpy_s(_szDBName, DATABASE_NAME_STRLEN, pszDBName, _TRUNCATE);
	_uiPort = uiPort;

	return FinishInit(reconnectConfig);
}

//***************************************************************************
// @brief 커넥션 풀을 초기화하고 지정된 wchar_t 접속 정보로 초기 커넥션을 미리 생성합니다.
// @param pwszDBHost 데이터베이스 서버 호스트 주소 (Wide character 문자열)
// @param pwszDBUserId 데이터베이스 사용자 계정 ID
// @param pwszDBPasswd 데이터베이스 사용자 계정 비밀번호
// @param pwszDBName 접속할 데이터베이스 이름
// @param uiPort 데이터베이스 서버 포트 번호
// @param reconnectConfig 재연결 정책 설정 구조체
// @return 초기화 및 연결 성공 시 true, 실패 시 false
//***************************************************************************
bool CMySQLConnPool::Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort, const TReconnectConfig& reconnectConfig)
{
	StopHealthCheckThread();
	StopDelayedTaskThread();
	StopReconnectWorkers();
	Clear();

	int nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_SERVER_NAME_STRLEN < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, _szDBHost, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_ID_STRLEN < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, _szDBUserId, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_PASSWORD_STRLEN < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, _szDBPasswd, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_NAME_STRLEN < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, _szDBName, nLength, NULL, NULL) == 0 ) return false;

	_uiPort = uiPort;

	return FinishInit(reconnectConfig);
}

//***************************************************************************
// @brief 공통 초기화 마무리 로직 (커넥션 사전 생성 및 백그라운드 스레드 기동)
// @param reconnectConfig 적용할 재연결 설정 구조체
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CMySQLConnPool::FinishInit(const TReconnectConfig& reconnectConfig)
{
	TReconnectConfig cfgToApply = reconnectConfig;
	if( !ValidateReconnectConfig(reconnectConfig) )
	{
		LOG_ERROR(_T("FinishInit: invalid TReconnectConfig, falling back to default"));
		cfgToApply = TReconnectConfig{};
	}

	_nBackoffBaseMs.store(cfgToApply.nBackoffBaseMs, std::memory_order_relaxed);
	_nBackoffMaxMs.store(cfgToApply.nBackoffMaxMs, std::memory_order_relaxed);
	_nBackoffMaxShift.store(cfgToApply.nBackoffMaxShift, std::memory_order_relaxed);
	_nBackoffJitterMs.store(cfgToApply.nBackoffJitterMs, std::memory_order_relaxed);

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		CBaseMySQL* pConn = xnew<CBaseMySQL>(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( pConn == nullptr )
		{
			LOG_ERROR(_T("FinishInit: xnew<CBaseMySQL> failed (index=%d)"), i);
			Clear();
			return false;
		}

		if( !pConn->Connect() )
		{
			xdelete(pConn);
			Clear();
			return false;
		}

		_pMySQLConns[i].value.store(pConn, std::memory_order_release);
	}

	StartDelayedTaskThread();
	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

//***************************************************************************
// @brief 새로운 데이터베이스 연결 객체를 생성하고 실제 연결을 시도합니다.
// @param nType 대상 슬롯 인덱스 번호 (로깅 및 추적용)
// @return 연결에 성공한 CBaseMySQL 포인터, 실패 시 nullptr
//***************************************************************************
CBaseMySQL* CMySQLConnPool::TryReconnect(int32 nType)
{
	CBaseMySQL* pNewConn = xnew<CBaseMySQL>(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect: xnew<CBaseMySQL> failed (index=%d)"), nType);
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
// @return 유효한 CBaseMySQL 포인터, 실패 시 nullptr
//***************************************************************************
CBaseMySQL* CMySQLConnPool::GetMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetMySQLConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	_pRefCount[nType].value.fetch_add(1, std::memory_order_relaxed);
	CBaseMySQL* pMySQLConn = _pMySQLConns[nType].value.load(std::memory_order_acquire);

	if( pMySQLConn == nullptr || !pMySQLConn->IsConnected() )
	{
		_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetMySQLConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	return pMySQLConn;
}

//***************************************************************************
// @brief 참조 카운트 변경 없이 슬롯의 커넥션을 조회합니다.
// @param nType 조회할 슬롯 인덱스 번호
// @return CBaseMySQL 포인터, 유효하지 않은 경우 nullptr
//***************************************************************************
CBaseMySQL* CMySQLConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;
	return _pMySQLConns[nType].value.load(std::memory_order_acquire);
}

//***************************************************************************
// @brief 사용이 끝난 커넥션 슬롯의 참조 카운트를 감소시킵니다.
// @param nType 반환할 커넥션 슬롯 인덱스 번호
//***************************************************************************
void CMySQLConnPool::ReleaseMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;
	_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// @brief 빈 슬롯을 탐색하고 즉시 선점합니다.
// @return 선점 성공한 슬롯 인덱스 번호, 실패 시 -1
//***************************************************************************
int32 CMySQLConnPool::PopFreeSlotIndex(void) 
{
	uint32 nStart = _nNextSlotHint.fetch_add(1, std::memory_order_relaxed);

	for( int32 k = 0; k < _nMaxPoolSize; ++k ) {
		int32 i = static_cast<int32>((nStart + k) % _nMaxPoolSize);

		int32 expected = 0;
		if( _pRefCount[i].value.compare_exchange_strong(expected, 1, std::memory_order_acq_rel) ) {
			CBaseMySQL* pConn = _pMySQLConns[i].value.load(std::memory_order_acquire);
			if( pConn && pConn->IsConnected() ) return i;

			_pRefCount[i].value.store(0, std::memory_order_release);
		}
	}
	return -1;
}

//***************************************************************************
// @brief 재연결 워커가 새로 확보한 커넥션을 슬롯에 교체(Swap) 적용합니다.
// @param nType 교체 대상 슬롯 인덱스 번호
// @param pNewConn 새로 연결된 CBaseMySQL 객체 포인터
//***************************************************************************
void CMySQLConnPool::ApplyReconnectedConn(int32 nType, CBaseMySQL* pNewConn)
{
	if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	CBaseMySQL* pOldConn = nullptr;
	{
		SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[nType]);
		pOldConn = _pMySQLConns[nType].value.load(std::memory_order_acquire);
		_pMySQLConns[nType].value.store(pNewConn, std::memory_order_release);
	}

	if( pOldConn == nullptr ) return;

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

		auto now = std::chrono::steady_clock::now();
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType].value, now });
	}
	else
	{
		xdelete(pOldConn);
	}
}

//***************************************************************************
// @brief 재연결 실패 시 지수 백오프 및 지터를 계산하여 재시도를 예약합니다.
// @param nType 재시도가 실패한 슬롯 인덱스 번호
//***************************************************************************
void CMySQLConnPool::ScheduleRetry(int32 nType)
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

	_delayedTaskQueue.Reserve(static_cast<int>(nDelayMs), [this, nType]() {
		CBaseMySQL* pCur = _pMySQLConns[nType].value.load(std::memory_order_acquire);
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
void CMySQLConnPool::OnReconnectFailed(int32 nType)
{
	ScheduleRetry(nType);
}

//***************************************************************************
// @brief 재연결 성공 시 백오프 실패 카운트를 초기화합니다.
// @param nType 성공한 슬롯 인덱스 번호
//***************************************************************************
void CMySQLConnPool::OnReconnectSucceeded(int32 nType)
{
	_pRetryFailCount[nType].value.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// @brief 격리 큐 정리 및 끊긴 슬롯을 주기적으로 스캔하여 재연결 워커에 디스패치합니다.
//***************************************************************************
void CMySQLConnPool::HealthCheckLoop(void)
{
	CVector<CBaseMySQL*> vDeletes;

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

				auto now = std::chrono::steady_clock::now();
				auto elapsedTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - item.lastLogTime).count();

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

			CBaseMySQL* pCur = _pMySQLConns[i].value.load(std::memory_order_acquire);
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
void CMySQLConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

//***************************************************************************
// @brief 헬스체크 스레드를 안전하게 종료하고 대기합니다.
//***************************************************************************
void CMySQLConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads();
}

//***************************************************************************
// @brief CDelayedTaskQueue의 만료 작업 처리 루프 함수입니다.
//***************************************************************************
void CMySQLConnPool::DelayedTaskLoop(void)
{
	_delayedTaskQueue.ProcessExpiredTasks();
}

//***************************************************************************
// @brief 지연 타이머 큐 전용 스레드를 생성하고 구동을 시작합니다.
//***************************************************************************
void CMySQLConnPool::StartDelayedTaskThread(void)
{
	_delayedTaskThreadMgr.CreateThread([this]() { DelayedTaskLoop(); });
}

//***************************************************************************
// @brief 지연 타이머 큐 스레드를 안전하게 중지하고 종료를 대기합니다.
//***************************************************************************
void CMySQLConnPool::StopDelayedTaskThread(void)
{
	_delayedTaskQueue.Stop();
	_delayedTaskThreadMgr.JoinThreads();
}

//***************************************************************************
// @brief 재연결이 필요한 슬롯 인덱스를 대기열에 넣고 워커 스레드를 깨웁니다.
// @param nType 재연결 대상 슬롯 인덱스 번호
//***************************************************************************
void CMySQLConnPool::EnqueueReconnect(int32 nType)
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
bool CMySQLConnPool::TryExitIfExcess(void)
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
void CMySQLConnPool::ReconnectWorkerLoop(void)
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

		CBaseMySQL* pNewConn = TryReconnect(nType);
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
void CMySQLConnPool::StartReconnectWorkers(int32 nWorkerCount)
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
void CMySQLConnPool::StopReconnectWorkers(void)
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
void CMySQLConnPool::SetWorkerCount(int32 nNewCount)
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
bool CMySQLConnPool::SetReconnectConfig(const TReconnectConfig& reconnectConfig)
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
CMySQLConnPool::TReconnectConfig CMySQLConnPool::GetReconnectConfig(void) const
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
void CMySQLConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CBaseMySQL*> vShutdownDeletes;

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseMySQL* pConn = _pMySQLConns[i].value.load(std::memory_order_acquire);
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

			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i].value, now });
		}
		else
		{
			xdelete(pConn);
		}

		_pMySQLConns[i].value.store(nullptr, std::memory_order_relaxed);
	}

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