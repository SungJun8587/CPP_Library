namespace Xlnt
{
	//***************************************************************************
	//
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
			_worksheet.cell(col + 1, 1).value(headers[col]);
		}

		// 데이터 추가
		if( isCastUtf8 )	// 한글 깨짐 방지 : ANSI(예: CP949, Windows-1252) -> UTF-8 변경하여 액셀 저장 
		{
			Iconv::CIconvUtil iconvUtil("CP949", "UTF-8");

			for( uint32 row = 0; row < data.size(); ++row )
			{
				auto excel_row = data[row].serialize();
				for( uint32 col = 0; col < excel_row.size(); ++col )
				{
					_worksheet.cell(col + 1, row + 2).value(iconvUtil.AnsiToUtf8(excel_row[col]));
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
	//
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