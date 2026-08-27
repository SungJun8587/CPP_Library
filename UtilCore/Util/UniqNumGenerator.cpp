
//***************************************************************************
// UniqNumGenerator.cpp : implementation of the CUniqNumGenerator class.
//
//***************************************************************************

#include "pch.h"
#include "UniqNumGenerator.h"

//***************************************************************************
// @brief CUniqNumGenerator 생성자
//***************************************************************************
CUniqNumGenerator::CUniqNumGenerator() 
{
	_bitset = nullptr;
	_start = 1;
	_end = 1;
	_weightValue = 0;
	_mode = DEF_MODE_JAM;
	_rotate_mode_idx = 1;
}

//***************************************************************************
// @brief CUniqNumGenerator 소멸자 (동적 할당 객체 해제)
//***************************************************************************
CUniqNumGenerator::~CUniqNumGenerator() 
{
	SAFE_DELETE(_bitset);
}

//***************************************************************************
// @brief 번호 생성기 초기화 함수 (메모리 누수 보완 반영)
// @param mode 생성 모드 (DEF_MODE_ROTATE, DEF_MODE_NON_MEMORY_ROTATE, DEF_MODE_JAM)
// @param start 생성할 시작 번호
// @param end 생성할 끝 번호
// @return bool 초기화 성공 여부
//***************************************************************************
bool CUniqNumGenerator::initialize(size_t mode, size_t start, size_t end) 
{
	_start = start;
	_end = end + 1;
	_mode = mode;
	_rotate_mode_idx = 1;

	// 이전 생성된 _bitset 안전하게 제거 (메모리 누수 방지)
	SAFE_DELETE(_bitset);

	if( _mode != DEF_MODE_NON_MEMORY_ROTATE ) 
	{
		_bitset = new CBitField(_end - _start);
	}

	if( _start != 1 )
		_weightValue = _start;
	else
		_weightValue = 0;

	return true;
}

//***************************************************************************
// @brief 사용 중인 번호 상태 전체 리셋
//***************************************************************************
void CUniqNumGenerator::reset() 
{
	if( _bitset ) 
	{
		_bitset->reset();
	}
	_rotate_mode_idx = 1;
}

//***************************************************************************
// @brief 현재 순환 탐색 인덱스 위치 반환
// @return size_t 현재 탐색 위치 인덱스
//***************************************************************************
size_t CUniqNumGenerator::get_current_value() 
{
	return _rotate_mode_idx;
}

//***************************************************************************
// @brief 사용 가능한 유일 번호(ID) 발급
// @return size_t 발급된 번호 (0일 경우 발급 실패)
//***************************************************************************
size_t CUniqNumGenerator::get() 
{
	size_t retValue = 0;

	if( _mode == DEF_MODE_ROTATE )
		retValue = getRotateFree();
	else if( _mode == DEF_MODE_NON_MEMORY_ROTATE ) 
	{
		retValue = _rotate_mode_idx++;

		if( _end <= _rotate_mode_idx )
			_rotate_mode_idx = 1;
	}
	else
		retValue = getJamFree();

	if( retValue == 0 )
		return 0;

	return (retValue + _weightValue);
}

//***************************************************************************
// @brief 발급받았던 번호를 반납 및 해제
// @param num 반납할 고유 번호
//***************************************************************************
void CUniqNumGenerator::free(size_t num) 
{
	if( _mode == DEF_MODE_NON_MEMORY_ROTATE || _bitset == nullptr )
		return;

	if( num < _weightValue || num >= (_end + _weightValue) ) 
	{
		return;
	}

	num -= _weightValue;

	_bitset->setOff(num);
}

//***************************************************************************
// @brief 순환 모드로 사용 가능한 번호 탐색 (링 버퍼 전체 탐색 로직 보완)
// @return size_t 할당받은 번호 offset (0은 할당 실패)
//***************************************************************************
size_t CUniqNumGenerator::getRotateFree() 
{
	if( !_bitset ) return 0;

	size_t total_count = _end - 1;

	// 최대 한 바퀴 전체 탐색하도록 보완
	for( size_t i = 0; i < total_count; ++i ) 
	{
		if( _rotate_mode_idx >= _end ) 
		{
			_rotate_mode_idx = 1;
		}

		if( !_bitset->isOn(_rotate_mode_idx) ) 
		{
			size_t found = _rotate_mode_idx;
			_bitset->setOn(found);
			_rotate_mode_idx++; // 다음 탐색 위치 이동
			return found;
		}

		_rotate_mode_idx++;
	}

	return 0; // 빈 자리가 없음
}

//***************************************************************************
// @brief 순차 모드로 앞에서부터 사용 가능한 번호 탐색
// @return size_t 할당받은 번호 offset (0은 할당 실패)
//***************************************************************************
size_t CUniqNumGenerator::getJamFree()
{
	if( !_bitset ) return 0;

	for( size_t retValue = 1; retValue < _end; retValue++ ) 
	{
		if( !_bitset->isOn(retValue) ) 
		{
			_bitset->setOn(retValue);
			return retValue;
		}
	}

	return 0;
}