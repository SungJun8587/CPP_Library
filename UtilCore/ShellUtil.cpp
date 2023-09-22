
//***************************************************************************
// ShellUtil.cpp : implementation of the ShellUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "ShellUtil.h"

//***************************************************************************
//
BOOL IsAbleFile(const TCHAR* ptszSourceFullPath, const SH_APPLY_FILEINFO ShApplyFileInfo)
{
	size_t		nTokenCount = 0;
	size_t		nIndex = 0;
	TCHAR		tszFileNameExt[FILENAMEEXT_STRLEN];
	TCHAR		tszFileName[FILENAME_STRLEN];
	TCHAR		tszFileExt[FILEEXT_STRLEN];
	TCHAR		tszTime[FILEATT_DATETIME_STRLEN];
	TCHAR* ptszApplyExt = NULL;

	CMemBuffer<TCHAR>* pTDestination = NULL;

	SYSTEMTIME	stLocal;

	if( !ptszSourceFullPath ) return false;
	if( _tcslen(ptszSourceFullPath) < 1 ) return false;

	if( _tcslen(ShApplyFileInfo.m_tszModifyStDate) > 0 || _tcslen(ShApplyFileInfo.m_tszModifyEdDate) > 0 )
	{
		if( !GetFileInfoTime(ptszSourceFullPath, FILEINFO_LASTWRITETIME, stLocal) ) return false;

		_sntprintf_s(tszTime, _countof(tszTime), _TRUNCATE, _T("%04d-%02d-%02d"), stLocal.wYear, stLocal.wMonth, stLocal.wDay);

		if( _tcscmp(tszTime, ShApplyFileInfo.m_tszModifyStDate) < 0 || _tcscmp(tszTime, ShApplyFileInfo.m_tszModifyEdDate) > 0 ) return false;
	}

	if( ShApplyFileInfo.m_tszApplyExt && _tcslen(ShApplyFileInfo.m_tszApplyExt) > 0 && _tcscmp(ShApplyFileInfo.m_tszApplyExt, _T("*.*")) != 0 )
	{
		if( !FileNameExtPathPassing(tszFileNameExt, ptszSourceFullPath) ) return false;
		if( !FileNameExtPassing(tszFileName, tszFileExt, tszFileNameExt) ) return false;

		nTokenCount = TokenCount(ShApplyFileInfo.m_tszApplyExt, _T(";"));

		if( nTokenCount > 0 )
		{
			pTDestination = new CMemBuffer<TCHAR>[nTokenCount];

			Tokenize(pTDestination, ShApplyFileInfo.m_tszApplyExt, _T(";"));

			for( nIndex = 0; nIndex < nTokenCount; nIndex++ )
			{
				ptszApplyExt = pTDestination[nIndex].GetBuffer() + 1;
				if( _tcscmp(ptszApplyExt, tszFileExt) == 0 ) break;
			}

			if( pTDestination )
			{
				delete[]pTDestination;
				pTDestination = NULL;
			}

			if( ShApplyFileInfo.m_bIsApply )
			{
				if( nIndex >= nTokenCount ) return false;
				else return true;
			}
			else
			{
				if( nIndex >= nTokenCount ) return true;
				else return false;
			}
		}
	}

	return true;
}

//***************************************************************************
//
BOOL CreateDirectoryRecursive(const TCHAR* ptszFolder)
{
	size_t		nLen = 0;
	size_t		nCount = 0;
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR* ptszSourceLoc = NULL;

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( !ptszFolder ) return false;
	if( (nLen = _tcslen(ptszFolder)) < 1 ) return false;

	for( ptszSourceLoc = (TCHAR*)(ptszFolder + nLen - 1); nCount < nLen; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == '/' || *ptszSourceLoc == '\\' )
		{
			nCount++;
			break;
		}
		nCount++;
	}

	_tcsncpy_s(tszSourceFolder, _countof(tszSourceFolder), ptszFolder, nLen - nCount);
	_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s\\*.*"), tszSourceFolder);

	if( (hFindFile = FindFirstFile(tszActiveFolder, &FindData)) == INVALID_HANDLE_VALUE )
	{
		if( !CreateDirectoryRecursive(tszSourceFolder) )
		{
			FindClose(hFindFile);
			return false;
		}
	}

	FindClose(hFindFile);

	return CreateDirectory(ptszFolder, NULL);
}

