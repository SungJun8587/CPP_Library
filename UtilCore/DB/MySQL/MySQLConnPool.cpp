
//***************************************************************************
// MySQLConnPool.cpp : implementation of the CMySQLConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLConnPool.h"

// 안전 장치 및 모니터링 타임아웃 상수 설정
constexpr int32 WAIT_TIMEOUT_MS = 100;
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000; // 좀비 지속 시 5분마다 경고

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CMySQLConnPool::CMySQLConnPool(int32 nMaxPoolSize)
	: _nMaxPoolSize(nMaxPoolSize)
	, _bStopHealthCheck(false)
	, _nHealthCheckIntervalMs(500)
{
	// unique_ptr 배열로 생성자에서 단 1회만 고정 할당 (주소 무효화 방지)
	_pMySQLConns = std::make_unique<std::atomic<CBaseMySQL*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<std::atomic<int32>[]>(_nMaxPoolSize);
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize);

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		_pMySQLConns[i].store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].store(0, std::memory_order_relaxed);
	}

	memset(&_szDBHost[0], 0, DATABASE_SERVER_NAME_STRLEN);
	memset(&_szDBUserId[0], 0, DATABASE_DSN_USER_ID_STRLEN);
	memset(&_szDBPasswd[0], 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	memset(&_szDBName[0], 0, DATABASE_NAME_STRLEN);
	_uiPort = 0;
}

CMySQLConnPool::~CMySQLConnPool(void)
{
	StopHealthCheckThread();
	Clear();
}

//***************************************************************************
// 초기화 (순차 처리 구간)
//***************************************************************************
bool CMySQLConnPool::Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort)
{
	StopHealthCheckThread();
	Clear();

	strncpy_s(_szDBHost, DATABASE_SERVER_NAME_STRLEN, pszDBHost, _TRUNCATE);
	strncpy_s(_szDBUserId, DATABASE_DSN_USER_ID_STRLEN, pszDBUserId, _TRUNCATE);
	strncpy_s(_szDBPasswd, DATABASE_DSN_USER_PASSWORD_STRLEN, pszDBPasswd, _TRUNCATE);
	strncpy_s(_szDBName, DATABASE_NAME_STRLEN, pszDBName, _TRUNCATE);
	_uiPort = uiPort;

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		CBaseMySQL* pConn = xnew<CBaseMySQL>(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( pConn == nullptr )
		{
			LOG_ERROR(_T("Init: xnew<CBaseMySQL> failed (index=%d)"), i);
			Clear();
			return false;
		}

		if( !pConn->Connect() )
		{
			xdelete(pConn);
			Clear();
			return false;
		}

		_pMySQLConns[i].store(pConn, std::memory_order_release);
	}

	StartHealthCheckThread();
	return true;
}

bool CMySQLConnPool::Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort)
{
	StopHealthCheckThread();
	Clear();

	int nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_SERVER_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, _szDBHost, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_ID_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, _szDBUserId, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_PASSWORD_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, _szDBPasswd, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, _szDBName, nLength, NULL, NULL) == 0 ) return false;

	_uiPort = uiPort;

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		CBaseMySQL* pConn = xnew<CBaseMySQL>(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( pConn == nullptr )
		{
			LOG_ERROR(_T("Init: xnew<CBaseMySQL> failed (index=%d)"), i);
			Clear();
			return false;
		}

		if( !pConn->Connect() )
		{
			xdelete(pConn);
			Clear();
			return false;
		}

		_pMySQLConns[i].store(pConn, std::memory_order_release);
	}

	StartHealthCheckThread();
	return true;
}

//***************************************************************************
// 무거운 커넥션 맺기 시도 (락-밖 영역에서 단독 실행)
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
// 다중 스레드 안전 획득 (원자적 참조 카운트 메커니즘)
//***************************************************************************
CBaseMySQL* CMySQLConnPool::GetMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetMySQLConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	// 1. 참조 카운트를 증가시켜 백그라운드의 자원 해제 개입을 차단 (선점)
	_pRefCount[nType].fetch_add(1, std::memory_order_relaxed);

	// 2. 가시성이 확보된 포인터를 안전하게 로드
	CBaseMySQL* pMySQLConn = _pMySQLConns[nType].load(std::memory_order_acquire);

	// 3. 커넥션 준비 상태 검증
	if( pMySQLConn == nullptr || !pMySQLConn->IsConnected() )
	{
		// 사용 불가 상태라면 카운트를 환원하고 nullptr 반환 (백그라운드 스레드가 리커넥트 처리)
		_pRefCount[nType].fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetMySQLConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	return pMySQLConn;
}

