
//***************************************************************************
// XlntWorkbookReader.h : interface for the CXlntWorkbookReader class.
//
//***************************************************************************

#ifndef UC_XLNTWORKBOOKREADER_H
#define UC_XLNTWORKBOOKREADER_H

#include <string>
#include <vector>

#include <BaseRedefineDataType.h>

#include "XlntUtil.h"
#include "XlntDefine.h"

namespace Xlnt
{
	//***************************************************************************
	// @class CXlntWorkbookReader
	// @brief CXlntUtil(xlnt)로 .xlsx의 시트 이름 목록과 시트별 셀 값을 타입을 보존해서 읽습니다.
	// @detail CXlntUtil::ReadCell은 셀을 문자열로만 돌려주어 숫자/문자열/불리언 구분이 사라지므로,
	//         셀 타입은 GetWorkSheet()로 얻은 xlnt 워크시트에서 직접 읽어 SCell에 담습니다.
	//         수식 셀은 파일에 저장된 계산 결과 값을 읽습니다(재계산하지 않음).
	//         날짜 서식이 적용된 숫자는 일련번호가 아니라 ISO 8601 문자열로 읽습니다.
	//         파일을 열 수 없거나 구조가 잘못되었으면 std::runtime_error를 던집니다.
	//***************************************************************************
	class CXlntWorkbookReader
	{
	public:
		//***************************************************************************
		// @brief xlsx 파일을 열어 시트 이름 목록을 읽습니다. 시트 본문은 ReadSheet에서 읽습니다.
		// @param filePath .xlsx 파일 경로
		// @exception std::runtime_error 파일을 열 수 없거나 xlsx 구조가 잘못됐을 때
		//***************************************************************************
		explicit CXlntWorkbookReader(const _tstring& filePath);

		//***************************************************************************
		// @brief 시트 이름을 통합 문서 순서대로 반환합니다. (UTF-8)
		//***************************************************************************
		const std::vector<std::string>& GetSheetNames() const { return _sheetNames; }

		//***************************************************************************
		// @brief 시트 하나의 셀 값을 읽습니다.
		// @param name  시트 이름 (UTF-8)
		// @param rows  [out] 행 번호(1-base) -> 행. 값이 있는 셀만 채워집니다
		// @param error [out] 시트가 없을 때의 사유
		// @return 읽었으면 true
		//***************************************************************************
		bool ReadSheet(const std::string& name, TRows& rows, std::string& error);

	private:
		CXlntUtil					_excel;
		std::vector<std::string>	_sheetNames;
	};
}

#endif // ndef UC_XLNTWORKBOOKREADER_H