//***************************************************************************
//
BOOL RemoveDirectoryRecursive(const TCHAR* ptszFolder, const BOOL bSelfDel)
{
	static TCHAR	tszSelfSourceFolder[DIRECTORY_STRLEN] = { 0, };

	BOOL		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( !ptszFolder ) return false;
	if( _tcslen(ptszFolder) < 1 ) return false;

	if( ptszFolder[_tcslen(ptszFolder) - 1] != '/' && ptszFolder[_tcslen(ptszFolder) - 1] != '\\' )
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s\\"), ptszFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s\\*.*"), ptszFolder);
	}
	else
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s"), ptszFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s*.*"), ptszFolder);
	}

	if( _tcslen(tszSelfSourceFolder) < 1 ) _tcsncpy_s(tszSelfSourceFolder, _countof(tszSelfSourceFolder), ptszFolder, _TRUNCATE);

	hFindFile = FindFirstFile(tszActiveFolder, &FindData);

	// Check if sub folders exists.
	if( INVALID_HANDLE_VALUE != hFindFile )
	{	// There are sub-folders.
		while( bResult )
		{
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if( _tcscmp(FindData.cFileName, _T(".")) != 0 && _tcscmp(FindData.cFileName, _T("..")) != 0 )
				{
					_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s\\"), tszSourceFolder, FindData.cFileName);
					if( !RemoveDirectoryRecursive(tszActiveFullPath) )
					{
						FindClose(hFindFile);
						return false;
					}
				}
			}
			else
			{
				_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s"), tszSourceFolder, FindData.cFileName);
				DeleteFile(tszActiveFullPath);
			}

			bResult = FindNextFile(hFindFile, &FindData);
		}
	}

	FindClose(hFindFile);

	if( !bSelfDel && _tcscmp(tszSelfSourceFolder, ptszFolder) == 0 )
	{
		tszSelfSourceFolder[0] = '\0';
		return true;
	}

	return RemoveDirectory(ptszFolder);
}

//***************************************************************************
//
BOOL CopyFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo)
{
	BOOL		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR		tszDestFullPath[FULLPATH_STRLEN];
	TCHAR		tszDestFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( !ptszSourceFolder || !ptszDestFolder ) return false;
	if( _tcslen(ptszSourceFolder) < 1 || _tcslen(ptszDestFolder) < 1 ) return false;

	if( ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '/' && ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '\\' )
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s\\"), ptszSourceFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s\\*.*"), ptszSourceFolder);
	}
	else
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s"), ptszSourceFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s*.*"), ptszSourceFolder);
	}

	if( ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '/' && ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '\\' )
		_sntprintf_s(tszDestFolder, _countof(tszDestFolder), _TRUNCATE, _T("%s\\"), ptszDestFolder);
	else
		_sntprintf_s(tszDestFolder, _countof(tszDestFolder), _TRUNCATE, _T("%s"), ptszDestFolder);

	hFindFile = FindFirstFile(tszActiveFolder, &FindData);

	// Check if sub folders exists.
	if( INVALID_HANDLE_VALUE != hFindFile )
	{	// There are sub-folders.
		while( bResult )
		{
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if( _tcscmp(FindData.cFileName, _T(".")) != 0 && _tcscmp(FindData.cFileName, _T("..")) != 0 )
				{
					_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s"), tszSourceFolder, FindData.cFileName);
					_sntprintf_s(tszDestFullPath, _countof(tszDestFullPath), _TRUNCATE, _T("%s%s"), tszDestFolder, FindData.cFileName);

					CreateDirectory(tszDestFullPath, NULL);
					CopyFileRecursive(tszActiveFullPath, tszDestFullPath, ShApplyFileInfo);
				}
			}
			else
			{
				_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s"), tszSourceFolder, FindData.cFileName);
				_sntprintf_s(tszDestFullPath, _countof(tszDestFullPath), _TRUNCATE, _T("%s%s"), tszDestFolder, FindData.cFileName);

				if( IsAbleFile(tszActiveFullPath, ShApplyFileInfo) )
					CopyFile(tszActiveFullPath, tszDestFullPath, FALSE);
			}

			bResult = FindNextFile(hFindFile, &FindData);
		}
	}

	FindClose(hFindFile);
	bResult = true;

	return bResult;
}

