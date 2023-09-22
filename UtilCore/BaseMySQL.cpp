
//***************************************************************************
// BaseMySQL.cpp: implementation of the CBaseMySQL class.
//
//***************************************************************************

#include "pch.h"
#include "BaseMySQL.h"
#include <vector>

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CBaseMySQL::CBaseMySQL()
	: m_bConnected(false), m_uiPort(0), m_uiBindCount(0), m_uiParamNum(0), m_pConn(NULL), m_pStmt(NULL), m_pBind(NULL)
{
	memset(m_szDBHost, 0, DATABASE_NAME_STRLEN);
	memset(m_szDBUserId, 0, DATABASE_NAME_STRLEN);
	memset(m_szDBPasswd, 0, DATABASE_NAME_STRLEN);
	memset(m_szDBName, 0, DATABASE_NAME_STRLEN);

	memset(m_szSelectDBName, 0, DATABASE_NAME_STRLEN);
	memset(m_szCharacterSet, 0, DATABASE_CHARACTERSET_STRLEN);
	memset(m_szPluginDir, 0, FULLPATH_STRLEN);
}

//***************************************************************************
//
CBaseMySQL::~CBaseMySQL()
{
	Disconnect();
}

//***************************************************************************
//
BOOL CBaseMySQL::InitConnHandle(const int nConnectTimeOut, const int nReadTimeOut, const int nWriteTimeOut)
{
	if( (m_pConn = mysql_init(m_pConn)) == NULL )
	{
		LOG_ERROR(_T("%s mysql_init error"), __TFUNCTION__);
		return false;
	}

#ifdef USE_PLUGIN_DIR
	if( m_strPluginDir.empty() == false )
	{
		mysql_options(m_pConn, MYSQL_PLUGIN_DIR, m_szPluginDir);
	}
#endif

	if( nConnectTimeOut > 0 )
	{
		mysql_options(m_pConn, MYSQL_OPT_CONNECT_TIMEOUT, (char*)&nConnectTimeOut);
	}

	if( nReadTimeOut > 0 )
	{
		mysql_options(m_pConn, MYSQL_OPT_READ_TIMEOUT, (char*)&nReadTimeOut);
	}

	if( nWriteTimeOut > 0 )
	{
		mysql_options(m_pConn, MYSQL_OPT_WRITE_TIMEOUT, (char*)&nWriteTimeOut);
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Connect()
{
	if( m_bConnected ) return true;

	if( !m_szDBHost ) return false;

	try {
		if( false == InitConnHandle() )
			return false;

		if( (m_pConn = mysql_real_connect(m_pConn, m_szDBHost, m_szDBUserId, m_szDBPasswd, m_szDBName, m_uiPort, NULL, 0)) == NULL )
		{
			TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
			GetErrorMessage(tszMessage);
			LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
			mysql_close(m_pConn);
			return false;
		}

		m_bConnected = true;

		if( m_szCharacterSet && strlen(m_szCharacterSet) > 0 )
		{
			if( mysql_set_character_set(m_pConn, m_szCharacterSet) != 0 )
			{
				TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
				GetErrorMessage(tszMessage);
				LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
			}
		}
		else
		{
			TCHAR	tszCharacterSetName[DATABASE_CHARACTERSET_STRLEN];
			GetCharacterSetName(tszCharacterSetName);
			LOG_DEBUG(_T("%s, mysql default character_set is (%s)"), __TFUNCTION__, tszCharacterSetName);
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
BOOL CBaseMySQL::PrepareClose()
{
	if( m_pBind )
	{
		for( uint32 i = 0; i < m_uiBindCount; i++ )
		{
			if( m_pBind[i].buffer )
			{
				delete[] m_pBind[i].buffer;
				m_pBind[i].buffer = NULL;
			}
		}
		free(m_pBind);
		m_pBind = NULL;
	}

	if( m_pStmt )
	{
		mysql_stmt_close(m_pStmt);
		m_pStmt = NULL;
	}

	m_uiParamNum = 0;
	m_uiBindCount = 0;

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Connect(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort)
{
	strncpy_s(m_szDBHost, _countof(m_szDBHost), pszDBHost, _TRUNCATE);
	strncpy_s(m_szDBUserId, _countof(m_szDBUserId), pszDBUserId, _TRUNCATE);
	strncpy_s(m_szDBPasswd, _countof(m_szDBPasswd), pszDBPasswd, _TRUNCATE);
	strncpy_s(m_szDBName, _countof(m_szDBName), pszDBName, _TRUNCATE);

	m_uiPort = nPort;

	return Connect();
}

//***************************************************************************
//
BOOL CBaseMySQL::Disconnect()
{
	PrepareClose();

	if( m_pConn ) mysql_close(m_pConn);

	m_bConnected = false;

	return true;
}

//***************************************************************************
//
MYSQL* CBaseMySQL::GetConnPtr()
{
	return m_pConn;
}

//***************************************************************************
//
BOOL CBaseMySQL::IsConnected()
{
	return m_bConnected;
}

//***************************************************************************
//
BOOL CBaseMySQL::GetServerInfo(TCHAR* ptszServerInfo)
{
	if( !m_bConnected ) return false;

	char szServerDescInfo[DATABASE_BUFFER_SIZE];

	const char* pszServerInfo = mysql_get_server_info(m_pConn);
	const char* pszHostInfo = mysql_get_host_info(m_pConn);
	unsigned long ulServerVersion = mysql_get_server_version(m_pConn);

	sprintf_s(szServerDescInfo, DATABASE_BUFFER_SIZE, "ServerInfo[%s], HostInfo[%s], ServerVersion[%ul]", pszServerInfo, pszHostInfo, ulServerVersion);

#ifdef _UNICODE
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szServerDescInfo, -1, NULL, 0);
	if( nLength == 0 || DATABASE_CHARACTERSET_STRLEN < nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)szServerDescInfo, -1, ptszServerInfo, nLength) == 0 ) return false;
#else
	strncpy_s(ptszServerInfo, DATABASE_BUFFER_SIZE, szServerDescInfo, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::GetClientInfo(TCHAR* ptszClientInfo)
{
	if( !m_bConnected ) return false;

	char szClientDescInfo[DATABASE_BUFFER_SIZE];

	const char* pszClientInfo = mysql_get_client_info();
	unsigned long ulClientVersion = mysql_get_client_version();

	sprintf_s(szClientDescInfo, DATABASE_BUFFER_SIZE, "ClientInfo[%s], ClientVersion[%ul]", pszClientInfo, ulClientVersion);

#ifdef _UNICODE
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szClientDescInfo, -1, NULL, 0);
	if( nLength == 0 || DATABASE_CHARACTERSET_STRLEN < nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)szClientDescInfo, -1, ptszClientInfo, nLength) == 0 ) return false;
#else
	strncpy_s(ptszClientInfo, DATABASE_BUFFER_SIZE, szClientDescInfo, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::SetCharacterSetName(const TCHAR* ptszCharacterSetName)
{
#ifdef _UNICODE	
	int nLength = WideCharToMultiByte(CP_ACP, 0, ptszCharacterSetName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(m_szCharacterSet) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, ptszCharacterSetName, -1, m_szCharacterSet, nLength, NULL, NULL) == 0 ) return false;
#else
	strncpy_s(m_szCharacterSet, DATABASE_CHARACTERSET_STRLEN, ptszCharacterSetName, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::GetCharacterSetName(TCHAR* ptszCharacterSetName)
{
	if( !m_bConnected ) return false;

	if( !m_szCharacterSet || strlen(m_szCharacterSet) < 1 )
	{
		strncpy_s(m_szCharacterSet, DATABASE_CHARACTERSET_STRLEN, mysql_character_set_name(m_pConn), _TRUNCATE);
	}

#ifdef _UNICODE	
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)m_szCharacterSet, -1, NULL, 0);
	if( nLength == 0 || DATABASE_CHARACTERSET_STRLEN < nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)m_szCharacterSet, -1, ptszCharacterSetName, nLength) == 0 ) return false;
#else
	strncpy_s(ptszCharacterSetName, DATABASE_CHARACTERSET_STRLEN, m_szCharacterSet, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// mysql_query("SET NAMES 'utf8mb4'");
// mysql_query("SET CHARACTER SET utf8mb4");
// mysql_query("SET COLLATION_CONNECTION = 'utf8mb4_unicode_ci'");
BOOL CBaseMySQL::GetCharacterSetInfo(MY_CHARSET_INFO& charset)
{
	if( !m_bConnected ) return false;

	mysql_get_character_set_info(m_pConn, &charset);

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen)
{
	if( mysql_real_escape_string(m_pConn, pszDest, pszSrc, iLen) != 0 ) return false;

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Autocommit(bool bSetvalue)
{
	int ac = (bSetvalue) ? 1 : 0;

	if( !mysql_autocommit(m_pConn, ac) )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::StartTransaction()
{
	if( mysql_query(m_pConn, "START TRANSACTION") != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Commit()
{
	if( mysql_query(m_pConn, "COMMIT") != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Rollback()
{
	if( mysql_query(m_pConn, "ROLLBACK") != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
uint64 CBaseMySQL::GetAffectedRow()
{
	if( !m_bConnected )
	{
		return 0;
	}

	return mysql_affected_rows(m_pConn);
}

//***************************************************************************
//
uint32 CBaseMySQL::GetErrorNo()
{
	return mysql_errno(m_pConn);
}

//***************************************************************************
//
BOOL CBaseMySQL::GetErrorMessage(TCHAR* ptszMessage)
{
	if( !m_bConnected ) return false;

	char* pszMessage = (char*)mysql_error(m_pConn);

#ifdef _UNICODE	
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, NULL, 0);
	if( nLength == 0 || MYSQL_MAX_MESSAGE_LENGTH < nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, ptszMessage, nLength) == 0 ) return false;
#else
	strncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::GetStmtErrorMessage(TCHAR* ptszMessage)
{
	if( !m_bConnected ) return false;

	char* pszMessage = (char*)mysql_stmt_error(m_pStmt);

#ifdef _UNICODE	
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, NULL, 0);
	if( nLength == 0 || MYSQL_MAX_MESSAGE_LENGTH < nLength ) return false;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, ptszMessage, nLength) == 0 ) return false;
#else
	strncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::SelectDB(const char* pszSelectDBName)
{
	if( !m_bConnected )
	{
		return false;
	}

	if( mysql_select_db(m_pConn, pszSelectDBName) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}
	else
	{
		strncpy_s(m_szSelectDBName, _countof(m_szSelectDBName), pszSelectDBName, _TRUNCATE);
		return true;
	}
}

//***************************************************************************
//
BOOL CBaseMySQL::SelectDB(const wchar_t* pwszSelectDBName)
{
	char szSelectDBName[DATABASE_NAME_STRLEN];

	if( !m_bConnected )
	{
		return false;
	}

	int nLength = WideCharToMultiByte(CP_ACP, 0, pwszSelectDBName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSelectDBName) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszSelectDBName, -1, szSelectDBName, nLength, NULL, NULL) == 0 ) return false;

	if( mysql_select_db(m_pConn, szSelectDBName) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}
	else
	{
		strncpy_s(m_szSelectDBName, _countof(m_szSelectDBName), szSelectDBName, _TRUNCATE);
		return true;
	}
}

//***************************************************************************
//
BOOL CBaseMySQL::Prepare(const char* pszSQL)
{
	if( !m_bConnected )
	{
		if( Connect() == false )
		{
			ErrorQuery(__FUNCTION__, pszSQL);
			return false;
		}
	}

	PrepareClose();

	for( int i = 0; i < 2; ++i )
	{
		m_pStmt = mysql_stmt_init(m_pConn);
		if( m_pStmt == NULL )
		{
			ErrorQuery(__FUNCTION__, pszSQL);
			return false;
		}

		if( mysql_stmt_prepare(m_pStmt, pszSQL, (int)strlen(pszSQL)) != 0 )
		{
			unsigned int nErrorNo = mysql_errno(m_pConn);
			if( i == 0 && (nErrorNo == CR_SERVER_GONE_ERROR || nErrorNo == CR_SERVER_LOST) )
			{
				Disconnect();
				if( Connect() == false )
				{
					ErrorQuery(__FUNCTION__, pszSQL);
					return false;
				}

				PrepareClose();
			}
			else
			{
				StmtErrorQuery(__FUNCTION__, pszSQL);
				return false;
			}
		}
		else
		{
			break;
		}
	}
	
	m_uiBindCount = mysql_stmt_param_count(m_pStmt);
	if( m_uiBindCount > 0 )
	{
		m_pBind = (MYSQL_BIND*)malloc(sizeof(MYSQL_BIND) * m_uiBindCount);
		if( m_pBind == NULL )
		{
			ErrorQuery(__FUNCTION__, pszSQL, 0, "Bind Malloc Error");
			return false;
		}

		memset(m_pBind, 0, sizeof(MYSQL_BIND) * m_uiBindCount);
		m_uiParamNum = 0;
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Prepare(const wchar_t* pwszSQL)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Prepare(szSQL);
}

//***************************************************************************
// 
BOOL CBaseMySQL::BindParam(const char* pszValue, ulong ulBufSize)
{
	if( m_pStmt == NULL )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( m_uiParamNum + 1 > m_uiBindCount )
	{
		LOG_ERROR(_T("%s, ErrorMsg : Bind Error - Index(%d) is not correct - Bind Count(%d)"), __TFUNCTION__, m_uiParamNum, m_uiBindCount);
		return false;
	}

	m_pBind[m_uiParamNum].buffer_type = MYSQL_TYPE_STRING;
	m_pBind[m_uiParamNum].buffer = (char*)pszValue;
	m_pBind[m_uiParamNum].buffer_length = ulBufSize;
	m_pBind[m_uiParamNum].length = &m_pBind[m_uiParamNum].buffer_length;
	m_pBind[m_uiParamNum].is_unsigned = false;
	m_uiParamNum++;

	return true;
}

//***************************************************************************
// 
BOOL CBaseMySQL::BindParam(const wchar_t* pwszValue, ulong ulBufSize)
{
	if( m_pStmt == NULL )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( m_uiParamNum + 1 > m_uiBindCount )
	{
		LOG_ERROR(_T("%s, ErrorMsg : Bind Error - Index(%d) is not correct - Bind Count(%d)"), __TFUNCTION__, m_uiParamNum, m_uiBindCount);
		return false;
	}

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszValue, -1, NULL, 0, NULL, NULL);
	m_pBind[m_uiParamNum].buffer_type = MYSQL_TYPE_STRING;
	m_pBind[m_uiParamNum].buffer = new char[nLength];
	if( WideCharToMultiByte(CP_UTF8, 0, pwszValue, -1, (LPSTR)m_pBind[m_uiParamNum].buffer, nLength, NULL, NULL) == 0 ) return false;
	m_pBind[m_uiParamNum].buffer_length = nLength;
	m_pBind[m_uiParamNum].length = &m_pBind[m_uiParamNum].buffer_length;
	m_pBind[m_uiParamNum].is_unsigned = false;
	m_uiParamNum++;

	return true;
}

//***************************************************************************
// 1. Prepare(const char* pszSQL) 함수 호출
// 2. BindParam() 함수 호출
// 3. PrepareExecute() 함수 호출
// 4. PrepareClose() 함수 호출
BOOL CBaseMySQL::PrepareExecute(uint64_t* pnIdx)
{
	if( m_pStmt == NULL )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( mysql_stmt_bind_param(m_pStmt, m_pBind) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetStmtErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, mysql_stmt_bind_param, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}

	if( mysql_stmt_execute(m_pStmt) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetStmtErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, mysql_stmt_execute, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}

	if( pnIdx )
	{
		*pnIdx = mysql_stmt_insert_id(m_pStmt);
	}

	PrepareClose();

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Execute(const char* pszSQL)
{
	return Query(pszSQL);
}

//***************************************************************************
//
BOOL CBaseMySQL::Execute(const wchar_t* pwszSQL)
{
	return Query(pwszSQL);
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const char* pszSQL)
{
	char cTryCount = 0;
	bool bRes = false;

	if( !m_bConnected )
	{
		if( Connect() == false )
		{
			ErrorQuery(__FUNCTION__, pszSQL);
			return false;
		}
	}

	for( int i = 0; i < 2; ++i )
	{
		if( mysql_query(m_pConn, pszSQL) != 0 )
		{
			++cTryCount;
			unsigned int nErrorNo = mysql_errno(m_pConn);
			if( cTryCount == 1 && (nErrorNo == CR_SERVER_GONE_ERROR || nErrorNo == CR_SERVER_LOST) )
			{
				Disconnect();
				if( Connect() == false )
				{
					ErrorQuery(__FUNCTION__, pszSQL);
					return false;
				}
			}
			else
			{
				if( nErrorNo == ER_NO_SUCH_TABLE || nErrorNo == ER_BAD_TABLE_ERROR )
				{
					ErrorQuery(__FUNCTION__, pszSQL, nErrorNo, mysql_error(m_pConn));
				}
				else
				{
					ErrorQuery(__FUNCTION__, pszSQL, nErrorNo, mysql_error(m_pConn));
				}
				break;
			}
		}
		else
		{
			bRes = true;
			break;
		}
	}

	return bRes;
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const wchar_t* pwszSQL)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Query(szSQL);
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const char* pszSQL, MYSQL_RES*& pRes)
{
	if( Query(pszSQL) == false ) return false;

	pRes = mysql_use_result(m_pConn);
	if( pRes == NULL )
	{
		ErrorQuery(__FUNCTION__, pszSQL);
		return false;
	}

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const wchar_t* pwszSQL, MYSQL_RES*& pRes)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Query(szSQL, pRes);
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const char* pszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row))
{
	if( Query(pszSQL) == false ) return false;

	MYSQL_RES* pRes = mysql_use_result(m_pConn);
	if( pRes == NULL )
	{
		ErrorQuery(__FUNCTION__, pszSQL);
		return false;
	}

	MYSQL_ROW Row;

	while( (Row = mysql_fetch_row(pRes)) )
	{
		if( FetchRow(pclsData, Row) == false ) break;
	}

	mysql_free_result(pRes);

	return true;
}

//***************************************************************************
//
BOOL CBaseMySQL::Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row))
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	if( Query(szSQL) == false ) return false;

	MYSQL_RES* pRes = mysql_use_result(m_pConn);
	if( pRes == NULL )
	{
		ErrorQuery(__FUNCTION__, szSQL);
		return false;
	}

	MYSQL_ROW Row;

	while( (Row = mysql_fetch_row(pRes)) )
	{
		if( FetchRow(pclsData, Row) == false ) break;
	}

	mysql_free_result(pRes);

	return true;
}

//***************************************************************************
//
uint64 CBaseMySQL::GetRowCount(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_rows(pRes);
}

//***************************************************************************
//
uint64 CBaseMySQL::GetFieldCount(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_fields(pRes);
}

//***************************************************************************
//
BOOL CBaseMySQL::GetFields(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount)
{
	if( pRes == NULL ) return false;

	ui64FieldCount = GetFieldCount(pRes);
	pFields = mysql_fetch_field(pRes);

	return true;
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Rows, const int nColNum, char* pszValue, int nBufSize)
{
	if( Rows[nColNum] )
	{
		strncpy_s(pszValue, nBufSize, Rows[nColNum], _TRUNCATE);
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Rows, const int nColNum, wchar_t* pwszValue, int nBufSize)
{
	if( Rows[nColNum] )
	{
		int nLength = MultiByteToWideChar(CP_UTF8, 0, (LPSTR)Rows[nColNum], -1, NULL, 0);
		if( nLength == 0 || nBufSize < nLength ) return;
		if( MultiByteToWideChar(CP_UTF8, 0, (LPSTR)Rows[nColNum], -1, pwszValue, nLength) == 0 ) return;
		nBufSize = nLength;
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, bool& bIsData)
{
	if( Row[nColNum] )
	{
		bIsData = atoi(Row[nColNum]);
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, int32& i32Data)
{
	if( Row[nColNum] )
	{
		i32Data = atoi(Row[nColNum]);
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, uint32& ui32Data)
{
	if( Row[nColNum] )
	{
		ui32Data = GetUInt32(Row[nColNum]);
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, int64& i64Data)
{
	if( Row[nColNum] )
	{
		i64Data = atoll(Row[nColNum]);
	}
}

//***************************************************************************
//
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, uint64& ui64Data)
{
	if( Row[nColNum] )
	{
		ui64Data = GetUInt64(Row[nColNum]);
	}
}

//***************************************************************************
//
void CBaseMySQL::ErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno, const char* pszMessage)
{
	TCHAR tszFunc[MAX_PATH];
	TCHAR tszSQL[DATABASE_BUFFER_SIZE];
	TCHAR tszMessage[MYSQL_MAX_MESSAGE_LENGTH];

	if( uiErrno < 1 )
	{
		uiErrno = mysql_errno(m_pConn);
		pszMessage = (char*)mysql_error(m_pConn);
	}

#ifdef _UNICODE	
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszFunc, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszFunc) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszFunc, -1, tszFunc, nLength) == 0 ) return;

	nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSQL, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszSQL) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSQL, -1, tszSQL, nLength) == 0 ) return;

	nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszMessage) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, tszMessage, nLength) == 0 ) return;
