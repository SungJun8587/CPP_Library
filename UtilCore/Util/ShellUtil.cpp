
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
	TCHAR*		ptszApplyExt = nullptr;

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

		std::vector<_tstring> destination;
		Tokenize(destination, ShApplyFileInfo.m_tszApplyExt, _T(";"));

		nTokenCount = destination.size();
		for( nIndex = 0; nIndex < nTokenCount; nIndex++ )
		{
			ptszApplyExt = const_cast<TCHAR*>(destination[nIndex].c_str());
			if( _tcscmp(ptszApplyExt, tszFileExt) == 0 ) break;
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

