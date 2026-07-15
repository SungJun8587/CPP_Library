
//***************************************************************************
// OdbcConnPool.cpp : implementation of the COdbcConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcConnPool.h"

// 안전 장치 및 모니터링 타임아웃 상수 설정
constexpr int32 WAIT_TIMEOUT_MS = 100;          // 끊긴 커넥션 교체 시, 기존 스레드가 이탈할 때까지 대기하는 최대 임계치 (100ms)
constexpr int64 LOG_ALERT_INTERVAL_MS = 300000; // 격리 큐에 묶인 좀비 커넥션 경고 로그 스팸 방지 주기 (5분)

//***************************************************************************
// Construction/Destruction
//***************************************************************************

COdbcConnPool::COdbcConnPool(int32 nMaxPoolSize)
	: _dbClass(EDBClass::NONE)
	, _nMaxPoolSize(nMaxPoolSize)
	, _bStopHealthCheck(false)
	, _nHealthCheckIntervalMs(500)
{
	// unique_ptr 배열로 생성자에서 단 1회만 고정 할당 (주소 무효화 및 메모리 주소 흔들림 방지)
	_pOdbcConns = std::make_unique<std::atomic<CBaseODBC*>[]>(_nMaxPoolSize);
	_pRefCount = std::make_unique<std::atomic<int32>[]>(_nMaxPoolSize);

	// CPU 캐시 라인 충돌(False Sharing)을 방지하도록 구조 정렬이 맞춰진 SpinLockDefault를 배열 형태로 생성합니다.
	_slotLocks = std::make_unique<SpinLockDefault[]>(_nMaxPoolSize);

	// 초기 배열 원소 기본값 세팅
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pOdbcConns[i].store(nullptr, std::memory_order_relaxed);
		_pRefCount[i].store(0, std::memory_order_relaxed);
	}
	memset(&_tszDSN[0], 0, sizeof(_tszDSN));
}

COdbcConnPool::~COdbcConnPool(void)
{
	// 1. 소멸자 호출 시 감시 스레드를 안전하게 정지시켜 추가적인 레이스 컨디션을 예방
	StopHealthCheckThread();
	// 2. 남아있는 모든 커넥션 자원을 누수 없이 원자적으로 정리
	Clear();
}

//***************************************************************************
// 초기화 (순차 처리 구간)
//***************************************************************************
bool COdbcConnPool::Init(const EDBClass dbClass, const TCHAR* ptszDSN)
{
	// 혹시 모를 재초기화 시나리오에 대비해 기존 백그라운드 스레드를 정지하고 풀 청소 진행
	StopHealthCheckThread();
	Clear();

	_dbClass = dbClass;
	_tcsncpy_s(_tszDSN, _countof(_tszDSN), ptszDSN, _TRUNCATE);

	// 지정된 최대 크기만큼 커넥션을 맺고 배열(풀)을 채워줌 (초기화 과정은 싱글 스레드 영역이므로 순차 할당)
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

		// 다른 스레드가 생성된 커넥션을 볼 수 있도록 Release 세맨틱으로 메모리에 기록
		_pOdbcConns[i].store(pConn, std::memory_order_release);
	}

	// 동적 감시 및 복구 작업을 수행할 백그라운드 헬스체크 스레드 가동
	StartHealthCheckThread();
	return true;
}

