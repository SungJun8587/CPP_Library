
//***************************************************************************
// BaseCircularLinkedList.h: interface for the CBaseCircularLinkedList class.
//
//***************************************************************************

#ifndef __BASECIRCULARLINKEDLIST_H__
#define __BASECIRCULARLINKEDLIST_H__

#include <iostream>
using namespace std;

#define LINKEDLIST_RETCODE_SUCCESS							1
#define LINKEDLIST_RETCODE_FAIL								-1
#define LINKEDLIST_RETCODE_NO_MATCH							2
#define LINKEDLIST_RETCODE_EQUAL							1
#define LINKEDLIST_RETCODE_NOT_EQUAL						0
#define LINKEDLIST_RETCODE_NO_ELEMENT						-1
#define LINKEDLIST_RETCODE_WRONG_PARAM						-2

#ifdef UNICODE
#define tcout std::wcout // unicode enabled
#else
#define tcout std::cout
#endif

#ifndef __OELEMENT__
#define __OELEMENT__

//***************************************************************************
//
template<class TYPE> class OElement
{
public:
	TYPE Element;
	OElement<TYPE>* pNext = NULL;
};

#endif // __OELEMENT__

//***************************************************************************
//
template<class TYPE> class CBaseCircularLinkedList
{
public:
	CBaseCircularLinkedList();
	virtual ~CBaseCircularLinkedList();

	int Add(const TYPE& Element);
	int	AddIndex(const TYPE& Element, const DWORD dwIndex);
	int AddFirst(const TYPE& Element);
	int AddLast(const TYPE& Element);
	int AddBeforeCurrent(const TYPE& Element);
	int AddAfterCurrent(const TYPE& Element);
	int	UpdateIndex(const DWORD dwIndex, const TYPE& Element);
	int UpdateCurrent(const TYPE& Element);
	int Subtract(const TYPE& Element);
	int SubtractAllDup(const TYPE& Element);
	int	SubtractIndex(const DWORD dwIndex);
	int SubtractFirst();
	int SubtractLast();
	int SubtractCurrent();
	TYPE* MoveFirst();
	TYPE* MoveLast();
	TYPE* MovePrev();
	TYPE* MoveNext();
	TYPE* MoveIndex(const DWORD dwIndex);
	TYPE* GetCurrent() const;
	TYPE  At(const DWORD dwIndex) const;
	int GetCount() const;
	int Reset();

#ifdef _UNICODE
	void Printing(wostream& StreamBuffer);
#else
	void Printing(ostream& StreamBuffer);
#endif

private:
	int Compare(const BYTE* pBCompee, const BYTE* pBComper, const DWORD dwSize) const;

private:
	OElement<TYPE>* m_pHead;
	OElement<TYPE>* m_pTail;
	OElement<TYPE>* m_pCurrent;

	DWORD m_dwCount;
};

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

template<class TYPE> CBaseCircularLinkedList<TYPE>::CBaseCircularLinkedList()
{
	m_pHead = NULL;
	m_pTail = NULL;
	m_pCurrent = NULL;

	m_dwCount = 0;
}

