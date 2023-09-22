
//***************************************************************************
// SafeLinkedList.h: interface for the CSafeLinkedList class.
//
//***************************************************************************

#ifndef __SAFELINKEDLIST_H__
#define __SAFELINKEDLIST_H__

#ifndef __BASELINKEDLIST_H__
#include <BaseLinkedList.h>
#endif

#ifndef __RWLOCK_H__
#include <RwLock.h>
#endif

//***************************************************************************
//
template<class TYPE> class CSafeLinkedList : public CBaseLinkedList<TYPE>
{
public:
	CSafeLinkedList() { };
	~CSafeLinkedList() { };

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
template<class TYPE> int CSafeLinkedList<TYPE>::Add(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::Add(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::AddIndex(Element, dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::AddFirst(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::AddLast(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::AddBeforeCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::AddAfterCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::UpdateIndex(dwIndex, Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::UpdateCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}


//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::Subtract(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::SubtractAllDup(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::SubtractIndex(dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractFirst()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::SubtractFirst();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractLast()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::SubtractLast();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractCurrent()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::SubtractCurrent();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveFirst()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::MoveFirst();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveLast()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::MoveLast();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MovePrev()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::MovePrev();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveNext()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::MoveNext();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::GetCurrent(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::GetCurrent()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseLinkedList<TYPE>::GetCurrent();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeLinkedList<TYPE>::At(const DWORD dwIndex)
{
	TYPE Temp;

	m_RWLock.SharedLock();
	{
		Temp = CBaseLinkedList<TYPE>::At(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::GetCount()
{
	DWORD dwCount;

	m_RWLock.SharedLock();
	{
		dwCount = CBaseLinkedList<TYPE>::GetCount();
	}
	m_RWLock.SharedUnLock();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Reset()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseLinkedList<TYPE>::Reset();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> void CSafeLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#else
//***************************************************************************
//
template<class TYPE> void CSafeLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	int nReturn;

	m_RWLock.SharedLock();
	{
		nReturn = CBaseLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);
	}
	m_RWLock.SharedUnLock();

	return nReturn;
}

#endif // ndef __SAFELINKEDLIST_H__