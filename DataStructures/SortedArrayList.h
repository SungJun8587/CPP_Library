
//***************************************************************************
// SortedArrayList.h: interface for the CSortedArrayList class.
//
//***************************************************************************

#ifndef __SORTEDARRAYLIST_H__
#define __SORTEDARRAYLIST_H__

#ifndef __SWAP_H__
#include "Swap.h"
#endif

#ifndef __BASEARRAYLIST_H__
#include "BaseArrayList.h"
#endif

//***************************************************************************
//
template<class TYPE>
class CSortedArrayList : public CBaseArrayList<TYPE>
{
	// Atributes
public:
	int m_nCutOff;		// variable for sort speed tunning. Recommended range [3..128]

// Construction
public:
	CSortedArrayList();
	CSortedArrayList(const CSortedArrayList& x);
	virtual ~CSortedArrayList();

	// Assigment
public:
	CSortedArrayList & operator = (const CSortedArrayList& x);

	// Comparison 
public:
	bool operator <  (const CSortedArrayList& x) const;
	bool operator <= (const CSortedArrayList& x) const;
	bool operator == (const CSortedArrayList& x) const;
	bool operator != (const CSortedArrayList& x) const;
	bool operator >  (const CSortedArrayList& x) const;
	bool operator >= (const CSortedArrayList& x) const;

	// Operator
public:
	operator CBaseArrayList<TYPE>();

	// Method
public:
	virtual int AddSorted(TYPE tValue);				// insert new element to sorted array and return insertion index
	virtual void SetAt(TYPE tValue);					// insert new element to sorted array. 
	virtual int Lookup(TYPE tValue);					// binary search if fail return -1
	virtual int Search(TYPE tValue);					// binary search if fail return nearest item
	virtual void DataSort(int nType, int nMethod);	// sort

// Private sort method
protected:
	void BubbleSortAsc(int nLow, int nHigh);
	void InsertionSortAsc(int nLow, int nHigh);
	void SelectSortAsc(int nLow, int nHigh);
	void QuickSortAsc(int nLow, int nHigh);
	void ShellSecondSortAsc(int nHigh);
	void ShellThirdSortAsc(int nHigh);

	void BubbleSortDesc(int nLow, int nHigh);
	void InsertionSortDesc(int nLow, int nHigh);
	void SelectSortDesc(int nLow, int nHigh);
	void QuickSortDesc(int nLow, int nHigh);
	void ShellSecondSortDesc(int nHigh);
	void ShellThirdSortDesc(int nHigh);
};

//***************************************************************************
//
template<class TYPE>
inline CSortedArrayList<TYPE>::CSortedArrayList()
{
	m_nCutOff = 64;
}

//***************************************************************************
//
template<class TYPE>
inline CSortedArrayList<TYPE>::CSortedArrayList(const CSortedArrayList& x)
{
	*this = x;
}

//***************************************************************************
//
template<class TYPE>
inline CSortedArrayList<TYPE>::~CSortedArrayList()
{
}

//***************************************************************************
//
template<class TYPE>
inline CSortedArrayList<TYPE>& CSortedArrayList<TYPE>::operator = (const CSortedArrayList& x)
{
	if( this != &x )
	{
		m_nCutOff = x.m_nCutOff;
		Copy(x);
	}

	return *this;
}

//***************************************************************************
//
template<class TYPE>
inline int CSortedArrayList<TYPE>::Search(TYPE tValue)
{	// binary search if fail return nearest item
	int nLow = 0, nHigh = GetMaxSize(), nMid;

	while( nLow < nHigh )
	{
		nMid = (nLow + nHigh) / 2;
		if( CompareElementsLess(&m_pTypeData[nMid], &tValue) )
		{
			nLow = nMid + 1;
		}
		else
		{
			nHigh = nMid;
		}
	}

	return nLow;
}

