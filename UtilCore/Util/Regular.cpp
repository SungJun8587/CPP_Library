
//***************************************************************************
// Regular.cpp: implementation of the Regular Expression Functions.
//
//***************************************************************************

#include "pch.h"
#include "Regular.h"

//***************************************************************************
//
bool IsAllAscii(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istascii(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllAlpha(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalpha(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllKorean(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !(*ptszSourceLoc & 0x80) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllNumeric(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istdigit(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllAlphaNum(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalnum(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllAlphaKor(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalpha(*ptszSourceLoc) && !AsciiChar::IsKoreanChar(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllKorNum(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !AsciiChar::IsKoreanChar(*ptszSourceLoc) && !_istdigit(*ptszSourceLoc) ) return false;

	return true;
}

//***************************************************************************
//
bool IsAllAlphaKorNum(const TCHAR* ptszSource)
{
	const TCHAR* ptszSourceLoc = NULL;

	if( !ptszSource ) return false;
	if( _tcslen(ptszSource) < 0 ) return false;

	for( ptszSourceLoc = ptszSource; *ptszSourceLoc; ptszSourceLoc++ )
		if( !_istalnum(*ptszSourceLoc) && !AsciiChar::IsKoreanChar(*ptszSourceLoc) ) return false;

	return true;
}