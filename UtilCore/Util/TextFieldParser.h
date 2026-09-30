
//***************************************************************************
// TextFieldParser.h : interface for the CTextFieldParser class.
//
//***************************************************************************

#ifndef UC_TEXTFIELDPARSER_H
#define UC_TEXTFIELDPARSER_H

#include <tchar.h>
#include <algorithm>
#include <cstddef>

//***************************************************************************
// @class CTextFieldParser
// @brief 구분자 기반 텍스트 한 줄을 필드 단위로 파싱하는 제로카피 유틸리티.
//
// @details
// 원본 버퍼를 수정하지 않고 [begin, end) 반열린 구간으로만 필드를 가리키며
// (Field), 문자열 복사나 숫자 변환은 필요한 시점에만 수행합니다.
// _tcstok 는 연속된 구분자를 하나로 합쳐 빈 필드가 사라지므로(컬럼 밀림
// 발생) 탭/쉼표 구분 텍스트 파일(예: CSV 유사 포맷)을 다룰 때는 이 파서를
// 사용합니다.
//
// 모든 멤버가 static이며 인스턴스 상태를 갖지 않으므로 인스턴스화하지
// 않고 사용합니다.
//***************************************************************************
class CTextFieldParser
{
public:
	CTextFieldParser() = delete;

	//***************************************************************************
	// @struct Field
	// @brief 원본 버퍼 [b, e) 구간을 가리키는 비소유(non-owning) 뷰.
	//***************************************************************************
	struct Field
	{
		const TCHAR* b;	// 필드 시작 위치
		const TCHAR* e;	// 필드 끝 위치(가리키는 문자는 포함하지 않음)
	};

	//***************************************************************************
	// @brief ASCII 공백 문자 여부를 판정합니다.
	// @details 로캘/MBCS 영향을 받는 _istspace 대신 ASCII 공백만 판정하여,
	//          DBCS 후행 바이트를 공백으로 오판하는 상황(디버그 CRT assert 등)을
	//          막습니다.
	// @param c 판정할 문자
	// @return bool 공백(스페이스/탭/CR/LF/수직탭/폼피드)이면 true
	//***************************************************************************
	static bool IsBlank(TCHAR c)
	{
		return c == _T(' ') || c == _T('\t') || c == _T('\r') || c == _T('\n') || c == _T('\v') || c == _T('\f');
	}

	//***************************************************************************
	// @brief 필드 우측의 공백을 제거합니다(구간 축소, 복사 없음).
	// @param f 우측 경계(e)를 축소할 필드
	//***************************************************************************
	static void TrimRight(Field& f)
	{
		while( f.e > f.b && IsBlank(f.e[-1]) ) --f.e;
	}

	//***************************************************************************
	// @brief 필드 좌우의 공백을 모두 제거합니다(구간 축소, 복사 없음).
	// @param f 좌우 경계(b, e)를 축소할 필드
	//***************************************************************************
	static void TrimBoth(Field& f)
	{
		while( f.b < f.e && IsBlank(*f.b) ) ++f.b;
		TrimRight(f);
	}

	//***************************************************************************
	// @brief [b, e) 구간을 chDelim 기준으로 최대 nMax개 필드로 분리합니다.
	// @details 연속된 구분자도 빈 필드로 보존합니다(컬럼 밀림 방지). 필드가
	//          nMax개를 초과하면 나머지는 버립니다(끝 필드에 구분자가 남아있는
	//          채로 잘릴 수 있음 — 호출자가 열 개수를 알고 있어야 합니다).
	// @param b     분리할 구간의 시작 위치
	// @param e     분리할 구간의 끝 위치(가리키는 문자는 포함하지 않음)
	// @param chDelim 필드 구분자(탭 '\t', 쉼표 ',' 등)
	// @param out 분리된 필드를 채울 배열(최소 nMax개 크기)
	// @param nMax  out 배열의 크기(채울 수 있는 최대 필드 수)
	// @return size_t 실제로 채운 필드 개수
	//***************************************************************************
	static size_t SplitFields(const TCHAR* b, const TCHAR* e, TCHAR chDelim, Field* out, size_t nMax)
	{
		size_t n = 0;
		const TCHAR* fb = b;

		for( const TCHAR* c = b; n < nMax; ++c )
		{
			if( c == e || *c == chDelim )
			{
				out[n].b = fb;
				out[n].e = c;
				++n;

				if( c == e ) break;
				fb = c + 1;
			}
		}
		return n;
	}

