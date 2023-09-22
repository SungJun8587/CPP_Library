
//***************************************************************************
// BaseFile.h : interface for the CBaseFile class.
//
//***************************************************************************

#ifndef __BASEFILE_H__
#define __BASEFILE_H__

//***************************************************************************
// Synchoronus File Processing
//***************************************************************************

class CBaseFile
{
public:
	CBaseFile();
	~CBaseFile();

	bool	Create(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
	void	Close(void);

	bool	SetPosition(LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
	bool	GetFileInformation(LPBY_HANDLE_FILE_INFORMATION lpFileInformation);
	bool	FlushBuffers(void);

	DWORD	GetSize(LPDWORD lpFileSizeHigh);
	DWORD	Read(LPVOID lpBuffer, DWORD nNumberOfBytesToRead);
	DWORD	Write(LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

protected:
	HANDLE		m_hFile;
};

#endif // ndef __BASEFILE_H__