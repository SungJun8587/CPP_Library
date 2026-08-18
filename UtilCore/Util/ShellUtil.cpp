
//***************************************************************************
// ShellUtil.cpp : implementation of the ShellUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "ShellUtil.h"

//***************************************************************************
// @brief 파일 확장자가 필터 조건(멀티 확장자 포함)에 일치하는지 확인하는 함수
// @param ptszFilePath 검사할 파일의 전체 경로 또는 확장자 문자열
// @param pExtFilter   구분자(;)로 분리된 확장자 필터 문자열 (예: "txt;log;csv")
// @return 필터 목록에 존재하면 true, 없으면 false
//***************************************************************************
bool IsMatchedExtension(const TCHAR* ptszFilePath, const TCHAR* pExtFilter)
{
	if( pExtFilter == nullptr || pExtFilter[0] == _T('\0') )
	{
		return false;
	}

	// 전체 허용 와일드카드 처리
	if( _tcsicmp(pExtFilter, _T("*")) == 0 || _tcsicmp(pExtFilter, _T("*.*")) == 0 )
	{
		return true;
	}

	// 파일 경로에서 확장자 추출
	TCHAR szExt[64] = { 0, };
	const TCHAR* pDot = _tcsrchr(ptszFilePath, _T('.'));
	if( pDot != nullptr )
	{
		_tcscpy_s(szExt, _countof(szExt), pDot + 1);
	}
	else
	{
		_tcscpy_s(szExt, _countof(szExt), _T(""));
	}

	// 세미콜론(;)으로 구분된 확장자 목록 비교
	_tstring strFilter(pExtFilter);
	std::basic_stringstream<_TCHAR> ss(strFilter);
	_tstring item;

	while( std::getline(ss, item, _T(';')) )
	{
		item.erase(0, item.find_first_not_of(_T(" \t")));
		item.erase(item.find_last_not_of(_T(" \t")) + 1);

		if( !item.empty() && item[0] == _T('.') )
		{
			item = item.substr(1);
		}

		if( _tcsicmp(item.c_str(), szExt) == 0 )
		{
			return true; // 일치함 발견
		}
	}

	return false; // 일치함 없음
}

//***************************************************************************
// @brief 파일이 필터 정책(화이트리스트/블랙리스트)에 적합한지 최종 판정하는 함수
// @param ptszFilePath 검사할 파일의 전체 경로
// @param ShApplyFileInfo 파일 필터링 옵션 구조체
//                        * m_nFilterMode 의미:
//                          - 0 : 필터링 없음 (전체 허용)
//                          - 1 : 화이트리스트 (지정한 확장자만 허용)
//                          - 2 : 블랙리스트 (지정한 확장자는 비허용/제외)
// @return 허용 대상이면 true, 제외 대상이면 false
//***************************************************************************
bool IsAbleFile(const TCHAR* ptszFilePath, const SH_APPLY_FILEINFO& ShApplyFileInfo)
{
	// 0번 모드: 필터링 없음 (전체 허용)
	if( ShApplyFileInfo.m_nFilterMode == 0 )
		return true;

	// 확장자가 필터 목록에 포함되어 있는지 여부 확인
	bool bIsMatched = IsMatchedExtension(ptszFilePath, ShApplyFileInfo.m_tszApplyExt);

	if( ShApplyFileInfo.m_nFilterMode == 1 )
	{
		// 1번 모드 (화이트리스트): 목록에 "있어야만" 허용 (포함되어야 true)
		return bIsMatched;
	}
	else if( ShApplyFileInfo.m_nFilterMode == 2 )
	{
		// 2번 모드 (블랙리스트): 목록에 "있으면" 차단 (포함되어 있으면 false, 없어야 true)
		return !bIsMatched;
	}

	return true;
}

