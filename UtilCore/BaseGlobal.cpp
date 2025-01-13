
//***************************************************************************
// This file contains the implementation of processing for external variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseGlobal.h"

#ifdef __MEMORY_H__
	CMemory* gpMemory = nullptr;
#endif

#ifdef __THREADMANAGER_H__
	CThreadManager* gpThreadManager = nullptr;
#endif

#ifdef __DEADLOCKPROFILER_H__
	CDeadLockProfiler* gpDeadLockProfiler = nullptr;
#endif

class CBaseGlobal
{
public:
	CBaseGlobal()
	{
#ifdef __MEMORY_H__
		gpMemory = new CMemory();
#endif

#ifdef __THREADMANAGER_H__
		gpThreadManager = new CThreadManager();
#endif	

#ifdef __DEADLOCKPROFILER_H__
		gpDeadLockProfiler = new CDeadLockProfiler();
#endif	
	}

	~CBaseGlobal()
	{
#ifdef __MEMORY_H__
		if( gpMemory != nullptr ) delete gpMemory;
#endif	

#ifdef __THREADMANAGER_H__
		if( gpThreadManager != nullptr ) delete gpThreadManager;
#endif	

#ifdef __DEADLOCKPROFILER_H__
		if( gpDeadLockProfiler != nullptr ) delete gpDeadLockProfiler;
#endif	
	}
} GCBaseGlobal;