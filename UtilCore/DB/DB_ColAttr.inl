template<SQLSMALLINT TARGET_TYPE>
struct odbc_col_attr_base
{
	static SQLSMALLINT	const target_type = TARGET_TYPE;
};


template<typename _TMain>
struct odbc_col_attr : odbc_col_attr_base<SQL_C_DEFAULT>
{
};


#define ODBC_COL_ATTR(d_type, t_type) \
	template<> \
	struct odbc_col_attr<d_type> : odbc_col_attr_base<t_type> \
	{}

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


class CDBColAttr
{
public:
	CDBColAttr(void)
	{
		m_nTargetType = SQL_C_DEFAULT;
		m_ptrBuff = nullptr;
		m_nBuffSize = 0;
	}

	void SetColAttr(SQLSMALLINT nTargetType)
	{
		m_nTargetType = nTargetType;
	}

	void SetValue(void* ptrBuff, int32 nBuffSize)
	{
		m_ptrBuff = ptrBuff;
		m_nBuffSize = nBuffSize;
	}

public:
	SQLSMALLINT		m_nTargetType;
	SQLPOINTER		m_ptrBuff;
	int32			m_nBuffSize;
};


class CDBColAttrMgr
{
public:
	CDBColAttrMgr(void) {
	}

	template<typename EDataType>
	CDBColAttr& operator()(EDataType& data)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<EDataType>::target_type);
		m_dbColAttr.SetValue(&data, sizeof(EDataType));
		return m_dbColAttr;
	}

	CDBColAttr& operator()(CHAR* data, int32& nBuffSize)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<CHAR*>::target_type);
		m_dbColAttr.SetValue(data, nBuffSize);
		return m_dbColAttr;
	}

	CDBColAttr& operator()(WCHAR* data, int32& nBuffSize)
	{
		m_dbColAttr.SetColAttr(odbc_col_attr<WCHAR*>::target_type);
		m_dbColAttr.SetValue(data, nBuffSize * 2);
		return m_dbColAttr;
	}

public:
	CDBColAttr		m_dbColAttr;
};
