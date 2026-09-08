
//***************************************************************************
// AdoConnPool.h : interface for the CAdoConnPool class.
// 
//***************************************************************************

#ifndef UC_ADOCONNPOOL_H
#define UC_ADOCONNPOOL_H

#include <Memory/Allocator.h>
#include <Memory/Containers.h>
#include <Thread/CacheAlignment.h>
#include <Thread/PlatformLock.h>
#include <Thread/ThreadManager.h>
#include <DB/ADO/AdoDB.h>
#include <Containers/Queue/DelayedTaskQueue.h>

//***************************************************************************
// @brief ADO 커넥션 풀을 관리하는 클래스
// @detail 데이터베이스 커넥션의 생명주기, 자동 재연결, 헬스체크,
//         지수 백오프 기반 타이머 재시도 및 자원 격리를 관리합니다.
//***************************************************************************
class CAdoConnPool : public BaseAllocator
{
private:
	//***************************************************************************
	// @brief 참조 카운트가 남아 즉시 삭제하지 못하고 격리된 오래된 커넥션 정보 구조체
	// @detail 백그라운드에서 참조 카운트가 0이 되는 시점에 안전하게 해제하기 위한 정보를 담습니다.
	//***************************************************************************
	struct TQuarantineItem
	{
		CAdoDB* pConn;                             // 격리된 커넥션 객체 포인터
		std::atomic<int32>* pRefCount;             // 해당 슬롯의 참조 카운트 변수 포인터
		// [수정 — 강제 정리 타임아웃이 절대 안 걸리던 버그] 이전에는 이
		// lastLogTime 한 필드를 "격리 큐에 들어온 이후 전체 경과 시간"과
		// "마지막 경고 로그 시각" 두 용도로 같이 썼다. HealthCheckLoop가
		// 5분마다 경고를 남길 때마다 이 필드를 now로 리셋했는데, 그러면
		// FORCE_CLEANUP_TIMEOUT_MS(10분) 판정에 쓰는 경과 시간도 함께
		// 리셋되어 참조 카운트가 절대 안 풀리는 좀비 커넥션이 강제
		// 정리되지 못하고 무한정 격리 큐에 남아있게 됐다(COdbcConnPool과
		// 동일한 버그 — 그쪽 수정과 동일한 방식으로 분리한다). enqueueTime은
		// 최초 격리된 시각으로 절대 갱신되지 않고, lastLogTime만 경고
		// 쿨다운 판단용으로 갱신된다.
		std::chrono::steady_clock::time_point enqueueTime; // 격리 큐에 들어온 시각(강제 정리 타임아웃 판단용, 절대 갱신되지 않음)
		std::chrono::steady_clock::time_point lastLogTime; // 마지막으로 경고 로그를 출력한 시각
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10;	// 백오프 base 하한 값

public:
	//***************************************************************************
	// @brief 재연결 워커 수 및 지수 백오프 정책 설정 구조체
	// @detail 재시도 동작 시 사용할 스레드 수 및 타이밍(Backoff, Jitter 등)을 설정합니다.
	//***************************************************************************
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

	//***************************************************************************
	// @brief 커넥션 풀의 최대 크기를 반환합니다.
	// @return int32 설정된 커넥션 풀의 최대 슬롯 개수
	//***************************************************************************
	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	void		Clear(void);

	//***************************************************************************
	// @brief 지정한 슬롯 인덱스가 유효한 범위 내에 있는지 검사합니다.
	// @param nType 검사할 커넥션 슬롯 인덱스
	// @return bool 인덱스가 유효하면 true, 그렇지 않으면 false
	//***************************************************************************
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	CAdoDB* TryReconnect(int32 nType);

	//***************************************************************************
	// @brief 재접속된 새로운 커넥션을 해당 슬롯에 반영하고 기존 커넥션을
	//        안전하게 교체 또는 격리합니다.
	// @details [수정 — 반환값 추가] COdbcConnPool과 동일한 버그 — 이전에는
	// 반환값이 없어, 진입 시점에 refcount>0이라 스왑을 포기하고 pNewConn을
	// 그냥 버린 경우(정상적인 레이스)와 실제로 스왑까지 마친 경우를
	// 호출부(ReconnectWorkerLoop)가 구분할 수 없었다. 그 결과 스왑을
	// 포기한 경우에도 호출부가 무조건 OnReconnectSucceeded()를 불러
	// 백오프 실패 카운트를 리셋하는 오정보 상태가 생겼다. 실제 스왑
	// 여부를 bool로 반환해 호출부가 성공/실패를 정확히 구분하게 한다.
	// @return 새 커넥션을 슬롯에 실제로 적용했으면 true, refcount 경합으로
	//         포기(pNewConn 폐기)했으면 false.
	//***************************************************************************
	bool		ApplyReconnectedConn(int32 nType, CAdoDB* pNewConn);

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
	// @details [이식 — COdbcConnPool과 동일] Init()/ReleaseAdoConn()/
	// ApplyReconnectedConn() 세 곳에서 "이 슬롯이 이제 사용 가능해졌다"는
	// 이벤트가 발생할 때마다 호출한다. _pInFreeSlotQueue로 "이 슬롯이
	// 지금 큐 안에 있는지"를 추적해, 이미 큐에 있으면 추가로 넣지 않는다.
	//***************************************************************************
	void		EnqueueFreeSlot(int32 nType);

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
	std::unique_ptr<PLock[]>							_slotLocks;     // 단독 락으로 교체

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;         // 슬롯별 재연결 진행 여부 플래그 배열
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRetryFailCount;       // 슬롯별 연속 재시도 실패 횟수 배열

