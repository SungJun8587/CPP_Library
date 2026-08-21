
//***************************************************************************
// SoftwareInfo.cpp: implementation of the Software Information Class.
//
//***************************************************************************

#include "pch.h"
#include "SoftwareInfo.h"

//***************************************************************************
// @brief   지정된 파일의 리소스 정보에서 버전 및 언어 정보를 추출합니다.
// @param   ptszAppName 버전을 조회할 파일의 경로
// @param   ptszVersion 추출된 버전 문자열을 전달받을 버퍼 포인터
// @param   ptszLanguage 추출된 언어 명칭 문자열을 전달받을 버퍼 포인터
// @return  BOOL 추출 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL GetVersionLangOfFile(TCHAR* ptszAppName, TCHAR* ptszVersion, TCHAR* ptszLanguage)
{
	BOOL		bResult = false;
	DWORD		dwScratch = 0;
	DWORD		dwInfSize = 0;
	DWORD* pdwLangChar;
	UINT		uSize = 0;
	BYTE* pbInfBuff = NULL;
	TCHAR		tszResource[MAX_BUFFER_SIZE];
	TCHAR* ptszTempVersion = NULL;

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
					// [수정] VerLanguageName의 3번째 인자(cchLangName)는 문자 개수를 요구합니다.
					// sizeof(tszResource)(바이트)를 넘기면 UNICODE 빌드에서 실제 버퍼 크기의
					// 2배 값이 전달되어 오버플로우 위험이 있습니다.
					if( VerLanguageName(LOWORD(*pdwLangChar), tszResource, _countof(tszResource)) )
						_tcsncpy_s(ptszLanguage, MAX_BUFFER_SIZE, tszResource, _TRUNCATE);

					_stprintf_s(tszResource, _countof(tszResource), _T("\\StringFileInfo\\%04X%04X\\FileVersion"), LOWORD(*pdwLangChar), HIWORD(*pdwLangChar));

					if( VerQueryValue(pbInfBuff, tszResource, (void**)(&ptszTempVersion), &uSize) )
						_tcsncpy_s(ptszVersion, MAX_BUFFER_SIZE, ptszTempVersion, _TRUNCATE);

					bResult = true;
				}
			}

			delete[]pbInfBuff;
		}
	}

	return bResult;
}

//***************************************************************************
// @brief   CIeInfo 클래스 생성자
//***************************************************************************
CIeInfo::CIeInfo()
{
	ZeroMemory(&m_Ie, sizeof(SWINFO_IE));
}

//***************************************************************************
// @brief   CIeInfo 클래스 소멸자
//***************************************************************************
CIeInfo::~CIeInfo()
{
}

//***************************************************************************
// @brief   레지스트리를 조회하여 Internet Explorer의 버전 및 빌드 정보를 수집합니다.
// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CIeInfo::GetInformation()
{
	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	long	lRetCode = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszBuild[IE_BUILD_STRLEN];
	TCHAR	tszVersion[IE_VERSION_STRLEN];

	HKEY	hKeyIE;

	// [수정] RegOpenKeyEx가 실패해 아래 두 분기의 내부 if 블록이 전혀 실행되지 않는
	// 경우(레지스트리 키가 없는 시스템 등), tszBuild/tszVersion이 초기화되지 않은
	// 채로 이후 _tcsncpy_s(m_Ie...)에 그대로 복사되던 버그를 방지하기 위해
	// 분기 진입 전에 기본값으로 초기화합니다.
	_tcsncpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"), _TRUNCATE);
	_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);

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
			// [수정] 기존 조건 `!((lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN))`은
			// 드모르간 전개 시 "성공 && 길이정상"일 때 "UnKnown"으로 덮어쓰는 반대 논리였습니다.
			// 아래 NT 분기와 동일하게 "실패 시에만" UnKnown으로 대체하도록 부정(!)을 제거했습니다.
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcsncpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"), _TRUNCATE);

			dwNameLen = sizeof(WIN_IE_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			tszVersion[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, WIN_IE_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);

			// [수정] 기존에는 이 분기에서 hKeyIE를 닫는 코드가 아예 없어 핸들이 누수되었습니다.
			RegCloseKey(hKeyIE);
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
				_tcsncpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"), _TRUNCATE);

			dwNameLen = sizeof(NT_IE_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			tszVersion[0] = '\0';

			lRetCode = RegQueryValueEx(hKeyIE, NT_IE_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);

			// [수정] RegOpenKeyEx 실패 시(else) hKeyIE가 초기화되지 않은 채 RegCloseKey에
			// 전달되던 버그를 막기 위해, 성공한 경우에만 닫도록 스코프를 좁혔습니다.
			RegCloseKey(hKeyIE);
		}
	}
	else
	{
		_tcsncpy_s(tszBuild, _countof(tszBuild), _T("UnKnown"), _TRUNCATE);
		_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);
	}

	_tcsncpy_s(m_Ie.m_tszBuild, _countof(m_Ie.m_tszBuild), tszBuild, _TRUNCATE);
	_tcsncpy_s(m_Ie.m_tszVersion, _countof(m_Ie.m_tszVersion), tszVersion, _TRUNCATE);

	return true;
}