//***************************************************************************
//
BOOL MoveFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo)
{
	BOOL		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR		tszDestFullPath[FULLPATH_STRLEN];
	TCHAR		tszDestFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( !ptszSourceFolder || !ptszDestFolder ) return false;
	if( _tcslen(ptszSourceFolder) < 1 || _tcslen(ptszDestFolder) < 1 ) return false;

	if( ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '/' && ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '\\' )
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s\\"), ptszSourceFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s\\*.*"), ptszSourceFolder);
	}
	else
	{
		_sntprintf_s(tszSourceFolder, _countof(tszSourceFolder), _TRUNCATE, _T("%s"), ptszSourceFolder);
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s*.*"), ptszSourceFolder);
	}

	if( ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '/' && ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '\\' )
		_sntprintf_s(tszDestFolder, _countof(tszDestFolder), _TRUNCATE, _T("%s\\"), ptszDestFolder);
	else
		_sntprintf_s(tszDestFolder, _countof(tszDestFolder), _TRUNCATE, _T("%s"), ptszDestFolder);

	hFindFile = FindFirstFile(tszActiveFolder, &FindData);

	// Check if sub folders exists.
	if( INVALID_HANDLE_VALUE != hFindFile )
	{	// There are sub-folders.
		while( bResult )
		{
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if( _tcscmp(FindData.cFileName, _T(".")) != 0 && _tcscmp(FindData.cFileName, _T("..")) != 0 )
				{
					_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s"), tszSourceFolder, FindData.cFileName);
					_sntprintf_s(tszDestFullPath, _countof(tszDestFullPath), _TRUNCATE, _T("%s%s"), tszDestFolder, FindData.cFileName);

					CreateDirectory(tszDestFullPath, NULL);
					CopyFileRecursive(tszActiveFullPath, tszDestFullPath, ShApplyFileInfo);
				}
			}
			else
			{
				_sntprintf_s(tszActiveFullPath, _countof(tszActiveFullPath), _TRUNCATE, _T("%s%s"), tszSourceFolder, FindData.cFileName);
				_sntprintf_s(tszDestFullPath, _countof(tszDestFullPath), _TRUNCATE, _T("%s%s"), tszDestFolder, FindData.cFileName);

				if( IsAbleFile(tszActiveFullPath, ShApplyFileInfo) )
					MoveFile(tszActiveFullPath, tszDestFullPath);
			}

			bResult = FindNextFile(hFindFile, &FindData);
		}
	}

	FindClose(hFindFile);
	bResult = true;

	return bResult;
}

//***************************************************************************
//
BOOL IsDirectory(const TCHAR* ptszFolder)
{
	BOOL		bResult = true;
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( !ptszFolder ) return false;
	if( _tcslen(ptszFolder) < 1 ) return false;

	if( ptszFolder[_tcslen(ptszFolder) - 1] != '/' && ptszFolder[_tcslen(ptszFolder) - 1] != '\\' )
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s\\*.*"), ptszFolder);
	else
		_sntprintf_s(tszActiveFolder, _countof(tszActiveFolder), _TRUNCATE, _T("%s*.*"), ptszFolder);

	hFindFile = FindFirstFile(tszActiveFolder, &FindData);

	// Check if sub folders exists.
	if( INVALID_HANDLE_VALUE == hFindFile )
		bResult = false;
	else bResult = true;

	FindClose(hFindFile);

	return bResult;
}

//***************************************************************************
//
long RegCreateKeyExRecursive(const HKEY hRoot, const TCHAR* ptszSubKey, const bool bReadOnly)
{
	int			nLen = 0;
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;
	TCHAR* ptszActiveSubKey = NULL;

	HKEY		hKey;
	REGSAM		samDesired = bReadOnly ? KEY_QUERY_VALUE | KEY_READ : KEY_ALL_ACCESS;

	lRetCode = RegOpenKeyEx(hRoot, ptszSubKey, 0, samDesired, &hKey);
	if( lRetCode == ERROR_SUCCESS ) return lRetCode;

	nLen = (int)_tcslen(ptszSubKey);
	for( int i = nLen - 1; i >= 0; i-- )
	{
		if( ptszSubKey[i] == '/' || ptszSubKey[i] == '\\' )
		{
			ptszActiveSubKey = new TCHAR[i + 1];
			_tcsncpy_s(ptszActiveSubKey, i, ptszSubKey, _TRUNCATE);

			lRetCode = RegCreateKeyExRecursive(hRoot, ptszActiveSubKey, bReadOnly);
			if( lRetCode != ERROR_SUCCESS )
			{
				delete[]ptszActiveSubKey;
				ptszActiveSubKey = NULL;

				RegCloseKey(hKey);

				return lRetCode;
			}

			delete[]ptszActiveSubKey;
			ptszActiveSubKey = NULL;
		}
	}

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
							  NULL, &hKey, &dwDisposition);

	RegCloseKey(hKey);

	return lRetCode;
}

//***************************************************************************
//
long RegDeleteKeyRecursive(const HKEY hKey, const TCHAR* ptszSubKey)
{
	long	lRetCode = 0;
	DWORD	dwSize = 0;
	TCHAR	szNewSubKey[REGISTRY_KEY_STRLEN];

	HKEY	newKey;

	FILETIME	FileTime;

	lRetCode = RegOpenKeyEx(hKey, ptszSubKey, 0, KEY_ALL_ACCESS, &newKey);
	if( lRetCode != ERROR_SUCCESS ) return lRetCode;

	while( 1 )
	{
		dwSize = REGISTRY_KEY_STRLEN;
		lRetCode = RegEnumKeyEx(newKey, 0, szNewSubKey, &dwSize, NULL, NULL, NULL, &FileTime);
		if( lRetCode != ERROR_SUCCESS ) break;

		lRetCode = RegDeleteKeyRecursive(newKey, szNewSubKey);
		if( lRetCode != ERROR_SUCCESS ) break;
	}

	RegCloseKey(newKey);

	return RegDeleteKey(hKey, ptszSubKey);
}

