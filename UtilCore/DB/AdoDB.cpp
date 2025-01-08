
//***************************************************************************
// AdoDB.cpp : implementation of the CAdoDB class.
//
//***************************************************************************

#include "pch.h"
#include "AdoDB.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CAdoDB::CAdoDB()
{
	CoInitialize(NULL);

	m_pRs = NULL;
	m_pCmd = NULL;
	m_pCon = NULL;
}

CAdoDB::~CAdoDB()
{
	if( ISOpen() )
	{
		m_pCon->Close();
		m_pCon = NULL;
	}
	if( ISRSCon() )
	{
		m_pRs->Close();
		m_pRs = NULL;
	}
	if( ISCommand() )
	{
		m_pCmd->Cancel();
		m_pCmd = NULL;
	}

	::CoUninitialize();
}

//***************************************************************************
//
BOOL CAdoDB::ISConnect()
{
	return ((m_pCon->GetState() != 0) ? TRUE : FALSE);
}

//***************************************************************************
//
BOOL CAdoDB::ISRSCon()
{
	return ((m_pRs->GetState() != 0) ? TRUE : FALSE);
}

//***************************************************************************
//
BOOL CAdoDB::ISOpen()
{
	return ((m_pCon->GetState() != 0) ? TRUE : FALSE);
}

//***************************************************************************
//
BOOL CAdoDB::ISCommand()
{
	return ((m_pCmd->GetState() != 0) ? TRUE : FALSE);
}

//***************************************************************************
//
int CAdoDB::DBConnect(LPCTSTR lptszConnstring, const int nTimeOut)
{
	HRESULT		hr;
	BOOL		bResult = FALSE;

	try
	{
		m_pCon.CreateInstance(__uuidof(Connection));

		m_pCon->ConnectionTimeout = nTimeOut;

		hr = m_pCon->Open((_bstr_t)lptszConnstring, L"", L"", -1);

		if( SUCCEEDED(hr) )
		{
			m_pCmd.CreateInstance(__uuidof(Command));
			m_pRs.CreateInstance(__uuidof(Recordset));
			bResult = ADO_OPEN_COMMAND_CREATE;
		}
		else bResult = ADO_OPEN_ERROR;
	}
	catch( ... )
	{
		bResult = ADO_OPEN_ERROR;
	}

	return bResult;
}

//***************************************************************************
//
_CommandPtr CAdoDB::GetCmdPointer()
{
	return m_pCmd;
}

//***************************************************************************
//
_RecordsetPtr CAdoDB::GetRecPointer()
{
	return m_pRs;
}

//***************************************************************************
//
BOOL CAdoDB::GetDBCon()
{
	return	ISOpen();
}

//***************************************************************************
//
void CAdoDB::ConClose()
{
	if( ISOpen() )	m_pCon->Close();
}

//***************************************************************************
//
void CAdoDB::RSClose()
{
	if( ISRSCon() )	m_pRs->Close();
}

//***************************************************************************
//
long CAdoDB::ConBeginTrans()
{
	return m_pCon->BeginTrans();
}

//***************************************************************************
//
void CAdoDB::ConCommitTrans()
{
	m_pCon->CommitTrans();
}

//***************************************************************************
//
void CAdoDB::ConRollbackTrans()
{
	m_pCon->RollbackTrans();
}

//***************************************************************************
//
void CAdoDB::ConCancel()
{
	m_pCon->Cancel();
}

//***************************************************************************
//
BOOL CAdoDB::IsEOF()
{
	return m_pRs->adoEOF;
}

//***************************************************************************
//
BOOL CAdoDB::Next()
{
	return (FAILED(m_pRs->MoveNext()) ? FALSE : TRUE);
}

//***************************************************************************
//
BOOL CAdoDB::Prev()
{
	return (FAILED(m_pRs->MovePrevious()) ? FALSE : TRUE);
}

//***************************************************************************
//
BOOL CAdoDB::First()
{
	return (FAILED(m_pRs->MoveFirst()) ? FALSE : TRUE);
}

//***************************************************************************
//
BOOL CAdoDB::Last()
{
	return (FAILED(m_pRs->MoveLast()) ? FALSE : TRUE);
}

//***************************************************************************
//
int CAdoDB::GetRecCount()
{
	return	(int)m_pRs->GetRecordCount();
}

//***************************************************************************
//
int CAdoDB::GetFieldCount()
{
	return	(int)m_pRs->Fields->GetCount();
}

//***************************************************************************
//
void CAdoDB::GetFieldByIndex(_variant_t x, LPTSTR lptszValue, int nValueLen)
{
	_variant_t	vData;
	_bstr_t		strFieldValue;

	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL )
	{
		strFieldValue = (_bstr_t)vData;

		_tcscpy_s(lptszValue, nValueLen, (LPTSTR)strFieldValue);
	}
	else lptszValue[0] = _T('\0');
}

