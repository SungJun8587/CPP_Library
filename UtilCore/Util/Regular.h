
//***************************************************************************
// Regular.h: interface for the Reqular Expression Functions.
//
//***************************************************************************

#ifndef UC_REGULAR_H
#define UC_REGULAR_H

#include <tchar.h>

// 로캘 비의존 ASCII 판별. 아래 IsAll*(TCHAR, 로캘 의존)과 달리 항상 A-Z/a-z/0-9만 참.
namespace AsciiChar
{
	constexpr bool IsAlpha(int c) noexcept { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
	constexpr bool IsDigit(int c) noexcept { return c >= '0' && c <= '9'; }
	constexpr bool IsAlnum(int c) noexcept { return IsAlpha(c) || IsDigit(c); }

	// 영숫자 + URL에서 흔히 쓰는 구분 문자("-.?/&=:")
	constexpr bool IsCharacter(int c) noexcept
	{
		return IsAlnum(c) || c == '-' || c == '.' || c == '?' || c == '/' || c == '&' || c == '=' || c == ':';
	}

	// 최상위 비트(0x80)가 켜진 값 = ANSI 멀티바이트(한글 등) 바이트
	constexpr bool IsKoreanChar(int c) noexcept { return (c & 0x80) != 0; }
}

bool	IsAllAscii(const TCHAR* ptszSource);
bool	IsAllAlphaNum(const TCHAR* ptszSource);
bool	IsAllAlphaKor(const TCHAR* ptszSource);
bool	IsAllKorNum(const TCHAR* ptszSource);
bool	IsAllAlphaKorNum(const TCHAR* ptszSource);

bool	IsAllAlpha(const TCHAR* ptszSource);
bool	IsAllKorean(const TCHAR* ptszSource);
bool	IsAllNumeric(const TCHAR* ptszSource);

#endif // ndef UC_REGULAR_H