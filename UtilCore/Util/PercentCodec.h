
//***************************************************************************
// PercentCodec.h : 바이트 단위 퍼센트 인코딩/디코딩 코어 (header-only).
//
// @details
// WebUtil(UrlEncode/UrlDecode/EncodeURIComponent 등)과 HTTP 파서
// (Network/HTTP/HttpParseUtil.h, FormUrlEncodedParser.h)가 같은 규칙을 쓰도록
// 한 곳에 모은 공용 코어입니다.
//  - <string>/<string_view>/<array>만 사용합니다. 플랫폼(Windows/TCHAR/코드페이지)
//    의존이 없어 서버 쪽 HTTP 파서에서 변환 없이 바로 쓸 수 있습니다.
//  - 입출력은 "바이트열"입니다. 문자 집합(UTF-8/ANSI) 변환은 호출부 책임입니다
//    (WebUtil은 코드페이지 변환 후 이 코어를 호출, HTTP는 UTF-8 가정으로 그대로 사용).
//  - 디코딩은 관용적입니다: "%" 뒤에 유효한 16진수 두 글자가 없으면 그 "%"는
//    원문 그대로 둡니다(엄격한 RFC 위반 검출이 목적이 아님).
//***************************************************************************

#ifndef UC_PERCENTCODEC_H
#define UC_PERCENTCODEC_H

#include <Util/Regular.h>

#include <array>
#include <string>
#include <string_view>

namespace PercentCodec
{
	using SafeTable = std::array<bool, 256>;

	// 인코딩 결과에 쓰는 16진수 문자(대문자 — RFC 3986 권장 정규형)
	inline constexpr char kHexDigitsUpper[17] = "0123456789ABCDEF";

	using AsciiChar::IsAlnum;

	//***************************************************************************
	// @brief 16진수 문자 1개를 nibble 값(0~15)으로 변환. 유효하지 않으면 -1.
	// @param c int로 받는다 — wchar_t(TCHAR)를 char로 좁히면 상위 바이트가 잘려
	//          유효하지 않은 문자가 유효한 hex로 오인될 수 있어서다.
	//***************************************************************************
	constexpr int HexNibble(int c) noexcept
	{
		if( c >= '0' && c <= '9' ) return c - '0';
		if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		return -1;
	}

	//***************************************************************************
	// @brief 안전 문자(인코딩하지 않을 문자) 테이블 생성. 영숫자는 기본 포함이며
	//        extraSafe에 나열한 문자를 추가로 안전 문자 취급한다.
	//***************************************************************************
	inline SafeTable BuildSafeTable(const char* extraSafe)
	{
		SafeTable table{};
		for( int i = 0; i < 256; i++ )
			table[i] = IsAlnum(static_cast<unsigned char>(i));

		if( extraSafe != nullptr )
		{
			for( const char* p = extraSafe; *p; p++ )
				table[static_cast<unsigned char>(*p)] = true;
		}
		return table;
	}

	//***************************************************************************
	// @brief RFC 3986 unreserved 문자(영숫자 + "-._~")만 안전 문자인 테이블.
	//        URL 쿼리 값/경로 세그먼트를 인코딩할 때의 기본값.
	//***************************************************************************
	inline const SafeTable& UnreservedTable()
	{
		static const SafeTable table = BuildSafeTable("-._~");
		return table;
	}

	//***************************************************************************
	// @brief 바이트열을 퍼센트 인코딩해 dest 끝에 덧붙인다(임시 문자열 없음).
	//        안전 문자는 그대로, 공백은 옵션에 따라 '+' 또는 "%20", 나머지는
	//        "%XX"(대문자 hex). 1-pass 처리.
	//***************************************************************************
	inline void AppendEncoded(std::string& dest, std::string_view source, const SafeTable& safeTable, bool spaceAsPlus = false)
	{
		for( char ch : source )
		{
			const unsigned char c = static_cast<unsigned char>(ch);
			if( safeTable[c] )
				dest.push_back(ch);
			else if( spaceAsPlus && c == ' ' )
				dest.push_back('+');
			else
			{
				dest.push_back('%');
				dest.push_back(kHexDigitsUpper[(c >> 4) & 0x0F]);
				dest.push_back(kHexDigitsUpper[c & 0x0F]);
			}
		}
	}

	//***************************************************************************
	// @brief AppendEncoded()의 새 문자열 반환 버전.
	//***************************************************************************
	inline std::string Encode(std::string_view source, const SafeTable& safeTable, bool spaceAsPlus = false)
	{
		std::string dest;
		dest.reserve(source.size());
		AppendEncoded(dest, source, safeTable, spaceAsPlus);
		return dest;
	}

	//***************************************************************************
	// @brief 퍼센트 인코딩을 원래 바이트열로 디코딩한다.
	// @param plusAsSpace true면 '+'를 공백으로 변환(application/x-www-form-urlencoded).
	//        "%2B"는 '+'로 디코딩되고 리터럴 '+'만 공백이 되므로 한 번의 pass로 충분하다
	//        (퍼센트 디코딩 전에 '+'를 따로 치환해 둘 필요가 없다).
	// @note "%"가 문자열 끝 근처이거나 뒤따르는 두 글자가 hex가 아니면 '%'는 리터럴.
	//***************************************************************************
	inline std::string Decode(std::string_view encoded, bool plusAsSpace = false)
	{
		std::string result;
		result.reserve(encoded.size());

		const size_t n = encoded.size();
		for( size_t i = 0; i < n; ++i )
		{
			const char c = encoded[i];

			if( c == '%' && i + 2 < n )
			{
				const int hi = HexNibble(static_cast<unsigned char>(encoded[i + 1]));
				const int lo = HexNibble(static_cast<unsigned char>(encoded[i + 2]));
				if( hi >= 0 && lo >= 0 )
				{
					result.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			else if( plusAsSpace && c == '+' )
			{
				result.push_back(' ');
				continue;
			}

			result.push_back(c);
		}
		return result;
	}
}

#endif // ndef UC_PERCENTCODEC_H