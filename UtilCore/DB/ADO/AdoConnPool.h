
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

#include <mutex>
#include <condition_variable>

class CAdoConnPool : public BaseAllocator
{
private:
	struct TQuarantineItem
	{
		CAdoDB* pConn;
		std::atomic<int32>* pRefCount;
		std::chrono::steady_clock::time_point lastLogTime;
	};

	static constexpr int64 RECONNECT_BACKOFF_MIN_MS = 10;

public:
	struct TReconnectConfig
	{
		int32	nWorkerCount = 4;
		int64	nBackoffBaseMs = 500;
		int64	nBackoffMaxMs = 30000;
		int32	nBackoffMaxShift = 6;
		int32	nBackoffJitterMs = 250;
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

	bool		IsRetryAllowed(int32 nType) const;
	void		OnReconnectFailed(int32 nType);
	void		OnReconnectSucceeded(int32 nType);
	static int64	NowMs(void);

	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

	void		ReconnectWorkerLoop(void);
	void		StartReconnectWorkers(int32 nWorkerCount);
	void		StopReconnectWorkers(void);

	void		SetWorkerCount(int32 nNewCount);
	bool		TryExitIfExcess(void);
	void		EnqueueReconnect(int32 nType);

protected:
	CAdoConnPool(const CAdoConnPool& rhs) = delete;
	CAdoConnPool& operator=(const CAdoConnPool& rhs) = delete;

	EDBClass								_dbClass;
	TCHAR									_tszConnStr[512];       // ADO 접속 문자열
	int										_nTimeOut;              // 커넥션 타임아웃
	const int32								_nMaxPoolSize;

	std::atomic<int64>			_nBackoffBaseMs;
	std::atomic<int64>			_nBackoffMaxMs;
	std::atomic<int32>			_nBackoffMaxShift;
	std::atomic<int32>			_nBackoffJitterMs;

	std::unique_ptr<CachePaddedAtomic<CAdoDB*>[]>		_pAdoConns;
	std::unique_ptr<CachePaddedAtomic<int32>[]>		_pRefCount;
	std::unique_ptr<SpinLockDefault[]>			_slotLocks;

	std::unique_ptr<CachePaddedAtomic<bool>[]>			_pReconnecting;
	std::unique_ptr<CachePaddedAtomic<int64>[]>		_pNextRetryAllowedMs;
	std::unique_ptr<CachePaddedAtomic<int32>[]>		_pRetryFailCount;

	CThreadManager				_healthCheckThreadMgr;
	std::atomic<bool>			_bStopHealthCheck;
	int32						_nHealthCheckIntervalMs;

	std::atomic<uint32>		_nNextSlotHint;

	CThreadManager				_reconnectWorkerMgr;
	std::atomic<bool>			_bStopReconnectWorkers;
	std::atomic<int32>			_nCurrentWorkerCount;
	std::atomic<int32>			_nDesiredWorkerCount;

	std::mutex					_reconnectQueueMutex;
	std::condition_variable	_reconnectQueueCv;
	std::queue<int32>			_reconnectPendingSlots;

	SpinLockDefault				_globalQuarantineLock;
	std::queue<TQuarantineItem>	_quarantineQueue;
};

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
	CAdoConnPool* _pPool;
	CAdoDB* _pConn;
	int32			_nAllocatedIndex;
};

#endif // ndef __ADOCONNPOOL_H__