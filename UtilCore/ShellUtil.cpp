
//***************************************************************************
// ShellUtil.cpp : implementation of the ShellUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "ShellUtil.h"

//***************************************************************************
//
bool IsAbleFile(const TCHAR* ptszSourceFullPath, const SH_APPLY_FILEINFO ShApplyFileInfo)
{
	size_t		nTokenCount = 0;
	size_t		nIndex = 0;
	TCHAR		tszFileNameExt[FILENAMEEXT_STRLEN];
	TCHAR		tszFileName[FILENAME_STRLEN];
	TCHAR		tszFileExt[FILEEXT_STRLEN];
	TCHAR		tszTime[FILEATT_DATETIME_STRLEN];
	TCHAR* ptszApplyExt = nullptr;

	CMemBuffer<TCHAR>* pTDestination = nullptr;

	SYSTEMTIME	stLocal;

	int iLength = static_cast<int>(_tcslen(ptszSourceFullPath));
	if( ptszSourceFullPath == nullptr || iLength == 0 ) return false;

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
				delete [] pTDestination;
				pTDestination = nullptr;
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
bool CreateDirectoryRecursive(const TCHAR* ptszFolder)
{
	int		iCount = 0;
	TCHAR	tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR	tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR* ptszSourceLoc = nullptr;

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	int iLength = static_cast<int>(_tcslen(ptszFolder));
	if( ptszFolder == nullptr || iLength == 0 ) return false;

	for( ptszSourceLoc = (TCHAR*)(ptszFolder + iLength - 1); iCount < iLength; ptszSourceLoc-- )
	{
		if( *ptszSourceLoc == '/' || *ptszSourceLoc == '\\' )
		{
			iCount++;
			break;
		}
		iCount++;
	}

	_tcsncpy_s(tszSourceFolder, _countof(tszSourceFolder), ptszFolder, iLength - iCount);
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

	return CreateDirectory(ptszFolder, nullptr);
}

//***************************************************************************
//
bool RemoveDirectoryRecursive(const TCHAR* ptszFolder, const bool bSelfDel)
{
	static TCHAR	tszSelfSourceFolder[DIRECTORY_STRLEN] = { 0, };

	bool		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	int iLength = static_cast<int>(_tcslen(ptszFolder));
	if( ptszFolder == nullptr || iLength == 0 ) return false;

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
bool CopyFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo)
{
	bool		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR		tszDestFullPath[FULLPATH_STRLEN];
	TCHAR		tszDestFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( ptszSourceFolder == nullptr || ptszDestFolder == nullptr ) return false;
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
bool MoveFileRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, const SH_APPLY_FILEINFO& ShApplyFileInfo)
{
	bool		bResult = true;
	TCHAR		tszActiveFullPath[FULLPATH_STRLEN];
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR		tszDestFullPath[FULLPATH_STRLEN];
	TCHAR		tszDestFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( ptszSourceFolder == nullptr || ptszDestFolder == nullptr ) return false;
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
bool IsDirectory(const TCHAR* ptszFolder)
{
	bool		bResult = true;
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	int iLength = static_cast<int>(_tcslen(ptszFolder));
	if( ptszFolder == nullptr || iLength == 0 ) return false;

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
	TCHAR* ptszActiveSubKey = nullptr;

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
				delete []ptszActiveSubKey;
				ptszActiveSubKey = nullptr;

				RegCloseKey(hKey);

				return lRetCode;
			}

			delete []ptszActiveSubKey;
			ptszActiveSubKey = nullptr;
		}
	}

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
							  nullptr, &hKey, &dwDisposition);

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
		lRetCode = RegEnumKeyEx(newKey, 0, szNewSubKey, &dwSize, nullptr, nullptr, nullptr, &FileTime);
		if( lRetCode != ERROR_SUCCESS ) break;

		lRetCode = RegDeleteKeyRecursive(newKey, szNewSubKey);
		if( lRetCode != ERROR_SUCCESS ) break;
	}

	RegCloseKey(newKey);

	return RegDeleteKey(hKey, ptszSubKey);
}

//***************************************************************************
//
bool RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD dwType, const void* pvValue, DWORD dwLength)
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
bool RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const BYTE* pbValue, DWORD dwLength)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
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
bool RegSetValue(const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName, const DWORD dwValue)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
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

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return 0;

	lRetCode = RegQueryValueEx(hKey, ptszName, nullptr, &dwType, nullptr, &dwLength);
	if( lRetCode != ERROR_SUCCESS )
	{
		RegCloseKey(hKey);
		return 0;
	}

	return dwLength;
}

