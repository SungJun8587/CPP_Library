
//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"

//***************************************************************************
// @brief 생성자: 기본 멤버 초기화
//***************************************************************************
COdbcAsyncSrv::COdbcAsyncSrv()
{
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
}

//***************************************************************************
// @brief 소멸자.
// @details [수정] 이전에는 FlushRemainingTasks()(내부에서 StopThread() 신호만
// 보냄) 직후 곧바로 ClearOdbcPools()를 호출해, 아직 실행 중일 수 있는 워커
// 스레드가 이미 해제된 커넥션 풀을 참조할 여지가 있었다. 이제는 Stop()으로
// 신호를 보낸 뒤 Join()으로 모든 워커 스레드가 실제로 종료했음을 확인하고
// 나서야 FlushRemainingTasks()/ClearOdbcPools()로 넘어간다 — 이 순서가
// 지켜지면 "워커가 아직 도는데 자원이 먼저 사라지는" 경쟁 자체가 성립하지
// 않는다.
//***************************************************************************
COdbcAsyncSrv::~COdbcAsyncSrv()
{
	Stop();
	Join();

	FlushRemainingTasks();
	ClearOdbcPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
}

//***************************************************************************
// @brief Stop()으로 신호를 보낸 워커 스레드들이 전부 종료할 때까지 대기합니다.
//***************************************************************************
void COdbcAsyncSrv::Join()
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
// _bStopThread를 세팅하거나 조건 변수를 notify할 필요가 없다(예전
// 버전은 이 함수 스스로 StopThread() 역할까지 겸했었는데, 소멸자가
// Stop()/Join()을 먼저 명시적으로 호출하는 지금 구조에서는 중복이라
// 제거했다).
//***************************************************************************
void COdbcAsyncSrv::FlushRemainingTasks()
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
// @brief ODBC 연결 풀을 정리합니다.
//***************************************************************************
void COdbcAsyncSrv::ClearOdbcPools()
{
	_odbcPools.clear(); // unique_ptr가 각자 알아서 해제 — 수동 delete 불필요
}

//***************************************************************************
// @brief 명령 핸들러를 등록합니다.
// @details [수정] 이전엔 insert()가 중복 키에 조용히 실패해도 무조건
// 넘겨받은 handler를 그대로 반환해, "반환값과 실제 맵에 등록된 것이
// 다른" 상황이 가능했다(호출부가 static 멤버로 이 반환값을 붙잡아두는
// 관례라 이 어긋남이 조용히 넘어갈 수 있었음). emplace()의 결과로 실제
// 등록된(또는 원래 있던) 항목을 반환하고, 중복이면 로그로 알린다.
//***************************************************************************
std::shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	auto [it, inserted] = _mapCommand.emplace(command, handler);

	if( !inserted )
	{
		LOG_ERROR(_T("COdbcAsyncSrv::Regist: duplicate async DB command registration... command: [%u]"), command);
	}

	return it->second;
}

//***************************************************************************
// @brief 서비스 시작 — ODBC 초기화 + 워커 스레드 기동까지 한 번에 처리합니다.
// @details [수정] 예전엔 이 함수가 InitOdbc()만 호출하고, 호출부가
// StartIoThreads()를 별도로 불러야 했다 — 실제로 이 호출이 빠진 채 DB
// 워커 스레드가 하나도 안 뜬 상태로 운영 중이던 사례가 발견되어, 두
// 단계를 여기서 하나로 묶어 그 실수 자체가 구조적으로 불가능하게 했다.
//***************************************************************************
bool COdbcAsyncSrv::StartService(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt)
{
	// [수정] 이미 시작된 인스턴스에 다시 호출되면 InitOdbc()의
	// _odbcPools.clear()가 현재 실행 중인 워커 스레드가 참조 중인 풀을
	// 파괴할 수 있다 — 재호출 자체를 막는다.
	if( _bStarted )
	{
		LOG_ERROR(_T("COdbcAsyncSrv::StartService: already started."));
		return false;
	}

	if( !InitOdbc(dbNodeVec, nMaxThreadCnt) )
		return false;

	// [수정] StartIoThreads()가 실패(std::thread 생성 예외)하면 그 함수
	// 내부에서 이미 Stop()+Join()으로 스레드 쪽은 정리되지만, _bOpen과
	// _odbcPools는 그대로 남는다 — 이 함수를 재호출하면 InitOdbc()가
	// 다시 초기화해주므로 "우연히" 안전하긴 하지만, 재호출 없이 그냥
	// 소멸되는 경로까지 고려해 여기서 명시적으로 정리한다.
	try
	{
		StartIoThreads();
	}
	catch( ... )
	{
		_bOpen = false;
		ClearOdbcPools();
		throw;
	}

	_bStarted = true;
	return true;
}

