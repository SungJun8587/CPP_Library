
//***************************************************************************
// BitOperate.h : interface for the CBitOperate class.
//
//***************************************************************************

#ifndef __BITOPERATE_H__
#define __BITOPERATE_H__

#include <cstddef>

//***************************************************************************
// @class CBitOperate
// @brief 템플릿 기반 단일 정수형 비트 플래그 연산 클래스
// @details
//  - 템플릿 타입 T(정수형)의 비트 단위 연산을 지원
//  - 비트 위치(인덱스)를 지정하여 켜기, 끄기, 검사 동작 수행
//  - [장점 및 활용]
//    1. 단일 정수 변수(8/16/32/64비트) 하나로 수십 가지의 상태 플래그 조합을 효율적으로 관리
//    2. 캐릭터 상태 이상(스턴, 중독 등), 아이템 속성, UI 설정 플래그, AI 상태 머신(FSM) 검사 시
//      메모리 및 네트워크 패킷 용량을 대폭 절약할 수 있음
//***************************************************************************
template< typename T, T DEFAULT = T(0) >
class CBitOperate
{
public:
	CBitOperate(void);
	virtual ~CBitOperate(void);

	void	Reset(T tBit = DEFAULT);
	void	Add(T tAddBit);
	void	Remove(T tRemoveBit);
	bool	IsContained(T tCompareBit);

	T		GetValue(void) { return _tBit; }

protected:
	CBitOperate(const CBitOperate& rhs);
	CBitOperate& operator=(const CBitOperate& rhs);

	T	_tBit; // 비트 플래그 데이터 저장 변수
};

#include "BitOperate.inl"

#endif // __BITOPERATE_H__