
//***************************************************************************
// SafeCircularLinkedList.h: interface for the CSafeCircularLinkedList class.
//
//***************************************************************************

#ifndef __SAFECIRCULARLINKEDLIST_H__
#define __SAFECIRCULARLINKEDLIST_H__

#ifndef __BASECIRCULARLINKEDLIST_H__
#include <BaseCircularLinkedList.h>
#endif

#ifndef __RWLOCK_H__
#include <RwLock.h>
#endif

//***************************************************************************
//
template<class TYPE> class CSafeCircularLinkedList : public CBaseCircularLinkedList<TYPE>
{
public:
	CSafeCircularLinkedList() { };
	~CSafeCircularLinkedList() { };

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
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Add(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::Add(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::AddIndex(Element, dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::AddFirst(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::AddLast(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::AddBeforeCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::AddAfterCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::UpdateIndex(dwIndex, Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::UpdateCurrent(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}


//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::Subtract(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::SubtractAllDup(Element);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::SubtractIndex(dwIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractFirst()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::SubtractFirst();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractLast()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::SubtractLast();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractCurrent()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::SubtractCurrent();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveFirst()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::MoveFirst();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveLast()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::MoveLast();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MovePrev()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::MovePrev();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveNext()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::MoveNext();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeCircularLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::MoveIndex(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::GetCurrent()
{
	TYPE *pTemp;

	m_RWLock.SharedLock();
	{
		pTemp = CBaseCircularLinkedList<TYPE>::GetCurrent();
	}
	m_RWLock.SharedUnLock();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeCircularLinkedList<TYPE>::At(const DWORD dwIndex)
{
	TYPE Temp;

	m_RWLock.SharedLock();
	{
		Temp = CBaseCircularLinkedList<TYPE>::At(dwIndex);
	}
	m_RWLock.SharedUnLock();

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::GetCount()
{
	DWORD dwCount;

	m_RWLock.SharedLock();
	{
		dwCount = CBaseCircularLinkedList<TYPE>::GetCount();
	}
	m_RWLock.SharedUnLock();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Reset()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::Reset();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> void CSafeCircularLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseCircularLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#else
//***************************************************************************
//
template<class TYPE> void CSafeCircularLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	m_RWLock.SharedLock();
	{
		CBaseCircularLinkedList<TYPE>::Printing(StreamBuffer);
	}
	m_RWLock.SharedUnLock();
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	int nReturn;

	m_RWLock.SharedLock();
	{
		nReturn = CBaseCircularLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);
	}
	m_RWLock.SharedUnLock();

	return nReturn;
}

#endif // ndef __SAFECIRCULARLINKEDLIST_H__