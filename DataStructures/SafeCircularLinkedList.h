
//***************************************************************************
// SafeCircularLinkedList.h: interface for the CSafeCircularLinkedList class.
//
//***************************************************************************

#ifndef __SAFECIRCULARLINKEDLIST_H__
#define __SAFECIRCULARLINKEDLIST_H__

#ifndef __BASECIRCULARLINKEDLIST_H__
#include <BaseCircularLinkedList.h>
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
	std::shared_mutex	_mutex;
};

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Add(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseCircularLinkedList<TYPE>::Add(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseCircularLinkedList<TYPE>::AddIndex(Element, dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::AddFirst(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::AddLast(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::AddBeforeCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::AddAfterCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::UpdateIndex(dwIndex, Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::UpdateCurrent(Element);

	return nReturn;
}


//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::Subtract(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::SubtractAllDup(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::SubtractIndex(dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractFirst()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::SubtractFirst();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractLast()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::SubtractLast();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::SubtractCurrent()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::SubtractCurrent();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveFirst()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::MoveFirst();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveLast()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::MoveLast();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MovePrev()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::MovePrev();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::MoveNext()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::MoveNext();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeCircularLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::MoveIndex(dwIndex);

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularLinkedList<TYPE>::GetCurrent()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularLinkedList<TYPE>::GetCurrent();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeCircularLinkedList<TYPE>::At(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE Temp = CBaseCircularLinkedList<TYPE>::At(dwIndex);

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::GetCount()
{
	std::shared_lock lockGuard(_mutex);
	
	DWORD dwCount = CBaseCircularLinkedList<TYPE>::GetCount();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Reset()
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseCircularLinkedList<TYPE>::Reset();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> void CSafeCircularLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseCircularLinkedList<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE> void CSafeCircularLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseCircularLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeCircularLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	std::shared_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);

	return nReturn;
}

#endif // ndef __SAFECIRCULARLINKEDLIST_H__