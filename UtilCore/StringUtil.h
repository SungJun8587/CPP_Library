
//***************************************************************************
// StringUtil.h : interface for the StringUtil Functions.
//
//***************************************************************************

#ifndef __STRINGUTIL_H__
#define __STRINGUTIL_H__

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

#ifndef __MEMBUFFER_H__
#include <MemBuffer.h> 
#endif

size_t	GetMultiByteLen(const int nCodePage, const TCHAR* ptszSource);

DWORD   AnsiToUnicode(wchar_t* unicode, size_t unicode_size, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi(char* ansi, size_t ansi_size, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8(char* utf8, size_t utf8_size, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode(wchar_t* unicode, size_t unicode_size, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8(char* utf8, size_t utf8_size, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi(char* ansi, size_t ansi_size, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUnicode(CMemBuffer<wchar_t>& unicode, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi(CMemBuffer<char>& ansi, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8(CMemBuffer<char>& utf8, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode(CMemBuffer<wchar_t>& unicode, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8(CMemBuffer<char>& utf8, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi(CMemBuffer<char>& ansi, const char* utf8, const size_t utf8_size);

bool	ByteToTChar(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer);
bool	TCharToByte(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer);

#ifdef _STRING_
DWORD	AnsiToUnicode_String(std::wstring& unicode, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi_String(std::string& ansi, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8_String(std::string& utf8, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode_String(std::wstring& unicode, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8_String(std::string& utf8, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi_String(std::string& ansi, const char* utf8, const size_t utf8_size);
#endif

// Window FullPath MaxLen = 260
bool	FolderPathPassing(TCHAR* ptszFolderPath, const TCHAR* ptszFullFilePath);
bool	FileNameExtPathPassing(TCHAR* ptszFileNameExt, const TCHAR* ptszFullFilePath);
bool	FileNameExtPassing(TCHAR* ptszFileName, TCHAR* ptszFileExt, const TCHAR* ptszFileNameExt);

bool	FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath);
bool	FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath);
bool	FileNameExtPassing(CMemBuffer<TCHAR>& FileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt);

// IE POST/GET Method FullUrl MaxLen = 2083, Etc Browser don't limit
bool	ParseURL(TCHAR* ptszProtocol, TCHAR* ptszHostName, TCHAR* ptszRequest, int& nPort, const TCHAR* ptszFullUrl);
bool    HostNamePassing(TCHAR* ptszHostName, const TCHAR* ptszFullUrl);
bool	UrlFullPathPassing(TCHAR* ptszUrlFullPath, const TCHAR* ptszFullUrl);
bool	QueryStringPassing(TCHAR* ptszQueryString, const TCHAR* ptszFullUrl);

bool    HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl);
bool	UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl);
bool	QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl);

size_t	TokenCount(const TCHAR* ptszSource, const TCHAR* ptszToken);

bool	Tokenize(TCHAR** pptszDestination, int& nCount, TCHAR* ptszSource, const TCHAR* ptszToken, const int nSize);
bool	Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

void	StrUpper(TCHAR* ptszDestination, const TCHAR* ptszSource);
void	StrLower(TCHAR* ptszDestination, const TCHAR* ptszSource);
void    StrReverse(TCHAR* ptszDestination, const TCHAR* ptszSource);
void	StrAppend(TCHAR* ptszDestination, const TCHAR* ptszSource);

bool	StrUpper(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool	StrLower(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool	StrReverse(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool	StrAppend(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend);

bool	StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart);
bool	StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const size_t nCount);
bool	StrMidToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const TCHAR tcToken);
bool	StrLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount);
bool	StrRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount);
bool	StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken);
bool	StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszSrcToken, const TCHAR* ptszDestToken);

size_t	StrFind(const TCHAR* ptszSource, const TCHAR tcCompare, const size_t nStart);
size_t	StrFind(const TCHAR* ptszSource, const TCHAR* ptszCompare, const size_t nStart);
size_t	StrReverseFind(const TCHAR* ptszSource, const TCHAR tcCompare);
size_t	StrReverseFind(const TCHAR* ptszSource, const TCHAR* ptszCompare);

void	TrimLeft(TCHAR* ptszSource);
void	TrimLeft(TCHAR* ptszSource, int nDataLength);
void	TrimLeft(TCHAR* ptszSource, TCHAR cToken);
void	TrimLeft(TCHAR* ptszSource, TCHAR* ptszToken);
bool	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcToken);
bool	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

void	TrimRight(TCHAR* ptszSource);
void	TrimRight(TCHAR* ptszSource, size_t nDataLength);
void	TrimRight(TCHAR* ptszSource, TCHAR cToken);
void	TrimRight(TCHAR* ptszSource, TCHAR* ptszToken);
bool	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcToken);
bool	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

bool	StrCutUnicodeAscii(TCHAR* ptszDestination, const TCHAR* ptszSource, const size_t nSize);
bool	StrCutUnicodeAscii(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nSize);

bool	StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken);
bool	StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken);

#endif // ndef __STRINGUTIL_H__
