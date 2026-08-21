
//***************************************************************************
// Wmi.cpp: implementation of the CEventLog class.
//
//***************************************************************************

#include "pch.h"
#include "Wmi.h"

//***************************************************************************
// @brief CWmi 생성자 - WMI 인터페이스 포인터를 초기화합니다.
// @note  COM 라이브러리 초기화(CoInitializeEx)/종료(CoUninitialize)는
//        프로세스/스레드 전역 자원이므로 CWmi가 아닌 호출자(main 등)가
//        책임지고 관리합니다. CWmi를 사용하기 전에 호출 스레드에서
//        CoInitializeEx()가 먼저 호출되어 있어야 합니다.
//***************************************************************************
CWmi::CWmi()
	: m_hrLastError(S_OK)
{
	m_pIWbemLocator = NULL;
	m_pIWbemServices = NULL;
}

//***************************************************************************
// @brief CWmi 소멸자 - 할당된 WMI 인터페이스 리소스를 해제합니다.
// @note  COM 종료(CoUninitialize)는 호출자 책임이므로 여기서 호출하지 않습니다.
//        (호출자가 COM을 아직 살아있는 상태로 유지하고 있을 때 소멸되어야
//        CComPtr::Release()가 안전합니다.)
//***************************************************************************
CWmi::~CWmi()
{
	Disconnect();
}

//***************************************************************************
// @brief 현재 연결을 해제하고 보유 중인 모든 WMI 인터페이스/결과 오브젝트를 정리합니다.
// @return void
//***************************************************************************
void CWmi::Disconnect()
{
	m_vecClassObject.clear();
	m_pIWbemServices.Release();
	m_pIWbemLocator.Release();
}

//***************************************************************************
// @brief WMI 서비스에 연결합니다.
// @param ptszHost 접속할 호스트명 또는 IP 주소 (NULL일 경우 로컬 컴퓨터)
// @param ptszUserName 접속할 사용자 계정명 (NULL일 경우 현재 사용자)
// @param ptszUserPass 접속할 사용자 비밀번호 (NULL일 경우 현재 사용자)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CWmi::Connect(TCHAR* ptszHost, TCHAR* ptszUserName, TCHAR* ptszUserPass)
{
	// [수정] 재연결 시 기존에 보유하고 있던 WMI 인터페이스가 Release() 없이 덮어써져
	// 누수되는 것을 방지하기 위해, 새 연결을 맺기 전에 기존 연결을 먼저 정리합니다.
	Disconnect();

	WCHAR   wszBuffer[MAX_BUFFER_SIZE] = { 0 };
	wchar_t* pwszHost = NULL;
	wchar_t* pwszUserName = NULL;
	wchar_t* pwszUserPass = NULL;

	std::wstring strHost;
	std::wstring strUserName;
	std::wstring strUserPass;

	// [수정] 기존에는 세 인자가 모두 채워져야만 원격 접속을 시도하고, 하나라도 비어 있으면
	// 조용히 로컬 접속으로 전환되었습니다. 이는 "각 인자는 독립적으로 NULL 허용"이라는
	// 헤더 문서와 어긋나며, 호스트만 지정하고 자격증명은 생략(현재 사용자 컨텍스트 사용)한
	// 호출이 의도와 다르게 로컬 머신에 연결되는 조용한 오류로 이어질 수 있습니다.
	// 이제 각 인자를 독립적으로 처리합니다.
#ifdef _UNICODE
	pwszHost = ptszHost;
	pwszUserName = ptszUserName;
	pwszUserPass = ptszUserPass;
#else
	if( ptszHost != NULL )
	{
		strHost = AnsiToUnicode(ptszHost);
		pwszHost = const_cast<wchar_t*>(strHost.c_str());
	}
	if( ptszUserName != NULL )
	{
		strUserName = AnsiToUnicode(ptszUserName);
		pwszUserName = const_cast<wchar_t*>(strUserName.c_str());
	}
	if( ptszUserPass != NULL )
	{
		strUserPass = AnsiToUnicode(ptszUserPass);
		pwszUserPass = const_cast<wchar_t*>(strUserPass.c_str());
	}
#endif

	// STEP 1. WMI에 대한 초기 locator를 획득한다.
	m_hrLastError = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator,
		(LPVOID*)&m_pIWbemLocator
	);
	if( FAILED(m_hrLastError) )
	{
		return FALSE;
	}

	if( pwszHost == NULL )
	{
		// STEP 2. IWbemLocator::ConnectServer()를 이용해 로컬 WMI에 접속한다.
		m_hrLastError = m_pIWbemLocator->ConnectServer(
			_bstr_t(L"ROOT\\CIMV2"),
			NULL,
			NULL,
			0,
			NULL,
			0,
			0,
			&m_pIWbemServices
		);
	}
	else
	{
		swprintf_s(wszBuffer, _countof(wszBuffer), L"\\\\%s\\ROOT\\CIMV2", pwszHost);

		// pwszUserName/pwszUserPass가 NULL이면 _bstr_t(NULL)은 null BSTR이 되어
		// ConnectServer()가 현재 사용자 보안 컨텍스트를 사용하도록 합니다.
		m_hrLastError = m_pIWbemLocator->ConnectServer(
			_bstr_t(wszBuffer),
			_bstr_t(pwszUserName),
			_bstr_t(pwszUserPass),
			0,
			NULL,
			0,
			0,
			&m_pIWbemServices
		);
	}

	if( FAILED(m_hrLastError) )
	{
		m_pIWbemLocator.Release();
		return FALSE;
	}

	// STEP 3. Proxy의 Security Level을 설정한다.
	m_hrLastError = CoSetProxyBlanket(
		m_pIWbemServices,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		NULL,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE
	);
	if( FAILED(m_hrLastError) )
	{
		m_pIWbemServices.Release();
		m_pIWbemLocator.Release();

		return FALSE;
	}

	return TRUE;
}

