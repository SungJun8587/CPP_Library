
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

bool Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int iLength);
bool Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const int iLength);
bool Base64Enc(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int iCodePage);
bool UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, int iCodePage);

bool UrlPathEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool HtmlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool Escape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool UnEscape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool EncodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool DecodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool EncodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool DecodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

#endif // ndef __WEBUTIL_H__
