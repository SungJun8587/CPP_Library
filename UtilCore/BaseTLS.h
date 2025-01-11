
//***************************************************************************
// This File include Information about extern thread_local variables.
// 
//***************************************************************************

#ifndef __BASETLS_H__
#define __BASETLS_H__

#pragma once

#ifndef _STACK_
#include <stack>
#endif

extern thread_local uint32				LThreadId;
extern thread_local std::stack<int32>	LLockStack;

#endif // ndef __BASETLS_H__
