
//***************************************************************************
// BaseODBC.cpp: implementation of the CBaseODBC class.
//
//***************************************************************************

#include "pch.h"
#include "BaseODBC.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CBaseODBC::CBaseODBC(const EDBClass dbClass, const bool bLoadExcelFile /*= false*/)
	: m_hEnv(SQL_NULL_HENV), m_hConn(SQL_NULL_HDBC), m_hStmt(SQL_NULL_HSTMT), m_DbClass(dbClass), m_nParamNum(0), m_nColNum(0)
	, m_bLoadExcelFile(bLoadExcelFile)
{
	memset(m_tszDSN, 0, sizeof(m_tszDSN));
	memset(m_tszQueryInfo, 0, sizeof(m_tszQueryInfo));
}

CBaseODBC::CBaseODBC(const EDBClass dbClass, const TCHAR* ptszDSN, const bool bLoadExcelFile /*= false*/)
	: m_hEnv(SQL_NULL_HENV), m_hConn(SQL_NULL_HDBC), m_hStmt(SQL_NULL_HSTMT), m_nParamNum(0), m_nColNum(0)
	, m_bLoadExcelFile(bLoadExcelFile)
{
	m_DbClass = dbClass;
	_tcsncpy_s(m_tszDSN, _countof(m_tszDSN), ptszDSN, _TRUNCATE);

	memset(m_tszQueryInfo, 0, sizeof(m_tszQueryInfo));
}

//***************************************************************************
//
CBaseODBC::~CBaseODBC()
{
	Disconnect();
}

//***************************************************************************
//
bool CBaseODBC::InitStmtHandle(const int64 lQueryTimeOut)
{
	if( SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_STMT, m_hConn, (SQLHSTMT*)&m_hStmt) )
		return false;

	if( SQL_SUCCESS != SQLSetStmtAttr(m_hStmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_READ_ONLY, 0) )
		return false;

	if( false == m_bLoadExcelFile )
	{
		if( SQL_SUCCESS != SQLSetStmtAttr(m_hStmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)lQueryTimeOut, 0) )
			return false;
	}

	return true;
}

//***************************************************************************
//
void CBaseODBC::FreeStmt(SQLUSMALLINT Option)
{
	if( m_hStmt != SQL_NULL_HSTMT )
	{
		// 함수는 ODBC API에서 SQL 문(statement)과 관련된 리소스를 해제하거나 문 핸들의 상태를 재설정하는 데 사용
		//	SQLHSTMT StatementHandle : 처리할 SQL 문 핸들
		//	SQLUSMALLINT Option : 문 핸들에서 수행할 작업의 종류를 지정
		//		- SQL_CLOSE : 현재 열린 커서를 닫음(결과 집합 처리가 끝난 후, 새로운 SQL 문 실행 전, 트랜잭션 종료 후)
		//		- SQL_UNBIND : 열 바인딩을 해제(SQLBindCol 함수 초기화)
		//		- SQL_RESET_PARAMS : 매개변수 바인딩을 재설정(SQLBindParameter 함수 초기화)
		//		- SQL_DROP : 문 핸들을 완전히 제거
		SQLFreeStmt(m_hStmt, Option);
	}
}

//***************************************************************************
//
void CBaseODBC::ClearStmt(void)
{
	FreeStmt(SQL_RESET_PARAMS);
	FreeStmt(SQL_UNBIND);
	FreeStmt(SQL_CLOSE);

	_tcsncpy_s(m_tszQueryInfo, SQL_MAX_MESSAGE_LENGTH, _T(""), _TRUNCATE);
	m_nParamNum = 0;
	m_nColNum = 0;
}

//***************************************************************************
//
void CBaseODBC::ResetParamStmt(void)
{
	// 매개변수 바인딩을 재설정(SQLBindParameter 함수 초기화)
	FreeStmt(SQL_RESET_PARAMS);
	m_nParamNum = 0;
}

//***************************************************************************
//
void CBaseODBC::UnBindColStmt(void)
{
	// 열 바인딩을 해제(SQLBindCol 함수 초기화)
	FreeStmt(SQL_UNBIND);
	m_nColNum = 0;
}