//***************************************************************************
//
BOOL RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD dwType, const void* pvValue, DWORD dwLength)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegOpenKeyEx(hRoot, ptszSubKey, dwOptions, samDesired, &hKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegSetValueEx(hKey, ptszName, 0, dwType, (BYTE*)pvValue, dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
BOOL RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const BYTE* pbValue, DWORD dwLength)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegSetValueEx(hKey, ptszName, 0, REG_SZ, pbValue, dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
BOOL RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const DWORD dwValue)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegSetValueEx(hKey, ptszName, 0, REG_DWORD, (CONST BYTE*) & dwValue, sizeof(DWORD));
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
DWORD GetRegSzValueLen(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName)
{
	long		lRetCode = 0;
	DWORD		dwLength = 0;
	DWORD		dwType = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return 0;

	lRetCode = RegQueryValueEx(hKey, ptszName, NULL, &dwType, NULL, &dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return 0;
	}

	return dwLength;
}

//***************************************************************************
//
BOOL RegGetValue(void* pvValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD& dwType)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegOpenKeyEx(hRoot, ptszSubKey, dwOptions, samDesired, &hKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegQueryValueEx(hKey, ptszName, NULL, &dwType, (BYTE*)pvValue, &dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
BOOL RegGetValue(BYTE* pbValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName)
{
	long		lRetCode = 0;
	DWORD		dwType = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegQueryValueEx(hKey, ptszName, NULL, &dwType, pbValue, &dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
BOOL RegGetValue(DWORD* pdwValue, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName)
{
	long		lRetCode = 0;
	DWORD		dwType = 0;
	DWORD		dwDisposition = 0;
	DWORD		dwSize = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	dwSize = sizeof(DWORD);
	lRetCode = RegQueryValueEx(hKey, ptszName, NULL, &dwType, (BYTE*)pdwValue, &dwSize);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);

		return false;
	}

	RegCloseKey(hKey);

	return true;
}

//***************************************************************************
//
BOOL IsRegKey(const HKEY hKey, const TCHAR* ptszSubKey)
{
	long	lRetCode = 0;

	HKEY	newKey;

	lRetCode = RegOpenKeyEx(hKey, ptszSubKey, 0, KEY_READ, &newKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	RegCloseKey(newKey);

	return true;
}

//***************************************************************************
//
HANDLE GetFileHandleDuplicate(TCHAR* ptszDestFullPath, TCHAR* ptszDestFileNameExt, const TCHAR* ptszFullPath)
{
	int		i = 0;
	TCHAR	tszFolderPath[FULLPATH_STRLEN];
	TCHAR	tszFileNameExt[FILENAMEEXT_STRLEN];
	TCHAR	tszFileName[FILENAME_STRLEN];
	TCHAR	tszFileExt[FILEEXT_STRLEN];
	TCHAR	tszTempFullPath[DIRECTORY_STRLEN + FILENAME_STRLEN];
	TCHAR   tszTempFileNameExt[FILENAMEEXT_STRLEN];

	HANDLE	hFile;

	if( !FolderPathPassing(tszFolderPath, ptszFullPath) ) return NULL;
	if( !FileNameExtPathPassing(tszFileNameExt, ptszFullPath) ) return NULL;
	if( !FileNameExtPassing(tszFileName, tszFileExt, tszFileNameExt) ) return NULL;

	CreateDirectoryRecursive(tszFolderPath);

	_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), tszFolderPath, tszFileNameExt);
	_sntprintf_s(tszTempFileNameExt, _countof(tszTempFileNameExt), _TRUNCATE, _T("%s.%s"), tszFileName, tszFileExt);

	hFile = CreateFile(tszTempFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		tszTempFullPath[0] = '\0';
		tszTempFileNameExt[0] = '\0';
		for( i = 1; i < MAX_FILENAME_CONVERT_INDEX_NUM; i++ )
		{
			_sntprintf_s(tszTempFileNameExt, _countof(tszTempFileNameExt), _TRUNCATE, _T("%s(%d).%s"), tszFileName, i, tszFileExt);
			_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), tszFolderPath, tszTempFileNameExt);
			hFile = CreateFile(tszTempFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);

			if( hFile != INVALID_HANDLE_VALUE ) break;

			CloseHandle(hFile);
		}

		if( i == MAX_FILENAME_CONVERT_INDEX_NUM ) return NULL;
	}

	if( hFile == INVALID_HANDLE_VALUE ) return NULL;

	_tcsncpy_s(ptszDestFullPath, FULLPATH_STRLEN, tszTempFullPath, _TRUNCATE);
	_tcsncpy_s(ptszDestFileNameExt, FILENAMEEXT_STRLEN, tszTempFileNameExt, _TRUNCATE);

	return hFile;
}

