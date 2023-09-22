
//***************************************************************************
// BaseArrayList.h: interface for the CBaseArrayList class.
//
//***************************************************************************

#ifndef __BASEARRAYLIST_H__
#define __BASEARRAYLIST_H__

#define ARRAYLIST_RETCODE_SUCCESS						1
#define ARRAYLIST_RETCODE_FAIL							-1
#define ARRAYLIST_RETCODE_NO_ELEMENT					0
#define ARRAYLIST_RETCODE_HAVE_ELEMENT					1

//***************************************************************************
//
template<class TYPE>
void CopyElements(TYPE* pDest, const TYPE* pSrc, int nCount)
{
	if( nCount == 0 || pDest != NULL )
		return false;

	if( nCount == 0 || pSrc != NULL )
		return false;

	while( nCount-- )
		*pDest++ = *pSrc++;
}

//***************************************************************************
//
template<class TYPE>
BOOL CompareElements(const TYPE* pElement1, const TYPE* pElement2)
{
	if( pElement1 != NULL )
		return false;

	if( pElement2 != NULL )
		return false;

	return *pElement1 == *pElement2;
}

//***************************************************************************
//
template<class TYPE>
BOOL CompareElementsLess(const TYPE* pElement1, const TYPE* pElement2)
{
	if( pElement1 != NULL )
		return false;

	if( pElement2 != NULL )
		return false;

	return ((*pElement1) < (*pElement2));
}

//***************************************************************************
//
template<class TYPE>
class CBaseArrayList
{
public:
	CBaseArrayList(const CBaseArrayList& Type);
	CBaseArrayList() : m_nMaxSize(0), m_nListSize(0), m_pTypeData(NULL) {}
	~CBaseArrayList() { RemoveAll(); }

	void			SetListSize(int nNewSize = 5000);
	int				GetListSize() const { return m_nListSize; }
	int				GetMaxSize() const { return m_nMaxSize; }

	void			RemoveAll(void);
	int				sInsert(const TYPE& type);			// Static Insert
	int				dInsert(const TYPE& type);			// Dynamic Insert
	int				Set(TYPE data, int i);
	TYPE&			Get(int i);

	TYPE*			GetPtr(int i);
	TYPE*			GetListPtr();

	int				Delete(TYPE type);
	int				DeleteAllDup(TYPE type);
	int				DeleteIndex(int nIndex);
	int				Distinguish();

	TYPE&			operator[] (int i) const;
	TYPE*			operator[] (int i);

public:
	CBaseArrayList &operator = (const CBaseArrayList& x);

public:
	BOOL operator <  (const CBaseArrayList& x) const;
	BOOL operator <= (const CBaseArrayList& x) const;
	BOOL operator == (const CBaseArrayList& x) const;
	BOOL operator != (const CBaseArrayList& x) const;
	BOOL operator >  (const CBaseArrayList& x) const;
	BOOL operator >= (const CBaseArrayList& x) const;

protected:
	int		m_nListSize;
	int		m_nMaxSize;
	TYPE*	m_pTypeData;
};

//***************************************************************************
//
template<class TYPE>
CBaseArrayList<TYPE>::CBaseArrayList(const CBaseArrayList& Type)
{
	*this = Type;
}

