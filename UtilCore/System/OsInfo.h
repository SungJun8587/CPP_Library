//***************************************************************************
// OsInfo.h: interface for the COsInfo Class.
//
//***************************************************************************

#ifndef __OSINFO_H__
#define __OSINFO_H__

#pragma once

#ifndef __SYSTEMBASEDEFINE_H__
#include <SystemBaseDefine.h>
#endif

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

typedef void (WINAPI* PGetNativeSystemInfo)(LPSYSTEM_INFO);
typedef BOOL(WINAPI* PGetProductInfo)(DWORD, DWORD, DWORD, DWORD, PDWORD);
typedef void (WINAPI* RtlGetVersion_FUNC)(OSVERSIONINFOEXW*);

BOOL GetVersionOS(OSVERSIONINFOEX* os);
BOOL IsWindowVersion(int nMajorVersion, int nMinorVersion, int nPlatformId);

//***************************************************************************
// @brief Windows OS 버전을 정의한 열거형입니다.
// @details Windows 3.1, 95부터 Windows 10, 11 및 Windows Server 시리즈까지의 OS 버전을 분류합니다.
//***************************************************************************
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
	WindowsServer2025,
	Windows11_21H2,
	Windows11_22H2,
	Windows11_23H2,
	Windows11_24H2
} WindowsVersion;

//***************************************************************************
// @brief Windows OS 에디션 제품군을 정의한 열거형입니다.
// @details Home, Professional, Enterprise, Datacenter Server 등 세부 제품 에디션을 분류합니다.
//***************************************************************************
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
// @brief 운영체제(Windows) 정보를 감지하고 관리하는 클래스입니다.
// @details 현재 실행 중인 Windows의 버전, 에디션, 아키텍처(32비트/64비트), 빌드 번호 및 서비스 팩 정보 등을 수집하고 조회하는 기능을 제공합니다.
//***************************************************************************
class COsInfo
{
public:
	//***************************************************************************
	// @brief COsInfo 클래스의 생성자입니다.
	// @detail 멤버 변수를 안전한 기본값으로 초기화만 합니다. 실제 OS 정보 감지는
	//         GetInformation()을 호출해야 수행됩니다(다른 정보 수집 클래스들과
	//         동일한 사용 패턴).
	//***************************************************************************
	COsInfo();

	//***************************************************************************
	// @brief COsInfo 클래스의 소멸자입니다.
	//***************************************************************************
	virtual ~COsInfo();

	//***************************************************************************
	// @brief OS 버전, 에디션, 서비스팩 등 전체 OS 정보를 감지합니다.
	// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 버전 정보 조회 실패)
	// @detail GetVersionEx/RtlGetVersion 조회에 성공한 경우에만 버전/에디션/
	//         서비스팩/설명 문자열까지 상세 감지를 진행합니다. 실패 시 멤버
	//         변수는 생성자에서 설정한 기본값(Windows, EditionUnknown, 빈
	//         문자열)으로 남습니다.
	//***************************************************************************
	BOOL GetInformation();

	//***************************************************************************
	// @brief 감지된 Windows OS 버전을 반환합니다.
	// @return WindowsVersion 감지된 OS 버전 열거형
	//***************************************************************************
	WindowsVersion GetWindowsVersion() const {
		return m_nWinVersion;
	}

	//***************************************************************************
	// @brief 감지된 Windows 에디션 정보를 반환합니다.
	// @return WindowsEdition 감지된 에디션 열거형
	//***************************************************************************
	WindowsEdition GetWindowsEdition() const {
		return m_nWinEdition;
	}

	//***************************************************************************
	// @brief OS 주 버전(Major Version)을 반환합니다.
	// @return DWORD 주 버전 번호
	//***************************************************************************
	DWORD GetMajorVersion() const {
		return m_Osvi.dwMajorVersion;
	}

	//***************************************************************************
	// @brief OS 부 버전(Minor Version)을 반환합니다.
	// @return DWORD 부 버전 번호
	//***************************************************************************
	DWORD GetMinorVersion() const {
		return m_Osvi.dwMinorVersion;
	}

	//***************************************************************************
	// @brief OS 빌드 번호(Build Number)를 반환합니다.
	// @param 없음
	// @return DWORD 빌드 번호
	//***************************************************************************
	DWORD GetBuildNumber() const {
		return m_Osvi.dwBuildNumber;
	}

