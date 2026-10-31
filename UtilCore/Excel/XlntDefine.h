
//***************************************************************************
// XlntDefine.h : Common header file for Excel including macros, constants, and types.
//
//***************************************************************************

#ifndef UC_XLNTDEFINE_H
#define UC_XLNTDEFINE_H

#include <map>
#include <string>
#include <vector>

namespace Xlnt
{
	//***************************************************************************
	// Constants
	//***************************************************************************

	constexpr size_t		XLSX_MAX_SHEET_NAME_LEN = 31;			// 시트 이름 최대 길이 (UTF-16 단위)
	constexpr size_t		XLSX_MAX_CELL_TEXT_BYTES = 32767;		// 셀 문자열 최대 길이 (UTF-8 바이트). 엑셀의 한도는 UTF-16 글자 수지만 xlnt가 바이트로 잘라(한글은 글자 중간에서 잘려 저장이 실패한다) 이 값이 한도
	constexpr double		XLSX_MAX_DATE_SERIAL = 2958466.0;		// 날짜 일련번호 상한(미포함). 9999-12-31이 2958465
	constexpr int			XLSX_MAX_ROW = 1048576;					// xlsx 최대 행 번호
	constexpr size_t		XLSX_MAX_COLUMN = 16384;				// xlsx 최대 열 수 (XFD)
	constexpr const char* XLSX_INVALID_SHEET_CHARS = "[]:*?/\\";	// 시트 이름에 쓸 수 없는 문자
	constexpr double		XLSX_EXACT_INTEGER_LIMIT = 1e15;		// 엑셀이 정확히 저장하는 정수의 상한 (유효숫자 15자리)

	//***************************************************************************
	// Types
	//***************************************************************************

	//***************************************************************************
	// @struct SCell
	// @brief 셀 하나의 값. 엑셀이 저장한 타입(숫자/문자열/불리언)을 그대로 보존한다.
	//***************************************************************************
	struct SCell
	{
		enum Kind { Empty, Num, Str, Bool } kind = Empty;
		std::string	text;				// Num: 원문 숫자 텍스트(예: "5", "0.1", "1E-07"), Str: 문자열
		bool		boolean = false;	// Bool 값
	};

	using TRow = std::vector<SCell>;		// 열 인덱스(0-base) -> 셀. 비어 있는 칸은 Empty
	using TRows = std::map<int, TRow>;		// 행 번호(1-base) -> 행. 비어 있는 행은 들어 있지 않다
}

#endif // ndef UC_XLNTDEFINE_H