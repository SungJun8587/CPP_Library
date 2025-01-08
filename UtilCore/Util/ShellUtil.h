
//***************************************************************************
// ShellUtil.h : interface for the ShellUtil Functions.
//
//***************************************************************************

#ifndef __SHELLUTIL_H__
#define __SHELLUTIL_H__

#ifndef	_INC_WINDOWS
#include <windows.h>
#endif

#ifndef	_INC_TIME
#include <time.h>
#endif

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

#ifndef	__STRINGUTIL_H__
#include <Util/StringUtil.h>
#endif

//***************************************************************************
//
typedef struct _SH_APPLY_FILEINFO
{
	bool		m_bIsApply;
	TCHAR		m_tszApplyExt[MAX_BUFFER_SIZE];
	TCHAR		m_tszModifyStDate[DATE_STRLEN];
	TCHAR		m_tszModifyEdDate[DATE_STRLEN];
} SH_APPLY_FILEINFO, * PSH_APPLY_FILEINFO;

//***************************************************************************
//
typedef struct _SH_FILESYSTEM_INFO
{
	TCHAR		m_tszFullPath[FULLPATH_STRLEN];
	TCHAR		m_tszFolder[DIRECTORY_STRLEN];
	TCHAR		m_tszFileNameExt[FILENAMEEXT_STRLEN];

} SH_FILESYSTEM_INFO, * PSH_FILESYSTEM_INFO;

//***************************************************************************
//
typedef struct _SH_REGISTRY_INFO
{
	_SH_REGISTRY_INFO() {
		memset(m_tszFullPathKey, 0, sizeof(m_tszFullPathKey));
		memset(m_tszSubPathKey, 0, sizeof(m_tszSubPathKey));
		memset(m_tszName, 0, sizeof(m_tszName));
		m_pbValue = NULL;

		m_dwType = 0;
		m_dwNameLen = 0;
		m_dwValueLen = 0;
	}

	~_SH_REGISTRY_INFO() {
		if( m_pbValue )
		{
			delete[]m_pbValue;
			m_pbValue = NULL;
		}
	}

	TCHAR	m_tszFullPathKey[REGISTRY_KEY_STRLEN];
	TCHAR	m_tszSubPathKey[REGISTRY_KEY_STRLEN];
	TCHAR	m_tszName[REGISTRY_NAME_STRLEN];
	BYTE*	m_pbValue;

	DWORD	m_dwType;
	DWORD	m_dwNameLen;
	DWORD	m_dwValueLen;
} SH_REGISTRY_INFO, * PSH_REGISTRY_INFO;

bool	IsAbleFile(const TCHAR* ptszSourceFullPath, const SH_APPLY_FILEINFO ShApplyFileInfo);

bool	CreateDirectoryRecursive(const TCHAR* ptszFolder);
bool	RemoveDirectoryRecursive(const TCHAR* ptszFolder, const bool  bSelfDel = TRUE);
bool	CopyFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo);
bool	MoveFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo);
bool	IsDirectory(const TCHAR* ptszFolder);

long	RegCreateKeyExRecursive(const HKEY hRoot, const TCHAR* ptszSubKey, const bool  bReadOnly);
long	RegDeleteKeyRecursive(const HKEY hKey, const TCHAR* ptszSubKey);
bool	RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD dwType, const void* pvValue, const DWORD dwLength);
bool	RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const BYTE* pbValue, const DWORD dwLength);
bool	RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const DWORD dwValue);

DWORD	GetRegSzValueLen(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName);
bool	RegGetValue(void* pvValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD& dwType);
bool	RegGetValue(BYTE* pbValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName);
bool	RegGetValue(DWORD* pdwValue, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName);
bool	IsRegKey(const HKEY hKey, const TCHAR* ptszSubKey);

HANDLE	GetFileHandleDuplicate(TCHAR* ptszDestFullPath, TCHAR* ptszDestFileNameExt, const TCHAR* ptszFullPath);

bool	GetProductKeyExtract(CMemBuffer<TCHAR>& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const bool bIsExtractBytesRange);

#endif // ndef __SHELLUTIL_H__

