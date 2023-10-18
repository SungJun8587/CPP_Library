
//***************************************************************************
// BaseDoublyLinkedList.h: interface for the CBaseDoublyLinkedList class.
//
//***************************************************************************

#ifndef __BASEDOUBLYLINKEDLIST_H__
#define __BASEDOUBLYLINKEDLIST_H__

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
	OElement<TYPE>* pPrev = nullptr;
	OElement<TYPE>* pNext = nullptr;
};

#endif // __OELEMENT__

//***************************************************************************
//
template<class TYPE> class CBaseDoublyLinkedList
{
public:
	CBaseDoublyLinkedList();
	virtual ~CBaseDoublyLinkedList();

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

template<class TYPE> CBaseDoublyLinkedList<TYPE>::CBaseDoublyLinkedList()
{
	m_pHead = NULL;
	m_pTail = NULL;
	m_pCurrent = NULL;

	m_dwCount = 0;
}

template<class TYPE> CBaseDoublyLinkedList<TYPE>::~CBaseDoublyLinkedList()
{
	Reset();
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::Add(const TYPE& Element)
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
		pElement->pPrev = NULL;
		pElement->pNext = NULL;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pPrevElement = m_pTail;
		pPrevElement->pNext = pElement;
		pElement->pPrev = pPrevElement;
		pElement->pNext = NULL;
		m_pTail = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::AddIndex(const TYPE& Element, const DWORD dwIndex)
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
		pPrevElement = m_pHead;
		pPrevElement->pPrev = pElement;
		pElement->pPrev = NULL;
		pElement->pNext = pPrevElement;
		m_pHead = pElement;
	}
	else
	{
		if( pNext == NULL )
		{
			pPrevElement->pNext = pElement;
			pElement->pPrev = pPrevElement;
			pElement->pNext = pNext;
			m_pTail = pElement;
		}
		else
		{
			pNext->pPrev->pNext = pElement;
			pElement->pPrev = pNext->pPrev;
			pElement->pNext = pNext;
		}
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::AddFirst(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pHead == NULL ) // The First TYPE Element.
	{
		pElement->pPrev = NULL;
		pElement->pNext = NULL;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pElement->pPrev = NULL;
		pElement->pNext = m_pHead;
		m_pHead = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::AddLast(const TYPE& Element)
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
		pElement->pPrev = NULL;
		pElement->pNext = NULL;
		m_pCurrent = m_pHead = m_pTail = pElement;
	}
	else
	{
		pPrevElement = m_pTail;
		pPrevElement->pNext = pElement;
		pElement->pPrev = pPrevElement;
		pElement->pNext = NULL;
		m_pTail = pElement;
	}

	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::AddBeforeCurrent(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pCurrent->pPrev == NULL )
	{
		pElement->pPrev = NULL;
		m_pHead = pElement;
	}
	else
	{
		m_pCurrent->pPrev->pNext = pElement;
		pElement->pPrev = m_pCurrent->pPrev;
	}

	pElement->pNext = m_pCurrent;
	m_pCurrent->pPrev = pElement;
	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::AddAfterCurrent(const TYPE& Element)
{
	OElement<TYPE>* pElement = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	pElement = new OElement<TYPE>;
	if( pElement == NULL )
	{
		return LINKEDLIST_RETCODE_FAIL;
	}

	if( m_pCurrent->pNext == NULL )
	{
		pElement->pNext = NULL;
		m_pTail = pElement;
	}
	else
	{
		m_pCurrent->pNext->pPrev = pElement;
		pElement->pNext = m_pCurrent->pNext;
	}

	pElement->pPrev = m_pCurrent;
	m_pCurrent->pNext = pElement;
	memcpy(&pElement->Element, &Element, sizeof(Element));

	m_dwCount++;

	return LINKEDLIST_RETCODE_SUCCESS;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::UpdateCurrent(const TYPE& Element)
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::UpdateIndex(const DWORD dwIndex, const TYPE& Element)
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::Subtract(const TYPE& Element)
{
	int nRetCode = -1;

	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	nRetCode = LINKEDLIST_RETCODE_NO_MATCH;

	pNext = m_pHead;
	while( pNext != NULL )
	{
		if( LINKEDLIST_RETCODE_EQUAL == Compare((BYTE *)&pNext->Element, (BYTE *)&Element, sizeof(TYPE)) )
		{
			if( pNext->pNext == NULL )
			{
				if( pNext->pPrev == NULL )
				{
					m_pCurrent = m_pHead = m_pTail = NULL;
				}
				else
				{
					pNext->pPrev->pNext = NULL;
					m_pTail = pNext->pPrev;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pTail;
					}
				}
			}
			else
			{
				if( pNext->pPrev == NULL )
				{
					pNext->pNext->pPrev = NULL;
					m_pHead = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pHead;
					}
				}
				else
				{
					pNext->pNext->pPrev = pNext->pPrev;
					pNext->pPrev->pNext = pNext->pNext;
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
		pNext = pNext->pNext;
	}

	return nRetCode;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::SubtractAllDup(const TYPE& Element)
{
	int nRetCode = -1;

	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pTempElement;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	nRetCode = LINKEDLIST_RETCODE_NO_MATCH;

	pNext = m_pHead;
	while( pNext != NULL )
	{
		pTempElement = pNext->pNext;
		if( LINKEDLIST_RETCODE_EQUAL == Compare((BYTE *)&pNext->Element, (BYTE *)&Element, sizeof(TYPE)) )
		{
			if( pNext->pNext == NULL )
			{
				if( pNext->pPrev == NULL )
				{
					m_pCurrent = m_pHead = m_pTail = pTempElement = NULL;
				}
				else
				{
					pNext->pPrev->pNext = NULL;
					m_pTail = pNext->pPrev;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pTail;
					}
					pTempElement = NULL;
				}
			}
			else
			{
				if( pNext->pPrev == NULL )
				{
					pNext->pNext->pPrev = NULL;
					m_pHead = pTempElement = pNext->pNext;
					if( m_pCurrent == pNext )
					{
						m_pCurrent = m_pHead;
					}
				}
				else
				{
					pNext->pNext->pPrev = pNext->pPrev;
					pNext->pPrev->pNext = pTempElement = pNext->pNext;
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
		pNext = pTempElement;
	}

	return nRetCode;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::SubtractIndex(const DWORD dwIndex)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	if( dwIndex < 0 || dwIndex >= m_dwCount ) return LINKEDLIST_RETCODE_WRONG_PARAM;

	pNext = m_pHead;
	for( DWORD i = 0; i < dwIndex; i++ )
	{
		pNext = pNext->pNext;
	}

	if( pNext->pNext == NULL )
	{
		if( pNext->pPrev == NULL )
		{
			m_pCurrent = m_pHead = m_pTail = NULL;
		}
		else
		{
			pNext->pPrev->pNext = NULL;
			m_pTail = pNext->pPrev;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pTail;
			}
		}
	}
	else
	{
		if( pNext->pPrev == NULL )
		{
			pNext->pNext->pPrev = NULL;
			m_pHead = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pHead;
			}
		}
		else
		{
			pNext->pNext->pPrev = pNext->pPrev;
			pNext->pPrev->pNext = pNext->pNext;
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::SubtractFirst()
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pNext = m_pHead;
	if( pNext->pNext == NULL )
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_pCurrent = NULL;
	}
	else
	{
		pNext->pNext->pPrev = NULL;
		m_pHead = pNext->pNext;
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::SubtractLast()
{
	OElement<TYPE>* pNext = NULL;

	if( m_pTail == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pNext = m_pTail;
	if( pNext->pPrev == NULL )
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_pCurrent = NULL;
	}
	else
	{
		pNext->pPrev->pNext = NULL;
		m_pTail = pNext->pPrev;
		if( m_pCurrent == pNext )
		{
			m_pCurrent = pNext->pPrev;
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::SubtractCurrent()
{
	OElement<TYPE>* pNext = NULL;

	if( m_pCurrent == NULL )
	{
		return LINKEDLIST_RETCODE_NO_ELEMENT;
	}

	pNext = m_pCurrent;
	if( pNext->pNext == NULL )
	{
		if( pNext->pPrev == NULL )
		{
			m_pCurrent = m_pHead = m_pTail = NULL;
		}
		else
		{
			pNext->pPrev->pNext = NULL;
			m_pTail = pNext->pPrev;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pTail;
			}
		}
	}
	else
	{
		if( pNext->pPrev == NULL )
		{
			pNext->pNext->pPrev = NULL;
			m_pHead = pNext->pNext;
			if( m_pCurrent == pNext )
			{
				m_pCurrent = m_pHead;
			}
		}
		else
		{
			pNext->pNext->pPrev = pNext->pPrev;
			pNext->pPrev->pNext = pNext->pNext;
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
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::MoveFirst()
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
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::MoveLast()
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
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::MovePrev()
{
	if( m_pCurrent == NULL || m_pCurrent->pPrev == NULL )
	{
		return NULL;
	}

	m_pCurrent = m_pCurrent->pPrev;

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::MoveNext()
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
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::MoveIndex(const DWORD dwIndex)
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
template<class TYPE> TYPE* CBaseDoublyLinkedList<TYPE>::GetCurrent() const
{
	if( m_pCurrent == NULL )
	{
		return NULL;
	}

	return &m_pCurrent->Element;
}

//***************************************************************************
//	
template<class TYPE> TYPE CBaseDoublyLinkedList<TYPE>::At(const DWORD dwIndex) const
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
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::GetCount() const
{
	return m_dwCount;
}

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::Reset()
{
	OElement<TYPE>* pNext = NULL;
	OElement<TYPE>* pPrevElement = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		while( pNext != NULL )
		{
			pPrevElement = pNext->pNext;
			if( pNext )
			{
				delete pNext;
				pNext = NULL;
			}
			pNext = pPrevElement;
		}
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
template<class TYPE> void CBaseDoublyLinkedList<TYPE>::Printing(wostream& StreamBuffer)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		while( pNext != NULL )
		{
			StreamBuffer << pNext->Element;
			if( pNext->pNext != NULL ) StreamBuffer << " -> ";
			pNext = pNext->pNext;
		}
	}

	StreamBuffer << std::endl;
}
#else
//***************************************************************************
//
template<class TYPE> void CBaseDoublyLinkedList<TYPE>::Printing(ostream& StreamBuffer)
{
	OElement<TYPE>* pNext = NULL;

	if( m_pHead != NULL )
	{
		pNext = m_pHead;
		while( pNext != NULL )
		{
			StreamBuffer << pNext->Element;
			if( pNext->pNext != NULL ) StreamBuffer << " -> ";
			pNext = pNext->pNext;
		}
	}

	StreamBuffer << std::endl;
}
#endif

//***************************************************************************
//	
template<class TYPE> int CBaseDoublyLinkedList<TYPE>::Compare(const BYTE* pBCompee, const BYTE* pBComper, const DWORD dwSize) const
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

#endif // ndef __BASEDOUBLYLINKEDLIST_H__