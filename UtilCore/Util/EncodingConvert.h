
//***************************************************************************
// EncodingConvert.h: interface for the EncodingConvert Function.
//
//***************************************************************************

#ifndef __ENCODINGCONVERT_H__
#define __ENCODINGCONVERT_H__

#ifndef __WINCHARSETCONV_H__
#include <Util/WinCharsetConv.h>
#endif

#ifdef _STRING_
std::wstring AnsiToUnicode(const std::string& ansi);
std::string UnicodeToAnsi(const std::wstring& unicode);

std::string UnicodeToUtf8(const std::wstring& unicode);
std::wstring Utf8ToUnicode(const std::string& utf8);

std::string AnsiToUtf8(const std::string& ansi);
std::string Utf8ToAnsi(const std::string& utf8);

_tstring	StringToTString(const std::string& src);
string		TStringToString(const _tstring& src);

_tstring	WStringToTString(const std::wstring& src);
wstring		TStringToWString(const _tstring& src);
#endif

#endif // ndef __ENCODINGCONVERT_H__