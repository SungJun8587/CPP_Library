
//***************************************************************************
// WebUtil.h: interface for the WebUtil Function.
//
//***************************************************************************

#ifndef	__WEBUTIL_H__
#define	__WEBUTIL_H__

#ifdef	__MEMBUFFER_H__
bool Base64Enc(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const size_t length);
bool Base64Dec(CMemBuffer<BYTE>& ByteDestination, const BYTE* pbSource, const size_t length);
bool Base64Enc(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool Base64Dec(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool UrlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const int iCodePage);
bool UrlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource, const int iCodePage);

bool UrlPathEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool HtmlEncode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool HtmlDecode(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool Escape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool UnEscape(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool EncodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool DecodeURI(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);

bool EncodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
bool DecodeURIComponent(CMemBuffer<TCHAR>& TDestination, const TCHAR* ptszSource);
#endif

#ifdef _STRING_
_tstring Base64Enc(const _tstring& source);
_tstring Base64Dec(const _tstring& source);

_tstring UrlEncode(const _tstring& source, const int iCodePage);
_tstring UrlDecode(const _tstring& source, const int iCodePage);

_tstring UrlPathEncode(const _tstring& source);

_tstring HtmlEncode(const _tstring& source);
_tstring HtmlDecode(const _tstring& source);

_tstring Escape(const _tstring& source);
_tstring UnEscape(const _tstring& source);

_tstring EncodeURI(const _tstring& source);
_tstring DecodeURI(const _tstring& source);

_tstring EncodeURIComponent(const _tstring& source);
_tstring DecodeURIComponent(const _tstring& source);
#endif

#endif // ndef __WEBUTIL_H__
