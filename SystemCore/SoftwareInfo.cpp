
//***************************************************************************
// SoftwareInfo.cpp: implementation of the Software Information Class.
//
//***************************************************************************

#include "pch.h"
#include "SoftwareInfo.h"

//***************************************************************************
//
BOOL GetVersionLangOfFile(TCHAR *ptszAppName, TCHAR *ptszVersion, TCHAR *ptszLanguage)
{
	BOOL		bResult = false;
	DWORD		dwScratch = 0;
	DWORD		dwInfSize = 0;
	DWORD		*pdwLangChar;
	UINT		uSize = 0;
	BYTE		*pbInfBuff = NULL;
	TCHAR		tszResource[MAX_BUFFER_SIZE];
	TCHAR		*ptszTempVersion = NULL;

	dwInfSize = GetFileVersionInfoSize(ptszAppName, &dwScratch);
	if( dwInfSize )
	{
		pbInfBuff = new BYTE[dwInfSize];
		memset(pbInfBuff, 0, dwInfSize);

		if( pbInfBuff )
		{
			if( GetFileVersionInfo(ptszAppName, 0, dwInfSize, pbInfBuff) )
			{
				if( VerQueryValue(pbInfBuff, _T("\\VarFileInfo\\Translation"), (void**)(&pdwLangChar), &uSize) )
				{
					if( VerLanguageName(LOWORD(*pdwLangChar), tszResource, sizeof(tszResource)) )
						_tcscpy_s(ptszLanguage, MAX_BUFFER_SIZE, tszResource);

					_stprintf_s(tszResource, _countof(tszResource), _T("\\StringFileInfo\\%04X%04X\\FileVersion"), LOWORD(*pdwLangChar), HIWORD(*pdwLangChar));

					if( VerQueryValue(pbInfBuff, tszResource, (void**)(&ptszTempVersion), &uSize) )
						_tcscpy_s(ptszVersion, MAX_BUFFER_SIZE, ptszTempVersion);

					bResult = true;
				}
			}

			delete[]pbInfBuff;
		}
	}

	return bResult;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CIeInfo::CIeInfo()
{
	ZeroMemory(&m_Ie, sizeof(SWINFO_IE));
}

CIeInfo::~CIeInfo()
{
}

//***************************************************************************
//
BOOL CIeInfo::GetInformation()
{
	DWORD	dwNameLen = 0;
	DWORD   dwValueLen = 0;
	long	lRetCode = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR   tszBuild[IE_BUILD_STRLEN];
	TCHAR   tszVersion[IE_VERSION_STRLEN];

	HKEY	hKeyIE;

	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), WIN_MICROSOFT_KEY, WIN_IE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyIE);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(WIN_IE_BUILD_NAME);
			dwValueLen = sizeof(tszBuild);

			tszBuild[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, WIN_IE_BUILD_NAME, NULL, &dwNameLen, (LPBYTE)tszBuild, &dwValueLen);
			if( !((lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN)) )
				_tcscpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"));

			dwNameLen = sizeof(WIN_IE_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			tszVersion[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, WIN_IE_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( !((lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN)) )
				_tcscpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"));
		}
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), NT_MICROSOFT_KEY, NT_IE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyIE);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(NT_IE_BUILD_NAME);
			dwValueLen = sizeof(tszBuild);

			tszBuild[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, NT_IE_BUILD_NAME, NULL, &dwNameLen, (LPBYTE)tszBuild, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcscpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"));

			dwNameLen = sizeof(NT_IE_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			tszVersion[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, NT_IE_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcscpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"));
		}

		RegCloseKey(hKeyIE);
	}
	else
	{
		_tcscpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"));
		_tcscpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"));
	}

	_tcscpy_s(m_Ie.m_tszBuild, _countof(m_Ie.m_tszBuild), tszBuild);
	_tcscpy_s(m_Ie.m_tszVersion, _countof(m_Ie.m_tszVersion), tszVersion);

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CDirectXInfo::CDirectXInfo()
{
	ZeroMemory(&m_DirectX, sizeof(SWINFO_DIRECTX));
}

CDirectXInfo::~CDirectXInfo()
{
}

//***************************************************************************
//
BOOL CDirectXInfo::GetInformation()
{
	DWORD	dwNameLen = 0;
	DWORD   dwValueLen = 0;
	long	lRetCode = 0;
	__int64 qwInstallVersion = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR   tszVersion[DIRECTX_VERSION_STRLEN];
	TCHAR   tszInstallVersion[DIRECTX_INSTALLVERSION_STRLEN];
	TCHAR   tszDescription[DIRECTX_DESCRIPTION_STRLEN];

	HKEY	hKeyDirectX;

	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), WIN_MICROSOFT_KEY, WIN_DIRECTX_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyDirectX);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(WIN_DIRECTX_INSTALLVER_NAME);
			dwValueLen = sizeof(qwInstallVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, WIN_DIRECTX_INSTALLVER_NAME, NULL, &dwNameLen, (LPBYTE)&qwInstallVersion, &dwValueLen);
			if( lRetCode == ERROR_SUCCESS )
			{
				_stprintf_s(tszInstallVersion, _countof(tszInstallVersion), _T("%d.%d.%d.%d"), LOBYTE(LOWORD(qwInstallVersion)),
					HIBYTE(LOWORD(qwInstallVersion)),
					LOBYTE(HIWORD(qwInstallVersion)),
					HIBYTE(HIWORD(qwInstallVersion)));
			}
			else _tcscpy_s(tszInstallVersion, _countof(tszInstallVersion), _T("UnKnown"));

			dwNameLen = sizeof(WIN_DIRECTX_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, WIN_DIRECTX_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcscpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"));
		}

		RegCloseKey(hKeyDirectX);
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), NT_MICROSOFT_KEY, NT_DIRECTX_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyDirectX);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(NT_DIRECTX_INSTALLVER_NAME);
			dwValueLen = sizeof(qwInstallVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, WIN_DIRECTX_INSTALLVER_NAME, NULL, &dwNameLen, (LPBYTE)&qwInstallVersion, &dwValueLen);
			if( lRetCode == ERROR_SUCCESS )
			{
				_stprintf_s(tszInstallVersion, _countof(tszInstallVersion), _T("%d.%d.%d.%d"), LOBYTE(LOWORD(qwInstallVersion)),
					HIBYTE(LOWORD(qwInstallVersion)),
					LOBYTE(HIWORD(qwInstallVersion)),
					HIBYTE(HIWORD(qwInstallVersion)));
			}
			else _tcscpy_s(tszInstallVersion, _countof(tszInstallVersion), _T("UnKnown"));

			dwNameLen = sizeof(NT_DIRECTX_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, NT_DIRECTX_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcscpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"));
		}

		RegCloseKey(hKeyDirectX);
	}

	tszDescription[0] = '\0';
	if( _tcscmp(tszVersion, _T("4.09.00.0900")) == 0 )
		_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0"));
	else if( _tcscmp(tszVersion, _T("4.09.00.0901")) == 0 )
		_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0a"));
	else if( _tcscmp(tszVersion, _T("4.09.00.0902")) == 0 )
		_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0b"));
	else if( _tcscmp(tszVersion, _T("4.09.00.0903")) == 0 )
		_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0c"));
	else if( _tcscmp(tszVersion, _T("4.09.00.0904")) == 0 )
	{
		if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
		{
			if( IsWindowVersion(5, -1, -1) )
				_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0c"));
			else if( IsWindowVersion(6, 0, -1) )
				_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 10"));
			else if( IsWindowVersion(6, 1, -1) )
				_tcscpy_s(tszDescription, _countof(tszDescription), _T("DirectX 11"));
		}
	}

	_tcscpy_s(m_DirectX.m_tszVersion, _countof(m_DirectX.m_tszVersion), tszVersion);
	_tcscpy_s(m_DirectX.m_tszInstallVersion, _countof(m_DirectX.m_tszInstallVersion), tszInstallVersion);
	_tcscpy_s(m_DirectX.m_tszDescription, _countof(m_DirectX.m_tszDescription), tszDescription);

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CJavaVMInfo::CJavaVMInfo()
{
}

