
//***************************************************************************
// BitField.cpp : implementation of the CBitField class.
//
//***************************************************************************

#include "pch.h"
#include "BitField.h"

//***************************************************************************
// @brief 외부 버퍼를 사용하는 CBitField 생성자
// @param buffer 비트 필드로 사용할 외부 size_t 버퍼 포인터
// @param size 버퍼의 전체 바이트 크기
//***************************************************************************
CBitField::CBitField(size_t* buffer, size_t size) 
{
	_isExtern = true;
	_machine_bit = (sizeof(size_t) * DEF_BIT_PER_BYTE);
	_real_array_size = size / sizeof(size_t);
	_bits = buffer;
}

//***************************************************************************
// @brief 내부 동적 할당을 사용하는 CBitField 생성자
// @param count 사용할 최대 비트 개수
//***************************************************************************
CBitField::CBitField(size_t count) 
{
	_isExtern = false;
	count++;
	_machine_bit = (sizeof(size_t) * DEF_BIT_PER_BYTE);
	_real_array_size = (count + (_machine_bit - 1)) / _machine_bit;
	_bits = new size_t[_real_array_size];
	reset();
}

//***************************************************************************
// @brief CBitField 소멸자 (내부 할당된 메모리 해제)
//***************************************************************************
CBitField::~CBitField() 
{
	if( !_isExtern )
		SAFE_DELETE_ARRAY(_bits);
}

//***************************************************************************
// @brief 전체 비트 배열을 0으로 초기화
//***************************************************************************
void CBitField::reset() 
{
	memset(_bits, 0, sizeof(size_t) * _real_array_size);
}

//***************************************************************************
// @brief 지정한 인덱스의 비트가 켜져(1) 있는지 확인
// @param idx 확인할 비트 인덱스 (1-based)
// @return true 비트가 1인 경우, false 비트가 0이거나 idx가 0인 경우
//***************************************************************************
bool CBitField::isOn(size_t idx) 
{
	if( !idx )
		return false;

	size_t array_idx = getArrayIdx(idx);
	size_t point_value = getPointValue(idx);

	return (_bits[array_idx] & point_value) != 0;
}

//***************************************************************************
// @brief 지정한 인덱스의 비트를 켬(1로 설정)
// @param idx 설정할 비트 인덱스 (1-based)
//***************************************************************************
void CBitField::setOn(size_t idx) 
{
	if( !idx )
		return;

	size_t array_idx = getArrayIdx(idx);
	size_t point_value = getPointValue(idx);

	_bits[array_idx] |= point_value;
}

//***************************************************************************
// @brief 지정한 인덱스의 비트를 끰(0으로 설정)
// @param idx 해제할 비트 인덱스 (1-based)
//***************************************************************************
void CBitField::setOff(size_t idx) 
{
	if( !idx )
		return;

	size_t array_idx = getArrayIdx(idx);
	size_t point_value = getPointValue(idx);

	_bits[array_idx] &= ~point_value;
}

//***************************************************************************
// @brief 비트 인덱스가 속한 배열 요소의 인덱스를 계산
// @param value 비트 인덱스
// @return size_t 배열의 인덱스
//***************************************************************************
size_t CBitField::getArrayIdx(size_t value) 
{
	if( value == 0 )
		return 0;

	return value / _machine_bit;
}

//***************************************************************************
// @brief 비트 위치에 해당하는 비트 마스크 값 계산 (64비트 오버플로우 오류 수정 반영)
// @param value 비트 인덱스
// @return size_t 비트 마스크 값
//***************************************************************************
size_t CBitField::getPointValue(size_t value) 
{
	if( !value )
		return 0;

	// 64비트 환경 오버플로우 방지를 위해 1ULL(Unsigned Long Long) 사용
	return static_cast<size_t>(1ULL << (value % _machine_bit));
}