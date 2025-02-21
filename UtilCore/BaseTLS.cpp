
//***************************************************************************
// This file contains the implementation of processing for external thread_local variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseTLS.h"

thread_local uint32				LThreadId = 0;

#ifdef __DEADLOCKPROFILER_H__
thread_local std::stack<int32>	LLockStack;
#endif