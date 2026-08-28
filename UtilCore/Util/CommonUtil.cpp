
//***************************************************************************
// CommonUtil.cpp : implementation of the CommonUtil Functions.
//
//***************************************************************************

#include "pch.h"
#include "CommonUtil.h"

//***************************************************************************
// @brief 데이터베이스 종류에 따른 ODBC DSN 연결 문자열을 생성합니다.
// @param ptszDSN 생성된 DSN 문자열이 저장될 버퍼
// @param dbClass 데이터베이스 종류 (MSSQL, MYSQL, ORACLE 등)
// @param ptszDSNDriver ODBC 드라이버 이름
// @param ptszDBHost 데이터베이스 호스트 주소
// @param nPort 데이터베이스 포트 번호
// @param ptszDBUserId 데이터베이스 사용자 ID
// @param ptszDBPasswd 데이터베이스 비밀번호
// @param ptszDBName 데이터베이스 이름
//***************************************************************************
void GetDBDSNString(TCHAR* ptszDSN, const EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
{
	unsigned int portToUse = 0;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// {SQL Server}
			// {ODBC Driver 18 for SQL Server} 설치 후 "신뢰할 수 없는 기관에서 인증서 체인을 발급했습니다" 오류 발생(Encrypt=Optional; 추가)
			// 참고 사항 : {ODBC Driver 18 for SQL Server} ODBC 드라이버 이용해 접속하면 메모릭 릭 16바이트 발생
			//	_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,%u;Database=%s;UID=%s;PWD=%s;Encrypt=no;TrustServerCertificate=yes;"),
			//		ptszDSNDriver, ptszDBHost, nPort, ptszDBName, ptszDBUserId, ptszDBPasswd);
			portToUse = (nPort == 0) ? 1433 : nPort;

			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,%u;Database=%s;UID=%s;PWD=%s;"),
				_T("{SQL Server}"), ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;
		case EDBClass::MYSQL:
			// ANSI : {MySQL ODBC 8.1 ANSI Driver}
			// UNICODE : {MySQL ODBC 8.1 UNICODE Driver}
			portToUse = (nPort == 0) ? 3306 : nPort;

			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;MULTI_STATEMENTS=1"),
				ptszDSNDriver, ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;
		case EDBClass::ORACLE:
			portToUse = (nPort == 0) ? 1521 : nPort;

			_sntprintf_s(ptszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=%s;DBQ=%s:%d/%s;UID=%s;PWD=%s;"),
				ptszDSNDriver, ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
		}
}

//***************************************************************************
// @brief 데이터베이스 종류에 따른 ADO Connection String을 생성합니다.
// @param ptszConnStr 생성된 연결 문자열이 저장될 버퍼
// @param dbClass 데이터베이스 종류 (MSSQL, MYSQL, ORACLE 등)
// @param ptszDBHost 데이터베이스 호스트 주소
// @param nPort 데이터베이스 포트 번호
// @param ptszDBUserId 데이터베이스 사용자 ID
// @param ptszDBPasswd 데이터베이스 비밀번호
// @param ptszDBName 데이터베이스 이름
//***************************************************************************
void GetADOConnectionString(TCHAR* ptszConnStr, const EDBClass dbClass, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
{
	unsigned int portToUse = 0;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// SQL Server OLE DB Provider 사용
			portToUse = (nPort == 0) ? 1433 : nPort;

			_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("Provider=SQLOLEDB;Data Source=%s,%u;Initial Catalog=%s;User ID=%s;Password=%s;"),
				ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;

		case EDBClass::MYSQL:
			// MySQL OLE DB Provider (MyOLEDB) 사용
			portToUse = (nPort == 0) ? 3306 : nPort;

			_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("Provider=MySQLProv;Data Source=%s;Port=%u;Database=%s;User ID=%s;Password=%s;Option=3;"),
				ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;

		case EDBClass::ORACLE:
			// Oracle OLE DB Provider 사용
			// Data Source는 TNS 명칭 또는 [호스트]:[포트]/[서비스명] 형식
			portToUse = (nPort == 0) ? 1521 : nPort;

			_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("Provider=OraOLEDB.Oracle;Data Source=%s:%u/%s;User ID=%s;Password=%s;"),
				ptszDBHost, portToUse, ptszDBName, ptszDBUserId, ptszDBPasswd);
			break;

		default:
			_tcscpy_s(ptszConnStr, DATABASE_DSN_STRLEN, _T(""));
			break;
	}
}

