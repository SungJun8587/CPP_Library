
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
// SRWLock ��ü�� ���� �б� ������ ��û
void CRWLock::SharedLock(void)
{
	AcquireSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock ��ü�� ���� �б� ������ ���� 
void CRWLock::SharedUnLock(void)
{
	ReleaseSRWLockShared(&m_SRWLock);
}

//***************************************************************************
// SRWLock ��ü�� ���� �б�/���� ������ ��û
void CRWLock::ExclusiveLock(void)
{
	AcquireSRWLockExclusive(&m_SRWLock);
}

//***************************************************************************
// SRWLock ��ü�� ���� �б�/���� ������ ��û
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