template<class TYPE> CBaseCircularLinkedList<TYPE>::~CBaseCircularLinkedList()
{
	Reset();
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::Add(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pHead == NULL ) // The First TYPE Element.
	{
		pElement->pNext = pElement;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pPrevElement = m_pTail;
		pPrevElement->pNext = pElement;
		pElement->pNext = m_pHead;
		m_pTail = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::AddIndex(const TYPE& Element, const DWORD dwIndex)
{
	OElement<TYPE>* pElement = NULL;
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( dwIndex < 0 || dwIndex > m_dwCount ) return LINKEDLIST_RETCODE_WRONG_PARAM;

	pNext = m_pHead;
	for( DWORD i = 0; i < dwIndex; i++ )
	{
		pPrevElement = pNext;
		pNext = pNext->pNext;
	}

	if( pPrevElement == NULL )
	{
		pElement->pNext = m_pHead;
		m_pHead = pElement;
		m_pTail->pNext = pElement;
	}
	else
	{
		if( pNext == m_pHead )
		{
			pElement->pNext = m_pHead;
			m_pTail->pNext = pElement;
			m_pTail = pElement;
		}
		else
		{
			pPrevElement->pNext = pElement;
			pElement->pNext = pNext;
		}
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::AddFirst(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pHead == NULL ) // The First TYPE Element.
	{
		pElement->pNext = pElement;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pElement->pNext = m_pHead;
		m_pHead = pElement;
		m_pTail->pNext = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::AddLast(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pHead == NULL ) // The First TYPE Element.
	{
		pElement->pNext = m_pHead;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pPrevElement = m_pTail;
		pPrevElement->pNext = pElement;
		pElement->pNext = m_pHead;
		m_pTail = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::AddBeforeCurrent(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	pNext = m_pHead;
	do
	{
		if( pNext == m_pCurrent ) break;

		pPrevElement = pNext;
		pNext = pNext->pNext;
	} while( pNext != m_pHead );

	if( pPrevElement != NULL )
		pPrevElement->pNext = pElement;
	else
	{
		m_pHead = pElement;
		m_pTail->pNext = pElement;
	}

	pElement->pNext = m_pCurrent;
	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::AddAfterCurrent(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pCurrent->pNext == m_pHead )
	{
		pPrevElement = m_pTail;
		pPrevElement->pNext = pElement;
		pElement->pNext = m_pHead;
		m_pTail = pElement;
	}
	else
	{
		pElement->pNext = m_pCurrent->pNext;
		m_pCurrent->pNext = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::UpdateCurrent(const TYPE& Element)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	memcpy(&m_pCurrent->Element, &Element, sizeof(Element));

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE& Element)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	if( dwIndex < 0 || dwIndex >= m_dwCount ) return LINKEDLIST_RETCODE_WRONG_PARAM;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		for( int i = 0; i < dwIndex; i++ )
		{
			pNext = pNext->pNext;
		}
	}

	memcpy(&pNext->Element, &Element, sizeof(Element));

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::Subtract(const TYPE& Element)
{
	DWORD dwIndex = 0;
	DWORD dwLinkCount = 0;
	int nRetCode = -1;

	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	nRetCode = LINKEDLIST_RETCODE_NO_MATCH;

	dwLinkCount = m_dwCount;
	pNext = m_pHead;
	do
	{
		if( LINKEDLIST_RETCODE_EQUAL == Compare((BYTE *)&pNext->Element, (BYTE *)&Element, sizeof(TYPE)) )
		{
			if( pNext->pNext == m_pHead )
			{
				if( pPrevElement == NULL || pPrevElement == m_pTail )
				{
					m_pCurrent = m_pHead = m_pTail = NULL;
				}
				else
				{
					pPrevElement->pNext = m_pHead;
					m_pTail = pPrevElement;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pTail;
					}
				}
			}
			else
			{
				if( pPrevElement == NULL )
				{
					m_pTail->pNext = pNext->pNext;
					m_pHead = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pHead;
					}
				}
				else
				{
					pPrevElement->pNext = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = pNext->pNext;
					}
				}
			}
			m_dwCount--;

			if( pNext )
			{
				delete pNext;
				pNext = NULL;
			}

			nRetCode = LINKEDLIST_RETCODE_SUCCESS;
			break;
		}

		if( pNext != m_pHead ) pPrevElement = pNext;
		pNext = pNext->pNext;

		dwIndex++;
	} while( dwIndex < dwLinkCount );

	return nRetCode;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::SubtractAllDup(const TYPE& Element)
{
	DWORD dwIndex = 0;
	DWORD dwLinkCount = 0;
	int nRetCode = -1;

	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;
	OElement<TYPE>* pTempElement;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	nRetCode = LINKEDLIST_RETCODE_NO_MATCH;

	dwLinkCount = m_dwCount;
	pNext = m_pHead;
	do
	{
		pTempElement = pNext->pNext;
		if( LINKEDLIST_RETCODE_EQUAL == Compare((BYTE *)&pNext->Element, (BYTE *)&Element, sizeof(TYPE)) )
		{
			if( pNext->pNext == m_pHead )
			{
				if( pPrevElement == NULL || pPrevElement == m_pTail )
				{
					m_pCurrent = m_pHead = m_pTail = pTempElement = NULL;
				}
				else
				{
					pPrevElement->pNext = m_pHead;
					m_pTail = pPrevElement;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pTail;
					}
					pTempElement = NULL;
				}
			}
			else
			{
				if( pPrevElement == NULL )
				{
					m_pTail->pNext = pNext->pNext;
					m_pHead = pTempElement = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pHead;
					}
				}
				else
				{
					pPrevElement->pNext = pTempElement = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = pNext->pNext;
					}
				}
			}
			m_dwCount--;

			if( pNext )
			{
				delete pNext;
				pNext = NULL;
			}

			nRetCode = LINKEDLIST_RETCODE_SUCCESS;

			if( pTempElement == NULL )
			{
				break;
			}
		}	

		pPrevElement = pNext;
		pNext = pTempElement;

		dwIndex++;
	} while( dwIndex < dwLinkCount );

	return nRetCode;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	if( dwIndex < 0 || dwIndex >= m_dwCount ) return LINKEDLIST_RETCODE_WRONG_PARAM;

	pNext = m_pHead;
	for( DWORD i = 0; i < dwIndex; i++ )
	{
		pPrevElement = pNext;
		pNext = pNext->pNext;
	}

	if( pNext->pNext == m_pHead )
	{
		if( pPrevElement == NULL || pPrevElement == m_pTail )
		{
			m_pCurrent = m_pHead = m_pTail = NULL;
		}
		else
		{
			pPrevElement->pNext = m_pHead;
			m_pTail = pPrevElement;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pTail;
			}
		}
	}
	else
	{
		if( pPrevElement == NULL )
		{
			m_pTail->pNext = pNext->pNext;
			m_pHead = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pHead;
			}
		}
		else
		{
			pPrevElement->pNext = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = pNext->pNext;
			}
		}
	}
	m_dwCount--;

	if( pNext )
	{
		delete pNext;
		pNext = NULL;
	}

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::SubtractFirst()
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pNext = m_pHead;
	if( pNext->pNext == m_pHead )
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_pCurrent = NULL;
	}
	else
	{
		m_pHead = pNext->pNext;
		m_pTail->pNext = pNext->pNext;
		if( m_pCurrent == pNext )
		{
			m_pCurrent = pNext->pNext;
		}
	}
	m_dwCount--;

	if( pNext )
	{
		delete pNext;
		pNext = NULL;
	}

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::SubtractLast()
{
	OElement<TYPE>* pMoveNext = NULL;
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pTail == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pMoveNext = m_pHead;
	do
	{
		if( pMoveNext == m_pTail ) break;

		pPrevElement = pMoveNext;
		pMoveNext = pMoveNext->pNext;
	} while( pMoveNext != m_pHead );

	pNext = m_pTail;
	if( pPrevElement == NULL )
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_pCurrent = NULL;
	}
	else
	{
		pPrevElement->pNext = m_pHead;
		m_pTail = pPrevElement;
		if( m_pCurrent == pNext )
		{
			m_pCurrent = pPrevElement;
		}
	}
	m_dwCount--;

	if( pNext )
	{
		delete pNext;
		pNext = NULL;
	}

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::SubtractCurrent()
{
	OElement<TYPE>* pMoveNext = NULL;
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pMoveNext = m_pHead;
	do
	{
		if( pMoveNext == m_pCurrent ) break;

		pPrevElement = pMoveNext;
		pMoveNext = pMoveNext->pNext;
	} while( pMoveNext != m_pHead );

	pNext = m_pCurrent;
	if( pNext->pNext == m_pHead )
	{
		if( pPrevElement == NULL || pPrevElement == m_pTail )
		{
			m_pCurrent = m_pHead = m_pTail = NULL;
		}
		else
		{
			pPrevElement->pNext = m_pHead;
			m_pTail = pPrevElement;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pTail;
			}
		}
	}
	else
	{
		if( pPrevElement == NULL )
		{
			m_pTail->pNext = pNext->pNext;
			m_pHead = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pHead;
			}
		}
		else
		{
			pPrevElement->pNext = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = pNext->pNext;
			}
		}
	}
	m_dwCount--;

	if( pNext )
	{
		delete pNext;
		pNext = NULL;
	}

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::MoveFirst()
{
	if( m_pHead == NULL )
	{
		return NULL;
	}

	m_pCurrent = m_pHead;

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::MoveLast()
{
	if( m_pTail == NULL )
	{
		return NULL;
	}

	m_pCurrent = m_pTail;

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::MovePrev()
{
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead == NULL || m_pCurrent == NULL )
	{
		return NULL;
	}

	pNext = m_pHead;
	do
	{
		if( pNext == m_pCurrent ) break;

		pPrevElement = pNext;
		pNext = pNext->pNext;
	} while( pNext != m_pHead );

	m_pCurrent = pPrevElement;

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::MoveNext()
{
	if( m_pCurrent == NULL || m_pCurrent->pNext == NULL )
	{
		return NULL;
	}

	m_pCurrent = m_pCurrent->pNext;

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return NULL;
	}

	if( dwIndex < 0 || dwIndex >= m_dwCount ) return NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		for( DWORD i = 0; i < dwIndex; i++ )
		{
			pNext = pNext->pNext;
		}
		m_pCurrent = pNext;
	}

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseCircularLinkedList<TYPE>::GetCurrent() const
{
	if( m_pCurrent == NULL )
	{
		return NULL;
	}

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE CBaseCircularLinkedList<TYPE>::At(const DWORD dwIndex) const
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return NULL;
	}

	if( dwIndex < 0 || dwIndex >= m_dwCount ) return NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		for( DWORD i = 0; i < dwIndex; i++ )
		{
			pNext = pNext->pNext;
		}
	}

	return pNext->Element;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::GetCount() const
{
	return m_dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::Reset()
{
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		do
		{
			pPrevElement = pNext->pNext;
			if( pNext )
			{
				delete pNext;
				pNext = NULL;
			}
			pNext = pPrevElement;
		} while( pNext != m_pHead );
	}

	m_pHead = NULL;
	m_pTail = NULL;
	m_pCurrent = NULL;
	m_dwCount = 0;

	return LINKEDLIST_RETCODE_SUCCESS;
}