//***************************************************************************
// @brief ODBC 초기화
//***************************************************************************
bool COdbcAsyncSrv::InitOdbc(const CVector<CDBNode>& dbNodeVec, const int32 nMaxThreadCnt)
{
	// [수정 — 주석 정정] 이전 주석은 "Stop→Start 반복 lifecycle"을
	// 언급했는데, 실제 정책은 그게 아니다 — _bStarted가 true인 정상
	// 실행 상태에서는 StartService() 재호출 자체가 거부된다(재시작
	// 불가). 여기서 상태를 리셋하는 이유는 오직 "StartService() 도중
	// 실패(StartIoThreads() 예외 등)한 뒤 같은 인스턴스로 재시도하는"
	// 예외 경로 하나뿐이다.
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
		LOG_ERROR(_T("InitOdbc: dbNodeVec is empty — a DB service requires at least one DB node."));
		return false;
	}

	_odbcPools.clear();
	_odbcPools.reserve(nDBCount);

	COdbcConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	for( auto& iter : dbNodeVec )
	{
		auto pool = std::make_unique<COdbcConnPool>(_nMaxThreadCnt);

		if( !pool->Init(iter._dbClass, iter._tszDSN, reconnectCfg) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			_odbcPools.clear(); // 지금까지 만든 풀들도 함께 정리(부분 초기화 상태로 안 남김)
			return false;
		}

		_odbcPools.push_back(std::move(pool));
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
// 던질 수 있다 — 예를 들어 8개는 성공하고 9번째에서 예외가 나면, 이미
// 만들어진 8개가 정리되지 않은 채 예외가 전파될 위험이 있었다. 이미 만든
// 스레드들을 Stop()+Join()으로 안전하게 정리한 뒤 예외를 다시 던진다.
//***************************************************************************
void COdbcAsyncSrv::StartIoThreads()
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
void COdbcAsyncSrv::RunningThread()
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
// 항목들이 처리되지 않은 채 스택에서 그냥 파괴됐다(unique_ptr이라 메모리
// 누수는 아니지만 DB 요청 자체가 유실됨). FlushRemainingTasks()는 전역
// 큐만 스왑해서 보므로 이미 로컬 큐로 넘어간 항목은 찾을 수 없었다.
//
// 이제는 Pop()의 반환값(nullptr 여부)만으로 종료를 판단한다 — Pop()은
// "로컬 큐를 무조건 먼저 소진 → 전역 큐 확인 → 로컬+전역 큐가 둘 다
// 비었고 Stop 상태일 때만 nullptr"라는 순서로 이미 짜여 있으므로, 이
// 방식이면 Stop() 시점에 이미 큐에 들어 있던 로컬 큐/전역 큐의 요청은
// 전부 정상 처리 경로(핸들러 디스패치)를 거쳐 소진된 뒤에야 워커가
// 종료된다.
//
// [주의 — 예외] 이건 "이미 큐에 들어 있던 요청"에만 해당한다. 처리
// *도중* TIMEOUT이 나서 재시도하려는 요청은 다르다 — 재시도는
// Push()를 다시 호출하는 건데, Push()는 Stop() 이후 항상 실패하므로
// (아래 Push() 참고) Stop()이 이미 호출된 뒤에 타임아웃난 요청의
// 재시도는 성공하지 못하고 그 자리에서 폐기된다(SubOutstandingRequest()로
// 카운터는 정확히 맞춰지므로 카운터 누수는 아니다 — 다만 그 DB 작업
// 자체는 이번 프로세스 수명 안에서 처리되지 않고 끝난다는 뜻). 이건
// 버그가 아니라 "종료 중에는 새로운 재시도를 받지 않는다"는 의도된
// shutdown 정책이다.
//
// FlushRemainingTasks()는 위 drain이 정상 동작하는 한 실제로 처리할
// 항목이 거의 없는 최종 방어적 안전망이다(Stop()/Push()가 같은
// _mutex를 공유해 "Stop 이후의 Push는 항상 실패"가 보장되므로, 특별한
// 레이스 윈도우가 있어서라기보다는 그냥 이중 안전장치 성격에 가깝다).
//***************************************************************************
void COdbcAsyncSrv::Action()
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
			// [수정] 이 분기에서 SubOutstandingRequest()가 누락돼 있었다 —
			// AddOutstandingRequest()와 짝이 안 맞아 카운터가 영구히
			// 드리프트하는 버그(핸들러 못 찾는 요청이 하나 생길 때마다
			// GetOutstandingRequests()가 절대 0으로 안 내려감).
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
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);

				uint16 logIdent = pAsyncRq->callIdent;
				pAsyncRq->bReTry = true;

				int nSize = Push(std::move(pAsyncRq));
				if( nSize == 0 )
				{
					// [수정] 재큐잉 실패(서비스 종료 시점 등) — 이 요청의
					// 소유권이 완전히 소멸했으므로 더 이상 outstanding이
					// 아니다. 여기서 SubOutstandingRequest()가 누락돼
					// 있었다(성공 시엔 유지가 맞고, 실패 시에만 감소해야
					// 하는데 그 분기 자체가 비어있었음).
					LOG_ERROR(_T("Failed to retry query because service is stopping... callIdent: [%u]"), logIdent);
					SubOutstandingRequest();
				}
				else
				{
					// 재큐잉 성공 — outstanding 유지(아직 처리 중인 상태).
					LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
				}

				continue;
			}
		}

