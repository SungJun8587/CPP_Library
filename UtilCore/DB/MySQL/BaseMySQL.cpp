
//***************************************************************************
// BaseMySQL.cpp: implementation of the CBaseMySQL class.
//
//***************************************************************************

#include "pch.h"
#include "BaseMySQL.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief CBaseMySQL 클래스의 기본 생성자입니다. 멤버 변수를 초기화합니다.
//***************************************************************************
CBaseMySQL::CBaseMySQL()
	: m_pConn(nullptr), m_pStmt(nullptr), m_uiPort(0), m_bConnected(false), m_bInTransaction(false)
{
	memset(&m_szDBHost[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBUserId[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBPasswd[0], 0, DATABASE_NAME_STRLEN);
	memset(&m_szDBName[0], 0, DATABASE_NAME_STRLEN);

	memset(&m_szCharacterSet[0], 0, DATABASE_CHARACTERSET_STRLEN);
	memset(&m_szSelectDBName[0], 0, DATABASE_CHARACTERSET_STRLEN);
}

//***************************************************************************
// @brief 데이터베이스 접속 정보를 인자로 받는 CBaseMySQL 클래스의 오버로딩된 생성자입니다.
// @param pszDBHost - 데이터베이스 서버 호스트 주소
// @param pszDBUserId - 데이터베이스 접속 사용자 ID
// @param pszDBPasswd - 데이터베이스 접속 비밀번호
// @param pszDBName - 접속할 기본 데이터베이스 이름
// @param nPort - 데이터베이스 서버 포트 번호
//***************************************************************************
CBaseMySQL::CBaseMySQL(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort)
	: m_pConn(nullptr), m_pStmt(nullptr), m_bConnected(false), m_bInTransaction(false)
{
	strncpy_s(m_szDBHost, _countof(m_szDBHost), pszDBHost, _TRUNCATE);
	strncpy_s(m_szDBUserId, _countof(m_szDBUserId), pszDBUserId, _TRUNCATE);
	strncpy_s(m_szDBPasswd, _countof(m_szDBPasswd), pszDBPasswd, _TRUNCATE);
	strncpy_s(m_szDBName, _countof(m_szDBName), pszDBName, _TRUNCATE);

	memset(&m_szCharacterSet[0], 0, DATABASE_CHARACTERSET_STRLEN);
	memset(&m_szSelectDBName[0], 0, DATABASE_CHARACTERSET_STRLEN);

	m_uiPort = nPort;
}

//***************************************************************************
// @brief CBaseMySQL 클래스의 소멸자입니다. 연결을 해제합니다.
//***************************************************************************
CBaseMySQL::~CBaseMySQL()
{
	Disconnect();
}

//***************************************************************************
// @brief MySQL 데이터베이스 서버에 연결을 수행합니다.
// @param uiConnectTimeOut - 연결 타임아웃 시간 (초)
// @param uiReadTimeOut - 읽기 타임아웃 시간 (초)
// @param uiWriteTimeOut - 쓰기 타임아웃 시간 (초)
// @param pszPluginDir - 플러그인 디렉토리 경로 (선택 사항)
// @return 연결 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Connect(const uint32 uiConnectTimeOut, const uint32 uiReadTimeOut, const uint32 uiWriteTimeOut, const char* pszPluginDir)
{
	if( m_bConnected ) return true;

	if( m_szDBHost[0] == '\0' ) return false;

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

		TCHAR tszHostInfo[128] = { 0, };
		TCHAR tszServerInfo[128] = { 0, };
		unsigned long ulServerVersion = 0;
		TCHAR tszCharacterSetName[128] = { 0, };

		bool bServerInfoResult = GetServerInfo(tszServerInfo, _countof(tszServerInfo));
		bool bHostInfoResult = GetHostInfo(tszHostInfo, _countof(tszHostInfo));
		bool bVersionResult = GetServerVersion(ulServerVersion);
		bool bCharsetResult = GetCharacterSetName(tszCharacterSetName, _countof(tszCharacterSetName));
		if( bServerInfoResult && bHostInfoResult && bVersionResult && bCharsetResult )
			LOG_DEBUG(_T("%s, Server: %s, DBMS: MySQL, Version: %s, Charset: %s"), __TFUNCTION__, tszHostInfo, tszServerInfo, tszCharacterSetName);
	}
	catch( ... )
	{
		Disconnect();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief MySQL 데이터베이스 서버와의 연결을 해제합니다.
// @return 항상 true 반환
//***************************************************************************
bool CBaseMySQL::Disconnect()
{
	if( m_pConn )
	{
		mysql_close(m_pConn);
		m_pConn = nullptr;
	}
	m_bConnected = false;
	m_bInTransaction = false;	// 연결이 끊기면 서버 측 트랜잭션도 더 이상 유효하지 않음

	LOG_DEBUG(_T("%s"), __TFUNCTION__);

	return true;
}

//***************************************************************************
// @brief 활성화된 Prepared Statement 핸들을 닫고 메모리를 해제합니다.
//***************************************************************************
void CBaseMySQL::StmtClose()
{
	if( m_pStmt )
	{
		mysql_stmt_close(m_pStmt);
		m_pStmt = nullptr;
	}
}

//***************************************************************************
// @brief 쿼리 결과 집합(MYSQL_RES)의 메모리를 해제합니다.
// @param res - 해제할 MYSQL_RES 결과 집합 포인터
//***************************************************************************
void CBaseMySQL::FreeResult(MYSQL_RES* res)
{
	if( res ) mysql_free_result(res);
}

//***************************************************************************
// @brief 내부 MySQL 연결 핸들(MYSQL*)을 반환합니다.
// @return MYSQL 연결 핸들 포인터
//***************************************************************************
MYSQL* CBaseMySQL::GetConnPtr()
{
	return m_pConn;
}

//***************************************************************************
// @brief 현재 데이터베이스에 연결되어 있는지 여부를 반환합니다.
// @return 연결 상태 (true: 연결됨, false: 연결 안 됨)
//***************************************************************************
bool CBaseMySQL::IsConnected()
{
	return m_bConnected;
}

//***************************************************************************
// @brief MySQL 서버 버전을 문자열 형태로 가져옴
// @param ptszServerInfo - 서버 버전 정보를 저장할 버퍼 (출력)
// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetServerInfo(TCHAR* ptszServerInfo, int32 nBufferLength)
{
	if( !m_bConnected || ptszServerInfo == nullptr || nBufferLength <= 0 ) return false;

	const char* pszServerInfo = mysql_get_server_info(m_pConn);
	if( !pszServerInfo ) return false;

#ifdef _UNICODE
	std::wstring wstrServerInfo = AnsiToUnicode(pszServerInfo);
	if( nBufferLength < (int32)(wstrServerInfo.size() + 1) ) return false;

	wcsncpy_s(ptszServerInfo, nBufferLength, wstrServerInfo.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszServerInfo, nBufferLength, pszServerInfo, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// @brief MySQL 호스트 연결 정보(주소 및 연결 방식)를 가져옴
// @param ptszHostInfo - 호스트 정보를 저장할 버퍼 (출력)
// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetHostInfo(TCHAR* ptszHostInfo, int32 nBufferLength)
{
	if( !m_bConnected || ptszHostInfo == nullptr || nBufferLength <= 0 ) return false;

	const char* pszHostInfo = mysql_get_host_info(m_pConn);
	if( !pszHostInfo ) return false;

#ifdef _UNICODE
	std::wstring wstrHostInfo = AnsiToUnicode(pszHostInfo);
	if( nBufferLength < (int32)(wstrHostInfo.size() + 1) ) return false;

	wcsncpy_s(ptszHostInfo, nBufferLength, wstrHostInfo.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszHostInfo, nBufferLength, pszHostInfo, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// @brief MySQL 서버 버전 정수값을 가져옴
// @param ulServerVersion - 서버 버전 숫자를 저장할 변수 참조 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetServerVersion(unsigned long& ulServerVersion)
{
	if( !m_bConnected ) return false;

	ulServerVersion = mysql_get_server_version(m_pConn);
	return true;
}

//***************************************************************************
// @brief MySQL 클라이언트 라이브러리 정보 문자열을 가져옴
// @param ptszClientInfo - 클라이언트 정보를 저장할 버퍼 (출력)
// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetClientInfo(TCHAR* ptszClientInfo, int32 nBufferLength)
{
	if( ptszClientInfo == nullptr || nBufferLength <= 0 ) return false;

	const char* pszClientInfo = mysql_get_client_info();
	if( !pszClientInfo ) return false;

#ifdef _UNICODE
	std::wstring wstrClientInfo = AnsiToUnicode(pszClientInfo);
	if( nBufferLength < (int32)(wstrClientInfo.size() + 1) ) return false;

	wcsncpy_s(ptszClientInfo, nBufferLength, wstrClientInfo.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszClientInfo, nBufferLength, pszClientInfo, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// @brief MySQL 클라이언트 라이브러리 버전 정수값을 가져옴
// @param ulClientVersion - 클라이언트 버전 숫자를 저장할 변수 참조 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetClientVersion(unsigned long& ulClientVersion)
{
	ulClientVersion = mysql_get_client_version();
	return true;
}

//***************************************************************************
// @brief MySQL 연결에 사용할 캐릭터셋 이름을 설정
// @param ptszCharacterSetName - 설정할 캐릭터셋 이름 문자열 버퍼
// @param nBufferLength - 입력된 문자열 버퍼의 크기 (TCHAR 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::SetCharacterSetName(const TCHAR* ptszCharacterSetName, int32 nBufferLength)
{
	if( ptszCharacterSetName == nullptr || nBufferLength <= 0 )
		return false;

	int32 nDataLength = static_cast<int32>(_tcsnlen(ptszCharacterSetName, nBufferLength));
	if( nDataLength <= 0 )
		return false;

#ifdef _UNICODE	
	std::wstring wstrCharacterSetName(ptszCharacterSetName, nDataLength);
	std::string strUtf8 = UnicodeToUtf8(wstrCharacterSetName);
	if( strUtf8.empty() || strUtf8.size() >= _countof(m_szCharacterSet) )
		return false;

	strncpy_s(m_szCharacterSet, _countof(m_szCharacterSet), strUtf8.c_str(), _TRUNCATE);
#else
	if( strncpy_s(m_szCharacterSet, _countof(m_szCharacterSet), ptszCharacterSetName, nDataLength) != 0 )
		return false;
#endif

	return true;
}

//***************************************************************************
// @brief MySQL 연결의 캐릭터셋 이름을 가져옴
// @param ptszCharacterSetName - 캐릭터셋 이름을 저장할 버퍼 (출력)
// @param nBufferLength - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetCharacterSetName(TCHAR* ptszCharacterSetName, int32 nBufferLength)
{
	if( !m_bConnected || ptszCharacterSetName == nullptr || nBufferLength <= 0 ) return false;

	if( strlen(m_szCharacterSet) < 1 )
	{
		const char* pszCurrentCharset = mysql_character_set_name(m_pConn);
		if( pszCurrentCharset )
		{
			strncpy_s(m_szCharacterSet, _countof(m_szCharacterSet), pszCurrentCharset, _TRUNCATE);
		}
	}

	if( strlen(m_szCharacterSet) < 1 ) return false;

#ifdef _UNICODE	
	std::wstring wstrCharacterSetName = Utf8ToUnicode(m_szCharacterSet);
	if( nBufferLength < (int32)(wstrCharacterSetName.size() + 1) ) return false;

	wcsncpy_s(ptszCharacterSetName, nBufferLength, wstrCharacterSetName.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszCharacterSetName, nBufferLength, m_szCharacterSet, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// mysql_query("SET NAMES 'utf8mb4'");
// mysql_query("SET CHARACTER SET utf8mb4");
// mysql_query("SET COLLATION_CONNECTION = 'utf8mb4_unicode_ci'");
// @brief 현재 연결의 캐릭터셋 상세 정보를 가져옵니다.
// @param charset - 캐릭터셋 정보를 저장할 MY_CHARSET_INFO 구조체 참조 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetCharacterSetInfo(MY_CHARSET_INFO& charset)
{
	if( !m_bConnected ) return false;

	mysql_get_character_set_info(m_pConn, &charset);

	return true;
}

//***************************************************************************
// @brief SQL 문에 삽입될 문자열의 특수 문자를 이스케이프 처리합니다.
// @param pszDest - 이스케이프된 결과 문자열을 저장할 버퍼 (출력)
// @param pszSrc - 원본 문자열
// @param iLen - 원본 문자열의 길이
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen)
{
	if( !m_bConnected || pszDest == nullptr || pszSrc == nullptr ) return false;

	mysql_real_escape_string(m_pConn, pszDest, pszSrc, iLen);

	return true;
}

//***************************************************************************
// @brief 자동 커밋(Auto-commit) 모드를 설정합니다.
// @param bSetvalue - true면 자동 커밋 활성화, false면 비활성화
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 데이터베이스 트랜잭션을 시작합니다.
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::StartTransaction()
{
	if( mysql_query(m_pConn, "START TRANSACTION") != 0 )
	{
		return false;
	}

	m_bInTransaction = true;

	return true;
}

//***************************************************************************
// @brief 현재 트랜잭션을 커밋합니다.
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Commit()
{
	m_bInTransaction = false;

	if( mysql_commit(m_pConn) != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 현재 트랜잭션을 롤백합니다.
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Rollback()
{
	m_bInTransaction = false;

	if( mysql_rollback(m_pConn) != 0 )
	{
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 사용할 데이터베이스를 멀티바이트 문자열 이름으로 선택합니다.
// @param pszSelectDBName - 선택할 데이터베이스 이름 (const char*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 사용할 데이터베이스를 유니코드 문자열 이름으로 선택합니다.
// @param pwszSelectDBName - 선택할 데이터베이스 이름 (const wchar_t*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::SelectDB(const wchar_t* pwszSelectDBName)
{
	char szSelectDBName[DATABASE_NAME_STRLEN];

	if( !m_bConnected )
	{
		return false;
	}

	std::string strSelectDBName = UnicodeToAnsi(pwszSelectDBName);
	if( strSelectDBName.empty() || sizeof(szSelectDBName) <= strSelectDBName.size() ) return false;

	strncpy_s(szSelectDBName, sizeof(szSelectDBName), strSelectDBName.c_str(), _TRUNCATE);

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
// @brief 멀티바이트 SQL 문으로 Prepared Statement를 준비합니다.
// @param pszSQL - 준비할 SQL 쿼리 문자열 (const char*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 유니코드 SQL 문으로 Prepared Statement를 준비합니다.
// @param pwszSQL - 준비할 SQL 쿼리 문자열 (const wchar_t*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Prepare(const wchar_t* pwszSQL)
{
	std::string strSQL = UnicodeToUtf8(pwszSQL);
	if( strSQL.empty() ) return false;

	return Prepare(strSQL.c_str());
}

//***************************************************************************
// @brief Prepared Statement에 단일 매개변수 구조체를 바인딩합니다.
// @param pbindParams - 바인딩할 MYSQL_BIND 구조체 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief Prepared Statement에 벡터 형태의 다중 매개변수를 바인딩합니다.
// @param bindParams - 바인딩할 MYSQL_BIND 벡터 컨테이너
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::PrepareBindParam(const CVector<MYSQL_BIND>& bindParams)
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
// @brief Prepared Statement의 속성을 설정합니다.
// @param attr_type - 설정할 속성 타입 (enum_stmt_attr_type)
// @param attr - 설정할 값의 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 준비된 Prepared Statement를 실행합니다.
// @param pnIdx - INSERT 시 생성된 Auto Increment 인덱스를 받아올 포인터 (선택 사항)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 멀티바이트 문자열 SQL 쿼리를 실행합니다. (Query 함수 호출)
// @param pszSQL - 실행할 SQL 쿼리 문자열 (const char*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Execute(const char* pszSQL)
{
	return Query(pszSQL);
}

//***************************************************************************
// @brief 유니코드 문자열 SQL 쿼리를 실행합니다. (Query 함수 호출)
// @param pwszSQL - 실행할 SQL 쿼리 문자열 (const wchar_t*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Execute(const wchar_t* pwszSQL)
{
	return Query(pwszSQL);
}

//***************************************************************************
// @brief 멀티바이트 문자열 SQL 쿼리를 수행합니다. (재연결 로직 포함)
// @param pszSQL - 실행할 SQL 쿼리 문자열 (const char*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Query(const char* pszSQL)
{
	char cTryCount = 0;
	bool bRes = false;

	if( !m_bConnected )
	{
		if( m_bInTransaction )
		{
			ErrorQuery(__FUNCTION__, pszSQL, 0, "Transaction lost due to disconnection");
			return false;
		}

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
			if( cTryCount == 1 && !m_bInTransaction && (nErrorNo == CR_SERVER_GONE_ERROR || nErrorNo == CR_SERVER_LOST) )
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
				ErrorQuery(__FUNCTION__, pszSQL, nErrorNo, mysql_error(m_pConn));

				if( m_bInTransaction && (nErrorNo == CR_SERVER_GONE_ERROR || nErrorNo == CR_SERVER_LOST) )
				{
					Disconnect();
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
// @brief 유니코드 문자열 SQL 쿼리를 수행합니다.
// @param pwszSQL - 실행할 SQL 쿼리 문자열 (const wchar_t*)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Query(const wchar_t* pwszSQL)
{
	std::string strSQL = UnicodeToUtf8(pwszSQL);
	if( strSQL.empty() ) return false;

	return Query(strSQL.c_str());
}

//***************************************************************************
// @brief 멀티바이트 SQL 쿼리를 실행하고 결과 집합(MYSQL_RES)을 반환합니다.
// @param pszSQL - 실행할 SQL 쿼리 문자열 (const char*)
// @param pRes - 결과 집합을 저장할 MYSQL_RES 포인터 레퍼런스 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 유니코드 SQL 쿼리를 실행하고 결과 집합(MYSQL_RES)을 반환합니다.
// @param pwszSQL - 실행할 SQL 쿼리 문자열 (const wchar_t*)
// @param pRes - 결과 집합을 저장할 MYSQL_RES 포인터 레퍼런스 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Query(const wchar_t* pwszSQL, MYSQL_RES*& pRes)
{
	std::string strSQL = UnicodeToUtf8(pwszSQL);
	if( strSQL.empty() ) return false;

	return Query(strSQL.c_str(), pRes);
}

//***************************************************************************
// @brief 멀티바이트 SQL 쿼리를 실행하고 각 행을 콜백 함수로 처리합니다.
// @param pszSQL - 실행할 SQL 쿼리 문자열 (const char*)
// @param pclsData - 콜백 함수에 전달할 사용자 정의 데이터 포인터
// @param FetchRow - 각 행(Row)을 처리할 콜백 함수 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
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
// @brief 유니코드 SQL 쿼리를 실행하고 각 행을 콜백 함수로 처리합니다.
// @param pwszSQL - 실행할 SQL 쿼리 문자열 (const wchar_t*)
// @param pclsData - 콜백 함수에 전달할 사용자 정의 데이터 포인터
// @param FetchRow - 각 행(Row)을 처리할 콜백 함수 포인터
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row))
{
	std::string strSQL = UnicodeToUtf8(pwszSQL);
	if( strSQL.empty() ) return false;

	if( Query(strSQL.c_str()) == false ) return false;

	MYSQL_RES* pRes = mysql_use_result(m_pConn);
	if( pRes == NULL )
	{
		ErrorQuery(__FUNCTION__, strSQL.c_str());
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
// @brief 가장 최근에 실행된 DML 문에 의해 영향을 받은 행(row)의 개수를 반환합니다.
// @return 영향받은 행의 개수 (uint64)
//***************************************************************************
uint64 CBaseMySQL::GetAffectedRow()
{
	if( !m_bConnected )
	{
		return 0;
	}

	return mysql_affected_rows(m_pConn);
}

//***************************************************************************
// @brief 가장 최근에 실행된 쿼리의 결과 컬럼(필드) 개수를 반환합니다.
// @return 컬럼 개수 (uint32)
//***************************************************************************
uint32 CBaseMySQL::GetFieldCount()
{
	if( !m_bConnected )
	{
		return 0;
	}

	return mysql_field_count(m_pConn);
}

//***************************************************************************
// @brief Prepared Statement 결과 집합의 행 개수를 반환합니다.
// @return 행 개수 (uint64)
//***************************************************************************
uint64 CBaseMySQL::GetStmtNumRows()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_num_rows(m_pStmt);
}

//***************************************************************************
// @brief Prepared Statement DML 실행에 의해 영향을 받은 행의 개수를 반환합니다.
// @return 영향받은 행의 개수 (uint64)
//***************************************************************************
uint64 CBaseMySQL::GetStmtAffectedRow()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_affected_rows(m_pStmt);
}

//***************************************************************************
// @brief Prepared Statement의 결과 컬럼 개수를 반환합니다.
// @return 컬럼 개수 (uint32)
//***************************************************************************
uint32 CBaseMySQL::GetStmtFieldCount()
{
	if( !m_pStmt )
	{
		return 0;
	}

	return mysql_stmt_field_count(m_pStmt);
}

//***************************************************************************
// @brief MYSQL_RES 결과 집합의 총 행(row) 개수를 반환합니다.
// @param pRes - 대상 MYSQL_RES 결과 집합 포인터
// @return 행 개수 (uint64)
//***************************************************************************
uint64 CBaseMySQL::GetNumRows(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_rows(pRes);
}

//***************************************************************************
// @brief MYSQL_RES 결과 집합의 총 컬럼(필드) 개수를 반환합니다.
// @param pRes - 대상 MYSQL_RES 결과 집합 포인터
// @return 컬럼 개수 (uint64)
//***************************************************************************
uint64 CBaseMySQL::GetNumFields(MYSQL_RES* pRes)
{
	if( pRes == NULL ) return 0;

	return mysql_num_fields(pRes);
}

//***************************************************************************
// @brief MYSQL_RES 결과 집합에서 필드 정보와 필드 개수를 가져옵니다.
// @param pRes - 대상 MYSQL_RES 결과 집합 포인터
// @param pFields - 필드 정보를 담을 MYSQL_FIELD 포인터 레퍼런스 (출력)
// @param ui64FieldCount - 필드 개수를 담을 변수 레퍼런스 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetFetchField(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount)
{
	if( pRes == NULL ) return false;

	ui64FieldCount = GetNumFields(pRes);
	pFields = mysql_fetch_field(pRes);

	return true;
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 멀티바이트 문자열 데이터를 가져옵니다.
// @param Rows - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param pszValue - 값을 저장할 버퍼 (출력)
// @param nBufSize - 버퍼 크기
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Rows, const int nColNum, char* pszValue, int nBufSize)
{
	if( Rows[nColNum] )
	{
		strncpy_s(pszValue, nBufSize, Rows[nColNum], _TRUNCATE);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 유니코드 문자열 데이터를 가져옵니다.
// @param Rows - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param pwszValue - 값을 저장할 유니코드 버퍼 (출력)
// @param nBufSize - 버퍼 크기
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Rows, const int nColNum, wchar_t* pwszValue, int nBufSize)
{
	if( Rows[nColNum] )
	{
		std::wstring wstrValue = Utf8ToUnicode(Rows[nColNum]);
		if( nBufSize < (int)(wstrValue.size() + 1) ) return;

		wcsncpy_s(pwszValue, nBufSize, wstrValue.c_str(), _TRUNCATE);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 불리언(bool) 데이터를 가져옵니다.
// @param Row - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param bIsData - 값을 저장할 불리언 변수 참조 (출력)
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, bool& bIsData)
{
	if( Row[nColNum] )
	{
		bIsData = atoi(Row[nColNum]);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 32비트 부호 있는 정수 데이터를 가져옵니다.
// @param Row - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param i32Data - 값을 저장할 int32 변수 참조 (출력)
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, int32& i32Data)
{
	if( Row[nColNum] )
	{
		i32Data = atoi(Row[nColNum]);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 32비트 부호 없는 정수 데이터를 가져옵니다.
// @param Row - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param ui32Data - 값을 저장할 uint32 변수 참조 (출력)
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, uint32& ui32Data)
{
	if( Row[nColNum] )
	{
		ui32Data = strtoul(Row[nColNum], NULL, 10);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 64비트 부호 있는 정수 데이터를 가져옵니다.
// @param Row - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param i64Data - 값을 저장할 int64 변수 참조 (출력)
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, int64& i64Data)
{
	if( Row[nColNum] )
	{
		i64Data = atoll(Row[nColNum]);
	}
}

//***************************************************************************
// @brief 결과 행(Row)의 특정 열에서 64비트 부호 없는 정수 데이터를 가져옵니다.
// @param Row - 데이터가 포함된 MYSQL_ROW 행 데이터
// @param nColNum - 가져올 컬럼 인덱스 번호
// @param ui64Data - 값을 저장할 uint64 변수 참조 (출력)
//***************************************************************************
void CBaseMySQL::GetData(const MYSQL_ROW Row, const int nColNum, uint64& ui64Data)
{
	if( Row[nColNum] )
	{
		ui64Data = strtoull(Row[nColNum], NULL, 10);
	}
}

//***************************************************************************
// @brief 가장 최근에 발생한 MySQL 에러 번호를 가져옵니다.
// @return 에러 번호 (uint32)
//***************************************************************************
uint32 CBaseMySQL::GetErrorNo()
{
	return mysql_errno(m_pConn);
}

//***************************************************************************
// @brief 가장 최근에 발생한 일반 에러 메시지를 가져옵니다.
// @param ptszMessage - 에러 메시지를 저장할 TCHAR 버퍼 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetErrorMessage(TCHAR* ptszMessage)
{
	if( !m_bConnected ) return false;

	char* pszMessage = (char*)mysql_error(m_pConn);

#ifdef _UNICODE	
	std::wstring wstrMessage = AnsiToUnicode(pszMessage);
	if( wstrMessage.size() >= MYSQL_MAX_MESSAGE_LENGTH ) return false;

	wcsncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, wstrMessage.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// @brief Prepared Statement 실행 중 발생한 에러 메시지를 가져옵니다.
// @param pStmt - 대상 MYSQL_STMT 포인터
// @param ptszMessage - 에러 메시지를 저장할 TCHAR 버퍼 (출력)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CBaseMySQL::GetStmtErrorMessage(MYSQL_STMT* pStmt, TCHAR* ptszMessage)
{
	if( !m_bConnected ) return false;

	char* pszMessage = (char*)mysql_stmt_error(pStmt);

#ifdef _UNICODE	
	std::wstring wstrMessage = AnsiToUnicode(pszMessage);
	if( wstrMessage.size() >= MYSQL_MAX_MESSAGE_LENGTH ) return false;

	wcsncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, wstrMessage.c_str(), _TRUNCATE);
#else
	strncpy_s(ptszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	return true;
}

//***************************************************************************
// @brief 일반 쿼리 실행 중 발생한 에러를 로그에 기록합니다.
// @param pszFunc - 에러가 발생한 함수 이름
// @param pszSQL - 에러가 발생한 SQL 쿼리 문자열
// @param uiErrno - 에러 번호 (기본값 0인 경우 내부에서 조회)
// @param pszMessage - 에러 메시지 (선택 사항)
//***************************************************************************
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
	std::wstring wstrFunc = AnsiToUnicode(pszFunc);
	if( _countof(tszFunc) < wstrFunc.size() + 1 ) return;
	wcsncpy_s(tszFunc, _countof(tszFunc), wstrFunc.c_str(), _TRUNCATE);

	std::wstring wstrSQL = AnsiToUnicode(pszSQL);
	if( _countof(tszSQL) < wstrSQL.size() + 1 ) return;
	wcsncpy_s(tszSQL, _countof(tszSQL), wstrSQL.c_str(), _TRUNCATE);

	std::wstring wstrMessage = AnsiToUnicode(pszMessage);
	if( _countof(tszMessage) < wstrMessage.size() + 1 ) return;
	wcsncpy_s(tszMessage, _countof(tszMessage), wstrMessage.c_str(), _TRUNCATE);
#else
	strncpy_s(tszFunc, MAX_PATH, pszFunc, _TRUNCATE);
	strncpy_s(tszSQL, DATABASE_BUFFER_SIZE, pszSQL, _TRUNCATE);
	strncpy_s(tszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	LOG_ERROR(_T("%s, QueryInfo[%s], ErrorNo[%u], ErrorMsg : %s"), tszFunc, tszSQL, uiErrno, tszMessage);
}

//***************************************************************************
// @brief Prepared Statement 실행 중 발생한 에러를 로그에 기록합니다.
// @param pStmt - 대상 MYSQL_STMT 포인터
// @param pszFunc - 에러가 발생한 함수 이름
// @param pszSQL - 에러가 발생한 SQL 쿼리 문자열
// @param uiErrno - 에러 번호 (기본값 0인 경우 내부에서 조회)
// @param pszMessage - 에러 메시지 (선택 사항)
//***************************************************************************
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
	std::wstring wstrFunc = AnsiToUnicode(pszFunc);
	if( _countof(tszFunc) < wstrFunc.size() + 1 ) return;
	wcsncpy_s(tszFunc, _countof(tszFunc), wstrFunc.c_str(), _TRUNCATE);

	std::wstring wstrSQL = AnsiToUnicode(pszSQL);
	if( _countof(tszSQL) < wstrSQL.size() + 1 ) return;
	wcsncpy_s(tszSQL, _countof(tszSQL), wstrSQL.c_str(), _TRUNCATE);

	std::wstring wstrMessage = AnsiToUnicode(pszMessage);
	if( _countof(tszMessage) < wstrMessage.size() + 1 ) return;
	wcsncpy_s(tszMessage, _countof(tszMessage), wstrMessage.c_str(), _TRUNCATE);
#else
	strncpy_s(tszFunc, MAX_PATH, pszFunc, _TRUNCATE);
	strncpy_s(tszSQL, DATABASE_BUFFER_SIZE, pszSQL, _TRUNCATE);
	strncpy_s(tszMessage, MYSQL_MAX_MESSAGE_LENGTH, pszMessage, _TRUNCATE);
#endif

	LOG_ERROR(_T("%s, QueryInfo[%s], StmtErrorNo[%u], StmtErrorMsg : %s"), tszFunc, tszSQL, uiErrno, tszMessage);
}