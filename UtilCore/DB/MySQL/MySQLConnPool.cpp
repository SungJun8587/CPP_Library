
//***************************************************************************
// MySQLConnPool.cpp : implementation of the CMySQLConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLConnPool.h"
#include <random>
#include <algorithm>

// 연결 교체 및 모니터링 타임아웃 상수 정의
constexpr int32 WAIT_TIMEOUT_MS = 100;          // 슬롯 커넥션 교체 시, 사용 중인 스레드가 이탈할 때까지 대기하는 최대 임계치 (100ms)
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000; // 격리 큐 상태 요약 경고 로그를 남기는 최소 주기 (5분). 개별 항목이 아니라
// 큐 전체에 대해 한 번만 찍으므로, 격리 항목이 몇 개든 로그량은 늘지 않는다.

//***************************************************************************
// Construction/Destruction
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
	, _quarantineLastSummaryLogTime(std::chrono::steady_clock::time_point::min())
{
	// unique_ptr 배열은 생성자에서 딱 1회만 원자 할당 (주소 유효화 및 메모리 주소 흔들림 방지)
	_pMySQLConns = std::make_unique<CachePaddedAtomic<CBaseMySQL*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);

	// CPU 캐시 라인 충돌(False Sharing)을 방지하도록 슬롯 단위로 정렬된 SpinLockDefault를 배열 형태로 생성한다.
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize);

	// 슬롯별 재연결 진행 여부 플래그 및 백오프 상태 배열
	_pReconnecting = std::make_unique<CachePaddedAtomic<bool>[]>(_nMaxPoolSize);
	_pNextRetryAllowedMs = std::make_unique<CachePaddedAtomic<int64>[]>(_nMaxPoolSize);
	_pRetryFailCount = std::make_unique<CachePaddedAtomic<int32>[]>(_nMaxPoolSize);

	// 초기 배열 값을 기본값 세팅
	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		_pMySQLConns[i].value.store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].value.store(0, std::memory_order_relaxed);
		_pReconnecting[i].value.store(false, std::memory_order_relaxed);
		_pNextRetryAllowedMs[i].value.store(0, std::memory_order_relaxed);
		_pRetryFailCount[i].value.store(0, std::memory_order_relaxed);
	}

	memset(&_szDBHost[0], 0, DATABASE_SERVER_NAME_STRLEN);
	memset(&_szDBUserId[0], 0, DATABASE_DSN_USER_ID_STRLEN);
	memset(&_szDBPasswd[0], 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	memset(&_szDBName[0], 0, DATABASE_NAME_STRLEN);
	_uiPort = 0;
}

CMySQLConnPool::~CMySQLConnPool(void)
{
	// 1. 소멸자 호출 시 감시/재연결 스레드들을 먼저 종료하고 레이스컨디션 발생 소지 방지
	StopHealthCheckThread();
	StopReconnectWorkers();
	// 2. 남아있는 모든 커넥션 자원을 순서 지켜 안전하게 정리
	Clear();
}

//***************************************************************************
// TReconnectConfig 값이 상식적인 범위 안에 있는지 검사한다. (Init / SetReconnectConfig 공용)
//***************************************************************************
bool CMySQLConnPool::ValidateReconnectConfig(const TReconnectConfig& cfg)
{
	if( cfg.nWorkerCount < 1 ) return false;
	// 너무 짧은 base는 DB가 계속 죽어있는 상황에서 재연결 시도가 사실상 무한루프처럼 돌며
	// CPU 스파이크와 재연결 폭주로 이어질 수 있어 최소 하한을 강제한다.
	if( cfg.nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS ) return false;
	if( cfg.nBackoffMaxMs < cfg.nBackoffBaseMs ) return false;
	// shift가 너무 크면 (BaseMs << shift) 연산에서 int64 오버플로로 음수 지연시간이 나올 수 있어 상한을 둔다.
	if( cfg.nBackoffMaxShift < 0 || cfg.nBackoffMaxShift > 30 ) return false;
	if( cfg.nBackoffJitterMs < 0 ) return false;
	return true;
}

