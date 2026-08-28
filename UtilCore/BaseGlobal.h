
//***************************************************************************
// This File include Information about extern variables.
// 
//***************************************************************************

#ifndef UC_BASEGLOBAL_H
#define UC_BASEGLOBAL_H

#ifdef UC_MEMORY_H
	class CMemory;
	extern class CMemory*	gpMemory;
#endif

#ifdef UC_GLOBALQUEUE_H
	class CGlobalQueue;
	extern class CGlobalQueue*	gpGlobalQueue;
#endif

#ifdef UC_JOBTIMER_H
	class CJobTimer;
	extern class CJobTimer*		gpJobTimer;
#endif

#ifdef UC_THREADMANAGER_H	
	class CThreadManager;
	extern class CThreadManager*	gpThreadManager;
#endif

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
	class CDeadLockProfiler;
	extern class CDeadLockProfiler* gpDeadLockProfiler;
#endif

namespace BaseGlobal
{
	void Init();
	void Destroy();
}

#endif // ndef UC_BASEGLOBAL_H
