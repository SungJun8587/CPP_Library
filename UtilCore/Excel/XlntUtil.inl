
//***************************************************************************
// XlntUtil.inl : implementation of the CXlntUtil class.
//
//***************************************************************************

namespace Xlnt
{
	//***************************************************************************
	// @brief 벡터 데이터를 현재 워크시트에 직렬화하여 기록합니다.
	// @detail 템플릿 타입 T의 필드 이름과 직렬화 메서드를 이용하여 엑셀의 헤더와 데이터를 작성하며,
	//         옵션에 따라 ANSI 문자열을 UTF-8로 변환하여 저장합니다.
	// @param data 직렬화하여 기록할 데이터 객체들의 벡터
	// @param isCastUtf8 ANSI 문자열을 UTF-8로 변환할지 여부 (기본값: false)
	//***************************************************************************
	template <typename T>
	void CXlntUtil::Serialize(const std::vector<T>& data, const bool isCastUtf8)
	{
		if( _worksheet.title().empty() )
		{
			throw std::runtime_error("Active sheet is not set.");
		}

		// 헤더 추가
		const auto headers = T::get_field_names();
		for( uint32 col = 0; col < headers.size(); ++col )
		{
			_worksheet.cell(col + 1, 1).value(isCastUtf8 ? AnsiToUtf8(headers[col]) : headers[col]);
		}

		// 데이터 추가
		if( isCastUtf8 )	// 한글 깨짐 방지 : ANSI(예: CP949, Windows-1252) -> UTF-8 변경하여 액셀 저장 
		{
			for( uint32 row = 0; row < data.size(); ++row )
			{
				auto excel_row = data[row].serialize();
				for( uint32 col = 0; col < excel_row.size(); ++col )
				{
					_worksheet.cell(col + 1, row + 2).value(AnsiToUtf8(excel_row[col]));
				}
			}
		}
		else
		{
			for( uint32 row = 0; row < data.size(); ++row )
			{
				auto excel_row = data[row].serialize();
				for( uint32 col = 0; col < excel_row.size(); ++col )
				{
					_worksheet.cell(col + 1, row + 2).value(excel_row[col]);
				}
			}
		}
	}

	//***************************************************************************
	// @brief 현재 워크시트의 데이터를 역직렬화하여 객체 벡터로 반환합니다.
	// @detail 워크시트의 2행부터 최고 행까지 순회하며 각 행의 셀 값을 읽어와 
	//         템플릿 타입 T의 객체로 역직렬화한 뒤 벡터에 담아 반환합니다.
	// @return 역직렬화된 객체들의 벡터 (std::vector<T>)
	//***************************************************************************
	template <typename T>
	std::vector<T> CXlntUtil::Deserialize()
	{
		std::vector<T> result;

		if( _worksheet.title().empty() )
		{
			throw std::runtime_error("Active sheet is not set.");
		}

		for( uint32 row = 2; row <= _worksheet.highest_row(); ++row )
		{
			std::vector<std::string> excel_row;
			for( uint32 col = 1; col <= _worksheet.highest_column(); ++col )
			{
				excel_row.push_back(_worksheet.cell(col, row).to_string());
			}

			T obj;
			obj.deserialize(excel_row);
			result.push_back(obj);
		}

		return result;
	}
}