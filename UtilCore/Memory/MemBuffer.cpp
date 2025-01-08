
//***************************************************************************
// MemBuffer.cpp : implementation of the Memory Buffer Alloc/Realloc Function.
//
//***************************************************************************

#include "pch.h"
#include "MemBuffer.h"

//***************************************************************************
//MemBufferCreate: Passed a MemBuffer structure, will allocate a memory buffer of MEM_BUFFER_SIZE.  
//	This buffer can then grow as needed.
//
void MemBufferCreate(MEMORY_BYTE_BUFFER* pMemBuffer, size_t nSize)
{
	pMemBuffer->m_nSize = nSize;

#ifdef TCMALLOC_TCMALLOC_H_
	pMemBuffer->m_pbBuffer = (unsigned char*)tc_malloc(pMemBuffer->m_nSize);
#else
	pMemBuffer->m_pbBuffer = (unsigned char*)malloc(pMemBuffer->m_nSize);
#endif

	pMemBuffer->m_pbPosition = pMemBuffer->m_pbBuffer;
}

//***************************************************************************
// MemBufferGrow: Double the size of the buffer that was passed to this function. 
//
void MemBufferGrow(MEMORY_BYTE_BUFFER* pMemBuffer)
{
	BYTE* pbBuffer;
	size_t	nSize;

	nSize = (size_t)(pMemBuffer->m_pbPosition - pMemBuffer->m_pbBuffer);
	pMemBuffer->m_nSize = pMemBuffer->m_nSize * 2;

#ifdef TCMALLOC_TCMALLOC_H_
	pbBuffer = (unsigned char*)tc_realloc(pMemBuffer->m_pbBuffer, pMemBuffer->m_nSize);
#else
	pbBuffer = (unsigned char*)realloc(pMemBuffer->m_pbBuffer, pMemBuffer->m_nSize);
#endif

	if( pbBuffer )
	{
		pMemBuffer->m_pbBuffer = pbBuffer;
		pMemBuffer->m_pbPosition = pMemBuffer->m_pbBuffer + nSize;
	}
}

//***************************************************************************
// MemBufferAddByte: Add a single byte to the memory buffer, grow if needed.
//
void MemBufferAddByte(MEMORY_BYTE_BUFFER* pMemBuffer, const BYTE bBuffer)
{
	if( (size_t)(pMemBuffer->m_pbPosition - pMemBuffer->m_pbBuffer) >= pMemBuffer->m_nSize )
		MemBufferGrow(pMemBuffer);

	*(pMemBuffer->m_pbPosition++) = bBuffer;
}

//***************************************************************************
// MemBufferAddBuffer: Add a range of bytes to the memory buffer, grow if needed.
//
void MemBufferAddBuffer(MEMORY_BYTE_BUFFER* pMemBuffer, const BYTE* pbBuffer, const size_t nSize)
{
	while( ((pMemBuffer->m_pbPosition - pMemBuffer->m_pbBuffer) + nSize) >= pMemBuffer->m_nSize )
		MemBufferGrow(pMemBuffer);

	memcpy_s(pMemBuffer->m_pbPosition, pMemBuffer->m_nSize, pbBuffer, nSize);
	pMemBuffer->m_pbPosition += nSize;
}

//***************************************************************************
// MemBufferDestroy: Passed a MemBuffer structure, will free a memory buffer 
//
void MemBufferDestroy(MEMORY_BYTE_BUFFER* pMemBuffer)
{
	pMemBuffer->m_nSize = 0;

#ifdef TCMALLOC_TCMALLOC_H_
	if( pMemBuffer->m_pbBuffer ) tc_free(pMemBuffer->m_pbBuffer);
#else
	if( pMemBuffer->m_pbBuffer ) free(pMemBuffer->m_pbBuffer);
#endif

	pMemBuffer->m_pbBuffer = NULL;
	pMemBuffer->m_pbPosition = NULL;
}

//***************************************************************************
//MemBufferCreate: Passed a MemBuffer structure, will allocate a memory buffer of MEM_BUFFER_SIZE.  
//	This buffer can then grow as needed.
void MemBufferCreate(MEMORY_CHAR_BUFFER* pMemBuffer, size_t nSize)
{
	pMemBuffer->m_nSize = nSize;

#ifdef TCMALLOC_TCMALLOC_H_
	pMemBuffer->m_ptszBuffer = (TCHAR*)tc_malloc(pMemBuffer->m_nSize * sizeof(TCHAR));
#else
	pMemBuffer->m_ptszBuffer = (TCHAR*)malloc(pMemBuffer->m_nSize * sizeof(TCHAR));
#endif

	pMemBuffer->m_ptszPosition = pMemBuffer->m_ptszBuffer;
}

//***************************************************************************
// MemBufferGrow: Double the size of the buffer that was passed to this function. 
//
void MemBufferGrow(MEMORY_CHAR_BUFFER* pMemBuffer)
{
	TCHAR* ptszBuffer;
	size_t	nSize;

	nSize = (size_t)(pMemBuffer->m_ptszPosition - pMemBuffer->m_ptszBuffer);
	pMemBuffer->m_nSize = pMemBuffer->m_nSize * 2;

#ifdef TCMALLOC_TCMALLOC_H_
	ptszBuffer = (TCHAR*)tc_realloc(pMemBuffer->m_ptszBuffer, pMemBuffer->m_nSize * sizeof(TCHAR));
#else
	ptszBuffer = (TCHAR*)realloc(pMemBuffer->m_ptszBuffer, pMemBuffer->m_nSize * sizeof(TCHAR));
#endif

	if( ptszBuffer )
	{
		pMemBuffer->m_ptszBuffer = ptszBuffer;
		pMemBuffer->m_ptszPosition = pMemBuffer->m_ptszBuffer + nSize;
	}
}

//***************************************************************************
// MemBufferAddByte: Add a single byte to the memory buffer, grow if needed.
//
void MemBufferAddByte(MEMORY_CHAR_BUFFER* pMemBuffer, const TCHAR tcBuffer)
{
	if( (size_t)(pMemBuffer->m_ptszPosition - pMemBuffer->m_ptszBuffer) >= pMemBuffer->m_nSize )
		MemBufferGrow(pMemBuffer);

	*(pMemBuffer->m_ptszPosition++) = tcBuffer;
}

//***************************************************************************
// MemBufferAddBuffer: Add a range of bytes to the memory buffer, grow if needed.
//
void MemBufferAddBuffer(MEMORY_CHAR_BUFFER* pMemBuffer, const TCHAR* ptszBuffer, const size_t nSize)
{
	while( ((pMemBuffer->m_ptszPosition - pMemBuffer->m_ptszBuffer) + nSize) >= pMemBuffer->m_nSize )
		MemBufferGrow(pMemBuffer);

	_tcsncpy_s(pMemBuffer->m_ptszPosition, pMemBuffer->m_nSize, ptszBuffer, nSize);
	pMemBuffer->m_ptszPosition += nSize;
}

//***************************************************************************
// MemBufferDestroy: Passed a MemBuffer structure, will free a memory buffer 
//
void MemBufferDestroy(MEMORY_CHAR_BUFFER* pMemBuffer)
{
	pMemBuffer->m_nSize = 0;

#ifdef TCMALLOC_TCMALLOC_H_
	if( pMemBuffer->m_ptszBuffer ) tc_free(pMemBuffer->m_ptszBuffer);
#else
	if( pMemBuffer->m_ptszBuffer ) free(pMemBuffer->m_ptszBuffer);
#endif

	pMemBuffer->m_ptszBuffer = NULL;
	pMemBuffer->m_ptszPosition = NULL;
}