//***************************************************************************
// @brief WQL 쿼리를 실행하여 WMI 오브젝트 목록을 수집합니다.
// @param ptszQuery 조회할 WMI 클래스명 (예: "Win32_OperatingSystem")
// @return 수집된 오브젝트 개수 (실패 시 -1)
//***************************************************************************
int CWmi::ExecQuery(const TCHAR* ptszQuery)
{
	// 한 번의 Next() 호출로 가져올 오브젝트 개수. 1개씩 가져오는 대신 배치로
	// 가져와 COM/RPC 왕복 횟수를 줄입니다(원격 WMI나 결과가 많은 클래스에서 유효).
	const ULONG WMI_FETCH_BATCH_SIZE = 32;

	ULONG ulRet = 0;
	int nIndex = 0;

	CComPtr<IEnumWbemClassObject> pEnum;

	if( !m_pIWbemServices ) return -1;

	// [수정] 고정 크기 버퍼 + _stprintf_s 대신 동적 문자열 결합을 사용합니다.
	// ptszQuery가 길어 고정 버퍼(예: 기존 256자)를 초과하면 _stprintf_s가
	// invalid parameter handler를 호출해 비정상 종료로 이어질 수 있었습니다.
	std::basic_string<TCHAR> strReqQuery = _T("SELECT * FROM ");
	strReqQuery += ptszQuery;

	m_hrLastError = m_pIWbemServices->ExecQuery(
		_bstr_t(L"WQL"),
		_bstr_t(strReqQuery.c_str()),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnum
	);
	if( FAILED(m_hrLastError) ) return -1;

	pEnum->Reset();

	// 기존 저장소 초기화 (CComPtr vector이므로 자동 Release)
	m_vecClassObject.clear();

	IWbemClassObject* rawObjects[WMI_FETCH_BATCH_SIZE] = { NULL };

	while( true )
	{
		m_hrLastError = pEnum->Next(WBEM_INFINITE, WMI_FETCH_BATCH_SIZE, rawObjects, &ulRet);
		if( FAILED(m_hrLastError) || ulRet == 0 ) break;

		for( ULONG i = 0; i < ulRet; i++ )
		{
			CComPtr<IWbemClassObject> spClass;
			// Next()가 이미 AddRef한 참조 소유권을 그대로 넘겨받습니다(추가 AddRef 없음).
			spClass.Attach(rawObjects[i]);
			rawObjects[i] = NULL;

			// vector에 추가 시 내부에서 AddRef() 실행 (Ref Count = 2)
			m_vecClassObject.push_back(spClass);
			nIndex++;

			// 루프 블록이 끝나면서 spClass 소멸자가 Release() 호출 (Ref Count = 1 로 유지되며 vector가 소유권 유지)
		}

		if( ulRet < WMI_FETCH_BATCH_SIZE ) break; // 마지막 배치
	}

	return nIndex;
}

//***************************************************************************
// @brief 특정 인덱스의 오브젝트에서 속성 값을 VARIANT 형태로 가져옵니다.
// @param nIndex 오브젝트 인덱스 (0-based)
// @param ptszProperty 가져올 속성명
// @param vtVal 결과를 전달받을 VARIANT 참조
// @return 성공 시 TRUE, 인덱스 오류 또는 속성 조회 실패 시 FALSE
//***************************************************************************
BOOL CWmi::GetProperties(int nIndex, const TCHAR* ptszProperty, VARIANT& vtVal)
{
	IWbemClassObject* pClass = NULL;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	m_hrLastError = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(m_hrLastError) ) return FALSE;

	return TRUE;
}

//***************************************************************************
// @brief 특정 인덱스의 오브젝트에서 문자열 속성 값을 가져옵니다.
// @param nIndex 오브젝트 인덱스 (0-based)
// @param ptszProperty 가져올 속성명
// @param ptszValue 결과 문자열을 저장할 버퍼
// @param dwSize 버퍼 크기 (문자 단위)
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CWmi::GetProperties(int nIndex, const TCHAR* ptszProperty, TCHAR* ptszValue, DWORD dwSize)
{
	VARIANT	vtVal;
	IWbemClassObject* pClass = NULL;

	if( !ptszValue || dwSize == 0 ) return FALSE;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	VariantInit(&vtVal);

	m_hrLastError = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(m_hrLastError) ) return FALSE;

	if( vtVal.vt == VT_BSTR && vtVal.bstrVal != NULL )
	{
#ifdef _UNICODE	
		_tcsncpy_s(ptszValue, dwSize, vtVal.bstrVal, _TRUNCATE);
#else
		std::string strAnsi = UnicodeToAnsi(vtVal.bstrVal);
		_tcsncpy_s(ptszValue, dwSize, strAnsi.c_str(), _TRUNCATE);
#endif
	}

	VariantClear(&vtVal);

	return TRUE;
}

//***************************************************************************
// @brief 특정 인덱스의 오브젝트에서 정수(long) 속성 값을 가져옵니다.
// @param nIndex 오브젝트 인덱스 (0-based)
// @param ptszProperty 가져올 속성명
// @param plValue 결과 값을 저장할 long 포인터
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CWmi::GetProperties(int nIndex, const TCHAR* ptszProperty, long* plValue)
{
	VARIANT	vtVal;
	IWbemClassObject* pClass = NULL;

	if( !plValue ) return FALSE;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	VariantInit(&vtVal);

	m_hrLastError = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(m_hrLastError) ) return FALSE;

	if( vtVal.vt == VT_I4 )
	{
		*plValue = vtVal.lVal;
	}

	VariantClear(&vtVal);

	return TRUE;
}