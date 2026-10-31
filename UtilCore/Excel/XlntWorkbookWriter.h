
//***************************************************************************
// XlntWorkbookWriter.h : interface for the CXlntWorkbookWriter class.
//
//***************************************************************************

#ifndef UC_XLNTWORKBOOKWRITER_H
#define UC_XLNTWORKBOOKWRITER_H

#include <string>
#include <vector>

#include <BaseRedefineDataType.h>

#include "XlntUtil.h"
#include "XlntDefine.h"

namespace Xlnt
{
	//***************************************************************************
	// @class CXlntWorkbookWriter
	// @brief SCell/TRows(CXlntWorkbookReader가 돌려주는 셀 모델)를 새 .xlsx 파일로 씁니다.
	// @detail 셀 타입을 보존해서 씁니다. Num은 숫자 셀, Str은 문자열 셀, Bool은 불리언 셀이 됩니다.
	//         문자열은 UTF-8이며 '='로 시작해도 수식이 아니라 문자열로 저장됩니다.
	//         빈 문자열(Str, text가 빈 값)과 Empty 셀은 쓰지 않습니다. (xlsx에서는 값 없는 셀과 같습니다)
	//         문자열은 시트를 만들기 전에 검사해서, 잘못된 UTF-8, XML에 쓸 수 없는 제어 문자(탭/개행/CR 제외), UTF-8 32767바이트 초과이면
	//         셀 주소를 담아 std::runtime_error를 던집니다. (xlnt는 제어 문자에서 예외, 긴 문자열은 조용히 잘라내므로 미리 막습니다)
	//         Num의 text는 SCell과 같은 규칙의 숫자 원문("5", "0.1", "1E-07")이어야 하며 숫자로 해석되지 않으면
	//         std::runtime_error를 던집니다. 쓴 파일은 CXlntWorkbookReader로 읽으면 같은 TRows가 됩니다.
	//         xlnt는 숫자를 유효숫자 15자리로 저장합니다(엑셀과 같은 정밀도). 0.30000000000000004는 0.3으로, 2^53을 넘는
	//         정수는 15자리로 반올림되어 저장되므로 큰 ID는 숫자 셀 대신 문자열(Str)로 쓰세요.
	//         Save 전에는 파일이 만들어지지 않고, Save는 임시 파일에 쓴 뒤 교체해서 기존 파일을 깨뜨리지 않습니다.
	//         한 객체는 한 스레드에서만 사용합니다.
	//***************************************************************************
	class CXlntWorkbookWriter
	{
	public:
		//***************************************************************************
		// @brief 빈 통합 문서를 만듭니다. 시트는 AddSheet로 추가한 것만 저장됩니다.
		//***************************************************************************
		CXlntWorkbookWriter();

		//***************************************************************************
		// @brief 시트를 통합 문서 끝에 추가하고 셀 값을 씁니다.
		// @param name 시트 이름 (UTF-8, 1~31자, []:*?/\ 불가, 대소문자 구분 없이 중복 불가)
		// @param rows 행 번호(1-base) -> 행. 행 번호가 1 미만이면 오류
		// @exception std::runtime_error 이름이 잘못됐거나 중복일 때, 숫자 셀이 숫자가 아닐 때, 문자열을 쓸 수 없을 때, 범위를 벗어났을 때.
		//            예외가 나도 통합 문서는 호출 전 상태 그대로입니다.
		//***************************************************************************
		void AddSheet(const std::string& name, const TRows& rows);

		//***************************************************************************
		// @brief 추가한 시트 수를 반환합니다.
		//***************************************************************************
		size_t GetSheetCount() const { return _sheetNames.size(); }

		//***************************************************************************
		// @brief 파일로 저장합니다. 시트가 하나도 없으면 오류입니다.
		// @param filePath 저장할 .xlsx 경로 (기존 파일은 덮어씁니다)
		// @exception std::runtime_error 시트가 없거나 저장에 실패했을 때
		//***************************************************************************
		void Save(const _tstring& filePath);

	private:
		CXlntUtil					_excel;
		std::vector<std::string>	_sheetNames;
		bool						_placeholder = true;	// CXlntUtil이 만든 기본 시트가 아직 남아 있는지
	};
}

#endif // ndef UC_XLNTWORKBOOKWRITER_H