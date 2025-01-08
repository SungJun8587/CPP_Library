
//***************************************************************************
// IconvUtil.h : interface for the IconvUtil Functions.
//
//***************************************************************************

#ifndef __ICONVUTIL_H__
#define __ICONVUTIL_H__

#include <../../Library/ExternalLib/libiconv-for-Windows-master/include/iconv.h>

#pragma comment(lib, LIB_NAME("libiconv"))

namespace Iconv
{
	class CIconvUtil
	{
		public:
			CIconvUtil(const std::string& fromEncoding, const std::string& toEncoding);
			~CIconvUtil();

			std::string Convert(const std::string& input) const;

			std::wstring Utf8ToWChar(const std::string& input) const;		// UTF-8 -> WCHAR_T ��ȯ
			std::string WCharToUtf8(const std::wstring& input) const;		// WCHAR_T -> UTF-8 ��ȯ

			//***************************************************************************
			// ���� ���� �Լ�: �پ��� ���ڵ� �� ��ȯ ����
			//	- UTF-8 �� UTF-16BE
			//	- UTF-8 �� UTF-16LE
			//	- UTF-8 �� ANSI(��: CP1252, CP949)
			static std::string ConvertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding) 
			{
				CIconvUtil converter(fromEncoding, toEncoding);
				return converter.Convert(input);
			}

	private:
		iconv_t		_cd;				// iconv ��ȯ �ڵ�
		std::string _fromEncoding;		// ���� ���ڵ�
		std::string _toEncoding;		// ��� ���ڵ�
	};
}

#endif // ndef __ICONVUTIL_H__