//***************************************************************************
// @brief 지정한 폴더 경로를 하위 폴더까지 재귀적으로 생성하는 함수
// @param ptszFolder 생성할 전체 폴더 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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

	// 경로 문자열 뒤에서부터 역슬래시나 슬래시 위치를 찾음
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

	// 상위 폴더가 존재하지 않는 경우 재귀적으로 상위 폴더 먼저 생성
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
// @brief 지정한 디렉토리와 그 하위의 모든 파일 및 폴더를 재귀적으로 삭제하는 함수
// @param ptszFolder 삭제할 대상 폴더 경로
// @param bSelfDel 최상위 폴더 자체까지 삭제할 것인지 여부
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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

	// 경로 끝에 슬래시(\ 또는 /)가 없으면 추가하여 검색 패턴 정규화
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

	// 하위 파일 및 폴더 탐색 및 삭제 수행
	if( INVALID_HANDLE_VALUE != hFindFile )
	{
		while( bResult )
		{
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// 현재 디렉토리(.)와 상위 디렉토리(..)는 건너뜀
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
				// 파일인 경우 개별 파일 삭제
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
// @brief 원본 폴더의 파일 및 하위 폴더들을 대상 폴더로 재귀적으로 복사하는 함수
// @param ptszSourceFolder 원본 폴더 경로
// @param ptszDestFolder 대상 폴더 경로
// @param ShApplyFileInfo 파일 필터링 옵션 구조체
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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

	// 경로 끝 문자 처리 정규화
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

	// 파일 및 디렉토리 순회 복사 수행
	if( INVALID_HANDLE_VALUE != hFindFile )
	{
		while( bResult )
		{
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// 하위 디렉토리인 경우 대상 측에도 폴더 생성 후 재귀 호출
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
				// 파일인 경우 필터 조건을 만족할 때만 복사 수행
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
// @brief 원본 폴더의 파일 및 하위 폴더들을 대상 폴더로 재귀적으로 이동하는 함수
// @param ptszSourceFolder 원본 폴더 경로
// @param ptszDestFolder 대상 폴더 경로
// @param ShApplyFileInfo 파일 필터링 옵션 구조체
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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

	// 파일 및 디렉토리 순회 이동 수행
	if( INVALID_HANDLE_VALUE != hFindFile )
	{
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
				// 파일인 경우 필터 조건 만족 시 MoveFile 수행
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
// @brief 지정한 경로가 유효한 디렉토리인지 확인하는 함수
// @param ptszFolder 검사할 폴더 경로
// @return 디렉토리 존재 시 true, 아니면 false
//***************************************************************************
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

	if( INVALID_HANDLE_VALUE == hFindFile )
		bResult = false;
	else bResult = true;

	FindClose(hFindFile);

	return bResult;
}

//***************************************************************************
// @brief 레지스트리 키를 하위 경로까지 재귀적으로 생성 또는 오픈하는 함수
// @param hRoot 루트 키 핸들 (예: HKEY_LOCAL_MACHINE)
// @param ptszSubKey 생성할 서브 키 경로
// @param bReadOnly 읽기 전용 모드 여부
// @return 성공 시 ERROR_SUCCESS (0), 실패 시 에러 코드
//***************************************************************************
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
				delete[]ptszActiveSubKey;
				ptszActiveSubKey = nullptr;

				RegCloseKey(hKey);

				return lRetCode;
			}

			delete[]ptszActiveSubKey;
			ptszActiveSubKey = nullptr;
		}
	}

	lRetCode = RegCreateKeyEx(hRoot, ptszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		nullptr, &hKey, &dwDisposition);

	RegCloseKey(hKey);

	return lRetCode;
}

