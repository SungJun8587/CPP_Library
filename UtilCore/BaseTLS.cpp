
//***************************************************************************
// This file contains the implementation of processing for external thread_local variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseTLS.h"

thread_local uint32				LThreadId = 0;
thread_local uint64				LEndTickCount = 0;

#ifdef __DEADLOCKPROFILER_H__
	thread_local CStack<int32>		LLockStack;
#endif

#ifdef __JOBQUEUE_H__
	thread_local CJobQueue*			LCurrentJobQueue = nullptr;
#endif