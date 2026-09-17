
//***************************************************************************
// FileManager.h : interface for the CFileManager class.
//
//***************************************************************************

#ifndef	UC_FILEMANAGER_H
#define	UC_FILEMANAGER_H

#define FM_BINARY 0
#define FM_TEXT   1

typedef	struct _SM_FILE_INDEX 
{
	TCHAR	m_szfn[20];
	DWORD	m_dwOff;
	DWORD	m_dwEnd;
} SM_FILE_INDEX, *PSM_FILE_INDEX;

//***************************************************************************
//
class CFileManager 
{
public:
	CFileManager();
	~CFileManager();
	
	FILE*	Open( TCHAR *pszDirectory, TCHAR *pszFilename, int nType = FM_BINARY );
	void	Close( FILE *fp );
};

#endif // ndef UC_FILEMANAGER_H