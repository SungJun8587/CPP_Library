
//***************************************************************************
// OsInfo.cpp: implementation of the COsInfo class.
//
//***************************************************************************

#include "pch.h"
#include "OsInfo.h"

#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER 0x8000
#endif

//***************************************************************************
// @brief OS 버전 정보를 가져옵니다.
// @param os OSVERSIONINFOEX 구조체 포인터
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL GetVersionOS(OSVERSIONINFOEX* os)
{
	HMODULE hMod;
	RtlGetVersion_FUNC func;
#ifdef UNICODE
	OSVERSIONINFOEXW* osw = os;
#else
	OSVERSIONINFOEXW o;
	OSVERSIONINFOEXW* osw = &o;
#endif

	hMod = LoadLibrary(TEXT("ntdll.dll"));
	if( hMod )
	{
		func = (RtlGetVersion_FUNC)GetProcAddress(hMod, "RtlGetVersion");
		if( func == 0 )
		{
			FreeLibrary(hMod);
			return FALSE;
		}

		ZeroMemory(osw, sizeof(*osw));
		osw->dwOSVersionInfoSize = sizeof(*osw);
		func(osw);

#ifndef	UNICODE
		os->dwBuildNumber = osw->dwBuildNumber;
		os->dwMajorVersion = osw->dwMajorVersion;
		os->dwMinorVersion = osw->dwMinorVersion;
		os->dwPlatformId = osw->dwPlatformId;
		os->wProductType = osw->wProductType;
		os->dwOSVersionInfoSize = sizeof(*os);

		// [수정] 미사용 변수였던 sz를 실제 변환 루프의 경계값으로 사용합니다.
		// 원본 소스가 널 종료되지 않은 극단적인 경우에도 os->szCSDVersion 배열의
		// 끝을 넘어 쓰지 않도록 방어합니다.
		DWORD sz = _countof(os->szCSDVersion);
		WCHAR* src = osw->szCSDVersion;
		unsigned char* dtc = (unsigned char*)os->szCSDVersion;
		DWORD nCopied = 0;
		while( *src && nCopied < sz - 1 )
		{
			*dtc++ = (unsigned char)*src++;
			nCopied++;
		}
		*dtc = '\0';
#endif

	}
	else
		return FALSE;

	FreeLibrary(hMod);
	return TRUE;
}

//***************************************************************************
// @brief 지정된 주/부 버전 및 플랫폼 ID와 현재 OS 버전이 일치하는지 검사합니다.
// @param nMajorVersion 주 버전 (-1인 경우 무시)
// @param nMinorVersion 부 버전 (-1인 경우 무시)
// @param nPlatformId 플랫폼 ID (-1인 경우 무시)
// @return 조건 일치 여부 (BOOL)
//***************************************************************************
BOOL IsWindowVersion(int nMajorVersion, int nMinorVersion, int nPlatformId)
{
	BOOL bRet = 0;

	OSVERSIONINFOEX osver = { 0 };

	osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	DWORDLONG dwlConditionMask = 0;
	DWORD dwMasks = 0;

	if( nMajorVersion != -1 )
	{
		osver.dwMajorVersion = nMajorVersion;
		VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_EQUAL);
		dwMasks |= VER_MAJORVERSION;
	}

	if( nMinorVersion != -1 )
	{
		osver.dwMinorVersion = nMinorVersion;
		VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_EQUAL);
		dwMasks |= VER_MINORVERSION;
	}

	if( nPlatformId != -1 )
	{
		osver.dwPlatformId = nPlatformId;
		VER_SET_CONDITION(dwlConditionMask, VER_PLATFORMID, VER_EQUAL);
		dwMasks |= VER_PLATFORMID;
	}

	bRet = VerifyVersionInfo(&osver, dwMasks, dwlConditionMask);

	return bRet;
}

//***************************************************************************
// @brief COsInfo 클래스의 생성자입니다.
// @detail 멤버 변수를 안전한 기본값으로 초기화만 합니다. 실제 OS 정보 감지는
//         GetInformation()에서 수행합니다.
//***************************************************************************
COsInfo::COsInfo()
{
	m_bOsVersionInfoEx = FALSE;
	m_nWinVersion = Windows;
	m_nWinEdition = EditionUnknown;
	m_tszDescription[0] = '\0';
	m_tszServicePack[0] = '\0';

	ZeroMemory(&m_Osvi, sizeof(OSVERSIONINFOEX));
	m_Osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	ZeroMemory(&m_Sysi, sizeof(SYSTEM_INFO));
}

