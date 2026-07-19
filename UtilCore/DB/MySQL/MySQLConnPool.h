//***************************************************************************
// MySQLConnPool.h : interface for the CMySQLConnPool class.
//
//***************************************************************************

#ifndef __MYSQLCONNPOOL_H__
#define __MYSQLCONNPOOL_H__

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

#ifndef	__BASEMYSQL_H__
#include <DB/MySQL/BaseMySQL.h>
#endif

#include <mutex>
#include <condition_variable>

class CMySQLConnPool
{
private:
	// =========================================================================
	// [TQuarantineItem]
	// 참조 중인 스레드가 있어서 즉시 삭제하지 못하고 임시 격리된 채 대기 중인 커넥션 정보 구조체
	// =========================================================================
	struct TQuarantineItem
	{
		CBaseMySQL* pConn;                 // 격리 대상이 된 오래된 MySQL 커넥션 포인터
		std::atomic<int32>* pRefCount;     // 해당 슬롯의 참조 카운트 주소 (0이 되는지 감시용)
		std::chrono::steady_clock::time_point enqueueTime; // 이 커넥션이 격리 큐에 들어온 시각 (요약 로그의 "가장 오래 갇힌 항목" 계산용)
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10; // 이보다 짧은 base는 재연결 폭주로 이어질 수 있어 거부

public:
	// =========================================================================
	// [TReconnectConfig]
	// 재연결 워커 개수와 백오프 정책을 운영 환경별로 조정할 수 있도록 외부에서 주입하는 설정값.
	// Init()의 초기 설정과 SetReconnectConfig()를 통한 런타임 변경 양쪽에 사용된다.
	// =========================================================================
	struct TReconnectConfig
	{
		int32	nWorkerCount = 4;     // 동시에 재연결 I/O를 수행할 워커 스레드 수
		int64	nBackoffBaseMs = 500;   // 최초 재시도 간격
		int64	nBackoffMaxMs = 30000; // 재시도 간격 상한 (30초)
		int32	nBackoffMaxShift = 6;     // 2^n 배까지만 증가 (오버플로 방지 겸 상한 역할)
		int32	nBackoffJitterMs = 250;   // 슬롯 간 재시도 타이밍이 겹치지 않도록 더하는 무작위 지터 상한
	};

	explicit CMySQLConnPool(int32 nMaxPoolSize);
	virtual ~CMySQLConnPool(void);

	// 풀을 초기화하고 지정된 접속 정보로 커넥션을 미리 생성하여 풀을 채운다.
	// reconnectConfig를 생략하면 기본값(워커 4개, 백오프 500ms~30s)으로 동작한다.
	// reconnectConfig가 유효성 검사를 통과하지 못하면 기본값으로 대체하고 에러를 로그로 남긴다.
	bool		Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd,
		const char* pszDBName, const uint32 uiPort,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});
	bool		Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd,
		const wchar_t* pwszDBName, const uint32 uiPort,
		const TReconnectConfig& reconnectConfig = TReconnectConfig{});

	// 참조를 증가시키고 특정 슬롯(nType)의 커넥션을 획득 및 반환한다.
	CBaseMySQL* GetMySQLConn(int32 nType);
	void		ReleaseMySQLConn(int32 nType);

	// PopFreeSlotIndex가 이미 예약(refcount>=1)해 놓은 슬롯의 커넥션 포인터를
	// 참조 카운트 변화 없이 조회한다. (MySQLConnGuard 전용)
	// 주의: 반드시 PopFreeSlotIndex로 슬롯을 보호한 상태에서만 호출해야 한다.
	CBaseMySQL* GetPooledConnUnsafe(int32 nType) const;

	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

	// 서비스 운영 중 재연결 백오프 정책과 워커 스레드 수를 동적으로 조정한다.
	// 유효성 검사를 통과하지 못한 값은 전부 무시하고 false를 반환한다 (부분 적용 없음, 전량 반영 또는 전량 거부).
	// 워커 수 변경은 목표치(_nDesiredWorkerCount)만 갱신하며, 실제 스레드 증감은 각 워커가
	// 스스로 판단해 수렴한다 (아래 TryExitIfExcess 참고).
	bool		SetReconnectConfig(const TReconnectConfig& reconnectConfig);

	// 현재 적용 중인 재연결 설정값의 스냅샷을 조회한다. (모니터링/디버깅용)
	TReconnectConfig GetReconnectConfig(void) const;

