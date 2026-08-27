
//***************************************************************************
// BitOperate.inl : implementation of the CBitOperate class.
//
//***************************************************************************

//***************************************************************************
// @brief CBitOperate 생성자
//***************************************************************************
template< typename T, T DEFAULT >
CBitOperate<T, DEFAULT>::CBitOperate(void)
	: _tBit(DEFAULT)
{
}

//***************************************************************************
// @brief CBitOperate 소멸자
//***************************************************************************
template< typename T, T DEFAULT >
CBitOperate<T, DEFAULT>::~CBitOperate(void)
{
}

//***************************************************************************
// @brief 비트 플래그 데이터 리셋/재설정
// @param tBit 설정할 기본 비트 값
//***************************************************************************
template< typename T, T DEFAULT >
void CBitOperate<T, DEFAULT>::Reset(T tBit)
{
	_tBit = tBit;
}

//***************************************************************************
// @brief 지정한 비트 위치(인덱스)의 플래그를 켬 (64비트 오버플로우 방지)
// @param tAddBit 켜고자 하는 비트 인덱스
//***************************************************************************
template< typename T, T DEFAULT >
void CBitOperate<T, DEFAULT>::Add(T tAddBit)
{
	_tBit = static_cast<T>(_tBit | (static_cast<T>(1) << tAddBit));
}

//***************************************************************************
// @brief 지정한 비트 위치(인덱스)의 플래그를 끰 (기존 XOR 버그 -> AND-NOT 수정)
// @param tRemoveBit 끄고자 하는 비트 인덱스
//***************************************************************************
template< typename T, T DEFAULT >
void CBitOperate<T, DEFAULT>::Remove(T tRemoveBit)
{
	_tBit = static_cast<T>(_tBit & ~(static_cast<T>(1) << tRemoveBit));
}

//***************************************************************************
// @brief 지정한 비트 위치(인덱스)의 플래그가 켜져 있는지 확인
// @param tCompareBit 확인할 비트 인덱스
// @return bool 비트가 켜져 있으면 true, 꺼져 있으면 false
//***************************************************************************
template< typename T, T DEFAULT >
bool CBitOperate<T, DEFAULT>::IsContained(T tCompareBit)
{
	T mask = static_cast<T>(static_cast<T>(1) << tCompareBit);
	return (_tBit & mask) == mask;
}