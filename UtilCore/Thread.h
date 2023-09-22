
//***************************************************************************
// Thread.h : interface for the CThread class.
//
//***************************************************************************

#ifndef __THREAD_H__
#define __THREAD_H__

#include <process.h>
#include <ObjectPool.h>

class CThread : public CPoolObj
{
public:
	CThread(void);
	virtual ~CThread(void)	{}

	bool			StartIoThreads(const __int32& nThreadCnt);
	virtual bool	RunningThread(const __int32& nThreadIdx) = 0;

	static unsigned int WINAPI ThreadMain(void*);

private:
	CThread(const CThread& rhs);
	CThread& operator=(const CThread& rhs);

protected:
	__int32			m_nThreadCnt;
	long volatile	m_nRunningThreadCnt;
};

#endif // ndef __THREAD_H__
