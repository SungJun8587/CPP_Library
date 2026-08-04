
//***************************************************************************
// MySQLAsyncSrv.cpp : implementation of the CMySQLAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLAsyncSrv.h"

extern CThreadManager* gpThreadManager;

//***************************************************************************
// @brief 싱글톤 인스턴스를 반환합니다.
// @return CMySQLAsyncSrv 공유 포인터
//***************************************************************************
std::shared_ptr<CMySQLAsyncSrv> CMySQLAsyncSrv::Instance() {
	static std::shared_ptr<CMySQLAsyncSrv> instance = std::make_shared<CMySQLAsyncSrv>();
	return instance;
}

//***************************************************************************
// @brief 생성자: 기본 멤버 초기화
//***************************************************************************
CMySQLAsyncSrv::CMySQLAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pMySQLConnPools = nullptr;
}

//***************************************************************************
// @brief 소멸자: 남은 작업 처리 후 리소스 정리
//***************************************************************************
CMySQLAsyncSrv::~CMySQLAsyncSrv()
{
	FlushRemainingTasks();
	StopThread();
	Clear();
	ClearMySQLConnPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

//***************************************************************************
// @brief 큐를 비웁니다.
// @details 큐에 남아있는 모든 요청을 안전하게 삭제합니다.
//***************************************************************************
void CMySQLAsyncSrv::Clear()
{
	// CSwapQueue를 비우기 위해 전체 크기만큼 척크 스왑 수행
	std::queue<st_DBAsyncRq*> tempQueue;

	int64_t totalSize = _queueDBAsyncRq.GetSize();
	if( totalSize > 0 )
	{
		_queueDBAsyncRq.SwapChunk(tempQueue, static_cast<size_t>(totalSize));
	}

	while( !tempQueue.empty() )
	{
		st_DBAsyncRq* pAsyncRq = tempQueue.front();
		tempQueue.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}
}

//***************************************************************************
// @brief 남은 작업을 강제로 처리합니다.
// @details 스레드를 중단하고 큐에 남은 요청을 메인 스레드에서 직접 실행합니다.
//***************************************************************************
void CMySQLAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Main program requested to flush remaining async DB tasks..."));

	_bStopThread.store(true);
	_cva.notify_all();
	_cvProducer.notify_all();

	std::queue<st_DBAsyncRq*> tempQueue;
	int64_t totalSize = _queueDBAsyncRq.GetSize();
	if( totalSize > 0 )
	{
		_queueDBAsyncRq.SwapChunk(tempQueue, static_cast<size_t>(totalSize));
	}

	int32 remainingCount = static_cast<int32>(tempQueue.size());
	if( remainingCount > 0 )
	{
		LOG_INFO(_T("Processing %d remaining DB requests in main thread..."), remainingCount);

		while( !tempQueue.empty() )
		{
			st_DBAsyncRq* pAsyncRq = tempQueue.front();
			tempQueue.pop();

			if( pAsyncRq == nullptr ) continue;

			COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
			if( it != _mapCommand.end() )
			{
				std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
				EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq);

				if( Ret != EDBReturnType::OK )
				{
					LOG_ERROR(_T("Failed to process task during manual flush... callIdent: [%u]"), pAsyncRq->callIdent);
				}

				SubOutstandingRequest();
			}
			else
			{
				LOG_ERROR(_T("Error not found command handler for task... callIdent: [%u]"), pAsyncRq->callIdent);
			}

			SAFE_DELETE(pAsyncRq);
		}
	}

	LOG_INFO(_T("Manual flush completed. All tasks processed."));
}

//***************************************************************************
// @brief MySQL 연결 풀을 정리합니다.
// @details 모든 연결 풀 객체를 삭제합니다.
//***************************************************************************
void CMySQLAsyncSrv::ClearMySQLConnPools()
{
	if( _pMySQLConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pMySQLConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pMySQLConnPools);
}

//***************************************************************************
// @brief 명령 핸들러를 등록합니다.
// @param command 명령 식별자
// @param handler 명령 핸들러 객체
// @return 등록된 핸들러
//***************************************************************************
std::shared_ptr<CDBAsyncSrvHandler> CMySQLAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
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
bool CMySQLAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitMySQL(dbNodeVec, nMaxThreadCnt);
}

