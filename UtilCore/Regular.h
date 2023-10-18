
//***************************************************************************
// Regular.h: interface for the Reqular Expression Functions.
//
//***************************************************************************

#ifndef __REGULAR_H__
#define __REGULAR_H__

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

bool	IsAllAscii(const TCHAR *ptszSource);
bool	IsAllAlphaNum(const TCHAR *ptszSource);
bool	IsAllAlphaKor(const TCHAR *ptszSource);
bool	IsAllKorNum(const TCHAR *ptszSource);
bool	IsAllAlphaKorNum(const TCHAR *ptszSource);

bool	IsAllAlpha(const TCHAR *ptszSource);
bool	IsAllKorean(const TCHAR *ptszSource);
bool	IsAllNumeric(const TCHAR *ptszSource);

bool	IsCharacter(const int ch);
bool	IsKoreanChar(const TCHAR ch);

#endif // ndef __REGULAR_H__