int IsFileType(const TCHAR* ptszFullPath)
{
	BOOL	bReturn = false;
	DWORD	dwReadSize = 0;
	int		nIsType = 0;
	WORD	wWord1, wWord2;
	BYTE    bByte;
	wchar_t wszBuffer[3];

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return 0;
	}

	bReturn = ReadFile(hFile, wszBuffer, sizeof(WORD) * 2, &dwReadSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);

		return 0;
	}
	wszBuffer[2] = '\0';

	CloseHandle(hFile);

	wWord1 = wszBuffer[0];
	wWord2 = wszBuffer[1];
	if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD || wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD )
	{
		if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD )
			nIsType = 1;		// UNICODE 
		else if( wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD )
			nIsType = 2;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		bByte = ((BYTE)(wWord2 & 0xff));
		if( wWord1 == (WORD)UTF_FILE_IDENTIFIER_WORD && bByte == (BYTE)UTF_FILE_IDENTIFIER_BYTE )
			nIsType = 3;		// UTF-8
		else
			nIsType = 4;		// ANSI
	}

	return nIsType;
}

//***************************************************************************
//
BOOL ReadFile(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath)
{
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	BYTE* pbBuffer = NULL;

	HANDLE	hFile;

	dwLength = GetFileSize(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	if( dwLength > dwMaxReadSize )
		dwReadNumSize = dwMaxReadSize;
	else dwReadNumSize = dwLength;

	ByteDestination.Init(dwLength);

	pbBuffer = ByteDestination.GetBuffer();
	while( 1 )
	{
		BOOL bReturn = ReadFile(hFile, pbBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
		if( bReturn == FALSE )
		{
			CloseHandle(hFile);

			return false;
		}

		if( dwMaxReadSize > dwReadSize ) break;

		dwReadOffset = dwReadOffset + dwReadNumSize;
		lReadSize = dwLength - dwReadOffset - dwMaxReadSize;
		if( lReadSize < 0 )
			dwReadNumSize = dwLength - dwReadOffset;
		else dwReadNumSize = dwMaxReadSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL ReadFile(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath)
{
	BOOL	bReturn = false;
	int		i = 0;
	int		nIsType = 0;
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	WORD	wWord1, wWord2;
	BYTE    bByte;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = NULL;
	wchar_t wszBuffer[3];
	wchar_t* pwszBuffer = NULL;

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;
	CMemBuffer<wchar_t>	WStrBuffer;

	if( !ptszFullPath ) return false;
	if( _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	dwMaxReadSize = dwLength;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	bReturn = ReadFile(hFile, wszBuffer, sizeof(WORD) * 2, &dwReadSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);

		return false;
	}
	wszBuffer[2] = '\0';

	wWord1 = wszBuffer[0];
	wWord2 = wszBuffer[1];
	if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD || wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD )
	{
		if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD ) nIsType = 1;
		else if( wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD ) nIsType = 2;

		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.Init(dwLength + 1);

		pwszBuffer = WStrBuffer.GetBuffer();

		pwszBuffer[0] = wWord2;

		dwReadOffset = dwReadOffset + 1;

		dwTotFileSize = dwLength + 1;

		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			BOOL bReturn = ReadFile(hFile, pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);

				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		CloseHandle(hFile);
	}
	else
	{
		bByte = ((BYTE)(wWord2 & 0xff));
		if( wWord1 == (WORD)UTF_FILE_IDENTIFIER_WORD && bByte == (BYTE)UTF_FILE_IDENTIFIER_BYTE )
		{
			nIsType = 3;

			dwLength = dwLength - sizeof(WORD) - sizeof(BYTE);

			StrBuffer.Init(dwLength + 1);

			pszBuffer = StrBuffer.GetBuffer();

			pszBuffer[0] = ((BYTE)(wWord2 >> 8));

			dwReadOffset = dwReadOffset + 1;
		}
		else
		{
			nIsType = 4;

			StrBuffer.Init(dwLength + 1);

			pszBuffer = StrBuffer.GetBuffer();

			pszBuffer[0] = ((BYTE)(wWord1 & 0xff));
			pszBuffer[1] = ((BYTE)(wWord1 >> 8));
			pszBuffer[2] = ((BYTE)(wWord2 & 0xff));
			pszBuffer[3] = ((BYTE)(wWord2 >> 8));

			dwReadOffset = dwReadOffset + 4;
		}

		dwTotFileSize = dwLength + 1;

		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			BOOL bReturn = ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
			if( bReturn == FALSE )
			{
				CloseHandle(hFile);

				return false;
			}

			if( dwMaxReadSize > dwReadSize ) break;

			dwReadOffset = dwReadOffset + dwReadNumSize;
			lReadSize = dwTotFileSize - dwReadOffset - dwMaxReadSize;
			if( lReadSize < 0 )
				dwReadNumSize = dwTotFileSize - dwReadOffset;
			else dwReadNumSize = dwMaxReadSize;
		}

		*(pszBuffer + dwReadNumSize - 1) = '\0';

		CloseHandle(hFile);
	}

#ifdef _UNICODE
	if( nIsType == 1 )
	{
		TDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( nIsType == 2 )
	{
		for( int i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		TDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( nIsType == 3 )
	{
		if( !UnicodeToUTF8(TDestination, pszBuffer) ) return false;
	}
	else
	{
		if( !MultiByteToWideCharStr(TDestination, pszBuffer) ) return false;
	}
#else
	if( nIsType == 1 )
	{
		if( !WideCharToMultiByteStr(TDestination, pwszBuffer) ) return false;
	}
	else if( nIsType == 2 )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( !WideCharToMultiByteStr(TDestination, pwszBuffer) ) return false;
	}
	else if( nIsType == 3 )
	{
		if( !AnsiToUTF8(TDestination, pszBuffer) ) return false;
	}
	else
	{
		TDestination.Init(strlen(pszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszBuffer, _TRUNCATE);
	}
#endif

	return true;
}

//***************************************************************************
//
BOOL ReadFileMap(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, NULL, PAGE_WRITECOPY, 0, dwLength, NULL);
	if( hFileMap == NULL )
	{
		CloseHandle(hFile);

		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == NULL )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);

		return false;
	}

	ByteDestination.Init(dwLength);

	memcpy(ByteDestination.GetBuffer(), lpvFile, dwLength);

	UnmapViewOfFile(lpvFile);
	lpvFile = NULL;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return true;
}

//***************************************************************************
//
BOOL ReadFileMap(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath)
{
	BOOL	bIsProcess = false;
	DWORD	dwLength = 0;
	int		i = 0;
	int		nFileType = 0;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	nFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, NULL, PAGE_WRITECOPY, 0, dwLength, NULL);
	if( hFileMap == NULL )
	{
		CloseHandle(hFile);

		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == NULL )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);

		return false;
	}

	bIsProcess = true;