CJavaVMInfo::~CJavaVMInfo()
{
}

//***************************************************************************
//
BOOL CJavaVMInfo::GetInformation()
{
	BOOL	bIsSunJVM = false;
	BOOL	bIsMsJVM = false;
	BOOL	bIsMCompany = false;
	BOOL	bIsSunCompany = false;

	DWORD	dwNameLen = 0;
	DWORD   dwValueLen = 0;
	DWORD	dwIndexEnum = 0;
	long	lRetCode = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszGroupName[REGISTRY_NAME_STRLEN];
	TCHAR   tszMsJVMRuntimeLibPath[FULLPATH_STRLEN];
	TCHAR   tszSunJVMRuntimeLibPath[FULLPATH_STRLEN];

	HKEY	hKeyEnum;
	HKEY	hKeyJavaVm;

	FILETIME MyFileTime;

 	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), WIN_MS_JAVAVM_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyJavaVm);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(WIN_MS_JAVAVM_RUNTIMELIB_NAME);
			dwValueLen = sizeof(tszMsJVMRuntimeLibPath);

			tszMsJVMRuntimeLibPath[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyJavaVm, WIN_MS_JAVAVM_RUNTIMELIB_NAME, NULL, &dwNameLen, (LPBYTE)tszMsJVMRuntimeLibPath, &dwValueLen);
			if( !((lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN)) )
				bIsMCompany = true;
		}
		RegCloseKey(hKeyJavaVm);

		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), WIN_SUN_JAVAVM_JRE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), WIN_SUN_JAVAVM_JRE_KEY, tszGroupName);

				lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyJavaVm);
				if( lRetCode == ERROR_SUCCESS )
				{
					dwNameLen = sizeof(WIN_SUN_JAVAVM_RUNTIMELIB_NAME);
					dwValueLen = sizeof(tszSunJVMRuntimeLibPath);

					tszSunJVMRuntimeLibPath[0] = '\0';

					lRetCode = RegQueryValueEx(hKeyJavaVm, WIN_SUN_JAVAVM_RUNTIMELIB_NAME, NULL, &dwNameLen, (LPBYTE)tszSunJVMRuntimeLibPath, &dwValueLen);
					if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
					{
						// Cannot read the class name
						RegCloseKey(hKeyJavaVm);
						dwNameLen = sizeof(tszGroupName);
						continue;
					}

					RegCloseKey(hKeyJavaVm);
					bIsSunCompany = true;
				}

				dwIndexEnum++;
				dwNameLen = sizeof(tszGroupName);
			}
		}
		RegCloseKey(hKeyEnum);
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), NT_MS_JAVAVM_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyJavaVm);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(NT_MS_JAVAVM_RUNTIMELIB_NAME);
			dwValueLen = sizeof(tszMsJVMRuntimeLibPath);

			tszMsJVMRuntimeLibPath[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyJavaVm, NT_MS_JAVAVM_RUNTIMELIB_NAME, NULL, &dwNameLen, (LPBYTE)tszMsJVMRuntimeLibPath, &dwValueLen);
			if( !((lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN)) )
				bIsMCompany = true;
		}
		RegCloseKey(hKeyJavaVm);

		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), NT_SUN_JAVAVM_JRE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), NT_SUN_JAVAVM_JRE_KEY, tszGroupName);

				lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyJavaVm);
				if( lRetCode == ERROR_SUCCESS )
				{
					dwNameLen = sizeof(NT_SUN_JAVAVM_RUNTIMELIB_NAME);
					dwValueLen = sizeof(tszSunJVMRuntimeLibPath);

					tszSunJVMRuntimeLibPath[0] = '\0';

					lRetCode = RegQueryValueEx(hKeyJavaVm, NT_SUN_JAVAVM_RUNTIMELIB_NAME, NULL, &dwNameLen, (LPBYTE)tszSunJVMRuntimeLibPath, &dwValueLen);
					if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
					{
						// Cannot read the class name
						RegCloseKey(hKeyJavaVm);
						dwNameLen = sizeof(tszGroupName);
						continue;
					}

					RegCloseKey(hKeyJavaVm);
					bIsSunCompany = true;
				}

				dwIndexEnum++;
				dwNameLen = sizeof(tszGroupName);
			}
		}
		RegCloseKey(hKeyEnum);
	}
	else
	{
		tszMsJVMRuntimeLibPath[0] = '\0';
		tszSunJVMRuntimeLibPath[0] = '\0';
	}

	BOOL	bResult = true;
	TCHAR	tszWindowSystemDir[DIRECTORY_STRLEN];
	TCHAR	tszActiveFile[FULLPATH_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	if( bIsMCompany )
	{
		GetSystemDirectory(tszWindowSystemDir, sizeof(tszWindowSystemDir));
		_stprintf_s(tszMsJVMRuntimeLibPath, _countof(tszMsJVMRuntimeLibPath), _T("%s\\*.*"), tszWindowSystemDir);

		hFindFile = FindFirstFile(tszMsJVMRuntimeLibPath, &FindData);

		// Check if sub folders exists.
		if( INVALID_HANDLE_VALUE != hFindFile )
		{	// There are sub-folders.
			while( bResult )
			{
				if( _tcscmp(FindData.cFileName, _T(".")) != 0 && _tcscmp(FindData.cFileName, _T("..")) != 0
					&& !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
				{
					_stprintf_s(tszActiveFile, _countof(tszActiveFile), _T("%s"), FindData.cFileName);
					_tcslwr_s(tszActiveFile, _tcslen(tszActiveFile) + 1);

					if( _tcsstr(tszActiveFile, _T("java")) && _tcsstr(tszActiveFile, _T(".vxd")) )
					{
						bIsMsJVM = true;
						break;
					}
				}

				bResult = FindNextFile(hFindFile, &FindData);
			}
		}
		else bIsMsJVM = false;

		FindClose(hFindFile);
	}
	else bIsMsJVM = false;

	if( bIsSunCompany )
	{
		if( _tcslen(tszSunJVMRuntimeLibPath) > 0 )
		{
			int		nCount = 0;
			TCHAR	tszBuffer[FULLPATH_STRLEN];
			TCHAR	*ptszBuffer = NULL;

			CMemBuffer<TCHAR> TDestination1;
			CMemBuffer<TCHAR> TDestination2;

			GetSystemDirectory(tszWindowSystemDir, sizeof(tszWindowSystemDir));

			_tcscpy_s(tszBuffer, _countof(tszBuffer), tszSunJVMRuntimeLibPath);
			ptszBuffer = _tcschr(tszBuffer, ';');

			if( ptszBuffer )
			{
				nCount = (int)(_tcslen(tszSunJVMRuntimeLibPath) - _tcslen(ptszBuffer));
				StrLeft(TDestination1, tszSunJVMRuntimeLibPath, nCount);

				StrReplace(TDestination2, TDestination1.GetBuffer(), _T("%systemroot%"), tszWindowSystemDir);
			}
			else
				StrReplace(TDestination2, tszSunJVMRuntimeLibPath, _T("%systemroot%"), tszWindowSystemDir);

			hFindFile = FindFirstFile(TDestination2.GetBuffer(), &FindData);

			// Check if sub folders exists.
			if( INVALID_HANDLE_VALUE != hFindFile )
			{	// There are sub-folders.
				bIsSunJVM = true;
			}
			else bIsSunJVM = false;

			FindClose(hFindFile);
		}
	}
	else bIsSunJVM = false;

	if( !bIsMsJVM && !bIsSunJVM )
		m_nIsJVM = 0;
	else if( bIsMsJVM && !bIsSunJVM )
		m_nIsJVM = 1;
	else if( !bIsMsJVM && bIsSunJVM )
		m_nIsJVM = 2;
	else if( bIsMsJVM && bIsSunJVM )
		m_nIsJVM = 3;

	return true;
}

//***************************************************************************
//
BOOL CJavaVMInfo::GetVersionMsJVM(TCHAR *ptszMsJVMVersion)
{
	TCHAR	tszVersion[MAX_BUFFER_SIZE];
	TCHAR	tszLanguage[MAX_BUFFER_SIZE];
	TCHAR	tszWindowSystemDir[DIRECTORY_STRLEN];
	TCHAR	tszRuntimeLibFileName[FILENAMEEXT_STRLEN];
	TCHAR	tszRuntimeLibFilePath[FULLPATH_STRLEN];

	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
		_stprintf_s(tszRuntimeLibFileName, _countof(tszRuntimeLibFileName), _T("%s"), WIN_MS_JAVAVM_RUNDLL_FILENAME);
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
		_stprintf_s(tszRuntimeLibFileName, _countof(tszRuntimeLibFileName), _T("%s"), NT_MS_JAVAVM_RUNDLL_FILENAME);
	else
		tszRuntimeLibFileName[0] = '\0';

	if( _tcslen(tszRuntimeLibFileName) < 1 )
	{
		ptszMsJVMVersion[0] = '\0';
		return false;
	}
	else
	{
		GetSystemDirectory(tszWindowSystemDir, sizeof(tszWindowSystemDir));
		_stprintf_s(tszRuntimeLibFilePath, _countof(tszRuntimeLibFilePath), _T("%s\\%s"), tszWindowSystemDir, tszRuntimeLibFileName);

		if( GetVersionLangOfFile(tszRuntimeLibFilePath, tszVersion, tszLanguage) )
		{
			if( _tcslen(tszVersion) < 1 )
			{
				ptszMsJVMVersion[0] = '\0';
				return false;
			}
			else _tcscpy_s(ptszMsJVMVersion, JAVAVM_VERSION_STRLEN, tszVersion);
		}
		else
		{
			ptszMsJVMVersion[0] = '\0';
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
BOOL CJavaVMInfo::GetVersionSunJVM(TCHAR *ptszSunJVMVersion)
{
	HKEY	hKeyEnum;

	DWORD	dwNameLen = 0;
	DWORD   dwValueLen = 0;
	DWORD	dwIndexEnum = 0;
	long	lRetCode = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszGroupName[REGISTRY_NAME_STRLEN] = { 0, };

	FILETIME MyFileTime;


	if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), WIN_SUN_JAVAVM_PLUGIN_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				dwIndexEnum++;
				dwNameLen = sizeof(tszGroupName);
			}
		}

		RegCloseKey(hKeyEnum);
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), NT_SUN_JAVAVM_PLUGIN_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				dwIndexEnum++;
				dwNameLen = sizeof(tszGroupName);
			}
		}

		RegCloseKey(hKeyEnum);
	}
	else 
	{
		tszGroupName[0] = '\0';
	}

	if( _tcslen(tszGroupName) < 1 )
	{
		ptszSunJVMVersion[0] = '\0';
		return false;
	}
	else _tcscpy_s(ptszSunJVMVersion, JAVAVM_VERSION_STRLEN, tszGroupName);

	return true;
}

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CInstallSwInfo::CInstallSwInfo()
{
}

