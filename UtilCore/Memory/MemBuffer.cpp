
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
	pMemBuffer->m_pbBuffer = static_cast<unsigned char*>(BaseAllocator::Alloc(static_cast<int32>(pMemBuffer->m_nSize)));
	pMemBuffer->m_pbPosition = pMemBuffer->m_pbBuffer;
}

//***************************************************************************
// MemBufferGrow: Double the size of the buffer that was passed to this function. 
//
void MemBufferGrow(MEMORY_BYTE_BUFFER* pMemBuffer)
{
	size_t nSize = (size_t)(pMemBuffer->m_pbPosition - pMemBuffer->m_pbBuffer);
	size_t nNewSize = pMemBuffer->m_nSize * 2;

	// BaseAllocator는 realloc을 직접 지원하지 않으므로 새 크기로 할당 후 복사/해제
	unsigned char* pbNewBuffer = static_cast<unsigned char*>(BaseAllocator::Alloc(static_cast<int32>(nNewSize)));
	if( pbNewBuffer != nullptr )
	{
		if( pMemBuffer->m_pbBuffer != nullptr )
		{
			memcpy_s(pbNewBuffer, nNewSize, pMemBuffer->m_pbBuffer, nSize);
			BaseAllocator::Release(pMemBuffer->m_pbBuffer);
		}
		pMemBuffer->m_nSize = nNewSize;
		pMemBuffer->m_pbBuffer = pbNewBuffer;
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
	if( pMemBuffer->m_pbBuffer )
	{
		BaseAllocator::Release(pMemBuffer->m_pbBuffer);
	}
	pMemBuffer->m_pbBuffer = NULL;
	pMemBuffer->m_pbPosition = NULL;
}

//***************************************************************************
//MemBufferCreate: Passed a MemBuffer structure, will allocate a memory buffer of MEM_BUFFER_SIZE.  
//	This buffer can then grow as needed.
void MemBufferCreate(MEMORY_CHAR_BUFFER* pMemBuffer, size_t nSize)
{
	pMemBuffer->m_nSize = nSize;
	pMemBuffer->m_ptszBuffer = static_cast<TCHAR*>(BaseAllocator::Alloc(static_cast<int32>(pMemBuffer->m_nSize * sizeof(TCHAR))));
	pMemBuffer->m_ptszPosition = pMemBuffer->m_ptszBuffer;
}

//***************************************************************************
// MemBufferGrow: Double the size of the buffer that was passed to this function. 
//
void MemBufferGrow(MEMORY_CHAR_BUFFER* pMemBuffer)
{
	size_t nSize = (size_t)(pMemBuffer->m_ptszPosition - pMemBuffer->m_ptszBuffer);
	size_t nNewSize = pMemBuffer->m_nSize * 2;

	TCHAR* ptszNewBuffer = static_cast<TCHAR*>(BaseAllocator::Alloc(static_cast<int32>(nNewSize * sizeof(TCHAR))));
	if( ptszNewBuffer != nullptr )
	{
		if( pMemBuffer->m_ptszBuffer != nullptr )
		{
			memcpy_s(ptszNewBuffer, nNewSize * sizeof(TCHAR), pMemBuffer->m_ptszBuffer, nSize * sizeof(TCHAR));
			BaseAllocator::Release(pMemBuffer->m_ptszBuffer);
		}
		pMemBuffer->m_nSize = nNewSize;
		pMemBuffer->m_ptszBuffer = ptszNewBuffer;
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
	if( pMemBuffer->m_ptszBuffer )
	{
		BaseAllocator::Release(pMemBuffer->m_ptszBuffer);
	}
	pMemBuffer->m_ptszBuffer = NULL;
	pMemBuffer->m_ptszPosition = NULL;
}