#ifdef _UNICODE
	if( nFileType == 1 )				// UNICODE FILE
	{
		TDestination.Init(dwLength + 1);

		_tcsncpy_s(TDestination.GetBuffer(), dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);
	}
	else if( nFileType == 2 )			// UNICODE(BIG ENDIAN) FILE
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = NULL;

		pwszBuffer = new wchar_t[dwLength + 1];

		_tcsncpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		TDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pwszBuffer, _TRUNCATE);

		if( pwszBuffer )
		{
			delete pwszBuffer;
			pwszBuffer = NULL;
		}
	}
	else if( nFileType == 3 )			// UTF-8 FILE
	{
		if( !UnicodeToUTF8(TDestination, (char*)lpvFile + 1) ) bIsProcess = false;
	}
	else if( nFileType == 4 )			// ANSI FILE
	{
		if( !MultiByteToWideCharStr(TDestination, (char*)lpvFile) ) bIsProcess = false;
	}
#else
	if( nFileType == 1 )				// UNICODE FILE
	{
		if( !WideCharToMultiByteStr(TDestination, (wchar_t*)lpvFile + 1) ) bIsProcess = false;
	}
	else if( nFileType == 2 )			// UNICODE(BIG ENDIAN) FILE
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = NULL;

		pwszBuffer = new wchar_t[dwLength + 1];

		wcscpy_s(pwszBuffer, dwLength + 1, (wchar_t*)lpvFile + 1);

		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;
		if( !WideCharToMultiByteStr(TDestination, pwszBuffer) ) bIsProcess = false;

		if( pwszBuffer )
		{
			delete pwszBuffer;
			pwszBuffer = NULL;
		}
	}
	else if( nFileType == 3 )			// UTF-8 FILE	
	{
		if( !AnsiToUTF8(TDestination, (char*)lpvFile + 1) ) bIsProcess = false;
	}
	else if( nFileType == 4 )			// ANSI FILE
	{
		TDestination.Init(dwLength + 1);

		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), (char*)lpvFile, _TRUNCATE);
	}
#endif

	UnmapViewOfFile(lpvFile);
	lpvFile = NULL;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}

