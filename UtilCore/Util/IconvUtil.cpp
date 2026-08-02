
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
	// 이 인스턴스는 이후 오직 fromEncoding -> toEncoding 방향으로만 사용됩니다.
	// @param fromEncoding 원본 문자셋 인코딩 이름
	// @param toEncoding 대상 문자셋 인코딩 이름
	CIconvUtil::CIconvUtil(const std::string& fromEncoding, const std::string& toEncoding)
		: _fromEncoding(fromEncoding), _toEncoding(toEncoding), _scratchCapacity(0)
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
	// @brief 스크래치 버퍼가 최소 requiredBytes만큼 확보되어 있게 합니다.
	//        기존 용량이 충분하면 재할당하지 않고 그대로 재사용합니다.
	//        new char[n]은 제로 초기화를 하지 않으므로, 호출자는 iconv()가
	//        실제로 쓴 바이트 수만큼만 읽어야 합니다.
	// @param requiredBytes 이번 변환에 필요한 최소 바이트 수
	// @return 스크래치 버퍼의 시작 포인터
	char* CIconvUtil::EnsureScratch(size_t requiredBytes) const
	{
		if( requiredBytes > _scratchCapacity )
		{
			_scratch.reset(new char[requiredBytes]);
			_scratchCapacity = requiredBytes;
		}
		return _scratch.get();
	}

	//***************************************************************************
	// @brief std::string 문자열을 생성 시 고정된 방향(fromEncoding -> toEncoding)으로 변환합니다.
	// @param input 변환할 원본 문자열
	// @return 변환된 결과 문자열
	std::string CIconvUtil::Convert(const std::string& input) const
	{
		iconv(_cd, nullptr, nullptr, nullptr, nullptr);	// shift state 초기화

		size_t inBytesLeft = input.size();
		size_t outBytesLeft = inBytesLeft * 4;				// 충분히 큰 출력 버퍼 크기
		char* outBufStart = EnsureScratch(outBytesLeft);

		char* inBuf = const_cast<char*>(input.c_str());
		char* outBuf = outBufStart;

		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 )
		{
			throw std::runtime_error("iconv conversion failed");
		}

		return std::string(outBufStart, outBuf - outBufStart);
	}

	//***************************************************************************
	// @brief std::wstring 문자열을 생성 시 고정된 방향(fromEncoding -> toEncoding)으로 변환합니다.
	//        fromEncoding/toEncoding 중 wchar_t 쪽 인코딩 이름은 플랫폼의 sizeof(wchar_t)와
	//        일치해야 합니다(Windows: "UTF-16LE" 등, sizeof(wchar_t)==2).
	// @param input 변환할 원본 와이드 문자열
	// @return 변환된 결과 문자열
	std::string CIconvUtil::Convert(const std::wstring& input) const
	{
		iconv(_cd, nullptr, nullptr, nullptr, nullptr);

		size_t inBytesLeft = input.size() * sizeof(wchar_t);
		size_t outBytesLeft = inBytesLeft * 4;						// 충분히 큰 출력 버퍼 크기
		char* outBufStart = EnsureScratch(outBytesLeft);

		char* inBuf = reinterpret_cast<char*>(const_cast<wchar_t*>(input.c_str()));
		char* outBuf = outBufStart;

		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 )
		{
			throw std::runtime_error("iconv conversion failed");
		}

		return std::string(outBufStart, outBuf - outBufStart);
	}

	//***************************************************************************
	// @brief std::string 문자열을 생성 시 고정된 방향(fromEncoding -> toEncoding)으로 변환합니다.
	//        fromEncoding/toEncoding 중 wchar_t 쪽 인코딩 이름은 플랫폼의 sizeof(wchar_t)와
	//        일치해야 합니다(Windows: "UTF-16LE" 등, sizeof(wchar_t)==2).
	// @param input 변환할 원본 문자열
	// @return 변환된 결과 와이드 문자열
	std::wstring CIconvUtil::ConvertW(const std::string& input) const
	{
		iconv(_cd, nullptr, nullptr, nullptr, nullptr);

		size_t inBytesLeft = input.size();
		size_t outBytesLeft = (inBytesLeft + 1) * sizeof(wchar_t);		// 충분한 버퍼 크기
		char* outBufStart = EnsureScratch(outBytesLeft);

		char* inBuf = const_cast<char*>(input.c_str());
		char* outBuf = outBufStart;

		if( iconv(_cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1 )
		{
			throw std::runtime_error("iconv conversion failed");
		}

		size_t convertedWChars = (outBuf - outBufStart) / sizeof(wchar_t);
		return std::wstring(reinterpret_cast<wchar_t*>(outBufStart), convertedWChars);
	}
}