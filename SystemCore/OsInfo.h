
//***************************************************************************
// OsInfo.h: interface for the COsInfo Class.
//
//***************************************************************************

#ifndef __OSINFO_H__
#define __OSINFO_H__

#pragma once

#include <windows.h>

#ifndef SM_SERVERR2
	#define SM_SERVERR2								0x00000059	
#endif

#define PRODUCT_UNDEFINED                           0x00000000

#define PRODUCT_ULTIMATE                            0x00000001
#define PRODUCT_HOME_BASIC                          0x00000002
#define PRODUCT_HOME_PREMIUM                        0x00000003
#define PRODUCT_ENTERPRISE                          0x00000004
#define PRODUCT_HOME_BASIC_N                        0x00000005
#define PRODUCT_BUSINESS                            0x00000006
#define PRODUCT_STANDARD_SERVER                     0x00000007
#define PRODUCT_DATACENTER_SERVER                   0x00000008
#define PRODUCT_SMALLBUSINESS_SERVER                0x00000009
#define PRODUCT_ENTERPRISE_SERVER                   0x0000000A
#define PRODUCT_STARTER                             0x0000000B
#define PRODUCT_DATACENTER_SERVER_CORE              0x0000000C
#define PRODUCT_STANDARD_SERVER_CORE                0x0000000D
#define PRODUCT_ENTERPRISE_SERVER_CORE              0x0000000E
#define PRODUCT_ENTERPRISE_SERVER_IA64              0x0000000F
#define PRODUCT_BUSINESS_N                          0x00000010
#define PRODUCT_WEB_SERVER                          0x00000011
#define PRODUCT_CLUSTER_SERVER                      0x00000012
#define PRODUCT_HOME_SERVER                         0x00000013
#define PRODUCT_STORAGE_EXPRESS_SERVER              0x00000014
#define PRODUCT_STORAGE_STANDARD_SERVER             0x00000015
#define PRODUCT_STORAGE_WORKGROUP_SERVER            0x00000016
#define PRODUCT_STORAGE_ENTERPRISE_SERVER           0x00000017
#define PRODUCT_SERVER_FOR_SMALLBUSINESS            0x00000018
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM        0x00000019
#define PRODUCT_HOME_PREMIUM_N                      0x0000001A
#define PRODUCT_ENTERPRISE_N                        0x0000001B
#define PRODUCT_ULTIMATE_N                          0x0000001C
#define PRODUCT_WEB_SERVER_CORE                     0x0000001D
#define PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT    0x0000001E
#define PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY      0x0000001F
#define PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING     0x00000020
#define PRODUCT_SERVER_FOUNDATION                   0x00000021
#define PRODUCT_HOME_PREMIUM_SERVER                 0x00000022
#define PRODUCT_SERVER_FOR_SMALLBUSINESS_V          0x00000023
#define PRODUCT_STANDARD_SERVER_V                   0x00000024
#define PRODUCT_DATACENTER_SERVER_V                 0x00000025
#define PRODUCT_ENTERPRISE_SERVER_V                 0x00000026
#define PRODUCT_DATACENTER_SERVER_CORE_V            0x00000027
#define PRODUCT_STANDARD_SERVER_CORE_V              0x00000028
#define PRODUCT_ENTERPRISE_SERVER_CORE_V            0x00000029
#define PRODUCT_HYPERV                              0x0000002A
#define PRODUCT_STORAGE_EXPRESS_SERVER_CORE         0x0000002B
#define PRODUCT_STORAGE_STANDARD_SERVER_CORE        0x0000002C
#define PRODUCT_STORAGE_WORKGROUP_SERVER_CORE       0x0000002D
#define PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE      0x0000002E
#define PRODUCT_STARTER_N                           0x0000002F
#define PRODUCT_PROFESSIONAL                        0x00000030
#define PRODUCT_PROFESSIONAL_N                      0x00000031
#define PRODUCT_SB_SOLUTION_SERVER                  0x00000032
#define PRODUCT_SERVER_FOR_SB_SOLUTIONS             0x00000033
#define PRODUCT_STANDARD_SERVER_SOLUTIONS           0x00000034
#define PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE      0x00000035
#define PRODUCT_SB_SOLUTION_SERVER_EM               0x00000036
#define PRODUCT_SERVER_FOR_SB_SOLUTIONS_EM          0x00000037
#define PRODUCT_SOLUTION_EMBEDDEDSERVER             0x00000038
#define PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE        0x00000039
#define PRODUCT_PROFESSIONAL_EMBEDDED               0x0000003A
#define PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT       0x0000003B
#define PRODUCT_ESSENTIALBUSINESS_SERVER_ADDL       0x0000003C
#define PRODUCT_ESSENTIALBUSINESS_SERVER_MGMTSVC    0x0000003D
#define PRODUCT_ESSENTIALBUSINESS_SERVER_ADDLSVC    0x0000003E
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE   0x0000003F
#define PRODUCT_CLUSTER_SERVER_V                    0x00000040
#define PRODUCT_EMBEDDED                            0x00000041
#define PRODUCT_STARTER_E                           0x00000042
#define PRODUCT_HOME_BASIC_E                        0x00000043
#define PRODUCT_HOME_PREMIUM_E                      0x00000044
#define PRODUCT_PROFESSIONAL_E                      0x00000045
#define PRODUCT_ENTERPRISE_E                        0x00000046
#define PRODUCT_ULTIMATE_E                          0x00000047
#define PRODUCT_ENTERPRISE_EVALUATION               0x00000048
#define PRODUCT_MULTIPOINT_STANDARD_SERVER          0x0000004C
#define PRODUCT_MULTIPOINT_PREMIUM_SERVER           0x0000004D
#define PRODUCT_STANDARD_EVALUATION_SERVER          0x0000004F
#define PRODUCT_DATACENTER_EVALUATION_SERVER        0x00000050
#define PRODUCT_ENTERPRISE_N_EVALUATION             0x00000054
#define PRODUCT_EMBEDDED_AUTOMOTIVE                 0x00000055
#define PRODUCT_EMBEDDED_INDUSTRY_A                 0x00000056
#define PRODUCT_THINPC                              0x00000057
#define PRODUCT_EMBEDDED_A                          0x00000058
#define PRODUCT_EMBEDDED_INDUSTRY                   0x00000059
#define PRODUCT_EMBEDDED_E                          0x0000005A
#define PRODUCT_EMBEDDED_INDUSTRY_E                 0x0000005B
#define PRODUCT_EMBEDDED_INDUSTRY_A_E               0x0000005C
#define PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER 0x0000005F
#define PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER  0x00000060
#define PRODUCT_CORE_ARM                            0x00000061
#define PRODUCT_CORE_N                              0x00000062
#define PRODUCT_CORE_COUNTRYSPECIFIC                0x00000063
#define PRODUCT_CORE_SINGLELANGUAGE                 0x00000064
#define PRODUCT_CORE                                0x00000065
#define PRODUCT_PROFESSIONAL_WMC                    0x00000067
#define PRODUCT_EMBEDDED_INDUSTRY_EVAL              0x00000069
#define PRODUCT_EMBEDDED_INDUSTRY_E_EVAL            0x0000006A
#define PRODUCT_EMBEDDED_EVAL                       0x0000006B
#define PRODUCT_EMBEDDED_E_EVAL                     0x0000006C
#define PRODUCT_NANO_SERVER                         0x0000006D
#define PRODUCT_CLOUD_STORAGE_SERVER                0x0000006E
#define PRODUCT_CORE_CONNECTED                      0x0000006F
#define PRODUCT_PROFESSIONAL_STUDENT                0x00000070
#define PRODUCT_CORE_CONNECTED_N                    0x00000071
#define PRODUCT_PROFESSIONAL_STUDENT_N              0x00000072
#define PRODUCT_CORE_CONNECTED_SINGLELANGUAGE       0x00000073
#define PRODUCT_CORE_CONNECTED_COUNTRYSPECIFIC      0x00000074
#define PRODUCT_CONNECTED_CAR                       0x00000075
#define PRODUCT_INDUSTRY_HANDHELD                   0x00000076
#define PRODUCT_PPI_PRO                             0x00000077
#define PRODUCT_ARM64_SERVER                        0x00000078
#define PRODUCT_EDUCATION                           0x00000079
#define PRODUCT_EDUCATION_N                         0x0000007A
#define PRODUCT_IOTUAP                              0x0000007B
#define PRODUCT_CLOUD_HOST_INFRASTRUCTURE_SERVER    0x0000007C
#define PRODUCT_ENTERPRISE_S                        0x0000007D
#define PRODUCT_ENTERPRISE_S_N                      0x0000007E
#define PRODUCT_PROFESSIONAL_S                      0x0000007F
#define PRODUCT_PROFESSIONAL_S_N                    0x00000080
#define PRODUCT_ENTERPRISE_S_EVALUATION             0x00000081
#define PRODUCT_ENTERPRISE_S_N_EVALUATION           0x00000082
#define PRODUCT_HOLOGRAPHIC                         0x00000087
#define PRODUCT_HOLOGRAPHIC_BUSINESS                0x00000088
#define PRODUCT_PRO_SINGLE_LANGUAGE                 0x0000008A
#define PRODUCT_PRO_CHINA                           0x0000008B
#define PRODUCT_ENTERPRISE_SUBSCRIPTION             0x0000008C
#define PRODUCT_ENTERPRISE_SUBSCRIPTION_N           0x0000008D
#define PRODUCT_DATACENTER_NANO_SERVER              0x0000008F
#define PRODUCT_STANDARD_NANO_SERVER                0x00000090
#define PRODUCT_DATACENTER_A_SERVER_CORE            0x00000091
#define PRODUCT_STANDARD_A_SERVER_CORE              0x00000092
#define PRODUCT_DATACENTER_WS_SERVER_CORE           0x00000093
#define PRODUCT_STANDARD_WS_SERVER_CORE             0x00000094
#define PRODUCT_UTILITY_VM                          0x00000095
#define PRODUCT_DATACENTER_EVALUATION_SERVER_CORE   0x0000009F
#define PRODUCT_STANDARD_EVALUATION_SERVER_CORE     0x000000A0
#define PRODUCT_PRO_WORKSTATION                     0x000000A1
#define PRODUCT_PRO_WORKSTATION_N                   0x000000A2
#define PRODUCT_PRO_FOR_EDUCATION                   0x000000A4
#define PRODUCT_PRO_FOR_EDUCATION_N                 0x000000A5
#define PRODUCT_AZURE_SERVER_CORE                   0x000000A8
#define PRODUCT_AZURE_NANO_SERVER                   0x000000A9
#define PRODUCT_ENTERPRISEG                         0x000000AB
#define PRODUCT_ENTERPRISEGN                        0x000000AC
#define PRODUCT_SERVERRDSH                          0x000000AF
#define PRODUCT_CLOUD                               0x000000B2
#define PRODUCT_CLOUDN                              0x000000B3
#define PRODUCT_HUBOS                               0x000000B4
#define PRODUCT_ONECOREUPDATEOS                     0x000000B6
#define PRODUCT_CLOUDE                              0x000000B7
#define PRODUCT_IOTOS                               0x000000B9
#define PRODUCT_CLOUDEN                             0x000000BA
#define PRODUCT_IOTEDGEOS                           0x000000BB
#define PRODUCT_IOTENTERPRISE                       0x000000BC
#define PRODUCT_LITE                                0x000000BD
#define PRODUCT_IOTENTERPRISES                      0x000000BF
#define PRODUCT_XBOX_SYSTEMOS                       0x000000C0
#define PRODUCT_XBOX_GAMEOS                         0x000000C2
#define PRODUCT_XBOX_ERAOS                          0x000000C3
#define PRODUCT_XBOX_DURANGOHOSTOS                  0x000000C4
#define PRODUCT_XBOX_SCARLETTHOSTOS                 0x000000C5
#define PRODUCT_XBOX_KEYSTONE                       0x000000C6
#define PRODUCT_AZURE_SERVER_CLOUDHOST              0x000000C7
#define PRODUCT_AZURE_SERVER_CLOUDMOS               0x000000C8
#define PRODUCT_CLOUDEDITIONN                       0x000000CA
#define PRODUCT_CLOUDEDITION                        0x000000CB
#define PRODUCT_AZURESTACKHCI_SERVER_CORE           0x00000196
#define PRODUCT_DATACENTER_SERVER_AZURE_EDITION     0x00000197
#define PRODUCT_DATACENTER_SERVER_CORE_AZURE_EDITION 0x00000198