//***************************************************************************
// 초기화 (char* 접속 정보)
//***************************************************************************
bool CMySQLConnPool::Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd,
	const char* pszDBName, const uint32 uiPort, const TReconnectConfig& reconnectConfig)
{
	// 혹시 재초기화 시나리오일 경우를 대비해 백그라운드 스레드들을 정리하고 풀 청소 선행
	StopHealthCheckThread();
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
// 초기화 (wchar_t* 접속 정보 — 내부 멀티바이트 버퍼로 변환 후 char* 경로와 동일하게 마무리)
//***************************************************************************
bool CMySQLConnPool::Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd,
	const wchar_t* pwszDBName, const uint32 uiPort, const TReconnectConfig& reconnectConfig)
{
	StopHealthCheckThread();
	StopReconnectWorkers();
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

	return FinishInit(reconnectConfig);
}

//***************************************************************************
// _szDBHost 등 접속 정보가 이미 채워진 뒤 공통으로 실행되는 초기화 마무리 로직.
// 유효성 검사, 재연결 정책 적용, 커넥션 사전 생성, 감시/재연결 스레드 기동을 담당한다.
//***************************************************************************
bool CMySQLConnPool::FinishInit(const TReconnectConfig& reconnectConfig)
{
	// 유효하지 않은 설정값이 들어오면 조용히 부분 적용하지 않고, 통째로 기본값으로 대체한다.
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

	// 지정된 최대 크기만큼 커넥션을 미리 배정(풀)에 채운다 (초기화 시점은 미리 준비하는 단계이므로 동기 할당)
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

		// 다른 스레드가 즉시 커넥션을 볼 수 있도록 release 세맨틱으로 메모리에 게시
		_pMySQLConns[i].value.store(pConn, std::memory_order_release);
	}

	// 정상 셋업 후 감시/재연결 담당 백그라운드 스레드들을 시작
	StartHealthCheckThread();
	StartReconnectWorkers(cfgToApply.nWorkerCount);
	return true;
}

//***************************************************************************
// 새로운 커넥션 신규 시도 (재-연결 순수한 단독 로직, 블로킹 I/O 포함)
//***************************************************************************
CBaseMySQL* CMySQLConnPool::TryReconnect(int32 nType)
{
	// 대기 시간 및 경쟁 조건을 최소화하기 위해, 기존 커넥션을 건드리지 않고 별도로 새로 만들어
	// '연결 작업' 자체는 슬롯 획득/락과 완전히 분리된 상태에서 진행한다.
	CBaseMySQL* pNewConn = xnew<CBaseMySQL>(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect: xnew<CBaseMySQL> failed (index=%d)"), nType);
		return nullptr;
	}

	// 네트워크 I/O 지연이 걸릴 수 있는 실제 연결 단계
	if( !pNewConn->Connect() )
	{
		xdelete(pNewConn);
		return nullptr;
	}

	return pNewConn;
}

//***************************************************************************
// 지정 슬롯의 참조 획득 (선점된 참조 카운트 대칭구조)
//***************************************************************************
CBaseMySQL* CMySQLConnPool::GetMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetMySQLConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	// -------------------------------------------------------------------------
	// [핵심 동시성 방어 메커니즘 1단계: 선점(Preemption)]
	// 참조 카운트를 낙관적으로 먼저 증가시켜서, 백그라운드 재연결 워커가
	// 해당 슬롯의 커넥션 포인터를 삭제하거나 메모리를 재할당 없이 안전하게 유지되도록 한다.
	// -------------------------------------------------------------------------
	_pRefCount[nType].value.fetch_add(1, std::memory_order_relaxed);

	// 2. 동시성(Happens-Before)이 보장된 이후 최신 슬롯의 포인터를 acquire 세맨틱으로 안전하게 로드
	CBaseMySQL* pMySQLConn = _pMySQLConns[nType].value.load(std::memory_order_acquire);

	// 3. 커넥션이 없거나 DB 연결이 끊긴 무효 상태인지 검사
	if( pMySQLConn == nullptr || !pMySQLConn->IsConnected() )
	{
		// 유효하지 않은 커넥션이라면 선점했던 카운트를 원래대로 해제(Release)한 뒤, nullptr 반환
		// (다음번 감시 시 헬스체크 스레드가 감지하여 재연결 워커에 위임하게 됨)
		_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetMySQLConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	// 정상적으로 살아서 유효있는 커넥션을 반환 (호출자는 사용 후에 반드시 해제 필요)
	return pMySQLConn;
}

