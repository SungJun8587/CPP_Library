
//***************************************************************************
// SafeArrayStack.h: interface for the CSafeArrayStack class.
//
//***************************************************************************

#ifndef __SAFEARRAYSTACK_H__
#define __SAFEARRAYSTACK_H__

#include <BaseArrayStack.h>
#include <RWLock.h>

//***************************************************************************
//
template<class TYPE>
class CSafeArrayStack : public CBaseArrayStack<TYPE>
{
public:

	CSafeArrayStack() {}
	CSafeArrayStack(int nSize)
	{
		if( nSize > 0 ) SetSize(nSize);
	}
	CSafeArrayStack(int nSize, int nGrowth)
	{
		if( nGrowth > 0 ) m_nGrowth = nGrowth;
		else m_nGrowth = 0;

		if( nSize > 0 ) SetSize(nSize);
	}
	~CSafeArrayStack() {}

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
void CSafeArrayStack<TYPE>::RemoveAll()
{
	m_RWLock.ExclusiveLock();
	{
		CBaseArrayStack<TYPE>::RemoveAll();
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayStack<TYPE>::SetSize(int nNewSize)
{
	m_RWLock.ExclusiveLock();
	{
		CBaseArrayStack<TYPE>::SetSize(nNewSize);
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayStack<TYPE>::GetSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayStack<TYPE>::GetSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayStack<TYPE>::GetCapa()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayStack<TYPE>::GetCapa();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayStack<TYPE>::GetDataSize()
{
	int nRet;

	m_RWLock.SharedLock();
	{
		nRet = CBaseArrayStack<TYPE>::GetDataSize();
	}
	m_RWLock.SharedUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayStack<TYPE>::Put(TYPE Object)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseArrayStack<TYPE>::Put(Object);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayStack<TYPE>::Get(TYPE& Object)
{
	int nRet;

	m_RWLock.ExclusiveLock();
	{
		nRet = CBaseArrayStack<TYPE>::Get(Object);
	}
	m_RWLock.ExclusiveUnLock();

	return nRet;
}

#endif // ndef __SAFEARRAYSTACK_H__
