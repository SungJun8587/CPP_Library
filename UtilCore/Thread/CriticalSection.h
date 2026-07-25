
//***************************************************************************
// CriticalSection.h : interface and implementation for the CCriticalSection class.
//
//***************************************************************************

#ifndef __CRITICALSECTION_H__
#define __CRITICALSECTION_H__

#ifndef __BASEMACRO_H__
#include <BaseMacro.h>
#endif

class CCriticalSection
{
public:
	explicit CCriticalSection(DWORD dwSpinCount = 4000)
	{
		//InitializeCriticalSection(&m_csSync);
		if( !InitializeCriticalSectionAndSpinCount(&m_csSync, dwSpinCount) )
		{
#pragma warning(push)
#pragma warning(disable:6011)
			CRASH;
#pragma warning(pop)
		}
	}

	virtual ~CCriticalSection(void)
	{
		DeleteCriticalSection(&m_csSync);
	}

	void Lock(void)
	{
		EnterCriticalSection(&m_csSync);
	}

	void Unlock(void)
	{
		LeaveCriticalSection(&m_csSync);
	}

private:
	CCriticalSection(const CCriticalSection& rhs);
	CCriticalSection& operator=(const CCriticalSection& rhs);

	CRITICAL_SECTION m_csSync;
};

class CCSLockGuard
{
public:
	explicit CCSLockGuard(CCriticalSection& criticalSection)
		: m_criticalSection(criticalSection)
	{
		m_criticalSection.Lock();
	}

	virtual ~CCSLockGuard(void)
	{
		m_criticalSection.Unlock();
	}

private:
	// 복사 및 대입 연산자 차단 (유일한 락 소유권 보장)
	CCSLockGuard(const CCSLockGuard& rhs);
	CCSLockGuard& operator=(const CCSLockGuard& rhs);

	CCriticalSection& m_criticalSection;
};

#endif // ndef __CRITICALSECTION_H__