protected:
	// 풀 안에 있는 모든 커넥션을 안전하게 정리하고 해제한다. (Shutdown용)
	void		Clear(void);

	// 전달받은 슬롯 인덱스(nType)가 유효한 범위(0 ~ _nMaxPoolSize - 1) 안에 있는지 검사한다.
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }

	// TReconnectConfig 값이 상식적인 범위 안에 있는지 검사한다. (Init/SetReconnectConfig 공용)
	static bool	ValidateReconnectConfig(const TReconnectConfig& cfg);

	// _szDBHost 등 접속 정보 필드가 이미 채워진 상태에서 호출하는 공통 초기화 마무리 로직.
	// (char/wchar_t 두 Init 오버로드가 각자 접속 정보만 채운 뒤 이 함수로 합류한다)
	bool		FinishInit(const TReconnectConfig& reconnectConfig);

	// 새 커넥션을 만들어 실제로 연결까지 시도하는 순수 재연결 로직 (블로킹 I/O 포함)
	CBaseMySQL* TryReconnect(int32 nType);

	// 재연결 워커가 새 커넥션을 확보한 뒤, 슬롯 교체(Swap) 및 낡은 커넥션 격리/삭제까지 마무리하는 로직
	void		ApplyReconnectedConn(int32 nType, CBaseMySQL* pNewConn);

	// 슬롯별 백오프 상태 관리: 지금 시점에 재시도를 시작해도 되는지 검사한다.
	bool		IsRetryAllowed(int32 nType) const;
	// 재연결 실패 시 연속 실패 횟수를 늘리고, 다음 허용 시각을 현재 백오프 원자 변수들 기준으로 지수적으로 뒤로 미룬다.
	void		OnReconnectFailed(int32 nType);
	// 재연결 성공 시 백오프 상태를 초기화한다.
	void		OnReconnectSucceeded(int32 nType);
	static int64	NowMs(void);

	// 백그라운드 헬스체크 스레드가 도는 실시간 감시 루프 (빠른 스캔 + 재연결 디스패치만 담당, 블로킹 없음)
	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

	// 재연결 대기열에서 슬롯을 꺼내 실제 TryReconnect + 스왑을 수행하는 워커 루프.
	// 매 순회 시작 시 TryExitIfExcess()로 스스로 초과 인원인지 확인하여 축소 요청에 반응한다.
	void		ReconnectWorkerLoop(void);
	// nWorkerCount개의 워커 스레드로 재연결 워커 풀을 새로 기동한다. (Init 전용)
	void		StartReconnectWorkers(int32 nWorkerCount);
	// 모든 재연결 워커를 완전히 종료시킨다. (Shutdown/재초기화 전용)
	void		StopReconnectWorkers(void);
	// 목표 워커 수(_nDesiredWorkerCount)를 nNewCount로 갱신한다.
	void		SetWorkerCount(int32 nNewCount);
	// 이 워커 자신이 현재 초과 인원인지 CAS로 판정한다. 초과라면 _nCurrentWorkerCount를 스스로
	// 1 줄이고 true를 반환한다 (호출자는 즉시 스레드를 종료해야 한다).
	bool		TryExitIfExcess(void);
	void		EnqueueReconnect(int32 nType);

