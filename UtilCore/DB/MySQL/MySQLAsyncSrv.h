
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef __MYSQLASYNCSRV__H__
#define __MYSQLASYNCSRV__H__

class CMySQLAsyncSrv
{
	typedef std::map<UINT16, shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	CMySQLAsyncSrv();
	virtual ~CMySQLAsyncSrv();

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
	bool InitMySQL(CVector<CDBNode> DBNodeVec, INT32 nMaxThreadCnt);

	void StartIoThreads(INT32 nMaxThreadCnt);
	bool Action();

	CMySQLConnPool*	GetAccountOdbcConnPool(void);
	CMySQLConnPool*	GetMySQLConnPool(uint64 m_nID);
	CMySQLConnPool*	GetLogOdbcConnPool();

	std::shared_mutex	_mutex;									// ť lock
	std::queue<st_DBAsyncRq*>			_queueDBAsyncRq;		// DB ��û ����ü ť
	COMMAND_MAP							_mapCommand;			// �� �ڵ鷯

	int32								_nDBCount;				// ������ Database ����
	bool								_bOpen;					// DB ���� ����
	int32								_nMaxThreadCnt;			// �ִ� ������ ����
	bool								_bStopThread;			// ������ ���� �÷���
	CMySQLConnPool**					_pMySQLConnPools;		// DB Connection Pool

public:
	static std::shared_ptr<CMySQLAsyncSrv> Instance();

protected:
	void		Clear(void);
};

static std::shared_ptr<CMySQLAsyncSrv> dbAsyncSrv_ = 0;

inline std::shared_ptr<CMySQLAsyncSrv> CMySQLAsyncSrv::Instance() {
	if( !dbAsyncSrv_ )
		dbAsyncSrv_ = std::make_shared<CMySQLAsyncSrv>();

	return dbAsyncSrv_;
}

#endif // ndef __MYSQLASYNCSRV__H__

