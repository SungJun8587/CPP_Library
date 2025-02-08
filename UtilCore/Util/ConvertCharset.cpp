
//***************************************************************************
// ConvertCharset.cpp: implementation of the ConvertCharset Functions.
//
//***************************************************************************

#include "pch.h"
#include "ConvertCharset.h"

//***************************************************************************
// CodePage
//	- CP_ACP : 시스템 기본 Windows ANSI 코드 페이지
//	- CP_MACCP : Macintosh 코드 페이지
//	- CP_OEMCP : OEM 코드 페이지
//	- CP_SYMBOL : 기호 코드 페이지(42)
//	- CP_THREAD_ACP : 현재 스레드에 대한 Windows ANSI 코드 페이지
//	- CP_UTF7 : UTF-7
//	- CP_UTF8 : UTF-8
// 
// sizeof, _countof 비교
//	sizeof : 할당받는 메모리 크기
//  _countof : 배열에 개수
//	Ex)
//		wchar_t wszBuffer[100];
//		sizeof(wszBuffer) : sizeof(wchar_t) * 100 = 200
//		_countof(wszBuffer) : 100
//
// 		char szSource[20] = "안녕123하세요";
//		wchar_t wszSource[20] = L"안녕123하세요";
//  
//		int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szSource, -1, nullptr, 0);		// 버퍼에 기록된 문자 수를 반환 : nLength(9) = 문자수(8) + 1('\0')
//		nLength = WideCharToMultiByte(CP_ACP, 0, wszSource, -1, nullptr, 0, nullptr, NULL);   // 버퍼에 기록된 바이트 수를 반환 : nLength(14) = 바이트 수(13) + 1('\0')
//      nLength = strlen(szSource);     // 멀티바이트 바이트 수                           // 한글은 2바이트, 아스키 문자는 1바이트 : nLength(13)
//      nLength = wcslen(wszSource);	// 와이드바이트 문자 수							// 버퍼에 기록된 문자 수를 반환 : nLength(8) 
size_t GetMultiByteLen(int nCodePage, const TCHAR* ptszSource)
{
	int iLength = static_cast<int>(_tcslen(ptszSource));
	if( ptszSource == nullptr || iLength == 0 ) return -1;

#ifdef _UNICODE
	iLength = WideCharToMultiByte(nCodePage, 0, ptszSource, iLength + 1, nullptr, 0, nullptr, NULL);
	if( iLength == 0 ) return -1;
	iLength--;
#endif

	return iLength;
}

