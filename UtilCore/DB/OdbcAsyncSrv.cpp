
//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"

extern CThreadManager* gpThreadManager;

//***************************************************************************
// @brief 생성자: 기본 멤버 초기화
//***************************************************************************
COdbcAsyncSrv::COdbcAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pOdbcConnPools = nullptr;
}

//***************************************************************************
// @brief 소멸자: 남은 작업 처리 후 리소스 정리
//***************************************************************************
COdbcAsyncSrv::~COdbcAsyncSrv()
{
	FlushRemainingTasks();
	//StopThread();
	//Clear();
	ClearOdbcPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

//***************************************************************************
// @brief 큐를 비웁니다.
// @details 큐에 남아있는 모든 요청을 안전하게 삭제합니다.
//***************************************************************************
void COdbcAsyncSrv::Clear()
{
	// [2/3번 수정] 크기를 먼저 읽고 그 값만큼 SwapChunk하는 대신,
	// 큐 전체를 한 번에(O(1), 대상이 비어있는 경우) 넘겨받는 전용 Swap()을 사용한다.
	// - GetSize() 이후 Push()가 끼어들 경우 방금 들어온 항목이 이번 드레인에서
	//   누락될 수 있는 TOCTOU 여지를 원천적으로 제거한다.
	CQueue<std::unique_ptr<st_DBAsyncRq>> tempQueue;
	_queueDBAsyncRq.Swap(tempQueue);

	// tempQueue가 비워질 때 unique_ptr이 알아서 메모리를 해제하므로 별도의 SAFE_DELETE가 불필요합니다.
	while( !tempQueue.empty() )
	{
		tempQueue.pop();
	}
}

//***************************************************************************
// @brief 남은 작업을 강제로 처리합니다.
// @details 스레드를 중단하고 큐에 남은 요청을 메인 스레드에서 직접 실행합니다.
//***************************************************************************
void COdbcAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Main program requested to flush remaining async DB tasks..."));

	_bStopThread.store(true);
	_cva.notify_all();
	_cvProducer.notify_all();

	// [2/3번 수정] Clear()와 동일하게 GetSize()+SwapChunk 대신 Swap()으로 한 번에 이관한다.
	CQueue<std::unique_ptr<st_DBAsyncRq>> tempQueue;
	_queueDBAsyncRq.Swap(tempQueue);

	int32 remainingCount = static_cast<int32>(tempQueue.size());
	if( remainingCount > 0 )
	{
		LOG_INFO(_T("Processing %d remaining DB requests in main thread..."), remainingCount);

		while( !tempQueue.empty() )
		{
			std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(tempQueue.front());
			tempQueue.pop();

			if( pAsyncRq == nullptr ) continue;

			COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
			if( it != _mapCommand.end() )
			{
				std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
				EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq.get());

				if( Ret != EDBReturnType::OK )
				{
					LOG_ERROR(_T("Failed to process task during manual flush... callIdent: [%u]"), pAsyncRq->callIdent);
				}
			}
			else
			{
				LOG_ERROR(_T("Error not found command handler for task... callIdent: [%u]"), pAsyncRq->callIdent);
			}

			SubOutstandingRequest();
		}
	}

	LOG_INFO(_T("Manual flush completed. All tasks processed."));
}

//***************************************************************************
// @brief ODBC 연결 풀을 정리합니다.
// @details 모든 연결 풀 객체를 삭제합니다.
//***************************************************************************
void COdbcAsyncSrv::ClearOdbcPools()
{
	if( _pOdbcConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pOdbcConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pOdbcConnPools);
}

//***************************************************************************
// @brief 명령 핸들러를 등록합니다.
// @param command 명령 식별자
// @param handler 명령 핸들러 객체
// @return 등록된 핸들러
//***************************************************************************
std::shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

//***************************************************************************
// @brief 서비스 시작
// @param dbNodeVec DB 노드 벡터
// @param nMaxThreadCnt 최대 스레드 수
// @return 성공 여부
//***************************************************************************
bool COdbcAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitOdbc(dbNodeVec, nMaxThreadCnt);
}

