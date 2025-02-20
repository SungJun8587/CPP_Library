
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
		// �Լ��� ODBC API���� SQL ��(statement)�� ���õ� ���ҽ��� �����ϰų� �� �ڵ��� ���¸� �缳���ϴ� �� ���
		//	SQLHSTMT StatementHandle : ó���� SQL �� �ڵ�
		//	SQLUSMALLINT Option : �� �ڵ鿡�� ������ �۾��� ������ ����
		//		- SQL_CLOSE : ���� ���� Ŀ���� ����(��� ���� ó���� ���� ��, ���ο� SQL �� ���� ��, Ʈ����� ���� ��)
		//		- SQL_UNBIND : �� ���ε��� ����(SQLBindCol �Լ� �ʱ�ȭ)
		//		- SQL_RESET_PARAMS : �Ű����� ���ε��� �缳��(SQLBindParameter �Լ� �ʱ�ȭ)
		//		- SQL_DROP : �� �ڵ��� ������ ����
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
	// �Ű����� ���ε��� �缳��(SQLBindParameter �Լ� �ʱ�ȭ)
	FreeStmt(SQL_RESET_PARAMS);
	m_nParamNum = 0;
}

//***************************************************************************
//
void CBaseODBC::UnBindColStmt(void)
{
	// �� ���ε��� ����(SQLBindCol �Լ� �ʱ�ȭ)
	FreeStmt(SQL_UNBIND);
	m_nColNum = 0;
}

//***************************************************************************
// 1. SQLAllocHandle(SQL_HANDLE_ENV) : ODBC ȯ�� �ڵ� ����
// 2. SQLAllocHandle(SQL_HANDLE_DBC) : �����ͺ��̽� ���� �ڵ� ����
// 3. SQLDriverConnect() : �����ͺ��̽����� ����
// 4. SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT) : �ڵ� Ŀ�� ��� ����
// 5. SQLAllocHandle(SQL_HANDLE_STMT) : SQL ����� �����ϰų� ����� ó���ϱ� ���� ��� �ڵ��� ����
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

		// - SQL_DRIVER_NOPROMPT : ����ڿ��� ������Ʈ�� ǥ������ �ʰ� ������ �õ�
		// - SQL_DRIVER_COMPLETE : ���� ���ڿ��� ������ ������ ���� ��� ����ڿ��� ������Ʈ�� ǥ���Ͽ� �Է��� ��û
		// - SQL_DRIVER_PROMPT : �׻� ����ڿ��� ������Ʈ�� ǥ���Ͽ� ���� ������ �Է��ϵ��� ��û
		// - SQL_DRIVER_COMPLETE_REQUIRED : ���� ���ڿ��� ���Ե� �����θ� ������ �õ��ϸ�, ������ ������ ������ ������Ʈ�� ǥ��(��, ����ڰ� �ʼ� ������ �Է��� �� �ֵ��� ����)
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
	// ���̺��� ���� �࿡ ���� �ϰ� �۾�(bulk operations)�� �����ϴ� �� ���
	//  SQLHSTMT     StatementHandle : SQL ��(statement) �ڵ�
	//	SQLUSMALLINT Operation : ������ �۾��� ������ ����
	//		- SQL_ADD : �� ���� ����
	//		- SQL_UPDATE_BY_BOOKMARK : �ϸ�ũ(bookmark)�� ������� Ư�� ���� ������Ʈ
	//		- SQL_DELETE_BY_BOOKMARK : �ϸ�ũ(bookmark)�� ������� Ư�� ���� ����
	//		- SQL_FETCH_BY_BOOKMARK : �ϸ�ũ(bookmark)�� ������� Ư�� ���� ������
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
		// SQLINTEGER cbValueMax : ���ڿ��� ��� ���ڿ��� ���̸� ����. ���ڿ��� �ƴ� ��� 0 �Ǵ� ����.
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
		// ����� ������ SQLFetch �Ǵ� SQLFetchScroll�� ȣ���� �� ����� ���ε� ������ �����ϴ� SQLULEN ��. 
		// ���� SQL_BIND_BY_COLUMN �����Ͽ� �� ���� ���ε��� ����. 
		// �� ���� ���ε��� ���� ��� ���� ���ε��� ������ ���� �Ǵ� ������ ���̷� �����Ͽ� ����.
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER)nQueryResultRecordSize, 0);
		if( SQL_SUCCESS != nRet )
			return false;

		// �� ���� ó���� ���� ������ ����(�뷮 �۾� �� �� �Ӽ��� �÷��� �� ���� ���� ���� ó���� �� �ֵ��� ����)
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)nMaxRowSize, 0);
		if( SQL_SUCCESS != nRet )
			return false;

		/*
		// �� ���� �۾� ���¸� ������ �迭�� �����͸� ����
		SQLUSMALLINT* pnStatus = new SQLUSMALLINT[nMaxRowSize];
		nRet = SQLSetStmtAttr(m_hStmt, SQL_ATTR_ROW_STATUS_PTR, (SQLPOINTER)pnStatus, 0);
		if( SQL_SUCCESS != nRet )
			return false;
		*/

		// �۾��� ���� ������ ������ �����͸� ����
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

	// �ڵ� Ŀ�� Off ��� ���� ����(true �̸� Off ��带 ����)
	//	- SQL_AUTOCOMMIT_ON(�⺻��) : �� SQL �� ���� �� �ڵ����� Ŀ��.
	//	- SQL_AUTOCOMMIT_OFF : Ʈ����� ���. ��������� SQLEndTran�� ȣ���Ͽ� Ŀ�� �Ǵ� �ѹ� �ʿ�.
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