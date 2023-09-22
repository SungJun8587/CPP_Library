//***************************************************************************
//	
template< typename _TMain >
BOOL CBaseODBC::BindParamInput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
									  0, dbParam.m_ptrBuff, dbParam.m_nBuffSize, (SQLLEN*)&dbParam.m_nBuffSize);

	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//	
template< typename _TMain >
BOOL CBaseODBC::BindParamOutput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_nParamSize,
									  0, dbParam.m_ptrBuff, dbParam.m_nBuffSize, (SQLLEN*)&dbParam.m_nBuffSize);

	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//	
template< typename _TMain >
BOOL CBaseODBC::BindCol(_TMain& tValue)
{
	long	lRetSize = 0;

	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	SQLRETURN nRet = SQLBindCol(m_hStmt, ++m_nColNum, dbCol.m_nTargetType, dbCol.m_ptrBuff, dbCol.m_nBuffSize, (SQLLEN*)&lRetSize);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], tValue[%d], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tValue, tszMessage);
		return false;
	}

	return true;
}

//***************************************************************************
//	
template< typename _TMain >
BOOL CBaseODBC::GetData(int nColNum, _TMain& tValue)
{
	long		lRetSize = 0;
	SQLRETURN	nRet;

	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	nRet = SQLGetData(m_hStmt, nColNum, dbCol.m_nTargetType, &dbCol.m_ptrBuff, dbCol.m_nBuffSize, (SQLLEN*)&lRetSize);

	if( lRetSize == SQL_NO_TOTAL || lRetSize == SQL_NULL_DATA )
		return false;
	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}