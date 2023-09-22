
//***************************************************************************
// SafeLinkedStack.h: interface for the CSafeLinkedStack class.
//
//***************************************************************************

#ifndef __SAFELINKEDSTACK_H__
#define __SAFELINKEDSTACK_H__

#include <BaseLinkedStack.h>
#include <RWLock.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedStack : public CBaseLinkedStack<TYPE>
{
public:
	int Push(const TYPE Element);
	int Pop(TYPE &Element);
	TYPE Peek();
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
int CSafeLinkedStack<TYPE>::Push(const TYPE Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedStack<TYPE>::Push(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::Pop(TYPE &Element)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseLinkedStack<TYPE>::Pop(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
TYPE CSafeLinkedStack<TYPE>::Peek()
{
	TYPE Element;

	m_RWLock.SharedLock();
	{
		Element = CBaseLinkedStack<TYPE>::Peek();
	}
	m_RWLock.SharedUnLock();

	return Element;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::IsEmpty()
{
	BOOL bRet;

	m_RWLock.SharedLock();
	{
		bRet = CBaseLinkedStack<TYPE>::IsEmpty();
	}
	m_RWLock.SharedUnLock();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::GetSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseLinkedStack<TYPE>::GetSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> 
void CSafeLinkedStack<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedStack<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#else
//***************************************************************************
//
template<class TYPE> 
void CSafeLinkedStack<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedStack<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

#endif // ndef __SAFELINKEDSTACK_H__