//***************************************************************************
// 멀티바이트 문자열을 와이드바이트 문자열로 변환
//	[out] wchar_t* unicode : 와이드바이트 문자열
//	[out] size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
//	[in] const char* ansi : 멀티바이트 문자열
//	[in] size_t ansi_size : 멀티바이트 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD AnsiToUnicode(wchar_t* unicode, size_t unicode_size, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	do
	{
		if( ansi == nullptr || ansi_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		if( unicode_size < (size_t)required_cch )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		unicode_size = required_cch;

		if( 0 == ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			unicode, static_cast<int>(unicode_size)
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 멀티바이트 문자열로 변환
//	[out] char* ansi : 멀티바이트 문자열
//	[out] size_t ansi_size : 멀티바이트 문자열 바이트 수 : 바이트 수 + 1('\0') 
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToAnsi(char* ansi, size_t ansi_size, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		if( ansi_size < (size_t)required_cch )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		ansi_size = required_cch;

		if( 0 == ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			ansi, static_cast<int>(ansi_size),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 UTF8 문자열로 변환
//	[out] char* utf8 : UTF8 문자열
//	[out] size_t utf8_size : UTF8 문자열 바이트 수 : 바이트 수 + 1('\0') 
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToUtf8(char* utf8, size_t utf8_size, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		if( utf8_size < (size_t)required_cch )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		utf8_size = required_cch;

		if( 0 == ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			utf8, static_cast<int>(utf8_size),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// UTF8 문자열을 와이드바이트 문자열로 변환
//	[out] wchar_t* unicode : 와이드바이트 문자열
//	[out] size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
//	[in] const char* utf8 : UTF8 문자열
//	[in] const size_t utf8_size : UTF8 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD Utf8ToUnicode(wchar_t* unicode, size_t unicode_size, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	do
	{
		if( utf8 == nullptr || utf8_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		if( unicode_size < (size_t)required_cch )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		unicode_size = required_cch;

		if( 0 == ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			unicode, static_cast<int>(unicode_size)
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// ansi string을 utf-8 string로 변환하기 위해선 ansi string -> unicode string -> utf-8 string의 변환 과정
DWORD AnsiToUtf8(char* utf8, size_t utf8_size, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	if( ansi == nullptr || ansi_size == 0 )
		return ERROR_INVALID_PARAMETER;

	wchar_t unicode[MAX_BUFFER_SIZE];
	size_t unicode_size = MAX_BUFFER_SIZE;

	if( (error = AnsiToUnicode(unicode, unicode_size, ansi, ansi_size)) != 0 ) return error;
	if( (error = UnicodeToUtf8(utf8, utf8_size, unicode, unicode_size)) != 0 ) return error;

	return error;
}

//***************************************************************************
// utf-8 string을 ansi string로 변환하기 위해선 utf-8 string -> unicode string -> ansi string의 변환 과정
DWORD Utf8ToAnsi(char* ansi, size_t ansi_size, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	if( utf8 == nullptr || utf8_size == 0 )
		return ERROR_INVALID_PARAMETER;

	wchar_t unicode[MAX_BUFFER_SIZE];
	size_t unicode_size = MAX_BUFFER_SIZE;

	if( (error = Utf8ToUnicode(unicode, unicode_size, utf8, utf8_size)) != 0 ) return error;
	if( (error = UnicodeToAnsi(ansi, ansi_size, unicode, unicode_size)) != 0 ) return error;

	return error;
}

#ifdef __MEMBUFFER_H__
//***************************************************************************
// 멀티바이트 문자열을 와이드바이트 문자열로 변환
//	[out] CMemBuffer<wchar_t>& unicode : 와이드바이트 문자열 버퍼
//	[in] const char* ansi : 멀티바이트 문자열
//	[in] size_t ansi_size : 멀티바이트 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD AnsiToUnicode(CMemBuffer<wchar_t>& unicode, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	do
	{
		if( ansi == nullptr || ansi_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		unicode.Init(required_cch);

		if( 0 == ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			const_cast<wchar_t*>(unicode.GetBuffer()),
			static_cast<int>(unicode.GetBufLength())
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 멀티바이트 문자열로 변환
//	[out] CMemBuffer<char>& ansi : 멀티바이트 문자열 버퍼
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToAnsi(CMemBuffer<char>& ansi, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		ansi.Init(required_cch);

		if( 0 == ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			const_cast<char*>(ansi.GetBuffer()), static_cast<int>(ansi.GetBufSize()),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 UTF-8 문자열로 변환
//	[out] CMemBuffer<char>& utf8 : UTF-8 문자열 버퍼
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToUtf8(CMemBuffer<char>& utf8, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		utf8.Init(required_cch);

		if( 0 == ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			const_cast<char*>(utf8.GetBuffer()), static_cast<int>(utf8.GetBufSize()),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// UTF-8 문자열을 와이드바이트 문자열로 변환
//	[out] CMemBuffer<wchar_t>& unicode : 와이드바이트 문자열 버퍼
//	[in] const char* utf8 : UTF-8 문자열
//	[in] const size_t utf8_size : UTF-8 바이트수
DWORD Utf8ToUnicode(CMemBuffer<wchar_t>& unicode, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	do
	{
		if( utf8 == nullptr || utf8_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		unicode.Init(required_cch);

		if( 0 == ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			const_cast<wchar_t*>(unicode.GetBuffer()), static_cast<int>(unicode.GetBufLength())
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 멀티바이트 문자열을 UTF-8 문자열로 변환
//	[out] CMemBuffer<char>& utf8 : UTF-8 문자열 버퍼
//	[in] const char* ansi : 멀티바이트 문자열
//	[in] const size_t ansi_size : 멀티바이트 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD AnsiToUtf8(CMemBuffer<char>& utf8, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	if( ansi == nullptr || ansi_size == 0 )
		return ERROR_INVALID_PARAMETER;

	CMemBuffer<wchar_t> unicode;

	if( (error = AnsiToUnicode(unicode, ansi, ansi_size)) != 0 ) return error;
	if( (error = UnicodeToUtf8(utf8, unicode.GetBuffer(), unicode.GetBufLength())) != 0 ) return error;

	return error;
}

//***************************************************************************
//
DWORD Utf8ToAnsi(CMemBuffer<char>& ansi, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	if( utf8 == nullptr || utf8_size == 0 )
		return ERROR_INVALID_PARAMETER;

	CMemBuffer<wchar_t> unicode;

	if( (error = Utf8ToUnicode(unicode, utf8, utf8_size)) != 0 ) return error;
	if( (error = UnicodeToAnsi(ansi, unicode.GetBuffer(), unicode.GetBufLength())) != 0 ) return error;

	return error;
}

//***************************************************************************
//
bool ByteToTChar(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer)
{
	int		nLength = 0;
	int     nSrcLen = 0;
	char* pszSource = nullptr;

	pszSource = (char*)pbBuffer;
	nSrcLen = (int)strlen(pszSource) + 1;

#ifdef _UNICODE
	if( (nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, nullptr, 0)) == 0 ) return false;

	TDestination.Init(nLength);

	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSource, nSrcLen, TDestination.GetBuffer(), nLength) == 0 ) return false;
#else
	nLength = nSrcLen;

	TDestination.Init(nLength);

	_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszSource, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
bool TCharToByte(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer)
{
	int		nLength = 0;
	int     nSrcLen = (int)_tcslen(ptszBuffer) + 1;
	char* pszDestination = nullptr;

#ifdef _UNICODE
	if( (nLength = WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, (LPSTR)pszDestination, 0, nullptr, NULL)) == 0 ) return false;

	Destination.Init(nLength);

	pszDestination = (char*)Destination.GetBuffer();

	if( WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, (LPSTR)pszDestination, (int)nLength, nullptr, NULL) == 0 ) return false;
#else
	nLength = nSrcLen;

	Destination.Init(nLength);

	pszDestination = (char*)Destination.GetBuffer();

	_tcsncpy_s(pszDestination, nLength, ptszBuffer, _TRUNCATE);
#endif

	return true;
}
#endif

#ifdef _STRING_
//***************************************************************************
// 멀티바이트 문자열을 와이드바이트 문자열로 변환
//	[out] std::wstring& unicode : 와이드바이트 문자열
//	[in] const char* ansi : 멀티바이트 문자열
//	[in] size_t ansi_size : 멀티바이트 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD AnsiToUnicode_String(std::wstring& unicode, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	do
	{
		if( ansi == nullptr || ansi_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		unicode.clear();

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		unicode.resize(required_cch);

		if( 0 == ::MultiByteToWideChar(
			CP_ACP,
			0,
			ansi, static_cast<int>(ansi_size),
			const_cast<wchar_t*>(unicode.c_str()), static_cast<int>(unicode.size())
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 멀티바이트 문자열로 변환
//	[out] std::string& ansi : 멀티바이트 문자열
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToAnsi_String(std::string& ansi, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		ansi.clear();

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		ansi.resize(required_cch);

		if( 0 == ::WideCharToMultiByte(
			CP_ACP,
			0,
			unicode, static_cast<int>(unicode_size),
			const_cast<char*>(ansi.c_str()), static_cast<int>(ansi.size()),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// 와이드바이트 문자열을 UTF8 문자열로 변환
//	[out] std::string& utf8 : UTF8 문자열
//	[in] const wchar_t* unicode : 와이드바이트 문자열
//	[in] const size_t unicode_size : 와이드바이트 문자열 문자수 : 문자 개수 + 1('\0')
DWORD UnicodeToUtf8_String(std::string& utf8, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		utf8.clear();

		// 버퍼에 기록된 바이트 수를 반환 : 바이트 수 + 1('\0')
		int required_cch = ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			nullptr, 0,
			nullptr, nullptr
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		utf8.resize(required_cch);

		if( 0 == ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			unicode, static_cast<int>(unicode_size),
			const_cast<char*>(utf8.c_str()), static_cast<int>(utf8.size()),
			nullptr, nullptr
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
// UTF8 문자열을 와이드바이트 문자열로 변환
//	[out] std::wstring& unicode : 와이드바이트 문자열
//	[in] const char* utf8 : UTF8 문자열
//	[in] const size_t utf8_size : UTF8 문자열 바이트 수 : 바이트 수 + 1('\0') 
DWORD Utf8ToUnicode_String(std::wstring& unicode, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	do
	{
		if( utf8 == nullptr || utf8_size == 0 )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

		unicode.clear();

		// 버퍼에 기록된 문자 수를 반환 : 문자 수 + 1('\0')
		int required_cch = ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			nullptr, 0
		);

		if( 0 == required_cch )
		{
			error = ::GetLastError();
			break;
		}

		unicode.resize(required_cch);

		if( 0 == ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8, static_cast<int>(utf8_size),
			const_cast<wchar_t*>(unicode.c_str()), static_cast<int>(unicode.size())
		) )
		{
			error = ::GetLastError();
			break;
		}

	} while( false );

	return error;
}

//***************************************************************************
//
DWORD AnsiToUtf8_String(std::string& utf8, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	if( ansi == nullptr || ansi_size == 0 )
		return ERROR_INVALID_PARAMETER;

	std::wstring unicode;

	if( (error = AnsiToUnicode_String(unicode, ansi, ansi_size)) != 0 ) return error;
	if( (error = UnicodeToUtf8_String(utf8, unicode.c_str(), unicode.size())) != 0 ) return error;

	return error;
}

//***************************************************************************
//
DWORD Utf8ToAnsi_String(std::string& ansi, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	if( utf8 == nullptr || utf8_size == 0 )
		return ERROR_INVALID_PARAMETER;

	std::wstring unicode;

	if( (error = Utf8ToUnicode_String(unicode, utf8, utf8_size)) != 0 ) return error;
	if( (error = UnicodeToAnsi_String(ansi, unicode.c_str(), unicode.size())) != 0 ) return error;

	return error;
}

//***************************************************************************
//
wstring StringToWString(const std::string& ansi)
{
	std::wstring unicode;
	if( AnsiToUnicode_String(unicode, ansi.c_str(), ansi.size()) != 0 ) return L"";
	return unicode;
}

//***************************************************************************
// wstring -> string
string WStringToString(const std::wstring& unicode)
{
	std::string ansi;
	if( UnicodeToAnsi_String(ansi, unicode.c_str(), unicode.size()) != 0 ) return "";
	return ansi;
}

//***************************************************************************
//
string UnicodeToUtf8(const std::wstring unicode)
{
	std::string utf8;
	if( UnicodeToUtf8_String(utf8, unicode.c_str(), unicode.size()) != 0 ) return "";
	return utf8;
}

//***************************************************************************
//
wstring Utf8ToUnicode(const std::string utf8)
{
	std::wstring unicode;
	if( Utf8ToUnicode_String(unicode, utf8.c_str(), utf8.size()) != 0 ) return L"";
	return unicode;
}

//***************************************************************************
//
string AnsiToUtf8(const std::string ansi)
{
	std::string utf8;
	if( AnsiToUtf8_String(utf8, ansi.c_str(), ansi.size()) != 0 ) return "";
	return utf8;
}

//***************************************************************************
//
string Utf8ToAnsi(const std::string utf8)
{
	std::string ansi;
	if( Utf8ToAnsi_String(ansi, utf8.c_str(), utf8.size()) != 0 ) return "";
	return ansi;
}

//***************************************************************************
//
_tstring StringToTString(const std::string& src)
{
#ifdef _UNICODE
	return StringToWString(src);
#else
	return src;
#endif
}

//***************************************************************************
//
std::string TStringToString(const _tstring& src)
{
#ifdef _UNICODE
	return WStringToString(src);
#else
	return src;
#endif
}
#endif

//***************************************************************************
//
_tstring WStringToTString(const std::wstring& src)
{
#ifdef _UNICODE
	return src;
#else
	return WStringToString(src);
#endif
}

//***************************************************************************
//
wstring TStringToWString(const _tstring& src)
{
#ifdef _UNICODE
	return src;
#else
	return StringToWString(src);
#endif
}
