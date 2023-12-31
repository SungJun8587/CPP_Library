
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
	m_pOdbcConnPools = nullptr;
	m_nDBCount = 0;
	m_nMaxThreadCnt = 0;
	m_bOpen = false;
}

COdbcAsyncSrv::~COdbcAsyncSrv()
{
}

//***************************************************************************
//
CDBAsyncSrvHandler* COdbcAsyncSrv::Regist(const BYTE command, CDBAsyncSrvHandler* const handler)
{
	mapCommand_.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

//***************************************************************************
//
bool COdbcAsyncSrv::StartService(INT32 nMaxThreadCnt, std::vector<CDBNode> DBNodeVec)
{
	return InitOdbc(nMaxThreadCnt, DBNodeVec);
}

//***************************************************************************
//
bool COdbcAsyncSrv::InitOdbc(INT32 nMaxThreadCnt, std::vector<CDBNode> DBNodeVec)
{
	m_nMaxThreadCnt = nMaxThreadCnt;

	m_nDBCount = static_cast<int32>(DBNodeVec.size());
	if( m_nDBCount <= 0 )
		return true;

	m_pOdbcConnPools = new COdbcConnPool*[m_nDBCount];
	int32 nIdx = 0;

	for( auto& iter : DBNodeVec )
	{
		if( nIdx >= m_nDBCount ) break;

		m_pOdbcConnPools[nIdx] = new COdbcConnPool(m_nMaxThreadCnt);
		if( NULL == m_pOdbcConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc COdbcConnPool"));
			return false;
		}

		if( false == m_pOdbcConnPools[nIdx]->Init(iter.m_dbClass, iter.m_tszDSN) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			return false;
		}

		++nIdx;
	}

	m_bOpen = true;
	StartIoThreads(m_nMaxThreadCnt);

	return true;
}

//***************************************************************************
//
bool COdbcAsyncSrv::RunningThread(const __int32& nThreadIdx)
{
	CThreadManager::SetTlsValue(nThreadIdx);

	if( m_bOpen )
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
	while( 1 )
	{
		st_DBAsyncRq* pAsyncRq = Pop();	// DB 콜 구조체를 큐에서 가저오기
		if( pAsyncRq == nullptr )
		{
			Sleep(1);
			continue;
		}

		COMMAND_MAP::iterator it = mapCommand_.find(pAsyncRq->callIdent);	// 구조체 내 식별자 확인 및 핸들러 풀에서 식별자로 핸들러 가져온다.
		if( mapCommand_.end() == it )
		{
			LOG_ERROR(_T("err not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
			SAFE_DELETE(pAsyncRq);
			continue;
		}

		uint64 startTick = _GetTickCount();

		CDBAsyncSrvHandler* command = static_cast<CDBAsyncSrvHandler*>(it->second);
		int nRet = command->ProcessAsyncCall(pAsyncRq);	// 패킷 처리
		if( nRet != static_cast<short>(DB_RETURN_TYPE::OK) )
		{
			LOG_ERROR(_T("Failed Async Call... callIdent: [%u]"), pAsyncRq->callIdent);

			// TODO.. 
			// DB 처리에서 false 리턴하면 이곳으로 온다.
			if( nRet == static_cast<short>(DB_RETURN_TYPE::TIMEOUT) && pAsyncRq->bReTry == false )
			{
				uint64 endTick = _GetTickCount();
				if( 300 <= endTick - startTick )
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%I64u], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, nRet, pAsyncRq->callIdent);

				pAsyncRq->bReTry = true;
				int nSize = Push(pAsyncRq);
				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), pAsyncRq->callIdent, nSize);
				continue;
			}
		}

		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%I64u], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, nRet, pAsyncRq->callIdent);

		SAFE_DELETE(pAsyncRq);
	}
	return true;
}

//***************************************************************************
//
int COdbcAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	CLockGuard<CCriticalSection>	lockGuard(lock_queueDBAsyncRq_);

	queueDBAsyncRq_.push(pAsyncRq);

	return static_cast<int>(queueDBAsyncRq_.size());
}

//***************************************************************************
//
st_DBAsyncRq* COdbcAsyncSrv::Pop()
{
	static int queueCount = 2;

	CLockGuard<CCriticalSection>	lockGuard(lock_queueDBAsyncRq_);

	if( 0 == queueDBAsyncRq_.size() )
		return NULL;

	st_DBAsyncRq* pAsyncRq = queueDBAsyncRq_.front();
	queueDBAsyncRq_.pop();

	if( MAX_WARNING_QUERY_QUEUE_SIZE <= queueDBAsyncRq_.size() && queueDBAsyncRq_.size() <= MAX_WARNING_QUERY_QUEUE_SIZE + 10 )
		LOG_ERROR(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(queueDBAsyncRq_.size()));

	if( queueDBAsyncRq_.size() > queueCount )
	{
		queueCount = (int)queueDBAsyncRq_.size();
		LOG_WARNING(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(queueDBAsyncRq_.size()));
	}

	return pAsyncRq;
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetAccountOdbcConnPool(void)
{
	if( m_pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	if( 0 == m_nDBCount )
	{
		LOG_ERROR(_T("[%s]m_nDBCount Is Zero DBCount[%d]"), __TFUNCTION__, m_nDBCount);
		return nullptr;
	}

	return m_pOdbcConnPools[0];
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool(uint64 m_nID)
{
	if( m_pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	int32 nIdx = m_nDBCount - 1;
 	if (2 < m_nDBCount)
 	{
 		if (0 < m_nID)
 			nIdx = (m_nID % (m_nDBCount - 1)) + 1;
 	}

	return m_pOdbcConnPools[nIdx];
}

//***************************************************************************
//
COdbcConnPool* COdbcAsyncSrv::GetLogOdbcConnPool()
{
	if( m_pOdbcConnPools == nullptr )
	{
		LOG_ERROR(_T("[%s]m_pOdbcConnPools Is Null"), __TFUNCTION__);
		return nullptr;
	}

	return m_pOdbcConnPools[2];
}