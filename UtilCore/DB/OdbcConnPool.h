
//***************************************************************************
// OdbcConnPool.h : interface for the COdbcConnPool class.
//
//***************************************************************************

#ifndef UC_ODBCCONNPOOL_H
#define UC_ODBCCONNPOOL_H

#include <Memory/Allocator.h>
#include <Memory/Containers.h>
#include <Thread/CacheAlignment.h>
#include <Thread/PlatformLock.h>
#include <Thread/ThreadManager.h>
#include <DB/BaseODBC.h>
#include <Containers/Queue/DelayedTaskQueue.h>

//***************************************************************************
// @brief ODBC 커넥션 풀 클래스
// @details 커넥션의 인라인 할당, 재연결 워커, 헬스체크 및 슬롯 관리를 수행하는 스레드 세이프 커넥션 풀입니다.
//***************************************************************************
class COdbcConnPool : public BaseAllocator
{
private:
	//***************************************************************************
	// @struct TQuarantineItem
	// @brief 격리된 커넥션 정보 구조체
	// @details 참조 카운트가 남아 즉시 삭제하지 못하고 격리된 오래된 커넥션의 정보를 보관합니다.
	//***************************************************************************
	struct TQuarantineItem
	{
		CBaseODBC* pConn;                  // 격리 대상 커넥션 포인터
		std::atomic<int32>* pRefCount;     // 감시할 슬롯의 참조 카운트 주소
		// [수정 — 강제 정리 타임아웃이 절대 안 걸리던 버그] 이전에는 이
		// lastLogTime 한 필드를 "격리 큐에 들어온 이후 전체 경과 시간"과
		// "마지막 경고 로그 시각" 두 용도로 같이 썼다. HealthCheckLoop가
		// 5분마다 경고를 남길 때마다 이 필드를 now로 리셋했는데, 그러면
		// FORCE_CLEANUP_TIMEOUT_MS(10분) 판정에 쓰는 경과 시간도 함께
		// 리셋되어 참조 카운트가 절대 안 풀리는 좀비 커넥션이 강제
		// 정리되지 못하고 무한정 격리 큐에 남아있게 됐다. 두 용도를
		// 분리한다 — enqueueTime은 최초 격리된 시각으로 절대 갱신되지
		// 않고, lastLogTime만 경고 쿨다운 판단용으로 갱신된다.
		std::chrono::steady_clock::time_point enqueueTime; // 격리 큐에 들어온 시각(강제 정리 타임아웃 판단용, 절대 갱신되지 않음)
		std::chrono::steady_clock::time_point lastLogTime; // 마지막 경고 로그 시각(쿨다운 판단용, 경고 시마다 갱신)
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10; // 백오프 base 하한 값

public:
	//***************************************************************************
	// @struct TReconnectConfig
	// @brief 재연결 정책 설정 구조체
	// @details 재연결 워커 수 및 지수 백오프 정책을 정의합니다.
	//***************************************************************************
	struct TReconnectConfig
	{
		int32	nWorkerCount = 4;			// 재연결을 전담하는 백그라운드 워커 스레드 수
		int64	nBackoffBaseMs = 500;		// 최초 재시도 대기 기본 시간 (밀리초)
		int64	nBackoffMaxMs = 30000;		// 재시도 대기 시간 상한선 (밀리초)
		int32	nBackoffMaxShift = 6;		// 백오프 지수 증가 횟수 상한
		int32	nBackoffJitterMs = 250;		// 동시 재시도 충돌 방지를 위한 지터 최대값 (밀리초)
	};

	explicit COdbcConnPool(int32 nMaxPoolSize);
	virtual ~COdbcConnPool(void);

	bool		Init(const EDBClass dbClass, const TCHAR* ptszDSN,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});

	CBaseODBC* GetOdbcConn(int32 nType);
	void		ReleaseOdbcConn(int32 nType);
	CBaseODBC* GetPooledConnUnsafe(int32 nType) const;

	//***************************************************************************
	// @brief 최대 풀 크기 조회
	// @return 커넥션 풀의 고정 최대 슬롯 크기
	//***************************************************************************
	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	void		Clear(void);

	//***************************************************************************
	// @brief 슬롯 인덱스 유효성 검사
	// @param nType 검사할 슬롯 인덱스
	// @return 인덱스가 유효한 범위 내에 있으면 true, 그렇지 않으면 false
	//***************************************************************************
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	CBaseODBC* TryReconnect(int32 nType);
	void		ApplyReconnectedConn(int32 nType, CBaseODBC* pNewConn);