//***************************************************************************
// @brief   CDirectXInfo 클래스 생성자
//***************************************************************************
CDirectXInfo::CDirectXInfo()
{
	ZeroMemory(&m_DirectX, sizeof(SWINFO_DIRECTX));
}

//***************************************************************************
// @brief   CDirectXInfo 클래스 소멸자
//***************************************************************************
CDirectXInfo::~CDirectXInfo()
{
}

//***************************************************************************
// @brief   레지스트리를 탐색하여 시스템에 설치된 DirectX 정보 및 버전을 수집합니다.
// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CDirectXInfo::GetInformation()
{
	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	long	lRetCode = 0;
	__int64 qwInstallVersion = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszVersion[DIRECTX_VERSION_STRLEN];
	TCHAR	tszInstallVersion[DIRECTX_INSTALLVERSION_STRLEN];
	TCHAR	tszDescription[DIRECTX_DESCRIPTION_STRLEN];

	HKEY	hKeyDirectX;

	// [수정] RegOpenKeyEx가 실패해 두 분기의 내부 if 블록이 전혀 실행되지 않는 경우
	// tszVersion/tszInstallVersion이 초기화되지 않은 채로 이후 _tcscmp/_tcscpy_s에
	// 사용되던 버그를 방지하기 위해 분기 진입 전에 기본값으로 초기화합니다.
	tszVersion[0] = '\0';
	_tcsncpy_s(tszInstallVersion, _countof(tszInstallVersion), _T("UnKnown"), _TRUNCATE);

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
			else _tcsncpy_s(tszInstallVersion, _countof(tszInstallVersion), _T("UnKnown"), _TRUNCATE);

			dwNameLen = sizeof(WIN_DIRECTX_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, WIN_DIRECTX_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);

			// [수정] RegOpenKeyEx 실패 시 초기화되지 않은 hKeyDirectX가 RegCloseKey에
			// 전달되던 버그를 막기 위해 성공한 경우에만 닫도록 스코프를 좁혔습니다.
			RegCloseKey(hKeyDirectX);
		}
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), NT_MICROSOFT_KEY, NT_DIRECTX_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyDirectX);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = sizeof(NT_DIRECTX_INSTALLVER_NAME);
			dwValueLen = sizeof(qwInstallVersion);

			// [수정] NT 분기인데 WIN(9x)용 상수 WIN_DIRECTX_INSTALLVER_NAME을 조회하던
			// 복사-붙여넣기 실수를 NT_DIRECTX_INSTALLVER_NAME으로 바로잡았습니다.
			lRetCode = RegQueryValueEx(hKeyDirectX, NT_DIRECTX_INSTALLVER_NAME, NULL, &dwNameLen, (LPBYTE)&qwInstallVersion, &dwValueLen);
			if( lRetCode == ERROR_SUCCESS )
			{
				_stprintf_s(tszInstallVersion, _countof(tszInstallVersion), _T("%d.%d.%d.%d"), LOBYTE(LOWORD(qwInstallVersion)),
					HIBYTE(LOWORD(qwInstallVersion)),
					LOBYTE(HIWORD(qwInstallVersion)),
					HIBYTE(HIWORD(qwInstallVersion)));
			}
			else _tcsncpy_s(tszInstallVersion, _countof(tszInstallVersion), _T("UnKnown"), _TRUNCATE);

			dwNameLen = sizeof(NT_DIRECTX_VERSION_NAME);
			dwValueLen = sizeof(tszVersion);

			lRetCode = RegQueryValueEx(hKeyDirectX, NT_DIRECTX_VERSION_NAME, NULL, &dwNameLen, (LPBYTE)tszVersion, &dwValueLen);
			if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) )
				_tcsncpy_s(tszVersion, _countof(tszVersion), _T("UnKnown"), _TRUNCATE);

			RegCloseKey(hKeyDirectX);
		}
	}

	tszDescription[0] = '\0';
	if( _tcscmp(tszVersion, _T("4.09.00.0900")) == 0 )
		_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0"), _TRUNCATE);
	else if( _tcscmp(tszVersion, _T("4.09.00.0901")) == 0 )
		_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0a"), _TRUNCATE);
	else if( _tcscmp(tszVersion, _T("4.09.00.0902")) == 0 )
		_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0b"), _TRUNCATE);
	else if( _tcscmp(tszVersion, _T("4.09.00.0903")) == 0 )
		_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0c"), _TRUNCATE);
	else if( _tcscmp(tszVersion, _T("4.09.00.0904")) == 0 )
	{
		if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
		{
			if( IsWindowVersion(5, -1, -1) )
				_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 9.0c"), _TRUNCATE);
			else if( IsWindowVersion(6, 0, -1) )
				_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 10"), _TRUNCATE);
			else if( IsWindowVersion(6, 1, -1) )
				_tcsncpy_s(tszDescription, _countof(tszDescription), _T("DirectX 11"), _TRUNCATE);
		}
	}

	_tcsncpy_s(m_DirectX.m_tszVersion, _countof(m_DirectX.m_tszVersion), tszVersion, _TRUNCATE);
	_tcsncpy_s(m_DirectX.m_tszInstallVersion, _countof(m_DirectX.m_tszInstallVersion), tszInstallVersion, _TRUNCATE);
	_tcsncpy_s(m_DirectX.m_tszDescription, _countof(m_DirectX.m_tszDescription), tszDescription, _TRUNCATE);

	return true;
}

