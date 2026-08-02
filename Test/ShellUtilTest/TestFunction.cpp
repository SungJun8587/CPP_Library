
//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"

//***************************************************************************
//
bool SHDirectoryRecursive(const TCHAR* ptszSourceFolder, const TCHAR* ptszDestFolder, SH_APPLY_FILEINFO& ShApplyFileInfo, std::vector<SH_FILESYSTEM_INFO*>& MemBufferFrom, std::vector<SH_FILESYSTEM_INFO*>& MemBufferTo)
{
	BOOL		bResult = true;
	TCHAR		tszActiveFolder[DIRECTORY_STRLEN + 16];
	TCHAR		tszSourceFullPath[MAX_PATH];
	TCHAR		tszSourceFolder[DIRECTORY_STRLEN];
	TCHAR		tszDestFullPath[MAX_PATH];
	TCHAR		tszDestFolder[DIRECTORY_STRLEN];

	WIN32_FIND_DATA		FindData;
	HANDLE				hFindFile;

	SH_FILESYSTEM_INFO* pFileFrom = NULL;
	SH_FILESYSTEM_INFO* pFileTo = NULL;

	if( !ptszSourceFolder || !ptszDestFolder ) return false;
	if( _tcslen(ptszSourceFolder) < 1 || _tcslen(ptszDestFolder) < 1 ) return false;

	if( ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '/' && ptszSourceFolder[_tcslen(ptszSourceFolder) - 1] != '\\' )
	{
		_stprintf_s(tszSourceFolder, _countof(tszSourceFolder), _T("%s\\"), ptszSourceFolder);
		_stprintf_s(tszActiveFolder, _countof(tszActiveFolder), _T("%s\\*.*"), ptszSourceFolder);
	}
	else
	{
		_tcsncpy_s(tszSourceFolder, _countof(tszSourceFolder), ptszSourceFolder, _TRUNCATE);
		_stprintf_s(tszActiveFolder, _countof(tszActiveFolder), _T("%s*.*"), ptszSourceFolder);
	}

	if( ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '/' && ptszDestFolder[_tcslen(ptszDestFolder) - 1] != '\\' )
		_stprintf_s(tszDestFolder, _countof(tszDestFolder), _T("%s\\"), ptszDestFolder);
	else
		_tcsncpy_s(tszDestFolder, _countof(tszDestFolder), ptszDestFolder, _TRUNCATE);

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
					_stprintf_s(tszSourceFullPath, _countof(tszSourceFullPath), _T("%s%s"), tszSourceFolder, FindData.cFileName);
					_stprintf_s(tszDestFullPath, _countof(tszDestFullPath), _T("%s%s"), tszDestFolder, FindData.cFileName);

					SHDirectoryRecursive(tszSourceFullPath, tszDestFullPath, ShApplyFileInfo, MemBufferFrom, MemBufferTo);
				}
			}
			else
			{
				_stprintf_s(tszSourceFullPath, _countof(tszSourceFullPath), _T("%s%s"), tszSourceFolder, FindData.cFileName);
				_stprintf_s(tszDestFullPath, _countof(tszDestFullPath), _T("%s%s"), tszDestFolder, FindData.cFileName);

				if( IsAbleFile(tszSourceFullPath, ShApplyFileInfo) )
				{
					pFileFrom = new SH_FILESYSTEM_INFO;

					_tcsncpy_s(pFileFrom->m_tszFullPath, _countof(pFileFrom->m_tszFullPath), tszSourceFullPath, _TRUNCATE);
					_tcsncpy_s(pFileFrom->m_tszFolder, _countof(pFileFrom->m_tszFolder), tszSourceFolder, _TRUNCATE);
					_tcsncpy_s(pFileFrom->m_tszFileNameExt, _countof(pFileFrom->m_tszFileNameExt), FindData.cFileName, _TRUNCATE);

					MemBufferFrom.push_back(pFileFrom);

					pFileTo = new SH_FILESYSTEM_INFO;

					_tcsncpy_s(pFileTo->m_tszFullPath, _countof(pFileTo->m_tszFullPath), tszDestFullPath, _TRUNCATE);
					_tcsncpy_s(pFileTo->m_tszFolder, _countof(pFileTo->m_tszFolder), tszDestFolder, _TRUNCATE);
					_tcsncpy_s(pFileTo->m_tszFileNameExt, _countof(pFileTo->m_tszFileNameExt), FindData.cFileName, _TRUNCATE);

					MemBufferTo.push_back(pFileTo);
				}
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
bool GetRegistryRecursive(HKEY hKey, TCHAR* ptszFullSubKey, TCHAR* ptszSubKey, std::vector<SH_REGISTRY_INFO*>& MemBuffer)
{
	long	lRetCode = 0;
	DWORD	dwSubKeyNumber = 0;
	DWORD	dwValueNumber = 0;
	DWORD	dwSubKeyLen = 0;
	DWORD	dwType = 0;
	DWORD	dwNameLen = 0;
	DWORD	dwValueLen = 0;

	TCHAR		tszFullSubKey[REGISTRY_KEY_STRLEN];
	TCHAR		tszSubKey[REGISTRY_KEY_STRLEN];
	TCHAR		tszName[REGISTRY_NAME_STRLEN];
	BYTE* pbValue = NULL;

	SH_REGISTRY_INFO* pShRegistryInfo = NULL;

	HKEY	hSubKey;

	lRetCode = RegOpenKeyEx(hKey, ptszSubKey, 0, KEY_READ, &hSubKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	lRetCode = RegQueryInfoKey(hSubKey, nullptr, nullptr, nullptr, &dwSubKeyNumber, nullptr, nullptr, &dwValueNumber, nullptr, nullptr, nullptr, NULL);
	if( lRetCode != ERROR_SUCCESS ) return false;

	if( _tcslen(ptszSubKey) > 0 && dwValueNumber == 0 )
	{
		pShRegistryInfo = new SH_REGISTRY_INFO;

		_tcsncpy_s(pShRegistryInfo->m_tszFullPathKey, _countof(pShRegistryInfo->m_tszFullPathKey), ptszFullSubKey, _TRUNCATE);
		_tcsncpy_s(pShRegistryInfo->m_tszSubPathKey, _countof(pShRegistryInfo->m_tszSubPathKey), ptszSubKey, _TRUNCATE);
		memset(pShRegistryInfo->m_tszName, 0, sizeof(pShRegistryInfo->m_tszName));
		pShRegistryInfo->m_pbValue = NULL;

		pShRegistryInfo->m_dwType = 0;
		pShRegistryInfo->m_dwNameLen = 0;
		pShRegistryInfo->m_dwValueLen = 0;

		MemBuffer.push_back(pShRegistryInfo);
	}

	for( int i = 0; i < (int)dwValueNumber; i++ )
	{
		dwType = 0;
		dwNameLen = REGISTRY_NAME_STRLEN;
		lRetCode = RegEnumValue(hSubKey, i, tszName, &dwNameLen, nullptr, &dwType, nullptr, &dwValueLen);
		if( lRetCode != ERROR_SUCCESS ) break;

		pbValue = new BYTE[dwValueLen];

		dwNameLen = REGISTRY_NAME_STRLEN;
		lRetCode = RegEnumValue(hSubKey, i, tszName, &dwNameLen, nullptr, &dwType, pbValue, &dwValueLen);
		if( lRetCode != ERROR_SUCCESS ) break;

		pShRegistryInfo = new SH_REGISTRY_INFO;

		_tcsncpy_s(pShRegistryInfo->m_tszFullPathKey, _countof(pShRegistryInfo->m_tszFullPathKey), ptszFullSubKey, _TRUNCATE);
		_tcsncpy_s(pShRegistryInfo->m_tszSubPathKey, _countof(pShRegistryInfo->m_tszSubPathKey), ptszSubKey, _TRUNCATE);
		_tcsncpy_s(pShRegistryInfo->m_tszName, _countof(pShRegistryInfo->m_tszName), tszName, _TRUNCATE);
		pShRegistryInfo->m_pbValue = pbValue;

		pShRegistryInfo->m_dwType = dwType;
		pShRegistryInfo->m_dwNameLen = dwNameLen;
		pShRegistryInfo->m_dwValueLen = dwValueLen;

		MemBuffer.push_back(pShRegistryInfo);
	}

	for( int i = 0; i < (int)dwSubKeyNumber; i++ )
	{
		dwSubKeyLen = REGISTRY_KEY_STRLEN;

		lRetCode = RegEnumKeyEx(hSubKey, i, tszSubKey, &dwSubKeyLen, nullptr, nullptr, nullptr, NULL);
		if( lRetCode != ERROR_SUCCESS ) break;

		_stprintf_s(tszFullSubKey, _countof(tszFullSubKey), _T("%s\\%s"), ptszFullSubKey, tszSubKey);
		GetRegistryRecursive(hSubKey, tszFullSubKey, tszSubKey, MemBuffer);
	}

	RegCloseKey(hSubKey);

	return true;
}

//***************************************************************************
// 1. 디렉터리 및 파일 제어 테스트
//***************************************************************************
void DirectoryAndFileOperations()
{
	_tprintf_s(_T("\n=== [1] 디렉터리 및 파일 제어 테스트 시작 ===\r\n"));

	LPCTSTR testDir = _T("C:\\TestRoot\\SubFolder\\Data");
	LPCTSTR destDir = _T("C:\\TestDest");

	// 재귀 디렉터리 생성 테스트
	if( CreateDirectoryRecursive(testDir) )
	{
		_tprintf_s(_T("[성공] 디렉터리 생성 완료: %s\r\n"), testDir);
	}
	else
	{
		_tprintf_s(_T("[실패] 디렉터리 생성 실패: %s\r\n"), testDir);
	}

	// 디렉터리 유효성 확인 테스트
	if( IsDirectory(_T("C:\\TestRoot\\SubFolder")) )
	{
		_tprintf_s(_T("[확인] 유효한 디렉터리입니다.\r\n"));
	}

	// 파일 정보 구조체 초기화 및 복사 테스트 준비
	SH_APPLY_FILEINFO applyInfo;
	memset(&applyInfo, 0, sizeof(applyInfo));
	_tcsncpy_s(applyInfo.m_tszApplyExt, _countof(applyInfo.m_tszApplyExt), _T(".txt;.log"), _TRUNCATE);
	applyInfo.m_bIsApply = true;

	// 복사/이동 테스트를 위한 폴더 구조 생성 및 검증
	// (실제 테스트를 원하실 경우 임의의 .txt 파일을 C:\TestRoot\SubFolder 내에 생성 후 실행해 보세요)

	// 재귀 디렉터리 삭제 테스트
	// if (RemoveDirectoryRecursive(_T("C:\\TestRoot")))
	// {
	//     _tprintf_s(_T("[성공] C:\\TestRoot 삭제 완료\r\n"));
	// }
}

//***************************************************************************
// 2. 레지스트리 제어 테스트
//***************************************************************************
void RegistryOperations()
{
	_tprintf_s(_T("\n=== [2] 레지스트리 제어 테스트 시작 ===\r\n"));

	LPCTSTR subKey = _T("SOFTWARE\\TestAppShellUtil");
	LPCTSTR valueName = _T("SampleVersion");
	DWORD dwWriteValue = 100;

	// 레지스트리 키 재귀 생성 및 값 설정 테스트
	long lRes = RegCreateKeyExRecursive(HKEY_CURRENT_USER, subKey, false);
	if( lRes == ERROR_SUCCESS )
	{
		_tprintf_s(_T("[성공] 레지스트리 키 생성/오픈 성공: HKCU\\%s\r\n"), subKey);

		if( RegSetValue(HKEY_CURRENT_USER, subKey, valueName, dwWriteValue) )
		{
			_tprintf_s(_T("[성공] 레지스트리 DWORD 값 쓰기 성공 (%s = %d)\r\n"), valueName, dwWriteValue);
		}
	}

	// 레지스트리 값 읽기 테스트
	DWORD dwReadValue = 0;
	if( RegGetValue(&dwReadValue, HKEY_CURRENT_USER, subKey, valueName) )
	{
		_tprintf_s(_T("[성공] 레지스트리 값 읽기 성공: %d\r\n"), dwReadValue);
	}

	// 키 존재 여부 확인 테스트
	if( IsRegKey(HKEY_CURRENT_USER, subKey) )
	{
		_tprintf_s(_T("[확인] 레지스트리 키가 존재합니다.\r\n"));
	}

	// 테스트용 레지스트리 정리 (삭제)
	// RegDeleteKeyRecursive(HKEY_CURRENT_USER, subKey);
}

//***************************************************************************
// 3. 파일 핸들 중복 처리 테스트
//***************************************************************************
void FileHandleDuplicate()
{
	_tprintf_s(_T("\n=== [3] 중복 파일 핸들 생성 테스트 시작 ===\r\n"));

	TCHAR destFullPath[FULLPATH_STRLEN] = { 0, };
	TCHAR destFileNameExt[FILENAMEEXT_STRLEN] = { 0, };
	LPCTSTR sampleSourcePath = _T("C:\\TestRoot\\SubFolder\\Data\\sample.txt");

	// 동일 파일명이 존재할 경우 (1), (2) 형태로 유니크한 핸들을 생성하는지 테스트
	// (주의: 해당 경로에 실제 파일이 있거나 부모 디렉터리가 존재해야 합니다)
	// HANDLE hFile = GetFileHandleDuplicate(destFullPath, destFileNameExt, sampleSourcePath);
	// if (hFile != NULL && hFile != INVALID_HANDLE_VALUE)
	// {
	//     _tprintf_s(_T("[성공] 유니크 파일 경로 생성됨: %s\r\n"), destFullPath);
	//     CloseHandle(hFile);
	// }
}

//***************************************************************************
// 4. 제품 키 추출 테스트
//***************************************************************************
void ProductKeyExtract()
{
	_tprintf_s(_T("\n=== [4] 제품 키 추출(DigitalProductID) 테스트 시작 ===\r\n"));

	_tstring productKey;
	// 더미 디지털 프로덕트 ID 버퍼 (실제 Windows 구조체 바이트 배열 필요)
	BYTE dummyProductId[1024] = { 0, };

	// 예시 호출 (바이트 범위 지정 방식)
	// bool bRes = GetProductKeyExtract(productKey, dummyProductId, sizeof(dummyProductId), true);
	// if (bRes)
	// {
	//     _tprintf_s(_T("[성공] 추출된 제품 키: %s\r\n"), productKey.c_str());
	// }
	// else
	// {
	//     _tprintf_s(_T("[참고] 올바른 DigitalProductID 바이트 배열이 아니어 실패했습니다.\r\n"));
	// }
}

//***************************************************************************
//
void SHDirectory()
{
	SH_FILESYSTEM_INFO* pFileFrom = NULL;
	SH_FILESYSTEM_INFO* pFileTo = NULL;

	SH_APPLY_FILEINFO ShApplyFileInfo;
	SHFILEOPSTRUCT	ShFile;

	std::vector<SH_FILESYSTEM_INFO*>	MemBufferFrom;
	std::vector<SH_FILESYSTEM_INFO*>	MemBufferTo;

	_tcsncpy_s(ShApplyFileInfo.m_tszApplyExt, _countof(ShApplyFileInfo.m_tszApplyExt), _T(".aspx;.config;.xml;.js;.udl;.css;.dll;.html;.htm"), _TRUNCATE);
	_tcsncpy_s(ShApplyFileInfo.m_tszModifyStDate, _countof(ShApplyFileInfo.m_tszModifyStDate), _T("2008-01-01"), _TRUNCATE);
	_tcsncpy_s(ShApplyFileInfo.m_tszModifyEdDate, _countof(ShApplyFileInfo.m_tszModifyEdDate), _T("2009-11-30"), _TRUNCATE);

	SHDirectoryRecursive(_T("D:\\소스백업\\Intranet\\"), _T("C:\\aaa\\Intranet\\"), ShApplyFileInfo, MemBufferFrom, MemBufferTo);

	// 1. From 경로 리스트 생성
	std::vector<TCHAR> vecBufferFrom;
	for( size_t i = 0; i < MemBufferFrom.size(); i++ )
	{
		pFileFrom = MemBufferFrom[i];
		if( pFileFrom && pFileFrom->m_tszFullPath )
		{
			size_t nLen = _tcslen(pFileFrom->m_tszFullPath) + 1;
			vecBufferFrom.insert(vecBufferFrom.end(), pFileFrom->m_tszFullPath, pFileFrom->m_tszFullPath + nLen);
		}

		if( pFileFrom )
		{
			delete pFileFrom;
			pFileFrom = NULL;
		}
	}
	vecBufferFrom.push_back(_T('\0'));

	// 2. To 경로 리스트 생성
	std::vector<TCHAR> vecBufferTo;
	for( size_t i = 0; i < MemBufferTo.size(); i++ )
	{
		pFileTo = MemBufferTo[i];
		if( pFileTo && pFileTo->m_tszFullPath )
		{
			size_t nLen = _tcslen(pFileTo->m_tszFullPath) + 1;
			vecBufferTo.insert(vecBufferTo.end(), pFileTo->m_tszFullPath, pFileTo->m_tszFullPath + nLen);
		}

		if( pFileTo )
		{
			delete pFileTo;
			pFileTo = NULL;
		}
	}
	vecBufferTo.push_back(_T('\0'));

	// 3. 파일 작업 구조체 설정
	ShFile.hwnd = NULL;
	ShFile.pFrom = vecBufferFrom.data();
	ShFile.pTo = vecBufferTo.data();
	ShFile.wFunc = FO_COPY;
	ShFile.fFlags = FOF_MULTIDESTFILES | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
	ShFile.lpszProgressTitle = _T("파일 복사");

	SHFileOperation(&ShFile);
}

//***************************************************************************
//
bool GetRegistry()
{
	long	lRetCode = 0;
	TCHAR	tszFullSubKey[REGISTRY_KEY_STRLEN];
	TCHAR	tszSubKey[REGISTRY_KEY_STRLEN];

	HKEY	hKey;

	SH_REGISTRY_INFO* pShRegistryInfo = NULL;

	std::vector<SH_REGISTRY_INFO*>		MemBuffer;

	_tcsncpy_s(tszFullSubKey, _countof(tszFullSubKey), _T("*"), _TRUNCATE);
	tszSubKey[0] = '\0';

	lRetCode = RegOpenKeyEx(HKEY_CLASSES_ROOT, tszFullSubKey, 0, KEY_READ, &hKey);
	if( lRetCode != ERROR_SUCCESS ) return false;

	if( !GetRegistryRecursive(hKey, tszFullSubKey, tszSubKey, MemBuffer) ) return false;

	RegCloseKey(hKey);

	for( size_t i = 0; i < MemBuffer.size(); i++ )
	{
		pShRegistryInfo = MemBuffer[i];

		_tprintf_s(_T("%s[%s = %s]\r\n"), pShRegistryInfo->m_tszFullPathKey, pShRegistryInfo->m_tszName, (TCHAR*)pShRegistryInfo->m_pbValue);

		if( pShRegistryInfo )
		{
			delete pShRegistryInfo;
			pShRegistryInfo = NULL;
		}
	}

	return true;
}