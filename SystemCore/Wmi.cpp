
//***************************************************************************
// Wmi.cpp: implementation of the CWmi class.
//
//***************************************************************************

#include "pch.h"
#include "Wmi.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CWmi::CWmi()
{
	// STEP 1. COM을 초기화한다.
	CoInitializeEx(0, COINIT_MULTITHREADED);

	m_pIWbemLocator = NULL;
	m_pIWbemServices = NULL;
}

CWmi::~CWmi()
{
	IWbemClassObject		*pClass = NULL;

	for( int i = 0; i < MemClassObject.GetCount(); i++ )
	{
		pClass = MemClassObject.At(i);
		if( pClass )
		{
			pClass->Release();
			pClass = NULL;
		}
	}

	if( m_pIWbemServices )
	{
		m_pIWbemServices->Release();
		m_pIWbemServices = NULL;
	}

	if( m_pIWbemLocator )
	{
		m_pIWbemLocator->Release();
		m_pIWbemLocator = NULL;
	}

	CoUninitialize();
}

//***************************************************************************
//
BOOL CWmi::Connect(TCHAR *ptszHost, TCHAR *ptszUserName, TCHAR *ptszUserPass)
{
	WCHAR   wszBuffer[MAX_BUFFER_SIZE];
	wchar_t	*pwszHost = NULL;
	wchar_t	*pwszUserName = NULL;
	wchar_t	*pwszUserPass = NULL;

	HRESULT hr;

	if( ptszHost != NULL && ptszUserName != NULL && ptszUserPass != NULL )
	{
#ifdef _UNICODE
		pwszHost = (TCHAR *)ptszHost;
		pwszUserName = (TCHAR *)ptszUserName;
		pwszUserPass = (TCHAR *)ptszUserPass;
#else
		CMemBuffer<wchar_t>	WHost;
		CMemBuffer<wchar_t>	WUserName;
		CMemBuffer<wchar_t>	WUserPass;

		if( !AnsiToUnicode(WHost, ptszHost, strlen(ptszHost)) ) return false;

		pwszHost = WHost.GetBuffer();

		if( !AnsiToUnicode(WUserName, ptszUserName, strlen(ptszUserName)) ) return false;

		pwszUserName = WUserName.GetBuffer();

		if( !AnsiToUnicode(WUserPass, ptszUserPass, strlen(ptszUserPass)) ) return false;

		pwszUserPass = WUserPass.GetBuffer();
#endif
	}

	// STEP 1. WMI에 대한 초기 locator를 획득한다.
	hr = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator,
		(LPVOID*)&m_pIWbemLocator
	);
	if( FAILED(hr) )
	{
		CoUninitialize();

		return false;
	}

	if( pwszHost == NULL || pwszUserName == NULL || pwszUserPass == NULL )
	{
		// STEP 2. IWbemLocator::ConnectServer()를 이용해 WMI에 접속한다.
		hr = m_pIWbemLocator->ConnectServer(
			_bstr_t(L"ROOT\\CIMV2"),		// Object path of WMI namespace
			NULL,							// User name. NULL = current user
			NULL,							// User password. NULL = current
			0,								// Locale
			NULL,							// Security flags
			0,								// Authority
			0,								// Context object
			&m_pIWbemServices				// Pointer to IWbemServices proxy
		);
	}
	else
	{
		StringCchPrintfW(wszBuffer, _countof(wszBuffer), L"\\\\%s\\ROOT\\CIMV2", pwszHost);

		hr = m_pIWbemLocator->ConnectServer(
			_bstr_t(wszBuffer),			// Object path of WMI namespace
			_bstr_t(pwszUserName),		// User name. NULL = current user
			_bstr_t(pwszUserPass),		// User password. NULL = current
			0,								// Locale
			NULL,							// Security flags
			0,								// Authority
			0,								// Context object
			&m_pIWbemServices				// Pointer to IWbemServices proxy
		);
	}

	if( FAILED(hr) )
	{
		m_pIWbemLocator->Release();
		m_pIWbemLocator = NULL;

		CoUninitialize();

		return false;
	}

	// STEP 3. Proxy의 Security Level을 설정한다.
	hr = CoSetProxyBlanket(
		m_pIWbemServices,            // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
	);
	if( FAILED(hr) )
	{
		m_pIWbemServices->Release();
		m_pIWbemServices = NULL;

		m_pIWbemLocator->Release();
		m_pIWbemLocator = NULL;

		CoUninitialize();

		return false;
	}

	return true;
}

