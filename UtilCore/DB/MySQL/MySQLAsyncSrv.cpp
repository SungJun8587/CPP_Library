
//***************************************************************************
// MySQLAsyncSrv.cpp : implementation of the CMySQLAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLAsyncSrv.h"

//***************************************************************************
// @brief 생성자: 기본 멤버 초기화
//***************************************************************************
CMySQLAsyncSrv::CMySQLAsyncSrv()
{
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
}

//***************************************************************************
// @brief 소멸자.
// @details [수정] 이전에는 FlushRemainingTasks() 직후 곧바로
// ClearMySQLConnPools()를 호출해, 아직 실행 중일 수 있는 워커 스레드가
// 이미 해제된 커넥션 풀을 참조할 여지가 있었다. 이제는 Stop()으로 신호를
// 보낸 뒤 Join()으로 모든 워커 스레드가 실제로 종료했음을 확인하고 나서야
// FlushRemainingTasks()/ClearMySQLConnPools()로 넘어간다 — 이 순서가
// 지켜지면 "워커가 아직 도는데 자원이 먼저 사라지는" 경쟁 자체가 성립하지
// 않는다.
//***************************************************************************
CMySQLAsyncSrv::~CMySQLAsyncSrv()
{
	Stop();
	Join();

	FlushRemainingTasks();
	ClearMySQLConnPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
}

//***************************************************************************
// @brief Stop()으로 신호를 보낸 워커 스레드들이 전부 종료할 때까지 대기합니다.
//***************************************************************************
void CMySQLAsyncSrv::Join()
{
	for( auto& worker : _workerThreads )
	{
		if( worker.joinable() )
			worker.join();
	}
	_workerThreads.clear();
}

//***************************************************************************
// @brief 남은 작업을 강제로 동기 처리합니다.
// @details [전제] 이 함수가 호출되는 시점엔 이미 Join()이 끝나 워커
// 스레드가 하나도 남아있지 않다고 가정한다 — 그래서 여기서 다시
// _bStopThread를 세팅하거나 조건 변수를 notify할 필요가 없다.
//***************************************************************************
void CMySQLAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Flushing remaining async DB tasks..."));

	CQueue<std::unique_ptr<st_DBAsyncRq>> tempQueue;
	_queueDBAsyncRq.Swap(tempQueue);

	int32 remainingCount = static_cast<int32>(tempQueue.size());
	if( remainingCount > 0 )
	{
		LOG_INFO(_T("Processing %d remaining DB requests synchronously..."), remainingCount);

		while( !tempQueue.empty() )
		{
			std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(tempQueue.front());
			tempQueue.pop();

			if( pAsyncRq == nullptr ) continue;

			COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
			if( it != _mapCommand.end() )
			{
				std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
				EDBReturnType ret = command->ProcessAsyncCall(pAsyncRq.get());

				if( ret != EDBReturnType::OK )
				{
					LOG_ERROR(_T("Failed to process task during flush... callIdent: [%u]"), pAsyncRq->callIdent);
				}
			}
			else
			{
				LOG_ERROR(_T("No handler found for task during flush... callIdent: [%u]"), pAsyncRq->callIdent);
			}

			SubOutstandingRequest();
		}
	}

	LOG_INFO(_T("Flush completed."));
}

//***************************************************************************
// @brief MySQL 연결 풀을 정리합니다.
//***************************************************************************
void CMySQLAsyncSrv::ClearMySQLConnPools()
{
	_mysqlPools.clear(); // unique_ptr가 각자 알아서 해제 — 수동 delete 불필요
}

//***************************************************************************
// @brief 명령 핸들러를 등록합니다.
// @details [수정] 이전엔 insert()가 중복 키에 조용히 실패해도 무조건
// 넘겨받은 handler를 그대로 반환해, "반환값과 실제 맵에 등록된 것이
// 다른" 상황이 가능했다. emplace()의 결과로 실제 등록된(또는 원래
// 있던) 항목을 반환하고, 중복이면 로그로 알린다.
//***************************************************************************
std::shared_ptr<CDBAsyncSrvHandler> CMySQLAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	auto [it, inserted] = _mapCommand.emplace(command, handler);

	if( !inserted )
	{
		LOG_ERROR(_T("CMySQLAsyncSrv::Regist: duplicate async DB command registration... command: [%u]"), command);
	}

	return it->second;
}

