template<enum_field_types TARGET_TYPE, bool IS_UNSIGNED = false>
struct mysql_param_attr_base
{
	static enum_field_types	const target_type = TARGET_TYPE;
	static bool	const is_unsigned = IS_UNSIGNED;
};


template<typename _TMain>
struct mysql_param_attr : mysql_param_attr_base<MYSQL_TYPE_NULL>
{
};


#define MYSQL_PARAM_ATTR(d_type, t_type, is_unsigned) \
	template<> \
	struct mysql_param_attr<d_type> : mysql_param_attr_base<t_type, is_unsigned> \
	{}

MYSQL_PARAM_ATTR(bool, MYSQL_TYPE_TINY, 0);
MYSQL_PARAM_ATTR(INT8, MYSQL_TYPE_TINY, false);
MYSQL_PARAM_ATTR(UINT8, MYSQL_TYPE_TINY, true);
MYSQL_PARAM_ATTR(INT16, MYSQL_TYPE_SHORT, false);
MYSQL_PARAM_ATTR(UINT16, MYSQL_TYPE_SHORT, true);
MYSQL_PARAM_ATTR(INT32, MYSQL_TYPE_LONG, false);
MYSQL_PARAM_ATTR(UINT32, MYSQL_TYPE_LONG, true);
MYSQL_PARAM_ATTR(INT64, MYSQL_TYPE_LONGLONG, false);
MYSQL_PARAM_ATTR(UINT64, MYSQL_TYPE_LONGLONG, true);
MYSQL_PARAM_ATTR(FLOAT, MYSQL_TYPE_FLOAT, false);
MYSQL_PARAM_ATTR(DOUBLE, MYSQL_TYPE_DOUBLE, false);
MYSQL_PARAM_ATTR(CHAR*, MYSQL_TYPE_STRING, false);
MYSQL_PARAM_ATTR(WCHAR*, MYSQL_TYPE_TINY_BLOB, false);
MYSQL_PARAM_ATTR(MYSQL_TIME, MYSQL_TYPE_TIMESTAMP, false);

class CMySQLParamAttr
{
public:
	CMySQLParamAttr()
	{
		m_nTargetType = MYSQL_TYPE_NULL;
		m_ptrBuff = nullptr;
		m_nBuffSize = 0;
		m_bIsUnsigned = false;
	}

	void SetParamAttr(enum_field_types nTargetType, bool bIsUnsigned)
	{
		m_nTargetType = nTargetType;
		m_bIsUnsigned = bIsUnsigned;
	}

	void SetValue(void* ptrBuff, int32 nBuffSize)
	{
		m_ptrBuff = ptrBuff;
		m_nBuffSize = nBuffSize;
	}

public:
	enum_field_types	m_nTargetType;
	void*				m_ptrBuff;
	int32				m_nBuffSize;
	bool				m_bIsUnsigned;

};

class CMySQLParamAttrMgr
{
public:
	CMySQLParamAttrMgr(void) {}

	template<typename EDataType>
	CMySQLParamAttr& operator()(EDataType& data)
	{
		m_dbParamAttr.SetParamAttr(mysql_param_attr<EDataType>::target_type, mysql_param_attr<EDataType>::is_unsigned);
		m_dbParamAttr.SetValue(&data, sizeof(EDataType));
		return m_dbParamAttr;
	}

	CMySQLParamAttr& operator()(CHAR* data, int32& nBuffSize)
	{
		m_dbParamAttr.SetParamAttr(mysql_param_attr<CHAR*>::target_type, mysql_param_attr<CHAR*>::is_unsigned);
		m_dbParamAttr.SetValue(data, nBuffSize);
		return m_dbParamAttr;
	}

	CMySQLParamAttr& operator()(WCHAR* data, int32& nBuffSize)
	{
		m_dbParamAttr.SetParamAttr(mysql_param_attr<WCHAR*>::target_type, mysql_param_attr<WCHAR*>::is_unsigned);
		m_dbParamAttr.SetValue(data, nBuffSize * 2);
		return m_dbParamAttr;
	}

public:
	CMySQLParamAttr		m_dbParamAttr;
};
