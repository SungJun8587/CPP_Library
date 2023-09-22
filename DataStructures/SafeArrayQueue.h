
//***************************************************************************
// SafeArrayQueue.h: interface for the CSafeArrayQueue class.
//
//***************************************************************************

#ifndef __SAFEARRAYQUEUE_H__
#define __SAFEARRAYQUEUE_H__

#include <BaseArrayQueue.h>
#include <RWLock.h>

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
		if( nGrowth > 0 ) m_nGrowth = nGrowth;
		else m_nGrowth = 0;

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
	CRWLock		m_RWLock;
};

//***************************************************************************
//
template<class TYPE>
void CSafeArrayQueue<TYPE>::RemoveAll()
{
	m_RWLock.ExclusiveLock();
	{
		CBaseArrayQueue<TYPE>::RemoveAll();
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayQueue<TYPE>::SetSize(int nNewSize)
{
	m_RWLock.ExclusiveLock();
	{
		CBaseArrayQueue<TYPE>::SetSize(nNewSize);
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayQueue<TYPE>::GetSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetCapa()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayQueue<TYPE>::GetCapa();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::GetDataSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayQueue<TYPE>::GetDataSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::Put(TYPE Object)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseArrayQueue<TYPE>::Put(Object);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayQueue<TYPE>::Get(TYPE& Object)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseArrayQueue<TYPE>::Get(Object);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

#endif // ndef __SAFEARRAYQUEUE_H__
