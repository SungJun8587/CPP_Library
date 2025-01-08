
//***************************************************************************
// SafeDoublyLinkedList.h: interface for the CSafeCircularDoublyLinkedList class.
//
//***************************************************************************

#ifndef __SAFEDOUBLYLINKEDLIST_H__
#define __SAFEDOUBLYLINKEDLIST_H__

#ifndef __BASECIRCULARDOUBLYLINKEDLIST_H__
#include <BaseCircularDoublyLinkedList.h>
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
	std::shared_mutex	_mutex;
};

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Add(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::Add(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddIndex(Element, dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddFirst(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddLast(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddBeforeCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::AddAfterCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::UpdateIndex(dwIndex, Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::UpdateCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::Subtract(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractAllDup(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractIndex(dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractFirst()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractFirst();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractLast()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractLast();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::SubtractCurrent()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::SubtractCurrent();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveFirst()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveFirst();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveLast()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveLast();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MovePrev()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::MovePrev();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::MoveNext()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveNext();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeCircularDoublyLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::MoveIndex(dwIndex);

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeCircularDoublyLinkedList<TYPE>::GetCurrent()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseCircularDoublyLinkedList<TYPE>::GetCurrent();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeCircularDoublyLinkedList<TYPE>::At(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);

	TYPE Temp = CBaseCircularDoublyLinkedList<TYPE>::At(dwIndex);

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::GetCount()
{
	std::shared_lock lockGuard(_mutex);
	
	DWORD dwCount = CBaseCircularDoublyLinkedList<TYPE>::GetCount();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Reset()
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::Reset();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//	
template<class TYPE> void CSafeCircularDoublyLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseCircularDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}

#else
//***************************************************************************
//
template<class TYPE> void CSafeCircularDoublyLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseCircularDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeCircularDoublyLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	std::shared_lock lockGuard(_mutex);
	
	int nReturn = CBaseCircularDoublyLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);

	return nReturn;
}

#endif // ndef __SAFEDOUBLYLINKEDLIST_H__