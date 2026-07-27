
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef __ODBCASYNCSRV__H__
#define __ODBCASYNCSRV__H__

class COdbcAsyncSrv
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
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

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

	// [Back-pressure] 큐 크기가 지정한 최대 용량(maxCapacity)을 초과할 경우, 공간이 생길 때까지 대기한다.
	// Pop()이 항목을 꺼낼 때마다 _cvProducer를 깨워주므로, 워커가 소비하는 속도를 넘어서는
	// 프로듀서를 여기서 자연스럽게 막아 큐가 무한정 커지는 것을 방지한다.
	void WaitPushCapacity(size_t maxCapacity) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cvProducer.wait(lock, [this, maxCapacity]() {
			return _queueDBAsyncRq.size() < maxCapacity || _bStopThread.load();
		});
	}

	// 외부에서 미완료 요청 수를 안전하게 조회하거나 조작할 수 있는 인터페이스
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
		_cva.notify_all();			// 대기 중인 모든 워커 스레드를 깨워 종료 조건을 확인시킴
		_cvProducer.notify_all();	// 대기 중인 모든 생산자 스레드도 깨워 종료시킴
	};

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	CQueue<st_DBAsyncRq*>				_queueDBAsyncRq;		// DB 요청 구조체 큐
	COMMAND_MAP							_mapCommand;			// 명령어별 핸들러

	int32								_nDBCount;				// 연결된 Database 개수
	bool								_bOpen;					// DB 서비스 오픈 여부
	int32								_nMaxThreadCnt;			// 최대 DB 비동기 워커 스레드 수 (= 각 COdbcConnPool의 nMaxPoolSize로도 사용)
	COdbcConnPool**						_pOdbcConnPools;		// DB별 Connection Pool 배열

public:
	static std::shared_ptr<COdbcAsyncSrv> Instance();

protected:
	void		Clear(void);			// DB 요청 큐 정리
	void		FlushRemainingTasks();	// 프로그램 종료 전 큐에 남은 작업을 마저 처리하는 함수
	void		ClearOdbcPools(void);	// _pOdbcConnPools 배열의 각 풀을 안전하게 해제 (Init 실패/소멸자 공용)

private:
	std::atomic<bool>			_bStopThread;					// 스레드 종료 플래그
	std::atomic<int32>          _nOutstandingRequests{ 0 };		// 미완료 요청 수 카운터 캡슐화

	std::mutex					_mutex;							// 큐 lock
	std::condition_variable		_cva;							// 워커 대기용 조건 변수
	std::condition_variable		_cvProducer;					// [Back-pressure] 큐 공간 부족으로 대기 중인 생산자용 조건 변수
};

#endif // ndef __ODBCASYNCSRV__H__