//***************************************************************************
// @brief Redis 연결 문자열(URI)을 생성합니다.
// @param ptszConnStr 생성된 연결 문자열이 저장될 버퍼
// @param ptszDBHost 데이터베이스 호스트 주소
// @param nPort 데이터베이스 포트 번호 (0 전달 시 기본값 6379 사용)
// @param ptszDBUserId 데이터베이스 사용자 ID (Redis 6.0+ ACL 기능 사용 시)
// @param ptszDBPasswd 데이터베이스 비밀번호
// @param ptszDBName Redis DB 인덱스 문자열 (예: "0", "1" 등. NULL 또는 빈값 전달 시 0)
//***************************************************************************
void GetRedisConnectionString(TCHAR* ptszConnStr, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
{
	ptszConnStr[0] = _T('\0');

	// 기본 포트 설정 (0 전달 시 6379)
	const unsigned int portToUse = (nPort == 0) ? 6379 : nPort;

	// Redis DB 인덱스 처리 (ptszDBName 문자열을 숫자 인덱스로 변환)
	unsigned int nDBIndex = 0;
	if( ptszDBName != NULL && _tcslen(ptszDBName) > 0 )
	{
		nDBIndex = (unsigned int)_ttoi(ptszDBName);
	}

	const BOOL bHasPasswd = (ptszDBPasswd != NULL && _tcslen(ptszDBPasswd) > 0);
	const BOOL bHasUser = (ptszDBUserId != NULL && _tcslen(ptszDBUserId) > 0);

	if( bHasPasswd )
	{
		if( bHasUser )
		{
			// Redis 6.0+ ACL 인증 (사용자 ID + 비밀번호)
			// 형식: redis://user:password@host:port/db_index
			_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("redis://%s:%s@%s:%u/%u"), 
				ptszDBUserId, ptszDBPasswd, ptszDBHost, portToUse, nDBIndex);
		}
		else
		{
			// 일반 AUTH 인증 (비밀번호만 존재)
			// 형식: redis://:password@host:port/db_index
			_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("redis://:%s@%s:%u/%u"), 
				ptszDBPasswd, ptszDBHost, portToUse, nDBIndex);
		}
	}
	else
	{
		// 인증 정보가 없는 경우
		// 형식: redis://host:port/db_index
		_sntprintf_s(ptszConnStr, DATABASE_DSN_STRLEN, _TRUNCATE, _T("redis://%s:%u/%u"), 
			ptszDBHost, portToUse, nDBIndex);
	}
}

//***************************************************************************
// @brief 1바이트 숫자 코드를 데이터베이스 종류 열거형(EDBClass)으로 변환합니다.
// @param num 데이터베이스 분류 번호 (1: MSSQL, 2: MYSQL, 3: ORACLE)
// @return 변환된 EDBClass 값
//***************************************************************************
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
// @brief ANSI 문자열을 32비트 부호 없는 정수로 변환합니다.
// @param pszText 변환할 문자열 포인터 (ANSI)
// @return 변환된 uint32 값 (실패 시 0)
//***************************************************************************
uint32 GetUInt32(const char* pszText)
{
	if( pszText == NULL ) return 0;

	return strtoul(pszText, NULL, 10);
}

//***************************************************************************
// @brief ANSI 문자열을 64비트 부호 없는 정수로 변환합니다.
// @param pszText 변환할 문자열 포인터 (ANSI)
// @return 변환된 uint64 값 (실패 시 0)
//***************************************************************************
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
// @brief 유니코드 와이드 문자열을 32비트 부호 없는 정수로 변환합니다.
// @param pwszText 변환할 와이드 문자열 포인터 (Unicode)
// @return 변환된 uint32 값 (실패 시 0)
//***************************************************************************
uint32 GetUInt32(const wchar_t* pwszText)
{
	if( pwszText == NULL ) return 0;

	return wcstoul(pwszText, NULL, 10);
}

//***************************************************************************
// @brief 유니코드 와이드 문자열을 64비트 부호 없는 정수로 변환합니다.
// @param pwszText 변환할 와이드 문자열 포인터 (Unicode)
// @return 변환된 uint64 값 (실패 시 0)
//***************************************************************************
uint64 GetUInt64(const wchar_t* pwszText)
{
	if( pwszText == NULL ) return 0;

#ifdef WIN32
	return _wcstoui64(pwszText, NULL, 10);
#else
	return wcstoull(pwszText, NULL, 10);
#endif
}