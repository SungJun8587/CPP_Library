
//***************************************************************************
// MemBuffer.h : interface for the Memory Buffer Alloc/Realloc Function.
//
//***************************************************************************

#ifndef __MEMBUFFER_H__
#define __MEMBUFFER_H__

#ifndef _TYPEINFO_
#include <typeinfo>
#endif

//***************************************************************************
//
typedef struct _MEMORY_BYTE_BUFFER
{
	BYTE* m_pbBuffer;		// 동적으로 메모리를 할당할 영역
	BYTE* m_pbPosition;		// 해당 영역의 포인터
	size_t	m_nSize;			// 할당된 메모리 크기	
} MEMORY_BYTE_BUFFER, * PMEMORY_BYTE_BUFFER;

void	MemBufferCreate(MEMORY_BYTE_BUFFER* pMemBuffer, size_t nSize = 10);
void	MemBufferGrow(MEMORY_BYTE_BUFFER* pMemBuffer);
void	MemBufferAddByte(MEMORY_BYTE_BUFFER* pMemBuffer, const BYTE bBuffer);
void	MemBufferAddBuffer(MEMORY_BYTE_BUFFER* pMemBuffer, const BYTE* pbBuffer, const size_t nSize);
void	MemBufferDestroy(MEMORY_BYTE_BUFFER* pMemBuffer);

//***************************************************************************
//
typedef struct _MEMORY_CHAR_BUFFER
{
	TCHAR* m_ptszBuffer;		// 동적으로 메모리를 할당할 영역
	TCHAR* m_ptszPosition;	// 해당 영역의 포인터
	size_t	m_nSize;			// 할당된 메모리 크기
} MEMORY_CHAR_BUFFER, * PMEMORY_CHAR_BUFFER;

void	MemBufferCreate(MEMORY_CHAR_BUFFER* pMemBuffer, size_t nSize = 10);
void	MemBufferGrow(MEMORY_CHAR_BUFFER* pMemBuffer);
void	MemBufferAddByte(MEMORY_CHAR_BUFFER* pMemBuffer, const TCHAR tcBuffer);
void	MemBufferAddBuffer(MEMORY_CHAR_BUFFER* pMemBuffer, const TCHAR* ptszBuffer, const size_t nSize);
void	MemBufferDestroy(MEMORY_CHAR_BUFFER* pMemBuffer);

//***************************************************************************
//
template<class TYPE> class CMemBuffer
{
public:
	CMemBuffer()
	{
		m_pBuffer = NULL;
		m_nBufSize = 0;
		m_nBufLength = 0;
	}

	~CMemBuffer()
	{
		Destroy();
	}

	void Init(size_t nBufSize)
	{
		const char* pszTypeName = typeid(TYPE).name();

		if( strcmp(pszTypeName, "wchar_t") == 0 )
			m_nBufSize = sizeof(wchar_t) * nBufSize - 1;
		else m_nBufSize = nBufSize;

		if( m_nBufSize < nBufSize ) Destroy();

#ifdef TCMALLOC_TCMALLOC_H_
		m_pBuffer = (TYPE*)tc_new(m_nBufSize);
#else
		m_pBuffer = new TYPE[m_nBufSize];
#endif

		memset(m_pBuffer, 0, m_nBufSize);
		m_nBufLength = nBufSize;
	}

	void Destroy()
	{
		if( m_pBuffer )
		{
#ifdef TCMALLOC_TCMALLOC_H_
			tc_delete(m_pBuffer);
#else
			delete[]m_pBuffer;
#endif
			m_pBuffer = NULL;
			m_nBufSize = 0;
			m_nBufLength = 0;
		}
	}

	// 버퍼 포인터
	TYPE* GetBuffer() const
	{
		return m_pBuffer;
	}

	// 버퍼에 할당된 바이트 수 : 바이트 수 + 1('\0')
	size_t GetBufSize() const
	{
		return m_nBufSize;
	}

	// 버퍼에 저장된 문자 수 : 문자 수 + 1('\0')
	size_t GetBufLength() const
	{
		return m_nBufLength;
	}

public:
	TYPE* m_pBuffer;		// 버퍼 포인터
	size_t	m_nBufSize;		// 버퍼에 할당된 바이트 수 : 바이트 수 + 1('\0')
	size_t  m_nBufLength;	// 버퍼에 저장된 문자 수 : 문자 수 + 1('\0')
};

#endif // ndef __MEMBUFFER_H__
