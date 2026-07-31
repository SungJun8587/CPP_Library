
//***************************************************************************
// WebUtil.h: interface for the WebUtil Function.
//
//***************************************************************************

#ifndef	__WEBUTIL_H__
#define	__WEBUTIL_H__

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

#endif // ndef __WEBUTIL_H__
