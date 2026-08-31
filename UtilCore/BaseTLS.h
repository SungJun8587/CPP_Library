
//***************************************************************************
// This File include Information about extern thread_local variables.
// 
//***************************************************************************

#ifndef UC_BASETLS_H
#define UC_BASETLS_H

extern thread_local uint32				LThreadId;
extern thread_local uint64				LEndTickCount;

#ifdef UC_DEADLOCKPROFILER_H
	extern thread_local std::stack<int32>	LLockStack;
#endif

#ifdef UC_SENDBUFFER_H
	extern thread_local CSendBufferChunkRef	LSendBufferChunk;
#endif

#ifdef UC_JOBQUEUE_H
	extern thread_local CJobQueue*		LCurrentJobQueue;
#endif

#endif // ndef UC_BASETLS_H
