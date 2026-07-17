
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

class COdbcConnPool
{
private:
	// =========================================================================
	// [TQuarantineItem]
	// 참조 중인 스레드가 있어 즉시 삭제하지 못하고 임시 격리해 둔 좀비 커넥션 정보 구조체
	// =========================================================================
	struct TQuarantineItem
	{
		CBaseODBC* pConn;                  // 삭제 대기 중인 ODBC 커넥션 포인터
		std::atomic<int32>* pRefCount;     // 대상 슬롯의 참조 카운터 주소 (0이 되는지 감시용)
		std::chrono::steady_clock::time_point lastLogTime; // 좀비 누수 경고 로그 스팸 방지용 타임스탬프
	};

public:
	explicit COdbcConnPool(int32 nMaxPoolSize);
	virtual ~COdbcConnPool(void);

	// 풀을 초기화하고 지정된 DSN으로 커넥션을 미리 생성하여 풀을 채웁니다.
	bool		Init(const EDBClass dbClass, const TCHAR* ptszDSN);

	// 스레드 안전하게 특정 슬롯(nType)의 커넥션을 획득 및 반환합니다.
	CBaseODBC*	GetOdbcConn(int32 nType);
	void		ReleaseOdbcConn(int32 nType);

	int32		GetMaxPoolSize(void) const { return _nMaxPoolSize; }
	int32		PopFreeSlotIndex(void);

protected:
	// 풀 내부의 모든 커넥션을 안전하게 해제하고 정리합니다. (Shutdown용)
	void		Clear(void);

	// 전달받은 슬롯 인덱스(nType)가 유효한 범위(0 ~ _nMaxPoolSize - 1) 내에 있는지 검증합니다.
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }

	// 백그라운드 스레드에서 락 영역 외부에서 안전하게 재연결을 시도하는 헬퍼 함수입니다.
	CBaseODBC* TryReconnect(int32 nType);

	// 백그라운드 헬스체크 스레드가 실행할 실시간 감시 및 커넥션 복구 루프입니다.
	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

protected:
	// 복사 생성 및 대입 연산 차단 (싱글톤 혹은 유일 객체 상태 유지)
	COdbcConnPool(const COdbcConnPool& rhs) = delete;
	COdbcConnPool& operator=(const COdbcConnPool& rhs) = delete;

	EDBClass								_dbClass;                       // 대상 데이터베이스 타입 (MSSQL, MySQL 등)
	TCHAR									_tszDSN[DATABASE_DSN_STRLEN];   // DB 접속 경로 (Data Source Name)
	const int32								_nMaxPoolSize;                  // 커넥션 풀의 고정 크기 (용량 제한)

	// =========================================================================
	// [CRITICAL INVARIANT / 불변 조건]
	// 아래 3개의 unique_ptr 배열은 오직 생성자(Constructor)에서 단 1회만 할당되어야 합니다.
	// 지연 격리 큐(_quarantineQueue)가 각 원소의 원자적 주소(&_pRefCount[i])를 참조하므로,
	// Init() 내부를 포함하여 런타임 중에 어떠한 이유로든 재할당(make_unique 등)해서는 안 됩니다.
	// =========================================================================
	std::unique_ptr<std::atomic<CBaseODBC*>[]>	_pOdbcConns; // 원자적으로 보호되는 실제 커넥션 포인터 배열
	std::unique_ptr<std::atomic<int32>[]>		_pRefCount;  // 개별 슬롯의 진입/이탈 스레드 수를 추적하는 참조 카운터 배열

	// 캐시 라인 정렬(alignas) 구조의 SpinLockDefault를 채택하여 인접 슬롯 간의 캐시 충돌을 차단합니다.
	std::unique_ptr<SpinLockDefault[]>			_slotLocks;  // 각 커넥션 슬롯 교체 시 경합을 방지하기 위한 개별 스핀락 배열

	CThreadManager				_healthCheckThreadMgr;       // 백그라운드 감시 스레드를 관리하는 매니저
	std::atomic<bool>			_bStopHealthCheck;           // 헬스체크 루프 중단 신호 플래그
	int32						_nHealthCheckIntervalMs;     // 헬스체크 주기 (기본값 500ms)

	SpinLockDefault				_globalQuarantineLock;       // 격리 큐에 대한 동시 접근을 보호하는 스핀락
	std::queue<TQuarantineItem>	_quarantineQueue;            // 참조 카운트가 남은 좀비 커넥션들이 해제를 위해 대기하는 큐
};

class OdbcConnGuard
{
public:
	explicit OdbcConnGuard(COdbcConnPool* pPool)
		: _pPool(pPool), _pConn(nullptr), _nAllocatedIndex(-1)
	{
		if( _pPool == nullptr ) return;

		// 1. 락 영역 내부 검증을 통과하고 안전하게 선점된 슬롯 번호 획득 (참조 카운트 = 1)
		_nAllocatedIndex = _pPool->PopFreeSlotIndex();

		if( _nAllocatedIndex != -1 )
		{
			// 2. [질문하신 부분 적용] 기존의 안전한 풀 표준 함수 사용 (참조 카운트 = 2가 됨)
			_pConn = _pPool->GetOdbcConn(_nAllocatedIndex);

			if( _pConn == nullptr )
			{
				// GetOdbcConn 실패 시 내부에서 카운트를 1 깎았을 것이므로, 
				// PopFreeSlotIndex가 올려둔 카운트 1마저 마저 깎아서 0으로 복원
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
	COdbcConnPool*	_pPool;
	CBaseODBC*		_pConn;
	int32			_nAllocatedIndex;
};

#endif // ndef __ODBCCONNPOOL_H__