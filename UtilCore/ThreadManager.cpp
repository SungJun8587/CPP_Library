
//***************************************************************************
// ThreadManager.cpp : implementation of the CThreadManager class.
//
//***************************************************************************

#include "pch.h"
#include "ThreadManager.h"

__declspec(thread) __int32	CThreadManager::m_nTlsIdx = 0;
