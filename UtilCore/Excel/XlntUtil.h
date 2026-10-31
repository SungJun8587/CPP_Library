
//***************************************************************************
// XlntUtil.h : interface for the CXlntUtil class.
//
//***************************************************************************

#ifndef UC_XLNTUTIL_H
#define UC_XLNTUTIL_H

#include <charconv>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <xlnt/xlnt.hpp>

#include <BaseRedefineDataType.h>
#include <Util/EncodingConvert.h>

#include "XlntDefine.h"

#pragma comment(lib, LIB_NAME("xlnt"))

namespace Xlnt
{
	//***************************************************************************
	// @brief xlnt 셀 하나를 타입(숫자/문자열/불리언)을 보존한 SCell로 바꿉니다.
	// @detail 숫자는 NumberText 규칙의 원문으로, 날짜 서식이 적용된 숫자는 ISO 8601 문자열로 바꿉니다.
	//         CXlntWorkbookReader가 쓰는 변환이며, 셀 읽기는 모두 이 규칙을 따릅니다.
	// @param cell 읽을 셀
	// @return 변환된 SCell (값이 없으면 Empty)
	//***************************************************************************
	SCell ToSCell(const xlnt::cell& cell);

	//***************************************************************************
	// @brief 셀 값을 문자열로 읽습니다. (ReadCell, Deserialize가 사용)
	// @detail xlnt의 cell::to_string()은 숫자 셀에서 값을 잃습니다(12345.678이 빈 문자열, 1.5e300이 쓰레기 문자열). 숫자 셀은
	//         ToSCell과 같은 표기(5, 0.1, 1.5e+300)로 돌려주고, 그 외 타입은 to_string()을 그대로 씁니다.
	// @param cell 읽을 셀
	// @return 셀 값 문자열
	//***************************************************************************
	std::string CellText(const xlnt::cell& cell);

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

	//***************************************************************************
	// @brief 엑셀 데이터 입출력을 위해 객체를 직렬화/역직렬화하는 템플릿 구조체입니다.
	// @detail 가변인자 템플릿을 사용하여 객체의 필드들을 std::tuple로 관리하며,
	//         엑셀 행 데이터(std::vector<std::string>)와의 상호 변환 기능을 제공합니다.
	//***************************************************************************
	template <typename... Fields>
	struct ExcelSerializable
	{
		using FieldTuple = std::tuple<Fields...>;
		FieldTuple fields; // 필드 데이터를 저장하는 튜플

		ExcelSerializable(Fields... args) : fields(std::make_tuple(args...)) {}

		//***************************************************************************
		// @brief 데이터를 Excel 행으로 직렬화합니다.
		// @return 문자열 벡터 형태의 행 데이터
		//***************************************************************************
		std::vector<std::string> serialize() const
		{
			std::vector<std::string> row;
			for_each_in_tuple(fields, [&row](const auto& field, std::size_t)
				{
					row.push_back(to_string(field));
				});
			return row;
		}

		//***************************************************************************
		// @brief Excel 행 데이터를 객체에 역직렬화하여 적용합니다.
		// @param row Excel 행 데이터 문자열 벡터
		//***************************************************************************
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

		//***************************************************************************
		// @brief 필드 이름을 가져옵니다.
		// @return 필드 이름 문자열 벡터
		//***************************************************************************
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
			else if constexpr( std::is_floating_point_v<T> )
			{
				// std::to_string(double)은 소수점 6자리로 잘라 값이 바뀐다. 다시 읽었을 때 같은 값이 되는 최단 표기를 쓴다.
				char buf[64];
				const auto r = std::to_chars(buf, buf + sizeof(buf), value);
				return std::string(buf, r.ptr);
			}
			else
			{
				return std::to_string(value);
			}
		}

