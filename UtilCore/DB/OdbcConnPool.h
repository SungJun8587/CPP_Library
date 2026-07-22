
//***************************************************************************
// OdbcConnPool.h : interface for the COdbcConnPool class.
//
//***************************************************************************

#ifndef __ODBCCONNPOOL_H__
#define __ODBCCONNPOOL_H__

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __CACHEALIGNMENT_H__
#include <Thread/CacheAlignment.h>
#endif

#ifndef __SPINLOCK_H__
#include <Thread/SpinLock.h>
#endif

#ifndef __THREADMANAGER_H__
#include <Thread/ThreadManager.h>
#endif

#ifndef __BASEODBC_H__
#include <DB/BaseODBC.h>
#endif

#include <mutex>
#include <condition_variable>

// [할당자 선택] COdbcConnPool은 DB 노드당 1개씩, 서버 기동 시 딱 한 번만 생성되는
// 객체다(핫패스 아님). Allocator.h의 설계 구분에 따르면 이런 "크거나 드물게 생성되는
// 객체"는 고빈도 핫패스용 PoolAllocator(xnew/xdelete)가 아니라 BaseAllocator를
// 상속해 RawAllocator 경로로 분리하는 것이 맞다. BaseAllocator는 데이터 멤버가 없고
// non-virtual 함수만 상속되므로 vptr 등 추가 오버헤드도 없다.
class COdbcConnPool : public BaseAllocator
{
private:
	// 격리 대기 중인(참조가 남아 즉시 삭제 못하는) 오래된 커넥션 정보
	struct TQuarantineItem
	{
		CBaseODBC* pConn;                  // 격리 대상 커넥션
		std::atomic<int32>* pRefCount;     // 감시할 참조 카운트 주소 (CachePaddedAtomic<int32>::value)
		std::chrono::steady_clock::time_point lastLogTime; // 마지막 경고 로그 시각
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10; // 백오프 base 하한 (재연결 폭주 방지)

public:
	// 재연결 워커 수 / 백오프 정책 (Init 초기 설정 및 SetReconnectConfig 런타임 변경에 공용 사용)
	struct TReconnectConfig
	{
		int32	nWorkerCount = 4;     // 재연결 워커 스레드 수
		int64	nBackoffBaseMs = 500;   // 최초 재시도 간격
		int64	nBackoffMaxMs = 30000; // 재시도 간격 상한
		int32	nBackoffMaxShift = 6;     // 지수 증가 상한 (2^n)
		int32	nBackoffJitterMs = 250;   // 재시도 지터 상한
	};

	explicit COdbcConnPool(int32 nMaxPoolSize);
	virtual ~COdbcConnPool(void);

	// 풀 초기화 및 지정 DSN으로 커넥션을 미리 채움. reconnectConfig 유효성 실패 시 기본값 대체.
	bool		Init(const EDBClass dbClass, const TCHAR* ptszDSN,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});

	// 참조 카운트 증가 후 슬롯(nType)의 커넥션을 획득/반환
	CBaseODBC* GetOdbcConn(int32 nType);
	void		ReleaseOdbcConn(int32 nType);

	// PopFreeSlotIndex가 이미 예약한 슬롯을 참조 카운트 변화 없이 조회 (OdbcConnGuard 전용)
	// 주의: 반드시 PopFreeSlotIndex로 슬롯을 선점한 상태에서만 호출
	CBaseODBC* GetPooledConnUnsafe(int32 nType) const;

	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	// 재연결 백오프/워커 수를 런타임에 조정. 유효성 실패 시 전량 거부(부분 적용 없음).
	// 워커 수 변경은 목표치만 갱신하며 각 워커가 스스로 수렴 (TryExitIfExcess 참고).
	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);

	// 현재 재연결 설정 스냅샷 조회 (모니터링용, 필드별 원자 변수라 완전한 원자적 스냅샷은 아님)
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	// 풀 내 모든 커넥션을 안전하게 정리 (Shutdown용)
	void		Clear(void);

	// 슬롯 인덱스 유효 범위 검사
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }

	// TReconnectConfig 값 유효성 검사 (Init/SetReconnectConfig 공용)
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	// 새 커넥션 생성 및 연결 시도 (블로킹 I/O)
	CBaseODBC* TryReconnect(int32 nType);

	// 재연결 워커가 확보한 새 커넥션으로 슬롯 교체 및 낡은 커넥션 격리/삭제
	void		ApplyReconnectedConn(int32 nType, CBaseODBC* pNewConn);

	// 슬롯별 백오프 상태 관리
	bool		IsRetryAllowed(int32 nType) const;
	void		OnReconnectFailed(int32 nType);     // 실패 시 백오프 지수 증가
	void		OnReconnectSucceeded(int32 nType);  // 성공 시 백오프 초기화
	static int64	NowMs(void);

	// 헬스체크 스레드 루프 (빠른 스캔 + 재연결 디스패치만, 블로킹 없음)
	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

	// 재연결 대기열에서 슬롯을 꺼내 TryReconnect + 스왑을 수행하는 워커 루프
	void		ReconnectWorkerLoop(void);
	void		StartReconnectWorkers(int32 nWorkerCount); // Init 전용
	void		StopReconnectWorkers(void);                 // Shutdown 전용

	// 목표 워커 수 갱신. 확대는 부족분 즉시 스폰, 축소는 조건변수로 깨워 각 워커가 스스로 종료 판단.
	void		SetWorkerCount(int32 nNewCount);

	// 이 워커가 초과 인원인지 CAS로 판정, 초과면 카운트 감소 후 true 반환
	bool		TryExitIfExcess(void);
	void		EnqueueReconnect(int32 nType);