//***************************************************************************
// @brief COsInfo 클래스의 소멸자입니다.
//***************************************************************************
COsInfo::~COsInfo()
{

}

//***************************************************************************
// @brief OS 버전, 에디션, 서비스팩 등 전체 OS 정보를 감지합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 버전 정보 조회 실패)
//***************************************************************************
BOOL COsInfo::GetInformation()
{
	BOOL bIsCanDetect = TRUE;
	PGetNativeSystemInfo pGNSI = NULL;

	// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
	if( !(m_bOsVersionInfoEx = GetVersionOS(&m_Osvi)) )
	{
		bIsCanDetect = FALSE;
	}

	pGNSI = reinterpret_cast<PGetNativeSystemInfo>(GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetNativeSystemInfo"));
	if( NULL != pGNSI ) pGNSI(&m_Sysi);
	else GetSystemInfo(&m_Sysi);

	if( bIsCanDetect )
	{
		DetectWindowsVersion();
		DetectWindowsEdition();
		DetectWindowsServicePack();
		DetectDescription();
	}

	return bIsCanDetect;
}

//***************************************************************************
// @brief OS가 NT 플랫폼 계열인지 확인합니다.
// @return NT 플랫폼이면 true, 아니면 false
//***************************************************************************
bool COsInfo::IsNTPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;
}

//***************************************************************************
// @brief OS가 Windows 9x 계열 플랫폼인지 확인합니다.
// @return Windows 9x 계열 플랫폼이면 true, 아니면 false
//***************************************************************************
bool COsInfo::IsWindowsPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

//***************************************************************************
// @brief OS가 Win32s 플랫폼인지 확인합니다.
// @return Win32s 플랫폼이면 true, 아니면 false
//***************************************************************************
bool COsInfo::IsWin32sPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32s;
}

//***************************************************************************
// @brief 실행 중인 시스템 플랫폼이 32비트 환경인지 확인합니다.
// @return 32비트 환경이면 true, 아니면 false
//***************************************************************************
bool COsInfo::Is32bitPlatform() const
{
	return !Is64bitPlatform();
}

//***************************************************************************
// @brief 실행 중인 시스템 플랫폼이 64비트 환경인지 확인합니다.
// @return 64비트 환경이면 true, 아니면 false
//***************************************************************************
bool COsInfo::Is64bitPlatform() const
{
	return (
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ||
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ALPHA64);
}

//***************************************************************************
// @brief OS 버전 및 에디션 정보를 종합하여 설명 문자열을 생성합니다.
//***************************************************************************
void COsInfo::DetectDescription()
{
	_tstring strVer = GetWindowsVersionDesc();
	_tstring strEdit = GetWindowsEditionDesc();

	_stprintf_s(m_tszDescription, _countof(m_tszDescription), _T("%s [%s]"), strVer.c_str(), strEdit.c_str());
}

