
//***************************************************************************
// ConvertCharset.cpp: implementation of the ConvertCharset Functions.
//
//***************************************************************************

#include "pch.h"
#include "ConvertCharset.h"

//***************************************************************************
// @brief TCHAR 문자열의 변환될 멀티바이트 길이(바이트 수)를 계산합니다.
// @param nCodePage 변환에 사용할 코드 페이지
// @param ptszSource 대상 TCHAR 문자열
// @return 성공 시 변환된 문자열 길이, 실패 시 -1
//***************************************************************************
int GetMultiByteLen(int nCodePage, const TCHAR* ptszSource)
{
	if( ptszSource == nullptr ) return -1;

	int length = static_cast<int>(_tcslen(ptszSource));
	if( length == 0 ) return -1;

#ifdef _UNICODE
	length = WideCharToMultiByte(nCodePage, 0, ptszSource, length + 1, nullptr, 0, nullptr, NULL);
	if( length == 0 ) return -1;
	length--;
#endif

	return length;
}

//***************************************************************************
// @brief ANSI 문자열을 Unicode(WideChar) 문자열로 변환합니다.
// @param unicode [out] 변환된 Unicode 문자열을 저장할 버퍼
// @param unicode_size [in] Unicode 버퍼의 크기 (문자 단위)
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
DWORD AnsiToUnicode(wchar_t* unicode, size_t unicode_size, const char* ansi, const size_t ansi_size)
{
	DWORD error = 0;

	do
	{
		if( ansi == nullptr || ansi_size == 0 || unicode == nullptr )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

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
			error = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

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
// @brief Unicode(WideChar) 문자열을 ANSI 문자열로 변환합니다.
// @param ansi [out] 변환된 ANSI 문자열을 저장할 버퍼
// @param ansi_size [in] ANSI 버퍼의 크기 (바이트 단위)
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
DWORD UnicodeToAnsi(char* ansi, size_t ansi_size, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 || ansi == nullptr )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

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
			error = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

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
// @brief Unicode(WideChar) 문자열을 UTF-8 문자열로 변환합니다.
// @param utf8 [out] 변환된 UTF-8 문자열을 저장할 버퍼
// @param utf8_size [in] UTF-8 버퍼의 크기 (바이트 단위)
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
DWORD UnicodeToUtf8(char* utf8, size_t utf8_size, const wchar_t* unicode, const size_t unicode_size)
{
	DWORD error = 0;

	do
	{
		if( unicode == nullptr || unicode_size == 0 || utf8 == nullptr )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

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
			error = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

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
// @brief UTF-8 문자열을 Unicode(WideChar) 문자열로 변환합니다.
// @param unicode [out] 변환된 Unicode 문자열을 저장할 버퍼
// @param unicode_size [in] Unicode 버퍼의 크기 (문자 단위)
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
DWORD Utf8ToUnicode(wchar_t* unicode, size_t unicode_size, const char* utf8, const size_t utf8_size)
{
	DWORD error = 0;

	do
	{
		if( utf8 == nullptr || utf8_size == 0 || unicode == nullptr )
		{
			error = ERROR_INVALID_PARAMETER;
			break;
		}

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
			error = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

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
// @brief ANSI 문자열을 UTF-8 문자열로 변환합니다 (Unicode 경유).
// @param utf8 [out] 변환된 UTF-8 문자열을 저장할 버퍼
// @param utf8_size [in] UTF-8 버퍼의 크기 (바이트 단위)
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief UTF-8 문자열을 ANSI 문자열로 변환합니다 (Unicode 경유).
// @param ansi [out] 변환된 ANSI 문자열을 저장할 버퍼
// @param ansi_size [in] ANSI 버퍼의 크기 (바이트 단위)
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 ANSI 문자열을 Unicode(WideChar) 문자열로 변환합니다.
// @param unicode [out] 변환된 Unicode 문자열을 저장할 CMemBuffer 버퍼 객체
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 Unicode(WideChar) 문자열을 ANSI 문자열로 변환합니다.
// @param ansi [out] 변환된 ANSI 문자열을 저장할 CMemBuffer 버퍼 객체
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 Unicode(WideChar) 문자열을 UTF-8 문자열로 변환합니다.
// @param utf8 [out] 변환된 UTF-8 문자열을 저장할 CMemBuffer 버퍼 객체
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 UTF-8 문자열을 Unicode(WideChar) 문자열로 변환합니다.
// @param unicode [out] 변환된 Unicode 문자열을 저장할 CMemBuffer 버퍼 객체
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 ANSI 문자열을 UTF-8 문자열로 변환합니다.
// @param utf8 [out] 변환된 UTF-8 문자열을 저장할 CMemBuffer 버퍼 객체
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief CMemBuffer를 사용하여 UTF-8 문자열을 ANSI 문자열로 변환합니다.
// @param ansi [out] 변환된 ANSI 문자열을 저장할 CMemBuffer 버퍼 객체
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief 바이트 버퍼(BYTE*)를 TCHAR 문자열 버퍼로 안전하게 변환합니다.
// @param TDestination [out] 변환된 TCHAR 문자열을 저장할 CMemBuffer 객체
// @param pbBuffer [in] 원본 바이트 버퍼
// @param buffer_size [in] 바이트 버퍼의 크기 (바이트 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool ByteToTChar(CMemBuffer<TCHAR>& TDestination, const BYTE* pbBuffer, size_t buffer_size)
{
	if( pbBuffer == nullptr || buffer_size == 0 ) return false;

	const char* pszSource = (const char*)pbBuffer;

#ifdef _UNICODE
	int nLength = MultiByteToWideChar(CP_ACP, 0, pszSource, static_cast<int>(buffer_size), nullptr, 0);
	if( nLength == 0 ) return false;

	TDestination.Init(nLength + 1);

	if( MultiByteToWideChar(CP_ACP, 0, pszSource, static_cast<int>(buffer_size), TDestination.GetBuffer(), nLength) == 0 ) return false;
	TDestination.GetBuffer()[nLength] = L'\0';
#else
	TDestination.Init(buffer_size);
	_tcsncpy_s(TDestination.GetBuffer(), TDestination.GetBufSize(), pszSource, buffer_size);
#endif

	return true;
}

//***************************************************************************
// @brief TCHAR 문자열 버퍼를 바이트 버퍼(BYTE*)로 변환합니다.
// @param Destination [out] 변환된 바이트 데이터를 저장할 CMemBuffer 객체
// @param ptszBuffer [in] 원본 TCHAR 문자열
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool TCharToByte(CMemBuffer<BYTE>& Destination, const TCHAR* ptszBuffer)
{
	if( ptszBuffer == nullptr ) return false;

	int nSrcLen = static_cast<int>(_tcslen(ptszBuffer)) + 1;

#ifdef _UNICODE
	int nLength = WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, nullptr, 0, nullptr, NULL);
	if( nLength == 0 ) return false;

	Destination.Init(nLength);
	char* pszDestination = (char*)Destination.GetBuffer();

	if( WideCharToMultiByte(CP_ACP, 0, ptszBuffer, nSrcLen, pszDestination, nLength, nullptr, NULL) == 0 ) return false;
#else
	int nLength = nSrcLen;
	Destination.Init(nLength);
	char* pszDestination = (char*)Destination.GetBuffer();

	_tcsncpy_s(pszDestination, nLength, ptszBuffer, _TRUNCATE);
#endif

	return true;
}
#endif

#ifdef _STRING_
//***************************************************************************
// @brief ANSI 문자열을 std::wstring으로 변환합니다.
// @param unicode [out] 변환된 결과를 저장할 std::wstring 참조
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief Unicode 문자열을 std::string(ANSI)으로 변환합니다.
// @param ansi [out] 변환된 결과를 저장할 std::string 참조
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief Unicode 문자열을 std::string(UTF-8)으로 변환합니다.
// @param utf8 [out] 변환된 결과를 저장할 std::string 참조
// @param unicode [in] 변환할 Unicode 문자열
// @param unicode_size [in] Unicode 문자열의 크기 (문자 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief UTF-8 문자열을 std::wstring으로 변환합니다.
// @param unicode [out] 변환된 결과를 저장할 std::wstring 참조
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief ANSI 문자열을 std::string(UTF-8)으로 변환합니다.
// @param utf8 [out] 변환된 결과를 저장할 std::string 참조
// @param ansi [in] 변환할 ANSI 문자열
// @param ansi_size [in] ANSI 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
// @brief UTF-8 문자열을 std::string(ANSI)으로 변환합니다.
// @param ansi [out] 변환된 결과를 저장할 std::string 참조
// @param utf8 [in] 변환할 UTF-8 문자열
// @param utf8_size [in] UTF-8 문자열의 크기 (바이트 단위, 널 문자 포함)
// @return 성공 시 0 (ERROR_SUCCESS), 실패 시 Win32 오류 코드
//***************************************************************************
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
#endif
