
//***************************************************************************
// DelimitedRowFile.h : 구분자 텍스트 파일을 행(T) 벡터로 적재/저장하는 공용
//                       템플릿 함수 모음.
//
//***************************************************************************

#ifndef UC_DELIMITEDROWFILE_H
#define UC_DELIMITEDROWFILE_H

#include <Util/TextFieldParser.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <vector>

//***************************************************************************
// @brief 구분자 텍스트 파일을 한 번에 읽어 행(T) 벡터로 적재합니다.
//
// @details
// 파일 전체를 한 번만 읽고(ReadFileMap) 제로카피로 라인을 분리합니다.
// \n, \r\n 모두 지원하고, UTF-8/UTF-16 BOM을 제거하며, 빈 줄(공백만 있는
// 줄 포함)은 건너뜁니다. 마지막 줄에 개행이 없어도 정상 처리됩니다.
// parseFn이 실패를 반환한 라인은 버리고 개수만 세어 로그로 남깁니다
// (해당 행의 키가 잘못돼 캐시에 쓰레기 값이 들어가는 것을 막기 위함).
//
// @tparam T 행 타입(값 시맨틱, 기본 생성 가능해야 함)
// @tparam ParseFn bool(const CTextFieldParser::Field* fields, size_t nFields, T& outRow)
//         형태로 호출 가능한 콜러블. 필수 필드가 없거나 파싱에 실패하면
//         false를 반환해 해당 라인을 버립니다.
// @tparam MaxFields 한 줄에서 분리할 최대 필드 수(스택 배열 크기). 파일의
//         실제 컬럼 수보다 커야 합니다.
//
// @param ptszFile   읽을 파일 경로
// @param chDelim    필드 구분자(탭 '\t', 쉼표 ',' 등)
// @param parseFn    필드 배열 -> 행(T) 변환 콜러블
// @param rows 적재된 행. 실패 시(또는 유효한 행이 하나도 없으면) 비워짐
//
// @return BOOL 유효한 행이 하나 이상 적재되면 TRUE, 아니면 FALSE
//***************************************************************************
template<class T, class ParseFn, size_t MaxFields = 16>
BOOL LoadDelimitedRows(const TCHAR* ptszFile, TCHAR chDelim, ParseFn parseFn, std::vector<T>& rows)
{
	using Field = CTextFieldParser::Field;

	rows.clear();
	if( !ptszFile || !*ptszFile ) return FALSE;

	_tstring text;
	// ReadFileMap 은 비const TCHAR* 를 받을 수 있으므로 const_cast (읽기만 함)
	if( !ReadFileMap(text, const_cast<TCHAR*>(ptszFile)) )
	{
		LOG_ERROR(_T(" # LoadDelimitedRows # ReadFileMap Fail, %s "), ptszFile);
		return FALSE;
	}

	// text.length() 사용 : _tcslen(p) 로 다시 찾으면 파일 내용에 임베디드 NUL이
	// 있을 때 거기서 끊겨 뒤쪽 데이터를 놓친다. text는 이미 정확한 길이를 알고
	// 있으므로 재스캔도 피한다.
	const TCHAR* p = text.c_str();
	const TCHAR* const end = p + text.length();

	// BOM 제거
#ifdef _UNICODE
	if( p < end && *p == static_cast<TCHAR>(0xFEFF) ) ++p;
#else
	if( end - p >= 3 && static_cast<unsigned char>(p[0]) == 0xEF && static_cast<unsigned char>(p[1]) == 0xBB && static_cast<unsigned char>(p[2]) == 0xBF ) p += 3;
#endif

	// std::count 는 표준 라이브러리 구현에 따라 수동 루프보다 벡터화되어 더
	// 빠를 수 있다(여전히 O(N) 스캔이지만, reserve()가 뒤의 파싱 루프에서
	// 재할당을 없애 주는 이득이 이 비용보다 크다).
	const size_t nLines = 1 + static_cast<size_t>(std::count(p, end, _T('\n')));
	rows.reserve(nLines);

	size_t	nSkipped = 0;
	Field	fields[MaxFields];

	while( p < end )
	{
		const TCHAR* lineEnd = p;
		while( lineEnd < end && *lineEnd != _T('\n') ) ++lineEnd;

		const TCHAR* next = (lineEnd < end) ? lineEnd + 1 : end;

		if( lineEnd > p && lineEnd[-1] == _T('\r') ) --lineEnd;

		bool bBlank = true;
		for( const TCHAR* c = p; c < lineEnd; ++c )
		{
			if( !CTextFieldParser::IsBlank(*c) ) { bBlank = false; break; }
		}

		if( !bBlank )
		{
			size_t	n = CTextFieldParser::SplitFields(p, lineEnd, chDelim, fields, MaxFields);
			T		row;

			if( parseFn(fields, n, row) )
				rows.push_back(row);
			else
				++nSkipped;
		}

		p = next;
	}

	if( nSkipped > 0 )
		LOG_WARNING(_T(" # LoadDelimitedRows # %s, Rows = %zu, Skipped = %zu "), ptszFile, rows.size(), nSkipped);
	else
		LOG_INFO(_T(" # LoadDelimitedRows # %s, Rows = %zu "), ptszFile, rows.size());

	return rows.empty() ? FALSE : TRUE;
}

