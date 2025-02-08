
//***************************************************************************
// StringUtil.h : interface for the StringUtil Functions.
//
//***************************************************************************

#ifndef __STRINGUTIL_H__
#define __STRINGUTIL_H__

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

#ifndef __ATLBASE_H__
#include <atlbase.h>
#endif

#ifdef	__MEMBUFFER_H__
bool	FolderPathPassing(CMemBuffer<TCHAR>& TFolderPath, const TCHAR* ptszFullFilePath);
bool	FileNameExtPathPassing(CMemBuffer<TCHAR>& TFileNameExt, const TCHAR* ptszFullFilePath);
bool	FileNameExtPassing(CMemBuffer<TCHAR>& FileName, CMemBuffer<TCHAR>& TFileExt, const TCHAR* ptszFileNameExt);
bool    HostNamePassing(CMemBuffer<TCHAR>& THostName, const TCHAR* ptszFullUrl);
bool	UrlFullPathPassing(CMemBuffer<TCHAR>& TUrlFullPath, const TCHAR* ptszFullUrl);
bool	QueryStringPassing(CMemBuffer<TCHAR>& TQueryString, const TCHAR* ptszFullUrl);

bool	Tokenize(CMemBuffer<TCHAR>*& ppTDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

bool	StrUpper(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource);
bool	StrLower(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource);
bool	StrReverse(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource);
bool	StrAppend(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR* ptszAppend);

bool	StrMid(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nStart);
bool	StrMid(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nStart, const size_t count);
bool	StrMidToken(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nStart, const TCHAR tcToken);
bool	StrLeft(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t count);
bool	StrRight(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t count);
bool	StrReplace(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR tcSrcToken, const TCHAR tcDestToken);
bool	StrReplace(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR* ptszSrcToken, const TCHAR* ptszDestToken);

bool	TrimLeft(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource);
bool	TrimLeft(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR tcToken);
bool	TrimLeft(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

bool	TrimRight(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource);
bool	TrimRight(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR tcToken);
bool	TrimRight(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const TCHAR* ptszToken);

bool	StrCutUnicodeAscii(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nSize);

bool	StrCatLocationToken(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR tcToken);
bool	StrCatLocationToken(CMemBuffer<TCHAR>& tDestination, const TCHAR* ptszSource, const size_t nLocation, const TCHAR* ptszToken);
#endif


#ifdef _STRING_
// Window FullPath MaxLen = 260
_tstring	FolderPathPassing(const _tstring& fullFilePath);
_tstring	FileNameExtPathPassing(const _tstring& fullFilePath);
bool		FileNameExtPassing(const _tstring& fileNameExt, _tstring& fileName, _tstring& fileExt);

// IE POST/GET Method FullUrl MaxLen = 2083, Etc Browser don't limit
bool		ParseURL(const _tstring& fullUrl, _tstring& protocol, _tstring& hostName, _tstring& request, int& nPort);
_tstring    HostNamePassing(const _tstring& fullUrl);
_tstring	UrlFullPathPassing(const _tstring& fullUrl);
_tstring	QueryStringPassing(const _tstring& fullUrl);

size_t		TokenCount(const _tstring& source, const _tstring& token);
bool		Tokenize(std::vector<_tstring>& dests, const _tstring& source, const _tstring& token);
#endif

#endif // ndef __STRINGUTIL_H__
