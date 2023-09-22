
//***************************************************************************
// WebUtil.h: interface for the WebUtil Function.
//
//***************************************************************************

#ifndef	__WEBUTIL_H__
#define	__WEBUTIL_H__

#ifndef	__STRINGUTIL_H__
#include <StringUtil.h> 
#endif

#ifndef	__MEMBUFFER_H__
#include <MemBuffer.h> 
#endif

BOOL Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int nLength);
BOOL Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int nLength);
BOOL Base64Enc(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

BOOL UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int nCodePage);
BOOL UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int nCodePage);

BOOL UrlPathEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

BOOL HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL HtmlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

BOOL Escape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL UnEscape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

BOOL EncodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL DecodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

BOOL EncodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
BOOL DecodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

#endif // ndef __WEBUTIL_H__