//***************************************************************************
// 1. SQLAllocHandle(SQL_HANDLE_ENV) : ODBC 환경 핸들 생성
// 2. SQLAllocHandle(SQL_HANDLE_DBC) : 데이터베이스 연결 핸들 생성
// 3. SQLDriverConnect() : 데이터베이스와의 연결
// 4. SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT) : 자동 커밋 모드 설정
// 5. SQLAllocHandle(SQL_HANDLE_STMT) : SQL 명령을 실행하거나 결과를 처리하기 위해 명령 핸들을 생성
bool CBaseODBC::Connect(const int64 lLoginTimeOut, const int64 lConnectionTimeOut)
{
	TCHAR tszOutConnStr[DATABASE_BUFFER_SIZE];
	TCHAR tszDriverVersion[256];
	SQLRETURN nRet;
	SQLSMALLINT nLen;

	if( m_hConn != SQL_NULL_HDBC ) return false;
	if( !m_tszDSN ) return false;

	try
	{
		if( SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv) )
			throw 0;

		// Set ODBC Version 3.0
		if( SQL_SUCCESS != SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER) )
			throw 0;

		if( SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hConn) )
			throw 0;

		if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)lLoginTimeOut, SQL_IS_INTEGER) )
			throw 0;

		if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)lConnectionTimeOut, SQL_IS_INTEGER) )
			throw 0;

		// - SQL_DRIVER_NOPROMPT : 사용자에게 프롬프트를 표시하지 않고 연결을 시도
		// - SQL_DRIVER_COMPLETE : 연결 문자열에 누락된 정보가 있을 경우 사용자에게 프롬프트를 표시하여 입력을 요청
		// - SQL_DRIVER_PROMPT : 항상 사용자에게 프롬프트를 표시하여 연결 정보를 입력하도록 요청
		// - SQL_DRIVER_COMPLETE_REQUIRED : 연결 문자열에 포함된 정보로만 연결을 시도하며, 누락된 정보가 있으면 프롬프트를 표시(단, 사용자가 필수 정보만 입력할 수 있도록 제한)
#ifdef _UNICODE
		nRet = SQLDriverConnect(m_hConn, nullptr, (SQLWCHAR*)m_tszDSN, SQL_NTS, (SQLWCHAR*)tszOutConnStr, DATABASE_BUFFER_SIZE, &nLen, SQL_DRIVER_NOPROMPT);
#else
		nRet = SQLDriverConnect(m_hConn, nullptr, (SQLCHAR*)m_tszDSN, SQL_NTS, (SQLCHAR*)tszOutConnStr, DATABASE_BUFFER_SIZE, &nLen, SQL_DRIVER_NOPROMPT);
#endif

		if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_DBC, m_hConn, tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
			LOG_INFO(_T("DSN : %s"), m_tszDSN);
			throw 0;
		}

		nRet = SQLGetInfo(m_hConn, SQL_DRIVER_VER, tszDriverVersion, sizeof(tszDriverVersion), nullptr);
		if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_DBC, m_hConn, tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
			LOG_INFO(_T("DSN : %s"), m_tszDSN);
			throw 0;
		}

		TCHAR tszServerName[128] = { 0, };
		TCHAR tszDBMSName[128] = { 0, };
		TCHAR tszDBMSVersion[128] = { 0, };
		TCHAR tszCharacterSetName[128] = { 0, };

		bool bServerResult = GetServerName(tszServerName, _countof(tszServerName));
		bool bNameResult = GetDBMSName(tszDBMSName, _countof(tszDBMSName));
		bool bVersionResult = GetDBMSVersion(tszDBMSVersion, _countof(tszDBMSVersion));
		bool bCharsetResult = GetServerCharacterSet(tszCharacterSetName, _countof(tszCharacterSetName));
		if( bServerResult && bNameResult && bVersionResult && bCharsetResult )
		{
			LOG_DEBUG(_T("%s, Server: %s, DBMS: %s, Version: %s, Charset: %s"), __TFUNCTION__, tszServerName, tszDBMSName, tszDBMSVersion, tszCharacterSetName);
		}
		else
		{
			LOG_INFO(_T("DSN : %s"), m_tszDSN);
			throw 0;
		}
	}
	catch( ... )
	{
		Disconnect();
		return false;
	}

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::Disconnect()
{
	SQLRETURN nRet;

	if( m_hStmt != SQL_NULL_HSTMT )
	{
		nRet = SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
		m_hStmt = SQL_NULL_HSTMT;
	}

	if( m_hConn != SQL_NULL_HDBC )
	{
		nRet = SQLDisconnect(m_hConn);
		if( nRet == SQL_ERROR )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_DBC, m_hConn, tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		}

		nRet = SQLFreeHandle(SQL_HANDLE_DBC, m_hConn);
		if( nRet == SQL_ERROR )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_DBC, m_hConn, tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		}

		m_hConn = SQL_NULL_HDBC;
	}

	if( m_hEnv != SQL_NULL_HENV )
	{
		SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
		m_hEnv = SQL_NULL_HENV;
	}

	LOG_DEBUG(_T("%s"), __TFUNCTION__);

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::IsConnected()
{
	if( m_hConn == SQL_NULL_HDBC ) return false;

	SQLUINTEGER	uiConnDead = SQL_CD_FALSE;
	SQLINTEGER	nLen = 0;

	if( SQL_SUCCESS != SQLGetConnectAttr(m_hConn, SQL_ATTR_CONNECTION_DEAD, &uiConnDead, SQL_IS_UINTEGER, &nLen) )
		return false;

	if( SQL_CD_TRUE == uiConnDead )
		return false;

	return true;
}

