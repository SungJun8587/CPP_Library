
//***************************************************************************
// XlntUtil.h : interface for the CXlntUtil class.
//
//***************************************************************************

#ifndef __XLNTUTIL_H__
#define __XLNTUTIL_H__

#include <xlnt/xlnt.hpp>

#ifndef	__BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

namespace Xlnt
{
	class CXlntUtil
	{
		public:
			CXlntUtil();
			~CXlntUtil();

			xlnt::worksheet& getSheet() { return _worksheet; }

			void OpenExcel(const _tstring& filePath);

			void AddSheet(const std::string& sheetName);
			void RemoveSheet(const std::string& sheetName);

			void WriteCell(const std::string& cell_ref, const std::string& value);
			void WriteCell(const uint32 row, const uint32 col, const std::string& value);
			void WriteCell(const std::string& cell_ref, const std::wstring& value);
			void WriteCell(const uint32 row, const uint32 col, const std::wstring& value);

			std::string ReadCell(const std::string& cell_ref);
			std::string ReadCell(const uint32 row, const uint32 col);

			void MergeCells(const std::string& cell1, const std::string& cell2);
			void MergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

			void UnMergeCells(const std::string& cell1, const std::string& cell2);
			void UnMergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

			void InsertRow(const uint32 row_num, const uint32 amount = 1);
			void InsertColumn(const uint32 col_num, const uint32 amount = 1);
			void DeleteRow(const uint32 row_num, const uint32 amount = 1);
			void DeleteColumn(const uint32 col_num, const uint32 amount = 1);
			void ReOrderSheets(const std::vector<std::string>& new_order);

			xlnt::range CreateRange(const std::string& start_cell, const std::string& end_cell);
			xlnt::range CreateRange(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

			void WriteToRange(xlnt::range range, const std::string& value);

			void SetRowHeight(const uint32 row, const double height);
			void SetColumnWidth(const uint32 col, const double width);
			void SetCellTextFormat(const uint32 row, const uint32 col);
			void SetAllCellTextFormat(const uint32 col);

			void SaveAs(const _tstring& filePath);

		private:
			xlnt::workbook	_workbook;
			xlnt::worksheet	_worksheet;
	};
}

#endif // ndef __XLNTUTIL_H__
