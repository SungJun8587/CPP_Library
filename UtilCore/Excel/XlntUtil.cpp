
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
	// @brief 지정한 경로의 엑셀 파일을 엽니다.
	// @detail xlnt 워크북 객체를 사용하여 지정된 파일 경로의 엑셀 파일을 로드합니다.
	// @param filePath 열고자 하는 엑셀 파일 경로
	//***************************************************************************
	void CXlntUtil::OpenExcel(const _tstring& filePath)
	{
		try
		{
			// file_path가 std::filesystem::path 혹은 string 계열일 때 TStringToString 활용 가능
#ifdef _UNICODE
			_workbook.load(TStringToString(filePath));
#else
			_workbook.load(filePath);
#endif
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
			_worksheet = _workbook.create_sheet();	// 시트 생성
			_worksheet.title(sheetName);			// 시트명 변경
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
		if( _workbook.contains(newSheetName) )
		{
			throw std::runtime_error("시트 이름이 이미 존재합니다: " + newSheetName);
		}

		_worksheet = _workbook.active_sheet();
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
	// @brief 인덱스로 시트를 제거합니다.
	// @detail 지정한 인덱스의 워크시트를 활성화한 후 워크북에서 삭제합니다.
	// @param sheetIndex 제거할 시트 인덱스
	//***************************************************************************
	void CXlntUtil::RemoveSheet(const uint32 sheetIndex)
	{
		try
		{
			_workbook.active_sheet(sheetIndex);
			_workbook.remove_sheet(_workbook.active_sheet());
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
		if( value.size() < 1 ) return;

		try
		{
			if( isCastUtf8 )
			{
				// AnsiToUtf8 활용
				_worksheet.cell(cell_ref).value(AnsiToUtf8(value));
			}
			else
			{
				_worksheet.cell(cell_ref).value(value);
			}
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
		if( value.size() < 1 ) return;

		try
		{
			if( isCastUtf8 )
			{
				_worksheet.cell(col, row).value(AnsiToUtf8(value));
			}
			else
			{
				_worksheet.cell(col, row).value(value);
			}
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 셀 참조 주소(문자열)를 이용해 셀에 와이드 문자열 값을 씁니다.
	// @detail Wstring 형태의 값을 지정된 인코딩 규칙에 맞게 변환하여 셀에 기록합니다.
	// @param cell_ref 셀 참조 주소 (예: "A1")
	// @param value 기록할 와이드 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const std::string& cell_ref, const std::wstring& value, const bool isCastUtf8)
	{
		if( value.size() < 1 ) return;

		try
		{
			if( isCastUtf8 )
			{
				// UnicodeToUtf8 활용
				_worksheet.cell(cell_ref).value(UnicodeToUtf8(value));
			}
			else
			{
				// UnicodeToAnsi 활용
				_worksheet.cell(cell_ref).value(UnicodeToAnsi(value));
			}
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("셀에 값을 쓸 수 없습니다: " + std::string(e.what()));
		}
	}

	//***************************************************************************
	// @brief 행과 열 번호를 이용해 셀에 와이드 문자열 값을 씁니다.
	// @detail 지정한 행, 열 좌표의 셀에 와이드 문자열을 인코딩 변환을 거쳐 기록합니다.
	// @param row 행 번호
	// @param col 열 번호
	// @param value 기록할 와이드 문자열 값
	// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
	//***************************************************************************
	void CXlntUtil::WriteCell(const uint32 row, const uint32 col, const std::wstring& value, const bool isCastUtf8)
	{
		if( value.size() < 1 ) return;

		try
		{
			if( isCastUtf8 )
			{
				_worksheet.cell(col, row).value(UnicodeToUtf8(value));
			}
			else
			{
				_worksheet.cell(col, row).value(UnicodeToAnsi(value));
			}
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
			return _worksheet.cell(cell_ref).to_string();
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
			return _worksheet.cell(col, row).to_string();
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
		_worksheet.merge_cells(cell1 + ":" + cell2);  // A1:B2 형태로 병합
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
		xlnt::cell_reference start_cell(start_col, start_row);  // 시작 셀
		xlnt::cell_reference end_cell(end_col, end_row);        // 끝 셀
		xlnt::range_reference range(start_cell, end_cell);      // 범위 지정

		_worksheet.merge_cells(range);							// 병합 수행
	}

	//***************************************************************************
	// @brief 두 셀 참조 주소를 기준으로 병합된 셀 범위를 해제합니다.
	// @detail 지정한 셀 범위의 병합 상태를 해제합니다.
	// @param cell1 시작 셀 참조 주소
	// @param cell2 끝 셀 참조 주소
	//***************************************************************************
	void CXlntUtil::UnMergeCells(const std::string& cell1, const std::string& cell2)
	{
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
		_worksheet.delete_columns(col_num, amount);		// 지정된 위치에 amount개의 열 삭제
	}

	//***************************************************************************
	// @brief 시트들의 순서를 재배치합니다.
	// @detail 새로운 시트 이름 순서 벡터에 맞춰 시트들을 재구성합니다.
	// @param new_order 새로운 순서의 시트 이름 벡터
	//***************************************************************************
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
	// @brief 시작과 끝 셀 주소를 이용해 xlnt 범위를 생성합니다.
	// @detail 문자열 형태의 셀 범위를 받아 xlnt::range 객체로 반환합니다.
	// @param start_cell 시작 셀 주소
	// @param end_cell 끝 셀 주소
	// @return 생성된 xlnt::range 객체
	//***************************************************************************
	xlnt::range CXlntUtil::CreateRange(const std::string& start_cell, const std::string& end_cell)
	{
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
	// @brief 현재 워크시트의 전체 행 수와 열 수를 가져옵니다.
	// @detail 시트에서 데이터가 존재하는 최고 행 번호와 최고 열 인덱스를 조회합니다.
	// @param rowCount [out] 조회된 행 개수
	// @param columnCount [out] 조회된 열 개수
	//***************************************************************************
	void CXlntUtil::GetRowColumnCount(uint32& rowCount, uint32& columnCount)
	{
		rowCount = _worksheet.highest_row();				// 마지막 행 번호
		columnCount = _worksheet.highest_column().index;	// 마지막 열 번호
	}

	//***************************************************************************
	// @brief 특정 행의 높이를 설정합니다.
	// @detail 지정한 행 번호의 행 속성에 접근하여 높이 값을 설정합니다.
	// @param row 대상 행 번호
	// @param height 설정할 높이 값
	//***************************************************************************
	void CXlntUtil::SetRowHeight(const uint32 row, const double height)
	{
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
		xlnt::cell cell = _worksheet.cell(col, row);		// 특정 열의 셀 가져오기
		xlnt::number_format text_format;
		auto formatted = text_format.format("@");			// 텍스트 형식 설정
		cell.number_format(formatted);						// 셀에 적용
	}

	//***************************************************************************
	// @brief 특정 열의 모든 행 셀에 텍스트 형식(서식)을 설정합니다.
	// @detail 1행부터 최고 행까지 순회하며 해당 열의 모든 셀에 텍스트 서식("@")을 적용합니다.
	// @param col 열 번호
	//***************************************************************************
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
	// @brief 현재 워크북을 지정한 파일 경로에 저장합니다.
	// @detail xlnt 워크북을 이용해 현재 데이터를 파일로 저장합니다.
	// @param filePath 저장할 파일 경로
	//***************************************************************************
	void CXlntUtil::SaveAs(const _tstring& filePath)
	{
		try
		{
#ifdef _UNICODE
			_workbook.save(TStringToString(filePath));
#else
			_workbook.save(filePath);
#endif
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error("파일을 저장할 수 없습니다: " + std::string(e.what()));
		}
	}
}