//***************************************************************************
// 자원 반환
//***************************************************************************
void CMySQLConnPool::ReleaseMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;
	_pRefCount[nType].fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// 백그라운드 헬스체크 루프 (지연 격리 및 락-밖 소멸 적용)
//***************************************************************************
void CMySQLConnPool::HealthCheckLoop(void)
{
	CVector<CBaseMySQL*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		// CVector가 소문자 API를 사용하는 규격에 맞춰 clear() 호출
		vDeletes.clear();

		// -----------------------------------------------------------------
		// PART A: 격리 큐 청소 (락 안에서는 원자적 판별 및 추출만 수행)
		// -----------------------------------------------------------------
		{
			// 신규 고성능 SpinLockGuard 적용
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size();

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				// 참조 카운트가 완전히 0이 된 정상 컨텍스트만 삭제 대상으로 분류
				if( item.pRefCount->load(std::memory_order_acquire) == 0 )
				{
					if( item.pConn != nullptr )
					{
						vDeletes.push_back(item.pConn);
					}
				}
				else
				{
					// 아직 사용 중인 좀비라면 5분 주기로 시스템 경고 로그 출력
					auto now = std::chrono::steady_clock::now();
					auto elapsedFromLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - item.lastLogTime).count();

					if( elapsedFromLastLog >= LOG_ALERT_INTERVAL_MS )
					{
						LOG_ERROR(_T("Quarantine Persistent Warning: Connection is still stuck in quarantine! Potential leak in application logic."));
						item.lastLogTime = now;
					}

					_quarantineQueue.push(item); // 큐에 다시 보류
				}
			}
		} // <-- qGuard 소멸하며 글로벌 스핀락 즉시 해제

		// -----------------------------------------------------------------
		// PART A-1: 무거운 I/O를 동반한 실제 소멸 작업 (락 밖에서 병목 격리 실행)
		// -----------------------------------------------------------------
		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine: Safely deleted stalled connection outside the lock."));
		}

		// -----------------------------------------------------------------
		// PART B: 정기 커넥션 끊김 감지 및 교체
		// -----------------------------------------------------------------
		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;

			CBaseMySQL* pCur = _pMySQLConns[i].load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->IsConnected() ) continue;

			if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;

			CBaseMySQL* pNewConn = TryReconnect(i);
			if( pNewConn == nullptr ) continue;

			if( _pRefCount[i].load(std::memory_order_acquire) > 0 )
			{
				xdelete(pNewConn);
				continue;
			}

			CBaseMySQL* pOldConn = nullptr;
			{
				SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[i]);
				pOldConn = _pMySQLConns[i].load(std::memory_order_acquire);
				_pMySQLConns[i].store(pNewConn, std::memory_order_release);
			}

			if( pOldConn != nullptr )
			{
				auto startTime = std::chrono::steady_clock::now();
				bool bTimeout = false;

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
					LOG_ERROR(_T("HealthCheck: Slot(%d) refcount high during swap. Moving to quarantine."), i);

					auto now = std::chrono::steady_clock::now();
					SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
					_quarantineQueue.push({ pOldConn, &_pRefCount[i], now });
				}
				else
				{
					xdelete(pOldConn);
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(_nHealthCheckIntervalMs));
	}
}

void CMySQLConnPool::StartHealthCheckThread(void)
{
	_bStopHealthCheck.store(false, std::memory_order_relaxed);
	_healthCheckThreadMgr.CreateThread([this]() { HealthCheckLoop(); });
}

void CMySQLConnPool::StopHealthCheckThread(void)
{
	_bStopHealthCheck.store(true, std::memory_order_relaxed);
	_healthCheckThreadMgr.JoinThreads();
}

//***************************************************************************
// 자원 전면 청소 및 프로세스 셧다운 (컴파일러 이슈 및 락-밖 소멸 통일 완수)
//***************************************************************************
void CMySQLConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CBaseMySQL*> vShutdownDeletes;

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseMySQL* pConn = _pMySQLConns[i].load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;

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
			xdelete(pConn);
		}

		_pMySQLConns[i].store(nullptr, std::memory_order_relaxed);
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