//***************************************************************************
// @brief OS 버전(Windows 10, 11, Server 등)을 내부적으로 감지합니다.
//***************************************************************************
void COsInfo::DetectWindowsVersion()
{
	if( m_bOsVersionInfoEx )
	{
		switch( m_Osvi.dwPlatformId )
		{
		case VER_PLATFORM_WIN32s:
		{
			m_nWinVersion = Windows32s;
			break;
		}
		// Test for the Windows 95 product family.
		case VER_PLATFORM_WIN32_WINDOWS:
		{
			switch( m_Osvi.dwMajorVersion )
			{
			case 4:
			{
				switch( m_Osvi.dwMinorVersion )
				{
				case 0:
				{
					if( m_Osvi.szCSDVersion[0] == 'B' || m_Osvi.szCSDVersion[0] == 'C' )
						m_nWinVersion = Windows95OSR2;
					else
						m_nWinVersion = Windows95;
					break;
				}
				case 10:
				{
					if( m_Osvi.szCSDVersion[0] == 'A' )
						m_nWinVersion = Windows98SE;
					else
						m_nWinVersion = Windows98;
					break;
				}
				case 90:
				{
					m_nWinVersion = WindowsMillennium;
					break;
				}
				}
				break;
			}
			}
			break;
		}
		// Test for the Windows NT product family.
		case VER_PLATFORM_WIN32_NT:
		{
			switch( m_Osvi.dwMajorVersion )
			{
			case 3:
			{
				m_nWinVersion = WindowsNT351;
				break;
			}
			case 4:
			{
				switch( m_Osvi.wProductType )
				{
				case 1:
				{
					m_nWinVersion = WindowsNT40;
					break;
				}
				case 3:
				{
					m_nWinVersion = WindowsNT40Server;
					break;
				}
				}
				break;
			}
			case 5:
			{
				switch( m_Osvi.dwMinorVersion )
				{
				case 0:
				{
					m_nWinVersion = Windows2000;
					break;
				}
				case 1:
				{
					m_nWinVersion = WindowsXP;
					break;
				}
				case 2:
				{
					if( m_Osvi.wSuiteMask == VER_SUITE_WH_SERVER )
					{
						m_nWinVersion = WindowsHomeServer;
					}
					else if( m_Osvi.wProductType == VER_NT_WORKSTATION && m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 )
					{
						m_nWinVersion = WindowsXPProfessionalx64;
					}
					else
					{
						m_nWinVersion = ::GetSystemMetrics(SM_SERVERR2) == 0 ? WindowsServer2003 : WindowsServer2003R2;
					}
					break;
				}
				}
				break;
			}
			case 6:
			{
				switch( m_Osvi.dwMinorVersion )
				{
				case 0:
				{
					if( m_Osvi.wProductType == VER_NT_WORKSTATION )
					{
						m_nWinVersion = WindowsVista;
						if( m_Osvi.wServicePackMajor == 1 )
							m_nWinVersion = WindowsVistaSP1;
						else if( m_Osvi.wServicePackMajor >= 2 )
							m_nWinVersion = WindowsVistaSP2;
					}
					else
					{
						m_nWinVersion = WindowsServer2008;
						if( m_Osvi.wServicePackMajor >= 2 )
							m_nWinVersion = WindowsServer2008SP2;
					}
					break;
				}
				case 1:
				{
					if( m_Osvi.wProductType == VER_NT_WORKSTATION )
					{
						m_nWinVersion = Windows7;
						if( m_Osvi.wServicePackMajor >= 1 )
							m_nWinVersion = Windows7SP1;
					}
					else
					{
						m_nWinVersion = WindowsServer2008R2;
						if( m_Osvi.wServicePackMajor >= 2 )
							m_nWinVersion = WindowsServer2008R2SP2;
					}
					break;
				}
				case 2:
				{
					m_nWinVersion = m_Osvi.wProductType == VER_NT_WORKSTATION ? Windows8 : WindowsServer2012;
					break;
				}
				case 3:
				{
					m_nWinVersion = m_Osvi.wProductType == VER_NT_WORKSTATION ? Windows81 : WindowsServer2012R2;
					break;
				}
				}
				break;
			}
			case 10:
			{
				switch( m_Osvi.dwMinorVersion )
				{
				case 0:
				{
					DWORD dwBuildNumber = GetBuildNumber();
					if( m_Osvi.wProductType == VER_NT_WORKSTATION )
					{
						m_nWinVersion = Windows10;
						if( dwBuildNumber == 10586 )
							m_nWinVersion = Windows10_1511;
						else if( dwBuildNumber == 14393 )
							m_nWinVersion = Windows10_1607;
						else if( dwBuildNumber == 15063 )
							m_nWinVersion = Windows10_1703;
						else if( dwBuildNumber == 16299 )
							m_nWinVersion = Windows10_1709;
						else if( dwBuildNumber == 17134 )
							m_nWinVersion = Windows10_1803;
						else if( dwBuildNumber == 17763 )
							m_nWinVersion = Windows10_1809;
						else if( dwBuildNumber == 18362 )
							m_nWinVersion = Windows10_1903;
						else if( dwBuildNumber == 18363 )
							m_nWinVersion = Windows10_1909;
						else if( dwBuildNumber == 19041 )
							m_nWinVersion = Windows10_2004;
						else if( dwBuildNumber == 19042 )
							m_nWinVersion = Windows10_20H2;
						else if( dwBuildNumber == 19043 )
							m_nWinVersion = Windows10_21H1;
						else if( dwBuildNumber == 19044 )
							m_nWinVersion = Windows10_21H2;
						else if( dwBuildNumber == 19045 )
							m_nWinVersion = Windows10_22H2;
						else if( dwBuildNumber == 22000 )
							m_nWinVersion = Windows11_21H2;
						else if( dwBuildNumber == 22621 )
							m_nWinVersion = Windows11_22H2;
						else if( dwBuildNumber == 22631 )
							m_nWinVersion = Windows11_23H2;
						else if( dwBuildNumber >= 26100 )
							m_nWinVersion = Windows11_24H2;
					}
					else
					{
						m_nWinVersion = WindowsServer2016;
						if( (dwBuildNumber >= 17763) && (dwBuildNumber < 20148) )
							m_nWinVersion = WindowsServer2019;
						else if( (dwBuildNumber >= 20148) && (dwBuildNumber < 26100) )
							m_nWinVersion = WindowsServer2022;
						else if( dwBuildNumber >= 26100 )
							m_nWinVersion = WindowsServer2025;
					}
					break;
				}
				}
				break;
			}
			}
			break;
		}
		}
	}
	else // Test for specific product on Windows NT 4.0 SP5 and earlier
	{
		HKEY	hKey;

		DWORD	dwValueLen = 0;
		long	lRetCode = 0;
		TCHAR	tszProductType[REGISTRY_VALUE_STRLEN];

		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\ProductOptions"), 0, KEY_QUERY_VALUE, &hKey);
		if( lRetCode != ERROR_SUCCESS ) return;

		dwValueLen = sizeof(tszProductType);

		tszProductType[0] = '\0';

		lRetCode = RegQueryValueEx(hKey, _T("ProductType"), NULL, NULL, (LPBYTE)tszProductType, &dwValueLen);

		// [수정] 기존에는 이 지점에서 실패 시(또는 길이 초과 시) RegCloseKey() 없이 바로
		// return하여 레지스트리 핸들이 누수되었습니다. 반환 전에 항상 정리합니다.
		if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > sizeof(tszProductType)) )
		{
			RegCloseKey(hKey);
			return;
		}

		RegCloseKey(hKey);

		// [수정] RegQueryValueEx()는 레지스트리에 저장된 REG_SZ 값이 널 종료되지 않은
		// 경우 결과 버퍼도 널로 끝난다고 보장하지 않습니다. 이후 _tcscmp()가 버퍼
		// 끝을 넘어 읽는 것을 막기 위해 반환된 바이트 길이를 문자 개수로 환산해
		// 명시적으로 널 종료합니다.
		{
			DWORD dwCharLen = dwValueLen / sizeof(TCHAR);
			if( dwCharLen >= _countof(tszProductType) ) dwCharLen = _countof(tszProductType) - 1;
			tszProductType[dwCharLen] = _T('\0');
		}

		if( _tcscmp(_T("WINNT"), tszProductType) == 0 )
		{
			if( m_Osvi.dwMajorVersion <= 4 )
			{
				m_nWinVersion = WindowsNT40;
				m_nWinEdition = Workstation;
			}
		}

		if( _tcscmp(_T("LANMANNT"), tszProductType) == 0 )
		{
			if( m_Osvi.dwMajorVersion == 5 && m_Osvi.dwMinorVersion == 2 )
			{
				m_nWinVersion = WindowsServer2003;
			}

			if( m_Osvi.dwMajorVersion == 5 && m_Osvi.dwMinorVersion == 0 )
			{
				m_nWinVersion = Windows2000;
				m_nWinEdition = Server;
			}

			if( m_Osvi.dwMajorVersion <= 4 )
			{
				m_nWinVersion = WindowsNT40;
				m_nWinEdition = Server;
			}
		}

		if( _tcscmp(_T("SERVERNT"), tszProductType) == 0 )
		{
			if( m_Osvi.dwMajorVersion == 5 && m_Osvi.dwMinorVersion == 2 )
			{
				m_nWinVersion = WindowsServer2003;
				m_nWinEdition = EnterpriseServer;
			}

			if( m_Osvi.dwMajorVersion == 5 && m_Osvi.dwMinorVersion == 0 )
			{
				m_nWinVersion = Windows2000;
				m_nWinEdition = AdvancedServer;
			}

			if( m_Osvi.dwMajorVersion <= 4 )
			{
				m_nWinVersion = WindowsNT40;
				m_nWinEdition = EnterpriseServer;
			}
		}
	}
}

