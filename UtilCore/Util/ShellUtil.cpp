
//***************************************************************************
// ShellUtil.cpp : implementation of the ShellUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "ShellUtil.h"

namespace fs = std::filesystem;

// IsDirectory()/CreateDirectoryRecursive()/RemoveDirectoryRecursive()/CopyFileRecursive()/MoveFileRecursive()/
// IsMatchedExtension()/IsAbleFile()/SH_APPLY_FILEINFO의 실제 구현은 DirectoryUtil.cpp에 있다.
// (std::filesystem::path 기반이라 인코딩 문제 없이 동작하며, Windows에서는 생성일까지 함께 판정한다)

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
	TCHAR	tszTempFullPath[DIRECTORY_STRLEN + FILENAME_STRLEN];
	TCHAR   tszTempFileNameExt[FILENAMEEXT_STRLEN];

	HANDLE	hFile;

	_tstring folderPath, fileNameExt;
	_tstring fileName, fileExt;

	folderPath = FolderPathPassing(ptszFullPath);
	fileNameExt = FileNameExtPathPassing(ptszFullPath);
	FileNameExtPassing(fileNameExt, fileName, fileExt);

	CreateDirectoryRecursive(fs::path(folderPath));

	_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), folderPath.c_str(), fileNameExt.c_str());
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
			_sntprintf_s(tszTempFullPath, _countof(tszTempFullPath), _TRUNCATE, _T("%s%s"), folderPath.c_str(), tszTempFileNameExt);
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