//***************************************************************************
// @brief MySQL 초기화
// @param dbNodeVec DB 노드 벡터
// @param nMaxThreadCnt 최대 스레드 수
// @return 성공 여부
//***************************************************************************
bool CMySQLAsyncSrv::InitMySQL(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	_bStopThread.store(false);

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pMySQLConnPools = new CMySQLConnPool * [_nDBCount]();

	CMySQLConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	int32 nIdx = 0;
	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pMySQLConnPools[nIdx] = new CMySQLConnPool(_nMaxThreadCnt);
		if( nullptr == _pMySQLConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc CMySQLConnPool"));
			ClearMySQLConnPools();
			return false;
		}

		if( false == _pMySQLConnPools[nIdx]->Init(iter._tszDBHost, iter._tszDBUserId, iter._tszDBPasswd, iter._tszDBName, iter._nPort, reconnectCfg) )
		{
			LOG_ERROR(_T("Failed to Initialize CMySQLConnPool"));
			ClearMySQLConnPools();
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
void CMySQLAsyncSrv::StartIoThreads()
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
bool CMySQLAsyncSrv::RunningThread()
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
bool CMySQLAsyncSrv::Action()
{
	static std::atomic<uint64> cumulateCallCnt{ 0 };
	std::queue<st_DBAsyncRq*> localQueue;

	while( !_bStopThread.load() )
	{
		st_DBAsyncRq* pAsyncRq = Pop(localQueue);
		if( pAsyncRq == nullptr )
		{
			continue;
		}

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			LOG_ERROR(_T("Error not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
			SAFE_DELETE(pAsyncRq);
			continue;
		}

		uint64 startTick = _GetTickCount();

		std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq);

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

				int nSize = Push(pAsyncRq);

				if( nSize == 0 )
				{
					SAFE_DELETE(pAsyncRq);
				}

				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
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
		SAFE_DELETE(pAsyncRq);
	}

	return true;
}

//***************************************************************************
// @brief 큐에 요청을 추가합니다.
// @param pAsyncRq 비동기 요청 객체
// @return 큐 크기 (0이면 실패)
//***************************************************************************
int CMySQLAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	if( _bStopThread.load() ) return 0;

	int queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(pAsyncRq));
	_cva.notify_one();
	return queueSize;
}

//***************************************************************************
// @brief 큐에서 요청을 꺼냅니다.
// @param localQueue 로컬 큐
// @return 요청 객체 (없으면 nullptr)
//***************************************************************************
st_DBAsyncRq* CMySQLAsyncSrv::Pop(std::queue<st_DBAsyncRq*>& localQueue)
{
	if( !localQueue.empty() )
	{
		st_DBAsyncRq* pAsyncRq = localQueue.front();
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
		if( MAX_WARNING_QUERY_QUEUE_SIZE <= currentSize && currentSize <= MAX_WARNING_QUERY_QUEUE_SIZE + 10 )
			LOG_ERROR(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(currentSize));

		int currentCount = static_cast<int>(currentSize);
		if( currentCount > _nLastWarnedQueueSize )
		{
			_nLastWarnedQueueSize = currentCount;
			LOG_WARNING(_T("Async DB Call Queue size... : [%d]"), _nLastWarnedQueueSize);
		}

		// 전체를 다 스왑해 오는 대신, 한 번에 처리할 적정량(64개)만 떼어옴 (스케일링 병목 개선)
		_queueDBAsyncRq.SwapChunk(localQueue, 64);
	}

	_cvProducer.notify_one();

	if( localQueue.empty() )
		return nullptr;

	st_DBAsyncRq* pAsyncRq = localQueue.front();
	localQueue.pop();
	return pAsyncRq;
}

//***************************************************************************
// @brief 첫 번째 MySQL 연결 풀 반환
// @return 계정용 MySQL 연결 풀
//***************************************************************************
CMySQLConnPool* CMySQLAsyncSrv::GetAccountConnPool(void)
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 0);
	return _pMySQLConnPools[0];
}

//***************************************************************************
// @brief ID 기반 MySQL 연결 풀 반환
// @param m_nID DB ID
// @return 해당 MySQL 연결 풀
//***************************************************************************
CMySQLConnPool* CMySQLAsyncSrv::GetMySQLConnPool(uint64 m_nID)
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pMySQLConnPools[nIdx];
}

//***************************************************************************
// @brief 로그용 MySQL 연결 풀 반환
// @return 로그 DB 연결 풀
//***************************************************************************
CMySQLConnPool* CMySQLAsyncSrv::GetLogConnPool()
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 2);
	return _pMySQLConnPools[2];
}