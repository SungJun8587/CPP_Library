
//***************************************************************************
// Thread.cpp : implementation of the CThread class.
//
//***************************************************************************

#include "pch.h"
#include "Thread.h"

typedef struct _st_ThreadArg
{
	CThread* pThread;
	__int32		nThreadIdx;
} st_ThreadArg;


unsigned int WINAPI CThread::ThreadMain(void* ptr)
{
	st_ThreadArg* pArg = (st_ThreadArg*)ptr;
	CThread* pThread = pArg->pThread;
	__int32			nThreadIdx = pArg->nThreadIdx;
	 
	pThread->RunningThread(nThreadIdx);

	delete pArg;

	return 0;
}

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CThread::CThread(void)
{
	m_nThreadCnt = 0;
	m_nRunningThreadCnt = 0;
}

//***************************************************************************
//
bool CThread::StartIoThreads(const __int32& nThreadCnt)
{
	st_ThreadArg* pArg;
	m_nThreadCnt = nThreadCnt;
	m_nRunningThreadCnt = nThreadCnt;

	__int32	nThreadId;
	HANDLE	hThread;
	for( __int32 i = 0; i < m_nThreadCnt; ++i )
	{
		pArg = new st_ThreadArg;
		pArg->pThread = this;
		pArg->nThreadIdx = i;

		hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadMain, (void*)pArg, 0, reinterpret_cast<unsigned __int32*>(&nThreadId));
		if( !hThread )
		{
			delete pArg;
			return false;
		}
	}

	return true;
}
