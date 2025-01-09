
//***************************************************************************
// IconvUtil.h : interface for the IconvUtil Functions.
//
//***************************************************************************

#ifndef __ICONVUTIL_H__
#define __ICONVUTIL_H__

namespace Iconv
{
	class CIconvUtil
	{
		public:
			CIconvUtil(const std::string& fromEncoding, const std::string& toEncoding);
			~CIconvUtil();

			std::string Convert(const std::string& input) const;
			std::string Convert(const std::wstring& input) const;
			std::wstring ConvertW(const std::string& input) const;

			//***************************************************************************
			// 정적 헬퍼 함수: 다양한 인코딩 간 변환 수행
			//	- UTF-8 -> ANSI(예: CP949, Windows-1252)
			//	- ANSI(예: CP949, Windows-1252) -> UTF-8
			static std::string ConvertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding) 
			{
				CIconvUtil converter(fromEncoding, toEncoding);
				return converter.Convert(input);
			}

			//***************************************************************************
			// 정적 헬퍼 함수: 다양한 인코딩 간 변환 수행
			//	- WCHAR_T -> ANSI(예: CP949, Windows-1252)
			//	- WCHAR_T -> UTF-8
			static std::string ConvertEncoding(const std::wstring& input, const std::string& fromEncoding, const std::string& toEncoding)
			{
				CIconvUtil converter(fromEncoding, toEncoding);
				return converter.Convert(input);
			}

			//***************************************************************************
			// 정적 헬퍼 함수: 다양한 인코딩 간 변환 수행
			//	- ANSI(예: CP949, Windows-1252) -> WCHAR_T
			//	- UTF-8 -> WCHAR_T
			static std::wstring ConvertEncodingW(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding)
			{
				CIconvUtil converter(fromEncoding, toEncoding);
				return converter.ConvertW(input);
			}

			std::string AnsiToUtf8(const std::string& input) const;			// ANSI -> UTF-8 변환
			std::string Utf8ToAnsi(const std::string& input) const;			// UTF-8 -> ANSI 변환

			std::wstring AnsiToWChar(const std::string& input) const;		// ANSI -> WCHAR_T 변환
			std::string WCharToAnsi(const std::wstring& input) const;		// WCHAR_T -> ANSI 변환

			std::wstring Utf8ToWChar(const std::string& input) const;		// UTF-8 -> WCHAR_T 변환
			std::string WCharToUtf8(const std::wstring& input) const;		// WCHAR_T -> UTF-8 변환
	private:
		iconv_t		_cd;				// iconv 변환 핸들
		std::string _fromEncoding;		// 원본 인코딩
		std::string _toEncoding;		// 대상 인코딩
	};
}

#endif // ndef __ICONVUTIL_H__
