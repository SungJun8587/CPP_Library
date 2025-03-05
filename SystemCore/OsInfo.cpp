
//***************************************************************************
// OsInfo.cpp: implementation of the COsInfo class.
//
//***************************************************************************

#include "pch.h"
#include "OsInfo.h"

#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER 0x8000
#endif

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

		DWORD sz = sizeof(os->szCSDVersion);
		WCHAR* src = osw->szCSDVersion;
		unsigned char* dtc = (unsigned char*)os->szCSDVersion;
		while( *src )
			*dtc++ = (unsigned char)*src++;
		*dtc = '\0';
#endif

	}
	else
		return FALSE;

	FreeLibrary(hMod);
	return TRUE;
}

//***************************************************************************
//
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
// Construction/Destruction
//***************************************************************************

COsInfo::COsInfo()
{
	BOOL bIsCanDetect = TRUE;
	PGetNativeSystemInfo pGNSI = NULL;

	m_bOsVersionInfoEx = FALSE;
	m_nWinVersion = Windows;
	m_nWinEdition = EditionUnknown;
	m_tszDescription[0] = '\0';
	m_tszServicePack[0] = '\0';

	// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
 	ZeroMemory(&m_Osvi, sizeof(OSVERSIONINFOEX));
	m_Osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

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
}

COsInfo::~COsInfo()
{

}

//***************************************************************************
//
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
									else if(dwBuildNumber == 19044 ) 
											m_nWinVersion = Windows10_21H2;
									else if(dwBuildNumber == 19045)
											m_nWinVersion = Windows10_22H2;
									else if( dwBuildNumber == 22000 )
										m_nWinVersion = Windows11_21H2;
									else if( dwBuildNumber == 22621 )
										m_nWinVersion = Windows11_22H2;
									else if( dwBuildNumber == 22631 )
										m_nWinVersion = Windows11_23H2;
								}
								else
								{
									m_nWinVersion = WindowsServer2016;
									if( (dwBuildNumber >= 17763) && (dwBuildNumber < 20148) )
										m_nWinVersion = WindowsServer2019;
									else if( dwBuildNumber >= 20148 )
										m_nWinVersion = WindowsServer2022;
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
		if( (lRetCode != ERROR_SUCCESS) || (dwValueLen > REGISTRY_VALUE_STRLEN) ) return;

		RegCloseKey(hKey);

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

						//#if _WIN32_WINNT >= 0x0601 // windows 7
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
						//#endif
				}
				break;
			}
		}
	}
}

//***************************************************************************
//
void COsInfo::DetectWindowsServicePack()
{
	// Display service pack (if any) and build number.
	if( m_Osvi.dwMajorVersion == 4 && _tcscmp(m_Osvi.szCSDVersion, _T("Service Pack 6")) == 0 )
	{
		HKEY	hKey;

		long	lRetCode = 0;

		// Test for SP6 versus SP6a.
		lRetCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009"), 0, KEY_QUERY_VALUE, &hKey);
		if( lRetCode == ERROR_SUCCESS )
			_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("Service Pack 6a (Build %d)"), m_Osvi.dwBuildNumber & 0xFFFF);
		else // Windows NT 4.0 prior to SP6a
		{
			_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("%s (Build %d)"), m_Osvi.szCSDVersion, m_Osvi.dwBuildNumber & 0xFFFF);
		}

		RegCloseKey(hKey);
	}
	else // Windows NT 3.51 and earlier or Windows 2000 and later
	{
		_stprintf_s(m_tszServicePack, _countof(m_tszServicePack), _T("%s (Build %d)"), m_Osvi.szCSDVersion, m_Osvi.dwBuildNumber & 0xFFFF);
	}
}

DWORD COsInfo::DetectProductInfo()
{
	DWORD dwProductInfo = PRODUCT_UNDEFINED;

	//#if _WIN32_WINNT >= 0x0600 
	if( m_Osvi.dwMajorVersion >= 6 )
	{
		PGetProductInfo lpProducInfo = reinterpret_cast<PGetProductInfo>(GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo"));
		if( NULL != lpProducInfo )
		{
			lpProducInfo(m_Osvi.dwMajorVersion, m_Osvi.dwMinorVersion, m_Osvi.wServicePackMajor, m_Osvi.wServicePackMinor, &dwProductInfo);
		}
	}
	//#endif

	return dwProductInfo;
}

bool COsInfo::IsNTPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;
}

bool COsInfo::IsWindowsPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

bool COsInfo::IsWin32sPlatform() const
{
	return m_Osvi.dwPlatformId == VER_PLATFORM_WIN32s;
}

bool COsInfo::Is32bitPlatform() const
{
	return !Is64bitPlatform();
}

