
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

#ifndef __DIRECTORYUTIL_H__
#include "DirectoryUtil.h"		// SH_APPLY_FILEINFO, IsMatchedExtension()/IsAbleFile(), 재귀 파일 조작 함수 — 플랫폼 공용 구현
#endif

// 기존 코드 호환용 포인터 typedef (SH_APPLY_FILEINFO 정의는 FileFilterUtil.h로 이전됨)
typedef SH_APPLY_FILEINFO* PSH_APPLY_FILEINFO;

//***************************************************************************
// @brief 파일 시스템(경로, 폴더, 파일명) 정보를 관리하는 구조체
//***************************************************************************
typedef struct _SH_FILESYSTEM_INFO
{
	TCHAR		m_tszFullPath[FULLPATH_STRLEN];       // 파일의 전체 경로 (드라이브+폴더+파일명+확장자)
	TCHAR		m_tszFolder[DIRECTORY_STRLEN];        // 파일이 속한 디렉토리(폴더) 경로
	TCHAR		m_tszFileNameExt[FILENAMEEXT_STRLEN]; // 파일의 이름과 확장자 (예: "document.txt")

} SH_FILESYSTEM_INFO, * PSH_FILESYSTEM_INFO;

//***************************************************************************
// @brief 윈도우 레지스트리 키 및 값 정보를 관리하는 구조체
//***************************************************************************
typedef struct _SH_REGISTRY_INFO
{
	// 생성자: 멤버 변수 초기화
	_SH_REGISTRY_INFO() {
		memset(m_tszFullPathKey, 0, sizeof(m_tszFullPathKey));
		memset(m_tszSubPathKey, 0, sizeof(m_tszSubPathKey));
		memset(m_tszName, 0, sizeof(m_tszName));
		m_pbValue = NULL;

		m_dwType = 0;
		m_dwNameLen = 0;
		m_dwValueLen = 0;
	}

	// 소수점/동적 메모리 해제 소멸자: 할당된 레지스트리 값 버퍼 정리
	~_SH_REGISTRY_INFO() {
		if( m_pbValue )
		{
			delete[]m_pbValue;
			m_pbValue = NULL;
		}
	}

	TCHAR	m_tszFullPathKey[REGISTRY_KEY_STRLEN];	// 레지스트리 키의 전체 경로
	TCHAR	m_tszSubPathKey[REGISTRY_KEY_STRLEN];   // 하위 레지스트리 경로
	TCHAR	m_tszName[REGISTRY_NAME_STRLEN];		// 레지스트리 값(Value)의 이름
	BYTE* m_pbValue;								// 레지스트리 값의 실제 데이터가 저장될 동적 버퍼 포인터

	DWORD	m_dwType;								// 레지스트리 데이터 타입 (REG_SZ, REG_DWORD 등)
	DWORD	m_dwNameLen;							// 레지스트리 이름의 길이
	DWORD	m_dwValueLen;							// 레지스트리 값 데이터의 바이트 크기
} SH_REGISTRY_INFO, * PSH_REGISTRY_INFO;

// IsDirectory()/CreateDirectoryRecursive()/RemoveDirectoryRecursive()/CopyFileRecursive()/MoveFileRecursive()는
// DirectoryUtil.h에서 선언됨(std::filesystem::path 기반, 플랫폼 공용)

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

bool GetProductKeyExtract(_tstring& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const bool bIsExtractBytesRange);

#endif // ndef __SHELLUTIL_H__