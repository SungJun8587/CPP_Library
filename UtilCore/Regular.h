
//***************************************************************************
// Regular.h: interface for the Reqular Expression Functions.
//
//***************************************************************************

#ifndef __REGULAR_H__
#define __REGULAR_H__

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

BOOL	IsAllAscii(const TCHAR *ptszSource);
BOOL	IsAllAlphaNum(const TCHAR *ptszSource);
BOOL	IsAllAlphaKor(const TCHAR *ptszSource);
BOOL	IsAllKorNum(const TCHAR *ptszSource);
BOOL	IsAllAlphaKorNum(const TCHAR *ptszSource);

BOOL	IsAllAlpha(const TCHAR *ptszSource);
BOOL	IsAllKorean(const TCHAR *ptszSource);
BOOL	IsAllNumeric(const TCHAR *ptszSource);

BOOL	IsCharacter(const int ch);
BOOL	IsKoreanChar(const TCHAR ch);

#endif // ndef __REGULAR_H__
