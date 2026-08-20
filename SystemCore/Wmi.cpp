//***************************************************************************
// Wmi.cpp: implementation of the CEventLog class.
//
//***************************************************************************

#include "pch.h"
#include "Wmi.h"

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 512
#endif

//***************************************************************************
// @brief CWmi 생성자 - WMI 인터페이스 포인터를 초기화합니다.
// @note  COM 라이브러리 초기화(CoInitializeEx)/종료(CoUninitialize)는
//        프로세스/스레드 전역 자원이므로 CWmi가 아닌 호출자(main 등)가
//        책임지고 관리합니다. CWmi를 사용하기 전에 호출 스레드에서
//        CoInitializeEx()가 먼저 호출되어 있어야 합니다.
//***************************************************************************
CWmi::CWmi()
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
	WCHAR   wszBuffer[MAX_BUFFER_SIZE] = { 0 };
	wchar_t* pwszHost = NULL;
	wchar_t* pwszUserName = NULL;
	wchar_t* pwszUserPass = NULL;

	HRESULT hr;

	std::wstring strHost;
	std::wstring strUserName;
	std::wstring strUserPass;

	if( ptszHost != NULL && ptszUserName != NULL && ptszUserPass != NULL )
	{
#ifdef _UNICODE
		pwszHost = (wchar_t*)ptszHost;
		pwszUserName = (wchar_t*)ptszUserName;
		pwszUserPass = (wchar_t*)ptszUserPass;
#else
		strHost = AnsiToUnicode(ptszHost);
		strUserName = AnsiToUnicode(ptszUserName);
		strUserPass = AnsiToUnicode(ptszUserPass);

		pwszHost = const_cast<wchar_t*>(strHost.c_str());
		pwszUserName = const_cast<wchar_t*>(strUserName.c_str());
		pwszUserPass = const_cast<wchar_t*>(strUserPass.c_str());
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
		return FALSE;
	}

	if( pwszHost == NULL || pwszUserName == NULL || pwszUserPass == NULL )
	{
		// STEP 2. IWbemLocator::ConnectServer()를 이용해 로컬 WMI에 접속한다.
		hr = m_pIWbemLocator->ConnectServer(
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

		hr = m_pIWbemLocator->ConnectServer(
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

	if( FAILED(hr) )
	{
		m_pIWbemLocator.Release();
		return FALSE;
	}

	// STEP 3. Proxy의 Security Level을 설정한다.
	hr = CoSetProxyBlanket(
		m_pIWbemServices,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		NULL,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE
	);
	if( FAILED(hr) )
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
	HRESULT hr;
	ULONG ulCount = 1;
	ULONG ulRet = 0;
	int nIndex = 0;
	TCHAR tszReqQuery[256];

	CComPtr<IEnumWbemClassObject> pEnum;

	if( !m_pIWbemServices ) return -1;

	_stprintf_s(tszReqQuery, _countof(tszReqQuery), _T("SELECT * FROM %s"), ptszQuery);

	hr = m_pIWbemServices->ExecQuery(
		_bstr_t(L"WQL"),
		_bstr_t(tszReqQuery),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnum
	);
	if( FAILED(hr) ) return -1;

	pEnum->Reset();

	// 기존 저장소 초기화 (CComPtr vector이므로 자동 Release)
	m_vecClassObject.clear();

	// [수정] Loop 내부에서 CComPtr 객체 생성
	while( true )
	{
		CComPtr<IWbemClassObject> spClass;

		// Next() 호출 시 &spClass를 통해 포인터 수집 (Ref Count = 1)
		hr = pEnum->Next(WBEM_INFINITE, 1, &spClass, &ulRet);
		if( FAILED(hr) || ulRet == 0 ) break;

		// vector에 추가 시 내부에서 AddRef() 실행 (Ref Count = 2)
		m_vecClassObject.push_back(spClass);
		nIndex++;

		// 루프 블록이 끝나면서 spClass 소멸자가 Release() 호출 (Ref Count = 1 로 유지되며 vector가 소유권 유지)
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
	HRESULT	hr;
	IWbemClassObject* pClass = NULL;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return FALSE;

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
	HRESULT	hr;
	VARIANT	vtVal;
	IWbemClassObject* pClass = NULL;

	if( !ptszValue || dwSize == 0 ) return FALSE;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	VariantInit(&vtVal);

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return FALSE;

	if( vtVal.vt == VT_BSTR && vtVal.bstrVal != NULL )
	{
#ifdef _UNICODE	
		_tcscpy_s(ptszValue, dwSize, vtVal.bstrVal);
#else
		std::string strAnsi = UnicodeToAnsi(vtVal.bstrVal);
		_tcscpy_s(ptszValue, dwSize, strAnsi.c_str());
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
	HRESULT	hr;
	VARIANT	vtVal;
	IWbemClassObject* pClass = NULL;

	if( !plValue ) return FALSE;

	if( nIndex < 0 || static_cast<size_t>(nIndex) >= m_vecClassObject.size() ) return FALSE;

	pClass = m_vecClassObject[nIndex];
	if( !pClass ) return FALSE;

	VariantInit(&vtVal);

	hr = pClass->Get(_bstr_t(ptszProperty), 0L, &vtVal, 0L, 0L);
	if( FAILED(hr) ) return FALSE;

	if( vtVal.vt == VT_I4 )
	{
		*plValue = vtVal.lVal;
	}

	VariantClear(&vtVal);

	return TRUE;
}