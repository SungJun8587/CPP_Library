
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
	: m_pConn(nullptr), m_pStmt(nullptr), m_uiPort(0), m_bConnected(false)
{
	memset(&m_szDBHost[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBUserId[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBPasswd[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBName[0], 0, DATABASE_NAME_STRLEN);

	memset(&m_szCharacterSet[0], 0, DATABASE_CHARACTERSET_STRLEN);
	memset(&m_szSelectDBName[0], 0, DATABASE_NAME_STRLEN);
}

//***************************************************************************
//
CBaseMySQL::CBaseMySQL(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort)
	: m_pConn(nullptr), m_pStmt(nullptr), m_bConnected(false)
{
	strncpy_s(m_szDBHost, _countof(m_szDBHost), pszDBHost, _TRUNCATE);
	strncpy_s(m_szDBUserId, _countof(m_szDBUserId), pszDBUserId, _TRUNCATE);
	strncpy_s(m_szDBPasswd, _countof(m_szDBPasswd), pszDBPasswd, _TRUNCATE);
	strncpy_s(m_szDBName, _countof(m_szDBName), pszDBName, _TRUNCATE);

	memset(&m_szCharacterSet[0], 0, DATABASE_CHARACTERSET_STRLEN);
	memset(&m_szSelectDBName[0], 0, DATABASE_NAME_STRLEN);

	m_uiPort = nPort;
}

//***************************************************************************
//
CBaseMySQL::~CBaseMySQL()
{
	Disconnect();
}

//***************************************************************************
//
bool CBaseMySQL::Connect(const uint32 uiConnectTimeOut, const uint32 uiReadTimeOut, const uint32 uiWriteTimeOut, const char *pszPluginDir)
{
	if( m_bConnected ) return true;

	if( !m_szDBHost ) return false;

	try 
	{
		m_pConn = mysql_init(nullptr);
		if( !m_pConn )
		{
			LOG_ERROR(_T("%s mysql_init error"), __TFUNCTION__);
			return false;
		}

#ifdef USE_PLUGIN_DIR
		if( pszPluginDir != nullptr )
		{
			mysql_options(m_pConn, MYSQL_PLUGIN_DIR, pszPluginDir);
		}
#endif
		if( uiConnectTimeOut > 0 )
		{
			if( mysql_options(m_pConn, MYSQL_OPT_CONNECT_TIMEOUT, &uiConnectTimeOut) )
			{
				mysql_close(m_pConn);
				return false;
			}
		}

		if( uiReadTimeOut > 0 )
		{
			if( mysql_options(m_pConn, MYSQL_OPT_READ_TIMEOUT, &uiReadTimeOut) )
			{
				mysql_close(m_pConn);
				return false;
			}
		}

		if( uiWriteTimeOut > 0 )
		{
			if( mysql_options(m_pConn, MYSQL_OPT_WRITE_TIMEOUT, &uiWriteTimeOut) )
			{
				mysql_close(m_pConn);
				return false;
			}
		}

		if( !mysql_real_connect(m_pConn, m_szDBHost, m_szDBUserId, m_szDBPasswd, m_szDBName, m_uiPort, nullptr, 0) )
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
bool CBaseMySQL::Disconnect()
{
	if( m_pConn )
	{
		mysql_close(m_pConn);
		m_pConn = nullptr;
	}
	m_bConnected = false;

	LOG_DEBUG(_T("%s"), __TFUNCTION__);

	return true;
}

//***************************************************************************
//
void CBaseMySQL::StmtClose()
{
	if( m_pStmt ) mysql_stmt_close(m_pStmt);
}

//***************************************************************************
//
void CBaseMySQL::FreeResult(MYSQL_RES* res)
{
	if( res ) mysql_free_result(res);
}

//***************************************************************************
//
MYSQL* CBaseMySQL::GetConnPtr()
{
	return m_pConn;
}

//***************************************************************************
//
bool CBaseMySQL::IsConnected()
{
	return m_bConnected;
}

//***************************************************************************
//
bool CBaseMySQL::GetServerInfo(TCHAR* ptszServerInfo)
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
bool CBaseMySQL::GetClientInfo(TCHAR* ptszClientInfo)
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
bool CBaseMySQL::SetCharacterSetName(const TCHAR* ptszCharacterSetName)
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
bool CBaseMySQL::GetCharacterSetName(TCHAR* ptszCharacterSetName)
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
bool CBaseMySQL::GetCharacterSetInfo(MY_CHARSET_INFO& charset)
{
	if( !m_bConnected ) return false;

	mysql_get_character_set_info(m_pConn, &charset);

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen)
{
	if( mysql_real_escape_string(m_pConn, pszDest, pszSrc, iLen) != 0 ) return false;

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::AutoCommit(bool bSetvalue)
{
	int ac = (bSetvalue) ? 1 : 0;

	if( mysql_autocommit(m_pConn, ac) != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::StartTransaction()
{
	if( mysql_query(m_pConn, "START TRANSACTION") != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::Commit()
{
	if( mysql_commit(m_pConn) != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::Rollback()
{
	if( mysql_rollback(m_pConn) != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::SelectDB(const char* pszSelectDBName)
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
bool CBaseMySQL::SelectDB(const wchar_t* pwszSelectDBName)
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
bool CBaseMySQL::Prepare(const char* pszSQL)
{
	if( !m_bConnected )
	{
		if( Connect() == false )
		{
			ErrorQuery(__FUNCTION__, pszSQL);
			return false;
		}
	}

	m_pStmt = mysql_stmt_init(m_pConn);
	if( !m_pStmt )
	{
		ErrorQuery(__FUNCTION__, pszSQL);
		Disconnect();
		return false;
	}

	if( mysql_stmt_prepare(m_pStmt, pszSQL, static_cast<int>(strlen(pszSQL))) != 0 )
	{
		unsigned int nErrorNo = mysql_errno(m_pConn);
		if( nErrorNo == CR_SERVER_GONE_ERROR || nErrorNo == CR_SERVER_LOST )
		{
			Disconnect();
			return false;
		}
		else
		{
			StmtErrorQuery(m_pStmt, __FUNCTION__, pszSQL);
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::Prepare(const wchar_t* pwszSQL)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Prepare(szSQL);
}

//***************************************************************************
//
bool CBaseMySQL::PrepareBindParam(const MYSQL_BIND* pbindParams)
{
	if( m_pStmt == nullptr )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( mysql_stmt_bind_param(m_pStmt, const_cast<MYSQL_BIND*>(pbindParams)) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetStmtErrorMessage(m_pStmt, tszMessage);
		LOG_ERROR(_T("%s, mysql_stmt_bind_param, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//
bool CBaseMySQL::PrepareBindParam(const std::vector<MYSQL_BIND>& bindParams)
{
	if( m_pStmt == nullptr )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( mysql_stmt_bind_param(m_pStmt, const_cast<MYSQL_BIND*>(bindParams.data())) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetStmtErrorMessage(m_pStmt, tszMessage);
		LOG_ERROR(_T("%s, mysql_stmt_bind_param, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
// - STMT_ATTR_UPDATE_MAX_LENGTH : 출력 버퍼의 최대 길이를 자동으로 업데이트할지 여부를 설정. 기본값: 0(비활성화), 1이면 mysql_stmt_store_result()를 호출하면 열 데이터의 최대 길이가 계산.
// - STMT_ATTR_CURSOR_TYPE : 커서 유형을 설정. 기본값: CURSOR_TYPE_NO_CURSOR, CURSOR_TYPE_READ_ONLY를 설정하면 서버에서 읽기 전용 커서를 활성화. 대량 데이터를 처리할 때 메모리 사용량을 줄이는 데 유용.
// - STMT_ATTR_PREFETCH_ROWS : 서버로부터 미리 가져올 행(row)의 개수를 설정. unsigned long. 기본값: 1, 이 값을 늘리면 행을 미리 가져와 네트워크 왕복 횟수를 줄일 수 있음.
// - STMT_ATTR_ARRAY_SIZE : 배열 바인딩 시 한 번에 처리할 행(row)의 개수를 설정. unsigned long. 기본값: 1, 대량 데이터 삽입에서 활용
bool CBaseMySQL::PrepareAttSet(enum enum_stmt_attr_type attr_type, const void* attr)
{
	if( mysql_stmt_attr_set(m_pStmt, attr_type, attr) )
	{
		return false;
	}

	return true;
}

//***************************************************************************
// 1. Prepare(const char* pszSQL) 함수 호출
// 2. StmtBindParam() 함수 호출
// 3. StmtAttSet() 함수 호출
// 4. StmtExecute() 함수 호출
// 5. StmtClose() 함수 호출
bool CBaseMySQL::PrepareExecute(uint64_t* pnIdx)
{
	bool bResult = true;

	if( m_pStmt == nullptr )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( mysql_stmt_execute(m_pStmt) != 0 )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetStmtErrorMessage(m_pStmt, tszMessage);
		LOG_ERROR(_T("%s, mysql_stmt_execute, ErrorMsg : %s"), __TFUNCTION__, tszMessage);
		bResult = false;
	}

	if( pnIdx )
	{
		*pnIdx = mysql_stmt_insert_id(m_pStmt);
	}

	return bResult;
}

//***************************************************************************
//
bool CBaseMySQL::Execute(const char* pszSQL)
{
	return Query(pszSQL);
}

//***************************************************************************
//
bool CBaseMySQL::Execute(const wchar_t* pwszSQL)
{
	return Query(pwszSQL);
}

//***************************************************************************
//
bool CBaseMySQL::Query(const char* pszSQL)
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
bool CBaseMySQL::Query(const wchar_t* pwszSQL)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Query(szSQL);
}

//***************************************************************************
//
bool CBaseMySQL::Query(const char* pszSQL, MYSQL_RES*& pRes)
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
bool CBaseMySQL::Query(const wchar_t* pwszSQL, MYSQL_RES*& pRes)
{
	char szSQL[DATABASE_BUFFER_SIZE];

	int nLength = WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || sizeof(szSQL) < (size_t)nLength ) return false;
	if( WideCharToMultiByte(CP_UTF8, 0, pwszSQL, -1, szSQL, nLength, NULL, NULL) == 0 ) return false;

	return Query(szSQL, pRes);
}

//***************************************************************************
//
bool CBaseMySQL::Query(const char* pszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row))
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
bool CBaseMySQL::Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row))
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
// INSERT, UPDATE, DELETE 같은 DML(Data Manipulation Language) 문이 실행된 후 영향을 받은(변경된) 행(row) 개수를 반환
uint64 CBaseMySQL::GetAffectedRow()
{
	if( !m_bConnected )
	{
		return 0;
	}

	return mysql_affected_rows(m_pConn);
}

//***************************************************************************
// 마지막으로 실행한 쿼리가 반환하는 컬럼(필드) 개수를 반환. 
// SELECT 문을 실행한 경우 해당 결과의 컬럼 개수를 반환하고,
// INSERT, UPDATE, DELETE 같은 DML 문을 실행한 경우에는 0을 반환
uint32 CBaseMySQL::GetFieldCount()
{
	if( !m_bConnected )
	{
		return 0;
	}

	return mysql_field_count(m_pConn);
}

//***************************************************************************
// 준비된 문(statement, MYSQL_STMT)을 실행한 후 결과 집합의 행 수를 반환
uint64 CBaseMySQL::GetStmtNumRows()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_num_rows(m_pStmt);
}

//***************************************************************************
// 준비된 문(statement, MYSQL_STMT)으로 INSERT, UPDATE, DELETE 등의 DML(Data Manipulation Language) 실행 후 영향을 받은 행(row) 개수를 반환
uint64 CBaseMySQL::GetStmtAffectedRow()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_affected_rows(m_pStmt);
}

//***************************************************************************
// 준비된 문(MYSQL_STMT)이 반환하는 컬럼 개수를 반환
uint32 CBaseMySQL::GetStmtFieldCount()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_field_count(m_pStmt);
}

//***************************************************************************
// SELECT 쿼리를 실행한 후, mysql_store_result() 또는 mysql_use_result()를 통해 가져온 MYSQL_RES 결과 집합의 총 행(row) 수를 반환
uint64 CBaseMySQL::GetNumRows(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_rows(pRes);
}

//***************************************************************************
// MYSQL_RES 결과 집합의 총 컬럼(필드) 수를 반환
uint64 CBaseMySQL::GetNumFields(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_fields(pRes);
}

//***************************************************************************
// MYSQL_RES 결과 집합에서 컬럼 정보를 포함한 MYSQL_FIELD 구조체를 반환
bool CBaseMySQL::GetFetchField(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount)
{
	if( pRes == NULL ) return false;

	ui64FieldCount = GetNumFields(pRes);
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
		ui32Data = strtoul(Row[nColNum], NULL, 10);
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
		ui64Data = strtoull(Row[nColNum], NULL, 10);
	}
}

//***************************************************************************
//
uint32 CBaseMySQL::GetErrorNo()
{
	return mysql_errno(m_pConn);
}

//***************************************************************************
//
bool CBaseMySQL::GetErrorMessage(TCHAR* ptszMessage)
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
bool CBaseMySQL::GetStmtErrorMessage(MYSQL_STMT* pStmt, TCHAR* ptszMessage)
{
	if( !m_bConnected ) return false;

	char* pszMessage = (char*)mysql_stmt_error(pStmt);

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
void CBaseMySQL::StmtErrorQuery(MYSQL_STMT* pStmt, const char* pszFunc, const char* pszSQL, uint32 uiErrno, const char* pszMessage)
{
	TCHAR tszFunc[MAX_PATH];
	TCHAR tszSQL[DATABASE_BUFFER_SIZE];
	TCHAR tszMessage[MYSQL_MAX_MESSAGE_LENGTH];

	if( uiErrno < 1 )
	{
		uiErrno = mysql_stmt_errno(pStmt);
		pszMessage = (char*)mysql_stmt_error(pStmt);
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


