
//***************************************************************************
// SafeArrayList.h: interface for the CSafeArrayList class.
//
//***************************************************************************

#ifndef __SAFEARRAYLIST_H__
#define __SAFEARRAYLIST_H__

#ifndef __SORTEDARRAYLIST_H__
#include "SortedArrayList.h"
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
	std::shared_mutex	_mutex;
};

//***************************************************************************
//
template<class TYPE>
CSafeArrayList<TYPE>::CSafeArrayList(const CSafeArrayList& Type)
{
	std::unique_lock lockGuard(_mutex);

	CSortedArrayList<TYPE>::CSortedArrayList(Type);
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator < (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator < (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator <= (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator <= (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator == (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator == (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator != (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator != (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator > (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator > (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CSafeArrayList<TYPE>::operator >= (const CSafeArrayList &x)
{
	std::shared_lock lockGuard(_mutex);

	BOOL bReturn = CSortedArrayList<TYPE>::operator >= (x);

	return bReturn;
}

//***************************************************************************
//
template<class TYPE>
inline CSafeArrayList< TYPE >& CSafeArrayList<TYPE>::operator = (const CSafeArrayList &x)
{
	std::unique_lock lockGuard(_mutex);

	CSortedArrayList<TYPE>::Copy(x);

	return *this;
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayList<TYPE>::SetListSize(int nNewSize)
{
	std::unique_lock lockGuard(_mutex);

	CSortedArrayList<TYPE>::SetListSize(nNewSize);
}

//***************************************************************************
//
template<class TYPE>
void CSafeArrayList<TYPE>::RemoveAll(void)
{
	std::unique_lock lockGuard(_mutex);

	CSortedArrayList<TYPE>::RemoveAll();
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::sInsert(const TYPE& Type)
{
	std::unique_lock lockGuard(_mutex);

	int nResult = CSortedArrayList<TYPE>::sInsert(Type);

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::dInsert(const TYPE& Type)
{
	std::unique_lock lockGuard(_mutex);

	int nResult = CSortedArrayList<TYPE>::dInsert(Type);

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::Set(TYPE Type, int i)
{
	std::unique_lock lockGuard(_mutex);

	int nResult = CSortedArrayList<TYPE>::Set(Type, i);

	return nResult;
}

//***************************************************************************
//
template<class TYPE>
TYPE CSafeArrayList<TYPE>::Get(int i)
{
	std::shared_lock lockGuard(_mutex);

	TYPE TypeData = CSortedArrayList<TYPE>::Get(i);

	return TypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::GetPtr(int i)
{
	std::shared_lock lockGuard(_mutex);

	TYPE* pTypeData = CSortedArrayList<TYPE>::GetPtr(i);

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::GetListPtr()
{
	std::shared_lock lockGuard(_mutex);

	TYPE **pTypeData;

	TYPE** pTypeData = CSortedArrayList<TYPE>::GetListPtr();

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE& CSafeArrayList<TYPE>::operator[] (int i) const
{
	if( !(i >= 0 && i < this->m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);

	std::shared_lock lockGuard(_mutex);

	TYPE TypeData = CSortedArrayList<TYPE>::operator[] (i);

	return TypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE* CSafeArrayList<TYPE>::operator[] (int i)
{
	if( !(i >= 0 && i < this->m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);

	std::shared_lock lockGuard(_mutex);

	TYPE* pTypeData = CSortedArrayList<TYPE>::operator[] (i);

	return pTypeData;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::GetMaxSize() const
{
	std::shared_lock lockGuard(_mutex);

	int nMaxSize = CSortedArrayList<TYPE>::GetMaxSize();

	return nMaxSize;
}

//***************************************************************************
//
template<class TYPE>
int CSafeArrayList<TYPE>::GetListSize() const
{
	std::shared_lock lockGuard(_mutex);

	int nListSize = CSortedArrayList<TYPE>::GetListSize();

	return nListSize;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::Delete(TYPE type)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CSortedArrayList<TYPE>::Delete(type);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::DeleteAllDup(TYPE type)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CSortedArrayList<TYPE>::DeleteAllDup(type);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::DeleteIndex(int nIndex)
{
	std::unique_lock lockGuard(_mutex);

	int nReturn = CSortedArrayList<TYPE>::DeleteIndex(nIndex);

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
int CSafeArrayList<TYPE>::Distinguish()
{
	std::unique_lock lockGuard(_mutex);
	
	int nReturn = CSortedArrayList<TYPE>::Distinguish();

	return nReturn;
}

//***************************************************************************
//	
template<class TYPE>
void CSafeArrayList<TYPE>::DataSort(int nType, int nMethod)
{
	std::unique_lock lockGuard(_mutex);

	CSortedArrayList<TYPE>::DataSort(nType, nMethod);
}

#endif // ndef __SAFEARRAYLIST_H__
