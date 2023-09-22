
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

BOOL	MultiByteToWideCharStr(wchar_t* pwszDestination, size_t count, const char* pszSource);
BOOL	WideCharToMultiByteStr(char* pszDestination, size_t count, const wchar_t* pwszSource);

BOOL	MultiByteToWideCharStr(CMemBuffer<wchar_t>& WDestination, const char* ptszSource);
BOOL	WideCharToMultiByteStr(CMemBuffer<char>& Destination, const wchar_t* pwszSource);

BOOL	AnsiToUTF8(CMemBuffer<char>& Destination, const char* ptszSource);
BOOL	UnicodeToUTF8(CMemBuffer<wchar_t>& WDestination, const char* ptszSource);

BOOL	UTF8ToAnsi(CMemBuffer<char>& Destination, const char* ptszSource);
BOOL	UTF8ToUnicode(CMemBuffer<char>& Destination, const wchar_t* pwszBuffer);

BOOL	TCharToByte(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer);
BOOL	ByteToTChar(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer);

// Window FullPath MaxLen = 260
BOOL	FolderPathPassing(TCHAR* ptszFolderPath, const TCHAR* ptszFullFilePath);
BOOL	FileNameExtPathPassing(TCHAR* ptszFileNameExt, const TCHAR* ptszFullFilePath);
BOOL	FileNameExtPassing(TCHAR* ptszFileName, TCHAR* ptszFileExt, const TCHAR* ptszFileNameExt);

BOOL	FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath);
BOOL	FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath);
BOOL	FileNameExtPassing(CMemBuffer<TCHAR>& FileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt);

// IE POST/GET Method FullUrl MaxLen = 2083, Etc Browser don't limit
BOOL	ParseURL(TCHAR* ptszProtocol, TCHAR* ptszHostName, TCHAR* ptszRequest, int& nPort, const TCHAR* ptszFullUrl);
BOOL    HostNamePassing(TCHAR* ptszHostName, const TCHAR* ptszFullUrl);
BOOL	UrlFullPathPassing(TCHAR* ptszUrlFullPath, const TCHAR* ptszFullUrl);
BOOL	QueryStringPassing(TCHAR* ptszQueryString, const TCHAR* ptszFullUrl);

BOOL    HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl);
BOOL	UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl);
BOOL	QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl);

size_t	TokenCount(const TCHAR* ptszSource, const TCHAR* ptszToken);

BOOL	Tokenize(TCHAR** pptszDestination, int& nCount, TCHAR* ptszSource, const TCHAR* ptszToken, const int nSize);
BOOL	Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

void	StrUpper(TCHAR* ptszDestination, const TCHAR* ptszSource);
void	StrLower(TCHAR* ptszDestination, const TCHAR* ptszSource);
void    StrReverse(TCHAR* ptszDestination, const TCHAR* ptszSource);
void	StrAppend(TCHAR* ptszDestination, const TCHAR* ptszSource);

BOOL	StrUpper(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL	StrLower(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL	StrReverse(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL	StrAppend(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend);

BOOL	StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart);
BOOL	StrMid(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const size_t nCount);
BOOL	StrMidToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nStart, const TCHAR tcToken);
BOOL	StrLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount);
BOOL	StrRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nCount);
BOOL	StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken);
BOOL	StrReplace(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszSrcToken, const TCHAR* ptszDestToken);

size_t	StrFind(const TCHAR* ptszSource, const TCHAR tcCompare, const size_t nStart);
size_t	StrFind(const TCHAR* ptszSource, const TCHAR* ptszCompare, const size_t nStart);
size_t	StrReverseFind(const TCHAR* ptszSource, const TCHAR tcCompare);
size_t	StrReverseFind(const TCHAR* ptszSource, const TCHAR* ptszCompare);

void	TrimLeft(TCHAR* ptszSource);
void	TrimLeft(TCHAR* ptszSource, int nDataLength);
void	TrimLeft(TCHAR* ptszSource, TCHAR cToken);
void	TrimLeft(TCHAR* ptszSource, TCHAR* ptszToken);
BOOL	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcToken);
BOOL	TrimLeft(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

void	TrimRight(TCHAR* ptszSource);
void	TrimRight(TCHAR* ptszSource, size_t nDataLength);
void	TrimRight(TCHAR* ptszSource, TCHAR cToken);
void	TrimRight(TCHAR* ptszSource, TCHAR* ptszToken);
BOOL	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR tcToken);
BOOL	TrimRight(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

BOOL	StrCutUnicodeAscii(TCHAR* ptszDestination, const TCHAR* ptszSource, const size_t nSize);
BOOL	StrCutUnicodeAscii(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nSize);

BOOL	StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken);
BOOL	StrCatLocationToken(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken);

#endif // ndef __STRINGUTIL_H__