//***************************************************************************
// 무거운 커넥션 맺기 시도 (락-밖 영역에서 단독 실행)
//***************************************************************************
CBaseODBC* COdbcConnPool::TryReconnect(int32 nType)
{
	// 대기 시간 및 잠금 범위 최소화를 위해, 실제 커넥션을 생성하고 핸드셰이크를 맺는
	// '무거운 작업'은 락을 일체 획득하지 않은 맨몸의 스레드 상태에서 수행합니다.
	CBaseODBC* pNewConn = xnew<CBaseODBC>(_dbClass, _tszDSN);
	if( pNewConn == nullptr )
	{
		LOG_ERROR(_T("TryReconnect: xnew<CBaseODBC> failed (index=%d)"), nType);
		return nullptr;
	}

	// 네트워크 I/O 병목이 걸릴 수 있는 소켓 연결 단계
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
CBaseODBC* COdbcConnPool::GetOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) )
	{
		LOG_ERROR(_T("GetOdbcConn: invalid index(%d), MaxPoolSize(%d)"), nType, _nMaxPoolSize);
		return nullptr;
	}

	// -------------------------------------------------------------------------
	// [핵심 동시성 방어 메커니즘 1단계: 선점(Preemption)]
	// 참조 카운트를 원자적으로 즉시 먼저 증가시켜서, 백그라운드 헬스체크 스레드가
	// 해당 슬롯의 커넥션 포인터를 소멸시키거나 메모리를 날리는 개입을 원천적으로 차단합니다.
	// -------------------------------------------------------------------------
	_pRefCount[nType].fetch_add(1, std::memory_order_relaxed);

	// 2. 가시성(Happens-Before)이 완전히 확보된 최신 상태의 포인터를 락-프리로 안전하게 로드
	CBaseODBC* pOdbcConn = _pOdbcConns[nType].load(std::memory_order_acquire);

	// 3. 커넥션이 없거나 DB 연결이 끊긴 무효 상태인지 검증
	if( pOdbcConn == nullptr || !pOdbcConn->IsConnected() )
	{
		// 유효하지 않은 커넥션이라면 증가시켰던 카운트를 원자적으로 복원(Release)한 뒤, nullptr 반환
		// (복원된 슬롯은 곧 백그라운드 헬스체크 스레드가 감지하여 재연결을 시도하게 됨)
		_pRefCount[nType].fetch_sub(1, std::memory_order_release);
		LOG_DEBUG(_T("GetOdbcConn: slot(%d) not ready, awaiting background reconnect"), nType);
		return nullptr;
	}

	// 정상적으로 검증된 살아있는 커넥션을 반환 (호출한 스레드는 이제 마음놓고 사용 가능)
	return pOdbcConn;
}

//***************************************************************************
// 자원 반환
//***************************************************************************
void COdbcConnPool::ReleaseOdbcConn(int32 nType)
{
	if( !IsValidIndex(nType) ) return;

	// 선점했던 점유권을 반환합니다. 카운터가 감소하여 0이 되는 순간,
	// 대기 중이던 백그라운드 헬스체크 스레드가 안전하게 수거할 수 있는 상태가 됩니다.
	_pRefCount[nType].fetch_sub(1, std::memory_order_release);
}

//***************************************************************************
// 현재 완전히 비어있어서 독점 사용 가능한 슬롯의 인덱스를 반환합니다. (없으면 -1)
//***************************************************************************
int32 COdbcConnPool::PopFreeSlotIndex(void)
{
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		// 1차 거름망: 이미 다른 비즈니스 스레드가 참조 중이라면 락 없이 빠르게 패스
		if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;

		// [동기화 핵심] 개별 슬롯의 스핀락을 획득하여 헬스체크 스레드 및 타 스레드의 개입을 차단
		SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[i]);

		// 2차 검문: 락 내부에서 참조 카운트가 여전히 0인지 재검증
		if( _pRefCount[i].load(std::memory_order_acquire) == 0 )
		{
			// 3차 검문: 락 내부에서 커넥션 객체가 존재하고 실제 유효하게 연결된 상태인지 최종 검증
			CBaseODBC* pConn = _pOdbcConns[i].load(std::memory_order_acquire);
			if( pConn != nullptr && pConn->IsConnected() )
			{
				// 참조 카운트를 1로 올려 완벽하게 내 것으로 독점 선점 확정
				_pRefCount[i].store(1, std::memory_order_release);
				return i;
			}
		}
	}
	return -1; // 현재 이용 가능한 깨끗한 커넥션이 풀에 없음
}