protected:
	// 복사 생성 및 대입 연산 금지 (싱글턴 혹은 유일 개체 소유를 위해)
	CMySQLConnPool(const CMySQLConnPool& rhs) = delete;
	CMySQLConnPool& operator=(const CMySQLConnPool& rhs) = delete;

	const int32								_nMaxPoolSize;                  // 커넥션 풀의 최대 크기 (용량 고정)

	// =========================================================================
	// [재연결 정책 - 필드별 개별 원자 변수]
	// 구조체 통째로 std::atomic<TReconnectConfig>로 감싸면 lock-free가 아니게 되어(내부 뮤텍스 폴백)
	// OnReconnectFailed 같은 핫패스에 숨은 락이 생기므로, 필드별로 쪼개서 각각 lock-free로 관리한다.
	// =========================================================================
	std::atomic<int64>			_nBackoffBaseMs;             // 최초 재시도 간격 (런타임 변경 가능)
	std::atomic<int64>			_nBackoffMaxMs;              // 재시도 간격 상한 (런타임 변경 가능)
	std::atomic<int32>			_nBackoffMaxShift;           // 지수 증가 상한 shift (런타임 변경 가능)
	std::atomic<int32>			_nBackoffJitterMs;           // 지터 상한 (런타임 변경 가능)

	// =========================================================================
	// [CRITICAL INVARIANT / 불변 조건]
	// 아래 unique_ptr 배열들은 반드시 생성자(Constructor)에서 단 1회만 할당되어야 한다.
	// 격리 대기 큐(_quarantineQueue)가 각 슬롯의 참조 카운트 주소(&_pRefCount[i].value)를 저장하므로,
	// Init() 도중을 포함하여 런타임 중에 절대 임의로든 재할당(make_unique 등)해서는 안 된다.
	//
	// CachePaddedAtomic<T>로 감싸 슬롯 하나당 캐시라인 하나를 점유하도록 강제했다. 서로 다른
	// 슬롯을 동시에 다루는 스레드들이 false sharing으로 서로의 캐시라인을 무효화시키는 것을 막는다.
	// =========================================================================
	std::unique_ptr<CachePaddedAtomic<CBaseMySQL*>[]>	_pMySQLConns; // 원자적으로 보호되는 실제 커넥션 포인터 배열
	std::unique_ptr<CachePaddedAtomic<int32>[]>		_pRefCount;   // 슬롯 사용중/이탈 여부를 원자적으로 관리하는 참조 카운터 배열 (가장 핫한 배열)

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
	std::chrono::steady_clock::time_point _quarantineLastSummaryLogTime; // 격리 큐 요약 경고를 마지막으로 남긴 시각.
	// HealthCheckLoop 스레드에서만 읽고 쓰므로 원자적일 필요가 없다.

private:
	char	_szDBHost[DATABASE_SERVER_NAME_STRLEN];         // 접속 대상 MySQL 호스트
	char	_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];       // 접속 계정
	char	_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN]; // 접속 비밀번호
	char	_szDBName[DATABASE_NAME_STRLEN];                // 접속 대상 데이터베이스 이름
	uint32	_uiPort;                                        // 접속 포트
};

class MySQLConnGuard
{
public:
	explicit MySQLConnGuard(CMySQLConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		// 1. 빈 슬롯 하나를 검색하고 원자적으로 즉시 슬롯 선점 확보 (참조 카운트 = 1)
		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			// 2. PopFreeSlotIndex가 이미 유효성을 확인했으므로, 참조 카운트를
			//    다시 증가시키지 않고 값만 조회한다. (참조 카운트는 1로 유지)
			_pConn = _pPool->GetPooledConnUnsafe(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				// 스왑 타이밍이나 레이스컨디션 등의 사유로 유효하다 판단했던 슬롯이 무효화된 경우
				// PopFreeSlotIndex가 잡은 카운트 1을 되돌려야 하므로 1회 해제
				_pPool->ReleaseMySQLConn(_nAllocatedIndex);
				_nAllocatedIndex = -1;
			}
		}
	}

	~MySQLConnGuard() noexcept
	{
		if( _pPool != nullptr && _nAllocatedIndex != -1 && _pConn != nullptr )
		{
			_pPool->ReleaseMySQLConn(_nAllocatedIndex);
		}
	}

	CBaseMySQL* operator->() const noexcept { return _pConn; }
	CBaseMySQL* get() const noexcept { return _pConn; }
	bool operator==(std::nullptr_t) const noexcept { return _pConn == nullptr; }
	bool operator!=(std::nullptr_t) const noexcept { return _pConn != nullptr; }

	MySQLConnGuard(const MySQLConnGuard&) = delete;
	MySQLConnGuard& operator=(const MySQLConnGuard&) = delete;

private:
	CMySQLConnPool* _pPool;
	CBaseMySQL* _pConn;
	int32			_nAllocatedIndex;
};

#endif // ndef __MYSQLCONNPOOL_H__