#if defined(_DEBUG)
		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);
#else
		uint64 endTick = _GetTickCount();
		if( 1000 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt.fetch_add(1, std::memory_order_relaxed), static_cast<int>(Ret), pAsyncRq->callIdent);
#endif

		SubOutstandingRequest();
	}
}

//***************************************************************************
// @brief 큐에 요청을 추가합니다.
// @details [수정] _bStopThread 확인을 _mutex 밖에서 했던 걸 안으로
// 옮겼다 — 이전엔 "Push()가 플래그를 false로 확인한 직후 Stop()이
// 끼어들어 플래그를 true로 바꾸고, 그 뒤에야 Push()가 큐에 항목을 넣는"
// 경쟁이 가능했다(메모리 안전성 문제는 아니었지만 "Stop() 이후 어떤
// Push()도 성공하지 않는다"는 계약이 애매했다). Stop()도 같은 _mutex
// 임계구역에서 플래그를 세팅하도록 맞춰서, 이제 Push()와 Stop() 중
// 하나만 먼저 완전히 끝난 뒤 다른 하나가 시작된다.
//***************************************************************************
int COdbcAsyncSrv::Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq)
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
std::unique_ptr<st_DBAsyncRq> COdbcAsyncSrv::Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue)
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
// _odbcPools[0]에 접근해 미정의 동작이 났다 — assert는 개발 중 계약
// 위반을 조기에 알리는 용도로 남겨두고, 별도로 nullptr을 반환하는
// 명시적 방어를 추가했다.
//***************************************************************************
COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool()
{
	assert(!_odbcPools.empty());
	if( _odbcPools.empty() )
		return nullptr;

	return _odbcPools[0].get();
}

//***************************************************************************
// @brief id로 담당 샤드의 커넥션 풀을 반환합니다.
// @details [단순화] 예전엔 "인덱스 0을 마스터로 예약하고 1번부터 샤드를
// 배치"하는 정책이 섞여 있었는데, 이건 "인스턴스 하나가 멤버+게임+로그를
// 겸하던" 옛 설계의 흔적이라 이 프로젝트가 실제로 그 정책을 요구한다는
// 근거가 없었다. 도메인별로 인스턴스가 분리된 지금은 균등 모듈로
// 샤딩으로 단순화한다 — 풀이 1개뿐이면 그 하나만 반환.
// [주의] 이 모듈로 방식은 "_odbcPools[0..N-1]이 전부 동등한 성격의
// 샤드"라는 전제다. 0번이 마스터/그 외가 리플리카처럼 역할이 다른
// 토폴로지라면, 그건 이 함수가 아니라 상위(CDbServiceManager 등)에서
// 별도로 표현해야 한다.
//***************************************************************************
COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool(uint64 id)
{
	assert(!_odbcPools.empty());
	if( _odbcPools.empty() )
		return nullptr;

	if( _odbcPools.size() == 1 )
		return _odbcPools[0].get();

	const size_t index = static_cast<size_t>(id) % _odbcPools.size();
	return _odbcPools[index].get();
}