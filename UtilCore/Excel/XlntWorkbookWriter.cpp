
//***************************************************************************
// XlntWorkbookWriter.cpp: implementation of the CXlntWorkbookWriter class.
//
//***************************************************************************

#include "pch.h"
#include "XlntWorkbookWriter.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace Xlnt
{
	namespace
	{
		enum class TextError { None, BadUtf8, BadChar };

		//***************************************************************************
		// @brief UTF-8 문자열이 xlsx(XML 1.0)에 그대로 쓸 수 있는 텍스트인지 검사하고 UTF-16 길이를 셉니다.
		// @detail 잘못된 UTF-8(오버롱, 서로게이트, 잘린 시퀀스)과 XML에 쓸 수 없는 문자(탭/개행/CR을 뺀 제어 문자, U+FFFE/FFFF)를
		//         거릅니다. xlnt는 제어 문자를 쓰다가 예외를 던지고 잘못된 UTF-8은 Save에서야 실패하므로, 시트를 만들기 전에 미리 검사합니다.
		// @param utf16Len [out] UTF-16 코드 유닛 수 (엑셀의 시트 이름 31자/셀 32767자 제한 기준)
		// @param badCp    [out] BadChar일 때 해당 코드 포인트
		//***************************************************************************
		TextError CheckText(const std::string& s, size_t& utf16Len, char32_t& badCp)
		{
			utf16Len = 0;
			size_t i = 0;
			const size_t n = s.size();

			while( i < n )
			{
				const unsigned char c = static_cast<unsigned char>(s[i]);
				char32_t cp = 0;
				size_t len = 0;

				if( c < 0x80 ) { len = 1; cp = c; }
				else if( c >= 0xC2 && c <= 0xDF ) { len = 2; cp = c & 0x1F; }
				else if( c >= 0xE0 && c <= 0xEF ) { len = 3; cp = c & 0x0F; }
				else if( c >= 0xF0 && c <= 0xF4 ) { len = 4; cp = c & 0x07; }
				else								return TextError::BadUtf8;

				if( i + len > n )
					return TextError::BadUtf8;

				for( size_t k = 1; k < len; ++k )
				{
					const unsigned char t = static_cast<unsigned char>(s[i + k]);
					if( (t & 0xC0) != 0x80 )
						return TextError::BadUtf8;
					cp = (cp << 6) | (t & 0x3F);
				}

				if( (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF) )
					return TextError::BadUtf8;

				if( (cp < 0x20 && cp != 0x09 && cp != 0x0A && cp != 0x0D) || cp == 0xFFFE || cp == 0xFFFF )
				{
					badCp = cp;
					return TextError::BadChar;
				}

				utf16Len += (cp >= 0x10000) ? 2 : 1;
				i += len;
			}

			return TextError::None;
		}

		//***************************************************************************
		// @brief 셀 주소("B3")를 만듭니다. (오류 메시지용)
		//***************************************************************************
		std::string CellAddress(size_t col0, int row)
		{
			std::string name;
			for( size_t c = col0 + 1; c > 0; c = (c - 1) / 26 )
				name.insert(name.begin(), static_cast<char>('A' + (c - 1) % 26));
			return name + std::to_string(row);
		}

		//***************************************************************************
		// @brief 셀 문자열을 검사합니다. 쓸 수 없으면 셀 주소와 사유를 담아 던집니다.
		//***************************************************************************
		void ValidateCellText(const std::string& text, size_t col0, int row)
		{
			size_t len = 0;
			char32_t bad = 0;
			const std::string where = "셀 " + CellAddress(col0, row) + ": ";

			switch( CheckText(text, len, bad) )
			{
			case TextError::BadUtf8:
				throw std::runtime_error(where + "UTF-8이 아닌 문자열입니다");
			case TextError::BadChar:
			{
				char buf[16];
				std::snprintf(buf, sizeof(buf), "U+%04X", static_cast<unsigned>(bad));
				throw std::runtime_error(where + "xlsx에 쓸 수 없는 제어 문자가 있습니다 (" + buf + ")");
			}
			case TextError::None:
				break;
			}

			// xlnt는 UTF-8 바이트 기준 32767을 넘으면 조용히 자르고, 한글 등은 글자 중간에서 잘려 Save가 실패한다.
			if( text.size() > XLSX_MAX_CELL_TEXT_BYTES )
				throw std::runtime_error(where + "문자열이 " + std::to_string(XLSX_MAX_CELL_TEXT_BYTES) + "바이트(UTF-8)를 넘습니다 (" + std::to_string(text.size()) + "바이트)");
		}

		//***************************************************************************
		// @brief ASCII 대소문자를 무시한 소문자 사본 (시트 이름 중복 비교용)
		// @detail 엑셀은 이름 비교에서 대소문자를 구분하지 않는다. 비ASCII 문자는 그대로 둔다.
		//***************************************************************************
		std::string Lower(std::string s)
		{
			for( char& ch : s )
				ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
			return s;
		}

		//***************************************************************************
		// @brief 시트 이름이 엑셀 규칙에 맞는지 검사합니다. 맞지 않으면 사유를 던집니다.
		//***************************************************************************
		void ValidateSheetName(const std::string& name)
		{
			if( name.empty() )
				throw std::runtime_error("시트 이름이 비어 있습니다");

			size_t len = 0;
			char32_t bad = 0;
			if( CheckText(name, len, bad) != TextError::None )
				throw std::runtime_error("시트 이름에 쓸 수 없는 문자가 있습니다");
			if( len > XLSX_MAX_SHEET_NAME_LEN )
				throw std::runtime_error("시트 이름이 31자를 넘습니다: " + name);
			if( name.find_first_of(XLSX_INVALID_SHEET_CHARS) != std::string::npos )
				throw std::runtime_error("시트 이름에 쓸 수 없는 문자가 있습니다: " + name);
			if( name.front() == '\'' || name.back() == '\'' )
				throw std::runtime_error("시트 이름은 작은따옴표로 시작하거나 끝날 수 없습니다: " + name);
		}

		//***************************************************************************
		// @brief SCell::Num 원문을 숫자로 해석합니다.
		// @detail '.', 'e', 'E'가 없으면 정수로 취급해 int64로 해석하고(범위를 넘으면 double), 그 외는 double입니다.
		//         nan/inf는 xlsx에 표현할 수 없어 실패입니다. 원문 전체가 숫자여야 합니다.
		// @param isInt [out] true면 iv, false면 dv에 값이 있습니다
		// @return 숫자로 해석했으면 true
		//***************************************************************************
		bool ParseNumber(const std::string& text, bool& isInt, long long& iv, double& dv)
		{
			const char* first = text.data();
			const char* last = first + text.size();

			if( text.find_first_of(".eE") == std::string::npos )
			{
				const auto r = std::from_chars(first, last, iv);
				if( r.ec == std::errc() && r.ptr == last )
				{
					isInt = true;
					return true;
				}
				// int64 범위를 넘는 정수 모양은 아래 double 경로로 간다.
			}

			const auto r = std::from_chars(first, last, dv);
			isInt = false;
			return r.ec == std::errc() && r.ptr == last && std::isfinite(dv);
		}
	}

	//***************************************************************************
	// @brief 빈 통합 문서를 만듭니다.
	// @detail CXlntUtil이 만드는 기본 시트("Sheet1")는 첫 AddSheet에서 그 이름으로 바꿔 씁니다.
	//***************************************************************************
	CXlntWorkbookWriter::CXlntWorkbookWriter()
	{
	}

	//***************************************************************************
	// @brief 시트를 추가하고 셀 값을 씁니다.
	// @param name 시트 이름 (UTF-8)
	// @param rows 행 번호 -> 행
	// @exception std::runtime_error 이름/값/범위 오류
	//***************************************************************************
	void CXlntWorkbookWriter::AddSheet(const std::string& name, const TRows& rows)
	{
		ValidateSheetName(name);

		const std::string key = Lower(name);
		for( const std::string& n : _sheetNames )
		{
			if( Lower(n) == key )
				throw std::runtime_error("시트 이름이 중복됩니다: " + name);
		}

		// 쓰기 전에 값을 모두 검증한다. 도중에 실패해도 통합 문서에는 반쯤 쓴 시트가 남지 않는다.
		for( const auto& [rowNo, row] : rows )
		{
			if( rowNo < 1 || rowNo > XLSX_MAX_ROW )
				throw std::runtime_error("행 번호가 범위를 벗어났습니다: " + std::to_string(rowNo));
			if( row.size() > XLSX_MAX_COLUMN )
				throw std::runtime_error("열 수가 범위를 벗어났습니다 (행 " + std::to_string(rowNo) + ")");

			for( size_t col = 0; col < row.size(); ++col )
			{
				const SCell& c = row[col];

				if( c.kind == SCell::Num )
				{
					bool isInt; long long iv; double dv;
					if( !ParseNumber(c.text, isInt, iv, dv) )
						throw std::runtime_error("셀 " + CellAddress(col, rowNo) + ": 숫자 셀 값이 숫자가 아닙니다: '" + c.text + "'");
				}
				else if( c.kind == SCell::Str && !c.text.empty() )
				{
					ValidateCellText(c.text, col, rowNo);
				}
			}
		}

		if( _placeholder )
		{
			_excel.RenameSheet(name);
			_placeholder = false;
		}
		else
		{
			_excel.AddSheet(name);
			_excel.ActiveSheet(name);
		}
		_sheetNames.push_back(name);

		xlnt::worksheet& ws = _excel.GetWorkSheet();

		for( const auto& [rowNo, row] : rows )
		{
			for( size_t col = 0; col < row.size(); ++col )
			{
				const SCell& c = row[col];
				if( c.kind == SCell::Empty || (c.kind == SCell::Str && c.text.empty()) )
					continue;

				xlnt::cell cell = ws.cell(static_cast<xlnt::column_t::index_t>(col + 1), static_cast<xlnt::row_t>(rowNo));

				switch( c.kind )
				{
				case SCell::Num:
				{
					bool isInt; long long iv; double dv;
					ParseNumber(c.text, isInt, iv, dv);		// 위에서 검증을 마쳤다
					if( isInt )
						cell.value(iv);
					else
						cell.value(dv);
					break;
				}
				case SCell::Bool:
					cell.value(c.boolean);
					break;
				case SCell::Str:
					cell.value(std::string(c.text));	// std::string 오버로드: '='로 시작해도 수식으로 해석하지 않는다
					break;
				default:
					break;
				}
			}
		}
	}

	//***************************************************************************
	// @brief 파일로 저장합니다. 임시 파일에 먼저 쓰고 교체합니다.
	// @param filePath 저장할 경로
	// @exception std::runtime_error 시트가 없거나 저장 실패
	//***************************************************************************
	void CXlntWorkbookWriter::Save(const _tstring& filePath)
	{
		if( _sheetNames.empty() )
			throw std::runtime_error("저장할 시트가 없습니다");

		namespace fs = std::filesystem;
		const fs::path target(filePath);
		const _tstring tmpPath = filePath + _T(".tmp");
		const fs::path tmp(tmpPath);

		try
		{
			_excel.SaveAs(tmpPath);
		}
		catch( ... )
		{
			std::error_code ignore;
			fs::remove(tmp, ignore);
			throw;
		}

		std::error_code ec;
		fs::rename(tmp, target, ec);			// Windows에서도 기존 파일을 덮어쓴다
		if( ec )
		{
			std::error_code ignore;
			fs::remove(tmp, ignore);
			throw std::runtime_error("파일을 저장할 수 없습니다: " + ec.message());
		}
	}
}