//***************************************************************************
// PopFreeSlotIndex 이후에 사용되는 refcount-free 조회 (MySQLConnGuard 전용)
//***************************************************************************
CBaseMySQL* CMySQLConnPool::GetPooledConnUnsafe(int32 nType) const
{
	if( !IsValidIndex(nType) ) return nullptr;

	// PopFreeSlotIndex가 이미 참조 카운트를 확보하고 유효성도 확인한 상태이므로,
	// 여기서는 카운트를 건드리지 않고 포인터만 안전하게 읽어온다.
	return _pMySQLConns[nType].value.load(std::memory_order_acquire);
}

//***************************************************************************
// 자원 반환
//***************************************************************************
void CMySQLConnPool::ReleaseMySQLConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;

	// 사용중이던 슬롯을 반환한다. 카운터가 감소하여 0이 되는 순간,
	// 헬스체크/재연결 워커가 안전하게 감지하고 재활용할 수 있는 상태가 된다.
	_pRefCount[nType].value.fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// 참조 없이 비어있는 슬롯을 검색해 즉시 선점하고 인덱스를 반환한다. (실패시 -1)
// 시작 위치(_nNextSlotHint)를 회전시켜서 특정 슬롯에 경합이 몰리는 현상을 완화한다.
//***************************************************************************
int32 CMySQLConnPool::PopFreeSlotIndex(void) {
	uint32 nStart = _nNextSlotHint.fetch_add(1, std::memory_order_relaxed);

	for( int32 k = 0; k < _nMaxPoolSize; ++k ) {
		int32 i = static_cast<int32>((nStart + k) % _nMaxPoolSize);

		int32 expected = 0;
		// 빈 슬롯 선점을 위한 시도
		if( _pRefCount[i].value.compare_exchange_strong(expected, 1, std::memory_order_acq_rel) ) {
			CBaseMySQL* pConn = _pMySQLConns[i].value.load(std::memory_order_acquire);
			if( pConn && pConn->IsConnected() ) return i;

			// 잘못 선점한 슬롯 되돌림
			_pRefCount[i].value.store(0, std::memory_order_release);
		}
	}
	return -1;
}

//***************************************************************************
// 재연결 워커가 새 커넥션을 확보한 뒤 호출하는 슬롯 교체 마무리 로직
//***************************************************************************
void CMySQLConnPool::ApplyReconnectedConn(int32 nType, CBaseMySQL* pNewConn)
{
	// 재연결 I/O가 진행되는 동안 슬롯이 그 사이 다시 사용 중으로 바뀌었다면 새로 만든 자원은 폐기
	if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
	{
		xdelete(pNewConn);
		return;
	}

	// 슬롯 배치 확정 완료 후, 자체 슬롯 스핀락 잠금 상태 교체(Swap) 진행
	CBaseMySQL* pOldConn = nullptr;
	{
		SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[nType]);
		pOldConn = _pMySQLConns[nType].value.load(std::memory_order_acquire);
		_pMySQLConns[nType].value.store(pNewConn, std::memory_order_release); // 새 커넥션 게시 완료
	}

	if( pOldConn == nullptr ) return;

	// 교체 직후, 낡은 자원을 참조하고 있던 짧은 순간에 진입한 스레드가 빠져나갈때까지 잠시 시간 여유 확인
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
		std::this_thread::yield(); // 다른 스레드에 CPU 자원을 양보하며 잠시 대기
	}

	if( bTimeout )
	{
		// [최종 안전 장치: 격리 미션]
		// 100ms를 초과해도 여전히 반환하지 않는 극히 드문 스레드가 있다면,
		// 이를 무리해 지우면 UAF(Use-After-Free) 크래시로 이어질 대신 격리 큐로 편입하고 발송한다.
		LOG_ERROR(_T("ReconnectWorker: Slot(%d) refcount high during swap. Moving to quarantine."), nType);

		auto now = std::chrono::steady_clock::now();
		SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
		_quarantineQueue.push({ pOldConn, &_pRefCount[nType].value, now });
	}
	else
	{
		// 아무도 참조하지 않음이 명확하게 확인된 낡은 커넥션을 즉시 삭제
		xdelete(pOldConn);
	}
}

