
//***************************************************************************
// BaseLinkedQueue.h: interface for the CBaseLinkedQueue class.
//
//***************************************************************************

#ifndef __BASELINKEDQUEUE_H__
#define __BASELINKEDQUEUE_H__

#ifdef UNICODE
#define tcout std::wcout // unicode enabled
#else
#define tcout std::cout
#endif

//***************************************************************************
//
template<class TYPE>
class CBaseLinkedQueue : private CBaseLinkedList<TYPE>
{
public:
	int EnQueue(const TYPE Element);
	int DeQueue(TYPE& Element);
	bool IsEmpty();
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
int CBaseLinkedQueue<TYPE>::EnQueue(const TYPE Element)
{
	return CBaseLinkedList<TYPE>::Add(Element);
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedQueue<TYPE>::DeQueue(TYPE& Element)
{
	Element = *CBaseLinkedList<TYPE>::MoveFirst();

	return CBaseLinkedList<TYPE>::SubtractFirst();
}

//***************************************************************************
//
template<class TYPE>
bool CBaseLinkedQueue<TYPE>::IsEmpty()
{
	return CBaseLinkedList<TYPE>::GetCount() < 1 ? true : false;
}

//***************************************************************************
//
template<class TYPE>
int CBaseLinkedQueue<TYPE>::GetSize()
{
	return CBaseLinkedList<TYPE>::GetCount();
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedQueue<TYPE>::Printing(wostream& StreamBuffer)
{
	CBaseLinkedList<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE>
void CBaseLinkedQueue<TYPE>::Printing(ostream& StreamBuffer)
{
	CBaseLinkedList<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __BASELINKEDQUEUE_H__