//***************************************************************************
// @brief 서비스 시작 — MySQL 초기화 + 워커 스레드 기동까지 한 번에 처리합니다.
//***************************************************************************
bool CMySQLAsyncSrv::StartService(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt)
{
	// [수정] 이미 시작된 인스턴스에 다시 호출되면 InitMySQL()의
	// _mysqlPools.clear()가 현재 실행 중인 워커 스레드가 참조 중인 풀을
	// 파괴할 수 있다 — 재호출 자체를 막는다.
	if( _bStarted )
	{
		LOG_ERROR(_T("CMySQLAsyncSrv::StartService: already started."));
		return false;
	}

	if( !InitMySQL(dbNodeVec, nMaxThreadCnt) )
		return false;

	// [수정] StartIoThreads()가 실패(std::thread 생성 예외)하면 그 함수
	// 내부에서 이미 Stop()+Join()으로 스레드 쪽은 정리되지만, _bOpen과
	// _mysqlPools는 그대로 남는다 — 여기서 명시적으로 정리한다.
	try
	{
		StartIoThreads();
	}
	catch( ... )
	{
		_bOpen = false;
		ClearMySQLConnPools();
		throw;
	}

	_bStarted = true;
	return true;
}

//***************************************************************************
// @brief MySQL 초기화
//***************************************************************************
bool CMySQLAsyncSrv::InitMySQL(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt)
{
	// [수정 — 주석 정정] 여기서 상태를 리셋하는 이유는 오직 "StartService()
	// 도중 실패(StartIoThreads() 예외 등)한 뒤 같은 인스턴스로 재시도하는"
	// 예외 경로 하나뿐이다 — _bStarted가 true인 정상 실행 상태에서는
	// StartService() 재호출 자체가 거부된다(재시작 불가).
	_bOpen = false;
	_bStopThread.store(false);

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	const int32 nDBCount = static_cast<int32>(dbNodeVec.size());
	if( nDBCount <= 0 )
	{
		// [수정] 예전엔 여기서 true를 반환해 "DB 없는 서비스"를 성공으로
		// 취급했는데, 그러면 StartService()가 뒤이어 StartIoThreads()로
		// 워커 스레드를 만들어도 _bOpen이 false라 RunningThread()가
		// 즉시 리턴해버려 스레드가 아무 일도 안 하고 바로 끝난다 —
		// 의미 없는 스레드 생성/소멸 비용만 발생. DB 서비스는 최소
		// 1개의 DB 노드가 있어야 한다는 계약으로 명시한다.
		LOG_ERROR(_T("InitMySQL: dbNodeVec is empty — a DB service requires at least one DB node."));
		return false;
	}

	_mysqlPools.clear();
	_mysqlPools.reserve(nDBCount);

	CMySQLConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	for( auto& iter : dbNodeVec )
	{
		auto pool = std::make_unique<CMySQLConnPool>(_nMaxThreadCnt);

		if( !pool->Init(iter._tszDBHost, iter._tszDBUserId, iter._tszDBPasswd, iter._tszDBName, iter._nPort, reconnectCfg) )
		{
			LOG_ERROR(_T("Failed to Initialize CMySQLConnPool"));
			_mysqlPools.clear(); // 지금까지 만든 풀들도 함께 정리(부분 초기화 상태로 안 남김)
			return false;
		}

		_mysqlPools.push_back(std::move(pool));
	}

	_bOpen = true;
	return true;
}