//***************************************************************************
// @brief OS 에디션(Professional, Enterprise, Server 등)을 내부적으로 감지합니다.
//***************************************************************************
void COsInfo::DetectWindowsEdition()
{
	if( m_bOsVersionInfoEx )
	{
		switch( m_Osvi.dwMajorVersion )
		{
		case 4:
		{
			switch( m_Osvi.wProductType )
			{
			case VER_NT_WORKSTATION:
			{
				m_nWinEdition = Workstation;
				break;
			}
			case VER_NT_SERVER:
			{
				m_nWinEdition = (m_Osvi.wSuiteMask & VER_SUITE_ENTERPRISE) != 0 ? EnterpriseServer : StandardServer;
				break;
			}
			}
			break;
		}
		case 5:
		{
			switch( m_Osvi.wProductType )
			{
			case VER_NT_WORKSTATION:
			{
				m_nWinEdition = (m_Osvi.wSuiteMask & VER_SUITE_PERSONAL) != 0 ? Home : Professional;
				break;
			}
			case VER_NT_SERVER:
			{
				switch( m_Osvi.dwMinorVersion )
				{
				case 0:
				{
					if( (m_Osvi.wSuiteMask & VER_SUITE_DATACENTER) != 0 )
					{
						m_nWinEdition = DatacenterServer;
					}
					else if( (m_Osvi.wSuiteMask & VER_SUITE_ENTERPRISE) != 0 )
					{
						m_nWinEdition = AdvancedServer;
					}
					else
					{
						m_nWinEdition = Server;
					}
					break;
				}
				default:
				{
					if( (m_Osvi.wSuiteMask & VER_SUITE_DATACENTER) != 0 )
					{
						m_nWinEdition = DatacenterServer;
					}
					else if( (m_Osvi.wSuiteMask & VER_SUITE_ENTERPRISE) != 0 )
					{
						m_nWinEdition = EnterpriseServer;
					}
					else if( (m_Osvi.wSuiteMask & VER_SUITE_BLADE) != 0 )
					{
						m_nWinEdition = WebServer;
					}
					else
					{
						m_nWinEdition = StandardServer;
					}
					break;
				}
				}
				break;
			}
			}
			break;
		}
		case 6:
		case 10:
		{
			DWORD dwReturnedProductType = DetectProductInfo();
			switch( dwReturnedProductType )
			{
			case PRODUCT_UNDEFINED:
				m_nWinEdition = EditionUnknown;
				break;
			case PRODUCT_ULTIMATE:
				m_nWinEdition = Ultimate;
				break;
			case PRODUCT_HOME_BASIC:
				m_nWinEdition = HomeBasic;
				break;
			case PRODUCT_HOME_PREMIUM:
				m_nWinEdition = HomePremium;
				break;
			case PRODUCT_ENTERPRISE:
				m_nWinEdition = Enterprise;
				break;
			case PRODUCT_HOME_BASIC_N:
				m_nWinEdition = HomeBasic_N;
				break;
			case PRODUCT_BUSINESS:
				m_nWinEdition = Business;
				break;
			case PRODUCT_STANDARD_SERVER:
				m_nWinEdition = StandardServer;
				break;
			case PRODUCT_DATACENTER_SERVER:
				m_nWinEdition = DatacenterServer;
				break;
			case PRODUCT_SMALLBUSINESS_SERVER:
				m_nWinEdition = SmallBusinessServer;
				break;
			case PRODUCT_ENTERPRISE_SERVER:
				m_nWinEdition = EnterpriseServer;
				break;
			case PRODUCT_STARTER:
				m_nWinEdition = Starter;
				break;
			case PRODUCT_DATACENTER_SERVER_CORE:
				m_nWinEdition = DatacenterServerCore;
				break;
			case PRODUCT_STANDARD_SERVER_CORE:
				m_nWinEdition = StandardServerCore;
				break;
			case PRODUCT_ENTERPRISE_SERVER_CORE:
				m_nWinEdition = EnterpriseServerCore;
				break;
			case PRODUCT_ENTERPRISE_SERVER_IA64:
				m_nWinEdition = EnterpriseServerIA64;
				break;
			case PRODUCT_BUSINESS_N:
				m_nWinEdition = Business_N;
				break;
			case PRODUCT_WEB_SERVER:
				m_nWinEdition = WebServer;
				break;
			case PRODUCT_CLUSTER_SERVER:
				m_nWinEdition = ClusterServer;
				break;
			case PRODUCT_HOME_SERVER:
				m_nWinEdition = HomeServer;
				break;
			case PRODUCT_STORAGE_EXPRESS_SERVER:
				m_nWinEdition = StorageExpressServer;
				break;
			case PRODUCT_STORAGE_STANDARD_SERVER:
				m_nWinEdition = StorageStandardServer;
				break;
			case PRODUCT_STORAGE_WORKGROUP_SERVER:
				m_nWinEdition = StorageWorkgroupServer;
				break;
			case PRODUCT_STORAGE_ENTERPRISE_SERVER:
				m_nWinEdition = StorageEnterpriseServer;
				break;
			case PRODUCT_SERVER_FOR_SMALLBUSINESS:
				m_nWinEdition = ServerForSmallBusiness;
				break;
			case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
				m_nWinEdition = SmallBusinessServerPremium;
				break;
			case PRODUCT_HOME_PREMIUM_N:
				m_nWinEdition = HomePremium_N;
				break;
			case PRODUCT_ENTERPRISE_N:
				m_nWinEdition = Enterprise_N;
				break;
			case PRODUCT_ULTIMATE_N:
				m_nWinEdition = Ultimate_N;
				break;
			case PRODUCT_WEB_SERVER_CORE:
				m_nWinEdition = WebServerCore;
				break;
			case PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT:
				m_nWinEdition = MediumBusinessServerManagement;
				break;
			case PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY:
				m_nWinEdition = MediumBusinessServerSecurity;
				break;
			case PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING:
				m_nWinEdition = MediumBusinessServerMessaging;
				break;
			case PRODUCT_SERVER_FOUNDATION:
				m_nWinEdition = ServerFoundation;
				break;
			case PRODUCT_HOME_PREMIUM_SERVER:
				m_nWinEdition = HomePremiumServer;
				break;
			case PRODUCT_SERVER_FOR_SMALLBUSINESS_V:
				m_nWinEdition = ServerForSmallBusiness_V;
				break;
			case PRODUCT_STANDARD_SERVER_V:
				m_nWinEdition = StandardServer_V;
				break;
			case PRODUCT_DATACENTER_SERVER_V:
				m_nWinEdition = DatacenterServer_V;
				break;
			case PRODUCT_ENTERPRISE_SERVER_V:
				m_nWinEdition = EnterpriseServer_V;
				break;
			case PRODUCT_DATACENTER_SERVER_CORE_V:
				m_nWinEdition = DatacenterServerCore_V;
				break;
			case PRODUCT_STANDARD_SERVER_CORE_V:
				m_nWinEdition = StandardServerCore_V;
				break;
			case PRODUCT_ENTERPRISE_SERVER_CORE_V:
				m_nWinEdition = EnterpriseServerCore_V;
				break;
			case PRODUCT_HYPERV:
				m_nWinEdition = HyperV;
				break;
			case PRODUCT_STORAGE_EXPRESS_SERVER_CORE:
				m_nWinEdition = StorageExpressServerCore;
				break;
			case PRODUCT_STORAGE_STANDARD_SERVER_CORE:
				m_nWinEdition = StorageStandardServerCore;
				break;
			case PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:
				m_nWinEdition = StorageWorkgroupServerCore;
				break;
			case PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:
				m_nWinEdition = StorageEnterpriseServerCore;
				break;
			case PRODUCT_STARTER_N:
				m_nWinEdition = Starter_N;
				break;
			case PRODUCT_PROFESSIONAL:
				m_nWinEdition = Professional;
				break;
			case PRODUCT_PROFESSIONAL_N:
				m_nWinEdition = Professional_N;
				break;
			case PRODUCT_SB_SOLUTION_SERVER:
				m_nWinEdition = SBSolutionServer;
				break;
			case PRODUCT_SERVER_FOR_SB_SOLUTIONS:
				m_nWinEdition = ServerForSBSolution;
				break;
			case PRODUCT_STANDARD_SERVER_SOLUTIONS:
				m_nWinEdition = StandardServerSolutions;
				break;
			case PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE:
				m_nWinEdition = StandardServerSolutionsCore;
				break;
			case PRODUCT_SB_SOLUTION_SERVER_EM:
				m_nWinEdition = SBSolutionServer_EM;
				break;
			case PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM:
				m_nWinEdition = ServerForSBSolution_EM;
				break;
			case PRODUCT_SOLUTION_EMBEDDEDSERVER:
				m_nWinEdition = SolutionEmbeddedServer;
				break;
			case PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE:
				m_nWinEdition = SolutionEmbeddedServerCore;
				break;
			case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE:
				m_nWinEdition = SmallBusinessServerPremiumCore;
				break;
			case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT:
				m_nWinEdition = EssentialBusinessServerMGMT;
				break;
			case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL:
				m_nWinEdition = EssentialBusinessServerADDL;
				break;
			case PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC:
				m_nWinEdition = EssentialBusinessServerMGMTSVC;
				break;
			case PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC:
				m_nWinEdition = EssentialBusinessServerADDLSVC;
				break;
			case PRODUCT_CLUSTER_SERVER_V:
				m_nWinEdition = ClusterServer_V;
				break;
			case PRODUCT_EMBEDDED:
				m_nWinEdition = Embedded;
				break;
			case PRODUCT_STARTER_E:
				m_nWinEdition = Starter_E;
				break;
			case PRODUCT_HOME_BASIC_E:
				m_nWinEdition = HomeBasic_E;
				break;
			case PRODUCT_HOME_PREMIUM_E:
				m_nWinEdition = HomePremium_E;
				break;
			case PRODUCT_PROFESSIONAL_E:
				m_nWinEdition = Professional_E;
				break;
			case PRODUCT_ENTERPRISE_E:
				m_nWinEdition = Enterprise_E;
				break;
			case PRODUCT_ULTIMATE_E:
				m_nWinEdition = Ultimate_E;
				break;
			case PRODUCT_CORE:
				m_nWinEdition = Windows10Home_E;
				break;
			case PRODUCT_EDUCATION:
				m_nWinEdition = Windows10Education_E;
				break;
			}
			break;
		}
		}
	}
}

