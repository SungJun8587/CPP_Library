
//***************************************************************************
// This File include Information about extern variables.
// 
//***************************************************************************

#ifndef __BASEGLOBAL_H__
#define __BASEGLOBAL_H__

#pragma once

#ifdef __MEMORY_H__
	class CMemory;
	extern class CMemory*	gpMemory;
#endif

#ifdef __GLOBALQUEUE_H__
	class CGlobalQueue;
	extern class CGlobalQueue*	gpGlobalQueue;
#endif

#ifdef __JOBTIMER_H__
	class CJobTimer;
	extern class CJobTimer*		gpJobTimer;
#endif

#ifdef __THREADMANAGER_H__	
	class CThreadManager;
	extern class CThreadManager*	gpThreadManager;
#endif

#ifdef __DEADLOCKPROFILER_H__
	class CDeadLockProfiler;
	extern class CDeadLockProfiler* gpDeadLockProfiler;
#endif

namespace BaseGlobal
{
	void Init();
	void Destroy();
}

#endif // ndef __BASEGLOBAL_H__
