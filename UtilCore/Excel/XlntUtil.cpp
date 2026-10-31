
//***************************************************************************
// XlntUtil.cpp: implementation of the CXlntUtil class.
//
//***************************************************************************

#include "pch.h"
#include "XlntUtil.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace Xlnt
{
	namespace
	{
		//***************************************************************************
		// @brief 워크시트 핸들이 시트를 가리키는지 확인합니다.
		// @detail xlnt::worksheet는 내부 구현체 포인터만 가진 핸들이라, 기본 생성되었거나 시트가 제거된 뒤에는
		//         비어 있으며 이때 title() 등을 호출하면 예외가 아니라 널 역참조가 됩니다.
		//***************************************************************************
		void RequireSheet(const xlnt::worksheet& sheet)
		{
			if( sheet == nullptr )
			{
				throw std::runtime_error("활성화된 시트가 없습니다.");
			}
		}

		//***************************************************************************
		// @brief 범위를 병합합니다. 이미 병합된 범위와 일부만 겹치면 예외, 똑같은 범위면 아무것도 하지 않습니다.
		// @detail 겹치게 병합하거나 같은 범위를 중복 병합하면 xlnt는 그대로 저장하고, 엑셀은 그 파일을 "복구"해야 열립니다.
		//***************************************************************************
		void MergeRange(xlnt::worksheet& sheet, const xlnt::range_reference& range)
		{
			RequireSheet(sheet);

			const auto left = range.top_left().column_index();
			const auto right = range.bottom_right().column_index();
			const auto top = range.top_left().row();
			const auto bottom = range.bottom_right().row();

			for( const xlnt::range_reference& merged : sheet.merged_ranges() )
			{
				const bool disjoint = merged.bottom_right().column_index() < left || merged.top_left().column_index() > right ||
					merged.bottom_right().row() < top || merged.top_left().row() > bottom;
				if( disjoint )
				{
					continue;
				}

				if( merged == range )
				{
					return;
				}
				throw std::runtime_error("이미 병합된 범위와 겹칩니다: " + merged.to_string());
			}

			sheet.merge_cells(range);
		}

		//***************************************************************************
		// @brief UTF-8 문자열 값을 셀에 씁니다. 빈 문자열이면 셀의 기존 값을 지웁니다.
		// @detail 빈 값을 무시하고 return 하면 이미 값이 있는 셀을 빈 값으로 덮어쓸 수 없고, 반대로 빈 값을 쓰려고
		//         새 셀을 만들 필요도 없으므로, 셀이 있을 때만 값을 지웁니다.
		//***************************************************************************
		void PutCell(xlnt::worksheet& sheet, const xlnt::cell_reference& ref, const std::string& utf8Value)
		{
			RequireSheet(sheet);

			// 범위를 넘는 셀은 엑셀이 열지 못하는 파일이 되고, 긴 문자열은 xlnt가 조용히 잘라낸다.
			if( ref.row() > static_cast<xlnt::row_t>(XLSX_MAX_ROW) || ref.column_index() > XLSX_MAX_COLUMN )
			{
				throw std::runtime_error("셀 좌표가 xlsx 범위를 벗어났습니다.");
			}
			// xlnt는 UTF-8 바이트 기준 32767을 넘는 문자열을 조용히 자르고, 한글 등은 글자 중간에서 잘려 저장이 실패한다.
			if( utf8Value.size() > XLSX_MAX_CELL_TEXT_BYTES )
			{
				throw std::runtime_error("문자열이 " + std::to_string(XLSX_MAX_CELL_TEXT_BYTES) + "바이트(UTF-8)를 넘습니다 (" + std::to_string(utf8Value.size()) + "바이트).");
			}

			if( utf8Value.empty() )
			{
				if( sheet.has_cell(ref) )
				{
					sheet.cell(ref).clear_value();
				}
				return;
			}

			sheet.cell(ref).value(utf8Value);
		}

		//***************************************************************************
		// @brief 숫자 셀 값을 SCell의 숫자 원문 표기로 바꿉니다.
		// @detail 정수 모양(1e15 미만)은 정수 문자열로, 그 외는 다시 읽었을 때 같은 값이 되는 최단 표기로 만듭니다.
		//         SCell::Num은 '.', 'e', 'E'가 없으면 정수로 취급하므로 이 규칙이면 해석이 달라지지 않습니다.
		//***************************************************************************
		std::string NumberText(double v)
		{
			char buf[64];

			if( v == std::floor(v) && std::fabs(v) < XLSX_EXACT_INTEGER_LIMIT )
			{
				if( v == 0 )
					return "0";
				std::snprintf(buf, sizeof(buf), "%.0f", v);
				return buf;
			}

			// 1e15 이상의 정수 모양 값은 지수 표기로 만든다. 엑셀은 15자리까지만 정확히 저장하므로 긴 자릿수가
			// 정수 문자열처럼 보이면(123456789012345680) 없던 정밀도가 있는 것처럼 보인다.
			const bool integral = (v == std::floor(v));
			const auto r = integral ? std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::scientific)
				: std::to_chars(buf, buf + sizeof(buf), v);
			return std::string(buf, r.ptr);
		}
	}

	//***************************************************************************
	// @brief xlnt 셀 하나를 타입을 보존한 SCell로 바꿉니다.
	// @detail 숫자는 cell.to_string()이 아니라 value<double>()로 읽습니다. xlnt의 숫자 to_string은 1.5e300처럼
	//         큰 값에서 고정 크기 버퍼를 넘겨 쓰레기 문자열을 만듭니다.
	//***************************************************************************
	SCell ToSCell(const xlnt::cell& cell)
	{
		SCell out;

		switch( cell.data_type() )
		{
		case xlnt::cell_type::empty:
			break;

		case xlnt::cell_type::boolean:
			out.kind = SCell::Bool;
			out.boolean = cell.value<bool>();
			break;

		case xlnt::cell_type::number:
			if( cell.is_date() && cell.value<double>() >= 1.0 && cell.value<double>() < XLSX_MAX_DATE_SERIAL )
			{
				// 날짜 서식이 적용된 숫자는 문자열로 읽는다. 엑셀은 "3-5" 같은 입력을 날짜로 바꿔 버리는데,
				// 이를 일련번호(45356)로 조용히 받으면 잘못된 데이터가 그대로 들어간다. 숫자 필드에서는 오류가 된다.
				out.kind = SCell::Str;
				out.text = cell.value<xlnt::datetime>().to_iso_string();	// 일련번호처럼 보이지 않는 ISO 8601 표기
			}
			else if( cell.is_date() && cell.value<double>() >= 0.0 && cell.value<double>() < 1.0 )
			{
				// 시간만 있는 값(0.5 = 12:00)은 날짜 부분이 없으므로 "HH:MM:SS"로 읽는다. (날짜로 바꾸면 1899-12-31T12:00:00 같은 값이 된다)
				const long long seconds = std::llround(cell.value<double>() * 86400.0) % 86400;
				char buf[16];
				std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", seconds / 3600, (seconds / 60) % 60, seconds % 60);
				out.kind = SCell::Str;
				out.text = buf;
			}
			else
			{
				out.kind = SCell::Num;
				out.text = NumberText(cell.value<double>());
			}
			break;

		case xlnt::cell_type::inline_string:
		case xlnt::cell_type::shared_string:
		case xlnt::cell_type::formula_string:
		case xlnt::cell_type::date:
		case xlnt::cell_type::error:
		default:
			out.kind = SCell::Str;
			out.text = cell.to_string();
			break;
		}

		return out;
	}

	//***************************************************************************
	// @brief 셀 값을 문자열로 읽습니다. 숫자 셀은 NumberText 규칙으로 바꿉니다.
	// @detail xlnt의 cell::to_string()은 숫자 셀에서 12345.678을 빈 문자열로, 1.5e300을 잘린 쓰레기 문자열로 돌려줍니다.
	//         숫자는 value<double>()로 읽어 바꾸고, 그 외 타입은 to_string()을 그대로 씁니다.
	//***************************************************************************
	std::string CellText(const xlnt::cell& cell)
	{
		if( cell.data_type() == xlnt::cell_type::number && !cell.is_date() )
		{
			return NumberText(cell.value<double>());
		}
		return cell.to_string();
	}

	//***************************************************************************
	// Construction/Destruction 
	//***************************************************************************

	CXlntUtil::CXlntUtil()
	{
		// 기본 워크북은 시트가 하나 있으므로 처음부터 그 시트를 현재 시트로 둔다. (비워 두면 ActiveSheet 호출 전 쓰기/읽기가 널 역참조)
		_worksheet = _workbook.active_sheet();
	}

	CXlntUtil::~CXlntUtil() {}

	//***************************************************************************
	// @brief 지정한 경로의 엑셀 파일을 엽니다.
	// @detail xlnt 워크북 객체를 사용하여 지정된 파일 경로의 엑셀 파일을 로드합니다.
	// @param filePath 열고자 하는 엑셀 파일 경로
	//***************************************************************************
	void CXlntUtil::OpenExcel(const _tstring& filePath)
	{
		// load()는 기존 시트를 먼저 지운다. 그 뒤 실패하면 이전 워크시트 핸들이 해제된 메모리를 가리키게 되므로 먼저 비운다.
		_worksheet = xlnt::worksheet();

		try
		{
#ifdef _UNICODE
			// 와이드 경로는 xlnt가 그대로 열 수 있으므로 한글 경로도 안전하다. (TStringToString은 ANSI일 수 있어 쓰지 않는다)
			_workbook.load(filePath);
#else
			// xlnt의 char 경로는 UTF-8로 해석된다. 한글 경로가 깨지지 않도록 ANSI에서 변환해 넘긴다.
			_workbook.load(AnsiToUtf8(filePath));
#endif

			// 로드한 워크북의 현재 시트를 따라간다. (이전 워크북의 시트 핸들이 남아 있으면 안 된다)
			_worksheet = _workbook.sheet_count() > 0 ? _workbook.active_sheet() : xlnt::worksheet();
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("파일을 열 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 새로운 시트를 생성하고 이름을 지정한 뒤 활성화합니다.
	// @detail 워크북에 새로운 시트를 추가하고 현재 워크시트로 지정합니다.
	// @param sheetName 추가할 시트 이름
	//***************************************************************************
	void CXlntUtil::AddSheet(const std::string& sheetName)
	{
		try
		{
			// 이름이 잘못됐거나 중복이면 title()이 예외를 던지는데, 그 전에 만든 시트가 남고 현재 시트도 바뀌어 있으면 안 된다.
			// 실패하면 만든 시트를 지우고 현재 시트를 되돌린다.
			const xlnt::worksheet previous = _worksheet;
			xlnt::worksheet created = _workbook.create_sheet();	// 시트 생성
			try
			{
				created.title(sheetName);						// 시트명 변경
			}
			catch( ... )
			{
				_workbook.remove_sheet(created);
				_worksheet = previous;
				throw;
			}
			_worksheet = created;
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("시트를 추가할 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 현재 활성화된 시트의 이름을 변경합니다.
	// @detail 변경하고자 하는 시트 이름이 이미 존재하는지 확인 후 이름을 변경합니다.
	// @param newSheetName 변경할 새 시트 이름
	//***************************************************************************
	void CXlntUtil::RenameSheet(const std::string& newSheetName)
	{
		RequireSheet(_worksheet);

		if( _worksheet.title() == newSheetName )
		{
			return;
		}

		if( _workbook.contains(newSheetName) )
		{
			throw std::runtime_error("시트 이름이 이미 존재합니다: " + newSheetName);
		}

		// 워크북의 active_sheet()가 아니라 ActiveSheet()로 지정한 현재 시트의 이름을 바꾼다.
		_worksheet.title(newSheetName);			// 시트명 변경
	}

	//***************************************************************************
	// @brief 시트 이름으로 해당 시트를 활성화합니다.
	// @detail 지정한 이름의 시트가 존재하는지 확인한 후 현재 워크시트로 지정합니다.
	// @param sheetName 활성화할 시트 이름
	//***************************************************************************
	void CXlntUtil::ActiveSheet(const std::string& sheetName)
	{
		if( !_workbook.contains(sheetName) )
		{
			throw std::runtime_error("시트가 존재하지 않습니다: " + sheetName);
		}
		_worksheet = _workbook.sheet_by_title(sheetName);
	}

	//***************************************************************************
	// @brief 시트 인덱스로 해당 시트를 활성화합니다.
	// @detail 워크북에서 지정한 인덱스의 시트를 활성화하고 현재 워크시트로 지정합니다.
	// @param sheetIndex 활성화할 시트 인덱스
	//***************************************************************************
	void CXlntUtil::ActiveSheet(const uint32 sheetIndex)
	{
		if( sheetIndex >= _workbook.sheet_count() )
		{
			throw std::runtime_error("시트 인덱스가 범위를 벗어났습니다: " + std::to_string(sheetIndex));
		}

		_workbook.active_sheet(sheetIndex);
		_worksheet = _workbook.active_sheet();
	}

	//***************************************************************************
	// @brief 이름으로 시트를 제거합니다.
	// @detail 지정한 이름을 가진 워크시트를 워크북에서 삭제합니다.
	// @param sheetName 제거할 시트 이름
	//***************************************************************************
	void CXlntUtil::RemoveSheet(const std::string& sheetName)
	{
		const std::vector<std::string> titles = GetSheetNames();
		for( uint32 index = 0; index < titles.size(); ++index )
		{
			if( titles[index] == sheetName )
			{
				RemoveSheet(index);
				return;
			}
		}

		throw std::runtime_error("시트를 삭제할 수 없습니다: 시트가 존재하지 않습니다: " + sheetName);
	}

	//***************************************************************************
	// @brief 인덱스로 시트를 제거합니다.
	// @detail 지정한 인덱스의 워크시트를 워크북에서 삭제합니다. 삭제한 시트가 현재 시트였다면
	//         같은 위치(마지막이었으면 그 앞)의 시트로 현재 시트를 옮깁니다.
	//         워크시트는 워크북 내부 구현체를 가리키는 핸들이라, 제거한 시트의 핸들을 그대로 두면
	//         이후 접근이 해제된 메모리를 읽게 됩니다.
	// @param sheetIndex 제거할 시트 인덱스
	//***************************************************************************
	void CXlntUtil::RemoveSheet(const uint32 sheetIndex)
	{
		try
		{
			if( sheetIndex >= _workbook.sheet_count() )
			{
				throw std::runtime_error("시트 인덱스가 범위를 벗어났습니다: " + std::to_string(sheetIndex));
			}

			xlnt::worksheet target = _workbook.sheet_by_index(sheetIndex);
			const bool wasCurrent = (_worksheet == target);

			_workbook.remove_sheet(target);

			if( wasCurrent )
			{
				const std::size_t count = _workbook.sheet_count();
				_worksheet = count > 0 ? _workbook.sheet_by_index(sheetIndex < count ? sheetIndex : count - 1) : xlnt::worksheet();
			}
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("시트를 삭제할 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 워크북에 포함된 모든 시트의 이름 목록을 반환합니다.
	// @detail 워크북의 시트 제목들을 순회하여 문자열 벡터로 수집합니다.
	// @return 시트 이름들의 벡터 (std::vector<std::string>)
	//***************************************************************************
	std::vector<std::string> CXlntUtil::GetSheetNames() const
	{
		std::vector<std::string> sheetNames;
		for( const auto& sheet : _workbook.sheet_titles() )
		{
			sheetNames.push_back(sheet);
		}
		return sheetNames;
	}

	//***************************************************************************
	// @brief 셀 참조 주소(문자열)를 이용해 셀에 문자열 값을 씁니다.
	// @detail 입력된 값의 유효성을 검사하고, 옵션에 따라 인코딩을 UTF-8로 변환하여 셀에 기록합니다.
	// @param cell_ref 셀 참조 주소 (예: "A1")
	// @param value 기록할 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const std::string& cell_ref, const std::string& value, const bool isCastUtf8)
	{
		try
		{
			PutCell(_worksheet, xlnt::cell_reference(cell_ref), isCastUtf8 ? AnsiToUtf8(value) : value);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 행과 열 번호를 이용해 셀에 문자열 값을 씁니다.
	// @detail 지정한 행, 열 좌표의 셀에 값을 기록하며, 옵션에 따라 인코딩을 변환합니다.
	// @param row 행 번호
	// @param col 열 번호
	// @param value 기록할 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const uint32 row, const uint32 col, const std::string& value, const bool isCastUtf8)
	{
		try
		{
			PutCell(_worksheet, xlnt::cell_reference(col, row), isCastUtf8 ? AnsiToUtf8(value) : value);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 셀 참조 주소(문자열)를 이용해 셀에 와이드 문자열 값을 씁니다.
	// @detail 와이드 문자열은 항상 UTF-8로 변환해서 기록합니다. (xlnt 문자열은 UTF-8이라 ANSI로 바꾸면 한글이 깨집니다)
	//         isCastUtf8는 std::string 오버로드와 시그니처를 맞추기 위한 값이며 사용하지 않습니다.
	// @param cell_ref 셀 참조 주소 (예: "A1")
	// @param value 기록할 와이드 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const std::string& cell_ref, const std::wstring& value, const bool isCastUtf8)
	{
		try
		{
			PutCell(_worksheet, xlnt::cell_reference(cell_ref), UnicodeToUtf8(value));
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 행과 열 번호를 이용해 셀에 와이드 문자열 값을 씁니다.
	// @detail 와이드 문자열은 항상 UTF-8로 변환해서 기록합니다. isCastUtf8는 사용하지 않습니다.
	// @param row 행 번호
	// @param col 열 번호
	// @param value 기록할 와이드 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const uint32 row, const uint32 col, const std::wstring& value, const bool isCastUtf8)
	{
		try
		{
			PutCell(_worksheet, xlnt::cell_reference(col, row), UnicodeToUtf8(value));
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 셀 참조 주소(문자열)를 이용해 셀의 값을 읽어옵니다.
	// @detail 지정한 셀의 데이터를 문자열 형태로 변환하여 반환합니다.
	// @param cell_ref 셀 참조 주소 (예: "A1")
	// @return 읽어온 셀의 문자열 값
	//***************************************************************************
	std::string CXlntUtil::ReadCell(const std::string& cell_ref)
	{
		try
		{
			RequireSheet(_worksheet);

			// worksheet::cell()은 없는 셀을 만들어 사용 범위(highest_row/column)를 늘리므로, 있는 셀만 읽는다.
			const xlnt::cell_reference ref(cell_ref);
			return _worksheet.has_cell(ref) ? CellText(_worksheet.cell(ref)) : std::string();
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀의 값을 읽을 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 행과 열 번호를 이용해 셀의 값을 읽어옵니다.
	// @detail 지정한 행, 열 좌표의 셀 데이터를 문자열 형태로 변환하여 반환합니다.
	// @param row 행 번호
	// @param col 열 번호
	// @return 읽어온 셀의 문자열 값
	//***************************************************************************
	std::string CXlntUtil::ReadCell(const uint32 row, const uint32 col)
	{
		try
		{
			RequireSheet(_worksheet);

			const xlnt::cell_reference ref(col, row);
			return _worksheet.has_cell(ref) ? CellText(_worksheet.cell(ref)) : std::string();
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀의 값을 읽을 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 두 셀 참조 주소를 기준으로 범위를 병합합니다.
	// @detail 시작 셀과 끝 셀 주소를 조합하여 엑셀 셀 병합을 수행합니다.
	// @param cell1 시작 셀 참조 주소
	// @param cell2 끝 셀 참조 주소
	//***************************************************************************
	void CXlntUtil::MergeCells(const std::string& cell1, const std::string& cell2)
	{
		RequireSheet(_worksheet);

		MergeRange(_worksheet, xlnt::range_reference(cell1 + ":" + cell2));	// A1:B2 형태로 병합
	}

	//***************************************************************************
	// @brief 시작 및 끝 행/열 번호를 기준으로 범위를 병합합니다.
	// @detail 행과 열 좌표를 기반으로 셀 범위를 지정한 후 병합을 수행합니다.
	// @param start_row 시작 행 번호
	// @param start_col 시작 열 번호
	// @param end_row 끝 행 번호
	// @param end_col 끝 열 번호
	//***************************************************************************
	void CXlntUtil::MergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		RequireSheet(_worksheet);

		xlnt::cell_reference start_cell(start_col, start_row);  // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);        // 끝 셀
		xlnt::range_reference range(start_cell, end_cell);      // 범위 지정

		MergeRange(_worksheet, range);							// 병합 수행
	}

	//***************************************************************************
	// @brief 두 셀 참조 주소를 기준으로 병합된 셀 범위를 해제합니다.
	// @detail 지정한 셀 범위의 병합 상태를 해제합니다.
	// @param cell1 시작 셀 참조 주소
	// @param cell2 끝 셀 참조 주소
	//***************************************************************************
	void CXlntUtil::UnMergeCells(const std::string& cell1, const std::string& cell2)
	{
		RequireSheet(_worksheet);

		_worksheet.unmerge_cells(cell1 + ":" + cell2);  // A1:B2 형태로 병합 해제
	}

	//***************************************************************************
	// @brief 시작 및 끝 행/열 번호를 기준으로 병합된 셀 범위를 해제합니다.
	// @detail 행과 열 좌표 기반으로 범위를 생성하여 병합을 해제합니다.
	// @param start_row 시작 행 번호
	// @param start_col 시작 열 번호
	// @param end_row 끝 행 번호
	// @param end_col 끝 열 번호
	//***************************************************************************
	void CXlntUtil::UnMergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		RequireSheet(_worksheet);

		xlnt::cell_reference start_cell(start_col, start_row);  // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);        // 끝 셀
		xlnt::range_reference range(start_cell, end_cell);      // 범위 지정
		_worksheet.unmerge_cells(range);                        // 병합 해제
	}

	//***************************************************************************
	// @brief 지정한 위치에 행을 삽입합니다.
	// @detail 특정 행 번호 위치에 지정된 개수만큼 행을 삽입합니다.
	// @param row_num 삽입할 행 번호
	// @param amount 삽입할 행 개수 (기본값: 1)
	//***************************************************************************
	void CXlntUtil::InsertRow(const uint32 row_num, const uint32 amount)
	{
		RequireSheet(_worksheet);

		_worksheet.insert_rows(row_num, amount);		// 지정된 위치에 amount개의 행 삽입
	}

	//***************************************************************************
	// @brief 지정한 위치에 열을 삽입합니다.
	// @detail 특정 열 번호 위치에 지정된 개수만큼 열을 삽입합니다.
	// @param col_num 삽입할 열 번호
	// @param amount 삽입할 열 개수 (기본값: 1)
	//***************************************************************************
	void CXlntUtil::InsertColumn(const uint32 col_num, const uint32 amount)
	{
		RequireSheet(_worksheet);

		_worksheet.insert_columns(col_num, amount);		// 지정된 위치에 amount개의 열 삽입
	}

	//***************************************************************************
	// @brief 지정한 위치의 행을 삭제합니다.
	// @detail 특정 행 번호 위치부터 지정된 개수만큼 행을 삭제합니다.
	// @param row_num 삭제할 행 번호
	// @param amount 삭제할 행 개수 (기본값: 1)
	//***************************************************************************
	void CXlntUtil::DeleteRow(const uint32 row_num, const uint32 amount)
	{
		RequireSheet(_worksheet);

		_worksheet.delete_rows(row_num, amount);		// 지정된 위치에 amount개의 행 삭제
	}

	//***************************************************************************
	// @brief 지정한 위치의 열을 삭제합니다.
	// @detail 특정 열 번호 위치부터 지정된 개수만큼 열을 삭제합니다.
	// @param col_num 삭제할 열 번호
	// @param amount 삭제할 열 개수 (기본값: 1)
	//***************************************************************************
	void CXlntUtil::DeleteColumn(const uint32 col_num, const uint32 amount)
	{
		RequireSheet(_worksheet);

		_worksheet.delete_columns(col_num, amount);		// 지정된 위치에 amount개의 열 삭제
	}

	//***************************************************************************
	// @brief 시트들의 순서를 재배치합니다.
	// @detail 새로운 시트 이름 순서 벡터에 맞춰 시트들을 재구성합니다.
	// @param new_order 새로운 순서의 시트 이름 벡터
	//***************************************************************************
	void CXlntUtil::ReOrderSheets(const std::vector<std::string>& new_order)
	{
		// 먼저 전부 검증한다. 중간에 실패해서 시트 순서가 반쯤 바뀐 채로 남지 않게 한다.
		std::vector<std::string> titles = GetSheetNames();
		for( std::size_t i = 0; i < new_order.size(); ++i )
		{
			if( std::find(titles.begin(), titles.end(), new_order[i]) == titles.end() )
			{
				throw std::runtime_error("시트가 존재하지 않습니다: " + new_order[i]);
			}
			if( std::find(new_order.begin(), new_order.begin() + i, new_order[i]) != new_order.begin() + i )
			{
				throw std::runtime_error("시트 이름이 중복되었습니다: " + new_order[i]);
			}
		}

		// xlnt에는 시트 이동 API가 없으므로, 원하는 위치에 복사본을 만들고 원본을 지운 뒤 이름을 되돌린다.
		// (시트를 지우고 이름만 같은 빈 시트를 만들면 셀 데이터가 모두 사라진다.)
		// new_order에 없는 시트는 지정한 시트들 뒤에 기존 순서대로 남는다.
		for( std::size_t pos = 0; pos < new_order.size(); ++pos )
		{
			titles = GetSheetNames();
			if( titles[pos] == new_order[pos] )
			{
				continue;
			}

			xlnt::worksheet original = _workbook.sheet_by_title(new_order[pos]);
			const bool wasCurrent = (_worksheet == original);

			xlnt::worksheet moved = _workbook.copy_sheet(original, pos);
			_workbook.remove_sheet(original);
			moved.title(new_order[pos]);

			if( wasCurrent )
			{
				_worksheet = moved;
			}
		}
	}

	//***************************************************************************
	// @brief 시작과 끝 셀 주소를 이용해 xlnt 범위를 생성합니다.
	// @detail 문자열 형태의 셀 범위를 받아 xlnt::range 객체로 반환합니다.
	// @param start_cell 시작 셀 주소
	// @param end_cell 끝 셀 주소
	// @return 생성된 xlnt::range 객체
	//***************************************************************************
	xlnt::range CXlntUtil::CreateRange(const std::string& start_cell, const std::string& end_cell)
	{
		RequireSheet(_worksheet);

		xlnt::range rng = _worksheet.range(start_cell + ":" + end_cell);
		return rng;
	}

	//***************************************************************************
	// @brief 행과 열 번호를 이용해 xlnt 범위를 생성합니다.
	// @detail 시작과 끝의 행/열 좌표를 이용해 xlnt::range 객체를 생성하여 반환합니다.
	// @param start_row 시작 행 번호
	// @param start_col 시작 열 번호
	// @param end_row 끝 행 번호
	// @param end_col 끝 열 번호
	// @return 생성된 xlnt::range 객체
	//***************************************************************************
	xlnt::range CXlntUtil::CreateRange(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		RequireSheet(_worksheet);

		xlnt::cell_reference start_cell(start_col, start_row); // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);       // 끝 셀
		xlnt::range_reference range_ref(start_cell, end_cell); // 범위 생성

		return _worksheet.range(range_ref);                    // 범위 반환
	}

	//***************************************************************************
	// @brief 지정한 범위 내의 모든 셀에 값을 일괄 기록합니다.
	// @detail 주어진 범위(xlnt::range)를 순회하며 각 셀에 동일한 값을 설정합니다.
	// @param range 대상 xlnt::range 객체
	// @param value 기록할 값
	//***************************************************************************
	void CXlntUtil::WriteToRange(xlnt::range range, const std::string& value)
	{
		for( auto row : range )
		{
			for( auto cell : row )
			{
				cell.value(value); // 범위 내 모든 셀에 값 설정
			}
		}
	}

	//***************************************************************************
	// @brief 현재 워크시트의 전체 행 수와 열 수를 가져옵니다.
	// @detail 시트에서 데이터가 존재하는 최고 행 번호와 최고 열 인덱스를 조회합니다.
	// @param rowCount [out] 조회된 행 개수
	// @param columnCount [out] 조회된 열 개수
	//***************************************************************************
	void CXlntUtil::GetRowColumnCount(uint32& rowCount, uint32& columnCount)
	{
		RequireSheet(_worksheet);

		// xlnt는 비어 있는 시트에도 highest_row/column을 1로 돌려주므로, 셀이 하나도 없으면 0으로 알린다.
		bool hasCell = false;
		for( auto row : _worksheet.rows(true) )
		{
			for( auto cell : row )
			{
				static_cast<void>(cell);
				hasCell = true;
				break;
			}
			if( hasCell )
			{
				break;
			}
		}

		rowCount = hasCell ? _worksheet.highest_row() : 0;						// 마지막 행 번호
		columnCount = hasCell ? _worksheet.highest_column().index : 0;			// 마지막 열 번호
	}

	//***************************************************************************
	// @brief 특정 행의 높이를 설정합니다.
	// @detail 지정한 행 번호의 행 속성에 접근하여 높이 값을 설정합니다.
	// @param row 대상 행 번호
	// @param height 설정할 높이 값
	//***************************************************************************
	void CXlntUtil::SetRowHeight(const uint32 row, const double height)
	{
		RequireSheet(_worksheet);

		_worksheet.row_properties(row).height = height;
	}

	//***************************************************************************
	// @brief 특정 열의 너비를 설정합니다.
	// @detail 지정한 열 번호의 열 속성에 접근하여 너비 값을 설정합니다.
	// @param col 대상 열 번호
	// @param width 설정할 너비 값
	//***************************************************************************
	void CXlntUtil::SetColumnWidth(const uint32 col, const double width)
	{
		RequireSheet(_worksheet);

		_worksheet.column_properties(col).width = width;
	}

	//***************************************************************************
	// @brief 특정 셀에 텍스트 형식(서식)을 설정합니다.
	// @detail 지정한 위치의 셀에 텍스트 서식("@")을 적용합니다.
	// @param row 행 번호
	// @param col 열 번호
	//***************************************************************************
	void CXlntUtil::SetCellTextFormat(const uint32 row, const uint32 col)
	{
		RequireSheet(_worksheet);

		_worksheet.cell(col, row).number_format(xlnt::number_format("@"));	// 텍스트 서식("@") 적용
	}

	//***************************************************************************
	// @brief 특정 열의 모든 행 셀에 텍스트 형식(서식)을 설정합니다.
	// @detail 1행부터 최고 행까지 순회하며 해당 열의 모든 셀에 텍스트 서식("@")을 적용합니다.
	// @param col 열 번호
	//***************************************************************************
	void CXlntUtil::SetAllCellTextFormat(const uint32 col)
	{
		RequireSheet(_worksheet);

		const xlnt::number_format textFormat("@");			// 텍스트 서식. 셀마다 만들 필요가 없다
		const uint32 lastRow = _worksheet.highest_row();	// 루프 중 사용 범위가 바뀌지 않도록 한 번만 읽는다

		for( uint32 row = 1; row <= lastRow; ++row )
		{
			_worksheet.cell(col, row).number_format(textFormat);
		}
	}

	//***************************************************************************
	// @brief 현재 워크북을 지정한 파일 경로에 저장합니다.
	// @detail xlnt 워크북을 이용해 현재 데이터를 파일로 저장합니다.
	// @param filePath 저장할 파일 경로
	//***************************************************************************
	void CXlntUtil::SaveAs(const _tstring& filePath)
	{
		try
		{
#ifdef _UNICODE
			// OpenExcel과 같은 이유로 와이드 경로를 그대로 넘긴다. (TStringToString은 ANSI일 수 있어 한글 경로가 깨진다)
			_workbook.save(filePath);
#else
			// xlnt의 char 경로는 UTF-8로 해석된다. 한글 경로가 깨지지 않도록 ANSI에서 변환해 넘긴다.
			_workbook.save(AnsiToUtf8(filePath));
#endif
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("파일을 저장할 수 없습니다: " + std::string(e.what()));
		}
	}
}