	void		ScheduleRetry(int32 nType);
	void		OnReconnectFailed(int32 nType);
	void		OnReconnectSucceeded(int32 nType);

	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

	void		DelayedTaskLoop(void);
	void		StartDelayedTaskThread(void);
	void		StopDelayedTaskThread(void);

	void		ReconnectWorkerLoop(void);
	void		StartReconnectWorkers(int32 nWorkerCount);
	void		StopReconnectWorkers(void);

	void		SetWorkerCount(int32 nNewCount);
	bool		TryExitIfExcess(void);
	void		EnqueueReconnect(int32 nType);

	//***************************************************************************
	// @brief 슬롯을 프리 큐에 등록합니다(중복 삽입 방지 포함).
	// @details [신규] Init()/ReleaseOdbcConn()/ApplyReconnectedConn() 세
	// 곳에서 "이 슬롯이 이제 사용 가능해졌다"는 이벤트가 발생할 때마다
	// 호출한다. 같은 슬롯이 반복적으로 연결 끊김→재연결을 겪으면서 매번
	// 프리 큐에 push만 하고 예전 항목을 빼지는 않으면, 사용 빈도가 낮은
	// 환경에서 재연결 이벤트만 계속 쌓여 큐가 무제한으로 커질 수 있다
	// (동작 자체는 안전하지만 — 중복 항목은 각각 CAS로 걸러진다 — 자원
	// 낭비다). _pInFreeSlotQueue로 "이 슬롯이 지금 큐 안에 있는지"를
	// 추적해, 이미 큐에 있으면 추가로 넣지 않는다.
	//***************************************************************************
	void		EnqueueFreeSlot(int32 nType);

protected:
	COdbcConnPool(const COdbcConnPool& rhs) = delete;
	COdbcConnPool& operator=(const COdbcConnPool& rhs) = delete;

	//-------------------------------------------------------------------------
	// 멤버 변수 정의
	//-------------------------------------------------------------------------
	EDBClass								_dbClass;                        // 대상 데이터베이스 종류
	TCHAR									_tszDSN[DATABASE_DSN_STRLEN];   // 데이터소스 이름(DSN) 연결 문자열
	const int32								_nMaxPoolSize;                  // 커넥션 풀의 고정 최대 슬롯 크기

	// 재연결 백오프 정책 관련 아토믹 변수들
	std::atomic<int64>			_nBackoffBaseMs;       // 현재 설정된 백오프 기본 대기 시간 (ms)
	std::atomic<int64>			_nBackoffMaxMs;        // 현재 설정된 백오프 최대 대기 시간 상한 (ms)
	std::atomic<int32>			_nBackoffMaxShift;     // 백오프 지수 계산 시 최대 시프트 횟수
	std::atomic<int32>			_nBackoffJitterMs;     // 백오프 지터 최대 범위 (ms)

	// 동적 메모리 및 캐시라인 정렬 슬롯 배열
	std::unique_ptr<CachePaddedAtomic<CBaseODBC*>[]>	_pOdbcConns;   // 슬롯별 실제 데이터베이스 연결 객체 포인터 배열
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRefCount;    // 슬롯 사용 중 여부를 관리하는 참조 카운터 배열
	std::unique_ptr<PLock[]>							_slotLocks;    // 단독 락으로 교체

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;     // 슬롯별 재연결 워커 처리 진행 여부 플래그
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRetryFailCount;   // 슬롯별 연속 재연결 실패 횟수

	// 헬스체크 스레드 관련 멤버
	CThreadManager				_healthCheckThreadMgr;         // 풀 전체 연결 상태를 검사하는 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;             // 헬스체크 루프 중단을 요청하는 플래그 아토믹 변수
	int32						_nHealthCheckIntervalMs;       // 헬스체크 검사 주기 (밀리초)

	// 지연 예약 타이머 큐 관련 멤버
	CDelayedTaskQueue			_delayedTaskQueue;             // 백오프 대기 시간을 처리하기 위한 지연 타이머 큐
	CThreadManager				_delayedTaskThreadMgr;         // 지연 타이머 큐의 작업을 처리하는 전담 스레드 매니저

