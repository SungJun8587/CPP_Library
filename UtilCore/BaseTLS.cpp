
//***************************************************************************
// This file contains the implementation of processing for external thread_local variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseTLS.h"

thread_local uint32				LThreadId = 0;
thread_local uint64				LEndTickCount = 0;

#ifdef UC_DEADLOCKPROFILER_H
	thread_local std::stack<int32>		LLockStack;
#endif

#ifdef UC_SENDBUFFER_H
	thread_local CSendBufferChunkRef	LSendBufferChunk;
#endif

#ifdef UC_JOBQUEUE_H
	thread_local CJobQueue*				LCurrentJobQueue = nullptr;
#endif