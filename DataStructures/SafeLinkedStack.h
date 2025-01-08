
//***************************************************************************
// SafeLinkedStack.h: interface for the CSafeLinkedStack class.
//
//***************************************************************************

#ifndef __SAFELINKEDSTACK_H__
#define __SAFELINKEDSTACK_H__

#include <BaseLinkedStack.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedStack : public CBaseLinkedStack<TYPE>
{
public:
	int Push(const TYPE Element);
	int Pop(TYPE &Element);
	TYPE Peek();
	bool IsEmpty();
	int GetSize();

#ifdef _UNICODE
	void Printing(wostream &StreamBuffer);
#else
	void Printing(ostream &StreamBuffer);
#endif

private:
	std::shared_mutex	_mutex;
};

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::Push(const TYPE Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedStack<TYPE>::Push(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::Pop(TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedStack<TYPE>::Pop(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
TYPE CSafeLinkedStack<TYPE>::Peek()
{
	std::shared_lock lockGuard(_mutex);

	TYPE Element = CBaseLinkedStack<TYPE>::Peek();

	return Element;
}

//***************************************************************************
//
template<class TYPE>
bool CSafeLinkedStack<TYPE>::IsEmpty()
{
	std::shared_lock lockGuard(_mutex);

	bool bRet = CBaseLinkedStack<TYPE>::IsEmpty();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedStack<TYPE>::GetSize()
{
	std::shared_lock lockGuard(_mutex);
	
	int nRet = CBaseLinkedStack<TYPE>::GetSize();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> 
void CSafeLinkedStack<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseLinkedStack<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE> 
void CSafeLinkedStack<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseLinkedStack<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __SAFELINKEDSTACK_H__
