
//***************************************************************************
// IconvUtil.cpp: implementation of the CIconvUtil class.
//
//***************************************************************************

#include "pch.h"
#include "IconvUtil.h"

namespace Iconv
{
	//***************************************************************************
	// Construction/Destruction 
	//***************************************************************************

	//***************************************************************************
	// @brief 지정된 인코딩(from -> to)으로 iconv 변환 핸들을 초기화합니다.
	// @param fromEncoding 원본 문자셋 인코딩 이름
	// @param toEncoding 대상 문자셋 인코딩 이름
	CIconvUtil::CIconvUtil(const std::string& fromEncoding, const std::string& toEncoding)
		: _fromEncoding(fromEncoding), _toEncoding(toEncoding) 
	{
		_cd = iconv_open(_toEncoding.c_str(), _fromEncoding.c_str());
		if( _cd == (iconv_t)-1 ) 
		{
			throw std::runtime_error("iconv_open failed: Unsupported encoding");
		}
	}

	//***************************************************************************
	// @brief 소멸자: 생성 시 할당된 iconv 변환 핸들을 닫아 리소스를 해제합니다.
	CIconvUtil::~CIconvUtil()
	{
		if( _cd != (iconv_t)-1 ) 
		{
			iconv_close(_cd);
		}
	}

	//***************************************************************************
	// @brief std::string 문자열을 다른 인코딩으로 변환합니다.
	// @param input 변환할 원본 문자열
	// @return 변환된 결과 문자열
	std::string CIconvUtil::Convert(const std::string& input) const
	{
		size_t inBytesLeft = input.size();
		size_t outBytesLeft = inBytesLeft * 4;				// 충분히 큰 출력 버퍼 크기
		std::vector<char> output(outBytesLeft, '\0');

		char* inBuf = const_cast<char*>(input.c_str());
		char* outBuf = output.data();
		char* outBufStart = outBuf;

		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 ) 
		{
			throw std::runtime_error("iconv conversion failed");
		}

		return std::string(output.data(), outBuf - outBufStart);
	}

	//***************************************************************************
	// @brief std::wstring 문자열을 std::string으로 변환합니다.
	// @param input 변환할 원본 와이드 문자열
	// @return 변환된 결과 문자열
	std::string CIconvUtil::Convert(const std::wstring& input) const
	{
		size_t inBytesLeft = input.size() * sizeof(wchar_t);
		size_t outBytesLeft = inBytesLeft * 4;						// 충분히 큰 출력 버퍼 크기
		std::vector<char> output(outBytesLeft, '\0');

		char* inBuf = reinterpret_cast<char*>(const_cast<wchar_t*>(input.c_str()));
		char* outBuf = output.data();
		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 )
		{
			throw std::runtime_error("iconv conversion failed");
		}

		return std::string(output.data());
	}

	//***************************************************************************
	// @brief std::string 문자열을 std::wstring으로 변환합니다.
	// @param input 변환할 원본 문자열
	// @return 변환된 결과 와이드 문자열
	std::wstring CIconvUtil::ConvertW(const std::string& input) const
	{
		size_t inBytesLeft = input.size();
		size_t outBytesLeft = (inBytesLeft + 1) * sizeof(wchar_t);		// 충분한 버퍼 크기
		std::vector<wchar_t> output(outBytesLeft / sizeof(wchar_t), L'\0');

		char* inBuf = const_cast<char*>(input.c_str());
		char* outBuf = reinterpret_cast<char*>(output.data());
		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 )
		{
			throw std::runtime_error("iconv conversion failed");
		}

		return std::wstring(output.data());
	}

	//***************************************************************************
	// @brief ANSI(예: CP949, Windows-1252) 인코딩 문자열을 UTF-8 인코딩으로 변환합니다.
	// @param input 변환할 ANSI 문자열
	// @return 변환된 UTF-8 문자열
	std::string CIconvUtil::AnsiToUtf8(const std::string& input) const
	{
		if( input.size() < 1 ) return "";

		return Convert(input);
	}

	//***************************************************************************
	// @brief UTF-8 인코딩 문자열을 ANSI(예: CP949, Windows-1252) 인코딩으로 변환합니다.
	// @param input 변환할 UTF-8 문자열
	// @return 변환된 ANSI 문자열
	std::string CIconvUtil::Utf8ToAnsi(const std::string& input) const
	{
		if( input.size() < 1 ) return "";

		return Convert(input);
	}

	//***************************************************************************
	// @brief ANSI(예: CP949, Windows-1252) 인코딩 문자열을 WCHAR_T(std::wstring)로 변환합니다.
	// @param input 변환할 ANSI 문자열
	// @return 변환된 WCHAR_T 문자열
	std::wstring CIconvUtil::AnsiToWChar(const std::string& input) const
	{
		if( input.size() < 1 ) return L"";

		return ConvertW(input);
	}

	//***************************************************************************
	// @brief WCHAR_T(std::wstring) 문자열을 ANSI(예: CP949, Windows-1252) 인코딩으로 변환합니다.
	// @param input 변환할 WCHAR_T 문자열
	// @return 변환된 ANSI 문자열
	std::string CIconvUtil::WCharToAnsi(const std::wstring& input) const
	{
		if( input.size() < 1 ) return "";

		return Convert(input);
	}

	//***************************************************************************
	// @brief UTF-8 인코딩 문자열을 WCHAR_T(std::wstring)로 변환합니다.
	// @param input 변환할 UTF-8 문자열
	// @return 변환된 WCHAR_T 문자열
	std::wstring CIconvUtil::Utf8ToWChar(const std::string& input) const
	{
		if( input.size() < 1 ) return L"";

		return ConvertW(input);
	}

	//***************************************************************************
	// @brief WCHAR_T(std::wstring) 문자열을 UTF-8 인코딩으로 변환합니다.
	// @param input 변환할 WCHAR_T 문자열
	// @return 변환된 UTF-8 문자열
	std::string CIconvUtil::WCharToUtf8(const std::wstring& input) const
	{
		if( input.size() < 1 ) return "";

		return Convert(input);
	}
}