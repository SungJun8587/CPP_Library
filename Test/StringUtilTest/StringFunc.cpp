
//***************************************************************************
// StringFunc.cpp : implementation of the C String Functions.
//
//***************************************************************************

#include "pch.h"
#include "StringFunc.h"

//***************************************************************************
// C언어 문자열 처리 함수 참조
// - https://dojang.io/course/view.php?id=6 : 코딩 도장
// - https://lypicfa.tistory.com/155 : 표준 라이브러리
// - https://yonjh.tistory.com/60 : 코딩시 UNICODE 관련 고려사항
// - https://modoocode.com/
// - https://jacking75.github.io/cpp_SecureString/
// - https://www.dotnetkorea.com/docs/c-language/string/?tabs=tabid-1
// - https://idsn.tistory.com/category/C%20%EC%96%B8%EC%96%B4/C%20%EC%96%B8%EC%96%B4%20%EB%A6%AC%EC%9D%B4%EB%B8%8C%EB%9F%AC%EB%A6%AC%20%ED%95%A8%EC%88%98
// - https://wonjayk.tistory.com/268 : StrSafe 함수들
// - https://kuaaan.tistory.com/66 : 문자열 함수 "_s" 시리즈 분석 (_tcsncpy_s 등)

//***************************************************************************
// This function or variable may be unsafe
// - strcpy
// - strncpy
// - strcat
// - strncat
// - strupr
// - strlwr
// - strset
// - strnset
// - strtok
// - strerror
// - mbstowcs
// - wcstombs 
// - sprintf
// - snprintf
// - vsprintf
// - vsnprintf
#ifdef _UNICODE
const wchar_t* defDest = L"wchar_t* dest";
const wchar_t* defFormat = L"const wchar_t* format";
#else
const char* defDest = "char* dest";
const char* defFormat = "const char* format";
#endif

//***************************************************************************
//
void Print_Va_List(TCHAR* ptszDest, size_t size, const TCHAR* ptszFmt, va_list arg_buff)
{
	ptszDest[0] = _T('\0');
	while( ptszFmt && *ptszFmt )
	{
		if( *ptszFmt == '%' )
		{
			ptszFmt++;
			if( *ptszFmt == 'd' )
			{
				_stprintf_s(ptszDest, size, _T("%s, %d"), ptszDest, va_arg(arg_buff, int));
			}
			else if( *ptszFmt == 'f' )
			{
				_stprintf_s(ptszDest, size, _T("%s, %f"), ptszDest, va_arg(arg_buff, float));
			}
			else if( *ptszFmt == 'c' )
			{
				_stprintf_s(ptszDest, size, _T("%s, '%c'"), ptszDest, va_arg(arg_buff, TCHAR));
			}
			else if( *ptszFmt == 'l' )
			{
				ptszFmt++;
				if( *ptszFmt == 'd' )
				{
					_stprintf_s(ptszDest, size, _T("%s, %ld"), ptszDest, va_arg(arg_buff, long));
				}
				else if( *ptszFmt == 'u' )
				{
					_stprintf_s(ptszDest, size, _T("%s, %lu"), ptszDest, va_arg(arg_buff, unsigned long));
				}
				else if( *ptszFmt == 'f' )
				{
					_stprintf_s(ptszDest, size, _T("%s, %lf"), ptszDest, va_arg(arg_buff, double));
				}
				else if( *ptszFmt == 'l' )
				{
					ptszFmt++;
					if( *ptszFmt == 'd' )
						_stprintf_s(ptszDest, size, _T("%s, %lld"), ptszDest, va_arg(arg_buff, long long));
					else if( *ptszFmt == 'u' )
						_stprintf_s(ptszDest, size, _T("%s, %llu"), ptszDest, va_arg(arg_buff, unsigned long long));
				}
			}
			else if( *ptszFmt == 's' )
			{
				_stprintf_s(ptszDest, size, _T("%s, \"%s\""), ptszDest, va_arg(arg_buff, TCHAR*));
			}
			else if( *ptszFmt == 'u' )
			{
				_stprintf_s(ptszDest, size, _T("%s, %u"), ptszDest, va_arg(arg_buff, unsigned int));
			}
			else if( *ptszFmt == 'z' )
			{
				ptszFmt++;
				if( *ptszFmt == 'u' )
				{
					_stprintf_s(ptszDest, size, _T("%s, %zu"), ptszDest, va_arg(arg_buff, size_t));
				}
			}
		}
		ptszFmt++;
	}
}

//***************************************************************************
//
void Print(const TCHAR* ptszFuncName, size_t nResult, const TCHAR* ptszFmt, ...)
{
	TCHAR tszFunc[500];
	va_list arg_ptr;

	va_start(arg_ptr, ptszFmt);
	_vstprintf_s(tszFunc, _countof(tszFunc), ptszFmt, arg_ptr);
	va_end(arg_ptr);

	_tprintf_s(_T("%s(%s) = %zu\n"), ptszFuncName, tszFunc, nResult);
}

//***************************************************************************
//
void Print(const TCHAR* ptszFuncName, TCHAR* ptszDest, const TCHAR* ptszFmt, ...)
{
	TCHAR tszFunc[500];
	va_list arg_ptr;

	va_start(arg_ptr, ptszFmt);
	_vstprintf_s(tszFunc, _countof(tszFunc), ptszFmt, arg_ptr);
	va_end(arg_ptr);

	_tprintf_s(_T("%s(%s) = %s\n"), ptszFuncName, tszFunc, ptszDest);
}

//***************************************************************************
// memcpy_s : 메모리를 지정된 길이만큼 복사(버퍼를 거치지 않고 복사하기 때문에 memmove보다 속도가 빠름)
// 
// errno_t memcpy_s(void *dest, size_t size, const void* src, size_t num); 
//	- 인수
//		[OUT] void* dest : 대상 버퍼
//		[IN] size_t size : 대상 버퍼의 크기  
//		[IN] const void* src : 원본 버퍼   
//		[IN] size_t num : 복사할 바이트 수
//	- 반환값 : 오류 번호
void func_memcpy_s()
{
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszDest[20];
	size_t num;

	num = sizeof(TCHAR) * 5;
	memcpy_s(tszDest, sizeof(tszDest), tszSrc, num);
	tszDest[num / 2] = _T('\0');
	Print(_T("memcpy_s"), tszDest, _T("%s, %zu, \"%s\", %zu"), defDest, sizeof(tszDest), tszSrc, num);

	num = sizeof(TCHAR) * _tcslen(tszSrc);
	memcpy_s(tszDest, sizeof(tszDest), tszSrc, num);
	tszDest[num / 2] = _T('\0');
	Print(_T("memcpy_s"), tszDest, _T("%s, %zu, \"%s\", %zu"), defDest, sizeof(tszDest), tszSrc, num);
}

//***************************************************************************
// strcpy_s, wcscpy_s, _tcscpy_s : 문자열을 복사
// 
// errno_t strcpy_s(char* dest, size_t size, const char* src); 
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기
//		[IN] const char* src : 원본 문자열
//	- 반환값 : 오류 번호
void func_strcopy_s()
{
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszDest[20];

	_tcscpy_s(tszDest, _countof(tszDest), tszSrc);
	Print(_T("_tcscpy_s"), tszDest, _T("%s, %zu, \"%s\""), defDest, _countof(tszDest), tszSrc);
}

