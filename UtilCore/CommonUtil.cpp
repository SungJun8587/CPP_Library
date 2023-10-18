
//***************************************************************************
// ClusteredMap.cpp : implementation of the CClusteredMap class.
//
//***************************************************************************

#include "pch.h"
#include "CommonUtil.h"

//***************************************************************************
//
void GetDBDSNString(TCHAR* ptszDSN, const DB_CLASS dbClass, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
{
	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={SQL Server};SERVER=%s,%u;Database=%s;UID=%s;PWD=%s"),
						 ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;
		case DB_CLASS::DB_MYSQL:
#ifdef _UNICODE	
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={MySQL ODBC 8.1 UNICODE Driver};SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;"),
						 ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
#else
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={MySQL ODBC 8.1 ANSI Driver};SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;"),
						 m_tszDBHost, m_nPort, m_tszDBName, m_tszDBUserId, m_tszDBPasswd);
#endif
			break;
		case DB_CLASS::DB_ORACLE:
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=Microsoft ODBC for Oracle};SERVER=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=%s)(PORT=%u))(CONNECT_DATA=(SID=%s)));Uid=%s;Pwd=%s"),
						 ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
	}
}

//***************************************************************************
//
DB_CLASS GetInt8ToDBClass(uint8 num)
{
	switch( num )
	{
		case 1:
			return DB_CLASS::DB_MSSQL;
			break;
		case 2:
			return DB_CLASS::DB_MYSQL;
			break;
		case 3:
			return DB_CLASS::DB_ORACLE;
			break;
		default:
			return DB_CLASS::DB_NONE;
			break;
	}
}

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