//***************************************************************************
// @brief 서비스 팩 및 상세 빌드 번호 정보를 내부적으로 감지합니다.
//***************************************************************************
void COsInfo::DetectWindowsServicePack()
{
	// Display service pack (if any) and build number.
	if( m_Osvi.dwMajorVersion == 4 && _tcscmp(m_Osvi.szCSDVersion, _T("Service Pack 6")) == 0 )
	{
		HKEY	hKey = NULL;

		long	lRetCode = 0;

		// Test for SP6 versus SP6a.
		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009"), 0, KEY_QUERY_VALUE, &hKey);
		if( lRetCode == ERROR_SUCCESS )
		{
			_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("Service Pack 6a (Build %d)"), m_Osvi.dwBuildNumber & 0xFFFF);
			// [수정] 기존에는 RegOpenKeyEx() 실패 시(else 분기)에도 이 아래의
			// RegCloseKey(hKey)가 무조건 호출되어, 초기화되지 않은 hKey 값을
			// CloseHandle류 API에 넘기는 정의되지 않은 동작이 발생했습니다.
			// 성공한 경우에만 닫도록 스코프를 좁힙니다.
			RegCloseKey(hKey);
		}
		else // Windows NT 4.0 prior to SP6a
		{
			_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("%s (Build %d)"), m_Osvi.szCSDVersion, m_Osvi.dwBuildNumber & 0xFFFF);
		}
	}
	else // Windows NT 3.51 and earlier or Windows 2000 and later
	{
		_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("%s (Build %d)"), m_Osvi.szCSDVersion, m_Osvi.dwBuildNumber & 0xFFFF);
	}
}

