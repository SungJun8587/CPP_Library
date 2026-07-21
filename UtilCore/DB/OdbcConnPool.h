
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
		std::atomic<int32>* pRefCount;     // 감시할 참조 카운트 주소
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

	// [불변 조건] 아래 배열들은 생성자에서 단 1회만 할당한다.
	// 격리 큐가 &_pRefCount[i] 주소를 저장하므로 런타임 재할당 금지.
	std::unique_ptr<std::atomic<CBaseODBC*>[]>	_pOdbcConns; // 슬롯별 커넥션 포인터
	std::unique_ptr<std::atomic<int32>[]>		_pRefCount;  // 슬롯별 참조 카운트

	std::unique_ptr<SpinLockDefault[]>			_slotLocks;  // 슬롯별 교체 보호 락 (false sharing 방지 정렬)

	std::unique_ptr<std::atomic<bool>[]>		_pReconnecting;       // 슬롯별 재연결 진행 중 여부 (중복 디스패치 방지)
	std::unique_ptr<std::atomic<int64>[]>		_pNextRetryAllowedMs; // 슬롯별 다음 재시도 허용 시각
	std::unique_ptr<std::atomic<int32>[]>		_pRetryFailCount;     // 슬롯별 연속 실패 횟수

	CThreadManager				_healthCheckThreadMgr;       // 헬스체크 스레드 매니저
	std::atomic<bool>			_bStopHealthCheck;           // 헬스체크 중단 신호
	int32						_nHealthCheckIntervalMs;     // 헬스체크 주기 (기본 500ms)

	std::atomic<uint32>		_nNextSlotHint;              // PopFreeSlotIndex 탐색 시작 힌트

	CThreadManager				_reconnectWorkerMgr;         // 재연결 워커 스레드 매니저
	std::atomic<bool>			_bStopReconnectWorkers;      // 워커 전체 종료 신호
	std::atomic<int32>			_nCurrentWorkerCount;        // 현재 워커 스레드 수
	std::atomic<int32>			_nDesiredWorkerCount;        // 목표 워커 스레드 수

	std::mutex					_reconnectQueueMutex;        // 재연결 대기열 보호 뮤텍스
	std::condition_variable	_reconnectQueueCv;           // 대기열 작업/워커 수 변경 알림
	std::queue<int32>			_reconnectPendingSlots;      // 재연결 대기 슬롯 인덱스 큐

	SpinLockDefault				_globalQuarantineLock;       // 격리 큐 보호 락
	std::queue<TQuarantineItem>	_quarantineQueue;            // 지연 삭제 대기 커넥션 큐
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