#else
	strncpy_s(tszFunc, MAX_PATH, pszFunc, _TRUNCATE);
	strncpy_s(tszSQL, DATABASE_BUFFER_SIZE, pszSQL, _TRUNCATE);
	strncpy_s(tszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	LOG_ERROR(_T("%s, QueryInfo[%s], ErrorNo[%u], ErrorMsg : %s"), tszFunc, tszSQL, uiErrno, tszMessage);
}

//***************************************************************************
//
void CBaseMySQL::StmtErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno, const char* pszMessage)
{
	TCHAR tszFunc[MAX_PATH];
	TCHAR tszSQL[DATABASE_BUFFER_SIZE];
	TCHAR tszMessage[MYSQL_MAX_MESSAGE_LENGTH];

	if( uiErrno < 1 )
	{
		uiErrno = mysql_stmt_errno(m_pStmt);
		pszMessage = (char*)mysql_stmt_error(m_pStmt);
	}

#ifdef _UNICODE	
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszFunc, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszFunc) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszFunc, -1, tszFunc, nLength) == 0 ) return;

	nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSQL, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszSQL) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszSQL, -1, tszSQL, nLength) == 0 ) return;

	nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, NULL, 0);
	if( nLength == 0 || sizeof(tszMessage) < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszMessage, -1, tszMessage, nLength) == 0 ) return;
#else
	strncpy_s(tszFunc, MAX_PATH, pszFunc, _TRUNCATE);
	strncpy_s(tszSQL, DATABASE_BUFFER_SIZE, pszSQL, _TRUNCATE);
	strncpy_s(tszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	LOG_ERROR(_T("%s, QueryInfo[%s], StmtErrorNo[%u], StmtErrorMsg : %s"), tszFunc, tszSQL, uiErrno, tszMessage);
}