bool COsInfo::Is64bitPlatform() const
{
	return (
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ||
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		m_Sysi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ALPHA64);
}

//***************************************************************************
// Windows 10 이후로 내부 버전은 10.0으로 고정, 빌드 번호만 증가
// Windows 10 / 11의 업데이트는 연도 기반(YYH1, YYH2) 명명법 사용
// Windows 10 빌드 19045까지가 최종 버전(Windows 10 22H2)
// Windows 11은 빌드 22000 이상(현재 22631까지 출시됨)
void COsInfo::DetectDescription()
{
	switch( GetWindowsVersion() )
	{
		case Windows:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows"));
			break;
		case Windows32s:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 32s"));
			break;
		case Windows95:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 95"));
			break;
		case Windows95OSR2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 95 SR2"));
			break;
		case Windows98:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 98"));
			break;
		case Windows98SE:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 98 SE"));
			break;
		case WindowsMillennium:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Me"));
			break;
		case WindowsNT351:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows NT 3.51"));
			break;
		case WindowsNT40:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows NT 4.0"));
			break;
		case WindowsNT40Server:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows NT 4.0 Server"));
			break;
		case Windows2000:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 2000"));
			break;
		case WindowsXP:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows XP"));
			break;
		case WindowsXPProfessionalx64:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows XP Professional x64"));
			break;
		case WindowsHomeServer:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Home Server"));
			break;
		case WindowsServer2003:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2003"));
			break;
		case WindowsServer2003R2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2003 R2"));
			break;
		case WindowsVista:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Vista"));
			break;
		case WindowsVistaSP1:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Vista SP1"));
			break;
		case WindowsVistaSP2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Vista SP2"));
			break;
		case WindowsServer2008:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2008"));
			break;
		case WindowsServer2008SP2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2008 SP2"));
			break;
		case WindowsServer2008R2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2008 R2"));
			break;
		case WindowsServer2008R2SP2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2008 R2 SP2"));
			break;
		case Windows7:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 7"));
			break;
		case Windows7SP1:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 7 SP1"));
			break;
		case WindowsServer2012:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2012"));
			break;
		case Windows8:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 8"));
			break;
		case WindowsServer2012R2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2012 R2"));
			break;
		case Windows81:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 8.1"));
			break;
		case Windows10:
		case Windows10_1511:
		case Windows10_1607:
		case Windows10_1703:
		case Windows10_1709:
		case Windows10_1803:
		case Windows10_1809:
		case Windows10_1903:
		case Windows10_1909:
		case Windows10_2004:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 10"));
			break;
		case Windows10_20H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 10 20H2"));
			break;
		case Windows10_21H1:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 10 21H1"));
			break;
		case Windows10_21H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 10 21H2"));
			break;
		case Windows10_22H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 10 22H2"));
			break;
		case WindowsServer2016:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2016"));
			break;
		case WindowsServer2019:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2019"));
			break;
		case WindowsServer2022:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows Server 2022"));
			break;
		case Windows11_21H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 11 21H2"));
			break;
		case Windows11_22H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 11 22H2"));
			break;
		case Windows11_23H2:
			_tcscpy_s(m_tszDescription, _countof(m_tszDescription), _T("Windows 11 23H2"));
			break;
	}

	switch( GetWindowsEdition() )
	{
		case EditionUnknown:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Edition unknown Edition]"));
			break;
		case Workstation:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Workstation Edition]"));
			break;
		case Server:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Server Edition]"));
			break;
		case AdvancedServer:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Advanced Server Edition]"));
			break;
		case Home:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Edition]"));
			break;
		case Ultimate:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Ultimate Edition]"));
			break;
		case HomeBasic:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Basic Edition]"));
			break;
		case HomePremium:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Premium Edition]"));
			break;
		case Enterprise:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Enterprise Edition]"));
			break;
		case HomeBasic_N:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Basic N Edition]"));
			break;
		case Business:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Business Edition]"));
			break;
		case Starter:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Starter Edition]"));
			break;
		case StandardServer:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Standard Server Edition]"));
			break;
		case EnterpriseServerCore:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Enterprise Server Core Edition]"));
			break;
		case EnterpriseServerIA64:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Enterprise Server IA64 Edition]"));
			break;
		case Business_N:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Business N Edition]"));
			break;
		case WebServer:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Web Server Edition]"));
			break;
		case ClusterServer:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Cluster Server Edition]"));
			break;
		case HomeServer:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Server Edition]"));
			break;
		case Professional:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Professional Edition]"));
			break;
		case Windows10Home_E:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Home Edition]"));
			break;
		case Windows10Education_E:
			_tcscat_s(m_tszDescription, _countof(m_tszDescription), _T("[Education Edition]"));
			break;
	}
}
