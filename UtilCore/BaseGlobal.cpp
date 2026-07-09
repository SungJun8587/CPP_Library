
//***************************************************************************
// This file contains the implementation of processing for external variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseGlobal.h"

#ifdef __MEMORY_H__
	CMemory* gpMemory = nullptr;
#endif

#ifdef __GLOBALQUEUE_H__
	CGlobalQueue* gpGlobalQueue = nullptr;
#endif

#ifdef __JOBTIMER_H__
	CJobTimer* gpJobTimer = nullptr;
#endif

#ifdef __THREADMANAGER_H__
	CThreadManager* gpThreadManager = nullptr;
#endif

#ifdef __DEADLOCKPROFILER_H__
	CDeadLockProfiler* gpDeadLockProfiler = nullptr;
#endif

//***************************************************************************
// 프로그램 시작시(main 함수)에서 호출
//	int main() 
//  {
//		BaseGlobal::Init();
//
//		// 서버 로직
//
//		BaseGlobal::Destroy();
//		return 0;
//	}
//
namespace BaseGlobal
{
	void Init()
	{
#ifdef __MEMORY_H__
		gpMemory = new CMemory();
#endif

#ifdef __GLOBALQUEUE_H__
		gpGlobalQueue = new CGlobalQueue();
#endif

#ifdef __JOBTIMER_H__
		gpJobTimer = new CJobTimer();
#endif

#ifdef __THREADMANAGER_H__
		gpThreadManager = new CThreadManager();
#endif	

#ifdef __DEADLOCKPROFILER_H__
		gpDeadLockProfiler = new CDeadLockProfiler();
#endif	
	}

	void Destroy()
	{
#ifdef __MEMORY_H__
		if( gpMemory != nullptr ) delete gpMemory;
#endif	

#ifdef __GLOBALQUEUE_H__
		if (gpGlobalQueue != nullptr) delete gpGlobalQueue;
#endif

#ifdef __JOBTIMER_H__
		if (gpJobTimer != nullptr) delete gpJobTimer;
#endif

#ifdef __THREADMANAGER_H__
		if( gpThreadManager != nullptr ) delete gpThreadManager;
#endif	

#ifdef __DEADLOCKPROFILER_H__
		if( gpDeadLockProfiler != nullptr ) delete gpDeadLockProfiler;
#endif	
	}
}