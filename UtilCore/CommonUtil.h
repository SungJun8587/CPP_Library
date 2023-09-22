
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
		if (nLen <= 0)
			return;

		char* pKey = (char*)(&refKey);

		pBuf[0] = pBuf[0] ^ pKey[0];
		for (__int32 i = 1; i < nLen; ++i) {
			pBuf[i] = pBuf[i] ^ pBuf[i - 1] ^ pKey[i & 7];
		}

		refKey += nLen;
	}

	inline void Decrypt(char* pBuf, __int64& refKey, __int32 nLen)
	{
		if (nLen <= 0)
			return;

		char* pKey = (char*)(&refKey);
		char source;
		char next_source;

		source = pBuf[0];
		pBuf[0] = pBuf[0] ^ pKey[0];
		for (__int32 i = 1; i<nLen; ++i)
		{
			next_source = pBuf[i];
			pBuf[i] = pBuf[i] ^ source ^ pKey[i & 7];
			source = next_source;
		}

		refKey += nLen;
	}
}

/*
//***************************************************************************
//
__inline std::string &ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<__int32, __int32>(std::isspace))));
	return s;
}

//***************************************************************************
//
__inline std::string &rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}
*/

//***************************************************************************
//
__inline std::string format_arg_list(const char *fmt, va_list args)
{
	if (!fmt) return "";
	__int32   result = -1, length = 256;
	char *buffer = 0;
	while (result == -1)
	{
		if (buffer) delete [] buffer;
		buffer = new char [length + 1];
		memset(buffer, 0, length + 1);
#pragma warning(push)
#pragma warning(disable:4996)
		result = _vsnprintf(buffer, length, fmt, args);
#pragma warning(pop)
		length *= 2;
	}
	std::string s(buffer);
	delete [] buffer;
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

uint32		GetUInt32(const char* pszText);
uint64		GetUInt64(const char* pszText);
uint32		GetUInt32(const wchar_t* pwszText);
uint64		GetUInt64(const wchar_t* pwszText);

#endif // ndef __COMMONUTIL_H__