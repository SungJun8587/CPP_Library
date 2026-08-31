
//***************************************************************************
// MySQLAsyncSrv.h : interface for the CMySQLAsyncSrv class.
//
//***************************************************************************

#ifndef UC_MYSQLASYNCSRV_H
#define UC_MYSQLASYNCSRV_H

#include <DB/MySQL/MySQLConnPool.h>
#include <Containers/Queue/ChunkedSwapQueue.h>

//***************************************************************************
// @brief MySQL 비동기 DB 요청 처리 및 스레드 풀 관리 서비스 클래스
// @detail 데이터베이스 요청을 큐에 쌓아 비동기 스레드에서 처리하며,
//         비동기 핸들러 맵 관리 및 MySQL 커넥션 풀을 제공합니다.
//***************************************************************************
class CMySQLAsyncSrv
{
	typedef std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

	//***************************************************************************
	// @brief 비동기 서비스 내부 설정 상수
	// @detail 큐 크기 경고 임계치 등을 정의하는 상수 열거형입니다.
	//***************************************************************************
	enum
	{
		MAX_WARNING_QUERY_QUEUE_SIZE = 100000,			// 이 값 이상이면 심각(LOG_ERROR) 수준 경고 대상
		MAX_WARNING_RESET_QUEUE_SIZE = 90000,			// 히스테리시스: 이 값 이하로 내려와야 경고를 재무장(rearm)함
		INITIAL_WARN_QUEUE_SIZE = 1000,				// 시작 워밍업 중 사소한 큐 증가로 바로 경고가 찍히지 않도록 하는 최초 기준치
	};

	static constexpr int64 QUEUE_SIZE_WARN_COOLDOWN_MS = 5000;	// 큐 증가 경고(LOG_WARNING) 재발생 최소 간격(쿨다운)

public:
	CMySQLAsyncSrv();
	virtual ~CMySQLAsyncSrv();

	virtual bool	RunningThread();

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	//***************************************************************************
	// @brief 현재 대기 중인 비동기 요청 큐의 크기를 반환합니다.
	// @return int 현재 큐에 대기 중인 요청 개수
	//***************************************************************************
	int GetQueryQueueSize() const {
		return static_cast<int>(_queueDBAsyncRq.GetSize());
	}

	//***************************************************************************
	// @brief 비동기 요청 큐가 비어있는지 확인합니다.
	// @return bool 큐가 비어있으면 true, 요청이 존재하면 false
	//***************************************************************************
	bool IsEmpty() const {
		return _queueDBAsyncRq.IsEmpty();
	}

	int Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq);
	std::unique_ptr<st_DBAsyncRq> Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue);

	//***************************************************************************
	// @brief 큐 용량이 설정값 미만이 되거나 스레드가 종료될 때까지 생산자 스레드를 대기시킵니다.
	// @param maxCapacity 대기 기준이 되는 최대 용량 크기
	//***************************************************************************
	void WaitPushCapacity(size_t maxCapacity) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cvProducer.wait(lock, [this, maxCapacity]() {
			return static_cast<size_t>(_queueDBAsyncRq.GetSize()) < maxCapacity || _bStopThread.load();
			});
	}

	//***************************************************************************
	// @brief 현재 처리 중인(미완료된) 비동기 요청 수를 반환합니다.
	// @return int32 현재 처리 중인 미완료 요청 수
	//***************************************************************************
	int32 GetOutstandingRequests() const {
		return _nOutstandingRequests.load(std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 처리 중인 비동기 요청 수를 1 증가시킵니다.
	//***************************************************************************
	void AddOutstandingRequest() {
		_nOutstandingRequests.fetch_add(1, std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 처리 중인 비동기 요청 수를 1 감소시킵니다.
	//***************************************************************************
	void SubOutstandingRequest() {
		_nOutstandingRequests.fetch_sub(1, std::memory_order_relaxed);
	}

	bool StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt = 0);
	bool InitMySQL(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt);

	void StartIoThreads();
	bool Action();

	//***************************************************************************
	// @brief 비동기 서비스 스레드를 중단하고 대기 중인 모든 조건 변수에 알림을 보냅니다.
	//***************************************************************************
	void StopThread() {
		_bStopThread.store(true);
		_cva.notify_all();
		_cvProducer.notify_all();
	};

	CMySQLConnPool* GetAccountConnPool(void);
	CMySQLConnPool* GetMySQLConnPool(uint64 m_nID);
	CMySQLConnPool* GetLogConnPool();

	CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>	_queueDBAsyncRq;				// 비동기 요청 큐
	COMMAND_MAP							_mapCommand;					// 명령 핸들러 맵

	int32								_nDBCount;						// DB 개수
	bool								_bOpen;							// 서비스 오픈 여부
	int32								_nMaxThreadCnt;					// 최대 스레드 수
	CMySQLConnPool** _pMySQLConnPools;				// MySQL 연결 풀 배열

public:
	static std::shared_ptr<CMySQLAsyncSrv> Instance();

protected:
	void		Clear(void);
	void		FlushRemainingTasks();
	void		ClearMySQLConnPools(void);

private:
	std::atomic<bool>			_bStopThread;							// 스레드 중단 플래그
	std::atomic<int32>          _nOutstandingRequests{ 0 };				// 처리 중 요청 수

	// 큐 크기 경고 관련 상태 — 모두 Pop()이 _mutex를 쥔 구간 안에서만 읽고 쓰므로 atomic이 아니어도 안전하다.
	int							_nLastWarnedQueueSize{ INITIAL_WARN_QUEUE_SIZE };	// 마지막으로 LOG_WARNING을 남긴 큐 크기(새 최댓값 갱신 기준)
	bool						_bMaxWarningActive{ false };						// MAX_WARNING_QUERY_QUEUE_SIZE 경고가 이미 발령된 상태인지(히스테리시스로 재무장)
	std::chrono::steady_clock::time_point	_lastQueueSizeWarnTime{};				// 마지막 LOG_WARNING 시각(쿨다운 판단용)

	std::mutex					_mutex;									// 동기화용 뮤텍스
	std::condition_variable		_cva;									// 소비자 대기 조건 변수
	std::condition_variable		_cvProducer;							// 생산자 대기 조건 변수
};

#endif // ndef UC_MYSQLASYNCSRV_H