		template <typename T>
		static void from_string(const std::string& str, T& value)
		{
			if constexpr( std::is_same_v<T, std::string> )
			{
				value = str;
			}
			else if( str.empty() )
			{
				// 값이 없는 셀은 필드 기본값을 그대로 둔다. (빈 문자열을 숫자로 바꾸려 하면 예외가 된다)
			}
			else if constexpr( std::is_same_v<T, bool> )
			{
				value = (str == "1" || str == "true" || str == "TRUE" || str == "True");
			}
			else if constexpr( std::is_arithmetic_v<T> )
			{
				// stoi/stof는 int/float 범위로 잘라 64비트 값과 double 정밀도를 잃고 "12abc"도 12로 받는다.
				// 필드 타입 그대로, 문자열 전체가 맞을 때만 받는다.
				const char* first = str.data();
				const char* last = first + str.size();
				const auto r = std::from_chars(first, last, value);
				if( r.ec != std::errc() || r.ptr != last )
				{
					throw std::runtime_error("셀 값을 숫자로 바꿀 수 없습니다: '" + str + "'");
				}
			}
		}
	};

	//***************************************************************************
	// @brief 엑셀 파일의 읽기, 쓰기, 편집 및 데이터 매핑 기능을 제공하는 유틸리티 클래스입니다.
	// @detail xlnt 라이브러리를 기반으로 워크북과 워크시트를 제어하며,
	//         셀 읽기/쓰기, 범위 설정, 시트 관리, 객체 직렬화/역직렬화 기능을 수행합니다.
	//***************************************************************************
	class CXlntUtil
	{
	public:
		CXlntUtil();
		~CXlntUtil();

		// 현재 워크시트는 워크북 내부를 가리키는 핸들이라, 워크북을 복사하면 복사본의 핸들이 원본을 가리켜 원본이 사라진 뒤 해제된
		// 메모리를 읽게 됩니다. 복사는 막고 이동만 허용합니다. (워크북 내부 객체의 주소가 그대로여서 핸들이 유효합니다)
		CXlntUtil(const CXlntUtil&) = delete;
		CXlntUtil& operator=(const CXlntUtil&) = delete;
		CXlntUtil(CXlntUtil&&) noexcept = default;
		CXlntUtil& operator=(CXlntUtil&&) noexcept = default;

		//***************************************************************************
		// @brief 관리 중인 워크북 참조를 반환합니다.
		// @return xlnt 워크북 참조
		//***************************************************************************
		xlnt::workbook& GetWorkbook() { return _workbook; }

		//***************************************************************************
		// @brief 현재 활성화된 워크시트 참조를 반환합니다.
		// @return xlnt 워크시트 참조
		//***************************************************************************
		xlnt::worksheet& GetWorkSheet() { return _worksheet; }

		//***************************************************************************
		// @brief 지정한 경로의 엑셀 파일을 엽니다.
		// @param filePath 열고자 하는 엑셀 파일 경로
		//***************************************************************************
		void OpenExcel(const _tstring& filePath);

		//***************************************************************************
		// @brief 새로운 시트를 추가합니다.
		// @param sheetName 추가할 시트 이름
		//***************************************************************************
		void AddSheet(const std::string& sheetName);

		//***************************************************************************
		// @brief 현재 활성화된 시트의 이름을 변경합니다.
		// @param sheetName 변경할 새 시트 이름
		//***************************************************************************
		void RenameSheet(const std::string& sheetName);

		//***************************************************************************
		// @brief 이름으로 시트를 활성화합니다.
		// @param sheetName 활성화할 시트 이름
		//***************************************************************************
		void ActiveSheet(const std::string& sheetName);

		//***************************************************************************
		// @brief 인덱스로 시트를 활성화합니다.
		// @param sheetIndex 활성화할 시트 인덱스 (기본값: 0)
		//***************************************************************************
		void ActiveSheet(const uint32 sheetIndex = 0);

		//***************************************************************************
		// @brief 이름으로 시트를 제거합니다.
		// @param sheetName 제거할 시트 이름
		//***************************************************************************
		void RemoveSheet(const std::string& sheetName);

		//***************************************************************************
		// @brief 인덱스로 시트를 제거합니다.
		// @param sheetIndex 제거할 시트 인덱스 (기본값: 0)
		//***************************************************************************
		void RemoveSheet(const uint32 sheetIndex = 0);

		//***************************************************************************
		// @brief 현재 시트의 이름을 반환합니다.
		// @return 현재 시트 이름
		//***************************************************************************
		std::string GetCurrentSheetName() const { return _worksheet == nullptr ? std::string() : _worksheet.title(); }

		//***************************************************************************
		// @brief 모든 시트의 이름 목록을 반환합니다.
		// @return 시트 이름 문자열 벡터
		//***************************************************************************
		std::vector<std::string> GetSheetNames() const;

		//***************************************************************************
		// @brief 지정한 셀에 문자열 값을씁니다. (std::string)
		// @param cell_ref 셀 참조 주소 (예: "A1")
		// @param value 기록할 문자열 값
		// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
		//***************************************************************************
		void WriteCell(const std::string& cell_ref, const std::string& value, const bool isCastUtf8 = false);

		//***************************************************************************
		// @brief 지정한 행과 열 번호의 셀에 문자열 값을 씁니다. (std::string)
		// @param row 행 번호
		// @param col 열 번호
		// @param value 기록할 문자열 값
		// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
		//***************************************************************************
		void WriteCell(const uint32 row, const uint32 col, const std::string& value, const bool isCastUtf8 = false);

		//***************************************************************************
		// @brief 지정한 셀에 와이드 문자열 값을 씁니다. (std::wstring)
		// @detail 항상 UTF-8로 변환해서 기록합니다. isCastUtf8는 std::string 오버로드와 모양을 맞추기 위한 값이며 사용하지 않습니다.
		// @param cell_ref 셀 참조 주소 (예: "A1")
		// @param value 기록할 와이드 문자열 값
		// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
		//***************************************************************************
		void WriteCell(const std::string& cell_ref, const std::wstring& value, const bool isCastUtf8 = false);

		//***************************************************************************
		// @brief 지정한 행과 열 번호의 셀에 와이드 문자열 값을 씁니다. (std::wstring)
		// @detail 항상 UTF-8로 변환해서 기록합니다. isCastUtf8는 std::string 오버로드와 모양을 맞추기 위한 값이며 사용하지 않습니다.
		// @param row 행 번호
		// @param col 열 번호
		// @param value 기록할 와이드 문자열 값
		// @param isCastUtf8 UTF-8 캐스팅 여부 (기본값: false)
		//***************************************************************************
		void WriteCell(const uint32 row, const uint32 col, const std::wstring& value, const bool isCastUtf8 = false);

		//***************************************************************************
		// @brief 지정한 셀의 값을 읽어옵니다.
		// @param cell_ref 셀 참조 주소 (예: "A1")
		// @return 읽어온 셀의 문자열 값
		//***************************************************************************
		std::string ReadCell(const std::string& cell_ref);

		//***************************************************************************
		// @brief 지정한 행과 열 번호의 셀 값을 읽어옵니다.
		// @param row 행 번호
		// @param col 열 번호
		// @return 읽어온 셀의 문자열 값
		//***************************************************************************
		std::string ReadCell(const uint32 row, const uint32 col);

		//***************************************************************************
		// @brief 두 셀 범위를 병합합니다.
		// @param cell1 시작 셀 참조 주소
		// @param cell2 끝 셀 참조 주소
		//***************************************************************************
		void MergeCells(const std::string& cell1, const std::string& cell2);

		//***************************************************************************
		// @brief 시작과 끝 행/열 번호로 범위를 병합합니다.
		// @param start_row 시작 행 번호
		// @param start_col 시작 열 번호
		// @param end_row 끝 행 번호
		// @param end_col 끝 열 번호
		//***************************************************************************
		void MergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

		//***************************************************************************
		// @brief 병합된 셀 범위를 해제합니다.
		// @param cell1 시작 셀 참조 주소
		// @param cell2 끝 셀 참조 주소
		//***************************************************************************
		void UnMergeCells(const std::string& cell1, const std::string& cell2);

		//***************************************************************************
		// @brief 시작과 끝 행/열 번호로 병합된 셀 범위를 해제합니다.
		// @param start_row 시작 행 번호
		// @param start_col 시작 열 번호
		// @param end_row 끝 행 번호
		// @param end_col 끝 열 번호
		//***************************************************************************
		void UnMergeCells(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

		//***************************************************************************
		// @brief 지정한 위치에 행을 삽입합니다.
		// @param row_num 삽입할 행 번호
		// @param amount 삽입할 행 개수 (기본값: 1)
		//***************************************************************************
		void InsertRow(const uint32 row_num, const uint32 amount = 1);

		//***************************************************************************
		// @brief 지정한 위치에 열을 삽입합니다.
		// @param col_num 삽입할 열 번호
		// @param amount 삽입할 열 개수 (기본값: 1)
		//***************************************************************************
		void InsertColumn(const uint32 col_num, const uint32 amount = 1);

		//***************************************************************************
		// @brief 지정한 위치의 행을 삭제합니다.
		// @param row_num 삭제할 행 번호
		// @param amount 삭제할 행 개수 (기본값: 1)
		//***************************************************************************
		void DeleteRow(const uint32 row_num, const uint32 amount = 1);

		//***************************************************************************
		// @brief 지정한 위치의 열을 삭제합니다.
		// @param col_num 삭제할 열 번호
		// @param amount 삭제할 열 개수 (기본값: 1)
		//***************************************************************************
		void DeleteColumn(const uint32 col_num, const uint32 amount = 1);

		//***************************************************************************
		// @brief 시트 순서를 재배치합니다.
		// @param new_order 새로운 순서의 시트 이름 벡터
		//***************************************************************************
		void ReOrderSheets(const std::vector<std::string>& new_order);

		//***************************************************************************
		// @brief 셀 범위를 생성합니다.
		// @param start_cell 시작 셀 참조 주소
		// @param end_cell 끝 셀 참조 주소
		// @return 생성된 xlnt 범위 객체
		//***************************************************************************
		xlnt::range CreateRange(const std::string& start_cell, const std::string& end_cell);

		//***************************************************************************
		// @brief 행/열 번호 기반으로 셀 범위를 생성합니다.
		// @param start_row 시작 행 번호
		// @param start_col 시작 열 번호
		// @param end_row 끝 행 번호
		// @param end_col 끝 열 번호
		// @return 생성된 xlnt 범위 객체
		//***************************************************************************
		xlnt::range CreateRange(const uint32 start_row, const uint32 start_col, const uint32 end_row, const uint32 end_col);

		//***************************************************************************
		// @brief 지정한 범위에 값을 일괄 기록합니다.
		// @param range 대상 xlnt 범위 객체
		// @param value 기록할 값
		//***************************************************************************
		void WriteToRange(xlnt::range range, const std::string& value);

		//***************************************************************************
		// @brief 전체 행과 열의 개수를 가져옵니다.
		// @param rowCount [out] 행 개수
		// @param columnCount [out] 열 개수
		//***************************************************************************
		void GetRowColumnCount(uint32& rowCount, uint32& columnCount);

		//***************************************************************************
		// @brief 특정 행의 높이를 설정합니다.
		// @param row 대상 행 번호
		// @param height 설정할 높이 값
		//***************************************************************************
		void SetRowHeight(const uint32 row, const double height);

		//***************************************************************************
		// @brief 특정 열의 너비를 설정합니다.
		// @param col 대상 열 번호
		// @param width 설정할 너비 값
		//***************************************************************************
		void SetColumnWidth(const uint32 col, const double width);

		//***************************************************************************
		// @brief 특정 셀의 텍스트 형식을 설정합니다.
		// @param row 행 번호
		// @param col 열 번호
		//***************************************************************************
		void SetCellTextFormat(const uint32 row, const uint32 col);

		//***************************************************************************
		// @brief 특정 열의 모든 셀 텍스트 형식을 설정합니다.
		// @param col 열 번호
		//***************************************************************************
		void SetAllCellTextFormat(const uint32 col);

		//***************************************************************************
		// @brief 현재 워크북을 지정한 경로에 저장합니다.
		// @param filePath 저장할 파일 경로
		//***************************************************************************
		void SaveAs(const _tstring& filePath);

		template <typename T>
		void Serialize(const std::vector<T>& data, const bool isCastUtf8 = false);

		template <typename T>
		std::vector<T> Deserialize();

	private:
		xlnt::workbook	_workbook;	// 엑셀 워크북 객체
		xlnt::worksheet	_worksheet;	// 현재 활성화된 엑셀 워크시트 객체
	};
}

#include "Excel/XlntUtil.inl"

#endif // ndef UC_XLNTUTIL_H