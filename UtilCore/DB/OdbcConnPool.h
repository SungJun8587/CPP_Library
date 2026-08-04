
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

#ifndef __DELAYEDTASKQUEUE_H__
#include <ThreadSafeContainers/DelayedTaskQueue.h>
#endif

#include <mutex>
#include <condition_variable>

class COdbcConnPool : public BaseAllocator
{
private:
	//***************************************************************************
	// @struct TQuarantineItem
	// @brief 참조 카운트가 남아 즉시 삭제하지 못하고 격리된 오래된 커넥션의 정보를 담는 구조체
	struct TQuarantineItem
	{
		CBaseODBC* pConn;                  // 격리 대상 커넥션 포인터
		std::atomic<int32>* pRefCount;     // 감시할 슬롯의 참조 카운트 주소
		std::chrono::steady_clock::time_point lastLogTime; // 과도한 체류 경고 로그를 출력하기 위한 마지막 시각
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10; // 백오프 base 하한 값

public:
	//***************************************************************************
	// @struct TReconnectConfig
	// @brief 재연결 워커 수 및 지수 백오프 정책을 정의하는 설정 구조체
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

	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	void		Clear(void);
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

protected:
	COdbcConnPool(const COdbcConnPool& rhs) = delete;
	COdbcConnPool& operator=(const COdbcConnPool& rhs) = delete;

	//-------------------------------------------------------------------------
	// 멤버 변수 정의
	//-------------------------------------------------------------------------
	EDBClass								_dbClass;                       // 대상 데이터베이스 종류
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
	std::unique_ptr<SpinLockDefault[]>					_slotLocks;    // 각 슬롯의 연결 교체 작업을 보호하는 개별 스핀락 배열

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;     // 슬롯별 재연결 워커 처리 진행 여부 플래그
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRetryFailCount;   // 슬롯별 연속 재연결 실패 횟수

	// 헬스체크 스레드 관련 멤버
	CThreadManager				_healthCheckThreadMgr;         // 풀 전체 연결 상태를 검사하는 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;             // 헬스체크 루프 중단을 요청하는 플래그 아토믹 변수
	int32						_nHealthCheckIntervalMs;       // 헬스체크 검사 주기 (밀리초)

	// 지연 예약 타이머 큐 관련 멤버
	CDelayedTaskQueue			_delayedTaskQueue;             // 백오프 대기 시간을 처리하기 위한 지연 타이머 큐
	CThreadManager				_delayedTaskThreadMgr;         // 지연 타이머 큐의 작업을 처리하는 전담 스레드 매니저

	std::atomic<uint32>			_nNextSlotHint;                // 슬롯 탐지 시 경합을 분산하기 위한 회전식 시작 인덱스 힌트

	// 재연결 워커 풀 관련 멤버
	CThreadManager				_reconnectWorkerMgr;           // 실제 I/O 재연결 및 스왑을 병렬 수행하는 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;        // 재연결 워커 전체 종료 신호 플래그
	std::atomic<int32>			_nCurrentWorkerCount;          // 현재 실제로 동작 중인 재연결 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;          // 런타임 설정으로 목표하는 재연결 워커 스레드 수

	std::mutex					_reconnectQueueMutex;          // 재연결 대기열 큐 보호용 뮤텍스
	std::condition_variable		_reconnectQueueCv;			   // 재연결 대기열에 작업 추가를 알리는 조건 변수
	CQueue<int32>				_reconnectPendingSlots;		   // 재연결 대상 슬롯 인덱스들을 보관하는 대기열 큐

	// 자원 격리(Quarantine) 관련 멤버
	SpinLockDefault				_globalQuarantineLock;         // 격리 큐 보호용 전용 스핀락
	CQueue<TQuarantineItem>		_quarantineQueue;              // 사용 중인 스레드가 빠져나오기를 기다리는 구형 커넥션 보관 큐
};

//***************************************************************************
// @class OdbcConnGuard
// @brief RAII 패턴을 사용하여 ODBC 커넥션 슬롯의 획득과 자동 반환을 보장하는 스마트 가드 클래스
class OdbcConnGuard
{
public:
	//***************************************************************************
	// @brief OdbcConnGuard 생성자입니다. 커넥션 풀에서 사용 가능한 커넥션을 즉시 할당받습니다.
	// @param pPool 커넥션을 할당받을 COdbcConnPool 객체 포인터
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
	// @brief OdbcConnGuard 소멸자입니다. 획득했던 커넥션 슬롯을 풀에 안전하게 반환합니다.
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
	COdbcConnPool*	_pPool;             // 커넥션이 소속된 풀 객체 포인터
	CBaseODBC*		_pConn;             // 획득된 실제 데이터베이스 연결 객체 포인터
	int32			_nAllocatedIndex;   // 선점된 슬롯의 인덱스 번호
};

#endif // ndef __ODBCCONNPOOL_H__