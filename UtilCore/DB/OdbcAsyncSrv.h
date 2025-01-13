
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef __ODBCASYNCSRV__H__
#define __ODBCASYNCSRV__H__

class COdbcAsyncSrv
{
	typedef std::map<UINT16, shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

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

	bool StartService(CVector<CDBNode> DBNodeVec, INT32 nMaxThreadCnt = 0);
	bool InitOdbc(CVector<CDBNode> DBNodeVec, INT32 nMaxThreadCnt);

	void StartIoThreads(INT32 nMaxThreadCnt);
	bool Action();
	void StopThread() {
		_bStopThread.store(true);	// �÷��׸� �����Ͽ� ���� ���� 
	};

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	std::shared_mutex	_mutex;									// ť lock
	std::queue<st_DBAsyncRq*>			_queueDBAsyncRq;		// DB ��û ����ü ť
	COMMAND_MAP							_mapCommand;			// �� �ڵ鷯

	int32								_nDBCount;				// ������ Database ����
	bool								_bOpen;					// DB ���� ����
	int32								_nMaxThreadCnt;			// �ִ� ������ ����
	COdbcConnPool**						_pOdbcConnPools;		// DB Connection Pool

public:
	static std::shared_ptr<COdbcAsyncSrv> Instance();

protected:
	void		Clear(void);

private:
	std::atomic<bool> _bStopThread;								// ������ ���� �÷���
};

static std::shared_ptr<COdbcAsyncSrv> OdbcAsyncSrv_ = 0;

inline std::shared_ptr<COdbcAsyncSrv> COdbcAsyncSrv::Instance() {
	if( !OdbcAsyncSrv_ )
		OdbcAsyncSrv_ = std::make_shared<COdbcAsyncSrv>();

	return OdbcAsyncSrv_;
}

#endif // ndef __ODBCASYNCSRV__H__