//***************************************************************************
// steady_clock 기준 현재 시각을 밀리초 정수로 변환 (백오프 시각 비교용)
//***************************************************************************
int64 CMySQLConnPool::NowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

//***************************************************************************
// 지정 슬롯이 지금 시점에 재연결을 시도해도 되는지 검사한다. (백오프 대기 중이면 false)
//***************************************************************************
bool CMySQLConnPool::IsRetryAllowed(int32 nType) const
{
	int64 nNextAllowed = _pNextRetryAllowedMs[nType].value.load(std::memory_order_acquire);
	return NowMs() >= nNextAllowed;
}

//***************************************************************************
// 재연결 실패 처리: 연속 실패 횟수를 늘리고, 다음 허용 시각을 백오프 원자 변수들 기준으로
// 지수적으로(+지터) 뒤로 미룬다.
// DB가 완전히 다운된 상황에서 모든 슬롯이 매 헬스체크 주기(500ms)마다 동시에
// 재연결을 두드려 DB에 재연결 폭주(connection storm)를 일으키는 것을 방지하기 위함이다.
//***************************************************************************
void CMySQLConnPool::OnReconnectFailed(int32 nType)
{
	int32 nFailCount = _pRetryFailCount[nType].value.fetch_add(1, std::memory_order_acq_rel) + 1;

	int32 nMaxShift = _nBackoffMaxShift.load(std::memory_order_relaxed);
	int32 nShift = std::min(nFailCount, nMaxShift);

	int64 nBaseMs = _nBackoffBaseMs.load(std::memory_order_relaxed);
	int64 nMaxMs = _nBackoffMaxMs.load(std::memory_order_relaxed);

	int64 nDelayMs = nBaseMs << nShift; // BASE * 2^nShift
	nDelayMs = std::min<int64>(nDelayMs, nMaxMs);

	// 슬롯마다 재시도 타이밍이 정확히 겹치지 않도록 무작위 지터를 더한다.
	thread_local std::mt19937 rng(std::random_device{}());
	int32 nJitterMax = _nBackoffJitterMs.load(std::memory_order_relaxed);
	std::uniform_int_distribution<int32> jitterDist(0, std::max(nJitterMax, 0));
	nDelayMs += jitterDist(rng);

	_pNextRetryAllowedMs[nType].value.store(NowMs() + nDelayMs, std::memory_order_release);

	LOG_DEBUG(_T("OnReconnectFailed: slot(%d) failCount(%d), next retry in %lldms"),
		nType, nFailCount, static_cast<long long>(nDelayMs));
}

//***************************************************************************
// 재연결 성공 처리: 백오프 상태를 완전히 초기화하여 다음 장애 발생 시 처음부터(base) 재시작하게 한다.
//***************************************************************************
void CMySQLConnPool::OnReconnectSucceeded(int32 nType)
{
	_pRetryFailCount[nType].value.store(0, std::memory_order_relaxed);
	_pNextRetryAllowedMs[nType].value.store(0, std::memory_order_release);
}

