
//***************************************************************************
// CommonUtil.h : interface for the CommonUtil Functions.
//
//***************************************************************************

#ifndef __COMMONUTIL_H__
#define __COMMONUTIL_H__

#pragma once

#include <iostream>
using namespace std;

#include <functional>
#include <random>

#ifndef __DBENUM_H__
#include <DB/DBEnum.h> 
#endif

#ifndef __CONVERTCHARSET_H__
#include <Util/ConvertCharset.h> 
#endif

namespace SYSTEM
{
	inline DWORD CoreCount(void)
	{
		SYSTEM_INFO	SystemInfo;
		GetSystemInfo(&SystemInfo);
		return SystemInfo.dwNumberOfProcessors + 1;
	}
};

namespace SECURITY
{
	inline void Encrypt(char* pBuf, __int64& refKey, __int32 nLen)
	{
		if( nLen <= 0 )
			return;

		char* pKey = (char*)(&refKey);

		pBuf[0] = pBuf[0] ^ pKey[0];
		for( __int32 i = 1; i < nLen; ++i )
		{
			pBuf[i] = pBuf[i] ^ pBuf[i - 1] ^ pKey[i & 7];
		}

		refKey += nLen;
	}

	inline void Decrypt(char* pBuf, __int64& refKey, __int32 nLen)
	{
		if( nLen <= 0 )
			return;

		char* pKey = (char*)(&refKey);
		char source;
		char next_source;

		source = pBuf[0];
		pBuf[0] = pBuf[0] ^ pKey[0];
		for( __int32 i = 1; i < nLen; ++i )
		{
			next_source = pBuf[i];
			pBuf[i] = pBuf[i] ^ source ^ pKey[i & 7];
			source = next_source;
		}

		refKey += nLen;
	}
}

//***************************************************************************
//
inline _tstring operator+ (const _tstring& s, const char* psz)
{
	return s + StringToTString(psz);
}

//***************************************************************************
//
inline _tstring operator+ (const _tstring& s, const wchar_t* pwsz)
{
	return s + WStringToTString(pwsz);
}

//***************************************************************************
//
inline bool operator== (const _tstring& s, const char* psz)
{
	return (s == StringToTString(psz));
}

//***************************************************************************
//
inline bool operator== (const _tstring& s, const wchar_t* pwsz)
{
	return (s == WStringToTString(pwsz));
}

//***************************************************************************
//
inline bool operator!= (const _tstring& s, const char* psz)
{
	return (s != StringToTString(psz));
}

//***************************************************************************
//
inline bool operator!= (const _tstring& s, const wchar_t* pwsz)
{
	return (s != WStringToTString(pwsz));
}

//***************************************************************************
// 
inline _tstring ltrim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	size_t start = s.find_first_not_of(t);
	return (start == _tstring::npos) ? _T("") : s.substr(start);
}

//***************************************************************************
// 
inline _tstring rtrim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	size_t end = s.find_last_not_of(t);
	return (end == std::string::npos) ? _T("") : s.substr(0, end + 1);
}

//***************************************************************************
// 
inline _tstring trim(const _tstring& s, const TCHAR* t = _T(" \t\n\r\f\v"))
{
	return rtrim(ltrim(s, t));
}

//***************************************************************************
//
__inline _tstring replaceAll(const _tstring& message, const _tstring& pattern, const _tstring& replace)
{
	_tstring result = message;
	size_t pos = 0;
	size_t offset = 0;

	while( (pos = result.find(pattern, offset)) != _tstring::npos )
	{
		result.replace(result.begin() + pos, result.begin() + pos + pattern.size(), replace);
		offset = pos + replace.size();
	}

	return result;
}

//***************************************************************************
//
__inline string string_format_arg_list(const char* pszFmt, va_list args)
{
	if( !pszFmt ) return "";

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	char* pszBuffer = 0;

	while( result == -1 )
	{
		if( pszBuffer ) delete[] pszBuffer;
		pszBuffer = new char[length + 1];
		memset(pszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = vsnprintf(pszBuffer, length, pszFmt, args);
#pragma warning(pop)
		length *= 2;
	}
	string s(pszBuffer);
	delete[] pszBuffer;

	return s;
}

//***************************************************************************
//
template<typename ... Args>
inline string string_format(const char* pszFmt, Args ... args)
{
	if( !pszFmt ) return "";

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	char* pszBuffer = 0;

	while( result == -1 )
	{
		if( pszBuffer ) delete[] pszBuffer;
		pszBuffer = new char[length + 1];
		memset(pszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _snprintf_s(pszBuffer, length + 1, length, pszFmt, args ...);
#pragma warning(pop)
		length *= 2;
	}
	string s(pszBuffer);
	delete[] pszBuffer;

	return s;
}

//***************************************************************************
//
__inline _tstring tstring_format_arg_list(const TCHAR* ptszFmt, va_list args)
{
	if( !ptszFmt ) return _T("");

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete [] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _vsntprintf(ptszBuffer, length, ptszFmt, args);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete [] ptszBuffer;

	return s;
}

//***************************************************************************
//
template<typename ... Args>
inline _tstring tstring_cformat(const char* pszFmt, Args ... args)
{
	if( !pszFmt ) return _T("");

	TCHAR tszFmt[256];

#ifdef _UNICODE
	AnsiToUnicode(tszFmt, _countof(tszFmt), pszFmt, strlen(pszFmt) + 1);
#else
	_tcsncpy_s(tszFmt, _countof(tszFmt), pszFmt, _TRUNCATE);
#endif

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _sntprintf_s(ptszBuffer, length + 1, length, tszFmt, args ...);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

//***************************************************************************
//
template<typename ... Args>
inline _tstring tstring_tcformat(const TCHAR* ptszFmt, Args ... args)
{
	if( !ptszFmt ) return _T("");

	__int32 result = -1, length = MAX_BUFFER_SIZE;
	TCHAR* ptszBuffer = 0;

	while( result == -1 )
	{
		if( ptszBuffer ) delete[] ptszBuffer;
		ptszBuffer = new TCHAR[length + 1];
		memset(ptszBuffer, 0, length + 1);

#pragma warning(push)
#pragma warning(disable:4996)
		result = _sntprintf_s(ptszBuffer, length + 1, length, ptszFmt, args ...);
#pragma warning(pop)
		length *= 2;
	}
	_tstring s(ptszBuffer);
	delete[] ptszBuffer;

	return s;
}

template<typename T>
T random(T minimum, T maximum)
{
	std::random_device rd;
	std::mt19937 engine(rd());
	std::uniform_int_distribution<T> distribution(minimum, maximum);
	return distribution(engine);
}

template<typename T>
_tstring to_tstring(T value)
{
#ifdef _UNICODE
	return to_wstring(value);
#else
	return to_string(value);
#endif
}

//***************************************************************************
//
__inline _tstring addCommas(int64 number)
{
	_tstring numStr = to_tstring(number);
	int insertPosition = static_cast<int>(numStr.length() - 3);

	while( insertPosition > 0 )
	{
		numStr.insert(insertPosition, _T(","));
		insertPosition -= 3;
	}
	return numStr;
}

void		GetDBDSNString(TCHAR* ptszDSN, const EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName);
EDBClass	GetInt8ToDBClass(uint8 num);
uint32		GetUInt32(const char* pszText);
uint64		GetUInt64(const char* pszText);
uint32		GetUInt32(const wchar_t* pwszText);
uint64		GetUInt64(const wchar_t* pwszText);

#endif // ndef __COMMONUTIL_H__