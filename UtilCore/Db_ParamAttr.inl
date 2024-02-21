template<SQLSMALLINT CDATA_TYPE, SQLSMALLINT SQL_DATA_TYPE, SQLULEN PARAM_SIZE = 0>
struct odbc_param_attr_base
{
	static SQLSMALLINT	const c_data_type = CDATA_TYPE;
	static SQLSMALLINT	const sql_data_type = SQL_DATA_TYPE;
	static SQLULEN		const param_size = PARAM_SIZE;
};


template<typename _TMain>
struct odbc_param_attr : odbc_param_attr_base<SQL_C_DEFAULT, SQL_VARCHAR >
{
};


#define ODBC_PARAM_ATTR(d_type, c_type, sql_type, p_size) \
	template<> \
	struct odbc_param_attr<d_type> : odbc_param_attr_base<c_type, sql_type, p_size> \
	{}

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


class CDBParamAttr
{
public:
	CDBParamAttr()
	{
		m_nCDataType = SQL_C_DEFAULT;
		m_nSqlDataType = SQL_VARCHAR;
		m_nParamSize = 0;

		m_ptrBuff = nullptr;
		m_nBuffSize = 0;
	}

	void SetParamAttr(SQLSMALLINT nCDataType, SQLSMALLINT nSqlDataType, SQLULEN nParamSize)
	{
		m_nCDataType = nCDataType;
		m_nSqlDataType = nSqlDataType;
		m_nParamSize = nParamSize;
	}

	void SetValue(void* ptrBuff, int32 nBuffSize)
	{
		m_ptrBuff = ptrBuff;
		m_nBuffSize = nBuffSize;
	}

public:
	SQLSMALLINT		m_nCDataType;
	SQLSMALLINT		m_nSqlDataType;
	SQLULEN			m_nParamSize;

	SQLPOINTER		m_ptrBuff;
	int32			m_nBuffSize;
};


class CDBParamAttrMgr
{
public:
	CDBParamAttrMgr(void) {}

	template<typename EDataType>
	CDBParamAttr& operator()(EDataType& data)
	{
		m_dbParamAttr.SetParamAttr(odbc_param_attr<EDataType>::c_data_type, odbc_param_attr<EDataType>::sql_data_type, odbc_param_attr<EDataType>::param_size);
		m_dbParamAttr.SetValue(&data, sizeof(EDataType));

		return m_dbParamAttr;
	}

	CDBParamAttr& operator()(CHAR* data)
	{
		m_dbParamAttr.SetParamAttr(odbc_param_attr<CHAR*>::c_data_type, odbc_param_attr<CHAR*>::sql_data_type, odbc_param_attr<CHAR*>::param_size);

		int32 nBuffSize = static_cast<int32>(strlen(data));
		m_dbParamAttr.SetValue(data, nBuffSize);
		m_dbParamAttr.m_nParamSize = (SQLULEN)nBuffSize + 1;

		return m_dbParamAttr;
	}

	CDBParamAttr& operator()(WCHAR* data)
	{
		m_dbParamAttr.SetParamAttr(odbc_param_attr<WCHAR*>::c_data_type, odbc_param_attr<WCHAR*>::sql_data_type, odbc_param_attr<WCHAR*>::param_size);

		int32 nBuffSize = static_cast<int32>(wcslen(data) * 2);
		m_dbParamAttr.SetValue(data, nBuffSize);
		m_dbParamAttr.m_nParamSize = (SQLULEN)nBuffSize + 1;

		return m_dbParamAttr;
	}

	CDBParamAttr& operator()(CHAR* data, int32& nBuffSize)
	{
		m_dbParamAttr.SetParamAttr(odbc_param_attr<CHAR*>::c_data_type, odbc_param_attr<CHAR*>::sql_data_type, odbc_param_attr<CHAR*>::param_size);

		m_dbParamAttr.SetValue(data, static_cast<int32>(nBuffSize));
		m_dbParamAttr.m_nParamSize = (SQLULEN)nBuffSize + 1;

		return m_dbParamAttr;
	}

	CDBParamAttr& operator()(WCHAR* data, int32& nBuffSize)
	{
		m_dbParamAttr.SetParamAttr(odbc_param_attr<WCHAR*>::c_data_type, odbc_param_attr<WCHAR*>::sql_data_type, odbc_param_attr<WCHAR*>::param_size);

		nBuffSize = static_cast<int32>(nBuffSize * 2);
		m_dbParamAttr.SetValue(data, nBuffSize);
		m_dbParamAttr.m_nParamSize = (SQLULEN)nBuffSize + 1;

		return m_dbParamAttr;
	}

public:
	CDBParamAttr		m_dbParamAttr;
};