//***************************************************************************
//
/// @brief ODBC 연결에서 서버 이름 정보를 가져옴
/// @param ptszServerName - 서버 이름을 저장할 버퍼 (출력)
/// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetServerName(TCHAR* ptszServerName, int32 nBufferLength)
{
	if( ptszServerName == nullptr || nBufferLength <= 0 ) return false;

	SQLRETURN nRet;
	SQLSMALLINT buffSize = 0;

#ifdef _UNICODE	
	nRet = SQLGetInfo(m_hConn, SQL_SERVER_NAME, (SQLWCHAR*)ptszServerName, (SQLSMALLINT)nBufferLength, &buffSize);
#else
	nRet = SQLGetInfo(m_hConn, SQL_SERVER_NAME, (SQLCHAR*)ptszServerName, (SQLSMALLINT)nBufferLength, &buffSize);
#endif	

	return (SQL_SUCCESS == nRet || SQL_SUCCESS_WITH_INFO == nRet);
}

//***************************************************************************
//
/// @brief ODBC 연결에서 DBMS 이름 정보를 가져옴
/// @param ptszDBMSName - DBMS 이름을 저장할 버퍼 (출력)
/// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetDBMSName(TCHAR* ptszDBMSName, int32 nBufferLength)
{
	if( ptszDBMSName == nullptr || nBufferLength <= 0 ) return false;

	SQLRETURN nRet;
	SQLSMALLINT buffSize = 0;

#ifdef _UNICODE	
	nRet = SQLGetInfo(m_hConn, SQL_DBMS_NAME, (SQLWCHAR*)ptszDBMSName, (SQLSMALLINT)nBufferLength, &buffSize);
#else
	nRet = SQLGetInfo(m_hConn, SQL_DBMS_NAME, (SQLCHAR*)ptszDBMSName, (SQLSMALLINT)nBufferLength, &buffSize);
#endif	

	return (SQL_SUCCESS == nRet || SQL_SUCCESS_WITH_INFO == nRet);
}

//***************************************************************************
//
/// @brief ODBC 연결에서 DBMS 버전 정보를 가져옴
/// @param ptszDBMSVersion - DBMS 버전 정보를 저장할 버퍼 (출력)
/// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetDBMSVersion(TCHAR* ptszDBMSVersion, int32 nBufferLength)
{
	if( ptszDBMSVersion == nullptr || nBufferLength <= 0 ) return false;

	SQLRETURN nRet;
	SQLSMALLINT buffSize = 0;

#ifdef _UNICODE	
	nRet = SQLGetInfo(m_hConn, SQL_DBMS_VER, (SQLWCHAR*)ptszDBMSVersion, (SQLSMALLINT)nBufferLength, &buffSize);
#else
	nRet = SQLGetInfo(m_hConn, SQL_DBMS_VER, (SQLCHAR*)ptszDBMSVersion, (SQLSMALLINT)nBufferLength, &buffSize);
#endif	

	return (SQL_SUCCESS == nRet || SQL_SUCCESS_WITH_INFO == nRet);
}

