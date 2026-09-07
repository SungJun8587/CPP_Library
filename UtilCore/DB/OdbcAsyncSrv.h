
//***************************************************************************
// OdbcAsyncSrv.h : interface for the COdbcAsyncSrv class.
//
//***************************************************************************

#ifndef UC_ODBCASYNCSRV_H
#define UC_ODBCASYNCSRV_H

#include <DB/OdbcConnPool.h>
#include <Containers/Queue/ChunkedSwapQueue.h>

#include <vector>
#include <thread>
#include <memory>
#include <unordered_map>

//***************************************************************************
// @brief DB 서비스 하나(예: 멤버/게임 도메인 중 하나)의 비동기 ODBC 처리 엔진.
// @details
// [설계 변경] 이전에는 static Instance()로 프로세스에 딱 하나만 존재하는
// 싱글턴이었다. 멤버/게임/로그처럼 서로 독립된 여러 DB를 동시에 다뤄야
// 하면서 "타입 하나당 인스턴스 하나"라는 싱글턴 제약과 정면으로 부딪혀,
// Instance()를 완전히 제거했다. 이제 이 클래스는 순수하게 "DB 서비스
// 하나를 나타내는 재사용 가능한 부품"이고, 여러 인스턴스를 소유/관리하는
// 책임은 상위의 CDbServiceManager로 옮긴다.
//
// [호출부 영향] COdbcAsyncSrv::Instance()를 쓰던 기존 코드(예:
// DECLARE_ODBC_DBASYNC_HANDLER 매크로, AccountDBHandler.cpp)는 전부
// CDbServiceManager를 거치도록 같이 고쳐야 한다 — 이 헤더만 바꿔서는
// 컴파일이 안 된다.
//***************************************************************************
class COdbcAsyncSrv
{
	typedef std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>	COMMAND_MAP;

public:
	COdbcAsyncSrv();
	virtual ~COdbcAsyncSrv();

	// 스레드/커넥션풀을 직접 소유하는 타입이라 얕은 복사가 위험하다 —
	// CDbServiceManager가 unique_ptr로 관리하므로 애초에 복사될 일이
	// 없어야 하고, 실수로라도 복사되면 컴파일 타임에 막는다.
	COdbcAsyncSrv(const COdbcAsyncSrv&) = delete;
	COdbcAsyncSrv& operator=(const COdbcAsyncSrv&) = delete;

	std::shared_ptr<CDBAsyncSrvHandler> Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler);

	//***************************************************************************
	// @brief 대기 중인 쿼리 큐의 크기 조회
	//***************************************************************************
	int GetQueryQueueSize() const {
		return static_cast<int>(_queueDBAsyncRq.GetSize());
	}

	//***************************************************************************
	// @brief 큐가 비어 있는지 확인
	//***************************************************************************
	bool IsEmpty() const {
		return _queueDBAsyncRq.IsEmpty();
	}

	int Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq);

	//***************************************************************************
	// @brief 백프레셔 처리: 지정한 최대 용량을 초과할 경우 큐 삽입 대기
	// @details [주의 — 알려진 한계] 이 대기 자체는 Push()와 원자적으로
	//          묶여 있지 않다. 여러 생산자 스레드가 동시에 이 함수를
	//          통과하면, 그 사이 다 같이 Push()를 호출해 maxCapacity를
	//          일시적으로 초과할 수 있다(하드 리밋이 아니라 "대략적인"
	//          상한). 정확한 상한 보장이 필요해지면 WaitPushCapacity()와
	//          Push()를 하나의 원자적 producer API로 묶는 리팩터링이
	//          필요하다 — 지금은 그 정도 엄밀함이 필요한 워크로드가
	//          아니라 별도로 처리하지 않았다.
	//***************************************************************************
	void WaitPushCapacity(size_t maxCapacity) {
		std::unique_lock<std::mutex> lock(_mutex);
		_cvProducer.wait(lock, [this, maxCapacity]() {
			return static_cast<size_t>(_queueDBAsyncRq.GetSize()) < maxCapacity || _bStopThread.load();
			});
	}

	int32 GetOutstandingRequests() const {
		return _nOutstandingRequests.load(std::memory_order_relaxed);
	}

	void AddOutstandingRequest() {
		_nOutstandingRequests.fetch_add(1, std::memory_order_relaxed);
	}

	void SubOutstandingRequest() {
		_nOutstandingRequests.fetch_sub(1, std::memory_order_relaxed);
	}

	//***************************************************************************
	// @brief 서비스 하나를 완전히 시작합니다 — ODBC 커넥션 풀 초기화 +
	//        워커 스레드 기동까지 한 번에 처리합니다.
	// @details [수정] 한 인스턴스당 정확히 한 번만 호출할 수 있도록
	//          _bStarted로 막았다. 이미 시작된 인스턴스에 다시 호출하면
	//          내부적으로 InitOdbc()가 _odbcPools.clear()를 하는데, 그
	//          시점에 기존 워커 스레드가 아직 그 풀들을 참조하며 실행
	//          중일 수 있어 위험하다 — 재시작이 필요하면 새 인스턴스를
	//          만들 것(예: CDbServiceManager가 소멸 후 재생성).
	// @param dbNodeVec 이 서비스(도메인) 하나가 관리할 DB 노드 목록.
	//        같은 논리 도메인의 샤드/복제본 여러 대를 나열하는 용도 —
	//        서로 다른 도메인(멤버/게임/로그)을 한 인스턴스에 섞지 말 것
	//        (그건 도메인마다 별도 COdbcAsyncSrv 인스턴스를 두는 게 맞음).
	//***************************************************************************
	bool StartService(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt = 0);

	//***************************************************************************
	// @brief 워커 스레드에게 "그만 멈춰라" 신호만 보냅니다 — 대기하지 않습니다.
	// @details [수정] _bStopThread 세팅을 Push()와 같은 _mutex 임계구역
	//          안에서 하도록 바꿨다 — 이래야 "Stop() 완료 시점 이후로는
	//          어떤 Push()도 성공하지 않는다"는 계약을 명확히 보장한다.
	//          반드시 Join()과 짝을 맞춰 호출할 것.
	//***************************************************************************
	void Stop() {
		{
			std::lock_guard<std::mutex> lockGuard(_mutex);
			_bStopThread.store(true);
		}
		_cva.notify_all();
		_cvProducer.notify_all();
	}

	//***************************************************************************
	// @brief Stop()으로 신호를 보낸 워커 스레드들이 실제로 전부 종료할 때까지 대기합니다.
	// @details Stop() 없이 이것만 호출하면 워커가 절대 안 끝나므로 영원히
	//          블로킹된다 — 반드시 Stop() 다음에 호출할 것.
	//***************************************************************************
	void Join();

	//***************************************************************************
	// @brief 이 서비스(도메인)의 기본 커넥션 풀을 반환합니다.
	//***************************************************************************
	COdbcConnPool* GetOdbcConnPool();

	//***************************************************************************
	// @brief 샤딩이 필요한 도메인에서, id로 담당 샤드의 커넥션 풀을 반환합니다.
	//***************************************************************************
	COdbcConnPool* GetOdbcConnPool(uint64 id);

