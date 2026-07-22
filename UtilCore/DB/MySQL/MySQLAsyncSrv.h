
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef __MYSQLASYNCSRV__H__
#define __MYSQLASYNCSRV__H__

class CMySQLAsyncSrv
{
	// 매 DB 비동기 호출마다(핫패스) 조회되므로 O(log n) 트리 탐색인 std::map 대신
	// O(1) 평균 탐색인 std::unordered_map을 사용한다. Regist()는 보통 시작 시 한 번만
	// 호출되고 이후엔 읽기 전용으로만 쓰이므로 컨테이너 교체에 따른 동시성 이슈는 없다.
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

	int GetQueryQueueSize() {
		std::lock_guard<std::mutex> lockGuard(_mutex); return static_cast<int>(_queueDBAsyncRq.size());
	}
	bool IsEmpty() {
		std::lock_guard<std::mutex> lockGuard(_mutex); return _queueDBAsyncRq.empty();
	}
	int Push(st_DBAsyncRq* pAsyncRq);
	st_DBAsyncRq* Pop();

	bool StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitMySQL(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();			// 대기 중인 모든 워커 스레드를 깨워 종료 조건을 확인시킴
	};

	CMySQLConnPool* GetAccountConnPool(void);
	CMySQLConnPool* GetMySQLConnPool(uint64 m_nID);
	CMySQLConnPool* GetLogConnPool();

	CQueue<st_DBAsyncRq*>				_queueDBAsyncRq;		// DB 요청 구조체 큐
	COMMAND_MAP							_mapCommand;			// 명령어별 핸들러

	int32								_nDBCount;				// 연결된 Database 개수
	bool								_bOpen;					// DB 서비스 오픈 여부
	int32								_nMaxThreadCnt;			// 최대 DB 비동기 워커 스레드 수 (= 각 CMySQLConnPool의 nMaxPoolSize로도 사용)
	CMySQLConnPool** _pMySQLConnPools;		// DB별 Connection Pool 배열

public:
	static std::shared_ptr<CMySQLAsyncSrv> Instance();

protected:
	void		Clear(void);				// DB 요청 큐 정리
	void		ClearMySQLConnPools(void);	// _pMySQLConnPools 배열의 각 풀을 안전하게 해제 (Init 실패/소멸자 공용)

private:
	std::atomic<bool>			_bStopThread;					// 스레드 종료 플래그
	// [성능] Push/Pop은 매 DB 요청마다 항상 배타적으로(unique_lock) 잠그는 핫패스이고,
	// 공유 잠금(shared_lock)은 모니터링용 GetQueryQueueSize/IsEmpty에서만 드물게 쓰인다.
	// shared_mutex는 배타적 잠금 자체도 std::mutex보다 무겁고, condition_variable_any도
	// 락 타입 소거 오버헤드가 있어 condition_variable보다 느리므로 표준 mutex/condition_variable로 교체한다.
	std::mutex					_mutex;							// 큐 lock
	std::condition_variable	_cva;							// 큐 대기용 조건 변수
};

#endif // ndef __MYSQLASYNCSRV__H__