#ifdef _UNICODE
//***************************************************************************
//
template<class TYPE> void CBaseCircularLinkedList<TYPE>::Printing(wostream& StreamBuffer)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		do
		{
			StreamBuffer << pNext->Element;

			if( pNext->pNext != m_pHead ) StreamBuffer << " -> ";

			pNext = pNext->pNext;
		} while( pNext != m_pHead );
	}

	StreamBuffer << std::endl;
}
#else
//***************************************************************************
//
template<class TYPE> void CBaseCircularLinkedList<TYPE>::Printing(ostream& StreamBuffer)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		do
		{
			StreamBuffer << pNext->Element;

			if( pNext->pNext != m_pHead ) StreamBuffer << " -> ";

			pNext = pNext->pNext;
		} while( pNext != m_pHead );
	}

	StreamBuffer << std::endl;
}
#endif

//***************************************************************************
//	
template<class TYPE> int CBaseCircularLinkedList<TYPE>::Compare(const BYTE* pBCompee, const BYTE* pBComper, const DWORD dwSize) const
{
	DWORD LCounter;

	for( LCounter = 0; LCounter < dwSize; LCounter++ )
	{
		if( *(pBCompee + LCounter) != *(pBComper + LCounter) )
		{
			return LINKEDLIST_RETCODE_NOT_EQUAL;
		}
	}

	return LINKEDLIST_RETCODE_EQUAL;
}

#endif // ndef __BASECIRCULARLINKEDLIST_H__