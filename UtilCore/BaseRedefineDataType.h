
//***************************************************************************
// This File include Information about overriding the data type.
// 
//***************************************************************************

#ifndef __BASEREDEFINEDATATYPE_H__
#define __BASEREDEFINEDATATYPE_H__

#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

using namespace std;

typedef signed char			int8;
typedef signed short		int16;
typedef signed int			int32;
typedef _W64 long			time32;
typedef signed __int64		int64;

typedef unsigned char		uint8, uchar;
typedef unsigned short		uint16, ushort, wchar;
typedef unsigned int		uint32;
typedef unsigned long		ulong;
typedef unsigned __int64	uint64, time64;

#ifdef _IOSTREAM_
	#ifdef UNICODE
		typedef std::wstring		_tstring;
		typedef std::wstringstream  _tstringstream;
		typedef std::wifstream      _tifstream;
		typedef std::wofstream      _tofstream;
		typedef std::wregex			_tregex;
		typedef std::wcmatch		_tcmatch;
	#else
		typedef std::string			_tstring;
		typedef std::ostringstream  _tstringstream;
		typedef std::ifstream       _tifstream;
		typedef std::ofstream       _tofstream;
		typedef std::regex			_tregex;
		typedef std::cmatch			_tcmatch;
	#endif
#endif

#endif // ndef __BASEREDEFINEDATATYPE_H__