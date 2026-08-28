
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef UC_ODBCASYNCSRV_H
#define UC_ODBCASYNCSRV_H

#include <DB/OdbcConnPool.h>
#include <Containers/Queue/ChunkedSwapQueue.h>

//***************************************************************************
// @brief 비동기 ODBC 데이터베이스 서비스 클래스
// @details 비동기 DB 요청 큐를 관리하고 커넥션 풀을 통해 스레드 세이프하게 쿼리를 처리합니다.
//***************************************************************************
class COdbcAsyncSrv
{
	typedef std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	//***************************************************************************
	// @brief 경고 임계값 상수 정의
	//***************************************************************************
	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,
	};

public:
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

	virtual bool	RunningThread();

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	//***************************************************************************
	// @brief 대기 중인 쿼리 큐의 크기 조회
	// @return 현재 큐에 쌓여있는 요청 수
	//***************************************************************************
	int GetQueryQueueSize() const {
		return static_cast<int>(_queueDBAsyncRq.GetSize());
	}

	//***************************************************************************
	// @brief 큐가 비어 있는지 확인
	// @return 큐가 비어 있으면 true, 그렇지 않으면 false
	//***************************************************************************
	bool IsEmpty() const {
		return _queueDBAsyncRq.IsEmpty();
	}

	int Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq);
	std::unique_ptr<st_DBAsyncRq> Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue);

	//***************************************************************************
	// @brief 백프레셔 처리: 지정한 최대 용량을 초과할 경우 큐 삽입 대기
	// @param maxCapacity 큐의 최대 허용 용량
	//***************************************************************************
	void WaitPushCapacity(size_t maxCapacity) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cvProducer.wait(lock, [this, maxCapacity]() {
			return static_cast<size_t>(_queueDBAsyncRq.GetSize()) < maxCapacity || _bStopThread.load();
			});
	}

	//***************************************************************************
	// @brief 현재 처리 중인 잔여 요청 수 조회
	// @return 처리 대기 및 실행 중인 총 요청 수
	//***************************************************************************
	int32 GetOutstandingRequests() const {
		return _nOutstandingRequests.load(std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 처리 중인 요청 수 1 증가
	//***************************************************************************
	void AddOutstandingRequest() {
		_nOutstandingRequests.fetch_add(1, std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 처리 중인 요청 수 1 감소
	//***************************************************************************
	void SubOutstandingRequest() {
		_nOutstandingRequests.fetch_sub(1, std::memory_order_relaxed);
	}

	bool StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitOdbc(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();

	//***************************************************************************
	// @brief 스레드 중단 플래그를 설정하고 대기 중인 조건 변수 깨움
	//***************************************************************************
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();
		_cvProducer.notify_all();
	};

	COdbcConnPool* GetAccountOdbcConnPool(void);
	COdbcConnPool* GetOdbcConnPool(uint64 m_nID);
	COdbcConnPool* GetLogOdbcConnPool();

	CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>	_queueDBAsyncRq;			// 비동기 요청 큐	
	COMMAND_MAP											_mapCommand;				// 명령 핸들러 맵

	int32								_nDBCount;					// DB 개수
	bool								_bOpen;						// 서비스 오픈 여부
	int32								_nMaxThreadCnt;				// 최대 스레드 수
	COdbcConnPool**						_pOdbcConnPools;			// ODBC 연결 풀 배열

public:
	static std::shared_ptr<COdbcAsyncSrv> Instance();

protected:
	void		Clear(void);
	void		FlushRemainingTasks();
	void		ClearOdbcPools(void);

private:
	std::atomic<bool>			_bStopThread;						// 스레드 중단 플래그
	std::atomic<int32>			_nOutstandingRequests{ 0 };			// 처리 중 요청 수
	int							_nLastWarnedQueueSize{ 2 };			// 마지막 경고 큐 크기

	std::mutex					_mutex;								// 동기화용 뮤텍스				
	std::condition_variable		_cva;								// 소비자 대기 조건 변수
	std::condition_variable		_cvProducer;						// 생산자 대기 조건 변수
};

#endif // ndef UC_ODBCASYNCSRV_H