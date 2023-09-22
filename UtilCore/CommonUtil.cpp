
//***************************************************************************
// ClusteredMap.cpp : implementation of the CClusteredMap class.
//
//***************************************************************************

#include "pch.h"
#include "CommonUtil.h"

//***************************************************************************
//
uint32 GetUInt32(const char* pszText)
{
	if( pszText == NULL ) return 0;

	return strtoul(pszText, NULL, 10);
}

//***************************************************************************
//
uint64 GetUInt64(const char* pszText)
{
	if( pszText == NULL ) return 0;

#ifdef WIN32
	return _strtoui64(pszText, NULL, 10);
#else
	return strtoull(pszText, NULL, 10);
#endif
}

//***************************************************************************
//
uint32 GetUInt32(const wchar_t* pwszText)
{
	if( pwszText == NULL ) return 0;

	return wcstoul(pwszText, NULL, 10);
}

//***************************************************************************
//
uint64 GetUInt64(const wchar_t* pwszText)
{
	if( pwszText == NULL ) return 0;

#ifdef WIN32
	return _wcstoui64(pwszText, NULL, 10);
#else
	return wcstoull(pwszText, NULL, 10);
#endif
}