protected:
	// 복사/대입 금지
	COdbcConnPool(const COdbcConnPool& rhs) = delete;
	COdbcConnPool& operator=(const COdbcConnPool& rhs) = delete;

	EDBClass								_dbClass;                       // DB 종류
	TCHAR									_tszDSN[DATABASE_DSN_STRLEN];   // DSN 문자열
	const int32								_nMaxPoolSize;                  // 풀 최대 크기 (고정)

	// 재연결 정책 값들. 구조체 통째 atomic으로 감싸면 lock-free가 깨지므로 필드별로 분리 관리.
	std::atomic<int64>			_nBackoffBaseMs;
	std::atomic<int64>			_nBackoffMaxMs;
	std::atomic<int32>			_nBackoffMaxShift;
	std::atomic<int32>			_nBackoffJitterMs;

	// [불변 조건] 아래 unique_ptr 배열들은 생성자에서 단 1회만 할당한다.
	// 격리 큐가 각 슬롯의 참조 카운트 주소(&_pRefCount[i].value)를 저장하므로,
	// Init() 도중을 포함해 런타임 중에 절대 임의로든 재할당(make_unique 등)해서는 안 된다.
	//
	// CachePaddedAtomic<T>로 감싸 슬롯 하나당 캐시라인 하나를 점유하도록 강제했다. 서로 다른
	// 슬롯을 동시에 다루는 스레드들이 false sharing으로 서로의 캐시라인을 무효화시키는 것을 막는다.
	std::unique_ptr<CachePaddedAtomic<CBaseODBC*>[]>	_pOdbcConns; // 슬롯별 실제 커넥션 포인터
	std::unique_ptr<CachePaddedAtomic<int32>[]>		_pRefCount;  // 슬롯 사용중/이탈 여부를 원자적으로 관리하는 참조 카운터 배열 (가장 핫한 배열)

	// 캐시 라인 정렬(alignas) 및 크기 고정 패딩이 적용된 SpinLockDefault로 채용하여 슬롯 간의 false sharing을 방지한다.
	std::unique_ptr<SpinLockDefault[]>			_slotLocks;  // 각 커넥션 슬롯 자체 교체 시점을 보호하기 위한 슬롯 스핀락 배열

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;       // 슬롯별 "현재 재연결 워커가 처리 중" 여부 (중복 디스패치 방지)
	std::unique_ptr<CachePaddedAtomic<int64>[]>		_pNextRetryAllowedMs; // 슬롯별 "이 시각 이후에만 재시도 허용" (백오프)
	std::unique_ptr<CachePaddedAtomic<int32>[]>		_pRetryFailCount;     // 슬롯별 연속 재연결 실패 횟수 (백오프 지수 계산용)

	CThreadManager				_healthCheckThreadMgr;       // 백그라운드 감시 스레드를 관리하는 매니저
	std::atomic<bool>			_bStopHealthCheck;           // 헬스체크 루프 중단 신호 플래그
	int32						_nHealthCheckIntervalMs;     // 헬스체크 주기 (기본값 500ms)

	std::atomic<uint32>		_nNextSlotHint;              // PopFreeSlotIndex 탐색 시작상 힌트 (슬롯별 경합 분산)

	CThreadManager				_reconnectWorkerMgr;         // 재연결 워커 스레드들을 관리하는 매니저
	std::atomic<bool>			_bStopReconnectWorkers;      // 재연결 워커 전체 종료 신호 (Shutdown 전용)
	std::atomic<int32>			_nCurrentWorkerCount;        // 현재 실제로 떠 있는(혹은 떠 있어야 할) 재연결 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;        // SetWorkerCount로 갱신되는 목표 워커 스레드 수

	std::mutex					_reconnectQueueMutex;        // 재연결 대기열 보호용 뮤텍스
	std::condition_variable	_reconnectQueueCv;           // 대기열에 새 작업이 들어왔거나 워커 수 목표가 바뀌었음을 알리는 조건 변수
	std::queue<int32>			_reconnectPendingSlots;      // 재연결이 필요한 슬롯 인덱스 대기열

	SpinLockDefault				_globalQuarantineLock;       // 격리 큐에 대한 전역 접근을 보호하는 스핀락
	std::queue<TQuarantineItem>	_quarantineQueue;            // 참조 카운트가 남아 있는 오래된 커넥션들을 지연 삭제 대기시키는 큐
};

class OdbcConnGuard
{
public:
	explicit OdbcConnGuard(COdbcConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		// 1. 빈 슬롯을 검색해 원자적으로 즉시 선점 (참조 카운트 = 1)
		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			// 2. 이미 유효성 확인된 슬롯이므로 카운트 재증가 없이 값만 조회
			_pConn = _pPool->GetPooledConnUnsafe(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				// 레이스컨디션 등으로 슬롯이 무효화된 경우, 선점했던 카운트 되돌림
				_pPool->ReleaseOdbcConn(_nAllocatedIndex);
				_nAllocatedIndex = -1;
			}
		}
	}

	~OdbcConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseOdbcConn(_nAllocatedIndex);
		}
	}

	CBaseODBC* operator->() const noexcept { return _pConn; }
	CBaseODBC* get() const noexcept { return _pConn; }
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	OdbcConnGuard(const OdbcConnGuard&) = delete;
	OdbcConnGuard& operator=(const OdbcConnGuard&) = delete;

private:
	COdbcConnPool* _pPool;
	CBaseODBC* _pConn;
	int32			_nAllocatedIndex;
};

#endif // ndef __ODBCCONNPOOL_H__