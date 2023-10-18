
//***************************************************************************
// SafeLinkedQueue.h: interface for the CSafeLinkedDeque class.
//
//***************************************************************************

#ifndef __SAFELINKEDQUEUE_H__
#define __SAFELINKEDQUEUE_H__

#include <BaseLinkedDeque.h>
#include <RWLock.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedDeque : public CBaseLinkedDeque<TYPE>
{
public:
	int PushFront(const TYPE Element);
	int PopFront(TYPE &Element);

	int PushBack(const TYPE Element);
	int PopBack(TYPE &Element);

	bool IsEmpty();
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
int CSafeLinkedDeque<TYPE>::PushFront(const TYPE Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedDeque<TYPE>::PushFront(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PopFront(TYPE &Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedDeque<TYPE>::PopFront(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PushBack(const TYPE Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedDeque<TYPE>::PushBack(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PopBack(TYPE &Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedDeque<TYPE>::PopBack(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::IsEmpty()
{
	bool bRet;

	m_RWLock.SharedLock();
	{
		bRet = CBaseLinkedDeque<TYPE>::IsEmpty();
	}
	m_RWLock.SharedUnLock();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::GetSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseLinkedDeque<TYPE>::GetSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedDeque<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedDeque<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#else
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedDeque<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedDeque<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

#endif // ndef __SAFELINKEDQUEUE_H__
