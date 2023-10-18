
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef __MYSQLASYNCSRV__H__
#define __MYSQLASYNCSRV__H__

class CMySQLAsyncSrv : public CThread
{
	typedef std::map<UINT16, CDBAsyncSrvHandler*>	COMMAND_MAP;

	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	CMySQLAsyncSrv();
	virtual ~CMySQLAsyncSrv();

	virtual bool	RunningThread(const __int32& nThreadIdx);

	CDBAsyncSrvHandler* Regist(const BYTE command, CDBAsyncSrvHandler* const handler);

	int GetQueryQueueSize() {
		CLockGuard<CCriticalSection> lockGuard(lock_queueDBAsyncRq_); return static_cast<int>(queueDBAsyncRq_.size());
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop();

	bool StartService(INT32 nMaxThreadCnt, std::vector<DBNode> DBNodeVec);
	bool Action();

	bool InitMySQL(INT32 nMaxThreadCnt, std::vector<DBNode> DBNodeVec);

	CMySQLConnPool* GetMySQLConnPool(uint64 m_nID);

	bool	m_bOpen;

	CCriticalSection						lock_queueDBAsyncRq_;	// ť lock
	std::queue<st_DBAsyncRq*>				queueDBAsyncRq_;		// DB ��û ����ü ť
	COMMAND_MAP								mapCommand_;			// �� �ڵ鷯

	CMySQLConnPool** m_pMySQLConnPools;		// db connection
	int32									m_nMaxThreadCnt;		// �ִ� ������ ����
	int32									m_nDBCount;				// ������ Database ���� (UserDB, GameDB)

public:
	static CMySQLAsyncSrv* Instance();
};

static CMySQLAsyncSrv* dbasyncSrv_ = 0;

inline CMySQLAsyncSrv* CMySQLAsyncSrv::Instance() {
	if( !dbasyncSrv_ )
		dbasyncSrv_ = new CMySQLAsyncSrv();

	return dbasyncSrv_;
}

#endif // ndef __MYSQLASYNCSRV__H__

