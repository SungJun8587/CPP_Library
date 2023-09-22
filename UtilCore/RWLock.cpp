
//***************************************************************************
// RWLock.cpp: implementation of the CRWLock class.
//
//***************************************************************************

#include "pch.h"
#include "RWLock.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CRWLock::CRWLock(void)
{
	InitializeSRWLock(&m_SRWLock);
}

//***************************************************************************
//
CRWLock::~CRWLock(void)
{
}

//***************************************************************************
// SRWLock 객체에 대한 읽기 권한을 요청
void CRWLock::SharedLock(void)
{
	AcquireSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기 권한을 해제 
void CRWLock::SharedUnLock(void)
{
	ReleaseSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기/쓰기 권한을 요청
void CRWLock::ExclusiveLock(void)
{
	AcquireSRWLockExclusive(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기/쓰기 권한을 요청
void CRWLock::ExclusiveUnLock(void)
{
#ifdef _MSC_VER
#pragma warning(disable:26110) // Warning C26110: Caller failing to hold lock 'handle' before calling function 'ReleaseSRWLockExclusive'.
#endif
	ReleaseSRWLockExclusive(&m_SRWLock);
#ifdef _MSC_VER
#pragma warning (default:26110)
#endif
}