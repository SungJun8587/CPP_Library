
//***************************************************************************
// FileUtil.h: interface for the FileUtil Functions.
//
//***************************************************************************

#ifndef UC_FILEUTIL_H
#define UC_FILEUTIL_H

#include <windows.h>
#include <time.h>
#include <tchar.h>

#include <Util/WinCharsetConv.h>

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

enum class EEncoding
{
	DEFAULT = 0,
	ANSI,
	UTF16_LE,
	UTF16_BE,
	UTF8_BOM,
	UTF8_NOBOM
};

// pBuffer/BuffSize 범위 내의 바이트열이 유효한(malformed 없는) UTF-8 멀티바이트
// 시퀀스로만 구성되어 있는지 검사합니다(오버롱 인코딩, UTF-16 서로게이트 영역,
// U+10FFFF 초과 등 malformed 시퀀스는 실패로 처리). 순수 ASCII만 있는 버퍼는
// ANSI와 구분할 근거가 없으므로 false를 반환합니다(호출부에서 ANSI로 분류됨).
bool		IsUTF8WithoutBom(const void* pBuffer, const size_t BuffSize);

#ifdef _WIN32
EEncoding	GetFileEncodingType(const TCHAR* ptszFullPath);

bool		ReadFile(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath);
bool		ReadFileMap(std::vector<BYTE>& byteDestination, const TCHAR* ptszFullPath);
bool		WriteFile(const TCHAR* ptszFullPath, const BYTE* pbBuffer, const DWORD dwLength);

bool		ReadFile(_tstring& destString, const TCHAR* ptszFullPath);
bool		ReadFileMap(_tstring& destString, const TCHAR* ptszFullPath);
bool		WriteFile(const TCHAR* ptszFullPath, const TCHAR* ptszBuffer, const size_t BufferSize, EEncoding fileType);

bool		GetFileInfoTime(const TCHAR* ptszFilePath, const int nCase, SYSTEMTIME& stLocal);
bool		IsExistFile(const TCHAR* ptszFilePath);

// 파일 크기를 64비트 값으로 반환합니다(GetFileSizeEx 기반). 4GiB 이상의 파일도
// 정확한 크기를 반환하며, 실패 시 0을 반환합니다.
ULONGLONG	GetFileSize(const TCHAR* ptszFilePath);

bool		GetFileInformation(const TCHAR* ptszFilePath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation);
bool		GetFileInfoAndEncoding(const TCHAR* ptszFullPath, LPBY_HANDLE_FILE_INFORMATION lpFileInformation, EEncoding& outEncoding);
#endif

EEncoding		GetFileEncodingType(const _tstring& filepath);
_tstring		ReadFile(const _tstring& filepath);
bool			WriteFile(const _tstring& filepath, const _tstring& content, EEncoding fileType);
bool			IsExistFile(const _tstring& filepath);
std::uintmax_t	GetFileSize(const _tstring& filepath);

#endif // ndef UC_FILEUTIL_H