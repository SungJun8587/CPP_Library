
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
	memset(m_tszLastError, 0, sizeof(m_tszLastError));
}

CBaseODBC::CBaseODBC(const EDBClass dbClass, const TCHAR* ptszDSN, const bool bLoadExcelFile /*= false*/)
	: m_hEnv(SQL_NULL_HENV), m_hConn(SQL_NULL_HDBC), m_hStmt(SQL_NULL_HSTMT), m_nParamNum(0), m_nColNum(0)
	, m_bLoadExcelFile(bLoadExcelFile)
{
	m_DbClass = dbClass;
	_tcsncpy_s(m_tszDSN, _countof(m_tszDSN), ptszDSN, _TRUNCATE);

	memset(m_tszQueryInfo, 0, sizeof(m_tszQueryInfo));
	memset(m_tszLastError, 0, sizeof(m_tszLastError));
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

	int32		nConnDead = SQL_CD_FALSE;
	SQLINTEGER	nLen = 0;

	if( SQL_SUCCESS != SQLGetConnectAttr(m_hConn, SQL_ATTR_CONNECTION_DEAD, &nConnDead, SQL_IS_UINTEGER, &nLen) )
		return false;

	if( SQL_CD_TRUE == nConnDead )
		return false;

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::DBMSInfo(TCHAR* ptszServerName, TCHAR* ptszDBMSName, TCHAR* ptszDBMSVersion)
{
	SQLRETURN nRet;
	SQLSMALLINT buffSize;

#ifdef _UNICODE	
	nRet = SQLGetInfo(m_hConn, SQL_SERVER_NAME, (SQLWCHAR*)ptszServerName, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;

	nRet = SQLGetInfo(m_hConn, SQL_DBMS_NAME, (SQLWCHAR*)ptszDBMSName, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;

	nRet = SQLGetInfo(m_hConn, SQL_DBMS_VER, (SQLWCHAR*)ptszDBMSVersion, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;
#else
	nRet = SQLGetInfo(m_hConn, SQL_SERVER_NAME, (SQLCHAR*)ptszServerName, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;

	nRet = SQLGetInfo(m_hConn, SQL_DBMS_NAME, (SQLCHAR*)ptszDBMSName, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;

	nRet = SQLGetInfo(m_hConn, SQL_DBMS_VER, (SQLCHAR*)ptszDBMSVersion, (SQLSMALLINT)DATABASE_BUFFER_SIZE, (SQLSMALLINT*)&buffSize);
	if( SQL_SUCCESS != nRet ) return false;
#endif	

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
bool CBaseODBC::BindParamInput(const TCHAR* ptszValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr((TCHAR*)ptszValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
		0, dbParam.m_ptrBuff, dbParam.m_nBuffSize, NULL);
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
bool CBaseODBC::BindParamInput(int32 iParamIndex, const TCHAR* ptszValue, SQLLEN& lRetSize)
{
	SQLSMALLINT nCDataType = SQL_C_DEFAULT;
	SQLSMALLINT nSqlDataType = SQL_VARCHAR;
	SQLULEN nBuffSize = 0;

#ifdef _UNICODE
	nBuffSize = static_cast<SQLULEN>((::wcslen(ptszValue) + 1) * 2);
	nCDataType = SQL_C_WCHAR;
	if( nBuffSize > DATABASE_WVARCHAR_MAX )
		nSqlDataType = SQL_WLONGVARCHAR;
	else nSqlDataType = SQL_WVARCHAR;
#else
	nBuffSize = static_cast<SQLULEN>((::strlen(ptszValue) + 1));
	nCDataType = SQL_C_CHAR;
	if( nBuffSize > DATABASE_VARCHAR_MAX )
		nSqlDataType = SQL_LONGVARCHAR;
	else nSqlDataType = SQL_VARCHAR;
#endif

	lRetSize = SQL_NTSL;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, nCDataType, nSqlDataType, nBuffSize,
		0, (SQLPOINTER)ptszValue, 0, &lRetSize);
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
bool CBaseODBC::BindParamInput(int32 iParamIndex, const BYTE* pbData, int32 iSize, SQLLEN& lRetSize)
{
	SQLSMALLINT cType = SQL_C_BINARY;
	SQLSMALLINT sqlType;

	if( pbData == nullptr )
	{
		lRetSize = SQL_NULL_DATA;
		iSize = 1;
	}
	else
		lRetSize = iSize;

	if( iSize > DATABASE_BINARY_MAX )
		sqlType = SQL_LONGVARBINARY;
	else sqlType = SQL_BINARY;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, cType, sqlType, iSize, 0, (BYTE*)pbData, iSize, &lRetSize);
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
bool CBaseODBC::BindParamOutput(TCHAR* ptszValue, int32& iBuffSize)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(ptszValue, iBuffSize);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
		0, dbParam.m_ptrBuff, dbParam.m_nBuffSize, (SQLLEN*)&dbParam.m_nBuffSize);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, iBuffSize, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
bool CBaseODBC::BindParamOutput(int32 iParamIndex, TCHAR* ptszValue, int32& iBuffSize, SQLLEN& lRetSize)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(ptszValue, iBuffSize);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
		0, dbParam.m_ptrBuff, dbParam.m_nBuffSize, &lRetSize);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, iBuffSize, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
bool CBaseODBC::BindParamOutput(int32 iParamIndex, BYTE* pbData, int32 iSize, SQLLEN& lRetSize)
{
	SQLSMALLINT cType = SQL_C_BINARY;
	SQLSMALLINT sqlType;

	if( pbData == nullptr )
	{
		lRetSize = SQL_NULL_DATA;
		iSize = 1;
	}
	else
		lRetSize = iSize;

	if( iSize > DATABASE_BINARY_MAX )
		sqlType = SQL_LONGVARBINARY;
	else sqlType = SQL_BINARY;

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, cType, sqlType, iSize, 0, pbData, iSize, &lRetSize);
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
bool CBaseODBC::BindCol(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* StrLen_or_Ind)
{
	int32 iBufferSize = 0;

	SQLRETURN nRet = SQLBindCol(m_hStmt, ColumnNumber, TargetType, TargetValue, BufferLength, StrLen_or_Ind);
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
bool CBaseODBC::BindCol(TCHAR* ptszValue, int32& iBuffSize)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(ptszValue, iBuffSize);

	SQLRETURN nRet = SQLBindCol(m_hStmt, ++m_nColNum, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, iBuffSize, tszMessage);
		return false;
	}
	iBuffSize = dbCol.m_nBuffSize;

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::BindCol(int32 iColIndex, TCHAR* ptszValue, int32& iBuffSize, SQLLEN& lRetSize)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(ptszValue, iBuffSize);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, &lRetSize);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%s,%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, ptszValue, iBuffSize, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
bool CBaseODBC::BindCol(int32 iColIndex, SQLSMALLINT targetType, int64& value, SQLLEN& lRetSize)
{
	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, targetType, &value, sizeof(uint64), &lRetSize);
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
bool CBaseODBC::BindCol(int32 iColIndex, SQLSMALLINT targetType, uint64& value, SQLLEN& lRetSize)
{
	int32 iBufferSize = 0;

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, targetType, &value, iBufferSize, &lRetSize);
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
bool CBaseODBC::GetData(int32 iColNum, TCHAR* ptszData, int32& iBufSize)
{
	SQLLEN		lRetSize;
	SQLRETURN	nRet;

#ifdef _UNICODE	
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_WCHAR, (SQLWCHAR*)ptszData, iBufSize, &lRetSize);
#else
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_CHAR, (SQLCHAR*)ptszData, iBufSize, &lRetSize);
#endif

	if( lRetSize == SQL_NO_TOTAL || lRetSize == SQL_NULL_DATA )
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