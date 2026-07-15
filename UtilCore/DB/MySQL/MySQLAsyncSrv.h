
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef __MYSQLASYNCSRV__H__
#define __MYSQLASYNCSRV__H__

class CMySQLAsyncSrv
{
	typedef std::map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	CMySQLAsyncSrv();
	virtual ~CMySQLAsyncSrv();

	virtual bool	RunningThread();

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	int GetQueryQueueSize() {
		std::shared_lock<std::shared_mutex> lockGuard(_mutex); return static_cast<int>(_queueDBAsyncRq.size());
	}
	bool IsEmpty() {
		std::shared_lock<std::shared_mutex> lockGuard(_mutex); return _queueDBAsyncRq.empty();
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop();

	bool StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitMySQL(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();			// 대기 중인 모든 워커 스레드를 일깨워 즉시 안전 종료 유도
	};

	CMySQLConnPool* GetAccountConnPool(void);
	CMySQLConnPool* GetMySQLConnPool(uint64 m_nID);
	CMySQLConnPool* GetLogConnPool();

	CQueue<st_DBAsyncRq*>				_queueDBAsyncRq;		// DB 요청 구조체 큐
	COMMAND_MAP							_mapCommand;			// 맵 핸들러

	int32								_nDBCount;				// 접속할 Database 개수
	bool								_bOpen;					// DB 오픈 여부
	int32								_nMaxThreadCnt;			// 최대 쓰레드 개수
	CMySQLConnPool** _pMySQLConnPools;		// MySQL Connection Pool

public:
	static std::shared_ptr<CMySQLAsyncSrv> Instance();

protected:
	void		Clear(void);

private:
	std::atomic<bool>			_bStopThread;					// 스레드 종료 플래그
	std::shared_mutex			_mutex;							// 큐 동기화 Lock
	std::condition_variable_any _cva;							// shared_mutex 호환 조건 변수
};

#endif // ndef __MYSQLASYNCSRV__H__