
//***************************************************************************
// ConfigFieldSpec.h : common types for the config field readers (JSON / XML).
//
//***************************************************************************

#ifndef UC_CONFIGFIELDSPEC_H
#define UC_CONFIGFIELDSPEC_H

#include <limits>

//***************************************************************************
// @enum EFieldType
// @brief ReadObject/ReadObjects가 객체 필드를 검증할 때 쓰는 값 종류.
//***************************************************************************
enum class EFieldType
{
	Int,		// 부호 있는 정수
	UInt,		// 부호 없는 정수
	Number,		// 정수 또는 실수
	String,		// 문자열
	Bool,		// 참/거짓
};

//***************************************************************************
// @struct SFieldSpec
// @brief 객체 필드 한 개의 검증 규칙.
// @details minV/maxV는 Int/UInt 필드에만 적용된다. required가 false면 필드가 없어도 통과한다.
//***************************************************************************
struct SFieldSpec
{
	const TCHAR*	name;
	EFieldType		type;
	int64			minV = (std::numeric_limits<int64>::min)();
	int64			maxV = (std::numeric_limits<int64>::max)();
	bool			required = true;
};

#endif // ndef UC_CONFIGFIELDSPEC_H