//***************************************************************************
//
bool RegGetValue(void* pvValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const DWORD dwOptions, const REGSAM samDesired, const TCHAR* ptszName, DWORD& dwType)
{
	long		lRetCode = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegOpenKeyEx(hRoot, ptszSubKey, dwOptions, samDesired, &hKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegQueryValueEx(hKey, ptszName, nullptr, &dwType, (BYTE*)pvValue, &dwLength);
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
bool RegGetValue(BYTE* pbValue, DWORD& dwLength, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName)
{
	long		lRetCode = 0;
	DWORD		dwType = 0;
	DWORD		dwDisposition = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegQueryValueEx(hKey, ptszName, nullptr, &dwType, pbValue, &dwLength);
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
bool RegGetValue(DWORD* pdwValue, const HKEY hRoot, const TCHAR* ptszSubKey, const TCHAR* ptszName)
{
	long		lRetCode = 0;
	DWORD		dwType = 0;
	DWORD		dwDisposition = 0;
	DWORD		dwSize = 0;

	HKEY		hKey;

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &dwDisposition);
	if( lRetCode != ERROR_SUCCESS ) return false;

	dwSize = sizeof(DWORD);
	lRetCode = RegQueryValueEx(hKey, ptszName, nullptr, &dwType, (BYTE*)pdwValue, &dwSize);
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
bool IsRegKey(const HKEY hKey, const TCHAR* ptszSubKey)
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

	hFile = CreateFile(tszTempFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		tszTempFullPath[0] = '\0';
		tszTempFileNameExt[0] = '\0';
		for( i = 1; i < MAX_FILENAME_CONVERT_INDEX_NUM; i++ )
		{
			_sntprintf_s(tszTempFileNameExt, _countof(tszTempFileNameExt), _TRUNCATE, _T("%s(%d).%s"), tszFileName, i, tszFileExt);
			_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), tszFolderPath, tszTempFileNameExt);
			hFile = CreateFile(tszTempFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);

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

//***************************************************************************
//
bool IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize)
{
	bool bUTF8 = true;
	unsigned char* start = (unsigned char*)pBuffer;
	unsigned char* end = (unsigned char*)pBuffer + BuffSize;
	while( start < end )
	{
		if( *start < 0x80 )			// (10000000)[output][/output]
		{
			start++;
		}
		else if( *start < (0xC0) )	// (11000000)
		{
			bUTF8 = false;
			break;
		}
		else if( *start < (0xE0) )	// (11100000)
		{
			if( start >= end - 1 )
				break;
			if( (start[1] & (0xC0)) != 0x80 )
			{
				bUTF8 = false;
				break;
			}
			start += 2;
		}
		else if( *start < (0xF0) )	// (11110000)
		{
			if( start >= end - 2 )
				break;
			if( (start[1] & (0xC0)) != 0x80 || (start[2] & (0xC0)) != 0x80 )
			{
				bUTF8 = false;
				break;
			}
			start += 3;
		}
		else
		{
			bUTF8 = false;
			break;
		}
	}
	return bUTF8;
}

//***************************************************************************
//
EFileType IsFileType(const TCHAR* ptszFullPath)
{
	BOOL		bReturn = false;
	DWORD		dwReadSize = 0;
	WORD		wWord1, wWord2;
	BYTE		bByte;
	wchar_t		wszBuffer[3];
	EFileType	eFileType = DEFAULT;

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return eFileType;
	}

	bReturn = ReadFile(hFile, wszBuffer, sizeof(WORD) * 2, &dwReadSize, NULL);
	if( !bReturn )
	{
		CloseHandle(hFile);
		return eFileType;
	}
	wszBuffer[2] = '\0';

	CloseHandle(hFile);

	wWord1 = wszBuffer[0];
	wWord2 = wszBuffer[1];
	if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD || wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD )
	{
		if( wWord1 == UNICODE_LE_FILE_IDENTIFIER_WORD )
			eFileType = UTF16_LE;		// UNICODE(LITTLE ENDIAN) 
		else if( wWord1 == UNICODE_BE_FILE_IDENTIFIER_WORD )
			eFileType = UTF16_BE;		// UNICODE(BIG ENDIAN)
	}
	else
	{
		bByte = ((BYTE)(wWord2 & 0xff));
		if( wWord1 == (WORD)UTF_FILE_IDENTIFIER_WORD && bByte == (BYTE)UTF_FILE_IDENTIFIER_BYTE )
			eFileType = UTF8_BOM;	// UTF8_BOM
		else
		{
			CMemBuffer<BYTE> ByteDestination;
			if( !ReadFileMap(ByteDestination, ptszFullPath) ) return eFileType;

			if( IsUTF8WithoutBom((const void*)ByteDestination.GetBuffer(), ByteDestination.GetBufSize()) )
				eFileType = UTF8_NOBOM;		// UTF8_NOBOM
			else
				eFileType = ANSI;			// ANSI
		}
	}

	return eFileType;
}

