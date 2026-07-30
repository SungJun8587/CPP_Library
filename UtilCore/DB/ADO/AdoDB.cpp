
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
int CAdoDB::Connect(const EDBClass dbClass, LPCTSTR lptszConnstring, const int nTimeOut)
{
	HRESULT		hr;
	BOOL		bResult = FALSE;

	try
	{
		m_pCon.CreateInstance(__uuidof(Connection));

		m_pCon->ConnectionTimeout = nTimeOut;
		m_DbClass = dbClass;

		hr = m_pCon->Open((_bstr_t)lptszConnstring, L"", L"", -1);

		if( SUCCEEDED(hr) )
		{
			m_pCmd.CreateInstance(__uuidof(Command));
			m_pRs.CreateInstance(__uuidof(Recordset));
			bResult = ADO_OPEN_COMMAND_CREATE;

			// 연결 성공 시 서버 정보 로그 출력
			TCHAR tszHostInfo[128] = { 0, };
			TCHAR tszDBMSName[128] = { 0, };
			TCHAR tszServerVersion[128] = { 0, };
			TCHAR tszCharacterSetName[128] = { 0, };

			bool bHostInfoResult = GetHostInfo(tszHostInfo, _countof(tszHostInfo));
			bool bNameResult = GetDBMSName(tszDBMSName, _countof(tszDBMSName));
			bool bVersionResult = GetServerVersion(tszServerVersion, _countof(tszServerVersion));
			bool bCharsetResult = GetCharacterSetName(tszCharacterSetName, _countof(tszCharacterSetName));

			if( bHostInfoResult && bNameResult && bVersionResult && bCharsetResult )
			{
				LOG_DEBUG(_T("%s, Server: %s, DBMS: %s, Version: %s, Charset: %s"),
					__TFUNCTION__, tszHostInfo, tszDBMSName, tszServerVersion, tszCharacterSetName);
			}
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
// @brief 인덱스를 이용해 필드 값을 문자열(LPTSTR)로 조회
// @param x          - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param lptszValue - 값을 저장할 버퍼 (출력)
// @param nValueLen  - 버퍼의 최대 크기 (문자 단위)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, LPTSTR lptszValue, int nValueLen)
{
	_variant_t	vData;
	_bstr_t		strFieldValue;

	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		strFieldValue = (_bstr_t)vData;
		_tcscpy_s(lptszValue, nValueLen, (LPTSTR)strFieldValue);
	}
	else
	{
		lptszValue[0] = _T('\0');
	}
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 long형으로 조회
// @param x           - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param lFieldValue - 값을 저장할 long 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, long& lFieldValue)
{
	_variant_t vData;
	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		if( vData.vt == VT_I2 )
			lFieldValue = vData.iVal;
		else if( vData.vt == VT_I4 || vData.vt == VT_INT )
			lFieldValue = vData.lVal;
		else
			lFieldValue = static_cast<long>(vData);
	}
	else
	{
		lFieldValue = 0;
	}
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 int32형으로 조회
// @param x            - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param nFieldValue  - 값을 저장할 int32 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, int32& nFieldValue)
{
	long lVal = 0;
	GetFieldByIndex(x, lVal);
	nFieldValue = static_cast<int32>(lVal);
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 ulong형으로 조회
// @param x            - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param ulFieldValue - 값을 저장할 ulong 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, ulong& ulFieldValue)
{
	_variant_t vData;
	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		ulFieldValue = static_cast<ulong>(vData);
	}
	else
	{
		ulFieldValue = 0;
	}
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 uint32형으로 조회
// @param x           - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param uFieldValue - 값을 저장할 uint32 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, uint32& uFieldValue)
{
	ulong ulVal = 0;
	GetFieldByIndex(x, ulVal);
	uFieldValue = static_cast<uint32>(ulVal);
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 double형으로 조회
// @param x             - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param dblFieldValue - 값을 저장할 double 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, double& dblFieldValue)
{
	_variant_t vData;
	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		dblFieldValue = static_cast<double>(vData);
	}
	else
	{
		dblFieldValue = 0.0;
	}
}

//***************************************************************************
// @brief 인덱스를 이용해 필드 값을 _tstring형으로 조회
// @param x             - 필드 인덱스 또는 이름 (_variant_t 타입)
// @param strFieldValue - 값을 저장할 _tstring 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByIndex(_variant_t x, _tstring& strFieldValue)
{
	_variant_t vData;
	vData = m_pRs->Fields->Item[x]->Value;

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		_bstr_t bstrVal = (_bstr_t)vData;
#ifdef UNICODE
		strFieldValue = static_cast<LPCTSTR>(bstrVal);
#else
		// ANSI 환경일 경우 _bstr_t(UTF-16)를 멀티바이트 문자열로 변환
		strFieldValue = static_cast<const char*>(bstrVal);
#endif
	}
	else
	{
		strFieldValue.clear();
	}
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 문자열(LPTSTR)로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param lptszValue     - 값을 저장할 버퍼 (출력)
// @param nValueLen      - 버퍼의 최대 크기 (문자 단위)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, LPTSTR lptszValue, int nValueLen)
{
	_variant_t	vData;
	_bstr_t		strFieldValue;

	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		strFieldValue = (_bstr_t)vData;
		_tcscpy_s(lptszValue, nValueLen, (LPTSTR)strFieldValue);
	}
	else
	{
		lptszValue[0] = _T('\0');
	}
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 long형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param lFieldValue    - 값을 저장할 long 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, long& lFieldValue)
{
	_variant_t	vData;

	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		if( vData.vt == VT_I2 )
			lFieldValue = vData.iVal;
		else
			lFieldValue = vData.lVal;
	}
	else
	{
		lFieldValue = 0;
	}
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 int32형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param nFieldValue    - 값을 저장할 int32 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, int32& nFieldValue)
{
	long lVal = 0;
	GetFieldByName(lptszFieldName, lVal);
	nFieldValue = static_cast<int32>(lVal);
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 ulong형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param ulFieldValue   - 값을 저장할 ulong 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, ulong& ulFieldValue)
{
	_variant_t vData;
	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		ulFieldValue = static_cast<ulong>(vData);
	}
	else
	{
		ulFieldValue = 0;
	}
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 uint32형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param uFieldValue    - 값을 저장할 uint32 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, uint32& uFieldValue)
{
	ulong ulVal = 0;
	GetFieldByName(lptszFieldName, ulVal);
	uFieldValue = static_cast<uint32>(ulVal);
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 double형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param dblFieldValue  - 값을 저장할 double 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, double& dblFieldValue)
{
	_variant_t vData;
	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		dblFieldValue = static_cast<double>(vData);
	}
	else
	{
		dblFieldValue = 0.0;
	}
}

