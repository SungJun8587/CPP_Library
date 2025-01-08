
//***************************************************************************
// SafeLinkedQueue.h: interface for the CSafeLinkedDeque class.
//
//***************************************************************************

#ifndef __SAFELINKEDQUEUE_H__
#define __SAFELINKEDQUEUE_H__

#include <BaseLinkedDeque.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedDeque : public CBaseLinkedDeque<TYPE>
{
public:
	int PushFront(const TYPE Element);
	int PopFront(TYPE &Element);

	int PushBack(const TYPE Element);
	int PopBack(TYPE &Element);

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
int CSafeLinkedDeque<TYPE>::PushFront(const TYPE Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nRet = CBaseLinkedDeque<TYPE>::PushFront(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PopFront(TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedDeque<TYPE>::PopFront(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PushBack(const TYPE Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedDeque<TYPE>::PushBack(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::PopBack(TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedDeque<TYPE>::PopBack(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
bool CSafeLinkedDeque<TYPE>::IsEmpty()
{
	std::shared_lock lockGuard(_mutex);
		
	bool bRet = CBaseLinkedDeque<TYPE>::IsEmpty();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedDeque<TYPE>::GetSize()
{
	std::shared_lock lockGuard(_mutex);

	int nRet = CBaseLinkedDeque<TYPE>::GetSize();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedDeque<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseLinkedDeque<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedDeque<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);

	CBaseLinkedDeque<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __SAFELINKEDQUEUE_H__