//***************************************************************************
// @brief vector -> 레거시 포인터 배열 변환(호환 API용).
// @details rows를 new[]로 할당한 (rows.size()+1)개 배열로 복사합니다(끝에
//          빈 센티널 1개 포함). 해제는 호출자의 delete[] 책임입니다.
// @param rows 변환할 원본 행 목록
// @param pOut new[]로 할당된 배열(실패 시 nullptr)
// @param nSize pOut의 유효 행 개수(실패 시 0, 센티널 제외)
// @return BOOL 성공 시 TRUE, rows가 비었거나 INT_MAX 이상이면 FALSE
//***************************************************************************
template<class T>
BOOL ToLegacyArray(const std::vector<T>& rows, T*& pOut, int& nSize)
{
	pOut = nullptr;
	nSize = 0;

	if( rows.empty() || rows.size() >= static_cast<size_t>(INT_MAX) ) return FALSE;

	pOut = new T[rows.size() + 1];
	std::copy(rows.begin(), rows.end(), pOut);
	nSize = static_cast<int>(rows.size());
	return TRUE;
}

//***************************************************************************
// @brief 행 배열을 텍스트 파일로 저장합니다. T는 GetStream(TCHAR*, size_t)
//        멤버(항상 NUL 종료, 개행 포함)를 제공해야 합니다.
// @tparam T GetStream(TCHAR* pBuf, size_t cchBuf) const 멤버를 갖는 행 타입
// @param ptszFile 저장할 파일 경로(기존 내용은 덮어씀)
// @param pRows    저장할 행 배열의 시작 포인터(nSize가 0이면 nullptr 허용)
// @param nSize    pRows의 행 개수
// @return BOOL 전부 정상적으로 쓰고 파일을 닫았으면 TRUE
//***************************************************************************
template<class T>
BOOL WriteDelimitedRows(const TCHAR* ptszFile, const T* pRows, int nSize)
{
	if( !ptszFile || nSize < 0 || (nSize > 0 && !pRows) ) return FALSE;

	FILE* pTargetFile = NULL;

	_tfopen_s(&pTargetFile, ptszFile, _T("wt"));
	if( !pTargetFile )
	{
		LOG_ERROR(_T("# WriteDelimitedRows Open Fail #,%s"), ptszFile);
		return FALSE;
	}

	BOOL	bRet = TRUE;
	TCHAR	tszBuffer[MAX_BUFFER_SIZE];

	for( int i = 0; i < nSize; ++i )
	{
		pRows[i].GetStream(tszBuffer, _countof(tszBuffer));

		if( _fputts(tszBuffer, pTargetFile) == _TEOF )
		{
			LOG_ERROR(_T("# WriteDelimitedRows Write Fail #,%s"), ptszFile);
			bRet = FALSE;
			break;
		}
	}

	if( fclose(pTargetFile) != 0 )
	{
		LOG_ERROR(_T("# WriteDelimitedRows Close Fail #,%s"), ptszFile);
		bRet = FALSE;
	}

	return bRet;
}

#endif // ndef UC_DELIMITEDROWFILE_H