
//***************************************************************************
// This File include Information about extern thread_local variables.
// 
//***************************************************************************

#ifndef __BASETLS_H__
#define __BASETLS_H__

#pragma once

extern thread_local uint32				LThreadId;
extern thread_local uint64				LEndTickCount;

#ifdef __DEADLOCKPROFILER_H__
	extern thread_local CStack<int32>	LLockStack;
#endif

#ifdef __JOBQUEUE_H__
	extern thread_local CJobQueue*		LCurrentJobQueue;
#endif

#endif // ndef __BASETLS_H__
