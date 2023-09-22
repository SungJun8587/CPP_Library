
//***************************************************************************
// SafeDoublyLinkedList.h: interface for the CSafeCircularDoublyLinkedList class.
//
//***************************************************************************

#ifndef __SAFEDOUBLYLINKEDLIST_H__
#define __SAFEDOUBLYLINKEDLIST_H__

#ifndef __BASECIRCULARDOUBLYLINKEDLIST_H__
#include <BaseCircularDoublyLinkedList.h>
#endif

#ifndef __RWLOCK_H__
#include <RwLock.h>
#endif

//***************************************************************************
//
template<class TYPE> class CSafeCircularDoublyLinkedList : public CBaseCircularDoublyLinkedList<TYPE>
{
public:
	CSafeCircularDoublyLinkedList() { };
	~CSafeCircularDoublyLinkedList() { };

	int Add(const TYPE &Element);
	int	AddIndex(const TYPE &Element, const DWORD dwIndex);
	int AddFirst(const TYPE &Element);
	int AddLast(const TYPE &Element);
	int AddBeforeCurrent(const TYPE &Element);
	int AddAfterCurrent(const TYPE &Element);
	int	UpdateIndex(const DWORD dwIndex, const TYPE &Element);
	int UpdateCurrent(const TYPE &Element);
	int Subtract(const TYPE &Element);
	int SubtractAllDup(const TYPE &Element);
	int	SubtractIndex(const DWORD dwIndex);
	int SubtractFirst();
	int SubtractLast();
	int SubtractCurrent();
	TYPE* MoveFirst();
	TYPE* MoveLast();
	TYPE* MovePrev();
	TYPE* MoveNext();
	TYPE* MoveIndex(const DWORD dwIndex);
	TYPE* GetCurrent();
	TYPE  At(const DWORD dwIndex);
	int GetCount();
	int Reset();

#ifdef _UNICODE
	void Printing(wostream &StreamBuffer);
#else
	void Printing(ostream &StreamBuffer);
#endif

private:
	int Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize);

private:
	CRWLock		m_RWLock;
};

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Add(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::Add(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddIndex(Element, dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddFirst(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddLast(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddBeforeCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddAfterCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::UpdateIndex(dwIndex, Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::UpdateCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::Subtract(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractAllDup(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractIndex(dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractFirst()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractFirst();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractLast()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractLast();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractCurrent()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractCurrent();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveFirst()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveFirst();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveLast()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveLast();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MovePrev()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::MovePrev();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveNext()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveNext();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeCircularDoublyLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveIndex(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::GetCurrent()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularDoublyLinkedList<TYPE>::GetCurrent();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeCircularDoublyLinkedList<TYPE>::At(const DWORD dwIndex)
{
	TYPE Temp;

	m_RWLock.SharedLock();
	{
		Temp = CBaseCircularDoublyLinkedList<TYPE>::At(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::GetCount()
{
	DWORD dwCount;

	m_RWLock.SharedLock();
	{
		dwCount = CBaseCircularDoublyLinkedList<TYPE>::GetCount();
	}
	m_RWLock.SharedUnLock();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Reset()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::Reset();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//	
template<class TYPE> void CSafeCircularDoublyLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseCircularDoublyLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}

#else
//***************************************************************************
//
template<class TYPE> void CSafeCircularDoublyLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseCircularDoublyLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	int nReturn;

	m_RWLock.SharedLock();
	{
		nReturn = CBaseCircularDoublyLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);
	}
	m_RWLock.SharedUnLock();

	return nReturn;
}

#endif // ndef __SAFEDOUBLYLINKEDLIST_H__