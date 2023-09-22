
//***************************************************************************
// BaseLinkedStack.h: interface for the CBaseLinkedStack class.
//
//***************************************************************************

#ifndef __BASELINKEDSTACK_H__
#define __BASELINKEDSTACK_H__

#ifdef UNICODE
#define tcout std::wcout // unicode enabled
#else
#define tcout std::cout
#endif

//***************************************************************************
//
template<class TYPE>
class CBaseLinkedStack : private CBaseDoublyLinkedList<TYPE>
{
public:
	int Push(const TYPE Element);
	int Pop(TYPE& Element);
	TYPE Peek();
	BOOL IsEmpty();
	int GetSize();

#ifdef _UNICODE
	void Printing(wostream& StreamBuffer);
#else
	void Printing(ostream& StreamBuffer);
#endif
};

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedStack<TYPE>::Push(const TYPE Element)
{
	return CBaseDoublyLinkedList<TYPE>::Add(Element);
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedStack<TYPE>::Pop(TYPE& Element)
{
	Element = *CBaseDoublyLinkedList<TYPE>::MoveLast();

	return CBaseDoublyLinkedList<TYPE>::SubtractLast();
}

//***************************************************************************
//
template<class TYPE>
TYPE CBaseLinkedStack<TYPE>::Peek()
{
	return *CBaseDoublyLinkedList<TYPE>::MoveLast();
}

//***************************************************************************
//
template<class TYPE>
BOOL CBaseLinkedStack<TYPE>::IsEmpty()
{
	return CBaseDoublyLinkedList<TYPE>::GetCount() < 1 ? true : false;
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedStack<TYPE>::GetSize()
{
	return CBaseDoublyLinkedList<TYPE>::GetCount();
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedStack<TYPE>::Printing(wostream& StreamBuffer)
{
	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedStack<TYPE>::Printing(ostream& StreamBuffer)
{
	CBaseDoublyLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __BASELINKEDSTACK_H__

