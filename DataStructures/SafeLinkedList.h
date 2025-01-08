
//***************************************************************************
// SafeLinkedList.h: interface for the CSafeLinkedList class.
//
//***************************************************************************

#ifndef __SAFELINKEDLIST_H__
#define __SAFELINKEDLIST_H__

#ifndef __BASELINKEDLIST_H__
#include <BaseLinkedList.h>
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
	std::shared_mutex	_mutex;
};

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Add(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseLinkedList<TYPE>::Add(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddIndex(const TYPE &Element, const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::AddIndex(Element, dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddFirst(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::AddFirst(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddLast(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::AddLast(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddBeforeCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::AddBeforeCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::AddAfterCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::AddAfterCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::UpdateIndex(dwIndex, Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::UpdateCurrent(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::UpdateCurrent(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Subtract(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::Subtract(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractAllDup(const TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::SubtractAllDup(Element);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::SubtractIndex(dwIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractFirst()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::SubtractFirst();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractLast()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::SubtractLast();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::SubtractCurrent()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::SubtractCurrent();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveFirst()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseLinkedList<TYPE>::MoveFirst();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveLast()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseLinkedList<TYPE>::MoveLast();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MovePrev()
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTemp = CBaseLinkedList<TYPE>::MovePrev();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::MoveNext()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseLinkedList<TYPE>::MoveNext();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CSafeLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseLinkedList<TYPE>::GetCurrent(dwIndex);

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE *CSafeLinkedList<TYPE>::GetCurrent()
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE* pTemp = CBaseLinkedList<TYPE>::GetCurrent();

	return pTemp;
}

//***************************************************************************
//	
template<class TYPE> TYPE CSafeLinkedList<TYPE>::At(const DWORD dwIndex)
{
	std::shared_lock lockGuard(_mutex);
	
	TYPE Temp = CBaseLinkedList<TYPE>::At(dwIndex);

	return Temp;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::GetCount()
{
	std::shared_lock lockGuard(_mutex);
	
	DWORD dwCount = CBaseLinkedList<TYPE>::GetCount();

	return dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Reset()
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CBaseLinkedList<TYPE>::Reset();

	return nReturn;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> void CSafeLinkedList<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseLinkedList<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE> void CSafeLinkedList<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

//***************************************************************************
//	
template<class TYPE> int CSafeLinkedList<TYPE>::Compare(const BYTE *pBCompee, const BYTE *pBComper, const DWORD dwSize)
{
	std::shared_lock lockGuard(_mutex);
	
	int nReturn = CBaseLinkedList<TYPE>::Compare(pBCompee, pBComper, dwSize);

	return nReturn;
}

#endif // ndef __SAFELINKEDLIST_H__