	//***************************************************************************
	// @brief 필드를 부호 없는 10진 정수로 파싱합니다(공백 트림 후 숫자만 허용).
	// @details 0 ~ 0xFFFFFFFF 범위를 비트 그대로 int 에 담습니다(마스크 값
	//          대응). 실패 시 out 은 변경하지 않습니다.
	// @param f   파싱할 필드
	// @param out 파싱 성공 시 결과를 담을 변수
	// @return bool 파싱 성공 여부
	//***************************************************************************
	static bool ParseInt(Field f, int& out)
	{
		TrimBoth(f);
		if( f.b == f.e ) return false;

		unsigned long long v = 0;
		for( const TCHAR* p = f.b; p < f.e; ++p )
		{
			if( *p < _T('0') || *p > _T('9') ) return false;

			v = v * 10 + static_cast<unsigned long long>(*p - _T('0'));
			if( v > 0xFFFFFFFFull ) return false;
		}

		out = static_cast<int>(static_cast<unsigned int>(v));
		return true;
	}

	//***************************************************************************
	// @brief 필드를 우측 공백 제거 후 고정 배열에 복사합니다(항상 NUL 종료).
	// @details 배열 크기는 sizeof 가 아닌 원소 개수(N) 기준입니다.
	// @tparam N dst 배열의 원소 개수
	// @param dst 복사 대상 고정 배열
	// @param f   복사할 필드(내부에서 우측 공백 제거 후 사용)
	//***************************************************************************
	template<size_t N>
	static void CopyStr(TCHAR(&dst)[N], Field f)
	{
		TrimRight(f);

		size_t len = static_cast<size_t>(f.e - f.b);
		if( len > N - 1 ) len = N - 1;

		std::copy(f.b, f.b + len, dst);
		dst[len] = _T('\0');
	}

	//***************************************************************************
	// @brief 고정 배열(src)의 내용을 크기(cchDst)가 주어진 임의 버퍼로
	//        복사합니다(항상 NUL 종료, 필요 시 잘림).
	// @tparam N   src 배열의 원소 개수
	// @param pDst 복사 대상 버퍼(nullptr 이거나 cchDst가 0이면 아무 것도 하지 않음)
	// @param cchDst pDst 버퍼의 크기(원소 개수, NUL 포함)
	// @param src   복사할 원본 고정 배열
	//***************************************************************************
	template<size_t N>
	static void CopyTruncate(TCHAR* pDst, size_t cchDst, const TCHAR(&src)[N])
	{
		if( !pDst || cchDst == 0 ) return;

		size_t len = _tcsnlen(src, N);
		if( len > cchDst - 1 ) len = cchDst - 1;

		std::copy(src, src + len, pDst);
		pDst[len] = _T('\0');
	}

	//***************************************************************************
	// @brief idx번째 필드가 존재하면 파싱하여 out에 채웁니다(없으면 out 유지).
	//        선택적(optional) 컬럼을 다루는 로더에서 사용합니다.
	// @param f   필드 배열
	// @param n   f 배열에 실제로 채워진 필드 개수
	// @param idx 읽을 필드의 인덱스
	// @param out idx번째 필드가 있고 파싱에 성공하면 갱신되는 변수
	//***************************************************************************
	static void OptInt(const Field* f, size_t n, size_t idx, int& out)
	{
		if( idx < n ) ParseInt(f[idx], out);
	}

	//***************************************************************************
	// @brief idx번째 필드가 존재하면 dst에 복사합니다(없으면 dst 유지).
	//        선택적(optional) 컬럼을 다루는 로더에서 사용합니다.
	// @tparam N dst 배열의 원소 개수
	// @param f   필드 배열
	// @param n   f 배열에 실제로 채워진 필드 개수
	// @param idx 읽을 필드의 인덱스
	// @param dst idx번째 필드가 있으면 복사될 고정 배열
	//***************************************************************************
	template<size_t N>
	static void OptStr(const Field* f, size_t n, size_t idx, TCHAR(&dst)[N])
	{
		if( idx < n ) CopyStr(dst, f[idx]);
	}
};

#endif // ndef UC_TEXTFIELDPARSER_H