
//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindParamInput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
		0, dbParam.m_ptrBuff, 0, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindParamInput(int32 iParamIndex, _TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
									  0, dbParam.m_ptrBuff, 0, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindParamOutput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
		0, dbParam.m_ptrBuff, 0, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindParamOutput(int32 iParamIndex, _TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
									  0, dbParam.m_ptrBuff, 0, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindCol(_TMain& tValue)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	SQLRETURN nRet = SQLBindCol(m_hStmt, ++m_nColNum, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, NULL);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::BindCol(int32 iColIndex, _TMain& tValue, SQLLEN& lRetSize)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, &lRetSize);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%lld], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
//	
template< typename _TMain >
bool CBaseODBC::GetData(int32 iColNum, _TMain& tValue)
{
	long		lRetSize = 0;
	SQLRETURN	nRet;

	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	nRet = SQLGetData(m_hStmt, iColNum, dbCol.m_nTargetType, &dbCol.m_ptrBuff, dbCol.m_nBuffSize, (SQLLEN*)&lRetSize);
	if( lRetSize == SQL_NO_TOTAL || lRetSize == SQL_NULL_DATA )
		return false;
	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}