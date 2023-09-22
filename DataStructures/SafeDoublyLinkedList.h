
//***************************************************************************
// SafeDoublyLinkedList.h: interface for the CSafeDoublyLinkedList class.
//
//***************************************************************************

#ifndef __SAFEDOUBLYLINKEDLIST_H__
#define __SAFEDOUBLYLINKEDLIST_H__

#ifndef __BASEDOUBLYLINKEDLIST_H__
#include <BaseDoublyLinkedList.h>
#endif

#ifndef __RWLOCK_H__
#include <RwLock.h>
#endif

//***************************************************************************
//
template<class TYPE> class CSafeDoublyLinkedList : public CBaseDoublyLinkedList<TYPE>
{
public:
	CSafeDoublyLinkedList() { };
	~CSafeDoublyLinkedList() { };

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
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Add(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::Add(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::AddIndex(Element, dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::AddFirst(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::AddLast(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::AddBeforeCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::AddAfterCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::UpdateIndex(dwIndex, Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::UpdateCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::Subtract(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::SubtractAllDup(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::SubtractIndex(dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractFirst()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::SubtractFirst();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractLast()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::SubtractLast();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractCurrent()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::SubtractCurrent();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveFirst()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::MoveFirst();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveLast()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::MoveLast();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MovePrev()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::MovePrev();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveNext()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::MoveNext();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeDoublyLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::MoveIndex(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::GetCurrent()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseDoublyLinkedList<TYPE>::GetCurrent();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeDoublyLinkedList<TYPE>::At(const DWORD dwIndex)
{
	TYPE Temp;

	m_RWLock.SharedLock();
	{
		Temp = CBaseDoublyLinkedList<TYPE>::At(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::GetCount()
{
	DWORD dwCount;

	m_RWLock.SharedLock();
	{
		dwCount = CBaseDoublyLinkedList<TYPE>::GetCount();
	}
	m_RWLock.SharedUnLock();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Reset()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::Reset();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//	
template<class TYPE> void CSafeDoublyLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}

#else
//***************************************************************************
//
template<class TYPE> void CSafeDoublyLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	int nReturn;

	m_RWLock.SharedLock();
	{
		nReturn = CBaseDoublyLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);
	}
	m_RWLock.SharedUnLock();

	return nReturn;
}

#endif // ndef __SAFEDOUBLYLINKEDLIST_H__