//***************************************************************************
//
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, LPTSTR lptszValue, int nValueLen)
{
	_variant_t	vData;
	_bstr_t		strFieldValue;

	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL )
	{
		strFieldValue = (_bstr_t)vData;

		_tcscpy_s(lptszValue, nValueLen, (LPTSTR)strFieldValue);
	}
	else lptszValue[0] = _T('\0');
}

//***************************************************************************
//
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, long& lFieldValue)
{
	_variant_t	vData;

	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt == VT_I2 )
		lFieldValue = vData.iVal;
	else lFieldValue = vData.lVal;
}

//***************************************************************************
//
BOOL CAdoDB::Open(LPCTSTR lptszSourceBuf, const long lOption)
{
	if( ISOpen() )
	{
		if( m_pRs == NULL ) m_pRs.CreateInstance(__uuidof(Recordset));
		m_pRs->PutRefActiveConnection(m_pCon);

		HRESULT		hr;
		m_pRs->CursorType = adOpenStatic;
		hr = m_pRs->Open(lptszSourceBuf, vtMissing, adOpenKeyset, adLockReadOnly, lOption);

		if( SUCCEEDED(hr) )
			return	TRUE;
		else return	FALSE;
	}
	else return	FALSE;
}

//***************************************************************************
//
BOOL CAdoDB::Execute(LPCTSTR lptszSourceBuf, const long lOption)
{
	if( ISOpen() )
	{
		m_pCmd->CommandText = lptszSourceBuf;
		m_pCmd->PutRefActiveConnection(m_pCon);

		m_pRs = m_pCmd->Execute(NULL, NULL, adCmdText);

		return	TRUE;
	}
	else return	FALSE;
}

//***************************************************************************
//
BOOL CAdoDB::StoredProcedureExecute(LPCTSTR lptszStoredName, const long lOption)
{
	if( ISOpen() )
	{
		m_pCmd->CommandText = lptszStoredName;
		m_pCmd->PutRefActiveConnection(m_pCon);

		m_pRs = m_pCmd->Execute(NULL, NULL, adCmdStoredProc);

		return	TRUE;
	}
	else return	FALSE;
}

//***************************************************************************
//
long CAdoDB::GetReturnValue()
{
	long retVal;

	retVal = m_pCmd->Parameters->Item[_variant_t("Return")]->Value;

	return retVal;
}

//***************************************************************************
//
void CAdoDB::CreateReturnParamAppend()
{
	_ParameterPtr	m_Param = NULL;

	m_Param = m_pCmd->CreateParameter(_bstr_t("Return"), adInteger, adParamReturnValue, 4);
	m_pCmd->Parameters->Append(m_Param);
}

//***************************************************************************
//
void CAdoDB::CreateArgParamAppend(_bstr_t bstrName, enum DataTypeEnum enumType, long lSize, _variant_t vt, BOOL bInOutCheck)
{
	_ParameterPtr	m_Param = NULL;

	if( bInOutCheck )
	{
		if( enumType == adVarChar && lSize == 0 )
		{
			_bstr_t		bstrParam(vt);

			if( bstrParam.length() == 0 )
			{
				VARIANT	varNULL;

				VariantInit(&varNULL);
				varNULL.vt = VT_NULL;
				_variant_t _varNULL(&varNULL);		// VARIANT SQL VT_NULL type

				m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamInput, 1, _varNULL);
			}
			else m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamInput, static_cast<long>(strlen((LPCSTR)bstrParam)), bstrParam);
		}
		else if( enumType == adInteger )
			m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamInput, 4, vt);
		else m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamInput, lSize, vt);
	}
	else
	{
		if( enumType == adInteger )
			m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamOutput, 4);
		else m_Param = m_pCmd->CreateParameter(bstrName, enumType, adParamOutput, lSize);
	}

	m_pCmd->Parameters->Append(m_Param);
}

//***************************************************************************
//
void CAdoDB::GetRs(_variant_t x, _bstr_t& ret)
{
	ret = m_pRs->Fields->Item[x]->Value;
}

//***************************************************************************
//
void CAdoDB::GetRs(_variant_t x, _variant_t& ret)
{
	ret = m_pRs->Fields->Item[x]->Value;
}

//***************************************************************************
//
void CAdoDB::GetRs(_variant_t x, float& ret)
{
	ret = m_pRs->Fields->Item[x]->Value;
}

//***************************************************************************
//
void CAdoDB::GetRs(_variant_t x, long& ret)
{
	ret = m_pRs->Fields->Item[x]->Value;
}

//***************************************************************************
//
void CAdoDB::GetRs(_variant_t x, double& ret)
{
	ret = m_pRs->Fields->Item[x]->Value;
}






















