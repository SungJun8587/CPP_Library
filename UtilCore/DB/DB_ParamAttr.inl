/// @brief ODBC 파라미터 타입 매핑을 위한 기본 구조체
template<SQLSMALLINT CDATA_TYPE, SQLSMALLINT SQL_DATA_TYPE, SQLULEN COLUMN_SIZE = 0>
struct odbc_param_attr_base
{
	static SQLSMALLINT	const c_data_type = CDATA_TYPE;
	static SQLSMALLINT	const sql_data_type = SQL_DATA_TYPE;
	static SQLULEN		const column_size = COLUMN_SIZE;
};


/// @brief 기본 파라미터 속성 템플릿 (특수화되지 않은 경우 기본값 사용)
template<typename _TMain>
struct odbc_param_attr : odbc_param_attr_base<SQL_C_DEFAULT, SQL_VARCHAR >
{
};


/// @brief C++ 타입과 ODBC C/SQL 데이터 타입 매핑을 위한 매크로
#define ODBC_PARAM_ATTR(d_type, c_type, sql_type, p_size) \
	template<> \
	struct odbc_param_attr<d_type> : odbc_param_attr_base<c_type, sql_type, p_size> \
	{}

// 각 데이터 타입별 ODBC 속성 특수화 정의
ODBC_PARAM_ATTR(bool, SQL_C_BIT, SQL_BIT, 2);
ODBC_PARAM_ATTR(INT8, SQL_C_STINYINT, SQL_TINYINT, 3);
ODBC_PARAM_ATTR(INT16, SQL_C_SSHORT, SQL_SMALLINT, 5);
ODBC_PARAM_ATTR(INT32, SQL_C_SLONG, SQL_INTEGER, 10);
ODBC_PARAM_ATTR(INT64, SQL_C_SBIGINT, SQL_BIGINT, 19);
ODBC_PARAM_ATTR(UINT8, SQL_C_UTINYINT, SQL_TINYINT, 3);
ODBC_PARAM_ATTR(UINT16, SQL_C_USHORT, SQL_SMALLINT, 5);
ODBC_PARAM_ATTR(UINT32, SQL_C_ULONG, SQL_INTEGER, 10);
ODBC_PARAM_ATTR(UINT64, SQL_C_UBIGINT, SQL_BIGINT, 19);
ODBC_PARAM_ATTR(FLOAT, SQL_C_FLOAT, SQL_REAL, 7);
ODBC_PARAM_ATTR(DOUBLE, SQL_C_DOUBLE, SQL_DOUBLE, 15);
ODBC_PARAM_ATTR(CHAR*, SQL_C_CHAR, SQL_VARCHAR, 254);
ODBC_PARAM_ATTR(WCHAR*, SQL_C_WCHAR, SQL_WVARCHAR, 254);
ODBC_PARAM_ATTR(SQL_TIMESTAMP_STRUCT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 23);


/// @brief 바인딩할 파라미터의 속성과 버퍼 정보를 관리하는 클래스
class CDBParamAttr
{
public:
	CDBParamAttr()
	{
		m_nCDataType = SQL_C_DEFAULT;
		m_nSqlDataType = SQL_VARCHAR;
		m_ulColumnSize = 0;

		m_ptrBuffer = nullptr;
		m_nBufferLength = 0;
		m_lDataLength = 0;
	}

	/// @brief 파라미터의 C타입, SQL타입, DB 컬럼 크기 설정
	void SetParamAttr(SQLSMALLINT nCDataType, SQLSMALLINT nSqlDataType, SQLULEN ulColumnSize)
	{
		m_nCDataType = nCDataType;
		m_nSqlDataType = nSqlDataType;
		m_ulColumnSize = ulColumnSize;
	}

	/// @brief 버퍼 포인터 및 버퍼 최대 용량/실제 데이터 길이 설정
	void SetValue(void* ptrBuffer, INT32 nBufferLength, INT64 lDataLength)
	{
		m_ptrBuffer = ptrBuffer;
		m_nBufferLength = nBufferLength;
		m_lDataLength = lDataLength;
	}

public:
	SQLSMALLINT		m_nCDataType;			// ODBC C 데이터 타입 (예: SQL_C_SLONG, SQL_C_CHAR 등)
	SQLSMALLINT		m_nSqlDataType;			// ODBC SQL 데이터 타입 (예: SQL_INTEGER, SQL_VARCHAR 등)
	SQLULEN			m_ulColumnSize;			// 데이터베이스 상의 컬럼 크기 / Precision (SQLBindParameter의 ColumnSize 인자용)

