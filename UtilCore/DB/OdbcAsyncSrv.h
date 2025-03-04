
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef __ODBCASYNCSRV__H__
#define __ODBCASYNCSRV__H__

class COdbcAsyncSrv
{
	typedef std::map<uint16, shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

	virtual bool	RunningThread();

	shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, shared_ptr<CDBAsyncSrvHandler> const handler);

	int GetQueryQueueSize() {
		std::shared_lock lockGuard(_mutex); return static_cast<int>(_queueDBAsyncRq.size());
	}
	bool IsEmpty() {
		std::shared_lock lockGuard(_mutex); return _queueDBAsyncRq.empty();
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop();

	bool StartService(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitOdbc(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();
	void StopThread() {
		_bStopThread.store(true);	// 플래그를 설정하여 루프 종료 
		_cva.notify_all();			// 모든 대기 스레드를 깨움
	};

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	std::queue<st_DBAsyncRq*>			_queueDBAsyncRq;		// DB 요청 구조체 큐
	COMMAND_MAP							_mapCommand;			// 맵 핸들러

	int32								_nDBCount;				// 접속할 Database 개수
	bool								_bOpen;					// DB 오픈 여부
	int32								_nMaxThreadCnt;			// 최대 쓰레드 개수
	COdbcConnPool**						_pOdbcConnPools;		// DB Connection Pool

public:
	static std::shared_ptr<COdbcAsyncSrv> Instance();

protected:
	void		Clear(void);

private:
	std::atomic<bool>			_bStopThread;					// 스레드 종료 플래그
	std::shared_mutex			_mutex;							// 큐 lock
	std::condition_variable_any _cva;							// shared_mutex와 함께 사용하는 조건 변수
};

static std::shared_ptr<COdbcAsyncSrv> OdbcAsyncSrv_ = 0;

inline std::shared_ptr<COdbcAsyncSrv> COdbcAsyncSrv::Instance() {
	if( !OdbcAsyncSrv_ )
		OdbcAsyncSrv_ = std::make_shared<COdbcAsyncSrv>();

	return OdbcAsyncSrv_;
}

#endif // ndef __ODBCASYNCSRV__H__

