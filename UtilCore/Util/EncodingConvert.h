
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

std::string	AnsiToUtf8(const std::string& ansi);
std::string	Utf8ToAnsi(const std::string& utf8);

_tstring StringToTString(const std::string& src);
std::string	TStringToString(const _tstring& src);

_tstring WStringToTString(const std::wstring& src);
std::wstring TStringToWString(const _tstring& src);

std::string TStringToUtf8(const _tstring& src);
_tstring Utf8ToTString(const std::string& src);

std::wstring AnsiToUnicode(const char* ansi, int32 dataLength);
std::string  UnicodeToAnsi(const wchar_t* unicode, int32 dataLength);
std::string  UnicodeToUtf8(const wchar_t* unicode, int32 dataLength);
std::wstring Utf8ToUnicode(const char* utf8, int32 dataLength);
std::string  AnsiToUtf8(const char* ansi, int32 dataLength);
std::string  Utf8ToAnsi(const char* utf8, int32 dataLength);
#endif

#endif // ndef __ENCODINGCONVERT_H__