
//***************************************************************************
// SafeArrayQueue.h: interface for the CSafeArrayQueue class.
//
//***************************************************************************

#ifndef __SAFEARRAYQUEUE_H__
#define __SAFEARRAYQUEUE_H__

#include <BaseArrayQueue.h>

//***************************************************************************
//
template<class TYPE>
class CSafeArrayQueue : public CBaseArrayQueue<TYPE>
{
public:

	CSafeArrayQueue() {}
	CSafeArrayQueue(int nSize)
	{
		if( nSize > 0 ) SetSize(nSize);
	}
	CSafeArrayQueue(int nSize, int nGrowth)
	{
		if( nGrowth > 0 ) this->m_nGrowth = nGrowth;
		else this->m_nGrowth = 0;

		if( nSize > 0 ) SetSize(nSize);
	}
	~CSafeArrayQueue() {}

	void	RemoveAll(void);
	void	SetSize(int nNewSize);
	int		GetSize(void);
	int		GetCapa(void);
	int		GetDataSize(void);
	int		Get(TYPE& Element);
	int		Put(TYPE Element);

private:
	std::shared_mutex	_mutex;
};

//***************************************************************************
//
template<class TYPE>
void CSafeArrayQueue<TYPE>::RemoveAll()
{
	std::unique_lock lockGuard(_mutex);

	CBaseArrayQueue<TYPE>::RemoveAll();
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayQueue<TYPE>::SetSize(int nNewSize)
{
	std::unique_lock lockGuard(_mutex);

	CBaseArrayQueue<TYPE>::SetSize(nNewSize);
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetSize()
{
	std::shared_lock lockGuard(_mutex);

	int nRet = CBaseArrayQueue<TYPE>::GetSize();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetCapa()
{
	std::shared_lock lockGuard(_mutex);

	int nRet = CBaseArrayQueue<TYPE>::GetCapa();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetDataSize()
{
	std::shared_lock lockGuard(_mutex);

	int nRet = CBaseArrayQueue<TYPE>::GetDataSize();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::Put(TYPE Object)
{
	std::unique_lock lockGuard(_mutex);

	int nRet = CBaseArrayQueue<TYPE>::Put(Object);

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::Get(TYPE& Object)
{
	std::unique_lock lockGuard(_mutex);
	
	int nRet = CBaseArrayQueue<TYPE>::Get(Object);

	return nRet;
}

#endif // ndef __SAFEARRAYQUEUE_H__
