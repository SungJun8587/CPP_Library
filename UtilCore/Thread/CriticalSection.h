
//***************************************************************************
// CriticalSection.h : interface and implementation for the CCriticalSection class.
//
//***************************************************************************

#ifndef __CRITICALSECTION_H__
#define __CRITICALSECTION_H__

#ifndef __BASEMACRO_H__
#include <BaseMacro.h>
#endif

//***************************************************************************
// @brief Windows 임계 구역(Critical Section)을 래핑하여 스레드 동기화를 제공하는 클래스입니다.
// @detail 스핀 카운트를 지원하는 InitializeCriticalSectionAndSpinCount를 사용하여 성능을 최적화하며,
//         복사 생성자와 대입 연산자를 차단하여 안정성을 보장합니다.
//***************************************************************************
class CCriticalSection
{
public:
	//***************************************************************************
	// @brief CCriticalSection 객체를 생성하고 임계 구역을 초기화합니다.
	// @param dwSpinCount 임계 구역의 스핀 카운트 (기본값: 4000)
	//***************************************************************************
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

	//***************************************************************************
	// @brief 소멸자. 할당된 임계 구역 리소스를 해제합니다.
	//***************************************************************************
	virtual ~CCriticalSection(void)
	{
		DeleteCriticalSection(&m_csSync);
	}

	//***************************************************************************
	// @brief 임계 구역에 진입(잠금)합니다.
	//***************************************************************************
	void Lock(void)
	{
		EnterCriticalSection(&m_csSync);
	}

	//***************************************************************************
	// @brief 임계 구역에서 이탈(잠금 해제)합니다.
	//***************************************************************************
	void Unlock(void)
	{
		LeaveCriticalSection(&m_csSync);
	}

private:
	CCriticalSection(const CCriticalSection& rhs);
	CCriticalSection& operator=(const CCriticalSection& rhs);

	CRITICAL_SECTION m_csSync;
};

//***************************************************************************
// @brief RAII 패턴을 사용하여 임계 구역의 잠금과 해제를 자동으로 관리하는 락 가드 클래스입니다.
// @detail 객체가 생성될 때 자동으로 락을 획득하고, 소멸할 때 자동으로 락을 해제합니다.
//***************************************************************************
class CCSLockGuard
{
public:
	//***************************************************************************
	// @brief CCSLockGuard 객체를 생성하고 대상 임계 구역을 잠급니다.
	// @param criticalSection 동기화에 사용할 CCriticalSection 객체 레퍼런스
	//***************************************************************************
	explicit CCSLockGuard(CCriticalSection& criticalSection)
		: m_criticalSection(criticalSection)
	{
		m_criticalSection.Lock();
	}

	//***************************************************************************
	// @brief 소멸자. 임계 구역의 잠금을 자동으로 해제합니다.
	//***************************************************************************
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