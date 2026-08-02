
//***************************************************************************
// IconvUtil.h : interface for the CIconvUtil class.
//
//***************************************************************************

#ifndef __ICONVUTIL_H__
#define __ICONVUTIL_H__

#include <iconv.h>
#include <memory>

#pragma comment(lib, LIB_NAME("libiconv"))

//***************************************************************************
// [주요 변환 기능별 호출 방법 및 예시 (Windows 환경 기준 CP949 / UTF-16LE 사용)]
//
// 1. AnsiToUtf8 (ANSI -> UTF-8)
//    - CIconvUtil conv("CP949", "UTF-8");
//    - std::string result = conv.Convert(ansiStr);
//    - 또는 정적 헬퍼: std::string result = CIconvUtil::ConvertEncoding(ansiStr, "CP949", "UTF-8");
//
// 2. Utf8ToAnsi (UTF-8 -> ANSI)
//    - CIconvUtil conv("UTF-8", "CP949");
//    - std::string result = conv.Convert(utf8Str);
//    - 또는 정적 헬퍼: std::string result = CIconvUtil::ConvertEncoding(utf8Str, "UTF-8", "CP949");
//
// 3. AnsiToWChar (ANSI -> wchar_t)
//    - CIconvUtil conv("CP949", "UTF-16LE"); // 리눅스는 "UTF-32LE" 등 사용
//    - std::wstring result = conv.ConvertW(ansiStr);
//    - 또는 정적 헬퍼: std::wstring result = CIconvUtil::ConvertEncodingW(ansiStr, "CP949", "UTF-16LE");
//
// 4. WCharToAnsi (wchar_t -> ANSI)
//    - CIconvUtil conv("UTF-16LE", "CP949");
//    - std::string result = conv.Convert(wcharStr);
//    - 또는 정적 헬퍼: std::string result = CIconvUtil::ConvertEncoding(wcharStr, "UTF-16LE", "CP949");
//
// 5. Utf8ToWChar (UTF-8 -> wchar_t)
//    - CIconvUtil conv("UTF-8", "UTF-16LE");
//    - std::wstring result = conv.ConvertW(utf8Str);
//    - 또는 정적 헬퍼: std::wstring result = CIconvUtil::ConvertEncodingW(utf8Str, "UTF-8", "UTF-16LE");
//
// 6. WCharToUtf8 (wchar_t -> UTF-8)
//    - CIconvUtil conv("UTF-16LE", "UTF-8");
//    - std::string result = conv.Convert(wcharStr);
//    - 또는 정적 헬퍼: std::string result = CIconvUtil::ConvertEncoding(wcharStr, "UTF-16LE", "UTF-8");
namespace Iconv
{
	//***************************************************************************
	// @brief CIconvUtil은 "고정 방향 변환기"입니다.
	//
	// 생성 시 지정한 fromEncoding -> toEncoding 방향은 인스턴스 수명 동안 절대
	// 바뀌지 않습니다. 즉 CIconvUtil("UTF-8", "CP949")로 만든 인스턴스는
	// 오직 UTF-8 -> CP949 변환에만 사용해야 하며, 다른 방향으로 사용하면
	// (예: 그 인스턴스로 ANSI -> WCHAR_T를 시도) 조용히 잘못된 결과를 냅니다.
	//
	// 사용 패턴:
	//   1) 같은 방향 변환을 반복 호출한다면(예: 스레드마다 UTF-8->CP949를
	//      계속 수행) 해당 방향 전용 CIconvUtil 인스턴스를 만들어
	//      (예: thread_local) 재사용하십시오.
	//   2) 가끔 1회성으로 특정 방향 변환이 필요하다면 정적 헬퍼
	//      ConvertEncoding() / ConvertEncodingW()를 사용하십시오. 매 호출마다
	//      임시 인스턴스를 생성/폐기합니다.
	class CIconvUtil
	{
	public:
		// fromEncoding -> toEncoding 방향을 생성 시 고정합니다. 이후 변경 불가.
		CIconvUtil(const std::string& fromEncoding, const std::string& toEncoding);
		~CIconvUtil();

		// 생성자에서 고정한 방향으로 변환합니다.
		std::string Convert(const std::string& input) const;
		std::string Convert(const std::wstring& input) const;
		std::wstring ConvertW(const std::string& input) const;

		//***************************************************************************
		// 정적 헬퍼 함수: 1회성 변환용. 매 호출마다 임시 CIconvUtil을 생성합니다.
		// 반복 호출이 필요하면 방향 전용 인스턴스를 직접 만들어 재사용하십시오.
		//	- UTF-8 -> ANSI(예: CP949, Windows-1252)
		//	- ANSI(예: CP949, Windows-1252) -> UTF-8
		static std::string ConvertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding)
		{
			CIconvUtil converter(fromEncoding, toEncoding);
			return converter.Convert(input);
		}

		//***************************************************************************
		// 정적 헬퍼 함수: 1회성 변환용.
		//	- WCHAR_T -> ANSI(예: CP949, Windows-1252)
		//	- WCHAR_T -> UTF-8
		static std::string ConvertEncoding(const std::wstring& input, const std::string& fromEncoding, const std::string& toEncoding)
		{
			CIconvUtil converter(fromEncoding, toEncoding);
			return converter.Convert(input);
		}

		//***************************************************************************
		// 정적 헬퍼 함수: 1회성 변환용.
		//	- ANSI(예: CP949, Windows-1252) -> WCHAR_T
		//	- UTF-8 -> WCHAR_T
		static std::wstring ConvertEncodingW(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding)
		{
			CIconvUtil converter(fromEncoding, toEncoding);
			return converter.ConvertW(input);
		}

	private:
		// 변환 결과를 담는 재사용 버퍼. 인스턴스(스레드) 수명 동안 유지되며
		// 필요할 때만 커집니다(축소되지 않음). 제로 초기화하지 않으므로
		// 반드시 iconv()가 실제로 쓴 바이트 수만큼만 읽어야 합니다.
		char* EnsureScratch(size_t requiredBytes) const;

		iconv_t		_cd;				// iconv 변환 핸들 (fromEncoding -> toEncoding 고정)
		std::string _fromEncoding;		// 원본 인코딩
		std::string _toEncoding;		// 대상 인코딩

		mutable std::unique_ptr<char[]> _scratch;			// 재사용 스크래치 버퍼
		mutable size_t                  _scratchCapacity;	// 현재 버퍼 크기(바이트)
	};
}

#endif // ndef __ICONVUTIL_H__