#define PRODUCT_UNLICENSED                          0xABCDABCD

typedef void (WINAPI *PGetNativeSystemInfo)(LPSYSTEM_INFO);
typedef BOOL(WINAPI *PGetProductInfo)(DWORD, DWORD, DWORD, DWORD, PDWORD);

typedef void (WINAPI *RtlGetVersion_FUNC)(OSVERSIONINFOEXW*);
BOOL GetVersionOS(OSVERSIONINFOEX* os);
BOOL IsWindowVersion(int nMajorVersion, int nMinorVersion, int nPlatformId);

//***************************************************************************
//
typedef enum WindowsVersion
{
   Windows,
   Windows32s,
   Windows95,
   Windows95OSR2,
   Windows98,
   Windows98SE,
   WindowsMillennium,
   WindowsNT351,
   WindowsNT40,
   WindowsNT40Server,
   Windows2000,
   WindowsXP,
   WindowsXPProfessionalx64,
   WindowsHomeServer,
   WindowsServer2003,
   WindowsServer2003R2,
   WindowsVista,
   WindowsVistaSP1,
   WindowsVistaSP2,
   WindowsServer2008,
   WindowsServer2008SP2,
   WindowsServer2008R2,
   WindowsServer2008R2SP2,
   Windows7,
   Windows7SP1,
   WindowsServer2012,
   Windows8,
   Windows81,
   WindowsServer2012R2,
   Windows10,
   Windows10_1511,
   Windows10_1607,
   Windows10_1703,
   Windows10_1709,
   Windows10_1803,
   Windows10_1809,
   Windows10_1903,
   Windows10_1909,
   Windows10_2004,
   Windows10_20H2,
   Windows10_21H1,
   Windows10_21H2,
   Windows10_22H2,
   WindowsServer2016,
   WindowsServer2019,
   WindowsServer2022,
   Windows11
} WindowsVersion;

