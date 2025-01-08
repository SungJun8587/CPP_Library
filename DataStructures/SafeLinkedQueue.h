
//***************************************************************************
// SafeLinkedQueue.h: interface for the CSafeLinkedQueue class.
//
//***************************************************************************

#ifndef __SAFELINKEDQUEUE_H__
#define __SAFELINKEDQUEUE_H__

#include <BaseLinkedQueue.h>

//***************************************************************************
//
template<class TYPE>
class CSafeLinkedQueue : public CBaseLinkedQueue<TYPE>
{
public:
	int EnQueue(const TYPE Element);
	int DeQueue(TYPE &Element);
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
int CSafeLinkedQueue<TYPE>::EnQueue(const TYPE Element)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseLinkedQueue<TYPE>::EnQueue(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::DeQueue(TYPE &Element)
{
	std::unique_lock lockGuard(_mutex);
	
	int nRet = CBaseLinkedQueue<TYPE>::DeQueue(Element);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
bool CSafeLinkedQueue<TYPE>::IsEmpty()
{
	std::shared_lock lockGuard(_mutex);

	bool bRet = CBaseLinkedQueue<TYPE>::IsEmpty();

	return bRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeLinkedQueue<TYPE>::GetSize()
{
	std::shared_lock lockGuard(_mutex);
	
	int nRet = CBaseLinkedQueue<TYPE>::GetSize();

	return nRet;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedQueue<TYPE>::Printing(wostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseLinkedQueue<TYPE>::Printing(StreamBuffer);
}
#else
//***************************************************************************
//
template<class TYPE>
void CSafeLinkedQueue<TYPE>::Printing(ostream &StreamBuffer)
{
	std::shared_lock lockGuard(_mutex);
	
	CBaseLinkedQueue<TYPE>::Printing(StreamBuffer);
}
#endif

#endif // ndef __SAFELINKEDQUEUE_H__