//***************************************************************************
// strncpy_s, wcsncpy_s, _tcsncpy_s : 문자열을 지정된 길이만큼 복사
// 
// errno_t strncpy_s(char* dest, size_t size, const char* src, size_t num);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기
//		[IN] const char* src : 원본 문자열
//		[IN] size_t num : 원본에서 복사할 최대 문자 수
//	- 반환값 : 오류 번호
void func_strncopy_s()
{
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszDest[20];
	size_t num;

	num = 8;
	_tcsncpy_s(tszDest, _countof(tszDest), tszSrc, num);
	Print(_T("_tcsncpy_s"), tszDest, _T("%s, %zu, \"%s\", %zu"), defDest, _countof(tszDest), tszSrc, num);

	num = _TRUNCATE;
	_tcsncpy_s(tszDest, _countof(tszDest), tszSrc, num);
	Print(_T("_tcsncpy_s"), tszDest, _T("%s, %zu, \"%s\", %zu"), defDest, _countof(tszDest), tszSrc, num);
}

//***************************************************************************
// strcat_s, wcscat_s, _tcscat_s : 두 개의 문자열을 연결
// 
// errno_t strcat_s(char* dest, size_t size, const char* src); 
//	- 인수
//		[IN/OUT] char* dest : ﻿원본 문자열 및 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기
//		[IN] const char* src : 붙일 문자열
//	- 반환값 : 오류 번호
void func_strcat_s()
{
	TCHAR tszDest[100];
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszAppend[20] = _T(" Language");

	_tcscpy_s(tszDest, _countof(tszDest), tszSrc);

	_tcscat_s(tszDest, _countof(tszDest), tszAppend);
	Print(_T("_tcscat_s"), tszDest, _T("\"%s\", %zu, \"%s\""), tszSrc, _countof(tszDest), tszAppend);
}

//***************************************************************************
// strncat_s, wcsncat_s, _tcsncat_s : 두 개의 문자열을 지정된 길이만큼 연결
// 
// errno_t strncat_s(char* dest, size_t size, const char* src, size_t num);
//	- 인수
//		[IN/OUT] char* dest : ﻿원본 문자열 및 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] const char* src : 붙일 문자열
//		[IN] size_t num : 붙일 최대 문자 수 
//	- 반환값 : 오류 번호
void func_strncat_s()
{
	TCHAR tszDest[100];
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszAppend[20] = _T(" Language");
	size_t num;

	_tcscpy_s(tszDest, _countof(tszDest), tszSrc);

	num = 8;
	_tcsncat_s(tszDest, _countof(tszDest), tszAppend, num);
	Print(_T("_tcsncat_s"), tszDest, _T("\"%s\", %zu, \"%s\", %zu"), tszSrc, _countof(tszDest), tszAppend, num);

	_tcscpy_s(tszDest, _countof(tszDest), tszSrc);

	num = _TRUNCATE;
	_tcsncat_s(tszDest, _countof(tszDest), tszAppend, num);
	Print(_T("_tcsncat_s"), tszDest, _T("\"%s\", %zu, \"%s\", %zu"), tszSrc, _countof(tszDest), tszAppend, num);
}

//***************************************************************************
// _strupr_s, _wcsupr_s, _tcsupr_s : 문자열을 대문자로 변환
// 
// errno_t _strupr_s(const char* str, size_t size);
//	- 인수
//		[IN/OUT] const char* str : 원본 문자열 및 변환후 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//	- 반환값 : 오류 번호
void func_strupr_s()
{
	TCHAR tszSrc[100] = _T("C Programming Language");
	TCHAR tszBuffer[100];

	_tcscpy_s(tszBuffer, _countof(tszBuffer), tszSrc);

	_tcsupr_s(tszBuffer, _countof(tszBuffer));
	Print(_T("_tcsupr_s"), tszBuffer, _T("\"%s\", %zu"), tszSrc, _countof(tszBuffer));
}

//***************************************************************************
// _strlwr_s, _wcslwr_s, _tcslwr_s : 문자열을 소문자로 변환
// 
// errno_t _strlwr_s(const char* str, size_t size);
//	- 인수
//		[IN] const char* str : 원본 문자열 및 변환후 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//	- 반환값 : 오류 번호
void func_strlwr_s()
{
	TCHAR tszSrc[100] = _T("C Programming Language");
	TCHAR tszBuffer[100];

	_tcscpy_s(tszBuffer, _countof(tszBuffer), tszSrc);

	_tcslwr_s(tszBuffer, _countof(tszBuffer));
	Print(_T("_tcslwr_s"), tszBuffer, _T("\"%s\", %zu"), tszSrc, _countof(tszBuffer));
}

//***************************************************************************
// strset_s, wcsset_s, _tcsset_s : 문자열을 특정 문자 또는 아스키 값으로 초기화
// 
// ﻿errno_t strset_s(const char* str, size_t size, int c);
//	- 인수
//		[IN/OUT] const char* str : 원본 문자열 ﻿및 변환후 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] int c : 초기화 문자 또는 아스키 값
//	- 반환값 : 오류 번호
void func_strset_s()
{
	TCHAR tszSrc[100] = _T("C Programming");
	TCHAR tszBuffer[100];
	TCHAR c = _T('*');

	_tcscpy(tszBuffer, tszSrc);

	_tcsset_s(tszBuffer, _countof(tszBuffer), c);
	Print(_T("_tcsset_s"), tszBuffer, _T("\"%s\", %zu, \"%c\""), tszSrc, _countof(tszBuffer), c);
}

//***************************************************************************
// strnset_s, wcsnset_s, _tcsnset_s : 문자열을 특정 문자 또는 아스키 값으로 일정 길이만큼 초기화
// 
// ﻿errno_t strnset_s(const char* str, size_t size, int c, size_t num);
//	- 인수
//		[IN] const char* str : 원본 문자열 ﻿및 변환후 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] int c : 초기화 문자 또는 아스키 값
//		[IN] size_t num : 초기화 할 문자 수  
//	- 반환값 : 오류 번호
void func_strnset_s()
{
	TCHAR tszSrc[100] = _T("C Programming");
	TCHAR tszBuffer[100];
	TCHAR c = _T('*');
	size_t num;

	_tcscpy(tszBuffer, tszSrc);

	num = 5;
	_tcsnset_s(tszBuffer, _countof(tszBuffer), c, num);
	Print(_T("_tcsnset_s"), tszBuffer, _T("\"%s\", %zu, \"%c\", %zu"), tszSrc, _countof(tszBuffer), c, num);

	num = _TRUNCATE;
	_tcsnset_s(tszBuffer, _countof(tszBuffer), c, num);
	Print(_T("_tcsnset_s"), tszBuffer, _T("\"%s\", %zu, \"%c\", %zu"), tszSrc, _countof(tszBuffer), c, num);
}

//***************************************************************************
// strtok_s, wcstok_s, _tcstok_s : 문자열을 토큰들로 분리
// 
// char* strtok_s(char* str, const char* delimiters, char** context);
//	- 인수
//		[IN] char* str : 원본 문자열
//		[IN] const char* delimiters : 구분 문자들을 포함하고 있는 문자열
//		[IN] char** context : ﻿다음 반환될 토큰의 위치를 저장하는 포인터
//	- 반환값 : 원본 문자열에서 찾은 마지막 토큰의 포인터를 반환하며 토큰이 더이상 없다면 NULL을 반환
void func_strtok_s()
{
	TCHAR tszBuffer1[100] = _T("C/Programming/Language, C++_Programming_Language");
	TCHAR tszBuffer2[100];
	TCHAR tszDelimiters[10] = _T(" _,/");
	TCHAR tszDest[200] = _T("");
	TCHAR* ptsz;
	TCHAR* ptszContext = NULL;

	_tcscpy_s(tszBuffer2, _countof(tszBuffer2), tszBuffer1);

	ptsz = _tcstok_s(tszBuffer2, tszDelimiters, &ptszContext);
	while( ptsz != NULL )
	{
		_sntprintf_s(tszDest, _countof(tszDest), _T("%s\n%s"), tszDest, ptsz);

		ptsz = _tcstok_s(nullptr, tszDelimiters, &ptszContext);
	}

	Print(_T("_tcstok_s"), tszDest, _T("\"%s\", \"%s\", \"%s\""), tszBuffer1, tszDelimiters, ptszContext);
}

