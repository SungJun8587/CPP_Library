
//***************************************************************************
// SafeArrayList.h: interface for the CSafeArrayList class.
//
//***************************************************************************

#ifndef __SAFEARRAYLIST_H__
#define __SAFEARRAYLIST_H__

#ifndef __SORTEDARRAYLIST_H__
#include "SortedArrayList.h"
#endif

#ifndef __RWLOCK_H__
#include "RWLock.h"
#endif

template<class TYPE>
class CSafeArrayList : public CSortedArrayList<TYPE>
{
public:
	CSafeArrayList(const CSafeArrayList& Type);
	CSafeArrayList() {}
	~CSafeArrayList() {}

	void			SetListSize(int nNewSize = 5000);
	int				GetListSize() const;
	int				GetMaxSize() const;

	void			RemoveAll(void);
	int				sInsert(const TYPE& type);			// Static Insert
	int				dInsert(const TYPE& type);			// Dynamic Insert
	int				Set(TYPE data, int i);
	TYPE			Get(int i);

	TYPE*			GetPtr(int i);
	TYPE*			GetListPtr();

	int				Delete(TYPE type);
	int				DeleteAllDup(TYPE type);
	int				DeleteIndex(int nIndex);
	int				Distinguish();

	TYPE&			operator[] (int i) const;
	TYPE*			operator[] (int i);

	void			DataSort(int nType, int nMethod);

public:
	CSafeArrayList &operator = (const CSafeArrayList &x);

public:
	BOOL operator <  (const CSafeArrayList &x);
	BOOL operator <= (const CSafeArrayList &x);
	BOOL operator == (const CSafeArrayList &x);
	BOOL operator != (const CSafeArrayList &x);
	BOOL operator >  (const CSafeArrayList &x);
	BOOL operator >= (const CSafeArrayList &x);

private:
	CRWLock		m_RWLock;
};

//***************************************************************************
//
template<class TYPE>
CSafeArrayList<TYPE>::CSafeArrayList(const CSafeArrayList& Type)
{
	m_RWLock.ExclusiveLock();
	{
		CSortedArrayList<TYPE>::CSortedArrayList(Type);
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator < (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator < (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator <= (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator <= (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator == (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator == (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator != (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator != (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator > (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator > (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator >= (const CSafeArrayList &x)
{
	BOOL bReturn;

	m_RWLock.SharedLock();
	{
		bReturn = CSortedArrayList<TYPE>::operator >= (x);
	}
	m_RWLock.SharedUnLock();

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline CSafeArrayList< TYPE >& CSafeArrayList<TYPE>::operator = (const CSafeArrayList &x)
{
	m_RWLock.ExclusiveLock();
	{
		CSortedArrayList<TYPE>::Copy(x);
	}
	m_RWLock.ExclusiveUnLock();

	return *this;
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayList<TYPE>::SetListSize(int nNewSize)
{
	m_RWLock.ExclusiveLock();
	{
		CSortedArrayList<TYPE>::SetListSize(nNewSize);
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayList<TYPE>::RemoveAll(void)
{
	m_RWLock.ExclusiveLock();
	{
		CSortedArrayList<TYPE>::RemoveAll();
	}
	m_RWLock.ExclusiveUnLock();
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::sInsert(const TYPE& Type)
{
	int nResult;

	m_RWLock.ExclusiveLock();
	{
		nResult = CSortedArrayList<TYPE>::sInsert(Type);
	}
	m_RWLock.ExclusiveUnLock();

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::dInsert(const TYPE& Type)
{
	int nResult;

	m_RWLock.ExclusiveLock();
	{
		nResult = CSortedArrayList<TYPE>::dInsert(Type);
	}
	m_RWLock.ExclusiveUnLock();

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::Set(TYPE Type, int i)
{
	int nResult;

	m_RWLock.ExclusiveLock();
	{
		nResult = CSortedArrayList<TYPE>::Set(Type, i);
	}
	m_RWLock.ExclusiveUnLock();

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
TYPE CSafeArrayList<TYPE>::Get(int i)
{
	TYPE TypeData;

	m_RWLock.SharedLock();
	{
		TypeData = CSortedArrayList<TYPE>::Get(i);
	}
	m_RWLock.SharedUnLock();

	return TypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::GetPtr(int i)
{
	TYPE *pTypeData;

	m_RWLock.SharedLock();
	{
		pTypeData = CSortedArrayList<TYPE>::GetPtr(i);
	}
	m_RWLock.SharedUnLock();

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::GetListPtr()
{
	TYPE **pTypeData;

	m_RWLock.SharedLock();
	{
		pTypeData = CSortedArrayList<TYPE>::GetListPtr();
	}
	m_RWLock.SharedUnLock();

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE& CSafeArrayList<TYPE>::operator[] (int i) const
{
	TYPE TypeData;

	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);

	m_RWLock.SharedLock();
	{
		TypeData = CSortedArrayList<TYPE>::operator[] (i);
	}
	m_RWLock.SharedUnLock();

	return TypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::operator[] (int i)
{
	TYPE *pTypeData;

	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);

	m_RWLock.SharedLock();
	{
		pTypeData = CSortedArrayList<TYPE>::operator[] (i);
	}
	m_RWLock.SharedUnLock();

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::GetMaxSize() const
{
	int nMaxSize;

	m_RWLock.SharedLock();
	{
		nMaxSize = CSortedArrayList<TYPE>::GetMaxSize();
	}
	m_RWLock.SharedUnLock();

	return nMaxSize;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::GetListSize() const
{
	int nListSize;

	m_RWLock.SharedLock();
	{
		nListSize = CSortedArrayList<TYPE>::GetListSize();
	}
	m_RWLock.SharedUnLock();

	return nListSize;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::Delete(TYPE type)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CSortedArrayList<TYPE>::Delete(type);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::DeleteAllDup(TYPE type)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CSortedArrayList<TYPE>::DeleteAllDup(type);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::DeleteIndex(int nIndex)
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CSortedArrayList<TYPE>::DeleteIndex(nIndex);
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::Distinguish()
{
	int nReturn;

	m_RWLock.ExclusiveLock();
	{
		nReturn = CSortedArrayList<TYPE>::Distinguish();
	}
	m_RWLock.ExclusiveUnLock();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
void CSafeArrayList<TYPE>::DataSort(int nType, int nMethod)
{
	m_RWLock.ExclusiveLock();
	{
		CSortedArrayList<TYPE>::DataSort(nType, nMethod);
	}
	m_RWLock.ExclusiveUnLock();
}

#endif // ndef __SAFEARRAYLIST_H__
