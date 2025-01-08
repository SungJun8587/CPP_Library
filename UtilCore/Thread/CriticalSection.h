
//***************************************************************************
// CriticalSection.h : interface and implementation for the CCriticalSection class.
//
//***************************************************************************

#ifndef __CRITICALSECTION_H__
#define __CRITICALSECTION_H__

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

#endif // ndef __CRITICALSECTION_H__

