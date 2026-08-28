
//***************************************************************************
// UniqNumGenerator.h : interface for the CUniqNumGenerator class.
//
//***************************************************************************

#ifndef UC_UNIQNUMGENERATOR_H
#define UC_UNIQNUMGENERATOR_H

#pragma warning ( push )
#pragma warning( disable : 4006 4251 4786 )

#include <Util/BitField.h>

//***************************************************************************
// @class CUniqNumGenerator
// @brief 유일한 고유 숫자 키 생성 및 관리 클래스
// @details
//  - 유일한 숫자키 생성기
//  - 0은 사용하지 않음
//  - 1억일때 3125000 Byte 사용 => 2.9MB
//  - start에서부터 end까지 지정한데로 나온다
//***************************************************************************
class CUniqNumGenerator 
{
	enum
	{
		DEF_DEFAULT_START = 1,				// 기본 시작 번호
		DEF_DEFAULT_END = 2147483647,		// 기본 종료 번호
	};

public:
	enum 
	{
		DEF_MODE_ROTATE = 0,				// 순환 검색 모드
		DEF_MODE_NON_MEMORY_ROTATE = 1,		// 메모리 미사용 단순 순환 모드
		DEF_MODE_JAM = 2,					// 순차 탐색 모드
	};

	bool initialize(size_t mode = DEF_MODE_JAM, size_t start = DEF_DEFAULT_START, size_t end = DEF_DEFAULT_END);
	size_t get();
	void free(size_t num);
	void reset();
	size_t get_current_value();

	CUniqNumGenerator();
	virtual ~CUniqNumGenerator();

private:
	size_t getRotateFree();
	size_t getJamFree();

	size_t _start;           // 번호 생성 시작값
	size_t _end;             // 번호 생성 종료 범위 (+1 값)
	size_t _weightValue;     // 시작 offset 오프셋 가중치
	size_t _mode;            // 동작 모드
	size_t _rotate_mode_idx; // 순환 모드 탐색용 인덱스

	CBitField* _bitset;      // 번호 사용 여부 플래그 저장 비트 필드
};

#pragma warning ( pop )

#endif // ndef UC_UNIQNUMGENERATOR_H