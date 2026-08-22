
//***************************************************************************
// This File include Information about overriding the data type.
//	- 프로젝트 전역 공통 데이터 타입 및 매크로 정의
//	- 크로스플랫폼(Windows / Linux / macOS) 호환성을 고려하여 작성
//***************************************************************************

#ifndef __BASEREDEFINEDATATYPE_H__
#define __BASEREDEFINEDATATYPE_H__

#pragma once

// 공통으로 자주 사용되는 표준 라이브러리 헤더 포함
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <cstdint> // 플랫폼 공통 표준 정수형 타입(int64_t 등) 사용을 위해 필수

// 주의: 헤더 파일 내 'using namespace std;'는 전역 네임스페이스 오염을 일으킬 수 있으나,
// 기존 프로젝트 구조 유지를 위해 포함되어 있습니다. (추후 제거 권장)
using namespace std;

//***************************************************************************
// 플랫폼별 환경 설정 및 기본 문자셋(TCHAR) 정의
//************************************************---------------------------
#ifdef _WIN32
#include <sqltypes.h>
#include <tchar.h>
#else
#ifndef _T
#define _T(x) x // 비Windows 환경에서 _T() 매크로 폴백 처리
#endif
#endif

//***************************************************************************
// 비Windows 환경이면서 TCHAR가 정의되지 않은 경우 유니코드 여부에 따라 정의
//***************************************************************************
#if !defined(_WIN32) && !defined(TCHAR)
#ifdef UNICODE
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif
#endif

//***************************************************************************
// 고정 크기 정수형 타입 재정의 (크로스플랫폼 표준 준수)
// MSVC 전용인 __int64나 _W64 대신 <cstdint>의 표준 타입을 사용합니다.
//************************************************---------------------------
typedef int8_t			int8;
typedef int16_t			int16;
typedef int32_t			int32;
typedef long			time32;
typedef int64_t			int64;

typedef uint8_t			uint8, uchar;
typedef uint16_t		uint16, ushort, wchar;
typedef uint32_t		uint32;
typedef unsigned long	ulong;
typedef uint64_t		uint64, time64;

//***************************************************************************
// 유니코드(UNICODE) 설정에 따른 동적 문자열 및 스트림 타입 정의 (_t 시리즈)
//************************************************---------------------------
#ifdef UNICODE
	#define _tcout						std::wcout
	#define _tcerr						std::wcerr
	typedef std::wstring				_tstring;
	typedef std::wstring_view			_tstring_view;
	typedef std::wstringstream			_tstringstream;
	typedef std::wifstream				_tifstream;
	typedef std::wofstream				_tofstream;
	typedef std::wregex					_tregex;
	typedef std::wcmatch				_tcmatch;
	typedef std::wsregex_token_iterator _tsregex_token_iterator;
#else
	#define _tcout						std::cout
	#define _tcerr						std::cerr
	typedef std::string					_tstring;
	typedef std::string_view			_tstring_view;
	typedef std::ostringstream			_tstringstream;
	typedef std::ifstream				_tifstream;
	typedef std::ofstream				_tofstream;
	typedef std::regex					_tregex;
	typedef std::cmatch					_tcmatch;
	typedef std::sregex_token_iterator	_tsregex_token_iterator;
#endif

//***************************************************************************
// 편의 템플릿 및 스마트 포인터 단축 매크로
//************************************************---------------------------
template<typename T>
using Atomic = std::atomic<T>;

// 클래스 이름을 받아 스마트 포인터 타입(예: CJobRef)을 자동 선언해 주는 매크로
#define USING_SHARED_PTR(name)	using name##Ref = std::shared_ptr<class name>;

//***************************************************************************
// 프로젝트 내 주요 클래스 스마트 포인터 정의
//***************************************************************************
USING_SHARED_PTR(CJob);
USING_SHARED_PTR(CJobQueue);

//***************************************************************************
// 배열 크기 및 바이트 크기 계산용 유틸리티 매크로
//************************************************---------------------------
#define size16(val)		static_cast<int16>(sizeof(val))
#define size32(val)		static_cast<int32>(sizeof(val))
#define len16(arr)		static_cast<int16>(sizeof(arr)/sizeof(arr[0]))
#define len32(arr)		static_cast<int32>(sizeof(arr)/sizeof(arr[0]))

#endif // ndef __BASEREDEFINEDATATYPE_H__