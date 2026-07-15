
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

#ifndef __SPINLOCK_H__
#include <Thread/SpinLock.h>
#endif

#ifndef __THREADMANAGER_H__
#include <Thread/ThreadManager.h>
#endif

#ifndef	__BASEMYSQL_H__
#include <DB/MySQL/BaseMySQL.h>
#endif

class CMySQLConnPool
{
private:
	struct TQuarantineItem
	{
		CBaseMySQL* pConn;
		std::atomic<int32>* pRefCount;
		std::chrono::steady_clock::time_point lastLogTime;
	};

public:
	explicit CMySQLConnPool(int32 nMaxPoolSize);
	virtual ~CMySQLConnPool(void);

	bool		Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort);
	bool		Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort);

	CBaseMySQL* GetMySQLConn(int32 nType);
	void		ReleaseMySQLConn(int32 nType);

protected:
	void		Clear(void);
	bool		IsValidIndex(int32 nType) const { return nType >= 0 && nType < _nMaxPoolSize; }

	CBaseMySQL* TryReconnect(int32 nType);
	void		HealthCheckLoop(void);
	void		StartHealthCheckThread(void);
	void		StopHealthCheckThread(void);

protected:
	CMySQLConnPool(const CMySQLConnPool& rhs) = delete;
	CMySQLConnPool& operator=(const CMySQLConnPool& rhs) = delete;

	const int32								_nMaxPoolSize;

	// =========================================================================
	// [CRITICAL INVARIANT / 불변 조건]
	// 아래 3개의 unique_ptr 배열은 오직 생성자(Constructor)에서 단 1회만 할당되어야 합니다.
	// 지연 격리 큐(_quarantineQueue)가 각 원소의 원자적 주소(&_pRefCount[i])를 참조하므로,
	// Init() 내부를 포함하여 런타임 중에 어떠한 이유로든 재할당해서는 안 됩니다.
	// =========================================================================
	std::unique_ptr<std::atomic<CBaseMySQL*>[]>	_pMySQLConns;
	std::unique_ptr<std::atomic<int32>[]>		_pRefCount;

	// 고성능 SpinLockDefault 배열 동적 생성 (alignas(kCacheLineSize)로 False Sharing 차단)
	std::unique_ptr<SpinLockDefault[]>			_slotLocks;

	CThreadManager				_healthCheckThreadMgr;
	std::atomic<bool>			_bStopHealthCheck;
	int32						_nHealthCheckIntervalMs;

	// 격리 큐 보호용 스핀락 및 대기 큐
	SpinLockDefault				_globalQuarantineLock;
	std::queue<TQuarantineItem>	_quarantineQueue;

private:
	char	_szDBHost[DATABASE_SERVER_NAME_STRLEN];
	char	_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	char	_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	char	_szDBName[DATABASE_NAME_STRLEN];
	uint32	_uiPort;
};

#endif // ndef __MYSQLCONNPOOL_H__