//***************************************************************************
//
/// @brief 각 DBMS별 서버 캐릭터셋(인코딩) 정보를 조회
/// @param ptszCharset - 캐릭터셋 이름을 저장할 버퍼 (출력)
/// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetServerCharacterSet(TCHAR* ptszCharset, int32 nBufferLength)
{
	if( ptszCharset == nullptr || nBufferLength <= 0 )
		return false;

	TCHAR tszQuery[256] = { 0, };

	// DBMS 종류에 따른 조회 쿼리 설정
	switch( m_DbClass )
	{
		case EDBClass::MSSQL:
			// SQL Server의 서버 전역 캐릭터셋/정렬 방식(Collation) 조회
			_stprintf_s(tszQuery, _T("SELECT @@SERVERNAME, SERVERPROPERTY('Collation')")); // 혹은 DATABASEPROPERTYEX 등 활용 가능
			// 혹은 가장 대표적인 서버 Collation 조회 쿼리:
			// _stprintf_s(szQuery, _T("SELECT CONVERT(varchar(128), DATABASEPROPERTYEX(DB_NAME(), 'Collation'))"));
			break;

		case EDBClass::MYSQL:
			// MySQL의 서버 캐릭터셋(character_set_server) 조회
			_stprintf_s(tszQuery, _T("SHOW VARIABLES LIKE 'character_set_server'"));
			break;

		case EDBClass::ORACLE:
			// Oracle의 데이터베이스 캐릭터셋 정보 조회
			_stprintf_s(tszQuery, _T("SELECT * FROM NLS_DATABASE_PARAMETERS WHERE PARAMETER = 'NLS_CHARACTERSET'"));
			break;

		default:
			return false;
	}

	// 쿼리 직접 실행
	if( !ExecDirect(tszQuery) )
	{
		ClearStmt();
		return false;
	}

	// 결과 셋 페치 및 데이터 바인딩 로직 처리
	// (기존 클래스에 구현된 Fetch와 BindCol 방식을 사용하여 첫 번째 결과 행의 값을 가져옴)
	if( Fetch() )
	{
		// 예시: 첫 번째 컬럼 혹은 지정된 컬럼 결과값을 ptszCharset 버퍼로 바인딩 및 가져오기
		// (사용하시는 구현 형태에 맞추어 BindCol 혹은 GetData 호출부 적용)
		if( !GetData(2, ptszCharset, nBufferLength) ) // MySQL의 경우 SHOW VARIABLES는 2번째 열에 값이 나옴
		{
			ClearStmt();
			return false;
		}
	}

	ClearStmt();
	
	return true;
}

