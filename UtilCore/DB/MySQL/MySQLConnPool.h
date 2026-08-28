
//***************************************************************************
// MySQLConnPool.h : interface for the CMySQLConnPool class.
//
//***************************************************************************

#ifndef UC_MYSQLCONNPOOL_H
#define UC_MYSQLCONNPOOL_H

#include <Memory/Allocator.h>
#include <Memory/Containers.h>
#include <Thread/CacheAlignment.h>
#include <Thread/PlatformLock.h>
#include <Thread/ThreadManager.h>
#include <DB/MySQL/BaseMySQL.h>
#include <Containers/Queue/DelayedTaskQueue.h>

//***************************************************************************
// @brief MySQL 커넥션 풀 및 자동 재연결/헬스체크 관리 클래스
// @detail 데이터베이스 연결을 고정된 크기의 슬롯으로 관리하며, 
//         자동 재연결, 백오프 정책, 헬스체크 및 RAII 가드를 지원합니다.
//***************************************************************************
class CMySQLConnPool : public BaseAllocator
{
private:
	//***************************************************************************
	// @brief 참조 카운트가 남아 즉시 삭제하지 못하고 격리된 오래된 커넥션 정보 구조체
	// @detail 사용 중인 스레드가 빠져나올 때까지 커넥션 해제를 지연 보관하기 위해 사용됩니다.
	//***************************************************************************
	struct TQuarantineItem
	{
		CBaseMySQL* pConn;                                  // 격리 대상 커넥션 포인터
		std::atomic<int32>* pRefCount;                      // 감시할 슬롯의 참조 카운트 주소
		std::chrono::steady_clock::time_point lastLogTime; // 과도한 체류 경고 로그를 출력하기 위한 마지막 시각
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10; // 백오프 base 하한 값

public:
	//***************************************************************************
	// @brief 재연결 워커 수 및 지수 백오프 정책 설정 구조체
	// @detail 백그라운드 재연결 워커 개수와 대기 시간 상한선, 지터(Jitter) 등의 설정을 포함합니다.
	//***************************************************************************
	struct TReconnectConfig
	{
		int32	nWorkerCount = 2;			// 재연결을 전담하는 백그라운드 워커 스레드 수
		int64	nBackoffBaseMs = 500;		// 최초 재시도 대기 기본 시간 (밀리초)
		int64	nBackoffMaxMs = 30000;		// 재시도 대기 시간 상한선 (밀리초)
		int32	nBackoffMaxShift = 6;		// 백오프 지수 증가 횟수 상한
		int32	nBackoffJitterMs = 250;		// 동시 재시도 충돌 방지를 위한 지터 최대값 (밀리초)
	};

	explicit CMySQLConnPool(int32 nMaxPoolSize);
	virtual ~CMySQLConnPool(void);

