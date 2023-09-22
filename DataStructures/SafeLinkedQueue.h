
//***************************************************************************
// SafeLinkedQueue.h: interface for the CSafeLinkedQueue class.
//
//***************************************************************************

#ifndef __SAFELINKEDQUEUE_H__
#define __SAFELINKEDQUEUE_H__

#include <BaseLinkedQueue.h>
#include <RWLock.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedQueue : public CBaseLinkedQueue<TYPE>
{
public:
	int EnQueue(const TYPE Element);
	int DeQueue(TYPE &Element);
	BOOL IsEmpty();
	int GetSize();

#ifdef _UNICODE
	void Printing(wostream &StreamBuffer);
#else
	void Printing(ostream &StreamBuffer);
#endif

private:
	CRWLock		m_RWLock;
};

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::EnQueue(const TYPE Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedQueue<TYPE>::EnQueue(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::DeQueue(TYPE &Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedQueue<TYPE>::DeQueue(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::IsEmpty()
{
	BOOL bRet;

	m_RWLock.SharedLock();
	{
		bRet = CBaseLinkedQueue<TYPE>::IsEmpty();
	}
	m_RWLock.SharedUnLock();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::GetSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseLinkedQueue<TYPE>::GetSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedQueue<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedQueue<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#else
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedQueue<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedQueue<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

#endif // ndef __SAFELINKEDQUEUE_H__