//***************************************************************************
// @brief 레지스트리 키와 그 하위 키들을 재귀적으로 삭제하는 함수
// @param hKey 부모 레지스트리 키 핸들
// @param ptszSubKey 삭제할 서브 키 이름
// @return 성공 시 ERROR_SUCCESS (0), 실패 시 에러 코드
//***************************************************************************
long RegDeleteKeyRecursive(const HKEY hKey, const TCHAR* ptszSubKey)
{
	long	lRetCode = 0;
	DWORD	dwSize = 0;
	TCHAR	szNewSubKey[REGISTRY_KEY_STRLEN];

	HKEY	newKey;

	FILETIME	FileTime;

	lRetCode = RegOpenKeyEx(hKey, ptszSubKey, 0, KEY_ALL_ACCESS, &newKey);
	if( lRetCode != ERROR_SUCCESS ) return lRetCode;

	// 하위 키가 존재할 경우 반복해서 재귀 삭제
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
// @brief 지정한 레지스트리 값(Value)에 데이터를 기록하는 함수 (일반 포인터형)
//***************************************************************************
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
// @brief 문자열(REG_SZ) 형태의 레지스트리 값을 기록하는 함수
//***************************************************************************
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
// @brief 숫자(REG_DWORD) 형태의 레지스트리 값을 기록하는 함수
//***************************************************************************
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
// @brief 레지스트리 문자열(REG_SZ) 값의 바이트 길이를 조회하는 함수
//***************************************************************************
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
// @brief 레지스트리 값을 읽어오는 함수 (일반 버퍼형)
//***************************************************************************
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
// @brief 레지스트리 바이트 배열 값을 읽어오는 함수
//***************************************************************************
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
// @brief 레지스트리 숫자(DWORD) 값을 읽어오는 함수
//***************************************************************************
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
// @brief 지정한 레지스트리 키가 존재하는지 확인하는 함수
//***************************************************************************
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
// @brief 동일한 이름의 파일이 존재할 경우 인덱스를 붙여 중복되지 않는 파일 핸들을 생성하는 함수
// @param ptszDestFullPath 생성된 최종 파일의 전체 경로를 반환받을 버퍼
// @param ptszDestFileNameExt 생성된 최종 파일명을 반환받을 버퍼
// @param ptszFullPath 원본 파일 전체 경로
// @return 성공 시 파일 핸들(HANDLE), 실패 시 NULL
//***************************************************************************
HANDLE GetFileHandleDuplicate(TCHAR* ptszDestFullPath, TCHAR* ptszDestFileNameExt, const TCHAR* ptszFullPath)
{
	int		i = 0;
	TCHAR	tszFolderPath[FULLPATH_STRLEN];
	TCHAR	tszTempFullPath[DIRECTORY_STRLEN + FILENAME_STRLEN];
	TCHAR   tszTempFileNameExt[FILENAMEEXT_STRLEN];

	HANDLE	hFile;

	_tstring folderPath, fileNameExt;
	_tstring fileName, fileExt;

	folderPath = FolderPathPassing(ptszFullPath);
	fileNameExt = FileNameExtPathPassing(ptszFullPath);
	FileNameExtPassing(fileNameExt, fileName, fileExt);

	CreateDirectoryRecursive(tszFolderPath);

	_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), tszFolderPath, fileNameExt.c_str());
	_sntprintf_s(tszTempFileNameExt, _countof(tszTempFileNameExt), _TRUNCATE, _T("%s.%s"), fileName.c_str(), fileExt.c_str());

	// 최초 파일 생성 시도 (동일 파일이 없으면 성공)
	hFile = CreateFile(tszTempFullPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);

		tszTempFullPath[0] = '\0';
		tszTempFileNameExt[0] = '\0';

		// 파일이 이미 존재하면 (1), (2) 형태로 인덱스를 붙여가며 생성 재시도
		for( i = 1; i < MAX_FILENAME_CONVERT_INDEX_NUM; i++ )
		{
			_sntprintf_s(tszTempFileNameExt, _countof(tszTempFileNameExt), _TRUNCATE, _T("%s(%d).%s"), fileName.c_str(), i, fileExt.c_str());
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
// @brief 레지스트리에 저장된 디지털 제품 ID(DigitalProductID)를 디코딩하여 윈도우 정품 인증키(Product Key)를 추출하는 함수
// @param TProductKey 추출된 제품 키 문자열을 저장할 참조 변수
// @param pbDigitalProductID 레지스트리에서 읽어온 원본 바이트 배열
// @param dwLength 데이터 바이트 길이
// @param bIsExtractBytesRange 바이트 추출 범위 플래그 (Windows 7 이전 vs Windows 8/10/11 이상 구분)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool GetProductKeyExtract(_tstring& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const bool bIsExtractBytesRange)
{
	int		nKeyStartIndex = 0;
	int		nKeyEndIndex = 0;
	int		nIsContainsN = 0;
	BYTE	bProductKeyExtract[16];
	BYTE* pbSrcDigitalProductID = nullptr;
	TCHAR* ptszDecodedChars = nullptr;

	// 제품 키 디코딩에 사용되는 허용 문자 맵 테이블 (Base24)
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

	// Windows 버전별 오프셋 인덱스 설정 (Win8 이상은 808 바이트, 그 이하는 52 바이트)
	if( bIsExtractBytesRange )
		nKeyStartIndex = 808;
	else nKeyStartIndex = 52;

	nKeyEndIndex = nKeyStartIndex + 15;

	// Windows 8 / Office 2013 이상 스타일 키 판별 ('N' 문자가 포함될 수 있는지 확인)
	nIsContainsN = (pbSrcDigitalProductID[nKeyStartIndex + 14] >> 3) & 1;
	pbSrcDigitalProductID[nKeyStartIndex + 14] = (BYTE)((pbSrcDigitalProductID[nKeyStartIndex + 14] & 0xF7) | ((nIsContainsN & 2) << 2));

	for( int i = nKeyStartIndex; i <= nKeyEndIndex; i++ )
		bProductKeyExtract[i - nKeyStartIndex] = pbSrcDigitalProductID[i];
	bProductKeyExtract[15] = '\0';

	delete[] pbSrcDigitalProductID;

	ptszDecodedChars = new TCHAR[nDecodeLength + 1];
	for( int i = nDecodeLength - 1; i >= 0; i-- )
	{
		// 5자리마다 하이픈(-) 삽입 위치 지정
		if( (i + 1) % 6 == 0 )
		{
			ptszDecodedChars[i] = _T('-');
		}
		else
		{
			// 실제 Base24 알고리즘 디코딩 수행
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
	ptszDecodedChars[nDecodeLength] = _T('\0');

	// 'N' 문자가 포함된 최신 OS 키 형식인 경우, 올바른 위치에 'N'을 재배치
	if( nIsContainsN != 0 )
	{
		int nFirstLetterIndex = 0;

		for( int k = 0; k < nNumLetters; k++ )
		{
			if( ptszDecodedChars[0] != ptszKeyChars[k] ) continue;
			nFirstLetterIndex = k;
			break;
		}

		_tstring strReplace;
		_tstring strTemp = ptszDecodedChars + 1;
		for( TCHAR ch : strTemp )
		{
			if( ch != _T('-') ) strReplace += ch;
		}

		_tstring strMid01 = strReplace.substr(1, nFirstLetterIndex);
		_tstring strMid02 = strReplace.substr(nFirstLetterIndex + 1);
		_tstring strAppend01 = strMid01 + _T("N");
		_tstring strAppend02 = strAppend01 + strMid02;

		_tstring strFinal;
		for( size_t i = 0; i < strAppend02.length(); ++i )
		{
			if( i > 0 && i % 5 == 0 )
			{
				strFinal += _T('-');
			}
			strFinal += strAppend02[i];
		}
		TProductKey = strFinal;
	}
	else
	{
		TProductKey = ptszDecodedChars;
	}

	if( ptszDecodedChars )
	{
		delete[]ptszDecodedChars;
		ptszDecodedChars = nullptr;
	}

	return true;
}