//***************************************************************************
//
int CWmi::ExecQuery(const TCHAR *ptszQuery)
{
	HRESULT	hr;
	ULONG	ulCount = 1;
	ULONG	ulRet = 0;
	int		nIndex = 0;
	TCHAR	tszReqQuery[128];

	IEnumWbemClassObject	*pEnum = NULL;
	IWbemClassObject		*pClass = NULL;

	if( !m_pIWbemServices ) return -1;

	StringCchPrintf(tszReqQuery, _countof(tszReqQuery), _T("SELECT * FROM %s"), ptszQuery);

	hr = m_pIWbemServices->ExecQuery(
		bstr_t(L"WQL"),
		bstr_t(tszReqQuery),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnum
	);
	if( FAILED(hr) ) return -1;

	pEnum->Reset();

	if( MemClassObject.GetCount() > 0 )
	{
		for( int i = 0; i < MemClassObject.GetCount(); i++ )
		{
			pClass = MemClassObject.At(i);
			if( pClass )
			{
				pClass->Release();
				pClass = NULL;
			}
		}

		MemClassObject.Reset();
	}

	while( pEnum->Next(WBEM_INFINITE, ulCount, &pClass, &ulRet) == WBEM_NO_ERROR )
	{
		MemClassObject.Add(pClass);

		nIndex++;
	}

	if( pEnum )
	{
		pEnum->Release();
		pEnum = NULL;
	}

	return nIndex;
}

//***************************************************************************
//
BOOL CWmi::GetProperties(int nIndex, const TCHAR *ptszProperty, VARIANT &vtVal)
{
	USES_CONVERSION;

	HRESULT	hr;

	IWbemClassObject		*pClass = NULL;

	if( nIndex > MemClassObject.GetCount() ) return false;

	pClass = MemClassObject.At(nIndex);
	if( !pClass ) return false;

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return false;

	return true;
}

//***************************************************************************
//
BOOL CWmi::GetProperties(int nIndex, const TCHAR *ptszProperty, TCHAR *ptszValue, DWORD dwSize)
{
	USES_CONVERSION;

	HRESULT	hr;
	VARIANT	vtVal;

	IWbemClassObject		*pClass = NULL;

	if( nIndex > MemClassObject.GetCount() ) return false;

	pClass = MemClassObject.At(nIndex);
	if( !pClass ) return false;

	VariantInit(&vtVal);

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return false;

	if( vtVal.vt == VT_BSTR )
	{
#ifdef _UNICODE	
		StringCchCopy(ptszValue, dwSize - 1, OLE2W(vtVal.bstrVal));
#else
		StringCchCopy(ptszValue, dwSize - 1, OLE2A(vtVal.bstrVal));
#endif
	}

	VariantClear(&vtVal);

	return true;
}

//***************************************************************************
//
BOOL CWmi::GetProperties(int nIndex, const TCHAR *ptszProperty, long *plValue)
{
	HRESULT	hr;
	VARIANT	vtVal;

	IWbemClassObject		*pClass = NULL;

	if( nIndex > MemClassObject.GetCount() ) return false;

	pClass = MemClassObject.At(nIndex);
	if( !pClass ) return false;

	VariantInit(&vtVal);

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return false;

	if( vtVal.vt == VT_I4 )
		*plValue = vtVal.lVal;

	VariantClear(&vtVal);

	return true;
}

