
//***************************************************************************
// XlntUtil.h : interface for the CXlntUtil class.
//
//***************************************************************************

#ifndef __XLNTUTIL_H__
#define __XLNTUTIL_H__

#include <tuple>
#include <vector>
#include <xlnt/xlnt.hpp>

#ifndef	__BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#pragma comment(lib, LIB_NAME("xlnt"))

namespace Xlnt
{
	// Helper: std::tuple의 각 요소를 반복적으로 처리
	template <typename Tuple, typename Func, std::size_t... Is>
	void for_each_impl(Tuple&& tuple, Func&& func, std::index_sequence<Is...>)
	{
		(func(std::get<Is>(tuple), Is), ...);
	}

	template <typename Tuple, typename Func>
	void for_each_in_tuple(Tuple&& tuple, Func&& func)
	{
		constexpr auto size = std::tuple_size<std::remove_reference_t<Tuple>>::value;
		for_each_impl(std::forward<Tuple>(tuple), std::forward<Func>(func), std::make_index_sequence<size>{});
	}

	// ExcelSerializable 템플릿 클래스
	template <typename... Fields>
	struct ExcelSerializable {
		using FieldTuple = std::tuple<Fields...>;
		FieldTuple fields;

		ExcelSerializable(Fields... args) : fields(std::make_tuple(args...)) {}

		// 직렬화: 데이터를 Excel 행으로 변환
		std::vector<std::string> serialize() const
		{
			std::vector<std::string> row;
			for_each_in_tuple(fields, [&row](const auto& field, std::size_t)
			{
				row.push_back(to_string(field));
			});
			return row;
		}

		// 역직렬화: Excel 행 데이터를 객체에 적용
		void deserialize(const std::vector<std::string>& row)
		{
			for_each_in_tuple(fields, [&row](auto& field, std::size_t index)
			{
				if( index < row.size() )
				{
					from_string(row[index], field);
				}
			});
		}

		// 필드 이름 가져오기 (데이터 클래스에서 구현 필요)
		static std::vector<std::string> get_field_names()
		{
			return {};
		}

	private:
		template <typename T>
		static std::string to_string(const T& value)
		{
			if constexpr( std::is_same_v<T, std::string> )
			{
				return value;
			}
			else
			{
				return std::to_string(value);
			}
		}

		template <typename T>
		static void from_string(const std::string& str, T& value)
		{
			if constexpr( std::is_integral_v<T> )
			{
				value = std::stoi(str);
			}
			else if constexpr( std::is_floating_point_v<T> )
			{
				value = std::stof(str);
			}
			else if constexpr( std::is_same_v<T, std::string> )
			{
				value = str;
			}
		}
	};

	class CXlntUtil
	{
	public:
		CXlntUtil();
		~CXlntUtil();

		xlnt::workbook& GetWorkbook() { return _workbook; }
		xlnt::worksheet& GetWorkSheet() { return _worksheet; }

		void OpenExcel(const _tstring& filePath);

		void AddSheet(const std::string& sheetName);
		void RenameSheet(const std::string& sheetName);
		void ActiveSheet(const std::string& sheetName);
		void ActiveSheet(const uint32 sheetIndex = 0);
		void RemoveSheet(const std::string& sheetName);
		void RemoveSheet(const uint32 sheetIndex = 0);

		std::string GetCurrentSheetName() const { return _worksheet.title(); }
		std::vector<std::string> GetSheetNames() const;

		void WriteCell(const std::string& cell_ref, const std::string& value, const bool isCastUtf8 = false);
		void WriteCell(const uint32 row, const uint32 col, const std::string& value, const bool isCastUtf8 = false);
		void WriteCell(const std::string& cell_ref, const std::wstring& value, const bool isCastUtf8 = false);
		void WriteCell(const uint32 row, const uint32 col, const std::wstring& value, const bool isCastUtf8 = false);

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

		void GetRowColumnCount(uint32& rowCount, uint32& columnCount);
		void SetRowHeight(const uint32 row, const double height);
		void SetColumnWidth(const uint32 col, const double width);
		void SetCellTextFormat(const uint32 row, const uint32 col);
		void SetAllCellTextFormat(const uint32 col);

		void SaveAs(const _tstring& filePath);

		template <typename T>
		void Serialize(const std::vector<T>& data, const bool isCastUtf8 = false);

		template <typename T>
		std::vector<T> Deserialize();

	private:
		xlnt::workbook	_workbook;
		xlnt::worksheet	_worksheet;
	};
}

#include "Excel/XlntUtil.inl"

#endif // ndef __XLNTUTIL_H__
