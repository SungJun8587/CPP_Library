
//***************************************************************************
// CommonUtil.cpp : implementation of the CommonUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "CommonUtil.h"

//***************************************************************************
//
void GetDBDSNString(TCHAR* ptszDSN, const EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
{
	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// {SQL Server}
			// {ODBC Driver 18 for SQL Server} 설치 후 "신뢰할 수 없는 기관에서 인증서 체인을 발급했습니다" 오류 발생(Encrypt=Optional; 추가)
			// 참고 사항 : {ODBC Driver 18 for SQL Server} ODBC 드라이버 이용해 접속하면 메모릭 릭 16바이트 발생
			//	_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,%u;Database=%s;UID=%s;PWD=%s;Encrypt=no;TrustServerCertificate=yes;"),
			//		ptszDSNDriver, ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,%u;Database=%s;UID=%s;PWD=%s;"),
				_T("{SQL Server}"), ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;
		case EDBClass::MYSQL:
			// ANSI : {MySQL ODBC 8.1 ANSI Driver}
			// UNICODE : {MySQL ODBC 8.1 UNICODE Driver}
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;MULTI_STATEMENTS=1"),
				ptszDSNDriver, ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;
		case EDBClass::ORACLE:
			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;DBQ=%s:%d/%s;UID=%s;PWD=%s;"),
				ptszDSNDriver, ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
	}
}

//***************************************************************************
//
EDBClass GetInt8ToDBClass(uint8 num)
{
	switch( num )
	{
		case 1:
			return EDBClass::MSSQL;
			break;
		case 2:
			return EDBClass::MYSQL;
			break;
		case 3:
			return EDBClass::ORACLE;
			break;
		default:
			return EDBClass::NONE;
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