//***************************************************************************
//	
bool CBaseODBC::PrepareQuery(const TCHAR* ptszQueryInfo)
{
	SQLRETURN nRet;

	if( m_hConn == SQL_NULL_HDBC ) return false;
	if( m_hStmt == SQL_NULL_HSTMT )
	{
		if( SQLAllocHandle(SQL_HANDLE_STMT, m_hConn, (SQLHSTMT*)&m_hStmt) != SQL_SUCCESS )
		{
			return false;
		}
	}

	ClearStmt();

	_tcsncpy_s(m_tszQueryInfo, SQL_MAX_MESSAGE_LENGTH, ptszQueryInfo, _TRUNCATE);

#ifdef _UNICODE	
	nRet = SQLPrepare(m_hStmt, (SQLWCHAR*)ptszQueryInfo, SQL_NTSL);
#else
	nRet = SQLPrepare(m_hStmt, (SQLCHAR*)ptszQueryInfo, SQL_NTSL);
#endif	

	if( SQL_SUCCESS != nRet && SQL_SUCCESS_WITH_INFO != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::Execute()
{
	SQLRETURN nRet;

	if( m_hConn == SQL_NULL_HDBC ) return false;
	if( m_hStmt == SQL_NULL_HSTMT )
	{
		if( SQLAllocHandle(SQL_HANDLE_STMT, m_hConn, (SQLHSTMT*)&m_hStmt) != SQL_SUCCESS )
		{
			return false;
		}
	}

	nRet = SQLExecute(m_hStmt);
	if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO && nRet != SQL_NO_DATA )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		TCHAR	tszSQLState[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage, tszSQLState);
		LOG_ERROR(_T("%s, QueryInfo[%s], Ret[%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, nRet, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::ExecDirect(const TCHAR* ptszQueryInfo)
{
	SQLRETURN nRet;

	if( m_hConn == SQL_NULL_HDBC ) return false;
	if( m_hStmt == SQL_NULL_HSTMT )
	{
		if( SQLAllocHandle(SQL_HANDLE_STMT, m_hConn, (SQLHSTMT*)&m_hStmt) != SQL_SUCCESS )
		{
			return false;
		}
	}

#ifdef _UNICODE	
	nRet = SQLExecDirect(m_hStmt, (SQLWCHAR*)ptszQueryInfo, SQL_NTS);
#else
	nRet = SQLExecDirect(m_hStmt, (SQLCHAR*)ptszQueryInfo, SQL_NTS);
#endif	

	if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO && nRet != SQL_NO_DATA )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], Ret[%d], ErrorMsg : %s"), __TFUNCTION__, ptszQueryInfo, nRet, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
// 1. AutoCommitOff
// 2. BulkSetStmtAttr
// 3. ExecDirect 
// 4. BindCol
// 5. BulkOperations
// 6. Commit
bool CBaseODBC::BulkOperations(SQLSMALLINT operation)
{
	// 테이블의 여러 행에 대해 일괄 작업(bulk operations)을 수행하는 데 사용
	//  SQLHSTMT     StatementHandle : SQL 문(statement) 핸들
	//	SQLUSMALLINT Operation : 수행할 작업의 유형을 지정
	//		- SQL_ADD : 새 행을 삽입
	//		- SQL_UPDATE_BY_BOOKMARK : 북마크(bookmark)를 기반으로 특정 행을 업데이트
	//		- SQL_DELETE_BY_BOOKMARK : 북마크(bookmark)를 기반으로 특정 행을 삭제
	//		- SQL_FETCH_BY_BOOKMARK : 북마크(bookmark)를 기반으로 특정 행을 가져옴
	SQLRETURN nRet = SQLBulkOperations(m_hStmt, operation);
	if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, Ret[%d], ErrorMsg : %s"), __TFUNCTION__, nRet, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseODBC::SetStmtAttr(SQLINTEGER fAttribute, SQLPOINTER rgbValue, SQLINTEGER cbValueMax)
{
	SQLRETURN nRet = SQL_ERROR;

	if( m_hStmt != SQL_NULL_HSTMT )
	{
		// SQLINTEGER cbValueMax : 문자열일 경우 문자열의 길이를 지정. 문자열이 아닐 경우 0 또는 무시.
		nRet = SQLSetStmtAttr(m_hStmt, fAttribute, rgbValue, cbValueMax);
		if( nRet != SQL_SUCCESS )
			return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseODBC::AllSets(LONG_PTR nQueryResultRecordSize, LONG_PTR nMaxRowSize)
{
	SQLRETURN nRet = SQL_ERROR;

	if( m_hStmt != SQL_NULL_HSTMT )
	{
		// 연결된 문에서 SQLFetch 또는 SQLFetchScroll을 호출할 때 사용할 바인딩 방향을 설정하는 SQLULEN 값. 
		// 값을 SQL_BIND_BY_COLUMN 설정하여 열 단위 바인딩을 선택. 
		// 행 단위 바인딩은 값을 결과 열이 바인딩될 버퍼의 길이 또는 버퍼의 길이로 설정하여 선택.
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER)nQueryResultRecordSize, 0);
		if( SQL_SUCCESS != nRet )
			return false;

		// 한 번에 처리할 행의 개수를 설정(대량 작업 시 이 속성을 늘려서 한 번에 여러 행을 처리할 수 있도록 설정)
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)nMaxRowSize, 0);
		if( SQL_SUCCESS != nRet )
			return false;

		/*
		// 각 행의 작업 상태를 저장할 배열의 포인터를 설정
		SQLUSMALLINT* pnStatus = new SQLUSMALLINT[nMaxRowSize];
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_STATUS_PTR, (SQLPOINTER)pnStatus, 0);
		if( SQL_SUCCESS != nRet )
			return false;
		*/

		// 작업된 행의 개수를 저장할 포인터를 설정
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROWS_FETCHED_PTR, (SQLPOINTER)m_nFetchedRows, 0);
		if( SQL_SUCCESS != nRet )
			return false;
	}

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::Fetch(void)
{
	SQLRETURN nRet = SQLFetch(m_hStmt);
	if( SQL_SUCCESS != nRet && SQL_SUCCESS_WITH_INFO != nRet )
	{
		if( SQL_NO_DATA == nRet )
			return false;

		if( false == m_bLoadExcelFile )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
			LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		}
		return false;
	}

	return true;
}

//***************************************************************************
//	
SQLRETURN CBaseODBC::GetFetch(void)
{
	return ::SQLFetch(m_hStmt);
}

//***************************************************************************
//	
SQLRETURN CBaseODBC::MoreResults(void)
{
	return SQLMoreResults(m_hStmt);
}

//***************************************************************************
//	
bool CBaseODBC::SetAutoCommitMode(SQLPOINTER valuePtr)
{
	if( SQL_NULL_HDBC == m_hConn )
		return false;

	// 자동 커밋 Off 모드 설정 여부(true 이면 Off 모드를 설정)
	//	- SQL_AUTOCOMMIT_ON(기본값) : 각 SQL 문 실행 후 자동으로 커밋.
	//	- SQL_AUTOCOMMIT_OFF : 트랜잭션 모드. 명시적으로 SQLEndTran을 호출하여 커밋 또는 롤백 필요.
	if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_AUTOCOMMIT, valuePtr, 0) )
		return false;

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::Commit()
{
	SQLRETURN	nRet;

	nRet = SQLEndTran(SQL_HANDLE_DBC, m_hConn, SQL_COMMIT);

	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}