//***************************************************************************
//
bool ReadFile(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath)
{
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	BYTE* pbBuffer = nullptr;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
		bool bReturn = ReadFile(hFile, pbBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
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
bool ReadFile(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath)
{
	bool	bReturn = false;
	int		i = 0;
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EFileType	eFileType = DEFAULT;

	CMemBuffer<char>	StrBuffer;
	CMemBuffer<wchar_t>	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	dwMaxReadSize = dwLength;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( eFileType == UTF16_BE || eFileType == UTF16_LE )
	{
		SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);

		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.Init(dwLength + 1);
		pwszBuffer = WStrBuffer.GetBuffer();

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
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
	else if( eFileType == ANSI || eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( eFileType == UTF8_BOM )
		{
			SetFilePointer(hFile, sizeof(WORD) + sizeof(BYTE), nullptr, FILE_BEGIN);

			dwLength = dwLength - (sizeof(WORD) + sizeof(BYTE));

			StrBuffer.Init(dwLength + 1);
			pszBuffer = StrBuffer.GetBuffer();
		}
		else
		{
			StrBuffer.Init(dwLength + 1);
			pszBuffer = StrBuffer.GetBuffer();

			dwMaxReadSize = dwMaxReadSize + 1;
		}

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
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

	if( eFileType == UTF16_LE || eFileType == UTF16_BE )
	{
		if( pwszBuffer == nullptr ) return false;
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( pszBuffer == nullptr ) return false;
	}
	else
	{
		if( pszBuffer == nullptr ) return false;
	}

#ifdef _UNICODE
	if( eFileType == UTF16_LE )
	{
		TDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( eFileType == UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer = pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		TDestination.Init(wcslen(pwszBuffer) + 1);
		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pwszBuffer, _TRUNCATE);
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToUnicode(TDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		if( AnsiToUnicode(TDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
#else
	if( eFileType == UTF16_LE )
	{
		if( UnicodeToAnsi(TDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == UTF16_BE )
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
		if( UnicodeToAnsi(TDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToAnsi(TDestination, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
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
bool ReadFileMap(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath)
{
	DWORD	dwLength = 0;
	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	ByteDestination.Init(dwLength);

	memcpy(ByteDestination.GetBuffer(), lpvFile, dwLength);

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return true;
}

//***************************************************************************
//
bool ReadFileMap(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath)
{
	bool	bIsProcess = false;
	DWORD	dwLength = 0;
	int		i = 0;
	EFileType	eFileType = DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	bIsProcess = true;

#ifdef _UNICODE
	if( eFileType == UTF16_LE )
	{
		TDestination.Init(dwLength + 1);

		_tcsncpy_s(TDestination.GetBuffer(), dwLength + 1, (wchar_t*)lpvFile + 1, _TRUNCATE);
	}
	else if( eFileType == UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

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
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToUnicode(TDestination, (char*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == ANSI )
	{
		if( AnsiToUnicode(TDestination, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
#else
	if( eFileType == UTF16_LE )
	{
		if( UnicodeToAnsi(TDestination, (wchar_t*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

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
		if( UnicodeToAnsi(TDestination, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) bIsProcess = false;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToAnsi(TDestination, (char*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == ANSI )
	{
		TDestination.Init(dwLength + 1);

		_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), (char*)lpvFile, _TRUNCATE);
	}
#endif

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}

#ifdef _STRING_
//***************************************************************************
//
bool ReadFile(_tstring& DestString, const TCHAR* ptszFullPath)
{
	bool	bReturn = false;
	int		i = 0;
	long	lReadSize = 0;
	DWORD	dwLength = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwReadSize = 0;
	DWORD	dwMaxReadSize = MAX_BUFFER_SIZE;
	DWORD	dwReadOffset = 0;
	DWORD	dwReadNumSize = 0;
	wchar_t	wcChar = L'\0';
	char* pszBuffer = nullptr;
	wchar_t* pwszBuffer = nullptr;

	HANDLE		hFile;
	EFileType	eFileType = DEFAULT;

	std::string		StrBuffer;
	std::wstring	WStrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	dwLength = GetFileSize(ptszFullPath);
	dwMaxReadSize = dwLength;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	if( eFileType == UTF16_BE || eFileType == UTF16_LE )
	{
		SetFilePointer(hFile, sizeof(WORD), nullptr, FILE_BEGIN);

		dwLength = dwLength - sizeof(WORD);

		WStrBuffer.resize(dwLength + 1);
		pwszBuffer = (wchar_t*)WStrBuffer.c_str();

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pwszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
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
	else if( eFileType == ANSI || eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( eFileType == UTF8_BOM )
		{
			SetFilePointer(hFile, sizeof(WORD) + sizeof(BYTE), nullptr, FILE_BEGIN);

			dwLength = dwLength - (sizeof(WORD) + sizeof(BYTE));

			StrBuffer.resize(dwLength + 1);
			pszBuffer = (char *)StrBuffer.c_str();
		}
		else
		{
			StrBuffer.resize(dwLength + 1);
			pszBuffer = (char*)StrBuffer.c_str();

			dwMaxReadSize = dwMaxReadSize + 1;
		}

		dwTotFileSize = dwLength + 1;
		if( dwTotFileSize > dwMaxReadSize )
			dwReadNumSize = dwMaxReadSize;
		else dwReadNumSize = dwTotFileSize;

		while( 1 )
		{
			bool bReturn = ReadFile(hFile, pszBuffer + dwReadOffset, dwReadNumSize, &dwReadSize, NULL);
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

	if( eFileType == UTF16_LE || eFileType == UTF16_BE )
	{
		if( pwszBuffer == nullptr ) return false;
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( pszBuffer == nullptr ) return false;
	}
	else
	{
		if( pszBuffer == nullptr ) return false;
	}

#ifdef _UNICODE
	if( eFileType == UTF16_LE )
	{
		DestString = pwszBuffer;
	}
	else if( eFileType == UTF16_BE )
	{
		for( i = 0; *pwszBuffer; i++ )
		{
			wcChar = *pwszBuffer;

			wcChar = SWAP16(wcChar);
			if( wcChar == 0xCDCD ) break;

			*pwszBuffer = wcChar;

			pwszBuffer = pwszBuffer++;
		}
		*pwszBuffer = '\0';

		pwszBuffer = pwszBuffer - i;

		DestString = pwszBuffer;
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToUnicode_String(DestString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		if( AnsiToUnicode_String(DestString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
#else
	if( eFileType == UTF16_LE )
	{
		if( UnicodeToAnsi_String(DestString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == UTF16_BE )
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
		if( UnicodeToAnsi_String(DestString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) return false;
	}
	else if( eFileType == UTF8_BOM || eFileType == UTF8_NOBOM )
	{
		if( Utf8ToAnsi_String(DestString, pszBuffer, strlen(pszBuffer) + 1) != 0 ) return false;
	}
	else
	{
		DestString = pszBuffer;
	}
#endif

	return true;
}

//***************************************************************************
//
bool ReadFileMap(_tstring& DestString, const TCHAR* ptszFullPath)
{
	bool	bIsProcess = false;
	DWORD	dwLength = 0;
	int		i = 0;
	EFileType	eFileType = DEFAULT;

	HANDLE	hFile, hFileMap;
	LPVOID	lpvFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	eFileType = IsFileType(ptszFullPath);

	hFile = CreateFile(ptszFullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return false;
	}

	dwLength = GetFileSize(ptszFullPath);

	hFileMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 0, dwLength, nullptr);
	if( hFileMap == nullptr )
	{
		CloseHandle(hFile);
		return false;
	}

	lpvFile = MapViewOfFile(hFileMap, FILE_MAP_COPY, 0, 0, 0);

	if( lpvFile == nullptr )
	{
		CloseHandle(hFile);
		CloseHandle(hFileMap);
		return false;
	}

	bIsProcess = true;

#ifdef _UNICODE
	if( eFileType == UTF16_LE )
	{
		DestString = (wchar_t*)lpvFile + 1;
	}
	else if( eFileType == UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

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

		DestString = pwszBuffer;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == UTF8_BOM )
	{
		if( Utf8ToUnicode_String(DestString, (char*)lpvFile + 3, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == UTF8_NOBOM )
	{
		if( Utf8ToUnicode_String(DestString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == ANSI )
	{
		if( AnsiToUnicode_String(DestString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
#else
	if( eFileType == UTF16_LE )
	{
		if( UnicodeToAnsi_String(DestString, (wchar_t*)lpvFile + 1, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == UTF16_BE )
	{
		wchar_t	wcChar = L'\0';
		wchar_t* pwszBuffer = nullptr;

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
		if( UnicodeToAnsi_String(DestString, pwszBuffer, wcslen(pwszBuffer) + 1) != 0 ) bIsProcess = false;

		if( pwszBuffer )
		{
			delete [] pwszBuffer;
			pwszBuffer = nullptr;
		}
	}
	else if( eFileType == UTF8_BOM )
	{
		if( Utf8ToAnsi_String(DestString, (char*)lpvFile + 3, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == UTF8_NOBOM )
	{
		if( Utf8ToAnsi_String(DestString, (char*)lpvFile, dwLength + 1) != 0 ) bIsProcess = false;
	}
	else if( eFileType == ANSI )
	{
		DestString = (char*)lpvFile;
	}
#endif

	UnmapViewOfFile(lpvFile);
	lpvFile = nullptr;

	CloseHandle(hFile);
	CloseHandle(hFileMap);

	return bIsProcess;
}
#endif

//***************************************************************************
//
bool SaveFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength)
{
	long	lWriteSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;

	HANDLE	hFile;

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
		bool bReturn = WriteFile(hFile, pbBuffer + dwWriteOffset, dwWriteSize, &dwWrittenSize, NULL);
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
bool SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	CMemBuffer<char>	StrBuffer;

	if( UnicodeToAnsi(StrBuffer, ptszBuffer, _tcslen(ptszBuffer) + 1) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	pszBuffer = (char*)ptszBuffer;
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
bool SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	wchar_t	wcChar = L'\0';
	wchar_t	wszChar[2];
	wchar_t* pwszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( AnsiToUnicode(WStrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
bool SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	wchar_t	wszChar[2];
	wchar_t* pwszBuffer = nullptr;

	HANDLE	hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	pwszBuffer = (TCHAR*)ptszBuffer;
#else
	CMemBuffer<wchar_t>	WStrBuffer;

	if( AnsiToUnicode(WStrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pwszBuffer = WStrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
bool SaveUTF8BOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = nullptr;
	wchar_t	wszChar[3];

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	if( AnsiToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
bool SaveUTF8NOBOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize)
{
	bool	bReturn = false;
	long	lWriteSize = 0;
	DWORD	dwTotFileSize = 0;
	DWORD	dwWrittenSize = 0;
	DWORD	dwMaxWriteSize = MAX_BUFFER_SIZE;
	DWORD	dwWriteOffset = 0;
	DWORD	dwWriteSize = 0;
	char* pszBuffer = nullptr;

	HANDLE	hFile;

	CMemBuffer<char>	StrBuffer;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;
	if( ptszBuffer == nullptr || _tcslen(ptszBuffer) < 1 ) return false;

#ifdef _UNICODE
	if( UnicodeToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#else
	if( AnsiToUtf8(StrBuffer, ptszBuffer, BufferSize) != 0 ) return false;

	pszBuffer = StrBuffer.GetBuffer();
#endif

	hFile = CreateFile(ptszFullPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
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
bool GetFileInfoTime(const TCHAR* ptszFullPath, const int nCase, SYSTEMTIME& stLocal)
{
	FILETIME ftCreate, ftAccess, ftWrite;
	SYSTEMTIME stUTC;

	HANDLE hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

	SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

	return true;
}

//***************************************************************************
//
bool IsExistFile(const TCHAR* ptszFullPath)
{
	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

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
DWORD GetFileSize(const TCHAR* ptszFullPath)
{
	DWORD		dwFileSizeLow = 0;
	DWORD		dwFileSizeHigh = 0;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	else dwFileSizeLow = 0;

	CloseHandle(hFile);

	return dwFileSizeLow;
}

//***************************************************************************
//
bool GetFileInformation(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
	bool		bResult = false;

	HANDLE		hFile;

	if( ptszFullPath == nullptr || _tcslen(ptszFullPath) < 1 ) return false;

	hFile = CreateFile(ptszFullPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);

	if( hFile != INVALID_HANDLE_VALUE )
		bResult = GetFileInformationByHandle(hFile, lpFileInformation);

	CloseHandle(hFile);

	return bResult;
}

//***************************************************************************
//
bool GetProductKeyExtract(CMemBuffer<TCHAR>& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const bool bIsExtractBytesRange)
{
	int		nKeyStartIndex = 0;
	int		nKeyEndIndex = 0;
	int		nIsContainsN = 0;
	BYTE	bProductKeyExtract[16];
	BYTE* pbSrcDigitalProductID = nullptr;
	TCHAR* ptszDecodedChars = nullptr;
	TCHAR* ptszDestLoc = nullptr;

	TCHAR ptszKeyChars [] = {
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
		delete []ptszDecodedChars;
		ptszDecodedChars = nullptr;
	}

	return true;
}

