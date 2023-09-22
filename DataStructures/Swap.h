
//***************************************************************************
// Swap.h: interface for the Swap class.
//
//***************************************************************************

#ifndef __SWAP_H__
#define __SWAP_H__

//***************************************************************************
//
template<class T> inline void Swap(T& x, T& y)
{
	T temp;

	temp = x;
	x = y;
	y = temp;
}

//***************************************************************************
//
template<class T> inline T& SwapRet(T& x, T& y)
{
	T temp;

	temp = x;
	x = y;
	y = temp;

	return x;
}

//***************************************************************************
//
template<class T> inline const T& Max(const T& x, const T& y)
{
	if( x > y )
	{
		return x;
	}

	return y;
}

//***************************************************************************
//
template<class T> inline const T& Min(const T& x, const T& y)
{
	if( x < y )
	{
		return x;
	}

	return y;
}

#endif // ndef __SWAP_H__
