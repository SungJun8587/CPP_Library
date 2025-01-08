
//***************************************************************************
// SafeDoublyLinkedList.h: interface for the CSafeDoublyLinkedList class.
//
//***************************************************************************

#ifndef __SAFEDOUBLYLINKEDLIST_H__
#define __SAFEDOUBLYLINKEDLIST_H__

#ifndef __BASEDOUBLYLINKEDLIST_H__
#include <BaseDoublyLinkedList.h>
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
	std::shared_mutex	_mutex;
};

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Add(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
		
	int nReturn = CBaseDoublyLinkedList<TYPE>::Add(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::AddIndex(Element, dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::AddFirst(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::AddLast(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::AddBeforeCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::AddAfterCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::UpdateIndex(dwIndex, Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::UpdateCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::Subtract(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::SubtractAllDup(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::SubtractIndex(dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractFirst()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::SubtractFirst();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractLast()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::SubtractLast();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::SubtractCurrent()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseDoublyLinkedList<TYPE>::SubtractCurrent();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveFirst()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::MoveFirst();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveLast()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::MoveLast();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MovePrev()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::MovePrev();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::MoveNext()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::MoveNext();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeDoublyLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::MoveIndex(dwIndex);

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeDoublyLinkedList<TYPE>::GetCurrent()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseDoublyLinkedList<TYPE>::GetCurrent();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeDoublyLinkedList<TYPE>::At(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE Temp = CBaseDoublyLinkedList<TYPE>::At(dwIndex);

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::GetCount()
{
	std::shared_lock lockGuard(_mutex);
	
	DWORD dwCount = CBaseDoublyLinkedList<TYPE>::GetCount();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Reset()
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseDoublyLinkedList<TYPE>::Reset();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//	
template<class TYPE> void CSafeDoublyLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}

#else
//***************************************************************************
//
template<class TYPE> void CSafeDoublyLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeDoublyLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	std::shared_lock lockGuard(_mutex);

	int nReturn = CBaseDoublyLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);

	return nReturn;
}

#endif // ndef __SAFEDOUBLYLINKEDLIST_H__