//***************************************************************************
//
BOOL SaveFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength)
{
	long	lWriteSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	if( dwLength > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwLength;

	while( 1 )
	{
		BOOL bReturn = WriteFile(hFile, pbBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
		if( bReturn == FALSE )
		{
			CloseHandle(hFile);

			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwLength - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwLength - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer)
{
	BOOL	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = NULL;

	HANDLE	hFile;

	if( !ptszFullPath ) return false;
	if( _tcslen(ptszFullPath) < 1 ) return false;
	if( !ptszBuffer ) return false;
	if( _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	StrBuffer;

	if( !WideCharToMultiByteStr(StrBuffer, ptszBuffer) ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	pszBuffer = (TCHAR*)ptszBuffer;
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	dwTotFileSize = (DWORD)strlen(pszBuffer);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pszBuffer + dwWriteOffset, (DWORD)dwWriteSize, (LPDWORD)&dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);

			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer)
{
	BOOL	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	wchar_t	wszChar[2];
	wchar_t* pwszBuffer = NULL;

	HANDLE	hFile;

	if( !ptszFullPath ) return false;
	if( _tcslen(ptszFullPath) < 1 ) return false;
	if( !ptszBuffer ) return false;
	if( _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( !MultiByteToWideCharStr(WStrBuffer, ptszBuffer) ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	swprintf_s(wszChar, 2, L"%c", UNICODE_LE_FILE_IDENTIFIER_WORD);
	bReturn = WriteFile(hFile, wszChar, (DWORD)wcslen(wszChar) + 1, &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);

		return false;
	}

	dwTotFileSize = ((DWORD)wcslen(pwszBuffer) * 2);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pwszBuffer + (dwWriteOffset / 2), dwWriteSize, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);

			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer)
{
	BOOL	bReturn = false;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	wchar_t wcChar = L'\0';
	wchar_t	wszChar[2];
	wchar_t* pwszBuffer = NULL;

	HANDLE	hFile;

	if( !ptszFullPath ) return false;
	if( _tcslen(ptszFullPath) < 1 ) return false;
	if( !ptszBuffer ) return false;
	if( _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( !MultiByteToWideCharStr(WStrBuffer, ptszBuffer) ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	swprintf_s(wszChar, 2, L"%c", UNICODE_BE_FILE_IDENTIFIER_WORD);
	bReturn = WriteFile(hFile, wszChar, (DWORD)wcslen(wszChar) + 1, &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);

		return false;
	}

	while( *pwszBuffer )
	{
		wcChar = *pwszBuffer;

		wcChar = SWAP16(wcChar);
		if( wcChar == 0xCDCD ) break;

		swprintf_s(wszChar, 2, L"%c", wcChar);
		bReturn = WriteFile(hFile, wszChar, (DWORD)wcslen(wszChar) + 1, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);

			return false;
		}

		pwszBuffer++;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL SaveUTF8File(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer)
{
	BOOL	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = NULL;
	wchar_t	wszChar[3];

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;

	if( !ptszFullPath ) return false;
	if( _tcslen(ptszFullPath) < 1 ) return false;
	if( !ptszBuffer ) return false;
	if( _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	if( !UTF8ToUnicode(StrBuffer, ptszBuffer) ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	if( !UTF8ToAnsi(StrBuffer, ptszBuffer) ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	swprintf_s(wszChar, 3, L"%c%c", UTF_FILE_IDENTIFIER_WORD, UTF_FILE_IDENTIFIER_BYTE);
	bReturn = WriteFile(hFile, wszChar, (DWORD)wcslen(wszChar) + 1, &dwWrittenSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);

		return false;
	}

	dwTotFileSize = (DWORD)strlen(pszBuffer);

	if( dwTotFileSize > dwMaxWriteSize )
		dwWriteSize = dwMaxWriteSize;
	else dwWriteSize = dwTotFileSize;

	while( 1 )
	{
		bReturn = WriteFile(hFile, pszBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
		if( !bReturn )
		{
			CloseHandle(hFile);

			return false;
		}

		if( dwMaxWriteSize > dwWrittenSize ) break;

		dwWriteOffset = dwWriteOffset + dwWriteSize;
		lWriteSize = dwTotFileSize - dwWriteOffset - dwMaxWriteSize;
		if( lWriteSize < 0 )
			dwWriteSize = dwTotFileSize - dwWriteOffset;
		else dwWriteSize = dwMaxWriteSize;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
BOOL GetFileInfoTime(const TCHAR* ptszFilePath, const int nCase, SYSTEMTIME& stLocal)
{
	FILETIME ftCreate, ftAccess, ftWrite;
	SYSTEMTIME stUTC;

	HANDLE hFile;

	hFile = CreateFile(ptszFilePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return false;

	// Retrieve the file times for the file.
	if( !GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite) )
	{
		CloseHandle(hFile);

		return false;
	}

	CloseHandle(hFile);

	if( nCase == FILEINFO_CREATETIME )
		FileTimeToSystemTime(&ftCreate, &stUTC);
	else if( nCase == FILEINFO_ACCESSTIME )
		FileTimeToSystemTime(&ftAccess, &stUTC);
	else if( nCase == FILEINFO_LASTWRITETIME )
		FileTimeToSystemTime(&ftWrite, &stUTC);
	else return false;

	SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

	return true;
}

//***************************************************************************
//
BOOL IsExistFile(const TCHAR* ptszFilePath)
{
	HANDLE		hFile;

	hFile = CreateFile(ptszFilePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		return false;
	}

	CloseHandle(hFile);

	return true;
}

//***************************************************************************
//
DWORD GetFileSize(const TCHAR* ptszFilePath)
{
	DWORD		dwFileSizeLow = 0;
	DWORD		dwFileSizeHigh = 0;

	HANDLE		hFile;

	hFile = CreateFile(ptszFilePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	else dwFileSizeLow = 0;

	CloseHandle(hFile);

	return dwFileSizeLow;
}

//***************************************************************************
//
BOOL GetFileInformation(const TCHAR* ptszFilePath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	BOOL		bResult = false;

	HANDLE		hFile;

	hFile = CreateFile(ptszFilePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		bResult = GetFileInformationByHandle(hFile, lpFileInformation);

	CloseHandle(hFile);

	return bResult;
}

//***************************************************************************
//
BOOL GetProductKeyExtract(CMemBuffer<TCHAR>& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const BOOL bIsExtractBytesRange)
{
	int		nKeyStartIndex = 0;
	int		nKeyEndIndex = 0;
	int		nIsContainsN = 0;
	BYTE	bProductKeyExtract[16];
	BYTE* pbSrcDigitalProductID = NULL;
	TCHAR* ptszDecodedChars = NULL;
	TCHAR* ptszDestLoc = NULL;

	TCHAR ptszKeyChars[] = {
							_T('B'), _T('C'), _T('D'), _T('F'), _T('G'), _T('H'), _T('J'), _T('K'), _T('M'),
							_T('P'), _T('Q'), _T('R'), _T('T'), _T('V'), _T('W'), _T('X'), _T('Y'),
							_T('2'), _T('3'), _T('4'), _T('6'), _T('7'), _T('8'), _T('9'), _T('\0')
	};
	const int nNumLetters = 24;
	const int nDecodeLength = 29;
	const int nDecodeStringLength = 15;

	if( !pbDigitalProductID ) return false;
	if( dwLength < 1 ) return false;

	pbSrcDigitalProductID = new BYTE[dwLength + 1];
	memcpy(pbSrcDigitalProductID, pbDigitalProductID, dwLength + 1);

	if( bIsExtractBytesRange )
		nKeyStartIndex = 808;
	else nKeyStartIndex = 52;

	nKeyEndIndex = nKeyStartIndex + 15;

	// Check if Windows 8/Office 2013 Style Key(Can contain the letter 'N')
	nIsContainsN = (pbSrcDigitalProductID[nKeyStartIndex + 14] >> 3) & 1;
	pbSrcDigitalProductID[nKeyStartIndex + 14] = (BYTE)((pbSrcDigitalProductID[nKeyStartIndex + 14] & 0xF7) | ((nIsContainsN & 2) << 2));

	for( int i = nKeyStartIndex; i <= nKeyEndIndex; i++ )
		bProductKeyExtract[i - nKeyStartIndex] = pbSrcDigitalProductID[i];
	bProductKeyExtract[15] = '\0';

	ptszDecodedChars = new TCHAR[nDecodeLength + 1];
	for( int i = nDecodeLength - 1; i >= 0; i-- )
	{
		if( (i + 1) % 6 == 0 )
		{
			ptszDecodedChars[i] = '-';
		}
		else
		{
			// Do the actual decoding.
			int nDigitMapIndex = 0;

			for( int j = nDecodeStringLength - 1; j >= 0; j-- )
			{
				int nByteValue = (nDigitMapIndex << 8) | bProductKeyExtract[j];

				bProductKeyExtract[j] = (BYTE)(nByteValue / nNumLetters);
				nDigitMapIndex = nByteValue % nNumLetters;
				ptszDecodedChars[i] = ptszKeyChars[nDigitMapIndex];
			}
		}
	}
	ptszDecodedChars[nDecodeLength] = '\0';

	// Remove first character and put 'N' in the right place
	if( nIsContainsN != 0 )
	{
		CMemBuffer<TCHAR> TDestReplace;
		CMemBuffer<TCHAR> TDestMid01;
		CMemBuffer<TCHAR> TDestMid02;
		CMemBuffer<TCHAR> TDestAppend01;
		CMemBuffer<TCHAR> TDestAppend02;

		int nFirstLetterIndex = 0;

		for( int k = 0; k < nNumLetters; k++ )
		{
			if( ptszDecodedChars[0] != ptszKeyChars[k] ) continue;
			nFirstLetterIndex = k;
			break;
		}

		StrReplace(TDestReplace, ptszDecodedChars + 1, _T("-"), _T(""));
		StrMid(TDestMid01, TDestReplace.GetBuffer(), 1, nFirstLetterIndex);
		StrMid(TDestMid02, TDestReplace.GetBuffer(), nFirstLetterIndex + 1, TDestReplace.GetBufSize() - (nFirstLetterIndex + 1));
		StrAppend(TDestAppend01, TDestMid01.GetBuffer(), _T("N"));
		StrAppend(TDestAppend02, TDestAppend01.GetBuffer(), TDestMid02.GetBuffer());
		StrCatLocationToken(TProductKey, TDestAppend02.GetBuffer(), 5, _T('-'));
	}
	else
	{
		TProductKey.Init(nDecodeLength + 1);

		ptszDestLoc = TProductKey.GetBuffer();

		_tcsncpy_s(ptszDestLoc, nDecodeLength, ptszDecodedChars, _TRUNCATE);
	}

	if( ptszDecodedChars )
	{
		delete[]ptszDecodedChars;
		ptszDecodedChars = NULL;
	}

	return true;
}

