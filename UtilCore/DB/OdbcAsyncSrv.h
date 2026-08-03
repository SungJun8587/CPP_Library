//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef __ODBCASYNCSRV__H__
#define __ODBCASYNCSRV__H__

#ifndef __ODBCCONNPOOL_H__
#include <DB/OdbcConnPool.h>
#endif

#ifndef __SWAPQUEUE_H__
#include <ThreadSafeContainers/SwapQueue.h>
#endif

class COdbcAsyncSrv
{
	typedef std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

	virtual bool	RunningThread();

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	// CSwapQueue의 원자적 카운터를 사용하여 락 없이 빠름
	int GetQueryQueueSize() const {
		return static_cast<int>(_queueDBAsyncRq.GetSize());
	}
	bool IsEmpty() const {
		return _queueDBAsyncRq.IsEmpty();
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop(std::queue<st_DBAsyncRq*>& localQueue);

	// [Back-pressure] CSwapQueue 크기 기준으로 대기
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
	bool InitOdbc(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();
		_cvProducer.notify_all();
	};

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	CSwapQueue<st_DBAsyncRq*>			_queueDBAsyncRq;		
	COMMAND_MAP							_mapCommand;

	int32								_nDBCount;
	bool								_bOpen;
	int32								_nMaxThreadCnt;
	COdbcConnPool**						_pOdbcConnPools;

public:
	static std::shared_ptr<COdbcAsyncSrv> Instance();

protected:
	void		Clear(void);
	void		FlushRemainingTasks();
	void		ClearOdbcPools(void);

private:
	std::atomic<bool>			_bStopThread;
	std::atomic<int32>			_nOutstandingRequests{ 0 };
	int							_nLastWarnedQueueSize{ 2 };

	std::mutex					_mutex;							
	std::condition_variable		_cva;
	std::condition_variable		_cvProducer;
};

#endif // ndef __ODBCASYNCSRV__H__