CInstallSwInfo::~CInstallSwInfo()
{
	INSTALL_SWINFO	*pInstallSwInfo = NULL;

	for( int i = 0; i < m_sInstallSwInfoArray.GetCount(); i++ )
	{
		pInstallSwInfo = m_sInstallSwInfoArray.At(i);

		if( pInstallSwInfo )
		{
			delete pInstallSwInfo;
			pInstallSwInfo = NULL;
		}
	}
}

//***************************************************************************
//
BOOL CInstallSwInfo::GetInformation()
{
	HKEY	hSubKey;
	HKEY	hKeyProperty;

	bool	bIsAdd = true;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];

	TCHAR   tszSubKeyName[REGISTRY_NAME_STRLEN];
	TCHAR   tszSubKeyValue[REGISTRY_VALUE_STRLEN];

	TCHAR   tszValue[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDisplayName[REGISTRY_VALUE_STRLEN];
	TCHAR	tszInstallSource[REGISTRY_VALUE_STRLEN];
	TCHAR	tszUninstallString[REGISTRY_VALUE_STRLEN];

	DWORD  	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	DWORD	dwIndexEnum = 0;
	DWORD	dwPropValueNumber = 0;
	DWORD	dwPropValueCount = 0;
	DWORD	dwType = 0;
	DWORD	dwCount = 0;
	long	lRetCode = 0;

	FILETIME	MyFileTime;

	INSTALL_SWINFO	*pInstallSwInfo = NULL;

	lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_SOFTWARE_UNINSTALL_KEY, 0, KEY_READ, &hSubKey);
	if( lRetCode == ERROR_SUCCESS )
	{
		dwNameLen = sizeof(tszSubKeyName);

		tszSubKeyName[0] = '\0';

		while( (lRetCode = RegEnumKeyEx(hSubKey, dwIndexEnum, tszSubKeyName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
		{
			_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), WIN_SOFTWARE_UNINSTALL_KEY, tszSubKeyName);

			tszDisplayName[0] = '\0';
			tszInstallSource[0] = '\0';
			tszUninstallString[0] = '\0';

			lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyProperty);
			if( lRetCode == ERROR_SUCCESS )
			{
				dwPropValueNumber = 0;
				dwPropValueCount = 0;
				if( RegQueryInfoKey(hKeyProperty, NULL, 0, 0, NULL, NULL, NULL, &dwPropValueNumber, NULL, NULL, NULL, &MyFileTime) == ERROR_SUCCESS )
				{
					while( dwPropValueNumber > dwPropValueCount )
					{
						dwNameLen = sizeof(tszSubKeyName);
						dwValueLen = sizeof(tszSubKeyValue);

						tszSubKeyName[0] = '\0';
						tszSubKeyValue[0] = '\0';

						lRetCode = RegEnumValue(hKeyProperty, dwPropValueCount, tszSubKeyName, &dwNameLen, NULL, &dwType, (LPBYTE)tszSubKeyValue, &dwValueLen);
						if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
						{
							tszSubKeyName[0] = '\0';
							tszSubKeyValue[0] = '\0';
						}
						else
						{
							tszValue[0] = '\0';
							switch( dwType )
							{
								case REG_BINARY:
								{
									if( tszSubKeyValue && _tcslen(tszSubKeyValue) > 0 )
									{
										dwCount = 0;
										while( dwCount < dwValueLen )
										{
											_stprintf_s(tszValue, _countof(tszValue), _T("%s%c"), tszValue, *(tszSubKeyValue + dwCount));
											dwCount++;
										}
									}
									break;
								}
								case REG_SZ:
								{
									_tcscpy_s(tszValue, _countof(tszValue), tszSubKeyValue);
									break;
								}
								case REG_MULTI_SZ:
								{
									_tcscpy_s(tszValue, _countof(tszValue), tszSubKeyValue);
									break;
								}
								default:
									break;
							}
						}

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_DISPLAYNAME_NAME) == 0 )
							_tcscpy_s(tszDisplayName, _countof(tszDisplayName), tszValue);

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_INSTALLSOURCE_NAME) == 0 )
							_tcscpy_s(tszInstallSource, _countof(tszInstallSource), tszValue);

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_UNINSTALLSTRING_NAME) == 0 )
							_tcscpy_s(tszUninstallString, _countof(tszUninstallString), tszValue);

						dwPropValueCount++;
					}

					if( tszDisplayName && _tcslen(tszDisplayName) > 0 )
					{
						bIsAdd = true;
						for( int i = 0; i < m_sInstallSwInfoArray.GetCount(); i++ )
						{
							if( _tcscmp(m_sInstallSwInfoArray.At(i)->m_tszDisplayName, tszDisplayName) == 0 )
							{
								bIsAdd = false;
								break;
							}
						}

						if( bIsAdd )
						{
							pInstallSwInfo = new INSTALL_SWINFO;

							_tcscpy_s(pInstallSwInfo->m_tszDisplayName, _countof(pInstallSwInfo->m_tszDisplayName), tszDisplayName);
							_tcscpy_s(pInstallSwInfo->m_tszInstallSource, _countof(pInstallSwInfo->m_tszInstallSource), tszInstallSource);
							_tcscpy_s(pInstallSwInfo->m_tszUninstallString, _countof(pInstallSwInfo->m_tszUninstallString), tszUninstallString);

							m_sInstallSwInfoArray.Add(pInstallSwInfo);
						}
					}
				}

				dwIndexEnum++;
				dwNameLen = sizeof(tszSubKeyName);
			}
		}

		RegCloseKey(hKeyProperty);
	}

	RegCloseKey(hSubKey);

	return true;
}
