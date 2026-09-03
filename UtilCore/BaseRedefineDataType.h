
//***************************************************************************
// This File include Information about overriding the data type.
//	- 프로젝트 전역 공통 데이터 타입 및 매크로 정의
//	- 크로스플랫폼(Windows / Linux / macOS) 호환성을 고려하여 작성
//***************************************************************************

#ifndef UC_BASEREDEFINEDATATYPE_H
#define UC_BASEREDEFINEDATATYPE_H

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
//***************************************************************************
typedef int8_t			int8;
typedef int16_t			int16;
typedef int32_t			int32;
typedef int32_t			time32;
typedef int64_t			int64;

typedef uint8_t			uint8, uchar;
typedef uint16_t		uint16, ushort, wchar;
typedef uint32_t		uint32;
typedef unsigned long	ulong;
typedef uint64_t		uint64, time64;

//***************************************************************************
// @brief 빌드 환경(_UNICODE)에 따라 문자열 및 메모리 제어 매크로를 분기 정의합니다. (_t 시리즈)
//***************************************************************************
#ifdef UNICODE
	#define __TFUNCTION__				__FUNCTIONW__
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
	#define __TFUNCTION__				__FUNCTION__
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
//***************************************************************************
template<typename T>
using Atomic = std::atomic<T>;
using Mutex = std::mutex;
using CondVar = std::condition_variable;
using UniqueLock = std::unique_lock<std::mutex>;
using LockGuard = std::lock_guard<std::mutex>;

#endif // ndef UC_BASEREDEFINEDATATYPE_H