
//***************************************************************************
// ConvertCharset.h : interface for the ConvertCharset Functions.
//
//***************************************************************************

#ifndef __CONVERTCHARSET_H__
#define __CONVERTCHARSET_H__

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

#ifndef __ATLBASE_H__
#include <atlbase.h>
#endif

size_t	GetMultiByteLen(const int nCodePage, const TCHAR* ptszSource);

DWORD   AnsiToUnicode(wchar_t* unicode, size_t unicode_size, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi(char* ansi, size_t ansi_size, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8(char* utf8, size_t utf8_size, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode(wchar_t* unicode, size_t unicode_size, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8(char* utf8, size_t utf8_size, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi(char* ansi, size_t ansi_size, const char* utf8, const size_t utf8_size);

#ifdef __MEMBUFFER_H__
DWORD	AnsiToUnicode(CMemBuffer<wchar_t>& unicode, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi(CMemBuffer<char>& ansi, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8(CMemBuffer<char>& utf8, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode(CMemBuffer<wchar_t>& unicode, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8(CMemBuffer<char>& utf8, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi(CMemBuffer<char>& ansi, const char* utf8, const size_t utf8_size);

bool	ByteToTChar(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer);
bool	TCharToByte(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer);
#endif

#ifdef _STRING_
DWORD	AnsiToUnicode_String(std::wstring& unicode, const char* ansi, const size_t ansi_size);
DWORD	UnicodeToAnsi_String(std::string& ansi, const wchar_t* unicode, const size_t unicode_size);

DWORD	UnicodeToUtf8_String(std::string& utf8, const wchar_t* unicode, const size_t unicode_size);
DWORD	Utf8ToUnicode_String(std::wstring& unicode, const char* utf8, const size_t utf8_size);

DWORD	AnsiToUtf8_String(std::string& utf8, const char* ansi, const size_t ansi_size);
DWORD	Utf8ToAnsi_String(std::string& ansi, const char* utf8, const size_t utf8_size);

wstring StringToWString(const std::string& ansi);
string WStringToString(const std::wstring& unicode);

string UnicodeToUtf8(const std::wstring unicode);
wstring Utf8ToUnicode(const std::string utf8);

string AnsiToUtf8(const std::string ansi);
string Utf8ToAnsi(const std::string utf8);

_tstring	StringToTString(const std::string& src);
string		TStringToString(const _tstring& src);

_tstring	WStringToTString(const std::wstring& src);
wstring		TStringToWString(const _tstring& src);
#endif

#endif // ndef __CONVERTCHARSET_H__
