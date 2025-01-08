
//***************************************************************************
// IconvUtil.cpp: implementation of the IconvUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "IconvUtil.h"

namespace Iconv
{
	//***************************************************************************
	// Construction/Destruction 
	//***************************************************************************

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
	// 소멸자: iconv 핸들 닫기
	CIconvUtil::~CIconvUtil()
	{
		if( _cd != (iconv_t)-1 ) 
		{
			iconv_close(_cd);
		}
	}

	//***************************************************************************
	// 문자열 변환 함수
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
	// UTF-8 -> WCHAR_T 변환
	std::wstring CIconvUtil::Utf8ToWChar(const std::string& input) const
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
	// WCHAR_T -> UTF-8 변환
	std::string CIconvUtil::WCharToUtf8(const std::wstring& input) const
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
}