	// [수정 — O(n) 스캔 제거] 예전에는 PopFreeSlotIndex()가 매 호출마다
	// _nNextSlotHint로 회전 시작점만 바꿔가며 풀 전체를 라운드로빈으로
	// 스캔했다 — 슬롯 대부분이 사용 중이거나 끊긴 상태일수록(=풀이 바쁠수록)
	// 최악의 경우 O(n)이 되는 구조였다. "지금 비어 있고 연결된 것으로 확인된"
	// 슬롯 인덱스만 담아두는 프리 큐로 바꿔, 정상 상황에서는 O(1)에 가깝게
	// 슬롯을 획득한다. 기존 회전식 힌트(_nNextSlotHint)는 더 이상 쓰이지
	// 않아 제거했다.
	std::mutex					_freeSlotMutex;                // 프리 슬롯 큐 보호용 뮤텍스
	CQueue<int32>				_freeSlotQueue;                // 사용 가능(참조 0 & 연결됨)한 슬롯 인덱스 큐
	std::unique_ptr<CachePaddedAtomic<bool>[]>	_pInFreeSlotQueue; // [신규] 슬롯별 "현재 프리 큐에 이미 들어가 있는지" 플래그 — 중복 삽입(큐 무제한 증식) 방지

	// 재연결 워커 풀 관련 멤버
	CThreadManager				_reconnectWorkerMgr;           // 실제 I/O 재연결 및 스왑을 병렬 수행하는 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;        // 재연결 워커 전체 종료 신호 플래그
	std::atomic<int32>			_nCurrentWorkerCount;          // 현재 실제로 동작 중인 재연결 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;          // 런타임 설정으로 목표하는 재연결 워커 스레드 수

	std::mutex					_reconnectQueueMutex;          // 재연결 대기열 큐 보호용 뮤텍스
	std::condition_variable		_reconnectQueueCv;             // 재연결 대기열에 작업 추가를 알리는 조건 변수
	CQueue<int32>				_reconnectPendingSlots;        // 재연결 대상 슬롯 인덱스들을 보관하는 대기열 큐

	// 자원 격리(Quarantine) 관련 멤버
	PLock						_globalQuarantineLock;         // 격리 큐 보호용 단독 락
	CQueue<TQuarantineItem>		_quarantineQueue;              // 사용 중인 스레드가 빠져나오기를 기다리는 구형 커넥션 보관 큐
};

//***************************************************************************
// @class OdbcConnGuard
// @brief RAII 패턴 스마트 가드 클래스
// @details RAII 패턴을 사용하여 ODBC 커넥션 슬롯의 획득과 자동 반환을 보장합니다.
//***************************************************************************
class OdbcConnGuard
{
public:
	//***************************************************************************
	// @brief OdbcConnGuard 생성자
	// @details 커넥션 풀에서 사용 가능한 커넥션을 즉시 할당받습니다.
	// @param pPool 커넥션을 할당받을 COdbcConnPool 객체 포인터
	//***************************************************************************
	explicit OdbcConnGuard(COdbcConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			_pConn = _pPool->GetPooledConnUnsafe(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				_pPool->ReleaseOdbcConn(_nAllocatedIndex);
				_nAllocatedIndex = -1;
			}
		}
	}

	//***************************************************************************
	// @brief OdbcConnGuard 소멸자
	// @details 획득했던 커넥션 슬롯을 풀에 안전하게 반환합니다.
	//***************************************************************************
	~OdbcConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseOdbcConn(_nAllocatedIndex);
		}
	}

	//***************************************************************************
	// @brief 멤버 접근 역참조 연산자
	// @return 내부 커넥션 객체 포인터
	//***************************************************************************
	CBaseODBC* operator->() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief 원시 포인터 조회
	// @return 내부 커넥션 객체 포인터
	//***************************************************************************
	CBaseODBC* get() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief nullptr 비교 동등 연산자
	// @return 커넥션 포인터가 nullptr이면 true, 그렇지 않으면 false
	//***************************************************************************
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }

	//***************************************************************************
	// @brief nullptr 비교 부등 연산자
	// @return 커넥션 포인터가 nullptr가 아니면 true, 그렇지 않으면 false
	//***************************************************************************
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	OdbcConnGuard(const OdbcConnGuard&) = delete;
	OdbcConnGuard& operator=(const OdbcConnGuard&) = delete;

private:
	COdbcConnPool* _pPool;             // 커넥션이 소속된 풀 객체 포인터
	CBaseODBC* _pConn;             // 획득된 실제 데이터베이스 연결 객체 포인터
	int32			_nAllocatedIndex;   // 선점된 슬롯의 인덱스 번호
};

#endif // ndef UC_ODBCCONNPOOL_H