//***************************************************************************
// 백그라운드 헬스체크 루프
// (격리 큐 청소 + 죽은 슬롯 빠른 스캔/디스패치만 담당. 실제 재연결 I/O는 워커 풀에 위임하여 블로킹하지 않는다)
//***************************************************************************
void CMySQLConnPool::HealthCheckLoop(void)
{
	// 헬스체크 주기마다 매번 새로 메모리를 할당/해제하는 비용을 없애기 위해
	// 루프 진입 전 딱 한 번 생성하여 CVector 내부 캐퍼시티(Capacity)를 지속적으로 재활용한다.
	CVector<CBaseMySQL*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		// 이전 회 순회 시의 잔여 데이터만 삭제하고 재사용
		vDeletes.clear();

		// -----------------------------------------------------------------
		// PART A: 격리 큐 청소 (락 안에서는 삭제 판별 및 분류만 신속하게 진행)
		// -----------------------------------------------------------------
		{
			// 전역 격리 큐 처리를 위한 전역 동기화 락을 확보한다.
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size(); // 새로운 요소까지 도는 것을 막기 위해 시작 크기를 명시적으로 확정

			// 이번 사이클이 끝난 뒤에도 여전히 갇혀있는 항목이 있다면 요약 로그를 위해 집계한다.
			// (개별 항목마다 로그를 남기지 않고, 사이클당 최대 한 줄로 요약한다)
			int32 nStillQuarantinedCount = 0;
			std::chrono::steady_clock::time_point oldestEnqueueTime{};
			bool bHasOldest = false;

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				// [실제 격리 해제 검사]
				// 해당 슬롯 관련의 참조 카운터가 마지막 순간에 0(이용 스레드 완전 이탈)이 되었는지 재차 확인
				if( item.pRefCount->load(std::memory_order_acquire) == 0 )
				{
					if( item.pConn != nullptr )
					{
						// 실시간 삭제 대신 리스트에 포인터를 적재
						vDeletes.push_back(item.pConn);
					}
				}
				else
				{
					// 여전히 이용 중인 참조가 있는 스레드가 존재한다면, 아직 해제가 불가하므로
					// 격리 큐에 재삽입하여 다음번 순회 시 재검사하도록 유지한다.
					++nStillQuarantinedCount;
					if( !bHasOldest || item.enqueueTime < oldestEnqueueTime )
					{
						oldestEnqueueTime = item.enqueueTime;
						bHasOldest = true;
					}
					_quarantineQueue.push(item);
				}
			}

			// [요약 경보 메커니즘]
			// 갇혀있는 항목이 몇 개든 사이클당 최대 한 줄로만 로그를 남긴다. 타임아웃 주기를
			// 체크하여 정확히 5분마다 걸러서 반복 로그 남발을 방지하되, 항목 수와 가장 오래
			// 갇힌 시간을 함께 남겨 개별 항목 단위 로그보다 오히려 더 실질적인 정보를 준다.
			if( nStillQuarantinedCount > 0 )
			{
				auto now = std::chrono::steady_clock::now();
				auto elapsedFromLastSummary = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - _quarantineLastSummaryLogTime).count();

				if( elapsedFromLastSummary >= LOG_ALERT_INTERVAL_MS )
				{
					auto oldestAgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - oldestEnqueueTime).count();

					LOG_ERROR(_T("Quarantine Persistent Warning: %d connection(s) still stuck, oldest for %lldms. Potential leak in application logic."),
						nStillQuarantinedCount, static_cast<long long>(oldestAgeMs));

					_quarantineLastSummaryLogTime = now;
				}
			}
		} // <-- qGuard가 여기서 이탈로 전역 스핀락(_globalQuarantineLock)을 초과점유하지 않는다

		// -----------------------------------------------------------------
		// PART A-1: 락 바깥에서 안전하게 지연 삭제 작업 (락 밖에서 실제 격리 해제)
		// -----------------------------------------------------------------
		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine: Safely deleted stalled connection outside the lock."));
		}

		// -----------------------------------------------------------------
		// PART B: 죽은 슬롯 빠른 스캔 및 재연결 디스패치 (블로킹 I/O 없음)
		// -----------------------------------------------------------------
		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			// [1차 검문]: 참조 중인 사용 중인 스레드가 있다면 재연결 없이 패스
			if( _pRefCount[i].value.load(std::memory_order_acquire) > 0 ) continue;

			// [2차 검문]: 커넥션이 정상적으로 연결 상태를 유지하고 있다면 패스
			CBaseMySQL* pCur = _pMySQLConns[i].value.load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->IsConnected() ) continue;

			// [2-1차 검문]: 연속 실패로 인한 백오프 대기 중이라면 이번 주기에는 재시도하지 않고 패스
			if( !IsRetryAllowed(i) ) continue;

			// [3차 검문]: 이미 재연결 워커가 이 슬롯을 처리 중이라면 중복 디스패치하지 않고 패스
			bool bExpected = false;
			if( !_pReconnecting[i].value.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel) )
				continue;

			// 실제 TryReconnect + 스왑은 워커 풀에 맡기고, 헬스체크 스레드는 즉시 다음 슬롯으로 진행
			EnqueueReconnect(i);
		}

		// CPU 자원 소모를 막기 위해 500ms만 대기 후에 재순회
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
	_healthCheckThreadMgr.JoinThreads(); // 모든 스레드가 완전히 이탈하여 종료 상태가 됨을 명시적으로 확인
}

