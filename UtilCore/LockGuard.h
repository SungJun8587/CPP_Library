
//***************************************************************************
// LockGuard.h : interface and implementation for the CLockGuard class.
//
//***************************************************************************

#ifndef __LOCKGUARD_H__
#define __LOCKGUARD_H__

#pragma once

//***************************************************************************
//
template<typename LOCK>
class CLockGuard
{
public:
	explicit CLockGuard(LOCK& lock);
	virtual ~CLockGuard(void);

private:
	CLockGuard(CLockGuard const&);
	CLockGuard& operator=(CLockGuard const&);

	LOCK& m_Lock;
};


//***************************************************************************
//
template<typename LOCK>
CLockGuard<LOCK>::CLockGuard(LOCK& lock)
	: m_Lock(lock)
{
	m_Lock.Lock();
}

//***************************************************************************
//
template<typename LOCK>
CLockGuard<LOCK>::~CLockGuard(void)
{
	m_Lock.Unlock();
}

#endif // ndef __LOCKGUARD_H__