	// 헬스체크 스레드 관련 멤버
	CThreadManager				_healthCheckThreadMgr;      // 헬스체크 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;          // 헬스체크 중지 플래그
	int32						_nHealthCheckIntervalMs;    // 헬스체크 주기 (밀리초)

	// 지연 예약 타이머 큐 관련 멤버
	CDelayedTaskQueue			_delayedTaskQueue;          // 백오프 지연 재시도 작업 큐
	CThreadManager				_delayedTaskThreadMgr;      // 지연 작업 처리 스레드 매니저

	// [수정 — O(n) 스캔 제거] 예전에는 PopFreeSlotIndex()가 매 호출마다
	// _nNextSlotHint로 회전 시작점만 바꿔가며 풀 전체를 라운드로빈으로
	// 스캔했다 — 슬롯 대부분이 사용 중이거나 끊긴 상태일수록(=풀이 바쁠수록)
	// 최악의 경우 O(n)이 되는 구조였다(COdbcConnPool에 먼저 적용한 것과
	// 동일한 개선을 이식). "지금 비어 있고 연결된 것으로 확인된" 슬롯
	// 인덱스만 담아두는 프리 큐로 바꿔, 정상 상황에서는 O(1)에 가깝게
	// 슬롯을 획득한다. 기존 회전식 힌트(_nNextSlotHint)는 더 이상 쓰이지
	// 않아 제거했다.
	std::mutex					_freeSlotMutex;                // 프리 슬롯 큐 보호용 뮤텍스
	CQueue<int32>				_freeSlotQueue;                // 사용 가능(참조 0 & 연결됨)한 슬롯 인덱스 큐
	std::unique_ptr<CachePaddedAtomic<bool>[]>	_pInFreeSlotQueue; // 슬롯별 "현재 프리 큐에 이미 들어가 있는지" 플래그 — 중복 삽입 방지

	// 재연결 워커 풀 관련 멤버
	CThreadManager				_reconnectWorkerMgr;        // 재접속 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;     // 재접속 워커 중지 플래그
	std::atomic<int32>			_nCurrentWorkerCount;       // 현재 활성화된 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;       // 목표 워커 스레드 수

	std::mutex					_reconnectQueueMutex;       // 재접속 대기열 접근 동기화 뮤텍스
	std::condition_variable		_reconnectQueueCv;          // 재접속 대기열 조건 변수
	CQueue<int32>				_reconnectPendingSlots;     // 재접속 대기 중인 슬롯 인덱스 큐

	// 자원 격리(Quarantine) 관련 멤버
	PLock						_globalQuarantineLock;      // 격리 큐 보호용 단독 락
	CQueue<TQuarantineItem>		_quarantineQueue;           // 좀비 커넥션 격리 큐
};

//***************************************************************************
// @brief RAII 패턴 기반 ADO 커넥션 가드 클래스
// @detail 커넥션 풀에서 커넥션 슬롯을 안전하게 할당받고, 스코프를 벗어날 때 자동으로 반환합니다.
//***************************************************************************
class AdoConnGuard
{
public:
	//***************************************************************************
	// @brief AdoConnGuard 객체를 생성하고 대상 풀에서 사용 가능한 커넥션을 자동으로 할당받습니다.
	// @param pPool 할당을 요청할 CAdoConnPool 객체의 포인터
	//***************************************************************************
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

	//***************************************************************************
	// @brief AdoConnGuard 소멸자로, 대여했던 커넥션 슬롯을 커넥션 풀에 자동으로 반환합니다.
	//***************************************************************************
	~AdoConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseAdoConn(_nAllocatedIndex);
		}
	}

	//***************************************************************************
	// @brief 관리 중인 CAdoDB 포인터에 접근하기 위한 멤버 접근 연산자
	// @return CAdoDB* 관리 중인 CAdoDB 객체 포인터
	//***************************************************************************
	CAdoDB* operator->() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief 관리 중인 CAdoDB 포인터 원본을 반환합니다.
	// @return CAdoDB* 관리 중인 CAdoDB 객체 포인터
	//***************************************************************************
	CAdoDB* get() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief 커넥션 포인터가 nullptr인지 확인합니다.
	// @param nullptr
	// @return bool 포인터가 nullptr이면 true, 아니면 false
	//***************************************************************************
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }

	//***************************************************************************
	// @brief 커넥션 포인터가 유효한지(nullptr가 아닌지) 확인합니다.
	// @param nullptr
	// @return bool 포인터가 nullptr가 아니면 true, 맞으면 false
	//***************************************************************************
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	AdoConnGuard(const AdoConnGuard&) = delete;
	AdoConnGuard& operator=(const AdoConnGuard&) = delete;

private:
	CAdoConnPool* _pPool;             // 관리 대상 커넥션 풀 포인터
	CAdoDB* _pConn;             // 대여된 ADO 커넥션 포인터
	int32			_nAllocatedIndex;   // 할당받은 풀 슬롯 인덱스
};

#endif // ndef UC_ADOCONNPOOL_H