//***************************************************************************
// 백그라운드 헬스체크 루프 (지연 격리 및 락-밖 소멸 적용)
//***************************************************************************
void COdbcConnPool::HealthCheckLoop(void)
{
	// 헬스체크 주기마다 매번 벡터 메모리를 할당/소멸하는 부하를 막기 위해
	// 루프 외곽에 딱 한 번 선언하여 CVector 내부 캐패시티(Capacity)를 지속적으로 재활용합니다.
	CVector<CBaseODBC*> vDeletes;

	while( !_bStopHealthCheck.load(std::memory_order_relaxed) )
	{
		// 루프 매 회차 진입 시 수집 컨테이너를 안전하게 비움
		vDeletes.clear();

		// -----------------------------------------------------------------
		// PART A: 격리 큐 청소 (락 안에서는 원자적 판별 및 추출만 신속하게 수행)
		// -----------------------------------------------------------------
		{
			// 글로벌 격리 큐 처리를 위한 독점 동기화 락을 획득합니다.
			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			size_t qSize = _quarantineQueue.size(); // 레이스 컨디션을 피하기 위해 순간의 크기를 스냅샷으로 확보

			for( size_t k = 0; k < qSize; ++k )
			{
				TQuarantineItem item = _quarantineQueue.front();
				_quarantineQueue.pop();

				// [지연 격리 해제 검사]
				// 해당 좀비 슬롯의 참조 카운터가 정말로 완전히 0(이용 스레드 완전 이탈)이 되었는지 원자적 확인
				if( item.pRefCount->load(std::memory_order_acquire) == 0 )
				{
					if( item.pConn != nullptr )
					{
						// 실시간 삭제 대상 리스트에 포인터만 수집
						vDeletes.push_back(item.pConn);
					}
				}
				else
				{
					// [경보 제어 메커니즘]
					// 여전히 이용 중인 비정상 좀비 스레드가 존재한다면, 로그 스팸 방지를 위한
					// 타임아웃 주기를 체크하여 정확히 5분마다 심각한 메모리 누수 위험을 개발자에게 인지시킴
					auto now = std::chrono::steady_clock::now();
					auto elapsedFromLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - item.lastLogTime).count();

					if( elapsedFromLastLog >= LOG_ALERT_INTERVAL_MS )
					{
						LOG_ERROR(_T("Quarantine Persistent Warning: Connection is still stuck in quarantine! Potential leak in application logic."));
						item.lastLogTime = now;
					}

					// 아직 정리가 불가하므로 격리 큐의 후방으로 돌려서 다음 루프 때 재검사하도록 유지
					_quarantineQueue.push(item);
				}
			}
		} // <-- qGuard의 블록 이탈로 글로벌 스핀락(_globalQuarantineLock)이 초고속으로 해제됨

		// -----------------------------------------------------------------
		// PART A-1: 무거운 I/O를 동반한 실제 소멸 작업 (락 밖에서 병목 격리 실행)
		// -----------------------------------------------------------------
		// 소켓 커넥트 연결 해제와 수반되는 커널 소멸 연산 등 '무거운 I/O'는 
		// 락을 완벽히 푼 '이 구간'에서 병목 없이 안전하고 빠르게 정리합니다.
		for( int32 i = 0; i < vDeletes.size(); ++i )
		{
			xdelete(vDeletes[i]);
			LOG_DEBUG(_T("Quarantine: Safely deleted stalled connection outside the lock."));
		}

		// -----------------------------------------------------------------
		// PART B: 정기 커넥션 끊김 감지 및 교체 (삼중 체크 메커니즘)
		// -----------------------------------------------------------------
		for( int32 i = 0; i < _nMaxPoolSize && !_bStopHealthCheck.load(std::memory_order_relaxed); i++ )
		{
			// [1차 검문]: 현재 슬롯을 사용 중인 비즈니스 스레드가 있다면 안전을 위해 패스
			if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;

			// [2차 검문]: 커넥션이 존재하고 정상 연결 상태를 유지하고 있다면 패스
			CBaseODBC* pCur = _pOdbcConns[i].load(std::memory_order_acquire);
			if( pCur != nullptr && pCur->IsConnected() ) continue;

			// [3차 검문]: 연결이 끊겨 교체 대기 상태이나, 찰나의 순간에 진입한 스레드가 있다면 패스 (경합 완벽 방지)
			if( _pRefCount[i].load(std::memory_order_acquire) > 0 ) continue;

			// [락 밖에서 안전한 사전 재연결]: 무거운 Connect를 락 없이 진행
			CBaseODBC* pNewConn = TryReconnect(i);
			if( pNewConn == nullptr ) continue; // 재연결 실패 시 다음 주기 혹은 타 슬롯 진행

			// 재연결 중에 찰나의 순간으로 또다시 사용자가 난입했다면 새로 만든 자원을 버리고 보류
			if( _pRefCount[i].load(std::memory_order_acquire) > 0 )
			{
				xdelete(pNewConn);
				continue;
			}

			// 안전 장치 확보 완료 후, 개별 슬롯 락을 빠르게 잡고 교체(Swap) 진행
			CBaseODBC* pOldConn = nullptr;
			{
				SpinLockGuard<SpinLockPreset::Default> guard(_slotLocks[i]);
				pOldConn = _pOdbcConns[i].load(std::memory_order_acquire);
				_pOdbcConns[i].store(pNewConn, std::memory_order_release); // 새 커넥션 노출 완료
			}

			if( pOldConn != nullptr )
			{
				// 교체 직후, 기존 자원을 소유하고 있던 아주 무거운 지연 쿼리 스레드가 빠져나가는지 안전 시간 동안 확인
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
					std::this_thread::yield(); // 다른 스레드에 CPU 자원을 양보하며 양전 대기
				}

				if( bTimeout )
				{
					// [안전 예방 조치: 지연 격리 이송]
					// 100ms를 초과할 정도로 오랜 시간 참조를 반환하지 않는 좀비 스레드가 있다면,
					// 이를 강제로 지워 UAF(Use-After-Free) 크래시를 내는 대신 격리 큐로 안전하게 추방합니다.
					LOG_ERROR(_T("HealthCheck: Slot(%d) refcount high during swap. Moving to quarantine."), i);

					auto now = std::chrono::steady_clock::now();
					SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
					_quarantineQueue.push({ pOldConn, &_pRefCount[i], now });
				}
				else
				{
					// 아무도 사용하지 않음이 깔끔하게 확인된 구식 커넥션은 즉시 삭제
					xdelete(pOldConn);
				}
			}
		}

		// CPU 자원 과소비를 전면 차단하기 위해 500ms의 대기 간격을 가짐
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
	_healthCheckThreadMgr.JoinThreads(); // 모든 스레드가 온전히 이탈하여 정지 상태가 됨을 동기적으로 확인
}