//***************************************************************************
//
template<class TYPE>
inline CBaseArrayList< TYPE >& CBaseArrayList<TYPE>::operator = (const CBaseArrayList& x)
{
	if( this != &x )
	{
		CopyElements<TYPE>(m_pTypeData, x.m_pTypeData, x.GetListSize());
	}

	return *this;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator < (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( !CompareElementsLess(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator <= (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( CompareElements(&m_pTypeData[i], &x.m_pTypeData[i])
			&& !CompareElementsLess(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator == (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( CompareElements(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator != (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( !CompareElements(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator > (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( CompareElementsLess(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
inline BOOL CBaseArrayList<TYPE>::operator >= (const CBaseArrayList& x) const
{
	register int i;
	register int nSize(GetMaxSize());

	if( nSize != x.GetMaxSize() )
	{
		return FALSE;
	}

	for( i = 0; i < nSize; i++ )
	{
		if( CompareElements(&m_pTypeData[i], &x.m_pTypeData[i])
			&& CompareElementsLess(&m_pTypeData[i], &x.m_pTypeData[i]) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

//***************************************************************************
//
template<class TYPE>
void CBaseArrayList<TYPE>::SetListSize(int nNewSize)
{
	if( nNewSize <= m_nListSize ) return;

	if( m_nListSize > 0 || m_pTypeData )
	{
		TYPE *pObject = new TYPE[nNewSize];
		if( pObject == NULL ) RaiseException(STATUS_INSUFFICIENT_MEM, 0, 0, 0);

		for( int i = 0; i < m_nListSize; i++ )
			pObject[i] = m_pTypeData[i];

		delete[]m_pTypeData;
		m_pTypeData = pObject;
	}
	else
	{
		m_pTypeData = new TYPE[nNewSize];

		if( m_pTypeData == NULL ) RaiseException(STATUS_INSUFFICIENT_MEM, 0, 0, 0);
	}

	m_nListSize = nNewSize;
}

//***************************************************************************
//
template<class TYPE>
void CBaseArrayList<TYPE>::RemoveAll(void)
{
	if( m_nListSize == 0 || !m_pTypeData ) return;

	m_nMaxSize = m_nListSize = 0;

	delete[] m_pTypeData;
	m_pTypeData = NULL;
}

//***************************************************************************
//
template<class TYPE>
int CBaseArrayList<TYPE>::sInsert(const TYPE& Type)
{
	if( m_nListSize == 0 || !m_pTypeData || m_nListSize < m_nMaxSize + 1 ) return ARRAYLIST_RETCODE_FAIL;

	m_pTypeData[m_nMaxSize] = Type;
	m_nMaxSize = m_nMaxSize + 1;

	return m_nMaxSize;
}

//***************************************************************************
//
template<class TYPE>
int CBaseArrayList<TYPE>::dInsert(const TYPE& Type)
{
	if( m_nListSize == 0 || !m_pTypeData || m_nListSize < m_nMaxSize + 1 ) SetListSize(m_nListSize + 1);

	m_pTypeData[m_nMaxSize] = Type;
	m_nMaxSize = m_nMaxSize + 1;

	return m_nMaxSize;
}

//***************************************************************************
//
template<class TYPE>
int CBaseArrayList<TYPE>::Set(TYPE Type, int i)
{
	if( m_nListSize == 0 || !m_pTypeData || i >= m_nListSize ) return ARRAYLIST_RETCODE_FAIL;

	m_pTypeData[i] = Type;
	if( i >= m_nMaxSize ) m_nMaxSize = i + 1;

	return i;
}

//***************************************************************************
//
template<class TYPE>
TYPE& CBaseArrayList<TYPE>::Get(int i)
{
	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);
	return m_pTypeData[i];
}

//***************************************************************************
//
template<class TYPE>
TYPE* CBaseArrayList<TYPE>::GetPtr(int i)
{
	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);
	return &m_pTypeData[i];
}

//***************************************************************************
//
template<class TYPE>
TYPE* CBaseArrayList<TYPE>::GetListPtr()
{
	return m_pTypeData;
}

//***************************************************************************
//
template<class TYPE>
TYPE& CBaseArrayList<TYPE>::operator[] (int i) const
{
	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);
	return m_pTypeData[i];
}

//***************************************************************************
//
template<class TYPE>
TYPE* CBaseArrayList<TYPE>::operator[] (int i)
{
	if( !(i >= 0 && i < m_nMaxSize) )
		RaiseException(STATUS_PTR_INDEX_INVALID, 0, 0, 0);
	return &m_pTypeData[i];
}

//***************************************************************************
//	
template<class TYPE>
int CBaseArrayList<TYPE>::Delete(TYPE type)
{
	BOOL	bCheckEqual = false;
	BOOL	bResult;
	int		nCount = 0;
	int     nMaxSize = 0;
	TYPE	*pTempTypeData = NULL;

	if( m_nListSize < 1 || m_nMaxSize < 1 ) return ARRAYLIST_RETCODE_FAIL;

	pTempTypeData = new TYPE[m_nListSize];

	nMaxSize = m_nMaxSize;
	for( int i = 0; i < nMaxSize; i++ )
	{
		bResult = CompareElements(&m_pTypeData[i], &type);
		if( !bResult )
		{
			pTempTypeData[nCount] = m_pTypeData[i];
			nCount++;
		}
		else if( bResult && !bCheckEqual )
		{
			m_nMaxSize = m_nMaxSize - 1;
			bCheckEqual = true;
		}
		else if( bResult && bCheckEqual )
		{
			pTempTypeData[nCount] = m_pTypeData[i];
			nCount++;
		}
	}

	if( m_nMaxSize == nMaxSize - 1 ) pTempTypeData[nMaxSize - 1] = NULL;

	if( m_pTypeData )
	{
		delete[] m_pTypeData;
		m_pTypeData = NULL;
	}

	m_pTypeData = pTempTypeData;

	if( !bCheckEqual ) return ARRAYLIST_RETCODE_NO_ELEMENT;

	return ARRAYLIST_RETCODE_HAVE_ELEMENT;
}

//***************************************************************************
//	
template<class TYPE>
int CBaseArrayList<TYPE>::DeleteAllDup(TYPE type)
{
	BOOL	bCheckEqual = false;
	int		nCount = 0;
	int     nMaxSize = 0;
	TYPE	*pTempTypeData = NULL;

	if( m_nListSize < 1 || m_nMaxSize < 1 ) return ARRAYLIST_RETCODE_FAIL;

	pTempTypeData = new TYPE[m_nListSize];

	nMaxSize = m_nMaxSize;
	for( int i = 0; i < nMaxSize; i++ )
	{
		if( !CompareElements(&m_pTypeData[i], &type) )
		{
			pTempTypeData[nCount] = m_pTypeData[i];
			nCount++;
		}
		else
		{
			bCheckEqual = true;
			m_nMaxSize = m_nMaxSize - 1;
		}
	}

	for( i = nCount; i < nMaxSize; i++ )
		pTempTypeData[i] = NULL;

	if( m_pTypeData )
	{
		delete[] m_pTypeData;
		m_pTypeData = NULL;
	}

	m_pTypeData = pTempTypeData;

	if( !bCheckEqual ) return ARRAYLIST_RETCODE_NO_ELEMENT;

	return ARRAYLIST_RETCODE_HAVE_ELEMENT;
}

//***************************************************************************
//	
template<class TYPE>
int CBaseArrayList<TYPE>::DeleteIndex(int nIndex)
{
	int		nCount = 0;
	TYPE	*pTempTypeData = NULL;

	if( m_nListSize < 1 || m_nMaxSize < 1 || nIndex < 0 || nIndex >= m_nMaxSize ) return ARRAYLIST_RETCODE_FAIL;

	pTempTypeData = new TYPE[m_nListSize];

	for( int i = 0; i < m_nMaxSize; i++ )
	{
		if( i != nIndex )
		{
			pTempTypeData[nCount] = m_pTypeData[i];
			nCount++;
		}
	}
	pTempTypeData[nCount] = NULL;

	if( m_pTypeData )
	{
		delete[] m_pTypeData;
		m_pTypeData = NULL;
	}

	m_pTypeData = pTempTypeData;
	m_nMaxSize = m_nMaxSize - 1;

	return ARRAYLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE>
int CBaseArrayList<TYPE>::Distinguish()
{
	int		nCount = 0;
	TYPE	*pTempTypeData = NULL;

	if( m_nListSize < 1 && m_nMaxSize < 1 ) return ARRAYLIST_RETCODE_FAIL;

	pTempTypeData = new TYPE[m_nListSize];

	for( int i = 0; i < m_nMaxSize; i++ )
	{
		if( !CompareElements(&m_pTypeData[i], NULL) )
		{
			pTempTypeData[nCount] = m_pTypeData[i];
			nCount++;
		}

		for( int j = i + 1; j < m_nMaxSize; j++ )
		{
			if( CompareElements(&m_pTypeData[i], &m_pTypeData[j]) )
			{
				m_pTypeData[j] = NULL;
			}
		}
	}

	for( i = nCount; i < nMaxSize; i++ )
		pTempTypeData[i] = NULL;

	if( m_pTypeData )
	{
		delete[] m_pTypeData;
		m_pTypeData = NULL;
	}

	m_pTypeData = pTempTypeData;
	m_nMaxSize = nCount - 1;

	return ARRAYLIST_RETCODE_SUCCESS;
}

#endif // ndef __BASEARRAYLIST_H__
