
//***************************************************************************
// BitField.h : interface for the CBitField class.
//
//***************************************************************************

#ifndef __BITFIELD_H__
#define __BITFIELD_H__

#include <cstddef>

//***************************************************************************
// @class CBitField
// @brief 비트 배열 생성 및 플래그 관리 클래스
// @details
//  - 비트 배열 생성
//  - 0이란 인덱스는 존재하지 않는다
//  - 1번째 비트 부터 생성자에서 지정한 count까지 지정된다
//  - [장점 및 활용]
//    1. 대용량 비트 플래그 관리: 수백만~수억 개 규모의 비트 데이터를 바이트 단위로 패킹하여 메모리 사용량을 최소화 (예: 1억 개 기준 약 2.9MB 사용)
//    2. 유일 번호/ID 관리자(UniqNumGenerator): 동적 할당 오브젝트 풀이나 고유 키 생성기의 비트셋 슬롯 관리에 최적화
//    3. 게임 콘텐츠 활용: 플레이어의 장기 출석 체크, 수백 개 퀘스트/업적 완료 상태, 타일맵 통행(Passable) 여부 관리 시 유용
//***************************************************************************
class CBitField
{
	enum
	{
		DEF_BIT_PER_BYTE = 8, // 한 바이트당 비트 수
	};

public:
	void reset();
	bool isOn(size_t idx);
	void setOn(size_t idx);
	void setOff(size_t idx);

	CBitField(size_t* buffer, size_t size);
	CBitField(size_t count);
	virtual ~CBitField();

private:
	size_t getArrayIdx(size_t value);
	size_t getPointValue(size_t value);

	size_t* _bits;						// 비트 데이터를 저장하는 배열 포인터
	size_t _real_array_size;			// 할당된 size_t 배열의 실제 크기
	size_t _machine_bit;				// 시스템 아키텍처의 비트 수 (32비트/64비트)
	bool _isExtern;						// 외부 버퍼 사용 여부
};

#endif // ndef __BITFIELD_H__