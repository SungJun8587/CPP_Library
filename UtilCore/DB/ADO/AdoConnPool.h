
//***************************************************************************
// AdoConnPool.h : interface for the CAdoConnPool class.
// 
//***************************************************************************

#ifndef __ADOCONNPOOL_H__
#define __ADOCONNPOOL_H__

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

#ifndef __ADODB_H__
#include <DB/AdoDB.h>
#endif

#ifndef __DELAYEDTASKQUEUE_H__
#include <ThreadSafeContainers/DelayedTaskQueue.h>
#endif

#include <mutex>
#include <condition_variable>

class CAdoConnPool : public BaseAllocator
{
private:
	//***************************************************************************
	// @struct TQuarantineItem
	// @brief 참조 카운트가 남아 즉시 삭제하지 못하고 격리된 오래된 커넥션의 정보를 담는 구조체
	struct TQuarantineItem
	{
		CAdoDB* pConn;                  // 격리된 커넥션 객체 포인터
		std::atomic<int32>* pRefCount;  // 해당 슬롯의 참조 카운트 변수 포인터
		std::chrono::steady_clock::time_point lastLogTime; // 마지막으로 경고 로그를 출력한 시각
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10;	// 백오프 base 하한 값

public:
	//***************************************************************************
	// @struct TReconnectConfig
	// @brief 재연결 워커 수 및 지수 백오프 정책을 정의하는 설정 구조체
	struct TReconnectConfig
	{
		int32	nWorkerCount = 4;           // 재시도 작업을 처리할 백그라운드 워커 스레드 수
		int64	nBackoffBaseMs = 500;       // 지연 백오프 기본 단위 시간 (밀리초)
		int64	nBackoffMaxMs = 30000;      // 지연 백오프 최대 대기 시간 상한선 (밀리초)
		int32	nBackoffMaxShift = 6;       // 백오프 지수 연산(Shift)의 최대 횟수
		int32	nBackoffJitterMs = 250;     // 재시도 타이밍 쏠림 방지를 위한 무작위 지연(Jitter) 범위 (밀리초)
	};

	explicit CAdoConnPool(int32 nMaxPoolSize);
	virtual ~CAdoConnPool(void);

	bool		Init(const EDBClass dbClass, const TCHAR* ptszConnStr, const int nTimeOut,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});

	CAdoDB* GetAdoConn(int32 nType);
	void		ReleaseAdoConn(int32 nType);
	CAdoDB* GetPooledConnUnsafe(int32 nType) const;

	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	void		Clear(void);
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	CAdoDB* TryReconnect(int32 nType);
	void		ApplyReconnectedConn(int32 nType, CAdoDB* pNewConn);

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
	CAdoConnPool(const CAdoConnPool& rhs) = delete;
	CAdoConnPool& operator=(const CAdoConnPool& rhs) = delete;

	//-------------------------------------------------------------------------
	// 멤버 변수 정의
	//-------------------------------------------------------------------------
	EDBClass								_dbClass;               // DB 종류 (MSSQL 등)
	TCHAR									_tszConnStr[512];       // ADO 접속 문자열
	int										_nTimeOut;              // 커넥션 타임아웃
	const int32								_nMaxPoolSize;          // 커넥션 풀 최대 크기

	// 재연결 백오프 정책 관련 아토믹 변수들
	std::atomic<int64>			_nBackoffBaseMs;            // 백오프 기본 단위 (밀리초)
	std::atomic<int64>			_nBackoffMaxMs;             // 백오프 최대 대기 시간 (밀리초)
	std::atomic<int32>			_nBackoffMaxShift;          // 지수 백오프 최대 시프트 횟수
	std::atomic<int32>			_nBackoffJitterMs;          // 지연 Jitter 무작위 범위 (밀리초)

	// 동적 메모리 및 캐시라인 정렬 슬롯 배열
	std::unique_ptr<CachePaddedAtomic<CAdoDB*>[]>		_pAdoConns;     // 커넥션 객체 포인터 배열 (캐시 라인 패딩 적용)
	std::unique_ptr<CachePaddedAtomic<int32>[] >		_pRefCount;     // 각 슬롯별 현재 대여 중인 참조 카운트 배열
	std::unique_ptr<SpinLockDefault[]>					_slotLocks;             // 슬롯별 독립 락 배열

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;         // 슬롯별 재연결 진행 여부 플래그 배열
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRetryFailCount;       // 슬롯별 연속 재시도 실패 횟수 배열

	// 헬스체크 스레드 관련 멤버
	CThreadManager				_healthCheckThreadMgr;      // 헬스체크 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;          // 헬스체크 중지 플래그
	int32						_nHealthCheckIntervalMs;    // 헬스체크 주기 (밀리초)

	// 지연 예약 타이머 큐 관련 멤버
	CDelayedTaskQueue			_delayedTaskQueue;          // 백오프 지연 재시도 작업 큐
	CThreadManager				_delayedTaskThreadMgr;      // 지연 작업 처리 스레드 매니저

	std::atomic<uint32>			_nNextSlotHint;             // 라운드 로빈 슬롯 탐색을 위한 힌트 값

	// 재연결 워커 풀 관련 멤버
	CThreadManager				_reconnectWorkerMgr;        // 재접속 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;     // 재접속 워커 중지 플래그
	std::atomic<int32>			_nCurrentWorkerCount;       // 현재 활성화된 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;       // 목표 워커 스레드 수

	std::mutex					_reconnectQueueMutex;       // 재접속 대기열 접근 동기화 뮤텍스
	std::condition_variable		_reconnectQueueCv;          // 재접속 대기열 조건 변수
	std::queue<int32>			_reconnectPendingSlots;     // 재접속 대기 중인 슬롯 인덱스 큐

	// 자원 격리(Quarantine) 관련 멤버
	SpinLockDefault				_globalQuarantineLock;      // 격리 큐 접근 동기화 스핀락
	std::queue<TQuarantineItem>	_quarantineQueue;           // 좀비 커넥션 격리 큐
};

//***************************************************************************
// @class AdoConnGuard
// @brief RAII 패턴을 사용하여 ADO 커넥션 슬롯의 획득과 자동 반환을 보장하는 스마트 가드 클래스
class AdoConnGuard
{
public:
	explicit AdoConnGuard(CAdoConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			_pConn = _pPool->GetPooledConnUnsafe(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				_pPool->ReleaseAdoConn(_nAllocatedIndex);
				_nAllocatedIndex = -1;
			}
		}
	}

	~AdoConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseAdoConn(_nAllocatedIndex);
		}
	}

	CAdoDB* operator->() const noexcept { return _pConn; }
	CAdoDB* get() const noexcept { return _pConn; }
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	AdoConnGuard(const AdoConnGuard&) = delete;
	AdoConnGuard& operator=(const AdoConnGuard&) = delete;

private:
	CAdoConnPool*	_pPool;				// 관리 대상 커넥션 풀 포인터
	CAdoDB*			_pConn;             // 대여된 ADO 커넥션 포인터
	int32			_nAllocatedIndex;   // 할당받은 풀 슬롯 인덱스
};

#endif // ndef __ADOCONNPOOL_H__