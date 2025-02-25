
//***************************************************************************
// BaseMySQL.h : interface for the CBaseMySQL class.
//
//***************************************************************************

#ifndef __BASEMYSQL_H__
#define __BASEMYSQL_H__

#include <mysql.h>
#include <mysqld_error.h>

#pragma comment(lib, LIB_NAME("libmysql"))

//***************************************************************************
// - mysql_library_init(0, NULL, NULL);		// MySQL 라이브러리 초기화(프로그램에서 단 한 번만 호출)
// - mysql_library_end();					// MySQL 라이브러리 메모리 정리(프로그램 종료 시 한 번만 호출)

//***************************************************************************
//
class CBaseMySQL : public CPoolObj
{
public:
	CBaseMySQL();
	CBaseMySQL(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort);

	~CBaseMySQL();

	bool		Connect(const uint32 uiConnectTimeOut = MYSQL_DEFAULT_CONNECTION_TIMEOUT, const uint32 uiReadTimeOut = MYSQL_DEFAULT_QUERY_READ_TIMEOUT, const uint32 uiWriteTimeOut = MYSQL_DEFAULT_QUERY_WRITE_TIMEOUT, const char* pszPluginDir = nullptr);
	bool		Disconnect();
	void		StmtClose();
	void		FreeResult(MYSQL_RES* res);

	MYSQL*		GetConnPtr();
	bool		IsConnected();