//***************************************************************************
//
template<class TYPE>
inline int CSortedArrayList<TYPE>::Lookup(TYPE tValue)
{	// binary search if fail return -1
	int nRet = Search(tValue);

	if( nRet < GetMaxSize() )
	{
		if( CompareElements(&m_pTypeData[nRet], &tValue) )
		{
			return nRet;
		}
	}

	return -1;
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::BubbleSortAsc(int nLow, int nHigh)
{
	int		i, j;

	for( i = 0; i < nHigh - 1; i++ )
	{
		for( j = i + 1; j < nHigh; j++ )
		{
			if( !CompareElementsLess(&m_pTypeData[i], &m_pTypeData[j]) )
				Swap(m_pTypeData[i], m_pTypeData[j]);
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::BubbleSortDesc(int nLow, int nHigh)
{
	int		i, j;

	for( i = 0; i < nHigh - 1; i++ )
	{
		for( j = i + 1; j < nHigh; j++ )
		{
			if( CompareElementsLess(&m_pTypeData[i], &m_pTypeData[j]) )
				Swap(m_pTypeData[i], m_pTypeData[j]);
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::InsertionSortAsc(int nLow, int nHigh)
{
	int		i, j;

	for( i = nLow + 1; i <= nHigh; i++ )
	{
		TYPE tValue = m_pTypeData[i];

		for( j = i; j > nLow && !CompareElementsLess(&m_pTypeData[j - 1], &tValue); j-- )
		{
			Set(m_pTypeData[j - 1], j);
		}

		Set(tValue, j);
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::InsertionSortDesc(int nLow, int nHigh)
{
	int		i, j;

	for( i = nLow + 1; i <= nHigh; i++ )
	{
		TYPE tValue = m_pTypeData[i];

		for( j = i; j > nLow && CompareElementsLess(&m_pTypeData[j - 1], &tValue); j-- )
		{
			Set(m_pTypeData[j - 1], j);
		}

		Set(tValue, j);
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::SelectSortAsc(int nLow, int nHigh)
{
	int		i, j;
	int		nIndex;

	for( i = nLow; i < nHigh - 1; i++ )
	{
		nIndex = i;

		TYPE tValue = m_pTypeData[i];

		for( j = i + 1; j < nHigh; j++ )
		{
			if( CompareElementsLess(&m_pTypeData[j], &tValue) )
			{
				tValue = m_pTypeData[j];
				nIndex = j;
			}
		}

		Set(m_pTypeData[i], nIndex);
		Set(tValue, i);
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::SelectSortDesc(int nLow, int nHigh)
{
	int		i, j;
	int		nIndex;

	for( i = nLow; i < nHigh - 1; i++ )
	{
		nIndex = i;

		TYPE tValue = m_pTypeData[i];

		for( j = i + 1; j < nHigh; j++ )
		{
			if( !CompareElementsLess(&m_pTypeData[j], &tValue) )
			{
				tValue = m_pTypeData[j];
				nIndex = j;
			}
		}

		Set(m_pTypeData[i], nIndex);
		Set(tValue, i);
	}
}

//***************************************************************************
//
// Quicksort: sort first N items in array A
// TYPE: must have copy constructor, operator =, and operator <
template<class TYPE>
inline void CSortedArrayList<TYPE>::QuickSortAsc(int nLow, int nHigh)
{
	int		i, j;
	int		nPivotIndex, nRandomIndex;
	TYPE	Pivot;

	if( nLow >= nHigh ) return;

	if( nLow < nHigh )
	{
		nRandomIndex = (int)(rand() % (nHigh - nLow + 1)) + nLow;

		Swap(m_pTypeData[nRandomIndex], m_pTypeData[nLow]);

		Pivot = m_pTypeData[nHigh];
		i = nLow - 1;

		for( j = nLow; j <= nHigh - 1; j++ )
		{
			if( !CompareElementsLess(&Pivot, &m_pTypeData[j]) )
			{
				i = i + 1;
				Swap(m_pTypeData[i], m_pTypeData[j]);
			}
		}

		Swap(m_pTypeData[i + 1], m_pTypeData[nHigh]);

		nPivotIndex = i + 1;

		QuickSortAsc(nLow, nPivotIndex - 1);
		QuickSortAsc(nPivotIndex + 1, nHigh);
	}
}

//***************************************************************************
//
// Quicksort: sort first N items in array A
// TYPE: must have copy constructor, operator =, and operator <
template<class TYPE>
inline void CSortedArrayList<TYPE>::QuickSortDesc(int nLow, int nHigh)
{
	int		i, j;
	int		nPivotIndex, nRandomIndex;
	TYPE	Pivot;

	if( nLow >= nHigh ) return;

	if( nLow < nHigh )
	{
		nRandomIndex = (int)(rand() % (nHigh - nLow + 1)) + nLow;

		Swap(m_pTypeData[nRandomIndex], m_pTypeData[nLow]);

		Pivot = m_pTypeData[nHigh];
		i = nLow - 1;

		for( j = nLow; j <= nHigh - 1; j++ )
		{
			if( CompareElementsLess(&Pivot, &m_pTypeData[j]) )
			{
				i = i + 1;
				Swap(m_pTypeData[i], m_pTypeData[j]);
			}
		}

		Swap(m_pTypeData[i + 1], m_pTypeData[nHigh]);

		nPivotIndex = i + 1;

		QuickSortDesc(nLow, nPivotIndex - 1);
		QuickSortDesc(nPivotIndex + 1, nHigh);
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::ShellSecondSortAsc(int nHigh)
{
	int		i, j, k;
	TYPE	TempTypeData;

	for( int nRound = nHigh / 2; nRound > 0; nRound = nRound / 2 )
	{
		for( i = 0; i < nRound; i++ )
		{
			for( j = i + nRound; j < nHigh; j = j + nRound )
			{
				TempTypeData = m_pTypeData[j];
				k = j;
				while( k > nRound - 1 && !CompareElementsLess(&m_pTypeData[k - nRound], &TempTypeData) )
				{
					Set(m_pTypeData[k - nRound], k);
					k = k - nRound;
				}

				Set(TempTypeData, k);
			}
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::ShellSecondSortDesc(int nHigh)
{
	int		i, j, k;
	int		nRound;
	TYPE	TempTypeData;

	for( nRound = nHigh / 2; nRound > 0; nRound = nRound / 2 )
	{
		for( i = 0; i < nRound; i++ )
		{
			for( j = i + nRound; j < nHigh; j = j + nRound )
			{
				TempTypeData = m_pTypeData[j];
				k = j;
				while( k > nRound - 1 && CompareElementsLess(&m_pTypeData[k - nRound], &TempTypeData) )
				{
					Set(m_pTypeData[k - nRound], k);
					k = k - nRound;
				}

				Set(TempTypeData, k);
			}
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::ShellThirdSortAsc(int nHigh)
{
	int		i, j, k;
	int		nRound;
	TYPE	TempTypeData;

	for( nRound = 1; nRound < nHigh; nRound = (3 * nRound) + 1 );
	for( nRound = nRound / 3; nRound > 0; nRound = nRound / 3 )
	{
		for( i = 0; i < nRound; i++ )
		{
			for( j = i + nRound; j < nHigh; j = j + nRound )
			{
				TempTypeData = m_pTypeData[j];
				k = j;
				while( k > nRound - 1 && !CompareElementsLess(&m_pTypeData[k - nRound], &TempTypeData) )
				{
					Set(m_pTypeData[k - nRound], k);
					k = k - nRound;
				}

				Set(TempTypeData, k);
			}
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::ShellThirdSortDesc(int nHigh)
{
	int		i, j, k;
	int		nRound;
	TYPE	TempTypeData;

	for( nRound = 1; nRound < nHigh; nRound = (3 * nRound) + 1 );
	for( nRound = nRound / 3; nRound > 0; nRound = nRound / 3 )
	{
		for( i = 0; i < nRound; i++ )
		{
			for( j = i + nRound; j < nHigh; j = j + nRound )
			{
				TempTypeData = m_pTypeData[j];
				k = j;
				while( k > nRound - 1 && CompareElementsLess(&m_pTypeData[k - nRound], &TempTypeData) )
				{
					Set(m_pTypeData[k - nRound], k);
					k = k - nRound;
				}

				Set(TempTypeData, k);
			}
		}
	}
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::DataSort(int nType, int nMethod)
{
	switch( nType )
	{
		case 1:
			{
				switch( nMethod )
				{
					case 1:
						BubbleSortAsc(0, GetMaxSize());
						break;
					case 2:
						InsertionSortAsc(0, GetMaxSize() - 1);
						break;
					case 3:
						SelectSortAsc(0, GetMaxSize());
						break;
					case 4:
						QuickSortAsc(0, GetMaxSize() - 1);
						break;
					case 5:
						ShellSecondSortAsc(GetMaxSize());
						break;
					case 6:
						ShellThirdSortAsc(GetMaxSize());
						break;
				}
				break;
			}
		case 2:
			{
				switch( nMethod )
				{
					case 1:
						BubbleSortDesc(0, GetMaxSize());
						break;
					case 2:
						InsertionSortDesc(0, GetMaxSize() - 1);
						break;
					case 3:
						SelectSortDesc(0, GetMaxSize());
						break;
					case 4:
						QuickSortDesc(0, GetMaxSize() - 1);
						break;
					case 5:
						ShellSecondSortDesc(GetMaxSize());
						break;
					case 6:
						ShellThirdSortDesc(GetMaxSize());
						break;
				}
				break;
			}
	}
}

//***************************************************************************
//
template<class TYPE>
inline int CSortedArrayList<TYPE>::AddSorted(TYPE tValue)
{
	int nInsertIndex = Search(tValue);

	if( nInsertIndex < GetListSize() )
	{
		Set(tValue, nInsertIndex);
	}
	else
	{
		if( GetMaxSize() <= GetListSize() )
			nInsertIndex = dInsert(tValue);
	}

	return nInsertIndex;
}

//***************************************************************************
//
template<class TYPE>
inline void CSortedArrayList<TYPE>::SetAt(TYPE tValue)
{
	AddSorted(tValue);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator < (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator < (x);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator <= (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator <= (x);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator == (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator == (x);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator != (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator != (x);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator > (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator > (x);
}

//***************************************************************************
//
template<class TYPE>
inline bool CSortedArrayList<TYPE>::operator >= (const CSortedArrayList& x) const
{
	return CBaseArrayList<TYPE>::operator >= (x);
}

//***************************************************************************
//
template<class TYPE>
inline CSortedArrayList<TYPE>::operator CBaseArrayList<TYPE>()
{
	return *this;
}

#endif // ndef __SORTED_ARRAY_H__
