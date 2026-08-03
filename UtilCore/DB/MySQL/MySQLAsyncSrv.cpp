
//***************************************************************************
// MySQLAsyncSrv.cpp : implementation of the CMySQLAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLAsyncSrv.h"

extern CThreadManager* gpThreadManager;

std::shared_ptr<CMySQLAsyncSrv> CMySQLAsyncSrv::Instance() {
	static std::shared_ptr<CMySQLAsyncSrv> instance = std::make_shared<CMySQLAsyncSrv>();
	return instance;
}

CMySQLAsyncSrv::CMySQLAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pMySQLConnPools = nullptr;
}

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

void CMySQLAsyncSrv::ClearMySQLConnPools()
{
	if( _pMySQLConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pMySQLConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pMySQLConnPools);
}

std::shared_ptr<CDBAsyncSrvHandler> CMySQLAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool CMySQLAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitMySQL(dbNodeVec, nMaxThreadCnt);
}

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

void CMySQLAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([this]() { RunningThread(); });
	}
}

bool CMySQLAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

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

int CMySQLAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	if( _bStopThread.load() ) return 0;

	int queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(pAsyncRq));
	_cva.notify_one();
	return queueSize;
}

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

CMySQLConnPool* CMySQLAsyncSrv::GetAccountConnPool(void)
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 0);
	return _pMySQLConnPools[0];
}

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

CMySQLConnPool* CMySQLAsyncSrv::GetLogConnPool()
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 2);
	return _pMySQLConnPools[2];
}