//***************************************************************************
//
typedef enum WindowsEdition
{
   EditionUnknown,

   Workstation,
   Server,
   AdvancedServer,
   Home,

   Ultimate,
   HomeBasic,
   HomePremium,
   Enterprise,
   HomeBasic_N,
   Business,
   StandardServer,
   DatacenterServer,
   SmallBusinessServer,
   EnterpriseServer,
   Starter,
   DatacenterServerCore,
   StandardServerCore,
   EnterpriseServerCore,
   EnterpriseServerIA64,
   Business_N,
   WebServer,
   ClusterServer,
   HomeServer,
   StorageExpressServer,
   StorageStandardServer,
   StorageWorkgroupServer,
   StorageEnterpriseServer,
   ServerForSmallBusiness,
   SmallBusinessServerPremium,
   HomePremium_N,
   Enterprise_N,
   Ultimate_N,
   WebServerCore,
   MediumBusinessServerManagement,
   MediumBusinessServerSecurity,
   MediumBusinessServerMessaging,
   ServerFoundation,
   HomePremiumServer,
   ServerForSmallBusiness_V,
   StandardServer_V,
   DatacenterServer_V,
   EnterpriseServer_V,
   DatacenterServerCore_V,
   StandardServerCore_V,
   EnterpriseServerCore_V,
   HyperV,
   StorageExpressServerCore,
   StorageStandardServerCore,
   StorageWorkgroupServerCore,
   StorageEnterpriseServerCore,
   Starter_N,
   Professional,
   Professional_N,
   SBSolutionServer,
   ServerForSBSolution,
   StandardServerSolutions,
   StandardServerSolutionsCore,
   SBSolutionServer_EM,
   ServerForSBSolution_EM,
   SolutionEmbeddedServer,
   SolutionEmbeddedServerCore,
   SmallBusinessServerPremiumCore,
   EssentialBusinessServerMGMT,
   EssentialBusinessServerADDL,
   EssentialBusinessServerMGMTSVC,
   EssentialBusinessServerADDLSVC,
   ClusterServer_V,
   Embedded,
   Starter_E,
   HomeBasic_E,
   HomePremium_E,
   Professional_E,
   Enterprise_E,
   Ultimate_E,
   Windows10Home_E,
   Windows10Education_E
} WindowsEdition;

