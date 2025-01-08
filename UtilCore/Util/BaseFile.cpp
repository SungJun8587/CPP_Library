
//***************************************************************************
// BaseFile.cpp : implementation of the CBaseFile class.
//
//***************************************************************************

#include "pch.h"
#include "BaseFile.h"

#define INVALID_FILE_POINTER	(DWORD)0xFFFFFFFF	

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CBaseFile::CBaseFile()
{
	m_hFile = INVALID_HANDLE_VALUE;
}

CBaseFile::~CBaseFile()
{
	if( m_hFile != INVALID_HANDLE_VALUE ) CloseHandle(m_hFile);
}

//***************************************************************************
//
bool CBaseFile::Create(LPCTSTR lpFileName,			// pointer to name of the file
					   DWORD dwDesiredAccess,							// access (read-write) mode
					   DWORD dwShareMode,								// share mode
					   LPSECURITY_ATTRIBUTES lpSecurityAttributes,		// pointer to security attributes
					   DWORD dwCreationDisposition,					// how to create
					   DWORD dwFlagsAndAttributes,						// file attributes
					   HANDLE hTemplateFile							// handle to file with attributes to copy 
)
{
	m_hFile = CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
						 dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	if( m_hFile == INVALID_HANDLE_VALUE ) return false;
	return true;
}

//***************************************************************************
//
void CBaseFile::Close()
{
	if( m_hFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

//***************************************************************************
//
bool CBaseFile::FlushBuffers()
{
	if( m_hFile == INVALID_HANDLE_VALUE ) return false;
	return (FlushFileBuffers(m_hFile) ? true : false);
}

//***************************************************************************
//
bool CBaseFile::SetPosition(LONG lDistanceToMove,		// number of bytes to move file pointer
							PLONG lpDistanceToMoveHigh,							// pointer to high-order DWORD of // distance to move
							DWORD dwMoveMethod									// how to move
)
{
	if( m_hFile == INVALID_HANDLE_VALUE ) return false;

	DWORD dwPtr = SetFilePointer(m_hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);

	if( dwPtr == INVALID_FILE_POINTER ) return false;

	return true;
}

//***************************************************************************
//
DWORD CBaseFile::GetSize(LPDWORD lpFileSizeHigh)
{
	DWORD dwSize;

	if( m_hFile == INVALID_HANDLE_VALUE ) return HandleToULong(INVALID_HANDLE_VALUE);
	dwSize = GetFileSize(m_hFile, lpFileSizeHigh);

	return dwSize;
}

//***************************************************************************
//
DWORD CBaseFile::Read(LPVOID lpBuffer, DWORD nNumberOfBytesToRead)
{
	DWORD	dwRead;

	if( m_hFile == INVALID_HANDLE_VALUE ) return HandleToULong(INVALID_HANDLE_VALUE);
	if( !ReadFile(m_hFile, lpBuffer, nNumberOfBytesToRead, &dwRead, NULL) )
		dwRead = 0;			// Read Fail

	return dwRead;
}

//***************************************************************************
//
DWORD CBaseFile::Write(LPCVOID lpBuffer,        // pointer to data to write to file
					   DWORD nNumberOfBytesToWrite,				// number of bytes to write
					   LPDWORD lpNumberOfBytesWritten,				// pointer to number of bytes written
					   LPOVERLAPPED lpOverlapped					// pointer to structure for overlapped I/O
)
{
	DWORD 	dwCurrent = 0;
	DWORD	dwTotal = 0;

	const int nMaxWrite = 65536;
	const char* pBuf = (const char*)lpBuffer;

	if( m_hFile == INVALID_HANDLE_VALUE ) return HandleToULong(INVALID_HANDLE_VALUE);

	do
	{
		DWORD dwDoWrite = min(nMaxWrite, nNumberOfBytesToWrite);
		if( !WriteFile(m_hFile, pBuf, dwDoWrite, &dwCurrent, NULL) )
			return dwTotal;
		dwTotal += dwCurrent;
		pBuf += dwCurrent;
		nNumberOfBytesToWrite -= dwCurrent;

	} while( nNumberOfBytesToWrite > 0 );

	return dwTotal;
}

//***************************************************************************
//
bool CBaseFile::GetFileInformation(LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	if( m_hFile == INVALID_HANDLE_VALUE ) return false;

	return (GetFileInformationByHandle(m_hFile, lpFileInformation) ? true : false);
}