//***************************************************************************
// mbstowcs_s : 멀티바이트 문자의 시퀀스를 와이드바이트 문자의 시퀀스로 변환
// 
// errno_t mbstowcs_s(size_t* pcnt, wchar_t* dest, size_t size, const char* src, size_t num)
//	- 인수
//		[IN] size_t* pcnt : 와이드바이트 문자 수를 반환
//		[IN] wchar_t* dest : 대상 문자열
//      [IN] size_t size : 대상 문자열 크기
//		[IN] const char* src : 원본 문자열
//		[IN] size_t num : 변환할 와이드바이트 문자의 최대 수   
//	- 반환값 : 오류 번호
void func_mbstowcs_s()
{
	wchar_t wszDest[100];
	char szSrc[] = "abc가나다123";
	size_t size;

	size_t num = _countof(szSrc);
	mbstowcs_s(&size, wszDest, _countof(wszDest), szSrc, num);

	wprintf_s(L"mbstowcs_s(wchar_t* dest, const char* src, %zu) = %s, size=%zu", num, wszDest, size);
	_tprintf_s(_T("\n"));
}

//***************************************************************************
// wcstombs_s : 와이드바이트 문자의 시퀀스를 멀티바이트 문자의 시퀀스로 변환
// 
// errno_t wcstombs_s(size_t* pcnt, char* dest, size_t size, const wchar_t* src, size_t num)
//	- 인수
//		[IN] size_t* pcnt : 멀티바이트 바이트 수를 반환
//		[IN] char* dest : 대상 문자열
//      [IN] size_t size : 대상 문자열 크기
//		[IN] const wchar_t* src : 원본 문자열
//		[IN] size_t num : 변환할 멀티바이트 문자의 최대 수     
//	- 반환값 : 오류 번호
void func_wcstombs_s()
{
	char szDest[100];
	wchar_t wszSrc[] = L"abc가나다123";
	size_t size;

	size_t num = _countof(wszSrc);
	wcstombs_s(&size, szDest, _countof(szDest), wszSrc, num);

	printf_s("wcstombs_s(char* dest, %zu, const wchar_t* src, %zu) = %s, size=%zu", _countof(szDest), num, szDest, size);
	_tprintf_s(_T("\n"));
}

//***************************************************************************
// strerror_s, _wcserror_s, _tcserror_s : 오류 번호 errnum에 해당하는 오류 메시지 문자열 출력
// 
// errno_t strerror_s(char* dest, size_t size, int errnum);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] int errnum : 오류 번호 
//	- 반환값 : 오류 번호
void func_strerror_s()
{
	TCHAR tszDest[100];
	int errnum = 1;

	_tcserror_s(tszDest, _countof(tszDest), errnum);
	Print(_T("_tcserror_s"), tszDest, _T("%zu, %d"), _countof(tszDest), errnum);
}

//***************************************************************************
// sprintf_s, swprintf_s, _stprintf_s : 형식화된 데이터를 버퍼로 출력
// 
// int sprintf_s(char* dest, size_t size, const char* format, argument-list);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_sprintf_s()
{
	TCHAR tszDest[100];
	TCHAR tszBuffer[100] = _T("C Programming");
	int nVer = 22;

	_stprintf_s(tszDest, _countof(tszDest), _T("Language is %s, Version is %d"), tszBuffer, nVer);
	Print(_T("_stprintf_s"), tszDest, _T("%s, %zu"), defDest, _countof(tszDest));
}

//***************************************************************************
// _snprintf_s, _snwprintf_s, _sntprintf_s : 형식화된 데이터를 버퍼로 출력
// 
// int _snprintf_s(char* dest, size_t size, size_t count, const char *format, argument-list);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] size_t count : 종료를 포함하지 않고 쓸 최대 문자 수 또는 _TRUNCATE((size_t)-1)
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_snprintf_s()
{
	TCHAR tszDest[100];
	TCHAR tszBuffer[100] = _T("C Programming");
	int nVer = 22;
	size_t count;

	count = 10;
	_sntprintf_s(tszDest, _countof(tszDest), count, _T("Language is %s, Version is %d"), tszBuffer, nVer);
	Print(_T("_sntprintf_s"), tszDest, _T("%s, %zu, %zu"), defDest, _countof(tszDest), count);

	count = _TRUNCATE;
	_sntprintf_s(tszDest, _countof(tszDest), count, _T("Language is %s, Version is %d"), tszBuffer, nVer);
	Print(_T("_sntprintf_s"), tszDest, _T("%s, %zu, %zu"), defDest, _countof(tszDest), count);
}

//***************************************************************************
// vsprintf_s, vswprintf_s, _vstprintf_s : 가변인자 리스트 데이터를 버퍼로 출력
// 
// int vsprintf_s(char* dest, size_t size, const char *format, va_list arg_ptr);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] const char* format : 출력할 포멧
//		[IN] va_list arg_ptr : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환하고, 오류가 발생하면 음수 값을 반환
void func_vsprintf_s(const TCHAR* ptszFmt, ...)
{
	TCHAR tszDest[100];
	TCHAR tszArg[MAX_BUFFER_SIZE];
	va_list arg_ptr;
	va_list arg_copybuff;

	va_start(arg_ptr, ptszFmt);

	va_copy(arg_copybuff, arg_ptr);
	Print_Va_List(tszArg, _countof(tszArg), ptszFmt, arg_copybuff);

	_vstprintf_s(tszDest, _countof(tszDest), ptszFmt, arg_ptr);
	va_end(arg_ptr);
	Print(_T("_vstprintf_s"), tszDest, _T("%s, %zu, \"%s\""), defDest, _countof(tszDest), ptszFmt);
}

//***************************************************************************
// vsnprintf_s, vsnwprintf_s, _vsntprintf_s : 가변인자 리스트 데이터를 버퍼로 출력
// 
// int vsnprintf_s(char* dest, size_t size, size_t count, const char* format, va_list arg_ptr);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t size : 대상 문자열 버퍼의 크기 
//		[IN] size_t count : 종료를 포함하지 않고 쓸 최대 문자 수 또는 _TRUNCATE((size_t)-1)
//		[IN] const char* format : 출력할 포멧
//		[IN] va_list arg_ptr : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환하고, 오류가 발생하면 음수 값을 반환
void func_vsnprintf_s(const TCHAR* ptszFmt, ...)
{
	TCHAR tszDest[100];
	TCHAR tszArg[MAX_BUFFER_SIZE];
	va_list arg_ptr;
	va_list arg_copybuff;
	size_t count;

	count = 10;
	va_start(arg_ptr, ptszFmt);

	va_copy(arg_copybuff, arg_ptr);
	Print_Va_List(tszArg, _countof(tszArg), ptszFmt, arg_copybuff);

	_vsntprintf_s(tszDest, _countof(tszDest), count, ptszFmt, arg_ptr);
	va_end(arg_ptr);
	Print(_T("_vsntprintf_s"), tszDest, _T("%s, %zu, %zu, \"%s\""), defDest, _countof(tszDest), count, ptszFmt);

	count = _TRUNCATE;
	va_start(arg_ptr, ptszFmt);
	_vsntprintf_s(tszDest, _countof(tszDest), count, ptszFmt, arg_ptr);
	va_end(arg_ptr);
	Print(_T("_vsntprintf_s"), tszDest, _T("%s, %zu, %zu, \"%s\""), defDest, _countof(tszDest), count, ptszFmt);
}