//***************************************************************************
//
class COsInfo  
{
public:
	COsInfo();
	virtual ~COsInfo();

	WindowsVersion COsInfo::GetWindowsVersion() const {
		return m_nWinVersion;								// returns the windows version
	}
	WindowsEdition COsInfo::GetWindowsEdition() const {
		return m_nWinEdition;								// returns the windows edition
	}

	DWORD GetMajorVersion() const {
		return m_Osvi.dwMajorVersion;						// returns major version
	}
	DWORD GetMinorVersion() const {
		return m_Osvi.dwMinorVersion;						// returns minor version
	}
	DWORD GetBuildNumber() const {
		return m_Osvi.dwBuildNumber;						// returns build number
	}
	DWORD GetPlatformID() const {
		return m_Osvi.dwPlatformId;							// returns platform ID
	}
	TCHAR* GetDescription() const {
		return (TCHAR *)m_tszDescription;					// returns description
	}
	TCHAR* GetServicePack() const {
		return (TCHAR *)m_tszServicePack;					// additional information about service pack
	}
   
	bool IsNTPlatform() const;								// true if NT platform
	bool IsWindowsPlatform() const;							// true is Windows platform
	bool IsWin32sPlatform() const;							// true is Win32s platform

	bool Is32bitPlatform() const;							// true if platform is 32-bit
	bool Is64bitPlatform() const;							// true if platform is 64-bit

	void DetectDescription();

private:
	void DetectWindowsVersion();
	void DetectWindowsEdition();
	void DetectWindowsServicePack();
	DWORD DetectProductInfo();

private:
	WindowsVersion		m_nWinVersion;
	WindowsEdition		m_nWinEdition;

	BOOL				m_bOsVersionInfoEx;

	TCHAR				m_tszDescription[ OS_DESCRIPTION_STRLEN ];
	TCHAR				m_tszServicePack[ OS_SERVICEPACK_STRLEN ];

	OSVERSIONINFOEX		m_Osvi;
	SYSTEM_INFO			m_Sysi;
};

#endif // ndef __OSINFO_H__