//***************************************************************************
// @brief   CJavaVMInfo 클래스 생성자
//***************************************************************************
CJavaVMInfo::CJavaVMInfo()
{
	m_nIsJVM = 0;
}

//***************************************************************************
// @brief   CJavaVMInfo 클래스 소멸자
//***************************************************************************
CJavaVMInfo::~CJavaVMInfo()
{
}

//***************************************************************************
// @brief   레지스트리 및 파일 탐색을 통해 MS/Sun JVM 설치 여부를 검사하고 유형을 결정합니다.
// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CJavaVMInfo::GetInformation()
{
	BOOL	bIsSunJVM = false;
	BOOL	bIsMsJVM = false;
	BOOL	bIsMCompany = false;
	BOOL	bIsSunCompany = false;

	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	DWORD	dwIndexEnum = 0;
	long	lRetCode = 0;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszGroupName[REGISTRY_NAME_STRLEN];
	TCHAR	tszMsJVMRuntimeLibPath[FULLPATH_STRLEN];
	TCHAR	tszSunJVMRuntimeLibPath[FULLPATH_STRLEN];

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

			// [수정] RegOpenKeyEx 실패 시 초기화되지 않은 hKeyJavaVm이 RegCloseKey에
			// 전달되던 버그를 막기 위해 성공한 경우에만 닫도록 스코프를 좁혔습니다.
			RegCloseKey(hKeyJavaVm);
		}

		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), WIN_SUN_JAVAVM_JRE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			// [수정] RegEnumKeyEx의 lpcchName은 문자 개수를 요구합니다. sizeof()(바이트)를
			// 넘기면 UNICODE 빌드에서 실제 버퍼보다 큰 크기를 알려주게 되어 오버플로우
			// 위험이 있습니다.
			dwNameLen = _countof(tszGroupName);

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
						// [수정] 기존에는 여기서 바로 continue하여 아래쪽의 dwIndexEnum++를
						// 건너뛰었고, 그 결과 같은 서브키를 무한히 재열거하는 행 위험이
						// 있었습니다. continue 전에 인덱스를 반드시 증가시킵니다.
						dwIndexEnum++;
						dwNameLen = _countof(tszGroupName);
						continue;
					}

					RegCloseKey(hKeyJavaVm);
					bIsSunCompany = true;
				}

				dwIndexEnum++;
				dwNameLen = _countof(tszGroupName);
			}

			RegCloseKey(hKeyEnum);
		}
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

			RegCloseKey(hKeyJavaVm);
		}

		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), NT_SUN_JAVAVM_JRE_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = _countof(tszGroupName);

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
						dwIndexEnum++;
						dwNameLen = _countof(tszGroupName);
						continue;
					}

					RegCloseKey(hKeyJavaVm);
					bIsSunCompany = true;
				}

				dwIndexEnum++;
				dwNameLen = _countof(tszGroupName);
			}

			RegCloseKey(hKeyEnum);
		}
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
		// [수정] GetSystemDirectory의 2번째 인자는 문자 개수를 요구합니다.
		// 바로 아래 bIsSunCompany 블록은 이미 _countof()를 올바르게 쓰고 있어
		// 비일관성이 뚜렷했습니다.
		GetSystemDirectory(tszWindowSystemDir, _countof(tszWindowSystemDir));
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
			GetSystemDirectory(tszWindowSystemDir, _countof(tszWindowSystemDir));

			// CMemBuffer 대신 _tstring 활용
			_tstring strPath = tszSunJVMRuntimeLibPath;

			// ';' 구분자가 포함된 경우 첫 번째 경로만 추출 (기존 StrLeft 대체)
			size_t nPos = strPath.find(_T(';'));
			if( nPos != _tstring::npos )
			{
				strPath = strPath.substr(0, nPos);
			}

			// %systemroot% 문자열을 시스템 디렉터리 경로로 치환 (기존 StrReplace 대체)
			const _tstring strTarget = _T("%systemroot%");
			size_t nReplacePos = strPath.find(strTarget);
			if( nReplacePos != _tstring::npos )
			{
				strPath.replace(nReplacePos, strTarget.length(), tszWindowSystemDir);
			}

			hFindFile = FindFirstFile(strPath.c_str(), &FindData);

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
// @brief   Microsoft JVM 모듈 파일에서 버전을 구하여 전달합니다.
// @param   ptszMsJVMVersion 추출된 버전 정보를 저장할 문자열 버퍼
// @return  BOOL 구하기 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CJavaVMInfo::GetVersionMsJVM(TCHAR* ptszMsJVMVersion)
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
		GetSystemDirectory(tszWindowSystemDir, _countof(tszWindowSystemDir));
		_stprintf_s(tszRuntimeLibFilePath, _countof(tszRuntimeLibFilePath), _T("%s\\%s"), tszWindowSystemDir, tszRuntimeLibFileName);

		if( GetVersionLangOfFile(tszRuntimeLibFilePath, tszVersion, tszLanguage) )
		{
			if( _tcslen(tszVersion) < 1 )
			{
				ptszMsJVMVersion[0] = '\0';
				return false;
			}
			else _tcsncpy_s(ptszMsJVMVersion, JAVAVM_VERSION_STRLEN, tszVersion, _TRUNCATE);
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
// @brief   레지스트리 플러그인 키를 조회하여 Sun JVM의 설치 버전을 구합니다.
// @param   ptszSunJVMVersion 추출된 버전 정보를 저장할 문자열 버퍼
// @return  BOOL 구하기 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CJavaVMInfo::GetVersionSunJVM(TCHAR* ptszSunJVMVersion)
{
	HKEY	hKeyEnum;

	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;
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
			// [수정] RegEnumKeyEx의 lpcchName은 문자 개수 단위입니다.
			dwNameLen = _countof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				dwIndexEnum++;
				dwNameLen = _countof(tszGroupName);
			}

			// [수정] RegOpenKeyEx 실패 시 초기화되지 않은 hKeyEnum이 RegCloseKey에
			// 전달되던 버그를 막기 위해 성공한 경우에만 닫도록 스코프를 좁혔습니다.
			RegCloseKey(hKeyEnum);
		}
	}
	else if( IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_NT) )
	{
		_stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s"), NT_SUN_JAVAVM_PLUGIN_KEY);

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyEnum);
		if( lRetCode == ERROR_SUCCESS )
		{
			dwNameLen = _countof(tszGroupName);

			tszGroupName[0] = '\0';

			while( (lRetCode = RegEnumKeyEx(hKeyEnum, dwIndexEnum, tszGroupName, &dwNameLen, 0, NULL, 0, &MyFileTime)) == ERROR_SUCCESS )
			{
				dwIndexEnum++;
				dwNameLen = _countof(tszGroupName);
			}

			RegCloseKey(hKeyEnum);
		}
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
	else _tcsncpy_s(ptszSunJVMVersion, JAVAVM_VERSION_STRLEN, tszGroupName, _TRUNCATE);

	return true;
}

