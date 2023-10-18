
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
#include <StringUtil.h>
#endif

#define SWAP16(s) (((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))) 
#define SWAP32(l) (((((l) & 0xff000000) >> 24) | (((l) & 0x00ff0000) >> 8) | (((l) & 0x0000ff00) << 8) | (((l) & 0x000000ff) << 24)))  

#define	UTF_FILE_IDENTIFIER_WORD			0xBBEF
#define	UTF_FILE_IDENTIFIER_BYTE			0xBF
#define	UNICODE_BE_FILE_IDENTIFIER_WORD		0xFFFE
#define	UNICODE_LE_FILE_IDENTIFIER_WORD		0xFEFF
#define FILEINFO_CREATETIME					1
#define FILEINFO_ACCESSTIME					2
#define FILEINFO_LASTWRITETIME				3
#define MAX_FILENAME_CONVERT_INDEX_NUM		10000

enum EFileType
{
	DEFAULT = 0,
	ANSI,
	UTF16_LE,
	UTF16_BE,
	UTF8_BOM,
	UTF8_NOBOM
};

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

bool		IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize);
EFileType	IsFileType(const TCHAR* ptszFullPath);
bool		ReadFile(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath);
bool		ReadFileMap(CMemBuffer<BYTE>& ByteDestination, const TCHAR* ptszFullPath);

bool		ReadFile(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath);
bool		ReadFileMap(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszFullPath);

#ifdef _STRING_
bool		ReadFile(_tstring& DestString, const TCHAR* ptszFullPath);
bool		ReadFileMap(_tstring& DestString, const TCHAR* ptszFullPath);
#endif

bool		SaveFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength);
bool		SaveAnsiFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize);
bool		SaveUnicodeBEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize);
bool		SaveUnicodeLEFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize);
bool		SaveUTF8BOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize);
bool		SaveUTF8NOBOMFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize);

bool		GetFileInfoTime(const TCHAR* ptszFilePath, const int nCase, SYSTEMTIME& stLocal);
bool		IsExistFile(const TCHAR* ptszFilePath);
DWORD		GetFileSize(const TCHAR* ptszFilePath);
bool		GetFileInformation(const TCHAR* ptszFilePath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation);

bool		GetProductKeyExtract(CMemBuffer<TCHAR>& TProductKey, const BYTE* pbDigitalProductID, const DWORD dwLength, const bool  bIsExtractBytesRange);

#endif // ndef __SHELLUTIL_H__

