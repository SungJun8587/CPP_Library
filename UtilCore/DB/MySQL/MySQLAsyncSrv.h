
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef __MYSQLASYNCSRV__H__
#define __MYSQLASYNCSRV__H__

#ifndef	__MYSQLCONNPOOL_H__
#include <DB/MySQL/MySQLConnPool.h>
#endif

#ifndef __SWAPQUEUE_H__
#include <ThreadSafeContainers/SwapQueue.h>
#endif

class CMySQLAsyncSrv
{
	typedef std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	CMySQLAsyncSrv();
	virtual ~CMySQLAsyncSrv();

	virtual bool	RunningThread();

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	int GetQueryQueueSize() const {
		return static_cast<int>(_queueDBAsyncRq.GetSize());
	}
	bool IsEmpty() const {
		return _queueDBAsyncRq.IsEmpty();
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop(std::queue<st_DBAsyncRq*>& localQueue);

	void WaitPushCapacity(size_t maxCapacity) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cvProducer.wait(lock, [this, maxCapacity]() {
			return static_cast<size_t>(_queueDBAsyncRq.GetSize()) < maxCapacity || _bStopThread.load();
			});
	}

	int32 GetOutstandingRequests() const {
		return _nOutstandingRequests.load(std::memory_order_relaxed);
	}

	void AddOutstandingRequest() {
		_nOutstandingRequests.fetch_add(1, std::memory_order_relaxed);
	}

	void SubOutstandingRequest() {
		_nOutstandingRequests.fetch_sub(1, std::memory_order_relaxed);
	}

	bool StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitMySQL(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();
		_cvProducer.notify_all();
	};

	CMySQLConnPool* GetAccountConnPool(void);
	CMySQLConnPool* GetMySQLConnPool(uint64 m_nID);
	CMySQLConnPool* GetLogConnPool();

	CSwapQueue<st_DBAsyncRq*>			_queueDBAsyncRq;
	COMMAND_MAP							_mapCommand;

	int32								_nDBCount;
	bool								_bOpen;
	int32								_nMaxThreadCnt;
	CMySQLConnPool** _pMySQLConnPools;

public:
	static std::shared_ptr<CMySQLAsyncSrv> Instance();

protected:
	void		Clear(void);
	void		FlushRemainingTasks();
	void		ClearMySQLConnPools(void);

private:
	std::atomic<bool>			_bStopThread;
	std::atomic<int32>          _nOutstandingRequests{ 0 };
	int							_nLastWarnedQueueSize{ 2 };

	std::mutex					_mutex;
	std::condition_variable		_cva;
	std::condition_variable		_cvProducer;
};

#endif // ndef __MYSQLASYNCSRV__H__