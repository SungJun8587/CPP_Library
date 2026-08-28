
//***************************************************************************
// Base64UrlUtil.h : Base64URL(RFC 4648 §5) encode/decode helpers.
//
//***************************************************************************

#ifndef UC_BASE64URLUTIL_H
#define UC_BASE64URLUTIL_H

#include <string>
#include <string_view>
#include <cstdint>

namespace base64url
{
	//***************************************************************************
	// @brief 바이트 데이터를 Base64URL(RFC 4648 §5)로 인코딩합니다.
	// @param data 인코딩할 바이트
	// @return std::string 인코딩 결과. 표준 Base64와 달리 '+'/'/' 대신 '-'/'_'를
	//         쓰고, 패딩('=')을 붙이지 않는다 — JWT(RFC 7519)가 요구하는 형식.
	//***************************************************************************
	inline std::string Encode(std::string_view data)
	{
		static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

		std::string result;
		result.reserve((data.size() + 2) / 3 * 4);

		size_t i = 0;
		while( i + 3 <= data.size() )
		{
			uint32_t v = (static_cast<uint8_t>(data[i]) << 16) | (static_cast<uint8_t>(data[i + 1]) << 8) | static_cast<uint8_t>(data[i + 2]);
			result.push_back(table[(v >> 18) & 0x3F]);
			result.push_back(table[(v >> 12) & 0x3F]);
			result.push_back(table[(v >> 6) & 0x3F]);
			result.push_back(table[v & 0x3F]);
			i += 3;
		}

		size_t remaining = data.size() - i;
		if( remaining == 1 )
		{
			uint32_t v = static_cast<uint8_t>(data[i]) << 16;
			result.push_back(table[(v >> 18) & 0x3F]);
			result.push_back(table[(v >> 12) & 0x3F]);
			// 패딩 없음 (JWT 규격)
		}
		else if( remaining == 2 )
		{
			uint32_t v = (static_cast<uint8_t>(data[i]) << 16) | (static_cast<uint8_t>(data[i + 1]) << 8);
			result.push_back(table[(v >> 18) & 0x3F]);
			result.push_back(table[(v >> 12) & 0x3F]);
			result.push_back(table[(v >> 6) & 0x3F]);
		}

		return result;
	}

	//***************************************************************************
	// @brief Base64URL 문자 하나를 6비트 값으로 되돌립니다 (내부용).
	// @return int 0~63 값, 유효하지 않은 문자면 -1
	//***************************************************************************
	inline int DecodeChar(char c) noexcept
	{
		if( c >= 'A' && c <= 'Z' ) return c - 'A';
		if( c >= 'a' && c <= 'z' ) return c - 'a' + 26;
		if( c >= '0' && c <= '9' ) return c - '0' + 52;
		if( c == '-' ) return 62;
		if( c == '_' ) return 63;
		return -1;
	}

	//***************************************************************************
	// @brief Base64URL 문자열을 바이트로 디코딩합니다.
	// @param encoded 디코딩할 Base64URL 문자열 (패딩 있어도/없어도 됨)
	// @param out [OUT] 디코딩된 바이트
	// @return bool 성공 여부 (유효하지 않은 문자가 섞여 있으면 false)
	//***************************************************************************
	inline bool Decode(std::string_view encoded, std::string& out)
	{
		out.clear();
		out.reserve(encoded.size() * 3 / 4 + 3);

		int buffer = 0;
		int bitsCollected = 0;

		for( char c : encoded )
		{
			if( c == '=' ) break; // 패딩이 섞여 있어도 관대하게 무시
			int v = DecodeChar(c);
			if( v < 0 ) return false;

			buffer = (buffer << 6) | v;
			bitsCollected += 6;

			if( bitsCollected >= 8 )
			{
				bitsCollected -= 8;
				out.push_back(static_cast<char>((buffer >> bitsCollected) & 0xFF));
			}
		}

		return true;
	}
}

#endif // ndef UC_BASE64URLUTIL_H