//***************************************************************************
// memcpy : 메모리를 지정된 길이만큼 복사(버퍼를 거치지 않고 복사하기 때문에 memmove보다 속도가 빠름)
// 
// void *memcpy(void* dest, const void* src, size_t num); 
//	- 인수
//		[OUT] void* dest : 대상 버퍼
//		[IN] const void* src : 원본 버퍼   
//		[IN] size_t num : 복사할 바이트 수
//	- 반환값 : 대상 버퍼
void func_memcpy()
{
	TCHAR tszSrc[] = _T("C Programming");
	TCHAR tszDest[30];
	size_t num;

	num = sizeof(TCHAR) * 6;
	memcpy(tszDest, tszSrc, num);
	tszDest[num / 2] = _T('\0');
	Print(_T("memcpy"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);

	num = sizeof(TCHAR) * _tcslen(tszSrc);
	memcpy(tszDest, tszSrc, num);
	tszDest[num / 2] = _T('\0');
	Print(_T("memcpy"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);
}

//***************************************************************************
// memset : 메모리의 내용(값)을 원하는 크기만큼 특정 값으로 설정
// 
// void* memset(void* ptr, int value, size_t num);
//	- 인수
//		[OUT] void* dest : 대상 버퍼
//		[IN] int value : 초기화 값  
//		[IN] size_t num : 초기화 할 바이트의 수
//	- 반환값 : 초기화된 메모리의 포인터을 반환, 오류가 발생하면 NULL을 반환
void func_memset()
{
	TCHAR tszDest[30];
	int value = 0;
	size_t num;

	num = sizeof(TCHAR) * 6;
	memset(tszDest, value, num);
	tszDest[num] = _T('\0');
	Print(_T("memset"), tszDest, _T("%s, %d, %zu"), defDest, value, num);
}

//***************************************************************************
// memmove : 메모리를 지정된 길이만큼 복사(중간에 버퍼를 이용해서 복사를 하므로 memcpy보다 안정성이 좋음)
// 
// void* memmove (void* dest, const void* src, size_t num);
//	- 인수
//		[OUT] void* dest : 대상 버퍼
//		[IN] const void* src : 원본 버퍼 
//		[IN] size_t num : 복사할 바이트 수
//	- 반환값 : 대상 버퍼
void func_memmove()
{
	TCHAR tszSrc[] = _T("C Programming");
	TCHAR tszDest[30];
	size_t num;

	num = sizeof(TCHAR) * 6;
	memmove(tszDest, tszSrc, num);
	tszDest[num] = _T('\0');
	Print(_T("memmove"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);

	num = sizeof(TCHAR) * _tcslen(tszSrc);
	memmove(tszDest, tszSrc, num);
	tszDest[num] = _T('\0');
	Print(_T("memmove"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);
}

//***************************************************************************
// memcmp : 두 개의 메모리 블록을 비교
// 
// int memcmp(const void* ptr1, const void* ptr2, size_t num);
//	- 인수 : 두 개의 메모리 블록의 관계에 따라 아래와 같이 정수 값을 리턴
//		[IN] const void* ptr1 : 비교할 메모리 블록
//		[IN] const void* ptr2 : 비교할 메모리 블록
//		[IN] size_t num : 비교할 바이트 수
//	- 반환값 : 두 메모리 블록의 관계에 따라 정수값을 리턴
//      > 0 : ptr1과 ptr2가 가리키는 메모리 블록에서 앞에서 부터 처음으로 다른 바이트를 살펴 보는데, 그 바이트를 unsigned char(아스키 )로 해석하였을 때, 그 값이 ptr1이 더 큰 경우
//      = 0 : num 바이트에 두 메모리 블록이 정확히 같음
//		< 0 : ptr1과 ptr2가 가리키는 메모리 블록에서 앞에서 부터 처음으로 다른 바이트를 살펴 보는데, 그 바이트를 unsigned char(아스키 )로 해석하였을 때, 그 값이 ptr2가 더 큰 경우
void func_memcmp()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	int nResult;

	size_t num1 = 2;
	nResult = memcmp(tszBuffer1, tszBuffer2, num1);
	Print(_T("memcmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num1);

	size_t num2 = _countof(tszBuffer2);
	nResult = memcmp(tszBuffer1, tszBuffer2, num2);
	Print(_T("memcmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num2);
}

//***************************************************************************
// memchr : 메모리 블록에서의 문자를 찾음
// 
// void* memchr(void* ptr, int value, size_t num);
//	- 인수
//		[OUT] void* ptr : 원본 버퍼
//		[IN] int value : 찾을 문자(아스키값)
//		[IN] size_t num : 검색을 시작한 부분부터 검색을 수행할 만큼의 바이트 수
//	- 반환값 : 원본 버퍼에서 value값과 일치하는 값이 가장 먼저 나타나는 곳을 가리키는 포인터를 반환하고, 없으면 NULL을 반환
void func_memchr()
{
	TCHAR tszBuffer[100] = _T("C Programming Language");
	TCHAR* ptszDest;
	int   ch = _T('m');
	size_t num;

	num = sizeof(TCHAR) * _tcslen(tszBuffer);
	ptszDest = (TCHAR*)memchr(tszBuffer, ch, num);
	Print(_T("memchr"), ptszDest, _T("\"%s\", \"%c\", %zu"), tszBuffer, ch, num);
}

//***************************************************************************
// sizeof : 실제로 차지하고 있는 메모리의 크기 반환
// 
// size_t sizeof(const char *str);
//	- 인수
//		[IN] const char *str : 원본 문자열
//	- 반환값 : 문자열의 길이(마지막 NULL값 제외)
void func_sizeof()
{
	TCHAR tszBuffer1[30] = _T("C Programming");
	TCHAR tszBuffer2[20] = _T(" Language");
	const TCHAR* tszBuffer3 = _T("씨 프로그래밍 언어");

	Print(_T("sizeof"), sizeof(tszBuffer1), _T("%s"), defDest);
	Print(_T("sizeof"), sizeof(tszBuffer2), _T("%s"), defDest);
	Print(_T("sizeof"), sizeof(tszBuffer3), _T("%s"), defDest);
}

//***************************************************************************
// _countof : 정적으로 할당된 배열의 요소 수를 반환
//	- #define _countof(array) (sizeof(array) / sizeof(array[0]))
// size_t _countof(array);
//	- 인수
//		[IN] array : 배열 이름
//	- 반환값 : 배열의 요소 수
void func_countof()
{
	TCHAR tszBuffer1[30] = _T("C Programming");
	TCHAR tszBuffer2[20] = _T(" Language");

	Print(_T("_countof"), _countof(tszBuffer1), _T("tszBuffer1"));
	Print(_T("_countof"), _countof(tszBuffer2), _T("tszBuffer2"));
}

//***************************************************************************
// strlen, wcslen, _tcslen : 문자열의 길이 반환
// 
// size_t strlen(const char* str);
//	- 인수
//		[IN] const char *str : 원본 문자열
//	- 반환값 : 문자열의 길이(종료문자 '\0'은 길이에서 제외)을 반환
void func_strlen()
{
	TCHAR tszBuffer1[20] = _T("C Programming");
	TCHAR tszBuffer2[20] = { _T('C'), _T(' '), _T('P'), _T('r'), _T('o'), _T('g'), _T('r'), _T('a'), _T('m'), _T('m'), _T('i'), _T('n'), _T('g'), _T('\0') };
	const TCHAR* tszBuffer3 = _T("씨 프로그래밍 언어");

	Print(_T("_tcslen"), _tcslen(tszBuffer1), _T("\"%s\""), tszBuffer1);
	Print(_T("_tcslen"), _tcslen(tszBuffer2), _T("\"%s\""), tszBuffer2);
	Print(_T("_tcslen"), _tcslen(tszBuffer3), _T("\"%s\""), tszBuffer3);
}

//***************************************************************************
// strcpy, wcscpy, _tcscpy : 문자열을 복사
// 
// char* strcpy(char* dest, const char* src);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] const char* src : 원본 문자열
//	- 반환값 : 대상 문자열
void func_strcopy()
{
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszDest[20];

	_tcscpy(tszDest, tszSrc);
	Print(_T("_tcscpy"), tszDest, _T("%s, \"%s\""), defDest, tszSrc);
}

//***************************************************************************
// strncpy, wcsncpy, _tcsncpy : 문자열을 지정된 길이만큼 복사
// 
// char* strncpy(char* dest, const char* src, size_t num);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] const char* src : 원본 문자열
//		[IN] size_t num : 원본에서 복사할 최대 문자 수
//	- 반환값 : 대상 문자열
void func_strncopy()
{
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszDest[20];
	size_t num;

	num = 5;
	_tcsncpy(tszDest, tszSrc, num);
	tszDest[num] = _T('\0');
	Print(_T("_tcsncpy"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);

	num = _countof(tszDest) - 1;
	_tcsncpy(tszDest, tszSrc, num);
	tszDest[num] = _T('\0');
	Print(_T("_tcsncpy"), tszDest, _T("%s, \"%s\", %zu"), defDest, tszSrc, num);
}

//***************************************************************************
// _strdup, _wcsdup, _tcsdup : 문자열 포인터 변수에 문자열을 복사
// 
// char* strdup(const char* src);
//	- 인수
//		[IN] const char* src : 원본 문자열
//	- 반환값 : 문자열 포인터
void func_strdup()
{
	TCHAR tszSrc[100] = _T("C Programming");
	TCHAR* ptszDest;

	ptszDest = _tcsdup(tszSrc);

	Print(_T("_tcsdup"), ptszDest, _T("\"%s\""), tszSrc);
}

//***************************************************************************
// strcat, wcscat, _tcscat : 두 개의 문자열을 연결
// 
// char* strcat(char* dest, const char* src);
//	- 인수
//		[IN/OUT] char* dest : ﻿원본 문자열 및 대상 문자열
//		[IN] const char* src : 붙일 문자열
//	- 반환값 : 대상 문자열
void func_strcat()
{
	TCHAR tszDest[100];
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszAppend[20] = _T(" Language");

	_tcscpy(tszDest, tszSrc);
	_tcscat(tszDest, tszAppend);

	Print(_T("_tcscat"), tszDest, _T("\"%s\", \"%s\""), tszSrc, tszAppend);
}

//***************************************************************************
// strncat, wcsncat, _tcsncat : 두 개의 문자열을 지정된 길이만큼 연결
// 
// char* strncat(char* dest, const char* src, size_t num);
//	- 인수
//		[IN/OUT] char* dest : ﻿원본 문자열 및 대상 문자열
//		[IN] const char* src : 붙일 문자열
//		[IN] size_t num : ﻿붙일 최대 문자 수
//	- 반환값 : 대상 문자열
void func_strncat()
{
	TCHAR tszDest[100];
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszAppend[20] = _T(" Language");
	size_t num;

	_tcscpy(tszDest, tszSrc);

	num = 5;
	_tcsncat(tszDest, tszAppend, num);
	Print(_T("_tcsncat"), tszDest, _T("\"%s\", \"%s\", %zu"), tszSrc, tszAppend, num);

	_tcscpy(tszDest, tszSrc);

	num = _countof(tszDest) - 1;
	_tcsncat(tszDest, tszAppend, num);
	Print(_T("_tcsncat"), tszDest, _T("\"%s\", \"%s\", %zu"), tszSrc, tszAppend, num);
}

//***************************************************************************
// strcmp, wcscmp, _tcscmp : 대소문자를 구분하여 두 개의 문자열을 비교
// 
// int strcmp(const char* str1, const char* str2);
//	- 인수
//		[IN] const char* str1 : 비교할 문자열
//		[IN] const char* str2 : 비교할 문자열
//	- 반환값 : 두 문자열의 관계에 따라 정수값을 리턴
//      > 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 큰 경우
//      = 0 : 문자가 모두 일치
//		< 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 작은 경우
void func_strcmp()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	TCHAR tszBuffer3[] = _T("abcd");
	int nResult;

	nResult = _tcscmp(tszBuffer1, tszBuffer2);
	Print(_T("_tcscmp"), nResult, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer2);

	nResult = _tcscmp(tszBuffer1, tszBuffer3);
	Print(_T("_tcscmp"), nResult, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer3);
}

//***************************************************************************
// strncmp, wcsncmp, _tcsncmp : 대소문자를 구분하여 지정된 길이만큼 두 개의 문자열을 비교
// 
// int strncmp(const char* str1, const char* str2, size_t num);
//	- 인수 : 두 문자열의 관계에 따라 정수값을 리턴
//		[IN] const char* str1 : 비교할 문자열
//		[IN] const char* str2 : 비교할 문자열
//		[IN] size_t num : 비교할 최대 문자 수
//	- 반환값 : ﻿두 문자열의 관계에 따라 정수값을 리턴
//      > 0 : str1과 str2 문자열에서 비교한 num개의 문자 중 최초로 일치하지 않는 문자의 값이 str1이 더 큰 경우
//      = 0 : num개의 문자가 모두 일치
//		< 0 : str1과 str2 문자열에서 비교한 num개의 문자 중 최초로 일치하지 않는 문자의 값이 str1이 더 작은 경우
void func_strncmp()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	int nResult;

	size_t num1 = 2;
	nResult = _tcsncmp(tszBuffer1, tszBuffer2, num1);
	Print(_T("_tcsncmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num1);

	size_t num2 = _countof(tszBuffer2);
	nResult = _tcsncmp(tszBuffer1, tszBuffer2, num2);
	Print(_T("_tcsncmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num2);
}

//***************************************************************************
// stricmp, wcsicmp, _tcsicmp : 대소문자를 구분하지 않고 두 개의 문자열을 비교
// 
// int stricmp(const char* str1, const char* str2);
//	- 인수
//		[IN] const char* str1 : 비교할 문자열
//		[IN] const char* str2 : 비교할 문자열
//	- 반환값 : 두 문자열의 관계에 따라 정수값을 리턴
//      > 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 큰 경우
//      = 0 : 문자가 모두 일치
//		< 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 작은 경우
void func_stricmp()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	TCHAR tszBuffer3[] = _T("abcd");
	int nResult;

	nResult = _tcsicmp(tszBuffer1, tszBuffer2);
	Print(_T("_tcsicmp"), nResult, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer2);

	nResult = _tcsicmp(tszBuffer1, tszBuffer3);
	Print(_T("_tcsicmp"), nResult, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer3);
}

//***************************************************************************
// strnicmp, wcsnicmp, _tcsnicmp : 대소문자를 구분하지 않고 지정된 길이만큼 두 개의 문자열을 비교
// 
// int strnicmp(const char* str1, const char* str2, size_t num);
//	- 인수 : 두 문자열의 관계에 따라 정수값을 리턴
//		[IN] const char* str1 : 비교할 문자열
//		[IN] const char* str2 : 비교할 문자열
//		[IN] size_t num : 비교할 최대 문자 수
//	- 반환값 : ﻿두 문자열의 관계에 따라 정수값을 리턴
//      > 0 : str1과 str2 문자열에서 비교한 num개의 문자 중 최초로 일치하지 않는 문자의 값이 str1이 더 큰 경우
//      = 0 : num개의 문자가 모두 일치
//		< 0 : str1과 str2 문자열에서 비교한 num개의 문자 중 최초로 일치하지 않는 문자의 값이 str1이 더 작은 경우
void func_strnicmp()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	int nResult;

	size_t num1 = 2;
	nResult = _tcsnicmp(tszBuffer1, tszBuffer2, num1);
	Print(_T("_tcsnicmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num1);

	size_t num2 = _countof(tszBuffer2);
	nResult = _tcsnicmp(tszBuffer1, tszBuffer2, num2);
	Print(_T("_tcsnicmp"), nResult, _T("\"%s\", \"%s\", %zu"), tszBuffer1, tszBuffer2, num2);
}

//***************************************************************************
// strcoll, wcscoll, _tcscoll : ﻿LC_COLLATE 설정에 따라 두 개의 문자열을 비교
// 
// int strcoll(const char* str1, const char* str2);
//	- 인수 : 두 문자열의 관계에 따라 정수값을 리턴
//		[IN] const char* str1 : 비교할 문자열
//		[IN] const char* str2 : 비교할 문자열
//	- 반환값
//      > 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 큰 경우
//      = 0 : 문자가 모두 일치
//		< 0 : str1과 str2 문자열에서 최초로 일치하지 않는 문자의 값이 str1이 더 작은 경우
void func_strcoll()
{
	TCHAR tszBuffer1[] = _T("abcd");
	TCHAR tszBuffer2[] = _T("abCd");
	int nResult;

	nResult = _tcscoll(tszBuffer1, tszBuffer2);
	Print(_T("_tcscoll"), nResult, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer2);
}

//***************************************************************************
// strchr, wcschr, _tcschr : 문자열의 앞에서부터 검색하여 문자열 내에 일치하는 문자가 있는지 검사
// 
// char* strchr(const char* str, int c);
//	- 인수
//		[IN] const char* str : 원본 문자열
//		[IN] int c : 찾을 문자(아스키값)
//	- 반환값 : 원본 문자열에서 일치하는 문자가 가장 먼저 나타나는 곳을 가리키는 포인터를 반환하고, 없으면 NULL을 반환
void func_strchr()
{
	TCHAR tszBuffer[100] = _T("C Programming Language");
	TCHAR* ptszDest;
	int   ch = _T('m');

	ptszDest = _tcschr(tszBuffer, ch);
	Print(_T("_tcschr"), ptszDest, _T("\"%s\", \"%c\""), tszBuffer, ch);
}

//***************************************************************************
// strrchr, wcsrchr, _tcsrchr : 문자열의 뒤에서부터 검색하여 문자열 내에 일치하는 문자가 있는지 검사
// 
// char* strrchr(const char* str, int c);
//	- 인수
//		[IN] const char* str : 원본 문자열
//		[IN] int ch : 찾을 문자(아스키값)
//	- 반환값 : 원본 문자열 뒷 부분에서 일치하는 문자가 가장 먼저 나타나는 곳을 가리키는 포인터를 반환하고, 없으면 NULL을 반환
void func_strrchr()
{
	TCHAR tszBuffer[100] = _T("C Programming Language");
	TCHAR* ptszDest;
	int   ch = _T('m');

	ptszDest = _tcsrchr(tszBuffer, ch);
	Print(_T("_tcsrchr"), ptszDest, _T("\"%s\", \"%c\""), tszBuffer, ch);
}

//***************************************************************************
// strstr, wcsstr, _tcsstr : 문자열의 앞에서부터 검색하여 문자열 내에 일치하는 문자열이 있는지 검사
// 
// char* strstr(const char* str1, const char* str2);
//	- 인수
//		[IN] const char* str1 : 원본 문자열
//		[IN] const char* str2 : 찾을 문자열
//	- 반환값 : 원본 문자열에 찾을 문자열이 가장 먼저 나타나는 곳을 가리키는 포인터를 반환하고, 없으면 NULL을 반환
void func_strstr()
{
	TCHAR tszBuffer1[100] = _T("C Programming Language");
	TCHAR tszBuffer2[100] = _T("ing");
	TCHAR* ptszDest;

	ptszDest = _tcsstr(tszBuffer1, tszBuffer2);
	Print(_T("_tcsstr"), ptszDest, _T("\"%s\", \"%s\""), tszBuffer1, tszBuffer2);
}

//***************************************************************************
// _strupr, _wcsupr, _tcsupr : 문자열을 대문자로 변환
// 
// char* _strupr(const char* str);
//	- 인수
//		[IN/OUT] const char* str : ﻿원본 문자열 및 변환후 대상 문자열
//	- 반환값 : 원본 문자열을 모두 대문자로 변환하고 결과로 시작 포인터를 반환
void func_strupr()
{
	TCHAR tszSrc[100] = _T("C Programming Language");
	TCHAR tszBuffer[100];
	TCHAR* ptszDest;

	_tcscpy(tszBuffer, tszSrc);

	ptszDest = _tcsupr(tszBuffer);
	Print(_T("_tcsupr"), ptszDest, _T("\"%s\""), tszSrc);
}

//***************************************************************************
// _strlwr, _wcslwr, _tcslwr : 문자열을 소문자로 변환
// 
// char* _strlwr(const char* str);
//	- 인수
//		[IN/OUT] const char* str : ﻿원본 문자열 및 변환후 대상 문자열
//	- 반환값 : 원본 문자열을 모두 소문자로 변환하고 결과로 시작 포인터를 반환
void func_strlwr()
{
	TCHAR tszSrc[100] = _T("C Programming Language");
	TCHAR tszBuffer[100];
	TCHAR* ptszDest;

	_tcscpy(tszBuffer, tszSrc);

	ptszDest = _tcslwr(tszBuffer);
	Print(_T("_tcslwr"), ptszDest, _T("\"%s\""), tszSrc);
}

//***************************************************************************
// strset, wcsset, _tcsset : 문자열을 특정 문자 또는 아스키 값으로 초기화
// 
// size_t strset(const char* str, int c);
//	- 인수
//		[IN/OUT] const char* str : ﻿원본 문자열 및 변환후 대상 문자열
//		[IN] int c : 초기화 문자 또는 아스키 값
//	- 반환값 : 초기화된 문자열의 포인터을 반환
void func_strset()
{
	TCHAR tszSrc[100] = _T("C Programming");
	TCHAR tszBuffer[100];
	TCHAR c = _T('*');
	TCHAR* tszDest;

	_tcscpy(tszBuffer, tszSrc);

	tszDest = _tcsset(tszBuffer, c);
	Print(_T("_tcsset"), tszDest, _T("\"%s\", \"%c\""), tszSrc, c);
}

//***************************************************************************
// strnset, wcsnset, _tcsnset : 문자열을 특정 문자 또는 아스키 값으로 일정 길이만큼 초기화
// 
// size_t strnset(const char* str, int c, size_t num);
//	- 인수
//		[IN/OUT] const char* str : ﻿원본 문자열 및 변환후 대상 문자열
//		[IN] int c : 초기화 문자 또는 아스키 값
//		[IN] size_t num : 초기화 할 문자 수  
//	- 반환값 : 초기화된 문자열의 포인터을 반환
void func_strnset()
{
	TCHAR tszSrc[100] = _T("C Programming");
	TCHAR tszBuffer[100];
	TCHAR c = _T('*');
	TCHAR* tszDest;
	size_t num;

	_tcscpy(tszBuffer, tszSrc);

	num = 5;
	tszDest = _tcsnset(tszBuffer, c, num);
	Print(_T("_tcsnset"), tszDest, _T("\"%s\", \"%c\", %zu"), tszSrc, c, num);

	num = _countof(tszBuffer);
	tszDest = _tcsnset(tszBuffer, c, num);
	Print(_T("_tcsnset"), tszDest, _T("\"%s\", \"%c\", %zu"), tszSrc, c, num);
}

//***************************************************************************
// strrev, wcsrev, _tcsrev : 문자열을 역순으로 변환
// 
// char* strrev(const char* str);
//	- 인수
//		[IN/OUT] const char* str : ﻿원본 문자열 및 변환후 대상 문자열
//	- 반환값 : 원본 문자열을 역순으로 변환하고 결과로 시작 포인터를 반환
void func_strrev()
{
	TCHAR tszSrc[100] = _T("C Programming Language");
	TCHAR tszBuffer[100];
	TCHAR* ptszDest;

	_tcscpy(tszBuffer, tszSrc);

	ptszDest = _tcsrev(tszBuffer);
	Print(_T("_tcsrev"), ptszDest, _T("\"%s\""), tszSrc);
}

//***************************************************************************
// strtok, wcstok, _tcstok : 문자열을 토큰들로 분리
// 
// char* strtok(char* str, const char* delimiters);
//	- 인수
//		[IN] char* str : 원본 문자열
//		[IN] const char* delimiters : 구분 문자들을 포함하고 있는 문자열
//	- 반환값 : 원본 문자열에서 찾은 마지막 토큰의 포인터를 반환하며 토큰이 더이상 없다면 NULL을 반환
void func_strtok()
{
	TCHAR tszBuffer1[100] = _T("C/Programming/Language, C++_Programming_Language");
	TCHAR tszBuffer2[100];
	TCHAR tszDelimiters[10] = _T(" _,/");
	TCHAR tszDest[200] = _T("");
	TCHAR* pch;

	_tcscpy_s(tszBuffer2, _countof(tszBuffer2), tszBuffer1);

	pch = _tcstok(tszBuffer2, tszDelimiters);
	while( pch != NULL )
	{
		_sntprintf_s(tszDest, _countof(tszDest), _T("%s\n%s"), tszDest, pch);

		pch = _tcstok(nullptr, tszDelimiters);
	}

	Print(_T("_tcstok"), tszDest, _T("\"%s\", \"%s\""), tszBuffer1, tszDelimiters);
}

//***************************************************************************
// strpbrk, wcspbrk, _tcspbrk : 대소문자를 구분하여 문자열에서 특정 문자의 위치를 검색
// 
// char* strpbrk(const char* str, const char* charset);
//	- 인수
//		[IN] const char* str : 원본 문자열
//		[IN] const char* charset : 문자셋 문자열
//	- 반환값 : 원본 문자열에서 찾은 첫 번째 문자가 있는 문자열의 포인트을 반환하며 검색 실패시 NULL을 반환
void func_strpbrk()
{
	TCHAR tszBuffer1[100] = _T("f(x) = a+b-(c*d/e)");
	TCHAR tszBuffer2[100];
	TCHAR tszCharSet[20] = _T(" ()=+-*/");
	TCHAR tszDest[200] = _T("");
	TCHAR* pch = tszBuffer2;

	_tcscpy(tszBuffer2, tszBuffer1);

	do
	{
		pch = _tcspbrk(pch, tszCharSet);

		if( pch != NULL )
		{
			_sntprintf_s(tszDest, _countof(tszDest), _T("%s\n%s"), tszDest, pch);
			++pch;
		}
	} while( pch != NULL );

	Print(_T("_tcspbrk"), tszDest, _T("\"%s\", \"%s\""), tszBuffer1, tszCharSet);
}

//***************************************************************************
// strcspn, wcscspn, _tcscspn : 일치하는 첫 번째 문자의 오프셋 찾기(문자열에서 일치되는 첫 번째 문자의 위치를 검색)
// 
// size_t strcspn(const char* str1, const char* str2);
//	- 인수
//		[IN] const char* str1 : 원본 문자열
//		[IN] const char* str2 : 검색할 문자들을 포함하고 있는 문자열
//	- 반환값 : str2의 문자들 중 원본 문자열에 일치하는 것이 있다면 첫번째로 일치하는 문자까지 도달하기 위해 읽어들인 문자들의 수를 반환하며, 없다면 원본 문자열의 길이를 반환
void func_strcspn()
{
	size_t i;
	TCHAR tszBuffer[100] = _T("CPP 18 Programming");
	TCHAR tszFilter[20] = _T("1234567890");

	i = _tcscspn(tszBuffer, tszFilter);
	Print(_T("_tcscspn"), i + 1, _T("\"%s\", \"%s\""), tszBuffer, tszFilter);
}

//***************************************************************************
// strspn, wcsspn, _tcsspn : 일치하지 않는 첫 번째 문자의 오프셋 찾기(문자열에서 일치되지 않는 첫 번째 문자의 위치를 검색)
// 
// size_t strspn(const char* str1, const char* str2);
//	- 인수
//		[IN] const char* str1 : 원본 문자열
//		[IN] const char* str2 : 검색할 문자들을 포함하고 있는 문자열
//	- 반환값 : str2의 문자들 중 원본 문자열에 일치되지 않는 첫번째 문자까지 도달하기 위해 읽어들인 문자들의 수를 반환하며, 없다면 원본 문자열의 길이를 반환
void func_strspn()
{
	size_t i;
	TCHAR tszBuffer[100] = _T("18 C Programming");
	TCHAR tszFilter[20] = _T("1234567890");

	i = _tcsspn(tszBuffer, tszFilter);
	Print(_T("_tcsspn"), i, _T("\"%s\", \"%s\""), tszBuffer, tszFilter);
}

//***************************************************************************
// strxfrm, wcsxfrm, _tcsxfrm : 문자열을 ﻿LC_COLLATE에 맞게 문자열을 일정 길이만큼 복사
// 
// size_t strxfrm(char* dest, const char* src, size_t num)
//	- 인수
//		[IN] char* dest : 대상 문자열
//		[IN] const char* src : 원본 문자열
//		[IN] size_t num : 복사할 최대 문자 수  
//	- 반환값 : 문자열의 길이(종료문자 '\0'은 길이에서 제외)
void func_strxfrm()
{
	TCHAR tszDest[100];
	TCHAR tszSrc[20] = _T("C Programming");
	TCHAR tszBuffer[20];
	size_t num, len;

	_tcscpy(tszBuffer, tszSrc);

	num = 5;
	len = _tcsxfrm(tszDest, tszBuffer, num);
	tszDest[num] = _T('\0');
	Print(_T("_tcsxfrm"), tszDest, _T("\"%s\", \"%c\", %zu"), tszSrc, num);

	num = _countof(tszBuffer);
	len = _tcsxfrm(tszDest, tszBuffer, num);
	Print(_T("_tcsxfrm"), tszDest, _T("\"%s\", \"%c\", %zu"), tszSrc, num);
}

//***************************************************************************
// mbstowcs : 멀티바이트 문자의 시퀀스를 와이드바이트 문자의 시퀀스로 변환
// 
// size_t mbstowcs(wchar_t* dest, const char* src, size_t num)
//	- 인수
//		[IN] wchar_t* dest : 대상 문자열
//		[IN] const char* src : 원본 문자열
//		[IN] size_t num : 변환할 와이드바이트 문자의 최대 수   
//	- 반환값 : 와이드바이트 문자 수를 반환
void func_mbstowcs()
{
	wchar_t wszDest[100];
	char szSrc[] = "abc가나다123";
	size_t size;

	size_t num = _countof(szSrc);
	size = mbstowcs(wszDest, szSrc, num);
	wszDest[size] = L'\0';

	wprintf_s(L"mbstowcs(wchar_t* dest, const char* src, %zu) = %s, size=%zu", num, wszDest, size);
	_tprintf_s(_T("\n"));
}

//***************************************************************************
// wcstombs : 와이드바이트 문자의 시퀀스를 멀티바이트 문자의 시퀀스로 변환
// 
// size_t wcstombs(char* dest, const wchar_t* src, size_t num)
//	- 인수
//		[IN] char* dest : 대상 문자열
//		[IN] const wchar_t* src : 원본 문자열
//		[IN] size_t num : 변환할 멀티바이트 문자의 최대 수     
//	- 반환값 : 멀티바이트 바이트 수를 반환
void func_wcstombs()
{
	char szDest[100];
	wchar_t wszSrc[] = L"abc가나다123";
	size_t size;

	size_t num = _countof(wszSrc);
	size = wcstombs(szDest, wszSrc, num);
	szDest[size] = '\0';

	printf_s("wcstombs(char* dest, const wchar_t* src, %zu) = %s, size=%zu", num, szDest, size);
	_tprintf_s(_T("\n"));
}

//***************************************************************************
// strerror, _wcserror, _tcserror : 오류 번호 errnum에 해당하는 오류 메시지 문자열 출력
// 
// char* strerror(int errnum); 
//	- 인수
//		[IN] int errnum : 오류 번호
//	- 반환값 : 오류 번호에 해당하는 오류 문자열의 포인터을 반환
void func_strerror()
{
	TCHAR* ptszDest;
	int errnum = 1;

	ptszDest = _tcserror(errnum);

	Print(_T("_tcserror"), ptszDest, _T("%d"), errnum);
}

//***************************************************************************
// printf, wprintf, _tprintf : 형식화된 문자 출력
// 
// int printf(const char* format, ...);
//	- 인수
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_printf()
{
	TCHAR tszBuffer[100] = _T("C Programming");
	int nVer = 21;

	_tprintf_s(_T("_tprintf(%s, argument-list) = "), defFormat);
	_tprintf(_T("Language is %s, Version %d"), tszBuffer, nVer);
	_tprintf_s(_T("\n"));
}

//***************************************************************************
// sprintf, swprintf, _stprintf : 형식화된 데이터를 버퍼로 출력
// 
// int sprintf(char* dest, const char* format, argument-list);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_sprintf()
{
	TCHAR tszDest[100];
	TCHAR tszBuffer[100] = _T("C Programming");
	int nVer = 22;

	_stprintf(tszDest, _T("Language is %s, Version is %d"), tszBuffer, nVer);
	Print(_T("_stprintf"), tszDest, _T("%s, %s, argument-list"), defDest, defFormat);
}

//***************************************************************************
// snprintf, snwprintf, _sntprintf : 형식화된 데이터를 버퍼로 출력
// 
// int snprintf(char* dest, size_t num, const char* format, argument-list);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t num : 추가할 최대 문자 수  
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_snprintf()
{
	TCHAR tszDest[100];
	TCHAR tszBuffer[100] = _T("C++ Programming");
	int nVer = 22;
	size_t num;

	num = 5;
	_sntprintf(tszDest, num, _T("Language is %s, Version is %d"), tszBuffer, nVer);
	tszDest[num] = _T('\0');
	Print(_T("_sntprintf"), tszDest, _T("%s, %zu, %s, argument-list"), defDest, num, defFormat);

	num = _countof(tszDest) - 1;
	_sntprintf(tszDest, num, _T("Language is %s, Version is %d"), tszBuffer, nVer);
	tszDest[num] = _T('\0');
	Print(_T("_sntprintf"), tszDest, _T("%s, %zu, %s, argument-list"), defDest, num, defFormat);
}

//***************************************************************************
// vprintf, vwprintf, _vtprintf : 형식화된 문자 출력
// 
// int printf(const char* format, ...);
//	- 인수
//		[IN] const char* format : 출력할 포멧
//		[IN] argument-list : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환
void func_vprintf(const TCHAR* ptszFmt, ...)
{
	TCHAR tszArg[MAX_BUFFER_SIZE];
	va_list arg_ptr;
	va_list arg_copybuff;

	va_start(arg_ptr, ptszFmt);

	va_copy(arg_copybuff, arg_ptr);
	Print_Va_List(tszArg, _countof(tszArg), ptszFmt, arg_copybuff);

	_tprintf_s(_T("_vtprintf(%s, \"%s\"%s) = "), defFormat, ptszFmt, tszArg);

	_vtprintf(ptszFmt, arg_ptr);
	va_end(arg_ptr);

	_tprintf_s(_T("\n"));
}

//***************************************************************************
// vsprintf, vswprintf, _vstprintf : 가변인자 리스트 데이터를 버퍼로 출력
// 
// int vsprintf(char* dest, const char *format, va_list arg_ptr);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] const char* format : 출력할 포멧
//		[IN] va_list arg_ptr : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환하고, 오류가 발생하면 음수 값을 반환
void func_vsprintf(const TCHAR* ptszFmt, ...)
{
	TCHAR tszDest[100];
	TCHAR tszArg[MAX_BUFFER_SIZE];
	va_list arg_ptr;
	va_list arg_copybuff;

	va_start(arg_ptr, ptszFmt);

	va_copy(arg_copybuff, arg_ptr);
	Print_Va_List(tszArg, _countof(tszArg), ptszFmt, arg_copybuff);

	_vstprintf(tszDest, ptszFmt, arg_ptr);
	va_end(arg_ptr);
	Print(_T("_vstprintf"), tszDest, _T("%s, \"%s\"%s"), defDest, ptszFmt, tszArg);
}

//***************************************************************************
// vsnprintf, vsnwprintf, _vsntprintf : 가변인자 리스트 데이터를 버퍼로 출력
// 
// int vsnprintf(char* dest, size_t num, const char *format, va_list arg_ptr);
//	- 인수
//		[OUT] char* dest : 대상 문자열
//		[IN] size_t num : 추가할 최대 문자 수   
//		[IN] const char* format : 출력할 포멧
//		[IN] va_list arg_ptr : 포멧에 대응되는 변수들
//	- 반환값 : 끝 NULL 문자를 계산하지 않고 배열에 작성된 바이트 수를 반환하고, 오류가 발생하면 음수 값을 반환
void func_vsnprintf(const TCHAR* ptszFmt, ...)
{
	TCHAR tszDest[100];
	TCHAR tszArg[MAX_BUFFER_SIZE];
	va_list arg_ptr;
	va_list arg_copybuff;
	size_t num;

	num = 6;
	va_start(arg_ptr, ptszFmt);

	va_copy(arg_copybuff, arg_ptr);
	Print_Va_List(tszArg, _countof(tszArg), ptszFmt, arg_copybuff);

	_vsntprintf(tszDest, num, ptszFmt, arg_ptr);
	tszDest[num] = _T('\0');

	va_end(arg_ptr);
	Print(_T("_vsntprintf"), tszDest, _T("%s, %zu, \"%s\"%s"), defDest, num, ptszFmt, tszArg);

	num = _countof(tszDest) - 1;
	va_start(arg_ptr, ptszFmt);
	_vsntprintf(tszDest, num, ptszFmt, arg_ptr);
	tszDest[num] = _T('\0');
	va_end(arg_ptr);
	Print(_T("_vsntprintf"), tszDest, _T("%s, %zu, \"%s\"%s"), defDest, num, ptszFmt, tszArg);
}