//***************************************************************************
// @brief GetProductInfo API를 사용하여 상세 프로덕트 유형을 감지합니다.
// @return 프로덕트 타입 ID
//***************************************************************************
DWORD COsInfo::DetectProductInfo()
{
	DWORD dwProductInfo = PRODUCT_UNDEFINED;

	if( m_Osvi.dwMajorVersion >= 6 )
	{
		PGetProductInfo lpProducInfo = reinterpret_cast<PGetProductInfo>(GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo"));
		if( NULL != lpProducInfo )
		{
			lpProducInfo(m_Osvi.dwMajorVersion, m_Osvi.dwMinorVersion, m_Osvi.wServicePackMajor, m_Osvi.wServicePackMinor, &dwProductInfo);
		}
	}

	return dwProductInfo;
}

//***************************************************************************
// @brief Windows 버전 열거형 ID 값을 문자열 설명으로 변환합니다.
// @return _tstring OS 버전명 문자열
//***************************************************************************
_tstring COsInfo::GetWindowsVersionDesc() const
{
	switch( m_nWinVersion )
	{
	case Windows:                 return _T("Windows");
	case Windows32s:              return _T("Windows 32s");
	case Windows95:               return _T("Windows 95");
	case Windows95OSR2:           return _T("Windows 95 SR2");
	case Windows98:               return _T("Windows 98");
	case Windows98SE:             return _T("Windows 98 SE");
	case WindowsMillennium:       return _T("Windows Me");
	case WindowsNT351:            return _T("Windows NT 3.51");
	case WindowsNT40:             return _T("Windows NT 4.0");
	case WindowsNT40Server:       return _T("Windows NT 4.0 Server");
	case Windows2000:             return _T("Windows 2000");
	case WindowsXP:               return _T("Windows XP");
	case WindowsXPProfessionalx64: return _T("Windows XP Professional x64");
	case WindowsHomeServer:       return _T("Windows Home Server");
	case WindowsServer2003:       return _T("Windows Server 2003");
	case WindowsServer2003R2:     return _T("Windows Server 2003 R2");
	case WindowsVista:            return _T("Windows Vista");
	case WindowsVistaSP1:         return _T("Windows Vista SP1");
	case WindowsVistaSP2:         return _T("Windows Vista SP2");
	case WindowsServer2008:       return _T("Windows Server 2008");
	case WindowsServer2008SP2:    return _T("Windows Server 2008 SP2");
	case WindowsServer2008R2:     return _T("Windows Server 2008 R2");
	case WindowsServer2008R2SP2:  return _T("Windows Server 2008 R2 SP2");
	case Windows7:                return _T("Windows 7");
	case Windows7SP1:             return _T("Windows 7 SP1");
	case WindowsServer2012:       return _T("Windows Server 2012");
	case Windows8:                return _T("Windows 8");
	case WindowsServer2012R2:     return _T("Windows Server 2012 R2");
	case Windows81:               return _T("Windows 8.1");
	case Windows10: case Windows10_1511: case Windows10_1607: case Windows10_1703:
	case Windows10_1709: case Windows10_1803: case Windows10_1809: case Windows10_1903:
	case Windows10_1909: case Windows10_2004: return _T("Windows 10");
	case Windows10_20H2:          return _T("Windows 10 20H2");
	case Windows10_21H1:          return _T("Windows 10 21H1");
	case Windows10_21H2:          return _T("Windows 10 21H2");
	case Windows10_22H2:          return _T("Windows 10 22H2");
	case WindowsServer2016:       return _T("Windows Server 2016");
	case WindowsServer2019:       return _T("Windows Server 2019");
	case WindowsServer2022:       return _T("Windows Server 2022");
	case WindowsServer2025:       return _T("Windows Server 2025");
	case Windows11_21H2:          return _T("Windows 11 21H2");
	case Windows11_22H2:          return _T("Windows 11 22H2");
	case Windows11_23H2:          return _T("Windows 11 23H2");
	case Windows11_24H2:          return _T("Windows 11 24H2");
	default:                      return _T("Unknown");
	}
}

//***************************************************************************
// @brief Windows 에디션 열거형 ID 값을 문자열 설명으로 변환합니다.
// @return _tstring OS 에디션명 문자열
//***************************************************************************
_tstring COsInfo::GetWindowsEditionDesc() const
{
	switch( m_nWinEdition )
	{
	case Workstation:             return _T("Workstation Edition");
	case Server:                  return _T("Server Edition");
	case AdvancedServer:          return _T("Advanced Server Edition");
	case Home:                    return _T("Home Edition");
	case Ultimate:                return _T("Ultimate Edition");
	case HomeBasic:               return _T("Home Basic Edition");
	case HomePremium:             return _T("Home Premium Edition");
	case Enterprise:              return _T("Enterprise Edition");
	case HomeBasic_N:             return _T("Home Basic N Edition");
	case Business:                return _T("Business Edition");
	case StandardServer:          return _T("Standard Server Edition");
	case EnterpriseServerCore:    return _T("Enterprise Server Core Edition");
	case EnterpriseServerIA64:    return _T("Enterprise Server IA64 Edition");
	case Business_N:              return _T("Business N Edition");
	case WebServer:               return _T("Web Server Edition");
	case ClusterServer:           return _T("Cluster Server Edition");
	case HomeServer:              return _T("Home Server Edition");
	case Professional:            return _T("Professional Edition");
	case Windows10Home_E:         return _T("Home Edition");
	case Windows10Education_E:    return _T("Education Edition");
	default:                      return _T("Edition unknown Edition");
	}
}