//***************************************************************************
// @brief IO 스레드를 시작합니다.
// @details [변경] gpThreadManager를 거치지 않고 이 인스턴스가 std::thread를
// 직접 소유한다 — Join()이 "정확히 이 인스턴스가 만든 스레드만" 기다려야
// 하는데, 공용 컨테이너에 맡기면 그 사이 다른 코드가 만든 스레드와 섞여서
// 정확한 대상만 골라 join하기 어렵다.
// [수정] std::thread 생성자는 시스템 자원 부족 등으로 std::system_error를
// 던질 수 있다 — 이미 만든 스레드들을 Stop()+Join()으로 안전하게 정리한
// 뒤 예외를 다시 던진다.
//***************************************************************************
void CMySQLAsyncSrv::StartIoThreads()
{
	_workerThreads.clear();
	_workerThreads.reserve(static_cast<size_t>(_nMaxThreadCnt));

	try
	{
		for( int32 i = 0; i < _nMaxThreadCnt; i++ )
		{
			_workerThreads.emplace_back([this]() { RunningThread(); });
		}
	}
	catch( ... )
	{
		Stop();
		Join();
		throw;
	}
}

//***************************************************************************
// @brief 스레드 실행 루프
//***************************************************************************
void CMySQLAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
}

//***************************************************************************
// @brief 요청 처리 루프
// @details [수정 — 데이터 유실 버그] 예전엔 while(!_bStopThread.load())로
// 종료 여부를 판단했다. Pop()이 SwapChunk(64)로 로컬 큐에 최대 64개를
// 한 번에 가져오는데, 그중 1개만 처리한 시점에 Stop()이 호출되면 while
// 조건이 곧바로 false가 되어 Action()이 리턴 — 로컬 큐에 남아있던 나머지
// 항목들이 처리되지 않은 채 스택에서 그냥 파괴됐다(DB 요청 자체가 유실됨).
// 이제는 Pop()의 반환값(nullptr 여부)만으로 종료를 판단한다.
//***************************************************************************
void CMySQLAsyncSrv::Action()
{
	static std::atomic<uint64> cumulateCallCnt{ 0 };
	CQueue<std::unique_ptr<st_DBAsyncRq>> localQueue;

	for( ;; )
	{
		std::unique_ptr<st_DBAsyncRq> pAsyncRq = Pop(localQueue);
		if( pAsyncRq == nullptr )
			break;

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			LOG_ERROR(_T("Error not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
			SubOutstandingRequest();
			continue;
		}

		uint64 startTick = _GetTickCount();

		std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq.get());

		if( Ret != EDBReturnType::OK )
		{
			LOG_ERROR(_T("Failed Async Call... callIdent: [%u]"), pAsyncRq->callIdent);

			if( Ret == EDBReturnType::TIMEOUT && pAsyncRq->bReTry == false )
			{
				uint64 endTick = _GetTickCount();
				if( 300 <= endTick - startTick )
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"),
						endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);

				uint16 logIdent = pAsyncRq->callIdent;
				pAsyncRq->bReTry = true;

				int nSize = Push(std::move(pAsyncRq));

				if( nSize == 0 )
				{
					// [수정] 재큐잉 실패(서비스 종료 시점 등) — 이 요청의
					// 소유권이 완전히 소멸했으므로 더 이상 outstanding이
					// 아니다. 이전 버전은 이 분기에서 SubOutstandingRequest()
					// 호출이 누락돼 있었다.
					LOG_ERROR(_T("Failed to retry query because service is stopping... callIdent: [%u]"), logIdent);
					SubOutstandingRequest();
				}
				else
				{
					LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
				}

				continue;
			}
		}

#if defined(_DEBUG)
		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"),
				endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);
#else
		uint64 endTick = _GetTickCount();
		if( 1000 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"),
				endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);
#endif

		SubOutstandingRequest();
	}
}

//***************************************************************************
// @brief 큐에 요청을 추가합니다.
// @details [수정] _bStopThread 확인을 _mutex 밖에서 했던 걸 안으로
// 옮겼다 — Stop()도 같은 _mutex 임계구역에서 플래그를 세팅하도록 맞춰서,
// 이제 Push()와 Stop() 중 하나만 먼저 완전히 끝난 뒤 다른 하나가 시작된다.
//***************************************************************************
int CMySQLAsyncSrv::Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq)
{
	std::lock_guard<std::mutex> lockGuard(_mutex);

	if( _bStopThread.load() )
		return 0;

	const int queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(std::move(pAsyncRq)));
	_cva.notify_one();

	return queueSize;
}

