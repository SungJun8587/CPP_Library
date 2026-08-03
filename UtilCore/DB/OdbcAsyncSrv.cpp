//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"
#include <algorithm>

extern CThreadManager* gpThreadManager;

std::shared_ptr<COdbcAsyncSrv> COdbcAsyncSrv::Instance() {
	static std::shared_ptr<COdbcAsyncSrv> instance = std::make_shared<COdbcAsyncSrv>();
	return instance;
}

COdbcAsyncSrv::COdbcAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pOdbcConnPools = nullptr;
}

COdbcAsyncSrv::~COdbcAsyncSrv()
{
	FlushRemainingTasks();

	StopThread();
	Clear();
	ClearOdbcPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

void COdbcAsyncSrv::Clear()
{
	// CSwapQueue를 비우기 위해 전체 크기만큼 넉넉하게 척크 스왑하거나 통째로 비움
	std::queue<st_DBAsyncRq*> tempQueue;

	// 안전하게 현재 남은 전체 사이즈만큼 척크 스왑을 수행하여 tempQueue로 이동
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

void COdbcAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Main program requested to flush remaining async DB tasks..."));

	_bStopThread.store(true);
	_cva.notify_all();
	_cvProducer.notify_all();

	// 큐에 남아있는 작업들을 안전하게 모두 가져옴
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
			}
			else
			{
				LOG_ERROR(_T("Error not found command handler for task... callIdent: [%u]"), pAsyncRq->callIdent);
			}

			SubOutstandingRequest();
			SAFE_DELETE(pAsyncRq);
		}
	}

	LOG_INFO(_T("Manual flush completed. All tasks processed."));
}

void COdbcAsyncSrv::ClearOdbcPools()
{
	if( _pOdbcConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pOdbcConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pOdbcConnPools);
}

std::shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool COdbcAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitOdbc(dbNodeVec, nMaxThreadCnt);
}

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

void COdbcAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([this]() { RunningThread(); });
	}
}

bool COdbcAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

bool COdbcAsyncSrv::Action()
{
	static uint64 cumulateCallCnt = 0;
	std::queue<st_DBAsyncRq*> localQueue; // 소비자별 로컬 처리용 큐 (더블 버퍼링 스왑 대상)

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
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);

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
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);
#else
		uint64 endTick = _GetTickCount();
		if( 1000 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);
#endif

		SubOutstandingRequest();
		SAFE_DELETE(pAsyncRq);
	}

	return true;
}

int COdbcAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	if( _bStopThread.load() ) return 0;

	// [1번 수정] 락 안에서 푸시와 사이즈 조회를 원자적으로 처리하므로 이중 delete 위험 소멸
	int queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(pAsyncRq));

	_cva.notify_one();

	return queueSize;
}

st_DBAsyncRq* COdbcAsyncSrv::Pop(std::queue<st_DBAsyncRq*>& localQueue)
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

		// 전체를 다 가져오는 대신, 한 번에 처리할 적정량(예: 64개)만 떼어옴
		_queueDBAsyncRq.SwapChunk(localQueue, 64);
	}

	_cvProducer.notify_one();

	if( localQueue.empty() )
		return nullptr;

	st_DBAsyncRq* pAsyncRq = localQueue.front();
	localQueue.pop();
	return pAsyncRq;
}

COdbcConnPool* COdbcAsyncSrv::GetAccountOdbcConnPool(void)
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 0);
	return _pOdbcConnPools[0];
}

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

COdbcConnPool* COdbcAsyncSrv::GetLogOdbcConnPool()
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 2);
	return _pOdbcConnPools[2];
}