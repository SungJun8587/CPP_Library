
//***************************************************************************
// EncodingConvert.cpp: implementation of the EncodingConvert Function.
//
//***************************************************************************

#include "pch.h"
#include "EncodingConvert.h"

#ifdef _STRING_
//***************************************************************************
// @brief std::string(ANSI)을 std::wstring으로 변환합니다.
// @param ansi 원본 std::string 문자열
// @return 변환된 std::wstring (실패 시 빈 문자열)
//***************************************************************************
wstring AnsiToUnicode(const std::string& ansi)
{
#ifdef __ICONVUTIL_H__
	if( ansi.empty() ) return L"";
	try
	{
		thread_local Iconv::CIconvUtil conv("CP949", "WCHAR_T");
		return conv.ConvertW(ansi);
	}
	catch( ... )
	{
		return L"";
	}
#else
	std::wstring unicode;
	if( AnsiToUnicode_String(unicode, ansi.c_str(), ansi.size()) != 0 ) return L"";
	return unicode;
#endif
}

//***************************************************************************
// @brief std::wstring을 std::string(ANSI)으로 변환합니다.
// @param unicode 원본 std::wstring 문자열
// @return 변환된 std::string (실패 시 빈 문자열)
//***************************************************************************
string UnicodeToAnsi(const std::wstring& unicode)
{
#ifdef __ICONVUTIL_H__
	if( unicode.empty() ) return "";
	try
	{
		thread_local Iconv::CIconvUtil conv("WCHAR_T", "CP949");
		return conv.Convert(unicode);
	}
	catch( ... )
	{
		return "";
	}
#else
	std::string ansi;
	if( UnicodeToAnsi_String(ansi, unicode.c_str(), unicode.size()) != 0 ) return "";
	return ansi;
#endif
}

//***************************************************************************
// @brief std::wstring을 UTF-8 std::string으로 변환합니다.
// @param unicode 원본 std::wstring 문자열
// @return 변환된 UTF-8 std::string (실패 시 빈 문자열)
//***************************************************************************
string UnicodeToUtf8(const std::wstring& unicode)
{
#ifdef __ICONVUTIL_H__
	if( unicode.empty() ) return "";
	try
	{
		thread_local Iconv::CIconvUtil conv("WCHAR_T", "UTF-8");
		return conv.Convert(unicode);
	}
	catch( ... )
	{
		return "";
	}
#else
	std::string utf8;
	if( UnicodeToUtf8_String(utf8, unicode.c_str(), unicode.size()) != 0 ) return "";
	return utf8;
#endif
}

//***************************************************************************
// @brief UTF-8 std::string을 std::wstring으로 변환합니다.
// @param utf8 원본 UTF-8 std::string 문자열
// @return 변환된 std::wstring (실패 시 빈 문자열)
//***************************************************************************
wstring Utf8ToUnicode(const std::string& utf8)
{
#ifdef __ICONVUTIL_H__
	if( utf8.empty() ) return L"";
	try
	{
		thread_local Iconv::CIconvUtil conv("UTF-8", "WCHAR_T");
		return conv.ConvertW(utf8);
	}
	catch( ... )
	{
		return L"";
	}
#else
	std::wstring unicode;
	if( Utf8ToUnicode_String(unicode, utf8.c_str(), utf8.size()) != 0 ) return L"";
	return unicode;
#endif
}

//***************************************************************************
// @brief std::string(ANSI)을 UTF-8 std::string으로 변환합니다.
// @param ansi 원본 ANSI std::string 문자열
// @return 변환된 UTF-8 std::string (실패 시 빈 문자열)
//***************************************************************************
string AnsiToUtf8(const std::string& ansi)
{
#ifdef __ICONVUTIL_H__
	if( ansi.empty() ) return "";
	try
	{
		thread_local Iconv::CIconvUtil conv("CP949", "UTF-8");
		return conv.Convert(ansi);
	}
	catch( ... )
	{
		return "";
	}
#else
	std::string utf8;
	if( AnsiToUtf8_String(utf8, ansi.c_str(), ansi.size()) != 0 ) return "";
	return utf8;
#endif
}

//***************************************************************************
// @brief UTF-8 std::string을 ANSI std::string으로 변환합니다.
// @param utf8 원본 UTF-8 std::string 문자열
// @return 변환된 ANSI std::string (실패 시 빈 문자열)
//***************************************************************************
string Utf8ToAnsi(const std::string& utf8)
{
#ifdef __ICONVUTIL_H__
	if( utf8.empty() ) return "";
	try
	{
		thread_local Iconv::CIconvUtil conv("UTF-8", "CP949");
		return conv.Convert(utf8);
	}
	catch( ... )
	{
		return "";
	}
#else
	std::string ansi;
	if( Utf8ToAnsi_String(ansi, utf8.c_str(), utf8.size()) != 0 ) return "";
	return ansi;
#endif
}

//***************************************************************************
// @brief std::string(ANSI)을 빌드 환경에 따른 _tstring으로 변환합니다.
// @param src 원본 std::string 문자열
// @return 변환된 _tstring
//***************************************************************************
_tstring StringToTString(const std::string& src)
{
#ifdef _UNICODE
	return AnsiToUnicode(src);
#else
	return src;
#endif
}

//***************************************************************************
// @brief _tstring을 std::string(ANSI)으로 변환합니다.
// @param src 원본 _tstring 문자열
// @return 변환된 std::string
//***************************************************************************
std::string TStringToString(const _tstring& src)
{
#ifdef _UNICODE
	return UnicodeToAnsi(src);
#else
	return src;
#endif
}

//***************************************************************************
// @brief std::wstring을 빌드 환경에 따른 _tstring으로 변환합니다.
// @param src 원본 std::wstring 문자열
// @return 변환된 _tstring
//***************************************************************************
_tstring WStringToTString(const std::wstring& src)
{
#ifdef _UNICODE
	return src;
#else
	return UnicodeToAnsi(src);
#endif
}

//***************************************************************************
// @brief _tstring을 std::wstring으로 변환합니다.
// @param src 원본 _tstring 문자열
// @return 변환된 std::wstring
//***************************************************************************
wstring TStringToWString(const _tstring& src)
{
#ifdef _UNICODE
	return src;
#else
	return AnsiToUnicode(src);
#endif
}
#endif