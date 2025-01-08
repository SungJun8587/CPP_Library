
//***************************************************************************
// SRWLock.cpp: implementation of the CSRWLock class.
//
//***************************************************************************

#include "pch.h"
#include "SRWLock.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CSRWLock::CSRWLock(void)
{
	InitializeSRWLock(&m_SRWLock);
}

//***************************************************************************
//
CSRWLock::~CSRWLock(void)
{
}

//***************************************************************************
// SRWLock 객체에 대한 읽기 권한을 요청
void CSRWLock::SharedLock(void)
{
	AcquireSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기 권한을 해제 
void CSRWLock::SharedUnLock(void)
{
	ReleaseSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기/쓰기 권한을 요청
void CSRWLock::ExclusiveLock(void)
{
	AcquireSRWLockExclusive(&m_SRWLock);
}

//***************************************************************************
// SRWLock 객체에 대한 읽기/쓰기 권한을 요청
void CSRWLock::ExclusiveUnLock(void)
{
#ifdef _MSC_VER
#pragma warning(disable:26110) // Warning C26110: Caller failing to hold lock 'handle' before calling function 'ReleaseSRWLockExclusive'.
#endif
	ReleaseSRWLockExclusive(&m_SRWLock);
#ifdef _MSC_VER
#pragma warning (default:26110)
#endif
}