//***************************************************************************
// @brief 필드 이름을 이용해 필드 값을 _tstring형으로 조회
// @param lptszFieldName - 필드 이름 (LPCTSTR)
// @param strFieldValue  - 값을 저장할 _tstring 변수 (출력)
//***************************************************************************
void CAdoDB::GetFieldByName(LPCTSTR lptszFieldName, _tstring& strFieldValue)
{
	_variant_t vData;
	vData = m_pRs->GetCollect((_variant_t)lptszFieldName);

	if( vData.vt != VT_NULL && vData.vt != VT_EMPTY )
	{
		_bstr_t bstrVal = (_bstr_t)vData;
#ifdef UNICODE
		strFieldValue = static_cast<LPCTSTR>(bstrVal);
#else
		strFieldValue = static_cast<const char*>(bstrVal);
#endif
	}
	else
	{
		strFieldValue.clear();
	}
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

//***************************************************************************
// @brief 각 DBMS별 서버 호스트 및 주소 정보를 조회
// @param lptszOut - 호스트 정보를 저장할 버퍼 (출력)
// @param nMaxLen - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CAdoDB::GetHostInfo(LPTSTR lptszOut, int nMaxLen)
{
	if( !ISOpen() || lptszOut == nullptr || nMaxLen <= 0 )
		return FALSE;

	TCHAR szQuery[256] = { 0, };

	switch( m_DbClass )
	{
		case EDBClass::MSSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT @@SERVERNAME"));
			break;

		case EDBClass::MYSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT @@hostname"));
			break;

		case EDBClass::ORACLE:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT SYS_CONTEXT('USERENV', 'SERVER_HOST') FROM DUAL"));
			break;

		default:
			_tcscpy_s(lptszOut, nMaxLen, _T("Localhost"));
			return FALSE;
	}

	try
	{
		_RecordsetPtr pRs = m_pCon->Execute(_bstr_t(szQuery), NULL, adCmdText);
		if( pRs != nullptr && !pRs->adoEOF )
		{
			_variant_t vt = pRs->Fields->Item[0L]->Value;
			if( vt.vt != VT_NULL && vt.vt != VT_EMPTY )
			{
				_bstr_t bstrVal = (_bstr_t)vt;
				_tcscpy_s(lptszOut, nMaxLen, (LPCTSTR)bstrVal);
				pRs->Close();
				return TRUE;
			}
			pRs->Close();
		}
	}
	catch( ... ) {}

	_tcscpy_s(lptszOut, nMaxLen, _T("Localhost"));
	return FALSE;
}

//***************************************************************************
// @brief 각 DBMS별 서버 제품명/종류 정보를 조회
// @param lptszOut - 서버 정보를 저장할 버퍼 (출력)
// @param nMaxLen - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CAdoDB::GetDBMSName(LPTSTR lptszOut, int nMaxLen)
{
	if( !ISOpen() || lptszOut == nullptr || nMaxLen <= 0 )
		return FALSE;

	TCHAR szQuery[256] = { 0, };

	switch( m_DbClass )
	{
		case EDBClass::MSSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT 'Microsoft SQL Server'"));
			break;

		case EDBClass::MYSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT 'MySQL'"));
			break;

		case EDBClass::ORACLE:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT 'Oracle'"));
			break;

		default:
			_tcscpy_s(lptszOut, nMaxLen, _T("Unknown DBMS"));
			return FALSE;
	}

	try
	{
		_RecordsetPtr pRs = m_pCon->Execute(_bstr_t(szQuery), NULL, adCmdText);
		if( pRs != nullptr && !pRs->adoEOF )
		{
			_variant_t vt = pRs->Fields->Item[0L]->Value;
			if( vt.vt != VT_NULL && vt.vt != VT_EMPTY )
			{
				_bstr_t bstrVal = (_bstr_t)vt;
				_tcscpy_s(lptszOut, nMaxLen, (LPCTSTR)bstrVal);
				pRs->Close();
				return TRUE;
			}
			pRs->Close();
		}
	}
	catch( ... ) {}

	_tcscpy_s(lptszOut, nMaxLen, _T("Unknown"));
	return FALSE;
}

//***************************************************************************
// @brief 각 DBMS별 서버 버전 정보를 문자열로 조회
// @param pszVersion - 버전 문자열을 저장할 버퍼 (LPTSTR)
// @param nMaxLen    - 버퍼의 최대 크기 (문자 단위, 보통 _countof(버퍼)로 전달)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CAdoDB::GetServerVersion(LPTSTR pszVersion, int nMaxLen)
{
	// 연결이 안되어 있거나 버퍼가 유효하지 않으면 실패
	if( !ISOpen() || pszVersion == nullptr || nMaxLen == 0 )
		return FALSE;

	// 버퍼 초기화
	pszVersion[0] = _T('\0');

	TCHAR szQuery[256] = { 0, };

	switch( m_DbClass )
	{
	case EDBClass::MSSQL:
		_stprintf_s(szQuery, _countof(szQuery), _T("SELECT CONVERT(varchar(128), SERVERPROPERTY('ProductVersion'))"));
		break;

	case EDBClass::MYSQL:
		_stprintf_s(szQuery, _countof(szQuery), _T("SELECT VERSION()"));
		break;

	case EDBClass::ORACLE:
		_stprintf_s(szQuery, _countof(szQuery), _T("SELECT BANNER FROM V$VERSION WHERE ROWNUM = 1"));
		break;

	default:
		return FALSE;
	}

	try
	{
		_RecordsetPtr pRs = m_pCon->Execute(_bstr_t(szQuery), NULL, adCmdText);
		if( pRs != nullptr && !pRs->adoEOF )
		{
			_variant_t vt = pRs->Fields->Item[0L]->Value;
			if( vt.vt != VT_NULL && vt.vt != VT_EMPTY )
			{
				_bstr_t bstrVal = (_bstr_t)vt;

				// 조회된 문자열을 전달받은 LPTSTR 버퍼에 안전하게 복사
				_tcscpy_s(pszVersion, nMaxLen, (LPCTSTR)bstrVal);

				pRs->Close();
				return TRUE;
			}
			pRs->Close();
		}
	}
	catch( ... ) {}

	return FALSE;
}

//***************************************************************************
// @brief 각 DBMS별 서버 캐릭터셋(인코딩) 정보를 조회
// @param lptszOut - 캐릭터셋 이름을 저장할 버퍼 (출력)
// @param nMaxLen - 버퍼 크기 (TCHAR 단위)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CAdoDB::GetCharacterSetName(LPTSTR lptszOut, int nMaxLen)
{
	if( !ISOpen() || lptszOut == nullptr || nMaxLen <= 0 )
		return FALSE;

	TCHAR szQuery[256] = { 0, };

	switch( m_DbClass )
	{
		case EDBClass::MSSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT CONVERT(varchar(128), SERVERPROPERTY('Collation'))"));
			break;

		case EDBClass::MYSQL:
			_stprintf_s(szQuery, _countof(szQuery), _T("SHOW VARIABLES LIKE 'character_set_server'"));
			break;

		case EDBClass::ORACLE:
			_stprintf_s(szQuery, _countof(szQuery), _T("SELECT VALUE FROM NLS_DATABASE_PARAMETERS WHERE PARAMETER = 'NLS_CHARACTERSET'"));
			break;

		default:
			_tcscpy_s(lptszOut, nMaxLen, _T("UTF-8"));
			return FALSE;
	}

	try
	{
		_RecordsetPtr pRs = m_pCon->Execute(_bstr_t(szQuery), NULL, adCmdText);
		if( pRs != nullptr && !pRs->adoEOF )
		{
			// MySQL의 SHOW VARIABLES는 결과의 두 번째 컬럼(Index 1)에 값이 위치
			long nColIdx = (m_DbClass == EDBClass::MYSQL) ? 1L : 0L;

			_variant_t vt = pRs->Fields->Item[nColIdx]->Value;
			if( vt.vt != VT_NULL && vt.vt != VT_EMPTY )
			{
				_bstr_t bstrVal = (_bstr_t)vt;
				_tcscpy_s(lptszOut, nMaxLen, (LPCTSTR)bstrVal);
				pRs->Close();
				return TRUE;
			}
			pRs->Close();
		}
	}
	catch( ... ) {}

	_tcscpy_s(lptszOut, nMaxLen, _T("UTF-8"));
	return FALSE;
}