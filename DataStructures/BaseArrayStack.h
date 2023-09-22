
//***************************************************************************
// BaseArrayStack.h: interface for the CBaseArrayStack class.
//
//***************************************************************************

#ifndef __BASEARRAYSTACK_H__
#define __BASEARRAYSTACK_H__

#include <memory.h>
#include <assert.h>

#define STATUS_INSUFFICIENT_MEM		0xE0000001
#define GROWTH_DEFAULT				50	// DEFAULT = 50%

//***************************************************************************
//
template<class TYPE>
class CBaseArrayStack
{
public:
	CBaseArrayStack();
	CBaseArrayStack(int nSize);
	CBaseArrayStack(int nSize, int nGrowth);
	~CBaseArrayStack();

	void	RemoveAll();

	inline	int	GetSize() const { return m_nSize - 1; }
	inline	int	GetCapa() const { return (m_nSize - GetDataSize() - 1); }
	inline	int	GetDataSize() const { return GetDataSize(m_nTop, m_nEnd); }
	void	SetSize(int nNewSize);
	int		Get(TYPE& Element);
	int		Put(TYPE Element);

protected:
	inline	int		GetDataSize(int nTop, int nEnd) const;
	inline	int		NextPos(int nPos);
	inline	int		PrevPos(int nPos);
	inline	void	Initialize(TYPE** pData, int nSize);

protected:
	TYPE**	m_pData;
	int		m_nSize;
	int		m_nGrowth;
	int		m_nTop;
	int		m_nEnd;
};

//***************************************************************************
// Construction/Destruction
//***************************************************************************

template<class TYPE>
CBaseArrayStack<TYPE>::CBaseArrayStack()
{
	m_nSize = 0;
	m_nGrowth = GROWTH_DEFAULT;
	m_pData = NULL;
	m_nTop = m_nEnd = 0;
}

template<class TYPE>
CBaseArrayStack<TYPE>::CBaseArrayStack(int nSize)
{
	m_nSize = 0;
	m_nGrowth = GROWTH_DEFAULT;
	m_pData = NULL;
	m_nTop = m_nEnd = 0;

	if( nSize > 0 ) SetSize(nSize);
}

template<class TYPE>
CBaseArrayStack<TYPE>::CBaseArrayStack(int nSize, int nGrowth)
{
	m_nSize = 0;
	m_nGrowth = GROWTH_DEFAULT;
	m_pData = NULL;
	m_nTop = m_nEnd = 0;

	if( nGrowth > 0 ) m_nGrowth = nGrowth;
	else m_nGrowth = 0;

	if( nSize > 0 ) SetSize(nSize);
}

template<class TYPE>
CBaseArrayStack<TYPE>::~CBaseArrayStack()
{
	for( int i = 0; i < m_nSize; i++ )
		if( m_pData[i] != NULL ) delete m_pData[i];

	delete[]m_pData;
}

//***************************************************************************
//
template<class TYPE>
void CBaseArrayStack<TYPE>::RemoveAll()
{
	if( m_nSize == 0 || m_pData == NULL ) return;

	for( int i = m_nTop; GetDataSize(i, m_nEnd) > 0; i = NextPos(i) )
		if( m_pData[i] != NULL ) delete m_pData[i];

	delete[]m_pData;
	m_nSize = 0;
	m_nTop = m_nEnd = 0;
}

//***************************************************************************
//
template<class TYPE>
inline int CBaseArrayStack<TYPE>::GetDataSize(int nTop, int nEnd) const
{
	if( m_nSize == 0 || m_pData == NULL || nTop == nEnd ) return 0;
	else if( nEnd > nTop )	return nEnd - nTop;
	else return m_nSize - nTop + nEnd;
}

//***************************************************************************
//
template<class TYPE>
inline int CBaseArrayStack<TYPE>::NextPos(int nPos)
{
	return ++nPos % m_nSize;
}

//***************************************************************************
//
template<class TYPE>
inline int CBaseArrayStack<TYPE>::PrevPos(int nPos)
{
	return --nPos % m_nSize;
}

//***************************************************************************
//
template<class TYPE>
inline void CBaseArrayStack<TYPE>::Initialize(TYPE** pData, int nSize)
{
	if( pData == NULL ) return;

	for( int i = 0; i < nSize; i++ ) pData[i] = NULL;
}

//***************************************************************************
//
template<class TYPE>
void CBaseArrayStack<TYPE>::SetSize(int nNewSize)
{
	int i;

	if( nNewSize > 0 ) nNewSize++;
	if( nNewSize <= m_nSize ) return;

	if( m_nSize > 0 || m_pData != NULL )
	{
		TYPE** pTempData = new TYPE*[nNewSize];
		if( pTempData == NULL ) RaiseException(STATUS_INSUFFICIENT_MEM, 0, 0, 0);

		Initialize(pTempData, nNewSize);

		if( GetDataSize(m_nTop, m_nEnd) > 0 )
		{
			int nTemp = m_nTop;

			for( i = 0; GetDataSize(nTemp, m_nEnd) > 0; nTemp = NextPos(nTemp), i++ )
			{
				pTempData[i] = m_pData[nTemp];
				m_pData[nTemp] = NULL;
			}

			m_nTop = 0;	m_nEnd = i;
		}
		else m_nTop = m_nEnd = 0;

		delete[]m_pData;
		m_pData = pTempData;
	}
	else
	{
		m_pData = new TYPE*[nNewSize];
		if( m_pData == NULL ) RaiseException(STATUS_INSUFFICIENT_MEM, 0, 0, 0);

		Initialize(m_pData, nNewSize);
		m_nTop = m_nEnd = 0;
	}

	m_nSize = nNewSize;

	return;
}

//***************************************************************************
//
template<class TYPE>
int CBaseArrayStack<TYPE>::Put(TYPE Object)
{
	if( m_nSize == 0 || m_pData == NULL ) return -1;
	if( GetCapa() <= 0 )
	{
		if( m_nGrowth > 0 )
		{
			SetSize((m_nSize * (m_nGrowth + 100)) / 100);
			if( GetCapa() <= 0 ) return -1;
		}
		else return -1;
	}

	TYPE* pTempObject = NULL;
	pTempObject = new TYPE;
	if( pTempObject == NULL ) pTempObject = new TYPE;

	memcpy(pTempObject, &Object, sizeof(Object));

	m_pData[m_nEnd] = pTempObject;
	m_nEnd = NextPos(m_nEnd);

	return 0;
}

//***************************************************************************
//
template<class TYPE>
int CBaseArrayStack<TYPE>::Get(TYPE& Object)
{
	if( GetDataSize() <= 0 ) return -1;

	memcpy(&Object, m_pData[m_nEnd - 1], sizeof(Object));

	delete m_pData[m_nEnd - 1];
	m_pData[m_nEnd - 1] = NULL;
	m_nEnd = PrevPos(m_nEnd);

	return 0;
}

#endif // ndef __BASEARRAYSTACK_H__