//***************************************************************************
// @brief 큐에서 요청을 꺼냅니다.
//***************************************************************************
std::unique_ptr<st_DBAsyncRq> CMySQLAsyncSrv::Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue)
{
	if( !localQueue.empty() )
	{
		std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(localQueue.front());
		localQueue.pop();
		return pAsyncRq;
	}

	{
		std::unique_lock<std::mutex> lockGuard(_mutex);

		_cva.wait(lockGuard, [this, &localQueue]() {
			return !_queueDBAsyncRq.IsEmpty() || _bStopThread.load();
			});

		if( _bStopThread.load() && _queueDBAsyncRq.IsEmpty() && localQueue.empty() )
			return nullptr;

		int64 currentSize = _queueDBAsyncRq.GetSize();
		int currentCount = static_cast<int>(currentSize);

		if( !_bMaxWarningActive && currentSize >= MAX_WARNING_QUERY_QUEUE_SIZE )
		{
			_bMaxWarningActive = true;
			LOG_ERROR(_T("Async DB Call Queue size exceeded critical threshold... : [%d]"), currentCount);
		}
		else if( _bMaxWarningActive && currentSize <= MAX_WARNING_RESET_QUEUE_SIZE )
		{
			_bMaxWarningActive = false; // 히스테리시스 하한 아래로 내려왔으므로 재무장
		}

		if( currentCount > _nLastWarnedQueueSize )
		{
			auto now = std::chrono::steady_clock::now();
			auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				now - _lastQueueSizeWarnTime).count();

			if( elapsedMs >= QUEUE_SIZE_WARN_COOLDOWN_MS )
			{
				_nLastWarnedQueueSize = currentCount;
				_lastQueueSizeWarnTime = now;
				LOG_WARNING(_T("Async DB Call Queue size... : [%d]"), currentCount);
			}
		}

		// 전체를 다 스왑해 오는 대신, 한 번에 처리할 적정량(64개)만 떼어옴 (스케일링 병목 개선)
		_queueDBAsyncRq.SwapChunk(localQueue, 64);
	}

	_cvProducer.notify_one();

	if( localQueue.empty() )
		return nullptr;

	std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(localQueue.front());
	localQueue.pop();
	return pAsyncRq;
}

//***************************************************************************
// @brief 이 서비스(도메인)의 기본(0번) 커넥션 풀을 반환합니다.
// @details [수정] assert()는 Release 빌드에서 사라지므로, StartService()
// 전에(또는 실패 후) 이 함수가 호출되면 Release에서는 아무 방어 없이
// _mysqlPools[0]에 접근해 미정의 동작이 났다 — assert는 개발 중 계약
// 위반을 조기에 알리는 용도로 남겨두고, 별도로 nullptr을 반환하는
// 명시적 방어를 추가했다.
//***************************************************************************
CMySQLConnPool* CMySQLAsyncSrv::GetMySQLConnPool()
{
	assert(!_mysqlPools.empty());
	if( _mysqlPools.empty() )
		return nullptr;

	return _mysqlPools[0].get();
}

//***************************************************************************
// @brief id로 담당 샤드의 커넥션 풀을 반환합니다.
// @details [단순화] 예전엔 "인덱스 0을 계정 DB로, 인덱스 2를 로그 DB로
// 예약하고 나머지를 게임 DB 샤드로 배치"하는 정책이 섞여 있었는데, 이건
// "인스턴스 하나가 멤버+게임+로그를 겸하던" 옛 설계의 흔적이라 이
// 프로젝트가 실제로 그 정책을 요구한다는 근거가 없었다. 도메인별로
// 인스턴스가 분리된 지금은 균등 모듈로 샤딩으로 단순화한다 — 풀이
// 1개뿐이면 그 하나만 반환.
//***************************************************************************
CMySQLConnPool* CMySQLAsyncSrv::GetMySQLConnPool(uint64 id)
{
	assert(!_mysqlPools.empty());
	if( _mysqlPools.empty() )
		return nullptr;

	if( _mysqlPools.size() == 1 )
		return _mysqlPools[0].get();

	const size_t index = static_cast<size_t>(id) % _mysqlPools.size();
	return _mysqlPools[index].get();
}