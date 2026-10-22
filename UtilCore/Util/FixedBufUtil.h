
//***************************************************************************
// FixedBufUtil.h : 고정 길이 버퍼(char[N], byte[N], std::array) 변환 유틸 (header-only).
//
// @details
// 패킷 구조체처럼 고정 폭 필드를 쓰는 코드에서 반복되는 "문자열 <-> char[N]",
// "바이트열 <-> 16진 문자열" 변환을 한 곳에 모은 것입니다.
//  - <string>/<string_view>/<array>/<cstring>만 사용합니다(프로토콜/플랫폼 비의존).
//  - 문자열은 바이트열로 취급합니다. 문자 집합 변환은 호출부 책임입니다.
//  - CopyStr는 항상 NUL 종단하며 버퍼를 넘기지 않습니다(초과분은 잘림).
//    UTF-8 멀티바이트 문자의 중간에서 잘릴 수 있으므로, 잘림이 허용되지 않는
//    입력은 반환값(잘림 여부)으로 먼저 거부하세요.
//  - 16진 출력은 소문자입니다(Crypto::CCryptoUtil::ToHex와 동일).
//    16진 입력은 대소문자 모두 허용합니다.
//***************************************************************************

#ifndef UC_FIXEDBUFUTIL_H
#define UC_FIXEDBUFUTIL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace FixedBuf
{
	//***************************************************************************
	// @brief 포인터+길이 버전. dst(dstSize 바이트)에 src를 복사하고 NUL 종단한다.
	// @param dst 복사 대상 버퍼
	// @param dstSize dst의 전체 크기(NUL 포함 바이트 수)
	// @param src 복사할 문자열
	// @return 전부 복사됐으면 true, 버퍼가 작아 잘렸으면 false.
	//         dstSize가 0이면 아무것도 쓰지 않고 src가 비어 있을 때만 true.
	//***************************************************************************
	inline bool CopyStr(char* dst, size_t dstSize, std::string_view src) noexcept
	{
		if( dstSize == 0 )
			return src.empty();

		const size_t len = (src.size() < dstSize - 1) ? src.size() : dstSize - 1;
		if( len > 0 )
			::memcpy(dst, src.data(), len);
		dst[len] = '\0';
		return len == src.size();
	}

	//***************************************************************************
	// @brief char[N]에 문자열을 복사한다(NUL 종단, 초과분 잘림).
	// @param dst 복사 대상 고정폭 배열
	// @param src 복사할 문자열
	// @return 전부 복사됐으면 true, 잘렸으면 false.
	//***************************************************************************
	template<size_t N>
	bool CopyStr(char(&dst)[N], std::string_view src) noexcept
	{
		static_assert(N > 0, "CopyStr: zero-length buffer");
		return CopyStr(dst, N, src);
	}

	//***************************************************************************
	// @brief 버퍼에서 NUL 경계를 넘지 않고 길이(NUL 제외)를 구한다.
	// @param src 대상 버퍼
	// @param maxLen 읽을 수 있는 최대 바이트 수
	// @return 첫 NUL까지의 길이. NUL이 없으면 maxLen.
	//***************************************************************************
	inline size_t StrLen(const char* src, size_t maxLen) noexcept
	{
		const void* nul = (maxLen > 0) ? ::memchr(src, '\0', maxLen) : nullptr;
		return nul ? static_cast<size_t>(static_cast<const char*>(nul) - src) : maxLen;
	}

	//***************************************************************************
	// @brief 고정폭 char 버퍼를 문자열로 읽는다. 첫 NUL 또는 maxLen 바이트까지만 취한다
	//        (NUL이 없는 오염된 입력에도 버퍼 밖을 읽지 않는다).
	// @param src 읽을 버퍼
	// @param maxLen 버퍼의 전체 크기
	// @return 읽은 문자열(NUL 제외)
	//***************************************************************************
	inline std::string ReadStr(const char* src, size_t maxLen)
	{
		return std::string(src, StrLen(src, maxLen));
	}

	//***************************************************************************
	// @brief 고정폭 char 배열을 문자열로 읽는다(ReadStr(ptr, maxLen)의 배열 편의 버전).
	// @param src 읽을 고정폭 배열
	// @return 읽은 문자열(NUL 제외)
	//***************************************************************************
	template<size_t N>
	std::string ReadStr(const char(&src)[N])
	{
		return ReadStr(src, N);
	}

	//***************************************************************************
	// @brief 16진수 문자 1개 -> nibble(0~15).
	// @param c 변환할 문자(0-9, a-f, A-F)
	// @return nibble 값. 유효하지 않으면 -1.
	//***************************************************************************
	constexpr int HexNibble(int c) noexcept
	{
		if( c >= '0' && c <= '9' ) return c - '0';
		if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		return -1;
	}

	//***************************************************************************
	// @brief 바이트열 -> 소문자 16진 문자열.
	// @param data 변환할 바이트열
	// @param len data의 바이트 수
	// @return 소문자 16진 문자열(길이 = len * 2)
	//***************************************************************************
	inline std::string ToHex(const void* data, size_t len)
	{
		static constexpr char kDigits[] = "0123456789abcdef";
		const uint8_t* p = static_cast<const uint8_t*>(data);

		std::string out;
		out.resize(len * 2);
		for( size_t i = 0; i < len; ++i )
		{
			out[i * 2] = kDigits[p[i] >> 4];
			out[i * 2 + 1] = kDigits[p[i] & 0x0F];
		}
		return out;
	}

	//***************************************************************************
	// @brief std::array 바이트열 -> 소문자 16진 문자열.
	// @param bytes 변환할 배열
	// @return 소문자 16진 문자열(길이 = N * 2)
	//***************************************************************************
	template<size_t N>
	std::string ToHex(const std::array<uint8_t, N>& bytes)
	{
		return ToHex(bytes.data(), N);
	}

	//***************************************************************************
	// @brief 16진 문자열 -> 바이트열. 전부 검증한 뒤에만 쓰므로 실패 시 out은 변경되지 않는다.
	// @param hex 16진 문자열(대소문자 모두 허용)
	// @param out 결과를 쓸 버퍼
	// @param outLen out에 쓸 바이트 수
	// @return 성공 시 true. hex.size()가 outLen*2가 아니거나 16진수가 아닌 문자가 있으면 false.
	//***************************************************************************
	inline bool FromHex(std::string_view hex, void* out, size_t outLen) noexcept
	{
		if( hex.size() != outLen * 2 )
			return false;

		// 전부 검증한 뒤에만 쓴다 — 실패 시 out을 건드리지 않기 위해
		for( char c : hex )
		{
			if( HexNibble(static_cast<unsigned char>(c)) < 0 )
				return false;
		}

		uint8_t* p = static_cast<uint8_t*>(out);
		for( size_t i = 0; i < outLen; ++i )
		{
			const int hi = HexNibble(static_cast<unsigned char>(hex[i * 2]));
			const int lo = HexNibble(static_cast<unsigned char>(hex[i * 2 + 1]));
			p[i] = static_cast<uint8_t>((hi << 4) | lo);
		}
		return true;
	}

	//***************************************************************************
	// @brief 16진 문자열 -> std::array. 실패 시 out은 변경되지 않는다.
	// @param hex 16진 문자열(대소문자 모두 허용)
	// @param out 결과를 쓸 배열(N 바이트)
	// @return 성공 시 true. 길이가 N*2가 아니거나 16진수가 아닌 문자가 있으면 false.
	//***************************************************************************
	template<size_t N>
	bool FromHex(std::string_view hex, std::array<uint8_t, N>& out) noexcept
	{
		return FromHex(hex, out.data(), N);
	}

	//***************************************************************************
	// @brief 원시 바이트 포인터(N 바이트)를 std::array로 복사한다.
	// @param raw 복사할 원본. 최소 N 바이트가 유효해야 한다.
	// @return 복사된 배열
	//***************************************************************************
	template<size_t N>
	std::array<uint8_t, N> ToArray(const void* raw) noexcept
	{
		std::array<uint8_t, N> out;
		::memcpy(out.data(), raw, N);
		return out;
	}
}

#endif // ndef UC_FIXEDBUFUTIL_H