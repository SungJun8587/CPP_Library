
//***************************************************************************
// XlntUtil.cpp: implementation of the CXlntUtil class.
//
//***************************************************************************

#include "pch.h"
#include "XlntUtil.h"

namespace Xlnt
{
	//***************************************************************************
	// Construction/Destruction 
	//***************************************************************************

	CXlntUtil::CXlntUtil() {}

	CXlntUtil::~CXlntUtil() {}

	//***************************************************************************
	//
	void CXlntUtil::OpenExcel(const _tstring& filePath)
	{
		try
		{
			_workbook.load(filePath);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("파일을 열 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 시트 추가
	void CXlntUtil::AddSheet(const std::string& sheetName)
	{
		try
		{
			_worksheet = _workbook.create_sheet();	// 시트 생성
			_worksheet.title(sheetName);			// 시트명 변경
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("시트를 추가할 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 시트 제거
	void CXlntUtil::RemoveSheet(const std::string& sheetName)
	{
		try
		{
			_workbook.remove_sheet(_workbook.sheet_by_title(sheetName));
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("시트를 삭제할 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 인덱스에 해당하는 시트 활성화
	void CXlntUtil::ActiveSheet(const uint32 sheetIndex)
	{
		_workbook.active_sheet(sheetIndex);
	}

	//***************************************************************************
	// 시트명 변경
	void CXlntUtil::RenameSheet(const std::string& sheetName)
	{
		_worksheet = _workbook.active_sheet();
		_worksheet.title(sheetName);			// 시트명 변경
	}
	
	//***************************************************************************
	// 셀 데이터 쓰기
	void CXlntUtil::WriteCell(const std::string& cell_ref, const std::string& value)
	{
		try
		{
			_worksheet.cell(cell_ref).value(value);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	//
	void CXlntUtil::WriteCell(const uint32 row, const uint32 col, const std::string& value)
	{
		try
		{
			_worksheet.cell(col, row).value(value);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 셀 데이터 쓰기
	void CXlntUtil::WriteCell(const std::string& cell_ref, const std::wstring& value)
	{
		try
		{
			Iconv::CIconvUtil iconvUtil("WCHAR_T", "UTF-8");
			_worksheet.cell(cell_ref).value(iconvUtil.WCharToUtf8(value));
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	//
	void CXlntUtil::WriteCell(const uint32 row, const uint32 col, const std::wstring& value)
	{
		try
		{
			Iconv::CIconvUtil iconvUtil("WCHAR_T", "UTF-8");
			_worksheet.cell(col, row).value(iconvUtil.WCharToUtf8(value));
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 셀 데이터 읽기
	std::string CXlntUtil::ReadCell(const std::string& cell_ref)
	{
		try
		{
			return _worksheet.cell(cell_ref).to_string();
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀의 값을 읽을 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	//
	std::string CXlntUtil::ReadCell(const uint32 row, const uint32 col)
	{
		try
		{
			return _worksheet.cell(row, col).to_string();
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀의 값을 읽을 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// 셀 병합
	void CXlntUtil::MergeCells(const std::string& cell1, const std::string& cell2)
	{
		_worksheet.merge_cells(cell1 + ":" + cell2);  // A1:B2 형태로 병합
	}

	//***************************************************************************
	//
	void CXlntUtil::MergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		xlnt::cell_reference start_cell(start_col, start_row);  // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);        // 끝 셀
		xlnt::range_reference range(start_cell, end_cell);      // 범위 지정

		_worksheet.merge_cells(range);							// 병합 수행
	}

	//***************************************************************************
	// 셀 병합 해제
	void CXlntUtil::UnMergeCells(const std::string& cell1, const std::string& cell2)
	{
		_worksheet.unmerge_cells(cell1 + ":" + cell2);  // A1:B2 형태로 병합 해제
	}

	//***************************************************************************
	//
	void CXlntUtil::UnMergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		xlnt::cell_reference start_cell(start_col, start_row);  // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);        // 끝 셀
		xlnt::range_reference range(start_cell, end_cell);      // 범위 지정
		_worksheet.unmerge_cells(range);                        // 병합 해제
	}

	//***************************************************************************
	// 행 삽입
	void CXlntUtil::InsertRow(const uint32 row_num, const uint32 amount)
	{
		_worksheet.insert_rows(row_num, amount);		// 지정된 위치에 amount개의 행 삽입
	}

	//***************************************************************************
	// 열 삽입
	void CXlntUtil::InsertColumn(const uint32 col_num, const uint32 amount)
	{
		_worksheet.insert_columns(col_num, amount);		// 지정된 위치에 amount개의 열 삽입
	}

	//***************************************************************************
	// 행 삭제
	void CXlntUtil::DeleteRow(const uint32 row_num, const uint32 amount)
	{
		_worksheet.delete_rows(row_num, amount);		// 지정된 위치에 amount개의 행 삭제
	}

	//***************************************************************************
	// 열 삭제
	void CXlntUtil::DeleteColumn(const uint32 col_num, const uint32 amount)
	{
		_worksheet.delete_columns(col_num, amount);		// 지정된 위치에 amount개의 열 삭제
	}

	//***************************************************************************
	// 시트 순서 변경
	void CXlntUtil::ReOrderSheets(const std::vector<std::string>& new_order)
	{
		std::vector<xlnt::worksheet> sheets;

		for( const auto& sheet_title : new_order )
		{
			auto sheet = _workbook.sheet_by_title(sheet_title);
			sheets.push_back(sheet);
		}

		// 기존 시트들 삭제
		for( int i = static_cast<int>(_workbook.sheet_count()) - 1; i >= 0; --i )
		{
			_workbook.remove_sheet(_workbook.sheet_by_index(i));  // 첫 번째 시트부터 삭제
		}

		for( const auto& sheet : sheets )
		{
			auto createSheet = _workbook.create_sheet();  // 새 순서대로 시트 추가
			createSheet.title(sheet.title());
		}
	}

	//***************************************************************************
	//
	xlnt::range CXlntUtil::CreateRange(const std::string& start_cell, const std::string& end_cell)
	{
		xlnt::range rng = _worksheet.range(start_cell + ":" + end_cell);
		return rng;
	}

	//***************************************************************************
	//
	xlnt::range CXlntUtil::CreateRange(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col)
	{
		xlnt::cell_reference start_cell(start_col, start_row); // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);       // 끝 셀
		xlnt::range_reference range_ref(start_cell, end_cell); // 범위 생성

		return _worksheet.range(range_ref);                        // 범위 반환
	}

	//***************************************************************************
	// Range에 데이터 쓰기
	void WriteToRange(xlnt::range range, const std::string& value)
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
	// 행 높이 설정
	void CXlntUtil::SetRowHeight(const uint32 row, const double height)
	{
		_worksheet.row_properties(row).height = height;
	}

	//***************************************************************************
	// 열 너비 설정
	void CXlntUtil::SetColumnWidth(const uint32 col, const double width)
	{
		_worksheet.column_properties(col).width = width;
	}

	//***************************************************************************
	// 셀에 텍스트 형식 설정
	void CXlntUtil::SetCellTextFormat(const uint32 row, const uint32 col)
	{
		xlnt::cell cell = _worksheet.cell(col, row);		// 특정 열의 셀 가져오기
		xlnt::number_format text_format;
		auto formatted = text_format.format("@");			// 텍스트 형식 설정
		cell.number_format(formatted);						// 셀에 적용
	}

	//***************************************************************************
	// 모든 행에 특정 셀에 텍스트 형식 설정
	void CXlntUtil::SetAllCellTextFormat(const uint32 col)
	{
		for( uint32 row = 1; row <= _worksheet.highest_row(); ++row )
		{
			auto cell = _worksheet.cell(col, row);			// 특정 열의 셀 가져오기
			xlnt::number_format text_format;
			auto formatted = text_format.format("@");		// 텍스트 형식 설정
			cell.number_format(formatted);					// 셀에 적용
		}
	}

	//***************************************************************************
	//
	void CXlntUtil::SaveAs(const _tstring& filePath)
	{
		try
		{
			_workbook.save(filePath);
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("파일을 저장할 수 없습니다: " + std::string(e.what()));
		}
	}
}