//***************************************************************************
// @brief   CInstallSwInfo 클래스 생성자
//***************************************************************************
CInstallSwInfo::CInstallSwInfo()
{
}

//***************************************************************************
// @brief   CInstallSwInfo 클래스 소멸자 (동적 할당된 설치 정보 메모리를 해제합니다)
//***************************************************************************
CInstallSwInfo::~CInstallSwInfo()
{
	INSTALL_SWINFO* pInstallSwInfo = NULL;

	for( size_t i = 0; i < m_sInstallSwInfoArray.size(); i++ )
	{
		pInstallSwInfo = m_sInstallSwInfoArray[i];

		if( pInstallSwInfo )
		{
			delete pInstallSwInfo;
			pInstallSwInfo = NULL;
		}
	}
	m_sInstallSwInfoArray.clear();
}

//***************************************************************************
// @brief   Windows 언인스톨 레지스트리를 스캔하여 설치된 소프트웨어 목록을 수집합니다.
// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
//***************************************************************************
BOOL CInstallSwInfo::GetInformation()
{
	HKEY	hSubKey;
	HKEY	hKeyProperty;

	bool	bIsAdd = true;

	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];

	TCHAR	tszSubKeyName[REGISTRY_NAME_STRLEN];
	TCHAR	tszSubKeyValue[REGISTRY_VALUE_STRLEN];

	TCHAR	tszValue[REGISTRY_VALUE_STRLEN];
	TCHAR	tszDisplayName[REGISTRY_VALUE_STRLEN];
	TCHAR	tszInstallSource[REGISTRY_VALUE_STRLEN];
	TCHAR	tszUninstallString[REGISTRY_VALUE_STRLEN];

	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;
	DWORD	dwIndexEnum = 0;
	DWORD	dwPropValueNumber = 0;
	DWORD	dwPropValueCount = 0;
	DWORD	dwType = 0;
	DWORD	dwCount = 0;
	long	lRetCode = 0;

	FILETIME	MyFileTime;

	INSTALL_SWINFO* pInstallSwInfo = NULL;

	lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_SOFTWARE_UNINSTALL_KEY, 0, KEY_READ, &hSubKey);
	if( lRetCode == ERROR_SUCCESS )
	{
		// [수정] RegEnumKeyEx의 lpcchName은 문자 개수 단위입니다.
		dwNameLen = _countof(tszSubKeyName);

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
						// [수정] RegEnumValue의 lpcchValueName도 문자 개수 단위입니다.
						dwNameLen = _countof(tszSubKeyName);
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
								// [수정] 기존에는 매 바이트마다 _stprintf_s(tszValue, "%s%c", tszValue, ...)로
								// tszValue 전체를 다시 포맷팅하여 O(N^2) 성능이었고, 목적지와 소스가
								// 같은 버퍼를 가리키는 sprintf 계열 호출은 표준상 정의되지 않은 동작입니다.
								// 바이트를 순회하며 직접 문자를 덧붙이는 단일 패스 방식으로 교체합니다.
								if( tszSubKeyValue && _tcslen(tszSubKeyValue) > 0 )
								{
									DWORD dwMaxCount = dwValueLen;
									if( dwMaxCount >= _countof(tszValue) ) dwMaxCount = _countof(tszValue) - 1;

									for( dwCount = 0; dwCount < dwMaxCount; dwCount++ )
									{
										tszValue[dwCount] = *(tszSubKeyValue + dwCount);
									}
									tszValue[dwMaxCount] = _T('\0');
								}
								break;
							}
							case REG_SZ:
							{
								_tcsncpy_s(tszValue, _countof(tszValue), tszSubKeyValue, _TRUNCATE);
								break;
							}
							case REG_MULTI_SZ:
							{
								_tcsncpy_s(tszValue, _countof(tszValue), tszSubKeyValue, _TRUNCATE);
								break;
							}
							default:
								break;
							}
						}

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_DISPLAYNAME_NAME) == 0 )
							_tcsncpy_s(tszDisplayName, _countof(tszDisplayName), tszValue, _TRUNCATE);

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_INSTALLSOURCE_NAME) == 0 )
							_tcsncpy_s(tszInstallSource, _countof(tszInstallSource), tszValue, _TRUNCATE);

						if( _tcscmp(tszSubKeyName, WIN_SOFTWARE_UNINSTALL_UNINSTALLSTRING_NAME) == 0 )
							_tcsncpy_s(tszUninstallString, _countof(tszUninstallString), tszValue, _TRUNCATE);

						dwPropValueCount++;
					}

					if( tszDisplayName && _tcslen(tszDisplayName) > 0 )
					{
						bIsAdd = true;
						for( size_t i = 0; i < m_sInstallSwInfoArray.size(); i++ )
						{
							if( _tcscmp(m_sInstallSwInfoArray[i]->m_tszDisplayName, tszDisplayName) == 0 )
							{
								bIsAdd = false;
								break;
							}
						}

						if( bIsAdd )
						{
							pInstallSwInfo = new INSTALL_SWINFO;

							_tcsncpy_s(pInstallSwInfo->m_tszDisplayName, _countof(pInstallSwInfo->m_tszDisplayName), tszDisplayName, _TRUNCATE);
							_tcsncpy_s(pInstallSwInfo->m_tszInstallSource, _countof(pInstallSwInfo->m_tszInstallSource), tszInstallSource, _TRUNCATE);
							_tcsncpy_s(pInstallSwInfo->m_tszUninstallString, _countof(pInstallSwInfo->m_tszUninstallString), tszUninstallString, _TRUNCATE);

							m_sInstallSwInfoArray.push_back(pInstallSwInfo);
						}
					}
				}

				// [수정] hKeyProperty는 이 while 반복마다 새로 열리므로, 기존처럼 루프 밖에서
				// 단 한 번만 닫으면 마지막을 제외한 모든 반복에서 핸들이 누수됩니다.
				// 사용이 끝나는 시점(반복마다)에 바로 닫습니다.
				RegCloseKey(hKeyProperty);
			}

			// [수정] 기존에는 이 두 줄이 위쪽 "if( lRetCode == ERROR_SUCCESS )"(hKeyProperty open)
			// 블록 안에만 있어서, 해당 하위 키의 RegOpenKeyEx가 실패하면(권한 문제 등)
			// dwIndexEnum이 증가하지 않고 dwNameLen도 재설정되지 않아 같은 서브키를
			// 무한히 재열거하는(RegEnumKeyEx 무한 루프) 행 위험이 있었습니다.
			// 이제 매 바깥쪽 반복마다 항상 실행되도록 if 블록 밖으로 옮겼습니다.
			dwIndexEnum++;
			dwNameLen = _countof(tszSubKeyName);
		}

		RegCloseKey(hSubKey);
	}
	else
	{
		// [수정] 기존에는 hSubKey를 여는 데 실패해도 아래쪽의 RegCloseKey(hSubKey)가
		// 도달하지 않아 문제는 없었지만(원본은 if 블록 밖에서 무조건 hSubKey를 닫았음),
		// 구조를 명확히 하기 위해 실패 처리를 명시적으로 분리했습니다.
	}

	return true;
}