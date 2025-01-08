
//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

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
	Clear();
	SAFE_DELETE_ARRAY(_pOdbcConnPools);
	_bStopThread = true;
	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

//***************************************************************************
//
void COdbcAsyncSrv::Clear()
{
	std::unique_lock lockGuard(_mutex);

	// 프로그램 종료시 큐 내용 초기화
	while( !_queueDBAsyncRq.empty() )
	{
		st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}

	for( int32 i = 0; i < _nDBCount; i++ )
		SAFE_DELETE(_pOdbcConnPools[i]);
}

//***************************************************************************
//
shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

//***************************************************************************
//
bool COdbcAsyncSrv::StartService(CVector<CDBNode> DBNodeVec, INT32 nMaxThreadCnt)
{
	return InitOdbc(DBNodeVec, nMaxThreadCnt);
}

//***************************************************************************
//
bool COdbcAsyncSrv::InitOdbc(CVector<CDBNode> DBNodeVec, INT32 nMaxThreadCnt)
{
	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());	// Thread Count는 Core 갯수만큼만 생성
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(DBNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pOdbcConnPools = new COdbcConnPool * [_nDBCount];
	int32 nIdx = 0;

	for( auto& iter : DBNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pOdbcConnPools[nIdx] = new COdbcConnPool(_nMaxThreadCnt);
		if( NULL == _pOdbcConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc COdbcConnPool"));
			return false;
		}

		if( false == _pOdbcConnPools[nIdx]->Init(iter.m_dbClass, iter.m_tszDSN) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	StartIoThreads(_nMaxThreadCnt);

	return true;
}

//***************************************************************************
//
void COdbcAsyncSrv::StartIoThreads(INT32 nMaxThreadCnt)
{
	for( int32 i = 0; i < nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([=]() {
			RunningThread();
		});
	}
}

//***************************************************************************
//
bool COdbcAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}

	return true;
}

//***************************************************************************
//
bool COdbcAsyncSrv::Action()
{
	static uint64 cumulateCallCnt = 0;
	while( !_bStopThread )
	{
		st_DBAsyncRq* pAsyncRq = Pop();	// DB 콜 구조체를 큐에서 가져오기
		if( pAsyncRq == nullptr )
		{
			Sleep(1);
			continue;
		}

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);	// 구조체 내 식별자 확인 및 핸들러 풀에서 식별자로 핸들러 가져온다.
		if( _mapCommand.end() == it )
		{
			LOG_ERROR(_T("Error not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
			SAFE_DELETE(pAsyncRq);
			continue;
		}

		uint64 startTick = _GetTickCount();

		shared_ptr<CDBAsyncSrvHandler> command = static_cast<shared_ptr<CDBAsyncSrvHandler>>(it->second);
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq);	// 패킷 처리
		if( Ret != EDBReturnType::OK )
		{
			LOG_ERROR(_T("Failed Async Call... callIdent: [%u]"), pAsyncRq->callIdent);

			// TODO.. 
			// DB 처리에서 false 리턴하면 이곳으로 온다.
			if( Ret == EDBReturnType::TIMEOUT && pAsyncRq->bReTry == false )
			{
				uint64 endTick = _GetTickCount();
				if( 300 <= endTick - startTick )
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);

				st_DBAsyncRq* copyAsyncRq = new st_DBAsyncRq{ *pAsyncRq };
				SAFE_DELETE(pAsyncRq);

				copyAsyncRq->bReTry = true;

				int nSize = Push(copyAsyncRq);
				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), pAsyncRq->callIdent, nSize);
				continue;
			}
		}

		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);

		SAFE_DELETE(pAsyncRq);
	}
	return true;
}

//***************************************************************************
//
int COdbcAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	std::unique_lock lockGuard(_mutex);

	_queueDBAsyncRq.push(pAsyncRq);

	return static_cast<int>(_queueDBAsyncRq.size());
}

//***************************************************************************
//
st_DBAsyncRq* COdbcAsyncSrv::Pop()
{
	static int queueCount = 2;

	std::unique_lock lockGuard(_mutex);

	if( 0 == _queueDBAsyncRq.size() )
		return NULL;

	st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
	_queueDBAsyncRq.pop();

	if( MAX_WARNING_QUERY_QUEUE_SIZE <= _queueDBAsyncRq.size() && _queueDBAsyncRq.size() <= MAX_WARNING_QUERY_QUEUE_SIZE + 10 )
		LOG_ERROR(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(_queueDBAsyncRq.size()));

	if( _queueDBAsyncRq.size() > queueCount )
	{
		queueCount = (int)_queueDBAsyncRq.size();
		LOG_WARNING(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(_queueDBAsyncRq.size()));
	}

	return pAsyncRq;
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetAccountOdbcConnPool(void)
{
	if( _pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	if( 0 == _nDBCount )
	{
		LOG_ERROR(_T("[%s]m_nDBCount Is Zero DBCount[%d]"), __TFUNCTION__, _nDBCount);
		return nullptr;
	}

	return _pOdbcConnPools[0];
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool(uint64 m_nID)
{
	if( _pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pOdbcConnPools[nIdx];
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetLogOdbcConnPool()
{
	if( _pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	return _pOdbcConnPools[2];
}