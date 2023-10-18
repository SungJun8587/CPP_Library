//***************************************************************************
// 
template< typename _TMain >
bool CBaseMySQL::BindParam(_TMain tValue)
{
	if( m_pStmt == NULL )
	{
		TCHAR	tszMessage[MYSQL_MAX_MESSAGE_LENGTH] = { 0, };
		GetErrorMessage(tszMessage);
		LOG_ERROR(_T("%s, ErrorMsg : %s"), __TFUNCTION__, _T("Bind Error - Not called Prepare Function"));
		return false;
	}

	if( m_uiParamNum + 1 > m_uiBindCount )
	{
		LOG_ERROR(_T("%s, ErrorMsg : Bind Error - Index(%d) is not correct - Bind Count(%d)"), __TFUNCTION__, m_uiParamNum, m_uiBindCount);
		return false;
	}

	CMySQLParamAttr dbParam = m_DBParamAttrMgr(tValue);

	m_pBind[m_uiParamNum].buffer_type = dbParam.m_nTargetType;
	m_pBind[m_uiParamNum].is_unsigned = dbParam.m_bIsUnsigned;
	m_pBind[m_uiParamNum].buffer = new _TMain[1];
	*((_TMain*)m_pBind[m_uiParamNum].buffer) = tValue;
	m_uiParamNum++;

	return true;
}