	SQLPOINTER		m_ptrBuffer;			// 바인딩할 메모리 버퍼 포인터
	INT32			m_nBufferLength;		// 버퍼의 최대 할당 크기 (Bytes 단위, 오버플로우 방지용)
	INT64			m_lDataLength;			// 버퍼에 실제로 들어있는 유효 데이터의 길이 (Bytes 단위)
};


/// @brief 다양한 타입의 파라미터를 받아 `CDBParamAttr` 객체로 변환해 주는 매니저 클래스
class CDBParamAttrMgr
{
public:
	CDBParamAttrMgr(void) {}

	/// @brief 일반 기본 타입(숫자형, 구조체 등) 처리 
	template<typename EDataType>
	CDBParamAttr& operator()(EDataType& data)
	{
		m_dbParamAttr.SetParamAttr(
			odbc_param_attr<EDataType>::c_data_type,
			odbc_param_attr<EDataType>::sql_data_type,
			odbc_param_attr<EDataType>::column_size
		);

		INT32 size = static_cast<INT32>(sizeof(EDataType));
		m_dbParamAttr.SetValue(&data, size, size);

		return m_dbParamAttr;
	}

	/// @brief ANSI 문자열(CHAR*) 처리
	CDBParamAttr& operator()(CHAR* data)
	{
		m_dbParamAttr.SetParamAttr(
			odbc_param_attr<CHAR*>::c_data_type,
			odbc_param_attr<CHAR*>::sql_data_type,
			odbc_param_attr<CHAR*>::column_size
		);

		INT32 nDataLen = static_cast<INT32>(std::strlen(data));
		m_dbParamAttr.SetValue(data, nDataLen + 1, nDataLen);
		m_dbParamAttr.m_ulColumnSize = static_cast<SQLULEN>(nDataLen + 1);

		return m_dbParamAttr;
	}

	/// @brief 유니코드 문자열(WCHAR*) 처리
	CDBParamAttr& operator()(WCHAR* data)
	{
		m_dbParamAttr.SetParamAttr(
			odbc_param_attr<WCHAR*>::c_data_type,
			odbc_param_attr<WCHAR*>::sql_data_type,
			odbc_param_attr<WCHAR*>::column_size
		);

		INT32 nDataLen = static_cast<INT32>(std::wcslen(data) * sizeof(WCHAR));
		INT32 nCapacity = nDataLen + static_cast<INT32>(sizeof(WCHAR)); // 널 문자 공간 포함

		m_dbParamAttr.SetValue(data, nCapacity, nDataLen);
		m_dbParamAttr.m_ulColumnSize = static_cast<SQLULEN>((std::wcslen(data)) + 1);

		return m_dbParamAttr;
	}

	/// @brief ANSI 문자열 + 버퍼 최대 크기 외부 지정 처리
	CDBParamAttr& operator()(CHAR* data, INT32& nBufferLength)
	{
		m_dbParamAttr.SetParamAttr(
			odbc_param_attr<CHAR*>::c_data_type,
			odbc_param_attr<CHAR*>::sql_data_type,
			odbc_param_attr<CHAR*>::column_size
		);

		INT32 nDataLen = static_cast<INT32>(std::strlen(data));
		m_dbParamAttr.SetValue(data, nBufferLength, nDataLen);
		m_dbParamAttr.m_ulColumnSize = static_cast<SQLULEN>(nBufferLength);

		return m_dbParamAttr;
	}

	/// @brief 유니코드 문자열 + 버퍼 최대 크기 외부 지정 처리
	CDBParamAttr& operator()(WCHAR* data, INT32& nBufferLength)
	{
		m_dbParamAttr.SetParamAttr(
			odbc_param_attr<WCHAR*>::c_data_type,
			odbc_param_attr<WCHAR*>::sql_data_type,
			odbc_param_attr<WCHAR*>::column_size
		);

		INT32 nDataLen = static_cast<INT32>(std::wcslen(data) * sizeof(WCHAR));
		m_dbParamAttr.SetValue(data, nBufferLength, nDataLen);
		m_dbParamAttr.m_ulColumnSize = static_cast<SQLULEN>(nBufferLength / sizeof(WCHAR));

		return m_dbParamAttr;
	}

public:
	CDBParamAttr		m_dbParamAttr;
};