	//***************************************************************************
	// @brief OS 플랫폼 ID를 반환합니다.
	// @return DWORD 플랫폼 ID
	//***************************************************************************
	DWORD GetPlatformID() const {
		return m_Osvi.dwPlatformId;
	}

	//***************************************************************************
	// @brief 감지된 OS 설명 문자열을 반환합니다.
	// @return const TCHAR* OS 설명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetDescription() const {
		return m_tszDescription;
	}

	//***************************************************************************
	// @brief 서비스 팩 및 빌드 정보 문자열을 반환합니다.
	// @return const TCHAR* 서비스 팩 문자열 포인터
	//***************************************************************************
	const TCHAR* GetServicePack() const {
		return m_tszServicePack;
	}

	//***************************************************************************
	// @brief OS가 NT 플랫폼 계열인지 확인합니다.
	// @return bool NT 플랫폼이면 true, 아니면 false
	//***************************************************************************
	bool IsNTPlatform() const;

	//***************************************************************************
	// @brief OS가 Windows 9x 계열 플랫폼인지 확인합니다.
	// @return bool Windows 9x 계열 플랫폼이면 true, 아니면 false
	//***************************************************************************
	bool IsWindowsPlatform() const;

	//***************************************************************************
	// @brief OS가 Win32s 플랫폼인지 확인합니다.
	// @param 없음
	// @return bool Win32s 플랫폼이면 true, 아니면 false
	//***************************************************************************
	bool IsWin32sPlatform() const;

	//***************************************************************************
	// @brief 실행 중인 시스템 플랫폼이 32비트 환경인지 확인합니다.
	// @return bool 32비트 환경이면 true, 아니면 false
	//***************************************************************************
	bool Is32bitPlatform() const;

	//***************************************************************************
	// @brief 실행 중인 시스템 플랫폼이 64비트 환경인지 확인합니다.
	// @return bool 64비트 환경이면 true, 아니면 false
	//***************************************************************************
	bool Is64bitPlatform() const;

	//***************************************************************************
	// @brief OS 버전 및 에디션 정보를 종합하여 설명 문자열을 생성합니다.
	//***************************************************************************
	void DetectDescription();

private:
	//***************************************************************************
	// @brief OS 버전(Windows 10, 11, Server 등)을 내부적으로 감지합니다.
	//***************************************************************************
	void DetectWindowsVersion();

	//***************************************************************************
	// @brief OS 에디션(Professional, Enterprise, Server 등)을 내부적으로 감지합니다.
	//***************************************************************************
	void DetectWindowsEdition();

	//***************************************************************************
	// @brief 서비스 팩 및 상세 빌드 번호 정보를 내부적으로 감지합니다.
	//***************************************************************************
	void DetectWindowsServicePack();

	//***************************************************************************
	// @brief GetProductInfo API를 사용하여 상세 프로덕트 유형을 감지합니다.
	// @return DWORD 프로덕트 타입 ID
	//***************************************************************************
	DWORD DetectProductInfo();

	//***************************************************************************
	// @brief Windows 버전 열거형 ID 값을 문자열 설명으로 변환합니다.
	// @return _tstring OS 버전명 문자열
	//***************************************************************************
	_tstring GetWindowsVersionDesc() const;

	//***************************************************************************
	// @brief Windows 에디션 열거형 ID 값을 문자열 설명으로 변환합니다.
	// @return _tstring OS 에디션명 문자열
	//***************************************************************************
	_tstring GetWindowsEditionDesc() const;

private:
	WindowsVersion		m_nWinVersion; // 감지된 Windows 버전 정보
	WindowsEdition		m_nWinEdition; // 감지된 Windows 에디션 정보

	BOOL				m_bOsVersionInfoEx; // GetVersionEx/RtlGetVersion 구동 성공 여부

	TCHAR				m_tszDescription[OS_DESCRIPTION_STRLEN]; // OS 전체 설명 문자열 버퍼
	TCHAR				m_tszServicePack[OS_SERVICEPACK_STRLEN]; // 서비스 팩 및 빌드 상세 문자열 버퍼

	OSVERSIONINFOEX		m_Osvi; // OS 버전 상세 구조체
	SYSTEM_INFO			m_Sysi; // 시스템 하드웨어 정보 구조체
};

#endif // ndef __OSINFO_H__