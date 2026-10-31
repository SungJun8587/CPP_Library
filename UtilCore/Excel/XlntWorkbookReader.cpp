
//***************************************************************************
// XlntWorkbookReader.cpp: implementation of the CXlntWorkbookReader class.
//
//***************************************************************************

#include "pch.h"
#include "XlntWorkbookReader.h"

namespace Xlnt
{
	//***************************************************************************
	// @brief xlsx를 열어 시트 이름 목록을 읽습니다. 시트 본문은 ReadSheet에서 읽는다.
	// @param filePath .xlsx 파일 경로
	// @exception std::runtime_error 파일을 열 수 없거나 xlsx 구조가 잘못됐을 때
	//***************************************************************************
	CXlntWorkbookReader::CXlntWorkbookReader(const _tstring& filePath)
	{
		_excel.OpenExcel(filePath);
		_sheetNames = _excel.GetSheetNames();
	}

	//***************************************************************************
	// @brief 시트 하나의 셀 값을 읽습니다.
	// @param name  시트 이름 (UTF-8)
	// @param rows  [out] 행 번호 -> 행. 값이 있는 셀만 채워진다
	// @param error [out] 시트가 없을 때의 사유
	// @return 읽었으면 true
	//***************************************************************************
	bool CXlntWorkbookReader::ReadSheet(const std::string& name, TRows& rows, std::string& error)
	{
		rows.clear();

		bool found = false;
		for( const std::string& n : _sheetNames )
		{
			if( n == name )
			{
				found = true;
				break;
			}
		}
		if( !found )
		{
			error = "시트 '" + name + "'가 없습니다";
			return false;
		}

		_excel.ActiveSheet(name);

		// rows(true): 값이 있는 셀만 순회한다. (빈 칸마다 셀을 만들지 않는다)
		for( auto row : _excel.GetWorkSheet().rows(true) )
		{
			for( auto cell : row )
			{
				SCell value = ToSCell(cell);
				if( value.kind == SCell::Empty )
					continue;

				TRow& target = rows[static_cast<int>(cell.row())];
				const size_t col = static_cast<size_t>(cell.column().index) - 1;
				if( target.size() <= col )
					target.resize(col + 1);
				target[col] = std::move(value);
			}
		}

		return true;
	}
}