private:
	// [캡슐화] 이 넷은 StartService()/소멸자 내부에서만 정해진 순서로
	// 호출되어야 하는 구현 세부사항이다 — 외부에서 직접 호출하면
	// lifecycle이 깨질 수 있다(예: 이미 스레드가 도는 상태에서
	// StartIoThreads()를 다시 부르면 joinable한 std::thread가 든
	// vector를 clear()하게 되어 std::terminate()가 발생함).
	// @details [수정 — 주석 정정] 이전 주석은 "Stop→Start 반복
	// lifecycle"을 언급했는데, 실제 정책은 그게 아니다: _bStarted가
	// true인 정상 실행 상태에서는 StartService() 재호출 자체가
	// 거부된다(재시작 불가). 여기서 상태를 리셋하는 이유는 오직
	// "StartService() 도중 실패(StartIoThreads() 예외 등)한 뒤 같은
	// 인스턴스로 재시도하는" 예외 경로 하나뿐이다.
	bool InitOdbc(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt);
	void StartIoThreads();

	// [단순화] 예전엔 bool을 반환했지만, 둘 다 항상 true만 반환했고 그
	// 반환값을 확인하는 곳도 없었다(std::thread의 진입 함수는 반환값을
	// 아예 안 씀) — 의미 없는 반환값이라 제거.
	void RunningThread();
	void Action();

	std::unique_ptr<st_DBAsyncRq> Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue);

	void FlushRemainingTasks();
	void ClearOdbcPools(void);

private:
	// [캡슐화] 예전엔 public이라 외부에서 srv._bOpen = false; 같은 코드로
	// 내부 상태를 직접 건드릴 수 있었다 — 이 클래스가 자기 lifecycle을
	// 스스로 책임진다는 설계 의도와 어긋나 private으로 내렸다. 외부에서
	// 이 값들을 참조해야 할 이유가 생기면 Getter를 추가할 것.
	CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>	_queueDBAsyncRq;			// 비동기 요청 큐
	COMMAND_MAP											_mapCommand;				// 명령 핸들러 맵

	bool								_bOpen;						// 서비스 오픈 여부
	int32								_nMaxThreadCnt;				// 최대 스레드 수

private:
	// [변경] COdbcConnPool** + 수동 new/delete[] 대신 vector<unique_ptr<>> —
	// 예외 안전성 확보(초기화 도중 실패해도 이미 만든 풀들이 자동 정리됨),
	// 수동 delete 누락 위험 제거.
	std::vector<std::unique_ptr<COdbcConnPool>>	_odbcPools;

	// [신규] 이 인스턴스가 직접 소유하는 워커 스레드. Join()이 "정확히 이
	// 인스턴스가 만든 스레드만" 기다려야 하므로, 공용 스레드 매니저
	// 컨테이너에 맡기지 않고 여기서 직접 std::thread로 들고 있는다.
	// [가정] 기존에 CThreadManager를 거치며 얻던 스레드 이름 태깅/데드락
	// 감지 연동이 있었다면 이 경로에는 빠져 있다 — 필요하면 스레드 생성
	// 직후 그 연동 초기화를 동일하게 추가해야 한다.
	std::vector<std::thread>						_workerThreads;

	std::atomic<bool>			_bStopThread;						// 스레드 중단 플래그
	bool						_bStarted{ false };					// StartService()가 이미 호출됐는지(중복 호출 방지)
	std::atomic<int32>			_nOutstandingRequests{ 0 };			// 처리 중 요청 수

	std::mutex					_mutex;								// 동기화용 뮤텍스
	std::condition_variable		_cva;								// 소비자 대기 조건 변수
	std::condition_variable		_cvProducer;						// 생산자 대기 조건 변수
};

#endif // ndef UC_ODBCASYNCSRV_H