	bool		GetServerInfo(TCHAR* ptszServerInfo);
	bool		GetClientInfo(TCHAR* ptszClientInfo);
	bool		SetCharacterSetName(const TCHAR* ptszCharacterSetName);
	bool		GetCharacterSetName(TCHAR* ptszCharacterSetName);
	bool		GetCharacterSetInfo(MY_CHARSET_INFO& charset);
	bool		GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen);

	bool		AutoCommit(bool bSetvalue);
	bool		StartTransaction();
	bool		Commit();
	bool		Rollback();

	bool		SelectDB(const char* pszSelectDBName);
	bool		SelectDB(const wchar_t* pszSelectDBName);

	bool		Prepare(const char* pszSQL);
	bool		Prepare(const wchar_t* pwszSQL);

	bool		PrepareBindParam(const MYSQL_BIND* pbindParams);
	bool		PrepareBindParam(const std::vector<MYSQL_BIND>& bindParams);
	bool		PrepareAttSet(enum enum_stmt_attr_type attr_type, const void* attr);
	bool		PrepareExecute(uint64_t* pnIdx = nullptr);

	bool		Execute(const char* pszSQL);
	bool		Execute(const wchar_t* pwszSQL);

	bool		Query(const char* pszSQL);
	bool		Query(const wchar_t* pwszSQL);
	bool		Query(const char* pszSQL, MYSQL_RES*& pRes);
	bool		Query(const wchar_t* pszSQL, MYSQL_RES*& pRes);

	bool		Query(const char* pszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));
	bool		Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));

	uint64		GetAffectedRow();
	uint32		GetFieldCount();

	uint64		GetStmtNumRows();
	uint64		GetStmtAffectedRow();
	uint32		GetStmtFieldCount();

	uint64		GetNumRows(MYSQL_RES* pRes);
	uint64		GetNumFields(MYSQL_RES* pRes);
	bool		GetFetchField(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount);

	void		GetData(const MYSQL_ROW Row, const int nColNum, bool& bIsData);
	void		GetData(const MYSQL_ROW Row, const int nColNum, char* pszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, wchar_t* pwszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int32& i32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint32& ui32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int64& i64Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint64& ui64Data);

	uint32		GetErrorNo();
	bool		GetErrorMessage(TCHAR* ptszMessage);
	bool		GetStmtErrorMessage(MYSQL_STMT* pStmt, TCHAR* ptszMessage);

	//***************************************************************************
	// bind.buffer에 pszValue를 가리키는 포인터(주소)를 저장 
	//	- 이 함수를 여러번 호출할 경우, 해당 변수는 서로 다른 메모리 공간이어야 함.
	//  - 예를 들어, 루프로 szValue에 데이터를 넣고 호출할 경우, szValue는 char szValue[batchCount][50]로 선언된 변수여야 함. 
	static MYSQL_BIND BindParam(const char* pszValue, ulong* ulBufLength)
	{
		MYSQL_BIND bind{};
		memset(&bind, 0, sizeof(bind));

		unsigned long size = static_cast<unsigned long>(strlen(pszValue));

		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = (void*)pszValue;			// const char*로 받은 문자열
		bind.buffer_length = size;				// 문자열 길이
		bind.length = ulBufLength;
	
		return bind;
	};

	//***************************************************************************
	// bind.buffer에 메모리를 생성하고, std::wstring을 UTF-8로 변환한 문자열을 복사 
	//	- 이 함수를 여러번 호출할 경우, 해당 변수는 서로 같은 메모리 공간이어도 됨.
	//  - 예를 들어, 루프로 wszValue에 데이터를 넣고 호출할 경우, wszValue는 wchar_t wszValue[50]로 선언된 변수
	static MYSQL_BIND BindParam(const wchar_t* pwszValue, ulong ulBufSize)
	{
		MYSQL_BIND bind{};
		memset(&bind, 0, sizeof(bind));

		// std::wstring을 UTF-8로 변환
		std::string utf8;

		int required_cch = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, pwszValue, static_cast<int>(ulBufSize), nullptr, 0, nullptr, nullptr);
		utf8.resize(required_cch);
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, pwszValue, -1, const_cast<char*>(utf8.c_str()), static_cast<int>(utf8.size()), NULL, NULL);

		unsigned long size = static_cast<unsigned long>(utf8.size());

		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = new char[size];
		memcpy(bind.buffer, utf8.c_str(), size);
		bind.buffer_length = size;
		bind.is_null = 0;

		// Bind length
		unsigned long* pulLength = new unsigned long(size);
		bind.length = pulLength;

		return bind;
	};

	//***************************************************************************
	// bind.buffer에 tValue를 가리키는 포인터(주소)를 저장
	//	- 이 함수를 여러번 호출할 경우, 해당 변수는 서로 다른 메모리 공간이어야 함.
	//  - 예를 들어, 루프로 tValue에 데이터를 넣고 호출할 경우, tValue는 자료형(int32, bool 등) value[batchCount]로 선언된 변수여야 함. 
	template<typename T>
	static MYSQL_BIND BindParam(const T& tValue)
	{
		MYSQL_BIND bind{};
		memset(&bind, 0, sizeof(bind));

		unsigned long size = static_cast<unsigned long>(sizeof(T));

		if constexpr( std::is_same_v<T, bool> )
		{
			bind.buffer_type = MYSQL_TYPE_TINY;
			bind.buffer = const_cast<bool*>(&tValue);
		}
		else if constexpr( std::is_same_v<T, INT8> )
		{
			bind.buffer_type = MYSQL_TYPE_TINY;
			bind.buffer = const_cast<INT8*>(&tValue);
			bind.is_unsigned = false;
		}
		else if constexpr( std::is_same_v<T, UINT8> )
		{
			bind.buffer_type = MYSQL_TYPE_TINY;
			bind.buffer = const_cast<UINT8*>(&tValue);
			bind.is_unsigned = true;
		}
		else if constexpr( std::is_same_v<T, INT16> )
		{
			bind.buffer_type = MYSQL_TYPE_SHORT;
			bind.buffer = const_cast<INT16*>(&tValue);
			bind.is_unsigned = false;
		}
		else if constexpr( std::is_same_v<T, UINT16> )
		{
			bind.buffer_type = MYSQL_TYPE_SHORT;
			bind.buffer = const_cast<UINT16*>(&tValue);
			bind.is_unsigned = true;
		}
		else if constexpr( std::is_same_v<T, INT32> )
		{
			bind.buffer_type = MYSQL_TYPE_LONG;
			bind.buffer = const_cast<INT32*>(&tValue);
			bind.is_unsigned = false;
		}
		else if constexpr( std::is_same_v<T, UINT32> )
		{
			bind.buffer_type = MYSQL_TYPE_LONG;
			bind.buffer = const_cast<UINT32*>(&tValue);
			bind.is_unsigned = true;
		}
		else if constexpr( std::is_same_v<T, INT64> )
		{
			bind.buffer_type = MYSQL_TYPE_LONGLONG;
			bind.buffer = const_cast<INT64*>(&tValue);
			bind.is_unsigned = false;
		}
		else if constexpr( std::is_same_v<T, UINT64> )
		{
			bind.buffer_type = MYSQL_TYPE_LONGLONG;
			bind.buffer = const_cast<UINT64*>(&tValue);
			bind.is_unsigned = true;
		}
		else if constexpr( std::is_same_v<T, float> )
		{
			bind.buffer_type = MYSQL_TYPE_FLOAT;
			bind.buffer = const_cast<float*>(&tValue);
		}
		else if constexpr( std::is_same_v<T, double> )
		{
			bind.buffer_type = MYSQL_TYPE_DOUBLE;
			bind.buffer = const_cast<double*>(&tValue);
		}
		else if constexpr( std::is_same_v<T, std::nullptr_t> )
		{
			bind.buffer_type = MYSQL_TYPE_NULL;
			bind.is_null = 1;
		}

		bind.buffer_length = size;

		return bind;
	};

	static void ClearBindParam(MYSQL_BIND bind)
	{
		if( bind.buffer_type == MYSQL_TYPE_STRING )
		{
			if( bind.buffer != nullptr )
			{
				SAFE_DELETE_ARRAY(bind.buffer);
				bind.buffer = nullptr;
			}

			if( bind.length != nullptr )
			{
				SAFE_DELETE(bind.length);
				bind.length = nullptr;
			}
		}
	}

private:
	void		ErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);
	void		StmtErrorQuery(MYSQL_STMT* pStmt, const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);

private:
	bool		m_bConnected;

	char		m_szDBHost[DATABASE_SERVER_NAME_STRLEN];
	char		m_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	char		m_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	char		m_szDBName[DATABASE_NAME_STRLEN];

	char		m_szCharacterSet[DATABASE_CHARACTERSET_STRLEN];
	char		m_szSelectDBName[DATABASE_NAME_STRLEN];

	uint32		m_uiPort;

	MYSQL*		m_pConn;		// MySQL Connection 핸들러
	MYSQL_STMT*	m_pStmt;		// MySQL 쿼리문 실행 관리
};

#endif // ndef __BASEMYSQL_H__