/// @brief 결과 컬럼 타겟 타입 매핑을 위한 기본 구조체
template<SQLSMALLINT TARGET_TYPE>
struct odbc_col_attr_base
{
	static SQLSMALLINT	const target_type = TARGET_TYPE;
};


/// @brief 기본 컬럼 속성 템플릿
template<typename _TMain>
struct odbc_col_attr : odbc_col_attr_base<SQL_C_DEFAULT>
{
};


/// @brief 결과 컬럼 타입 매핑을 위한 매크로
#define ODBC_COL_ATTR(d_type, t_type) \
	template<> \
	struct odbc_col_attr<d_type> : odbc_col_attr_base<t_type> \
	{}

// 결과 컬럼 타입별 대상 C 타입 매핑 정의
ODBC_COL_ATTR(bool, SQL_C_TINYINT);
ODBC_COL_ATTR(INT8, SQL_C_STINYINT);
ODBC_COL_ATTR(UINT8, SQL_C_UTINYINT);
ODBC_COL_ATTR(INT16, SQL_C_SSHORT);
ODBC_COL_ATTR(UINT16, SQL_C_USHORT);
ODBC_COL_ATTR(INT32, SQL_C_SLONG);
ODBC_COL_ATTR(UINT32, SQL_C_ULONG);
ODBC_COL_ATTR(INT64, SQL_C_SBIGINT);
ODBC_COL_ATTR(UINT64, SQL_C_UBIGINT);
ODBC_COL_ATTR(FLOAT, SQL_C_FLOAT);
ODBC_COL_ATTR(DOUBLE, SQL_C_DOUBLE);
ODBC_COL_ATTR(CHAR*, SQL_C_CHAR);
ODBC_COL_ATTR(WCHAR*, SQL_C_WCHAR);
ODBC_COL_ATTR(SQL_TIMESTAMP_STRUCT, SQL_C_TYPE_TIMESTAMP);


/// @brief 쿼리 결과 컬럼의 바인딩 속성과 버퍼 정보를 관리하는 클래스
class CDBColAttr
{
public:
	CDBColAttr(void)
	{
		m_nTargetType = SQL_C_DEFAULT;
		m_ptrBuffer = nullptr;
		m_nBufferLength = 0;
		m_lDataLength = 0;
	}

	/// @brief 타겟 C 타입 설정
	void SetColAttr(SQLSMALLINT nTargetType)
	{
		m_nTargetType = nTargetType;
	}

	/// @brief 결과 버퍼 포인터 및 용량, 데이터 길이 설정
	void SetValue(void* ptrBuffer, INT32 nBufferLength, INT64 lDataLength = 0)
	{
		m_ptrBuffer = ptrBuffer;
		m_nBufferLength = nBufferLength;
		m_lDataLength = lDataLength;
	}

public:
	SQLSMALLINT		m_nTargetType;			// 결과를 받을 때 사용할 C 데이터 타입 (SQLBindCol용)
	SQLPOINTER		m_ptrBuffer;			// 결과를 저장할 메모리 버퍼 포인터
	INT32			m_nBufferLength;		// 버퍼의 최대 할당 크기 (Bytes 단위, 오버플로우 방지용)
	INT64			m_lDataLength;			// 실제로 읽어온 데이터의 길이 (StrLen_or_Ind 등으로 바인딩될 값)
};


/// @brief 결과 컬럼 바인딩을 위한 속성 관리자 클래스
class CDBColAttrMgr
{
public:
	CDBColAttrMgr(void) {}

	/// @brief 일반 기본 타입 컬럼 결과 바인딩
	template<typename EDataType>
	CDBColAttr& operator()(EDataType& data)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<EDataType>::target_type);
		INT32 size = static_cast<INT32>(sizeof(EDataType));
		m_dbColAttr.SetValue(&data, size, size);
		return m_dbColAttr;
	}

	/// @brief ANSI 문자열(CHAR*) 컬럼 결과 바인딩
	CDBColAttr& operator()(CHAR* data, INT32& nBufferLength)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<CHAR*>::target_type);
		m_dbColAttr.SetValue(data, nBufferLength, 0);
		return m_dbColAttr;
	}

	/// @brief 유니코드 문자열(WCHAR*) 컬럼 결과 바인딩
	CDBColAttr& operator()(WCHAR* data, INT32& nBufferLength)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<WCHAR*>::target_type);
		INT32 nByteCapacity = nBufferLength * static_cast<INT32>(sizeof(WCHAR));
		m_dbColAttr.SetValue(data, nByteCapacity, 0);
		return m_dbColAttr;
	}

public:
	CDBColAttr		m_dbColAttr;
};