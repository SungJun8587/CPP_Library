
//***************************************************************************
// FileUtil.h : interface for the FileUtil Functions.
//
//***************************************************************************

#ifndef __FILEUTIL_H__
#define __FILEUTIL_H__

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

#define SWAP16(s) (((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))) 
#define SWAP32(l) (((((l) & 0xff000000) >> 24) | (((l) & 0x00ff0000) >> 8) | (((l) & 0x0000ff00) << 8) | (((l) & 0x000000ff) << 24)))  


#define	UTF_FILE_IDENTIFIER_BYTE1			0xEF
#define	UTF_FILE_IDENTIFIER_BYTE2			0xBB
#define	UTF_FILE_IDENTIFIER_BYTE3			0xBF

#define	UNICODE_LE_FILE_IDENTIFIER_BYTE1	0xFF
#define	UNICODE_LE_FILE_IDENTIFIER_BYTE2	0xFE

#define	UNICODE_BE_FILE_IDENTIFIER_BYTE1	0xFE	
#define	UNICODE_BE_FILE_IDENTIFIER_BYTE2	0xFF	

#define FILEINFO_CREATETIME					1
#define FILEINFO_ACCESSTIME					2
#define FILEINFO_LASTWRITETIME				3
#define MAX_FILENAME_CONVERT_INDEX_NUM		10000

enum class EEncoding
{
	DEFAULT = 0,
	ANSI,
	UTF16_LE,
	UTF16_BE,
	UTF8_BOM,
	UTF8_NOBOM
};

bool		IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize);

#ifdef _WIN32
EEncoding	IsFileType(const TCHAR* ptszFullPath);

bool		ReadFile(CMemBuffer<BYTE>& byteDestination, const TCHAR* ptszFullPath);
bool		ReadFileMap(CMemBuffer<BYTE>& byteDestination, const TCHAR* ptszFullPath);

bool		ReadFile(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszFullPath);
bool		ReadFileMap(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszFullPath);

bool		ReadFile(_tstring& destString, const TCHAR* ptszFullPath);
bool		ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath);

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
#endif

EEncoding		GetFileEncodingType(const _tstring& filepath);
_tstring		ReadFile(const _tstring& filepath);
bool			WriteFile(const _tstring& filepath, const _tstring& content, EEncoding fileType);
std::uintmax_t	GetFileSize(const _tstring& filepath);

#endif // ndef __FILEUTIL_H__

