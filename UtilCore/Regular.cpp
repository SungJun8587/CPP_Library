
//***************************************************************************
// Regular.cpp: implementation of the Regular Expression Functions.
//
//***************************************************************************

#include "pch.h"
#include "Regular.h"

//***************************************************************************
//
BOOL IsAllAscii(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istascii(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllAlpha(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalpha(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllKorean(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !(*ptszSourceLoc & 0x80) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllNumeric(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istdigit(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllAlphaNum(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalnum(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllAlphaKor(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalpha(*ptszSourceLoc) && !IsKoreanChar(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllKorNum(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !IsKoreanChar(*ptszSourceLoc) && !_istdigit(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsAllAlphaKorNum(const TCHAR *ptszSource)
{
	const TCHAR	*ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalnum(*ptszSourceLoc) && !IsKoreanChar(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
BOOL IsCharacter(const int ch)
{
	if( ch >= _T('0') && ch <= _T('9') ) return true;
	if( ch >= _T('a') && ch <= _T('z') ) return true;
	if( ch >= _T('A') && ch <= _T('Z') ) return true;
	if( ch == _T('-') || ch == _T('.') || ch == _T('?') || ch == _T('/') || ch == _T('&') || ch == _T('=') || ch == _T(':') ) return true;

	return false;
}

//***************************************************************************
//
BOOL IsKoreanChar(const TCHAR ch)
{
	return (ch & 0x80);
}


