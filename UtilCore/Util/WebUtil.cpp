//***************************************************************************
// WebUtil.cpp: implementation of the WebUtil Function.
//
//***************************************************************************

#include "pch.h"
#include "WebUtil.h"
#include "ConvertCharset.h" // ConvertCharset 함수 사용을 위한 헤더 포함

#include <array>
#include <cstring>

//***************************************************************************
static const char g_pcDigits[16] = {
	'0', '1' , '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F'
};

//***************************************************************************
static const char g_pcMimeBase64[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

//***************************************************************************
static int g_pnDecodeMimeBase64[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

//===========================================================================
// 퍼센트 인코딩/디코딩 계열(UrlEncode/Decode, EncodeURI(Component), UrlPathEncode)의
// 공통 로직. 문자 판별 테이블과 hex 파싱을 한 곳으로 모아, 문자 판별 오타나
// malformed 입력에 대한 방어 누락이 함수마다 중복되는 것을 방지한다.
//===========================================================================

// @brief 영숫자 여부 판별 ('0'-'9','A'-'Z','a'-'z')
static inline bool IsAlnum(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// @brief hex 문자 1개를 nibble 값으로 변환. 유효하지 않으면 -1 반환.
// (malformed % 시퀀스에서 잘못된 문자를 그대로 계산에 사용하는 것을 방지)
static inline int HexNibble(TCHAR c)
{
	if( c >= '0' && c <= '9' ) return c - '0';
	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	return -1;
}

// @brief 안전 문자(퍼센트 인코딩하지 않을 문자) 판별 테이블 생성.
// 영숫자는 기본 포함, pszExtraSafe에 나열된 문자를 추가로 안전 문자 취급한다.
static inline std::array<bool, 256> BuildSafeTable(const char* pszExtraSafe)
{
	std::array<bool, 256> table{};
	for( int i = 0; i < 256; i++ )
		table[i] = IsAlnum((unsigned char)i);

	if( pszExtraSafe != nullptr )
	{
		for( const char* p = pszExtraSafe; *p; p++ )
			table[(unsigned char)*p] = true;
	}
	return table;
}

// @brief 바이트 시퀀스를 퍼센트 인코딩. 안전 문자는 그대로, 공백은 옵션에 따라
// '+' 또는 %20으로, 나머지는 %XX로 인코딩한다. 1-pass로 처리하여 사이즈 계산과
// 채우기 로직이 어긋날 여지를 없앤다.
static inline std::string PercentEncodeBytes(const std::string& source, const std::array<bool, 256>& safeTable, bool bSpaceAsPlus)
{
	std::string dest;
	dest.reserve(source.size());

	for( unsigned char cChar : source )
	{
		if( safeTable[cChar] )
			dest.push_back((char)cChar);
		else if( bSpaceAsPlus && cChar == ' ' )
			dest.push_back('+');
		else
		{
			dest.push_back('%');
			dest.push_back(g_pcDigits[(cChar >> 4) & 0x0F]);
			dest.push_back(g_pcDigits[cChar & 0x0F]);
		}
	}
	return dest;
}

// @brief 퍼센트 인코딩 공통 진입점. source를 iCodePage(ANSI/UTF-8)로 변환한 뒤
// 이미 만들어진 안전 문자 테이블로 인코딩한다. 테이블은 extras가 고정된 각
// 호출부(UrlEncode 등)에서 함수-로컬 static으로 한 번만 생성해 넘겨준다 —
// 이 함수 내부에서 매번 새로 만들지 않는다.
static inline _tstring PercentEncodeCoreWithTable(const _tstring& source, int iCodePage, const std::array<bool, 256>& safeTable, bool bSpaceAsPlus)
{
	if( source.empty() ) return _T("");

	std::string sourceData;
#ifdef _UNICODE
	sourceData = (iCodePage == CP_UTF8)
		? UnicodeToUtf8(TStringToWString(source))
		: UnicodeToAnsi(TStringToWString(source));
#else
	sourceData = (iCodePage == CP_UTF8) ? AnsiToUtf8(source) : source;
#endif

	std::string destData = PercentEncodeBytes(sourceData, safeTable, bSpaceAsPlus);

#ifdef _UNICODE
	return WStringToTString(AnsiToUnicode(destData));
#else
	return destData;
#endif
}

// @brief 퍼센트 디코딩 공통 진입점. 인덱스 기반으로 경계를 확인하며 처리하므로
// "%"로 끝나거나 hex가 아닌 문자가 뒤따르는 malformed 입력에서도 문자열 경계를
// 벗어나지 않는다. 그런 경우 '%'는 리터럴 문자로 취급한다.
static inline _tstring PercentDecodeCore(const _tstring& source, int iCodePage, bool bPlusAsSpace)
{
	if( source.empty() ) return _T("");

	const size_t n = source.size();
	std::string destData;
	destData.reserve(n);

	size_t i = 0;
	while( i < n )
	{
		TCHAR ch = source[i];

		if( ch == '%' && i + 2 < n )
		{
			int hi = HexNibble(source[i + 1]);
			int lo = HexNibble(source[i + 2]);
			if( hi >= 0 && lo >= 0 )
			{
				destData.push_back((char)((hi << 4) | lo));
				i += 3;
				continue;
			}
			// 잘못된 %XX 시퀀스 -> '%'는 리터럴로 취급하고 한 글자만 전진
		}
		else if( bPlusAsSpace && ch == '+' )
		{
			destData.push_back(' ');
			i++;
			continue;
		}

		destData.push_back((char)ch);
		i++;
	}

	_tstring dest;
#ifdef _UNICODE
	dest = (iCodePage == CP_ACP)
		? WStringToTString(AnsiToUnicode(destData))
		: WStringToTString(Utf8ToUnicode(destData));
#else
	dest = (iCodePage == CP_ACP) ? destData : Utf8ToAnsi(destData);
#endif
	return dest;
}

//***************************************************************************
// @brief 문자열을 Base64 형식으로 인코딩합니다.
// @param source 인코딩할 원본 문자열
// @return Base64로 인코딩된 결과 문자열
_tstring Base64Enc(const _tstring& source)
{
	if( source.empty() ) return _T("");

	std::string sourceData;
#ifdef _UNICODE
	sourceData = UnicodeToUtf8(TStringToWString(source)); // UTF-8로 변환하여 다국어 지원
#else
	// 멀티바이트 환경인 경우 필요에 따라 Utf8 변환 거치기
	sourceData = AnsiToUtf8(source);
#endif

	const unsigned char* pSrc = reinterpret_cast<const unsigned char*>(sourceData.data());
	const size_t length = sourceData.size(); // 종료 널 문자는 인코딩 대상에서 제외

	std::string dest;
	dest.reserve(((length + 2) / 3) * 4);

	size_t i = 0;
	for( ; i + 3 <= length; i += 3 )
	{
		int c1 = pSrc[i], c2 = pSrc[i + 1], c3 = pSrc[i + 2];
		dest.push_back(g_pcMimeBase64[(c1 & 0xFC) >> 2]);
		dest.push_back(g_pcMimeBase64[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)]);
		dest.push_back(g_pcMimeBase64[((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6)]);
		dest.push_back(g_pcMimeBase64[c3 & 0x3F]);
	}

	const size_t remain = length - i;
	if( remain == 1 )
	{
		int c1 = pSrc[i];
		dest.push_back(g_pcMimeBase64[(c1 & 0xFC) >> 2]);
		dest.push_back(g_pcMimeBase64[(c1 & 0x03) << 4]);
		dest.push_back('=');
		dest.push_back('=');
	}
	else if( remain == 2 )
	{
		int c1 = pSrc[i], c2 = pSrc[i + 1];
		dest.push_back(g_pcMimeBase64[(c1 & 0xFC) >> 2]);
		dest.push_back(g_pcMimeBase64[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)]);
		dest.push_back(g_pcMimeBase64[(c2 & 0x0F) << 2]);
		dest.push_back('=');
	}

#ifdef _UNICODE
	return WStringToTString(AnsiToUnicode(dest));
#else
	return dest;
#endif
}

//***************************************************************************
// @brief Base64로 인코딩된 문자열을 디코딩합니다.
// @param source 디코딩할 Base64 문자열
// @param pResult 실패 원인을 받을 out 파라미터. nullptr이면 결과 확인을 생략(기존 호출부 호환용).
// @return 디코딩된 결과 문자열. 실패 시 빈 문자열을 반환하며 pResult에 원인이 채워진다.
_tstring Base64Dec(const _tstring& source, Base64DecodeResult* pResult)
{
	if( pResult ) *pResult = Base64DecodeResult::Success;
	if( source.empty() ) return _T("");

	if( source.size() % 4 != 0 )
	{
		if( pResult ) *pResult = Base64DecodeResult::InvalidLength;
		return _T("");
	}
	const size_t length = source.size();

	std::string destData;
	destData.reserve((length / 4) * 3);

	for( size_t i = 0; i < length; i += 4 )
	{
		TCHAR c1 = source[i], c2 = source[i + 1], c3 = source[i + 2], c4 = source[i + 3];

		int e1 = ((unsigned)c1 < 256) ? g_pnDecodeMimeBase64[(unsigned char)c1] : -1;
		int e2 = ((unsigned)c2 < 256) ? g_pnDecodeMimeBase64[(unsigned char)c2] : -1;
		int e3 = (c3 == '=') ? 0 : (((unsigned)c3 < 256) ? g_pnDecodeMimeBase64[(unsigned char)c3] : -1);
		int e4 = (c4 == '=') ? 0 : (((unsigned)c4 < 256) ? g_pnDecodeMimeBase64[(unsigned char)c4] : -1);

		// 패딩('=')은 블록의 마지막 두 자리에서만 허용
		if( c3 == '=' && c4 != '=' ) { if( pResult ) *pResult = Base64DecodeResult::InvalidCharacter; return _T(""); }

		if( e1 < 0 || e2 < 0 || e3 < 0 || e4 < 0 )
		{
			// 유효하지 않은 문자를 포함한 블록 발견 -> 즉시 실패 처리 (fail-closed)
			if( pResult ) *pResult = Base64DecodeResult::InvalidCharacter;
			return _T("");
		}

		destData.push_back((char)((e1 << 2) | ((e2 & 0x30) >> 4)));
		if( c3 != '=' )
			destData.push_back((char)(((e2 & 0xf) << 4) | ((e3 & 0x3c) >> 2)));
		if( c4 != '=' )
			destData.push_back((char)(((e3 & 0x3) << 6) | e4));
	}

	_tstring dest;
#ifdef _UNICODE
	dest = WStringToTString(Utf8ToUnicode(destData));	// UTF-8로 디코딩하도록 변경
#else
	dest = Utf8ToAnsi(destData); // 멀티바이트 환경인 경우
#endif
	return dest;
}

//***************************************************************************
// @brief 문자열을 URL 인코딩 형식으로 변환합니다.
// @param source 변환할 원본 문자열
// @param iCodePage 사용할 코드 페이지 (CP_ACP 또는 CP_UTF8)
// @return URL 인코딩된 결과 문자열
_tstring UrlEncode(const _tstring& source, const int iCodePage)
{
	static const std::array<bool, 256> table = BuildSafeTable("");
	return PercentEncodeCoreWithTable(source, iCodePage, table, true);
}

//***************************************************************************
// @brief URL 인코딩된 문자열을 디코딩합니다.
// @param source 디코딩할 URL 문자열
// @param iCodePage 사용할 코드 페이지
// @return 디코딩된 결과 문자열
_tstring UrlDecode(const _tstring& source, const int iCodePage)
{
	return PercentDecodeCore(source, iCodePage, true);
}

//***************************************************************************
// @brief URL 경로(Path) 영역에 맞춰 문자열을 인코딩합니다.
// @param source 변환할 원본 문자열
// @return 인코딩된 결과 문자열
_tstring UrlPathEncode(const _tstring& source)
{
	if( source.empty() ) return _T("");

	// 원본 사양: 제어문자(0~31)는 그대로 통과시키고, 공백(32)과 128 이상 바이트만
	// 퍼센트 인코딩한다. 공백을 '+'로 바꾸지 않는다는 점이 UrlEncode와 다르다.
	static const std::array<bool, 256> table = [] {
		std::array<bool, 256> t{};
		for( int i = 0; i < 256; i++ )
			t[i] = (i < 32) || (i > 32 && i < 128);
		return t;
		}();

	std::string sourceData;
#ifdef _UNICODE
	sourceData = UnicodeToUtf8(TStringToWString(source));
#else
	sourceData = AnsiToUtf8(source);
#endif

	std::string destData = PercentEncodeBytes(sourceData, table, false);

#ifdef _UNICODE
	return WStringToTString(AnsiToUnicode(destData));
#else
	return destData;
#endif
}

//***************************************************************************
// @brief 특수 문자를 HTML 엔티티 코드로 변환합니다.
// @param source 변환할 원본 문자열
// @return HTML 인코딩된 결과 문자열
_tstring HtmlEncode(const _tstring& source)
{
	if( source.empty() ) return _T("");

	static const struct { TCHAR ch; const TCHAR* entity; } s_table[] = {
		{ '<',  _T("&lt;")   },
		{ '>',  _T("&gt;")   },
		{ '&',  _T("&amp;")  },
		{ '"',  _T("&quot;") },
		{ '\'', _T("&#39;")  }, // 작은따옴표 속성 컨텍스트 XSS 방어
	};

	_tstring dest;
	dest.reserve(source.size());

	for( TCHAR ch : source )
	{
		bool bMatched = false;
		for( const auto& entry : s_table )
		{
			if( ch == entry.ch )
			{
				dest += entry.entity;
				bMatched = true;
				break;
			}
		}
		if( !bMatched )
			dest.push_back(ch);
	}
	return dest;
}

//***************************************************************************
// @brief HTML 엔티티 코드로 변환된 문자열을 원래 문자로 디코딩합니다.
// @param source 디코딩할 HTML 문자열
// @return 디코딩된 결과 문자열
_tstring HtmlDecode(const _tstring& source)
{
	if( source.empty() ) return _T("");

	static const struct { const TCHAR* entity; TCHAR ch; } s_table[] = {
		{ _T("&lt;"),   '<' },
		{ _T("&gt;"),   '>' },
		{ _T("&amp;"),  '&' },
		{ _T("&quot;"), '"' },
		{ _T("&#39;"),  '\'' },
		{ _T("&apos;"), '\'' }, // HTML5 명명 엔티티 형태도 함께 디코딩 대상에 포함
	};

	const size_t n = source.size();
	_tstring dest;
	dest.reserve(n);

	size_t i = 0;
	while( i < n )
	{
		bool bMatched = false;
		for( const auto& entry : s_table )
		{
			size_t entLen = _tcslen(entry.entity);
			if( i + entLen <= n && source.compare(i, entLen, entry.entity) == 0 )
			{
				dest.push_back(entry.ch);
				i += entLen;
				bMatched = true;
				break;
			}
		}
		if( !bMatched )
		{
			dest.push_back(source[i]);
			i++;
		}
	}
	return dest;
}

//***************************************************************************
// @brief 자바스크립트 Escape 형식으로 문자열을 인코딩합니다.
// @param source 변환할 원본 문자열
// @return 인코딩된 결과 문자열
_tstring Escape(const _tstring& source)
{
	if( source.empty() ) return _T("");

	wchar_t		wcChar = '\0';
	const wchar_t* pwszSourceData = nullptr;
	const wchar_t	wszExcept[] = L"*@-_+./";

	std::wstring sourceData;
#ifdef _UNICODE
	pwszSourceData = source.c_str();
#else
	sourceData = AnsiToUnicode(source);
	pwszSourceData = sourceData.c_str();
#endif

	_tstring dest;
	dest.reserve(source.size());

	const wchar_t* pwszSourceDoc = pwszSourceData;
	while( *pwszSourceDoc )
	{
		wcChar = *pwszSourceDoc;
		if( wcChar > 0x7f )
		{
			dest.push_back('%');
			dest.push_back('u');
			dest.push_back(g_pcDigits[(wcChar >> 12) & 0x0F]);
			dest.push_back(g_pcDigits[(wcChar >> 8) & 0x0F]);
			dest.push_back(g_pcDigits[(wcChar >> 4) & 0x0F]);
			dest.push_back(g_pcDigits[wcChar & 0x0F]);
		}
		else if( !(IsAlnum((unsigned char)wcChar) || wcschr(wszExcept, wcChar)) )
		{
			dest.push_back('%');
			dest.push_back(g_pcDigits[(wcChar >> 4) & 0x0F]);
			dest.push_back(g_pcDigits[wcChar & 0x0F]);
		}
		else
			dest.push_back((TCHAR)wcChar);

		pwszSourceDoc++;
	}
	return dest;
}

//***************************************************************************
// @brief Escape 형식으로 인코딩된 문자열을 디코딩합니다.
// @param source 디코딩할 문자열
// @return 디코딩된 결과 문자열
_tstring UnEscape(const _tstring& source)
{
	if( source.empty() ) return _T("");

	const size_t n = source.size();
	std::wstring destData;
	destData.reserve(n);

	size_t i = 0;
	while( i < n )
	{
		TCHAR ch = source[i];

		if( ch == '%' && i + 1 < n && source[i + 1] == 'u' && i + 5 < n )
		{
			int n1 = HexNibble(source[i + 2]);
			int n2 = HexNibble(source[i + 3]);
			int n3 = HexNibble(source[i + 4]);
			int n4 = HexNibble(source[i + 5]);
			if( n1 >= 0 && n2 >= 0 && n3 >= 0 && n4 >= 0 )
			{
				destData.push_back((wchar_t)((n1 << 12) | (n2 << 8) | (n3 << 4) | n4));
				i += 6;
				continue;
			}
			// 잘못된 %uXXXX 시퀀스 -> '%'는 리터럴로 취급
		}
		else if( ch == '%' && i + 2 < n )
		{
			int hi = HexNibble(source[i + 1]);
			int lo = HexNibble(source[i + 2]);
			if( hi >= 0 && lo >= 0 )
			{
				destData.push_back((wchar_t)((hi << 4) | lo));
				i += 3;
				continue;
			}
			// 잘못된 %XX 시퀀스 -> '%'는 리터럴로 취급
		}

		destData.push_back((wchar_t)ch);
		i++;
	}

	_tstring dest;
#ifndef _UNICODE
	dest = WStringToString(destData);
#else
	dest = destData;
#endif
	return dest;
}

//***************************************************************************
// @brief 자바스크립트 EncodeURI 형식으로 문자열을 인코딩합니다.
// @param source 변환할 원본 문자열
// @return 인코딩된 결과 문자열
// @param bSpaceAsPlus true면 기존 동작(공백->'+') 유지, false면 JS encodeURI 표준(공백->%20).
_tstring EncodeURI(const _tstring& source, bool bSpaceAsPlus)
{
	static const std::array<bool, 256> table = BuildSafeTable(",/?:@&=+$#");
	return PercentEncodeCoreWithTable(source, CP_UTF8, table, bSpaceAsPlus);
}

//***************************************************************************
// @brief EncodeURI 형식으로 인코딩된 문자열을 디코딩합니다.
// @param source 디코딩할 문자열
// @return 디코딩된 결과 문자열
_tstring DecodeURI(const _tstring& source)
{
	return PercentDecodeCore(source, CP_UTF8, true);
}

//***************************************************************************
// @brief 자바스크립트 EncodeURIComponent 형식으로 문자열을 인코딩합니다.
// @param source 변환할 원본 문자열
// @return 인코딩된 결과 문자열
_tstring EncodeURIComponent(const _tstring& source)
{
	static const std::array<bool, 256> table = BuildSafeTable("");
	return PercentEncodeCoreWithTable(source, CP_UTF8, table, true);
}

//***************************************************************************
// @brief EncodeURIComponent 형식으로 인코딩된 문자열을 디코딩합니다.
// @param source 디코딩할 문자열
// @return 디코딩된 결과 문자열
_tstring DecodeURIComponent(const _tstring& source)
{
	return PercentDecodeCore(source, CP_UTF8, true);
}