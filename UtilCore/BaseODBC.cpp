
//***************************************************************************
// BaseODBC.cpp: implementation of the CBaseODBC class.
//
//***************************************************************************

#include "pch.h"
#include "BaseODBC.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CBaseODBC::CBaseODBC(const DB_CLASS dbClass, const bool bLoadExcelFile /*= false*/)
	: m_hEnv(NULL), m_hConn(NULL), m_hStmt(NULL), m_DbClass(dbClass), m_nParamNum(0), m_nColNum(0), m_nFetchedRows(0)
	, m_bLoadExcelFile(bLoadExcelFile)
{
	memset(m_tszDSN, 0, sizeof(m_tszDSN));
	memset(m_tszQueryInfo, 0, sizeof(m_tszQueryInfo));
	memset(m_tszLastError, 0, sizeof(m_tszLastError));
}

CBaseODBC::CBaseODBC(const DB_CLASS dbClass, const TCHAR* ptszDSN, const bool bLoadExcelFile /*= false*/)
	: m_hEnv(NULL), m_hConn(NULL), m_hStmt(NULL), m_nParamNum(0), m_nColNum(0), m_nFetchedRows(0)
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
bool CBaseODBC::InitEnvHandle(void)
{
	if( SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv) )
		return false;

	// Set ODBC Version 3.0
	if( SQL_SUCCESS != SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER) )
		return false;

	return true;
}

//***************************************************************************
//
bool CBaseODBC::InitConnHandle(const LONG_PTR lLoginTimeOut, const LONG_PTR lConnectionTimeOut)
{
	if( SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hConn) )
		return false;

	if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)lLoginTimeOut, 0) )
		return false;

	if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)lConnectionTimeOut, 0) )
		return false;

	return true;
}

//***************************************************************************
//
bool CBaseODBC::InitStmtHandle(const LONG_PTR lQueryTimeOut)
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
void CBaseODBC::ClearStmt(void)
{
	if( m_hStmt )
	{
		SQLFreeStmt(m_hStmt, SQL_UNBIND);
		SQLFreeStmt(m_hStmt, SQL_RESET_PARAMS);
		SQLFreeStmt(m_hStmt, SQL_CLOSE);
	}

	_tcsncpy_s(m_tszQueryInfo, SQL_MAX_MESSAGE_LENGTH, _T(""), _TRUNCATE);
	m_nParamNum = 0;
	m_nColNum = 0;
}

//***************************************************************************
//
bool CBaseODBC::Connect()
{
	TCHAR tszOutConnStr[DATABASE_BUFFER_SIZE];
	SQLRETURN nRet;
	SQLSMALLINT nLen;

	if( m_hConn ) return false;

	if( !m_tszDSN )
		return false;

	try
	{
		if( false == InitEnvHandle() )
			throw 0;

		if( false == InitConnHandle() )
			throw 0;

#ifdef _UNICODE
		nRet = SQLDriverConnect(m_hConn, NULL, (SQLWCHAR*)m_tszDSN, (SQLSMALLINT)_tcslen(m_tszDSN), (SQLWCHAR*)tszOutConnStr, DATABASE_BUFFER_SIZE, &nLen, SQL_DRIVER_NOPROMPT);
#else
		nRet = SQLDriverConnect(m_hConn, NULL, (SQLCHAR*)m_tszDSN, (SQLSMALLINT)_tcslen(m_tszDSN), (SQLCHAR*)tszOutConnStr, DATABASE_BUFFER_SIZE, &nLen, SQL_DRIVER_NOPROMPT);
#endif

		if( nRet != SQL_SUCCESS && nRet != SQL_SUCCESS_WITH_INFO )
		{
			TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
			CDBError()(SQL_HANDLE_DBC, m_hConn, tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
			throw 0;
		}

		if( false == InitStmtHandle() )
			throw 0;
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

	if( m_hStmt )
	{
		nRet = SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
		m_hStmt = NULL;
	}

	if( m_hConn )
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
			
		m_hConn = NULL;
	}

	if( m_hEnv )
	{
		SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
		m_hEnv = NULL;
	}

	return true;
}

//***************************************************************************
//	
bool CBaseODBC::IsConnected()
{
	if( !m_hConn ) return false;

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
	ClearStmt();

	_tcsncpy_s(m_tszQueryInfo, SQL_MAX_MESSAGE_LENGTH, ptszQueryInfo, _TRUNCATE);

	SQLRETURN nRet;

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

	if( !m_hConn ) return false;
	if( m_hStmt == NULL )
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

	if( !m_hConn ) return false;
	if( m_hStmt == NULL )
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
//
bool CBaseODBC::AllSets(LONG_PTR nQueryResultRecordSize, LONG_PTR nMaxRowSize)
{
	SQLRETURN nRet = SQL_ERROR;

	if( m_hStmt != NULL )
	{
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER)nQueryResultRecordSize, SQL_IS_UINTEGER);
		if( SQL_SUCCESS != nRet )
			return false;

		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)nMaxRowSize, SQL_IS_UINTEGER);
		if( SQL_SUCCESS != nRet )
			return false;

		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROWS_FETCHED_PTR, (SQLPOINTER)&m_nFetchedRows, sizeof(SQLINTEGER));
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
bool CBaseODBC::AutoCommitOff(void)
{
	if( NULL == m_hConn )
		return false;

	if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0) )
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
bool CBaseODBC::AutoCommitOn(void)
{
	if( NULL == m_hConn )
		return false;

	if( SQL_SUCCESS != SQLSetConnectAttr(m_hConn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0) )
		return false;

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

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, cType, sqlType, iSize, 0, (BYTE*)pbData, 0, &lRetSize);
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
bool CBaseODBC::BindCol(int32 iParamIndex, TCHAR* ptszValue, int32& iBuffSize, SQLLEN& lRetSize)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(ptszValue, iBuffSize);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iParamIndex, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, &lRetSize);
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
	nRet = SQLDescribeCol(m_hStmt, iColNum, ColDescription.tszColName, DATABASE_COLUMN_NAME_STRLEN, &ColDescription.NameLength,
						  &ColDescription.DataType, (SQLULEN*)&ColDescription.dwColSize, &ColDescription.DigitSize, &ColDescription.Nullable);
#else
	nRet = SQLDescribeCol(m_hStmt, iColNum, (SQLCHAR*)ColDescription.tszColName, DATABASE_COLUMN_NAME_STRLEN, &ColDescription.NameLength,
						  &ColDescription.DataType, (SQLULEN*)&ColDescription.dwColSize, &ColDescription.DigitSize, &ColDescription.Nullable);
#endif

	nRet = SQLColAttribute(m_hStmt, iColNum, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, (SQLLEN*)&ColDescription.DispLength);

	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}

//***************************************************************************
//	
bool CBaseODBC::GetData(int32 iColNum, TCHAR* ptszData, int32& iBufSize)
{
	long		lRetSize = 0;
	SQLRETURN	nRet;

#ifdef _UNICODE	
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_CHAR, (SQLWCHAR*)ptszData, iBufSize, (SQLLEN*)&lRetSize);
#else
	nRet = SQLGetData(m_hStmt, iColNum, SQL_C_CHAR, (SQLCHAR*)ptszData, iBufSize, (SQLLEN*)&lRetSize);
#endif

	if( lRetSize == SQL_NO_TOTAL || lRetSize == SQL_NULL_DATA )
		return false;
	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}