//***************************************************************************
// 재연결 대기열에 슬롯 인덱스를 넣고 워커 하나를 깨운다.
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
// 이 워커 자신이 초과 인원인지 CAS로 판정한다. 초과라면 _nCurrentWorkerCount를
// 스스로 1 줄이고 true를 반환한다 (호출자는 즉시 스레드를 종료해야 한다).
// 여러 워커가 동시에 판정해도 CAS 경합으로 목표치를 초과한 만큼만 정확히 빠진다.
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
		// CAS 실패 시 nCur는 최신값으로 자동 갱신되어 while 조건에서 재평가된다.
	}
	return false;
}

//***************************************************************************
// 재연결 워커 루프 : 대기열에서 슬롯을 꺼내 실제 TryReconnect(I/O) + 스왑을 병렬로 수행
//***************************************************************************
void CMySQLConnPool::ReconnectWorkerLoop(void)
{
	while( true )
	{
		// 매 순회 시작 시 스스로 초과 인원인지 확인한다. SetWorkerCount로 축소 요청이 온 경우
		// 별도의 종료 신호를 큐에서 기다리지 않고 이 시점에 즉시 반영된다.
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

			// 전체 종료 신호가 왔고 남은 작업도 없다면 워커 스레드 종료
			if( _bStopReconnectWorkers.load(std::memory_order_relaxed) && _reconnectPendingSlots.empty() )
				return;

			if( _reconnectPendingSlots.empty() ) continue; // 루프 처음으로 돌아가 TryExitIfExcess 재확인

			nType = _reconnectPendingSlots.front();
			_reconnectPendingSlots.pop();
		}

		// 대기열에서 대기하는 동안 슬롯이 이미 다시 사용 중으로 바뀌었다면 재연결 불필요
		if( _pRefCount[nType].value.load(std::memory_order_acquire) > 0 )
		{
			_pReconnecting[nType].value.store(false, std::memory_order_release);
			continue;
		}

		// 여기서부터가 실제 블로킹 I/O 구간 — 다른 워커들은 이 슬롯과 무관하게 각자 슬롯을 병렬 처리한다
		CBaseMySQL* pNewConn = TryReconnect(nType);
		if( pNewConn == nullptr )
		{
			// 재연결 실패: 백오프를 적용하고, 다음 헬스체크 순회에서 백오프가 풀린 뒤 다시 큐잉되도록 둔다
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
// nWorkerCount개의 워커 스레드로 재연결 워커 풀을 새로 기동한다. (Init 전용)
//***************************************************************************
void CMySQLConnPool::StartReconnectWorkers(int32 nWorkerCount)
{
	_bStopReconnectWorkers.store(false, std::memory_order_relaxed);

	// 워커 수가 0 이하로 잘못 설정된 경우를 대비한 방어적 최소값 보정
	// (ValidateReconnectConfig를 이미 거쳤다면 정상적으로는 여기 걸릴 일이 없다)
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

void CMySQLConnPool::StopReconnectWorkers(void)
{
	_bStopReconnectWorkers.store(true, std::memory_order_relaxed);
	_reconnectQueueCv.notify_all(); // 대기 중인 모든 워커를 깨워 종료 조건을 확인하게 함
	_reconnectWorkerMgr.JoinThreads();
	_nCurrentWorkerCount.store(0, std::memory_order_relaxed);
	_nDesiredWorkerCount.store(0, std::memory_order_relaxed);
}

//***************************************************************************
// 목표 워커 수(_nDesiredWorkerCount)를 nNewCount로 갱신한다. (SetReconnectConfig 전용)
//***************************************************************************
void CMySQLConnPool::SetWorkerCount(int32 nNewCount)
{
	nNewCount = std::max(nNewCount, 1);
	_nDesiredWorkerCount.store(nNewCount, std::memory_order_release);

	int32 nExpected = _nCurrentWorkerCount.load(std::memory_order_acquire);
	if( nExpected < nNewCount )
	{
		int32 nBefore = nExpected;
		// CAS로 현재값을 목표치까지 원자적으로 끌어올린다. 동시에 여러 스레드가
		// SetWorkerCount(늘림)를 호출해도 이 루프가 실제로 스폰해야 할 정확한 개수를 결정해준다.
		while( nExpected < nNewCount &&
			!_nCurrentWorkerCount.compare_exchange_weak(nExpected, nNewCount, std::memory_order_acq_rel) )
		{
			// CAS 실패 시 nExpected가 최신값으로 갱신되어 재평가된다.
		}

		if( nExpected < nNewCount ) // CAS가 성공적으로 값을 올린 경우에만 그만큼 스레드를 스폰
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
		// 축소는 스레드를 직접 건드리지 않고 대기 중인 워커들을 깨워 스스로 판단하게 한다.
		_reconnectQueueCv.notify_all();
		LOG_DEBUG(_T("SetWorkerCount: target worker count decreased to %d, workers will self-exit"), nNewCount);
	}
}

//***************************************************************************
// 서비스 운영 중 재연결 백오프 정책과 워커 스레드 수를 동적으로 조정한다.
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

	// 워커 수는 단순 필드 갱신이 아니라 실제 스레드 기동/종료 조율이 필요하므로 별도 경로로 처리한다.
	SetWorkerCount(reconnectConfig.nWorkerCount);

	return true;
}

//***************************************************************************
// 현재 적용 중인 재연결 설정값의 스냅샷을 조회한다.
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
// 풀 자원 최종 청소 및 프로세스 셧다운
//***************************************************************************
void CMySQLConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CBaseMySQL*> vShutdownDeletes;

	// 풀 지정된 고정 크기인 _nMaxPoolSize 배열 전체를 순회하며 자원 정리 작업 진행
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseMySQL* pConn = _pMySQLConns[i].value.load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;

		// 사용 중인 스레드가 알아서 완전히 이탈할 때까지 기다림
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
			// 시스템 다운타임 상황에서 이탈하지 않는 참조 잔여로는 격리 리스트로 우선 격리한 뒤
			LOG_ERROR(_T("Clear: Slot(%d) refcount is zombie (%d). Moving to quarantine."), i, _pRefCount[i].value.load());

			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i].value, now });
		}
		else
		{
			xdelete(pConn); // 참조하는 스레드가 없으므로 즉시 안전 삭제
		}

		_pMySQLConns[i].value.store(nullptr, std::memory_order_relaxed);
	}

	// -----------------------------------------------------------------
	// 셧다운 시점 격리 큐 처리 (아키텍처 일관성을 위한 락-밖 소멸 유지)
	// -----------------------------------------------------------------
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
					vShutdownDeletes.push_back(item.pConn); // 락 바깥에서 최종 안전 삭제 예정 목록에 적재
				}
			}
			else
			{
				// [셧다운 시점 크래시 예방 정책: Safe Leak]
				// 프로세스 종료 마지막까지 이탈하지 않는 극소 수의 스레드가 존재할 경우라도,
				// 무리하게 삭제 시도 시 실제로 메모리 손상 크래시(UAF)로 이어진다.
				// 이를 방지하기 위해 삭제를 의도적으로 방기(Safe Leak)하고 남은 프로세스는 안전하게 종료되도록 한다.
				LOG_ERROR(_T("Clear: Abandoning leaked connection to prevent Use-After-Free crash."));
			}
		}
	} // <-- qGuard가 파괴되며 락이 해제됨

	// 격리된 소멸 예정들 중 락 바깥 단계에서 안전하게 마무리해 삭제한다
	for( int32 i = 0; i < vShutdownDeletes.size(); ++i )
	{
		xdelete(vShutdownDeletes[i]);
		LOG_DEBUG(_T("Clear: Safely deleted stalled quarantine connection during shutdown."));
	}
}