//***************************************************************************
// @brief ODBC 초기화
// @param dbNodeVec DB 노드 벡터
// @param nMaxThreadCnt 최대 스레드 수
// @return 성공 여부
//***************************************************************************
bool COdbcAsyncSrv::InitOdbc(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	_bStopThread.store(false);

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pOdbcConnPools = new COdbcConnPool * [_nDBCount]();

	COdbcConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	int32 nIdx = 0;
	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pOdbcConnPools[nIdx] = new COdbcConnPool(_nMaxThreadCnt);
		if( nullptr == _pOdbcConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc COdbcConnPool"));
			ClearOdbcPools();
			return false;
		}

		if( false == _pOdbcConnPools[nIdx]->Init(iter._dbClass, iter._tszDSN, reconnectCfg) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			ClearOdbcPools();
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	return true;
}

//***************************************************************************
// @brief IO 스레드를 시작합니다.
// @details ThreadManager를 통해 실행 스레드를 생성합니다.
//***************************************************************************
void COdbcAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([this]() { RunningThread(); });
	}
}

//***************************************************************************
// @brief 스레드 실행 루프
// @return 항상 true
//***************************************************************************
bool COdbcAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

//***************************************************************************
// @brief 요청 처리 루프
// @details 큐에서 요청을 꺼내 핸들러로 처리합니다.
// @return 항상 true
//***************************************************************************
bool COdbcAsyncSrv::Action()
{
	// [MySQL판 대조로 발견/수정] 여러 워커 스레드가 동시에 Action()을 돌며 이 값을 증가시키므로
	// 비atomic static 변수 + 후위증가는 동기화 없는 공유 상태 변경으로 데이터 레이스(UB)다.
	static std::atomic<uint64> cumulateCallCnt{ 0 };
	CQueue<std::unique_ptr<st_DBAsyncRq>> localQueue; // 소비자별 로컬 처리용 큐 (더블 버퍼링 스왑 대상)

	while( !_bStopThread.load() )
	{
		std::unique_ptr<st_DBAsyncRq> pAsyncRq = Pop(localQueue);
		if( pAsyncRq == nullptr )
		{
			continue;
		}

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			LOG_ERROR(_T("Error not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
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
					// 서버 중단 등으로 인해 재시도 큐 삽입에 실패하여 요청이 취소될 때 로그를 남깁니다.
					LOG_ERROR(_T("Failed to retry query because service is stopping... callIdent: [%u]"), logIdent);
				}

				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
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

	return true;
}

//***************************************************************************
// @brief 큐에 요청을 추가합니다.
// @param pAsyncRq 비동기 요청 객체
// @return 큐 크기 (0이면 실패)
//***************************************************************************
int COdbcAsyncSrv::Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq)
{
	if( _bStopThread.load() ) return 0;

	// [1번 수정] PushAndGetSize()가 잡는 큐 내부 락(PLock)과 별개로,
	// Pop()이 대기하는 조건(_cva)을 보호하는 COdbcAsyncSrv::_mutex를 여기서도 잡은 뒤
	// notify_one()까지 같은 임계구역 안에서 수행한다.
	// - 기존에는 이 구간이 _mutex 밖에서 실행되어, 컨슈머가 "predicate 검사 후 실제
	//   wait() 진입 전" 사이의 틈에 push+notify가 끼어들면 notify가 유실될 수 있었다
	//   (lost wakeup). 그 경우 해당 요청은 다음 push가 들어오기 전까지 방치될 수 있었다.
	// - Pop()이 SwapChunk()를 호출할 때도 이미 같은 _mutex를 먼저 잡은 뒤이므로,
	//   잠금 순서는 항상 (_mutex → 큐 내부 PLock)으로 일관되어 데드락 위험이 없다.
	int queueSize = 0;
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(std::move(pAsyncRq)));
		_cva.notify_one();
	}

	return queueSize;
}

//***************************************************************************
// @brief 큐에서 요청을 꺼냅니다.
// @param localQueue 로컬 큐
// @return 요청 객체 (없으면 nullptr)
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

		// 전체를 다 가져오는 대신, 한 번에 처리할 적정량(예: 64개)만 떼어옴
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
// @brief 첫 번째 ODBC 연결 풀 반환
// @return 계정용 ODBC 연결 풀
//***************************************************************************
COdbcConnPool* COdbcAsyncSrv::GetAccountOdbcConnPool(void)
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 0);
	return _pOdbcConnPools[0];
}

//***************************************************************************
// @brief ID 기반 ODBC 연결 풀 반환
// @param m_nID DB ID
// @return 해당 ODBC 연결 풀
//***************************************************************************
COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool(uint64 m_nID)
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pOdbcConnPools[nIdx];
}

//***************************************************************************
// @brief 로그용 ODBC 연결 풀 반환
// @return 로그 DB 연결 풀
//***************************************************************************
COdbcConnPool* COdbcAsyncSrv::GetLogOdbcConnPool()
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 2);
	return _pOdbcConnPools[2];
}