	bool		Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});
	bool		Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});

	CBaseMySQL* GetMySQLConn(int32 nType);
	void		ReleaseMySQLConn(int32 nType);
	CBaseMySQL* GetPooledConnUnsafe(int32 nType) const;

	//***************************************************************************
	// @brief 커넥션 풀의 고정된 최대 슬롯 크기를 반환합니다.
	// @return int32 커넥션 풀 최대 크기
	//***************************************************************************
	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	void		Clear(void);

	//***************************************************************************
	// @brief 지정한 인덱스가 풀의 유효한 슬롯 범위 내에 있는지 확인합니다.
	// @param nType 검사할 슬롯 인덱스
	// @return bool 유효한 인덱스이면 true, 그렇지 않으면 false
	//***************************************************************************
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	bool		FinishInit(const TReconnectConfig& reconnectConfig);

	CBaseMySQL* TryReconnect(int32 nType);
	void		ApplyReconnectedConn(int32 nType, CBaseMySQL* pNewConn);

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
	CMySQLConnPool(const CMySQLConnPool& rhs) = delete;
	CMySQLConnPool& operator=(const CMySQLConnPool& rhs) = delete;

	char									_szDBHost[DATABASE_SERVER_NAME_STRLEN];           // 데이터베이스 서버 호스트 주소
	char									_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];         // 데이터베이스 사용자 ID
	char									_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];   // 데이터베이스 사용자 비밀번호
	char									_szDBName[DATABASE_NAME_STRLEN];                  // 데이터베이스 이름
	uint32									_uiPort;                                          // 데이터베이스 포트 번호
	const int32								_nMaxPoolSize;                                    // 커넥션 풀의 고정 최대 슬롯 크기

	std::atomic<int64>			_nBackoffBaseMs;                                              // 현재 설정된 백오프 기본 대기 시간 (ms)
	std::atomic<int64>			_nBackoffMaxMs;                                               // 현재 설정된 백오프 최대 대기 시간 상한 (ms)
	std::atomic<int32>			_nBackoffMaxShift;                                            // 백오프 지수 계산 시 최대 시프트 횟수
	std::atomic<int32>			_nBackoffJitterMs;                                            // 백오프 지터 최대 범위 (ms)

	std::unique_ptr<CachePaddedAtomic<CBaseMySQL*>[]>	_pMySQLConns;                         // 슬롯별 실제 데이터베이스 연결 객체 포인터 배열
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRefCount;                           // 슬롯 사용 중 여부를 관리하는 참조 카운터 배열
	std::unique_ptr<PLock[]>							_slotLocks;                           // 단독 락 배열

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;                       // 슬롯별 재연결 워커 처리 진행 여부 플래그
	std::unique_ptr<CachePaddedAtomic<int32>[]>			_pRetryFailCount;                     // 슬롯별 연속 재연결 실패 횟수

	CThreadManager				_healthCheckThreadMgr;                                        // 풀 전체 연결 상태를 검사하는 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;                                            // 헬스체크 루프 중단을 요청하는 플래그 아토믹 변수
	int32						_nHealthCheckIntervalMs;                                      // 헬스체크 검사 주기 (밀리초)

	CDelayedTaskQueue			_delayedTaskQueue;                                            // 백오프 대기 시간을 처리하기 위한 지연 타이머 큐
	CThreadManager				_delayedTaskThreadMgr;                                        // 지연 타이머 큐의 작업을 처리하는 전담 스레드 매니저

	std::atomic<uint32>			_nNextSlotHint;                                               // 슬롯 탐지 시 경합을 분산하기 위한 회전식 시작 인덱스 힌트

	CThreadManager				_reconnectWorkerMgr;                                          // 실제 I/O 재연결 및 스왑을 병렬 수행하는 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;                                       // 재연결 워커 전체 종료 신호 플래그
	std::atomic<int32>			_nCurrentWorkerCount;                                         // 현재 실제로 동작 중인 재연결 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;                                         // 런타임 설정으로 목표하는 재연결 워커 스레드 수

	std::mutex					_reconnectQueueMutex;                                         // 재연결 대기열 큐 보호용 뮤텍스
	std::condition_variable		_reconnectQueueCv;                                            // 재연결 대기열에 작업 추가를 알리는 조건 변수
	CQueue<int32>				_reconnectPendingSlots;                                       // 재연결 대상 슬롯 인덱스들을 보관하는 대기열 큐

	PLock						_globalQuarantineLock;                                        // 격리 큐 보호용 단독 락
	CQueue<TQuarantineItem>		_quarantineQueue;                                             // 사용 중인 스레드가 빠져나오기를 기다리는 구형 커넥션 보관 큐
};

//***************************************************************************
// @brief RAII 패턴을 사용하여 MySQL 커넥션 슬롯의 획득과 자동 반환을 보장하는 스마트 가드 클래스
// @detail 객체 생성 시 커넥션 풀에서 슬롯을 할당받고, 소멸 시 커넥션을 자동으로 풀에 반환합니다.
//***************************************************************************
class MySQLConnGuard
{
public:
	//***************************************************************************
	// @brief MySQLConnGuard 생성자입니다. 커넥션 풀에서 사용 가능한 커넥션을 즉시 할당받습니다.
	// @param pPool 커넥션을 할당받을 CMySQLConnPool 객체 포인터
	//***************************************************************************
	explicit MySQLConnGuard(CMySQLConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			_pConn = _pPool->GetPooledConnUnsafe(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				_pPool->ReleaseMySQLConn(_nAllocatedIndex);
				_nAllocatedIndex = -1;
			}
		}
	}

	//***************************************************************************
	// @brief MySQLConnGuard 소멸자입니다. 획득했던 커넥션 슬롯을 풀에 안전하게 반환합니다.
	//***************************************************************************
	~MySQLConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseMySQLConn(_nAllocatedIndex);
		}
	}

	//***************************************************************************
	// @brief 내부 CBaseMySQL 포인터에 접근하기 위한 멤버 접근 연산자입니다.
	// @return CBaseMySQL* 할당받은 커넥션 객체 포인터
	//***************************************************************************
	CBaseMySQL* operator->() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief 내부 CBaseMySQL 포인터를 직접 반환합니다.
	// @return CBaseMySQL* 할당받은 커넥션 객체 포인터
	//***************************************************************************
	CBaseMySQL* get() const noexcept { return _pConn; }

	//***************************************************************************
	// @brief 커넥션 포인터가 nullptr인지 비교 연산을 수행합니다.
	// @return bool 커넥션 포인터가 nullptr이면 true, 아니면 false
	//***************************************************************************
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }

	//***************************************************************************
	// @brief 커넥션 포인터가 nullptr이 아닌지 비교 연산을 수행합니다.
	// @return bool 커넥션 포인터가 nullptr이 아니면 true, 맞으면 false
	//***************************************************************************
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	MySQLConnGuard(const MySQLConnGuard&) = delete;
	MySQLConnGuard& operator=(const MySQLConnGuard&) = delete;

private:
	CMySQLConnPool* _pPool;           // 커넥션이 소속된 풀 객체 포인터
	CBaseMySQL* _pConn;           // 획득된 실제 데이터베이스 연결 객체 포인터
	int32           _nAllocatedIndex; // 선점된 슬롯의 인덱스 번호
};

#endif // ndef UC_MYSQLCONNPOOL_H