
//***************************************************************************
// This File include Information about extern variables.
// 
//***************************************************************************

#ifndef __BASEGLOBAL_H__
#define __BASEGLOBAL_H__

#pragma once

#ifdef __MEMORY_H__
	extern class CMemory*	gpMemory;
#endif

#ifdef __THREADMANAGER_H__	
	extern class CThreadManager*	gpThreadManager;
#endif

#ifdef __SPINLOCK_H__
	extern class CDeadLockProfiler* gpDeadLockProfiler;
#endif

#endif // ndef __BASEGLOBAL_H__
