
//***************************************************************************
// AdoAsyncSrv.cpp : implementation of the CAdoAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "AdoAsyncSrv.h"
#include <algorithm>

extern CThreadManager* gpThreadManager;

std::shared_ptr<CAdoAsyncSrv> CAdoAsyncSrv::Instance() {
	static std::shared_ptr<CAdoAsyncSrv> instance = std::make_shared<CAdoAsyncSrv>();
	return instance;
}

CAdoAsyncSrv::CAdoAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pAdoConnPools = nullptr;
}

CAdoAsyncSrv::~CAdoAsyncSrv()
{
	FlushRemainingTasks();
	StopThread();
	Clear();
	ClearAdoPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

void CAdoAsyncSrv::Clear()
{
	std::unique_lock<std::mutex> lockGuard(_mutex);

	while( !_queueDBAsyncRq.empty() )
	{
		st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}
}

void CAdoAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Main program requested to flush remaining async ADO tasks..."));

	_bStopThread.store(true);
	_cva.notify_all();
	_cvProducer.notify_all();

	CQueue<st_DBAsyncRq*> tempQueue;
	{
		std::unique_lock<std::mutex> lockGuard(_mutex);
		tempQueue = std::move(_queueDBAsyncRq);
	}

	int32 remainingCount = static_cast<int32>(tempQueue.size());
	if( remainingCount > 0 )
	{
		while( !tempQueue.empty() )
		{
			st_DBAsyncRq* pAsyncRq = tempQueue.front();
			tempQueue.pop();

			if( pAsyncRq == nullptr ) continue;

			COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
			if( it != _mapCommand.end() )
			{
				std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
				command->ProcessAsyncCall(pAsyncRq);
			}

			SubOutstandingRequest();
			SAFE_DELETE(pAsyncRq);
		}
	}
}

void CAdoAsyncSrv::ClearAdoPools()
{
	if( _pAdoConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pAdoConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pAdoConnPools);
}

std::shared_ptr<CDBAsyncSrvHandler> CAdoAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool CAdoAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitAdo(dbNodeVec, nMaxThreadCnt);
}

bool CAdoAsyncSrv::InitAdo(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	_bStopThread.store(false);

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pAdoConnPools = new CAdoConnPool * [_nDBCount]();

	CAdoConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	int32 nIdx = 0;
	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pAdoConnPools[nIdx] = new CAdoConnPool(_nMaxThreadCnt);
		if( nullptr == _pAdoConnPools[nIdx] )
		{
			ClearAdoPools();
			return false;
		}

		// DBNode에 정의된 타임아웃(기본 5초 등)을 전달
		if( false == _pAdoConnPools[nIdx]->Init(iter._dbClass, iter._tszDSN, 5, reconnectCfg) )
		{
			ClearAdoPools();
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	return true;
}

void CAdoAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([this]() { RunningThread(); });
	}
}

bool CAdoAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

bool CAdoAsyncSrv::Action()
{
	while( !_bStopThread.load() )
	{
		st_DBAsyncRq* pAsyncRq = Pop();
		if( pAsyncRq == nullptr ) continue;

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			SAFE_DELETE(pAsyncRq);
			continue;
		}

		std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq);

		if( Ret != EDBReturnType::OK )
		{
			if( Ret == EDBReturnType::TIMEOUT && pAsyncRq->bReTry == false )
			{
				pAsyncRq->bReTry = true;
				int nSize = Push(pAsyncRq);
				if( nSize == 0 ) SAFE_DELETE(pAsyncRq);
				continue;
			}
		}

		SubOutstandingRequest();
		SAFE_DELETE(pAsyncRq);
	}

	return true;
}

int CAdoAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	int queueSize = 0;
	{
		std::unique_lock<std::mutex> lockGuard(_mutex);
		if( _bStopThread.load() ) return 0;

		_queueDBAsyncRq.push(pAsyncRq);
		queueSize = static_cast<int>(_queueDBAsyncRq.size());
	}
	_cva.notify_one();
	return queueSize;
}

st_DBAsyncRq* CAdoAsyncSrv::Pop()
{
	st_DBAsyncRq* pAsyncRq = nullptr;
	{
		std::unique_lock<std::mutex> lockGuard(_mutex);
		_cva.wait(lockGuard, [this]() { return !_queueDBAsyncRq.empty() || _bStopThread.load(); });

		if( _bStopThread.load() && _queueDBAsyncRq.empty() ) return nullptr;

		pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
	}
	_cvProducer.notify_one();
	return pAsyncRq;
}

CAdoConnPool* CAdoAsyncSrv::GetAccountAdoConnPool(void)
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 0);
	return _pAdoConnPools[0];
}

CAdoConnPool* CAdoAsyncSrv::GetAdoConnPool(uint64 m_nID)
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pAdoConnPools[nIdx];
}

CAdoConnPool* CAdoAsyncSrv::GetLogAdoConnPool()
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 2);
	return _pAdoConnPools[2];
}