//***************************************************************************
//	
bool CBaseODBC::Rollback()
{
	SQLRETURN	nRet;

	nRet = SQLEndTran(SQL_HANDLE_DBC, m_hConn, SQL_ROLLBACK);

	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}

//***************************************************************************
//	
/// @brief 일반적인 ODBC 매개변수(Parameter) 바인딩 함수
/// @param ipar - 매개변수 순번 (1부터 시작)
/// @param fParamType - 매개변수 타입 (SQL_PARAM_INPUT, SQL_PARAM_OUTPUT 등)
/// @param fCType - C 데이터 타입 (예: SQL_C_LONG, SQL_C_CHAR 등)
/// @param fSqlType - SQL 데이터 타입 (예: SQL_INTEGER, SQL_VARCHAR 등)
/// @param cbColDef - 대상 컬럼의 크기 또는 길이
/// @param ibScale - 소수점 자릿수
/// @param rgbValue - 값에 대한 버퍼 포인터
/// @param cbValueMax - 버퍼의 최대 바이트 크기
/// @param pcbValue - 데이터의 실제 길이나 상태를 반환받을 변수 포인터
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParameter(SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLULEN cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLLEN cbValueMax, SQLLEN* pcbValue)
{
	SQLRETURN nRet = SQLBindParameter(m_hStmt, ipar, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 자동 증가하는 순번을 사용하여 입력(Input) 문자열 매개변수를 바인딩
/// @param ptszValue - 바인딩할 문자열 값
/// @param plDataLength - 데이터 길이를 나타내는 포인터
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamInput(const TCHAR* ptszValue, SQLLEN* plDataLength)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr((TCHAR*)ptszValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, plDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 입력(Input) 문자열 매개변수를 바인딩 (문자열 길이에 따라 타입 동적 결정)
/// @param iParamIndex - 매개변수 순번 (인덱스)
/// @param ptszValue - 바인딩할 문자열 값
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamInput(int32 iParamIndex, const TCHAR* ptszValue, SQLLEN& lDataLength)
{
	SQLSMALLINT nCDataType = SQL_C_DEFAULT;
	SQLSMALLINT nSqlDataType = SQL_VARCHAR;
	SQLULEN ulColumnSize = 0;

#ifdef _UNICODE
	ulColumnSize = static_cast<SQLULEN>((::wcslen(ptszValue) + 1) * 2);
	nCDataType = SQL_C_WCHAR;
	if( ulColumnSize > DATABASE_WVARCHAR_MAX )
		nSqlDataType = SQL_WLONGVARCHAR;
	else nSqlDataType = SQL_WVARCHAR;
#else
	ulColumnSize = static_cast<SQLULEN>((::strlen(ptszValue) + 1));
	nCDataType = SQL_C_CHAR;
	if( ulColumnSize > DATABASE_VARCHAR_MAX )
		nSqlDataType = SQL_LONGVARCHAR;
	else nSqlDataType = SQL_VARCHAR;
#endif

	lDataLength = SQL_NTSL;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, nCDataType, nSqlDataType, ulColumnSize, 0, (SQLPOINTER)ptszValue, 0, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 입력(Input) 바이너리(Binary) 데이터 매개변수를 바인딩
/// @param iParamIndex - 매개변수 순번 (인덱스)
/// @param pbData - 바이너리 데이터 버퍼 포인터
/// @param nBufferLength - 버퍼 크기 (바이트 단위)
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamInput(int32 iParamIndex, const BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength)
{
	SQLSMALLINT cType = SQL_C_BINARY;
	SQLSMALLINT sqlType;
	SQLULEN ulColumnSize = 0;
	SQLLEN lBufferCapacity = 0;

	if( pbData == nullptr )
	{
		lDataLength = SQL_NULL_DATA;
		nBufferLength = 1;
	}
	else
		lDataLength = nBufferLength;

	if( nBufferLength > DATABASE_BINARY_MAX )
		sqlType = SQL_LONGVARBINARY;
	else sqlType = SQL_BINARY;

	ulColumnSize = nBufferLength;
	lBufferCapacity = nBufferLength;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, cType, sqlType, ulColumnSize, 0, (BYTE*)pbData, lBufferCapacity, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 자동 증가하는 순번을 사용하여 출력(Output) 문자열 매개변수를 바인딩
/// @param ptszValue - 출력값을 저장할 문자열 버퍼
/// @param nBufferLength - 버퍼 크기 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamOutput(TCHAR* ptszValue, int32& nBufferLength)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(ptszValue, nBufferLength);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, (SQLLEN*)&dbParam.m_lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, nBufferLength, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 출력(Output) 문자열 매개변수를 바인딩
/// @param iParamIndex - 매개변수 순번 (인덱스)
/// @param ptszValue - 출력값을 저장할 문자열 버퍼
/// @param nBufferLength - 버퍼 크기 참조
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamOutput(int32 iParamIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(ptszValue, nBufferLength);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, nBufferLength, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 출력(Output) 바이너리(Binary) 데이터 매개변수를 바인딩
/// @param iParamIndex - 매개변수 순번 (인덱스)
/// @param pbData - 출력 바이너리를 저장할 버퍼 포인터
/// @param nBufferLength - 버퍼 크기
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindParamOutput(int32 iParamIndex, BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength)
{
	SQLSMALLINT cType = SQL_C_BINARY;
	SQLSMALLINT sqlType;
	SQLULEN ulColumnSize = 0;
	SQLLEN lBufferCapacity = 0;

	if( pbData == nullptr )
	{
		lDataLength = SQL_NULL_DATA;
		nBufferLength = 1;
	}
	else
		lDataLength = nBufferLength;

	if( nBufferLength > DATABASE_BINARY_MAX )
		sqlType = SQL_LONGVARBINARY;
	else sqlType = SQL_BINARY;

	ulColumnSize = nBufferLength;
	lBufferCapacity = nBufferLength;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, cType, sqlType, ulColumnSize, 0, pbData, lBufferCapacity, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 결과 셋의 컬럼을 일반적인 방식으로 바인딩 (SQLBindCol 래퍼)
/// @param ColumnNumber - 컬럼 순번 (1부터 시작)
/// @param TargetType - 대상 C 데이터 타입
/// @param TargetValue - 결과 값을 받을 버퍼 포인터
/// @param BufferLength - 버퍼의 바이트 크기
/// @param plDataLength - 실제 데이터 길이를 받을 변수 포인터
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindCol(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* plDataLength)
{
	SQLRETURN nRet = SQLBindCol(m_hStmt, ColumnNumber, TargetType, TargetValue, BufferLength, plDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 자동 증가하는 순번을 사용하여 결과 셋의 문자열 컬럼을 바인딩
/// @param ptszValue - 문자열 결과 값을 받을 버퍼
/// @param nBufferLength - 버퍼 크기 참조
/// @param plDataLength - 실제 데이터 길이를 받을 변수 포인터
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindCol(TCHAR* ptszValue, int32& nBufferLength, SQLLEN* plDataLength)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(ptszValue, nBufferLength);

	SQLRETURN nRet = SQLBindCol(m_hStmt, ++m_nColNum, dbCol.m_nTargetType, dbCol.m_ptrBuffer, dbCol.m_nBufferLength, plDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, nBufferLength, tszMessage);
		return false;
	}
	nBufferLength = dbCol.m_nBufferLength;

	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 결과 셋의 문자열 컬럼을 바인딩
/// @param iColIndex - 컬럼 순번 (인덱스)
/// @param ptszValue - 문자열 결과 값을 받을 버퍼
/// @param nBufferLength - 버퍼 크기 참조
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindCol(int32 iColIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(ptszValue, nBufferLength);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, dbCol.m_nTargetType, dbCol.m_ptrBuffer, dbCol.m_nBufferLength, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, nBufferLength, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 결과 셋의 64비트 정수(int64) 컬럼을 바인딩
/// @param iColIndex - 컬럼 순번 (인덱스)
/// @param targetType - 대상 C 데이터 타입 (예: SQL_C_SBIGINT)
/// @param value - 값을 저장할 int64 변수 참조
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindCol(int32 iColIndex, SQLSMALLINT targetType, int64& value, SQLLEN& lDataLength)
{
	int32 nBufferLength = sizeof(int64);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, targetType, &value, nBufferLength, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], Value[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, value, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 결과 셋의 부호 없는 64비트 정수(uint64) 컬럼을 바인딩
/// @param iColIndex - 컬럼 순번 (인덱스)
/// @param targetType - 대상 C 데이터 타입
/// @param value - 값을 저장할 uint64 변수 참조
/// @param lDataLength - 데이터 길이 참조
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::BindCol(int32 iColIndex, SQLSMALLINT targetType, uint64& value, SQLLEN& lDataLength)
{
	int32 nBufferLength = sizeof(uint64);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, targetType, &value, nBufferLength, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], Value[%llu], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, value, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 대용량 데이터나 단일 컬럼 값을 직접 가져오는 함수 (SQLGetData 래퍼)
/// @param ColumnNumber - 컬럼 순번 (1부터 시작)
/// @param TargetType - 대상 C 데이터 타입
/// @param TargetValue - 값을 저장할 버퍼 포인터
/// @param BufferLength - 버퍼의 바이트 크기
/// @param plDataLength - 실제 데이터 길이를 받을 변수 포인터
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetData(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* plDataLength)
{
	SQLRETURN nRet = SQLGetData(m_hStmt, ColumnNumber, TargetType, TargetValue, BufferLength, plDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
/// @brief 인덱스를 지정하여 문자열 형태의 데이터를 직접 가져옴 (유니코드/멀티바이트 분기 처리 포함)
/// @param iColNum - 컬럼 순번 (인덱스)
/// @param ptszData - 문자열 데이터를 저장할 버퍼
/// @param nBufferLength - 버퍼 크기 참조 (입출력)
/// @return 성공 시 true, 실패 시 false
bool CBaseODBC::GetData(int32 iColNum, TCHAR* ptszData, int32& nBufferLength)
{
	SQLLEN		lDataLength;
	SQLRETURN	nRet;

#ifdef _UNICODE	
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_WCHAR, (SQLWCHAR*)ptszData, nBufferLength, &lDataLength);
#else
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_CHAR, (SQLCHAR*)ptszData, nBufferLength, &lDataLength);
#endif

	if( lDataLength == SQL_NO_TOTAL || lDataLength == SQL_NULL_DATA )
		return false;
	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}

//***************************************************************************
//	
short CBaseODBC::GetNumCols()
{
	short		temp;
	SQLRETURN	nRet;

	nRet = SQLNumResultCols(m_hStmt, &temp);

	return temp;
}

//***************************************************************************
//	
int64 CBaseODBC::RowCount()
{
	int64		i64RowCount;
	SQLRETURN	nRet;

	nRet = SQLRowCount(m_hStmt, (SQLLEN*)&i64RowCount);
	if( nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO )
		return i64RowCount;

	return -1;
}

//***************************************************************************
//	
long CBaseODBC::RowNumber()
{
	long lRowNumber = -1;

	SQLGetStmtAttr(m_hStmt, SQL_ATTR_ROW_NUMBER, &lRowNumber, SQL_IS_INTEGER, NULL);

	return lRowNumber;
}

//***************************************************************************
//	
bool CBaseODBC::DescribeCol(int32 iColNum, COL_DESCRIPTION& ColDescription)
{
	SQLRETURN	nRet;

#ifdef _UNICODE	
	nRet = SQLDescribeCol(m_hStmt, iColNum, (SQLWCHAR*)ColDescription.tszColName, DATABASE_COLUMN_NAME_STRLEN, &ColDescription.NameLength,
		&ColDescription.EDataType, (SQLULEN*)&ColDescription.dwColSize, &ColDescription.DigitSize, &ColDescription.Nullable);
#else
	nRet = SQLDescribeCol(m_hStmt, iColNum, (SQLCHAR*)ColDescription.tszColName, DATABASE_COLUMN_NAME_STRLEN, &ColDescription.NameLength,
		&ColDescription.EDataType, (SQLULEN*)&ColDescription.dwColSize, &ColDescription.DigitSize, &ColDescription.Nullable);
#endif

	nRet = SQLColAttribute(m_hStmt, iColNum, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, (SQLLEN*)&ColDescription.DispLength);

	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}