//***************************************************************************
// 풀 자원 전면 청소 및 프로세스 셧다운 (컴파일러 이슈 및 락-밖 소멸 통일 완수)
//***************************************************************************
void COdbcConnPool::Clear(void)
{
	auto now = std::chrono::steady_clock::now();
	CVector<CBaseODBC*> vShutdownDeletes;

	// 풀 내부의 고정 크기인 _nMaxPoolSize 배열 원소 전체를 선회하며 자원 수거 작업 돌입
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		CBaseODBC* pConn = _pOdbcConns[i].load(std::memory_order_acquire);
		if( pConn == nullptr ) continue;

		auto startTime = std::chrono::steady_clock::now();
		bool bTimeout = false;

		// 사용 중인 스레드가 있을 경우 안전하게 이탈을 기다림
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
			// 시스템 다운타임 상황에도 이탈하지 못한 좀비 스레드는 격리 리스트로 우선 격리해 둠
			LOG_ERROR(_T("Clear: Slot(%d) refcount is zombie (%d). Moving to quarantine."), i, _pRefCount[i].load());

			SpinLockGuard<SpinLockPreset::Default> qGuard(_globalQuarantineLock);
			_quarantineQueue.push({ pConn, &_pRefCount[i], now });
		}
		else
		{
			xdelete(pConn); // 참조하는 스레드가 없으므로 즉시 안전 삭제
		}

		_pOdbcConns[i].store(nullptr, std::memory_order_relaxed);
	}

	// -----------------------------------------------------------------
	// 셧다운 시점 최종 격리 큐 처리 (아키텍처 일관성을 위해 락-밖 소멸 적용)
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
					vShutdownDeletes.push_back(item.pConn); // 락 내부에서는 지울 포인터 수집만 수행
				}
			}
			else
			{
				// [셧다운 시점 크래시 원천 차단 정책: Safe Leak]
				// 프로세스 종료 시점까지도 쿼리가 안 끝나고 이탈하지 않는 악성 좀비 스레드가 끝끝내 버틴다면,
				// 무리하게 해제 시도 시 100% 확률로 사용중 메모리 소멸 크래시(UAF)가 유발됩니다.
				// 이를 완벽히 예방하기 위해 소멸을 의도적으로 방기(Safe Leak)시켜 서버 프로세스가 깔끔하게 종료되도록 합니다.
				LOG_ERROR(_T("Clear: Abandoning leaked connection to prevent Use-After-Free crash."));
			}
		}
	} // <-- qGuard가 파괴되며 락이 해제됨

	// 수집된 소멸 대상들을 락 영역 외곽에서 안전하게 삭제하며 마무리
	for( int32 i = 0; i < vShutdownDeletes.size(); ++i )
	{
		xdelete(vShutdownDeletes[i]);
		LOG_DEBUG(_T("Clear: Safely deleted stalled quarantine connection during shutdown."));
	}
}