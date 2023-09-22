
//***************************************************************************
// BaseLinkedDeque.h: interface for the CBaseLinkedDeque class.
//
//***************************************************************************

#ifndef __BASELINKEDDEQUE_H__
#define __BASELINKEDDEQUE_H__

#ifdef UNICODE
#define tcout std::wcout // unicode enabled
#else
#define tcout std::cout
#endif

//***************************************************************************
//
template<class TYPE>
class CBaseLinkedDeque : private CBaseDoublyLinkedList<TYPE>
{
public:
	int PushFront(const TYPE Element);
	int PopFront(TYPE& Element);

	int PushBack(const TYPE Element);
	int PopBack(TYPE& Element);

	BOOL IsEmpty();
	int GetSize();

#ifdef _UNICODE
	void Printing(wostream &StreamBuffer);
#else
	void Printing(ostream& StreamBuffer);
#endif
};

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedDeque<TYPE>::PushFront(const TYPE Element)
{
	return CBaseDoublyLinkedList<TYPE>::AddFirst(Element);
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedDeque<TYPE>::PopFront(TYPE& Element)
{
	Element = *CBaseDoublyLinkedList<TYPE>::MoveFirst();

	return CBaseDoublyLinkedList<TYPE>::SubtractFirst();
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedDeque<TYPE>::PushBack(const TYPE Element)
{
	return CBaseDoublyLinkedList<TYPE>::AddLast(Element);
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedDeque<TYPE>::PopBack(TYPE& Element)
{
	Element = *CBaseDoublyLinkedList<TYPE>::MoveLast();

	return CBaseDoublyLinkedList<TYPE>::SubtractLast();
}

//***************************************************************************
//
template<class TYPE>
BOOL CBaseLinkedDeque<TYPE>::IsEmpty()
{
	return CBaseDoublyLinkedList<TYPE>::GetCount() < 1 ? true : false;
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedDeque<TYPE>::GetSize()
{
	return CBaseDoublyLinkedList<TYPE>::GetCount();
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedDeque<TYPE>::Printing(wostream &StreamBuffer)
{
	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedDeque<TYPE>::Printing(ostream &StreamBuffer)
{
	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __BASELINKEDDEQUE_H__

