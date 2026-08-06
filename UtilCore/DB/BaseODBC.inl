
//***************************************************************************
// Template Functions
//***************************************************************************

//***************************************************************************
// @brief 템플릿을 사용하여 자동 증가하는 순번으로 입력(Input) 매개변수를 바인딩
// @param tValue - 바인딩할 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindParamInput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, (SQLLEN*)&dbParam.m_lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿과 인덱스를 지정하여 입력(Input) 매개변수를 바인딩
// @param iParamIndex - 매개변수 순번 (인덱스)
// @param tValue - 바인딩할 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindParamInput(int32 iParamIndex, _TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_INPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, (SQLLEN*)&dbParam.m_lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿을 사용하여 자동 증가하는 순번으로 출력(Output) 매개변수를 바인딩
// @param tValue - 출력값을 저장할 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindParamOutput(_TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, ++m_nParamNum, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, (SQLLEN*)&dbParam.m_lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿과 인덱스를 지정하여 출력(Output) 매개변수를 바인딩
// @param iParamIndex - 매개변수 순번 (인덱스)
// @param tValue - 출력값을 저장할 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindParamOutput(int32 iParamIndex, _TMain& tValue)
{
	CDBParamAttr& dbParam = m_DBParamAttrMgr(tValue);

	SQLRETURN nRet = SQLBindParameter(m_hStmt, iParamIndex, SQL_PARAM_OUTPUT, dbParam.m_nCDataType, dbParam.m_nSqlDataType, dbParam.m_ulColumnSize,
		0, dbParam.m_ptrBuffer, dbParam.m_nBufferLength, (SQLLEN*)&dbParam.m_lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿을 사용하여 자동 증가하는 순번으로 결과 셋의 컬럼을 바인딩
// @param tValue - 결과 값을 받을 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindCol(_TMain& tValue)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	SQLRETURN nRet = SQLBindCol(m_hStmt, ++m_nColNum, dbCol.m_nTargetType, dbCol.m_ptrBuffer, dbCol.m_nBufferLength, nullptr);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿과 인덱스를 지정하여 결과 셋의 컬럼을 바인딩
// @param iColIndex - 컬럼 순번 (인덱스)
// @param tValue - 결과 값을 받을 데이터 객체 (참조)
// @param lDataLength - 데이터 길이 참조
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::BindCol(int32 iColIndex, _TMain& tValue, SQLLEN& lDataLength)
{
	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	SQLRETURN nRet = SQLBindCol(m_hStmt, iColIndex, dbCol.m_nTargetType, dbCol.m_ptrBuffer, dbCol.m_nBufferLength, &lDataLength);
	if( SQL_SUCCESS != nRet )
	{
		TCHAR	tszMessage[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		CDBError()(SQL_HANDLE_STMT, m_hStmt, tszMessage);
		LOG_ERROR(_T("%s, QueryInfo[%s], ErrorMsg : %s"), __TFUNCTION__, m_tszQueryInfo, tszMessage);
		return false;
	}
	return true;
}

//***************************************************************************
// @brief 템플릿을 사용하여 인덱스 지정 컬럼의 데이터를 직접 가져옴
// @param iColNum - 컬럼 순번 (인덱스)
// @param tValue - 데이터를 저장할 데이터 객체 (참조)
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template< typename _TMain >
bool CBaseODBC::GetData(int32 iColNum, _TMain& tValue)
{
	SQLLEN		lDataLength;
	SQLRETURN	nRet;

	CDBColAttr& dbCol = m_DBColAttrMgr(tValue);

	nRet = SQLGetData(m_hStmt, iColNum, dbCol.m_nCDataType, dbCol.m_ptrBuffer, dbCol.m_nBufferLength, &lDataLength);
	if( lDataLength == SQL_NO_TOTAL || lDataLength == SQL_NULL_DATA )
		return false;
	return nRet == SQL_SUCCESS || nRet == SQL_SUCCESS_WITH_INFO;
}