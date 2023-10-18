
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef __ODBCASYNCSRV__H__
#define __ODBCASYNCSRV__H__

class COdbcAsyncSrv : public CThread
{
	typedef std::map<UINT16, CDBAsyncSrvHandler*>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

	virtual bool	RunningThread(const __int32& nThreadIdx);

	CDBAsyncSrvHandler* Regist(const BYTE command, CDBAsyncSrvHandler* const handler);

	int GetQueryQueueSize() {
		CLockGuard<CCriticalSection> lockGuard(lock_queueDBAsyncRq_); return static_cast<int>(queueDBAsyncRq_.size());
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop();

	bool StartService(INT32 nMaxThreadCnt, std::vector<CDBNode> DBNodeVec);
	bool Action();

	bool InitOdbc(INT32 nMaxThreadCnt, std::vector<CDBNode> DBNodeVec);

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	bool	m_bOpen;

	CCriticalSection						lock_queueDBAsyncRq_;	// 큐 lock
	std::queue<st_DBAsyncRq*>				queueDBAsyncRq_;		// DB 요청 구조체 큐
	COMMAND_MAP								mapCommand_;			// 맵 핸들러

	COdbcConnPool** m_pOdbcConnPools;		// db connection
	int32									m_nMaxThreadCnt;		// 최대 쓰레드 개수
	int32									m_nDBCount;				// 접속할 Database 개수 (UserDB, GameDB)

public:
	static COdbcAsyncSrv* Instance();
};

static COdbcAsyncSrv* OdbcAsyncSrv_ = 0;

inline COdbcAsyncSrv* COdbcAsyncSrv::Instance() {
	if( !OdbcAsyncSrv_ )
		OdbcAsyncSrv_ = new COdbcAsyncSrv();

	return OdbcAsyncSrv_;
}

#endif // ndef __ODBCASYNCSRV__H__

