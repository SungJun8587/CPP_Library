//***************************************************************************
// CpuInfo.cpp: implementation of the CCpuInfo class.
//
//***************************************************************************

#include "pch.h"
#include "CpuInfo.h"

#define SUPPORT_MMX				0x0001
#define SUPPORT_3DNOW			0x0002
#define SUPPORT_SSE				0x0004
#define SUPPORT_SSE2			0x0008

//***************************************************************************
// Construction/Destruction
//***************************************************************************

// - https://www.prowaretech.com/articles/current/assembly
// - https://sean.tistory.com/148
//***************************************************************************
// @brief CCpuInfo 클래스의 생성자로, 내부 변수를 초기화합니다.
//***************************************************************************
CCpuInfo::CCpuInfo()
{
	ZeroMemory(&m_Cpu, sizeof(HWINFO_CPU));

	m_dwSignature = 0;
	m_dwFeatureEbx = 0;
	m_dwFeatureEcx = 0;
	m_dwFeatures = 0;
	m_dwExtendedFeatureEdx = 0;

	m_dwEax1 = 0;
	m_dwEbx1 = 0;
	m_dwEcx1 = 0;
	m_dwEdx1 = 0;

	m_dwEax2 = 0;
	m_dwEbx2 = 0;
	m_dwEcx2 = 0;
	m_dwEdx2 = 0;
}

//***************************************************************************
// @brief CCpuInfo 클래스의 소멸자입니다.
//***************************************************************************
CCpuInfo::~CCpuInfo()
{
}

//***************************************************************************
// @brief 전체 CPU 정보를 수집하고 분석을 진행합니다.
// @return 성공 시 TRUE, 실패 시 FALSE
//***************************************************************************
BOOL CCpuInfo::GetInformation()
{
	DWORD		dwCpuId = 0;
	int			nFamily = 0;
	int			nModel = 0;
	int			nStepping = 0;
	int			nFamilyEx = 0;
	int			nModelEx = 0;

	if( cpu_id_supported() < 1 ) return false;

	dwCpuId = GetHighestCpuId();
	switch( dwCpuId )
	{
	case 0:				// don't do anything funky; return
		break;
	case 1:				// x86 cpu's do processor identification here
		GetCpuIdentification();
		break;
	case 2:				// intel cpu's find cache information here
		GetIntelCacheInfo();
		GetCpuIdentification();
		break;
	default:
		break;
	}

	DetectCpuGenInfo();
	DetectCpuDescInfo();
	DetectCpuSpeed();

	return true;
}

//***************************************************************************
// @brief CPU 일반 정보(캐시, 브랜드 이름, 확장 기능 등)를 감지합니다.
//***************************************************************************
void CCpuInfo::DetectCpuGenInfo()
{
	DWORD dwLargestExtendedFeature = 0;

	if( _tcscmp(m_Cpu.m_tszVendorName, VENDOR_INTEL_STR) == 0 )
		GetOldIntelName();

	dwLargestExtendedFeature = GetLargestExtendedFeature();
	if( dwLargestExtendedFeature >= AMD_L2CACHE_FEATURE )
	{
		GetAmdL2CacheInfo();
	}

	if( dwLargestExtendedFeature >= AMD_L1CACHE_FEATURE )
	{
		GetAmdL1CacheInfo();
	}

	if( dwLargestExtendedFeature >= NAMESTRING_FEATURE )
	{
		GetNameString();
	}

	if( dwLargestExtendedFeature >= AMD_EXTENDED_FEATURE )
	{
		GetExtendedFeature();
	}
}

//***************************************************************************
// @brief CPU의 상세 기술 정보(Family, Model, Stepping, 코어 수)를 감지합니다.
//***************************************************************************
void CCpuInfo::DetectCpuDescInfo()
{
	DWORD	dwSignature = 0;
	int		nFamily = 0;
	int		nModel = 0;
	int		nStepping = 0;
	int		nFamilyEx = 0;
	int		nModelEx = 0;

	SYSTEM_INFO stCpuInfo;

	GetSystemInfo(&stCpuInfo);

	if( m_dwSignature == 0 )
	{
		GetCpuIdentification();
	}

	nFamily = (m_dwSignature >> 8) & 0xF; // retrieve family

	if( nFamily == 15 ) // retrieve extended family
		nFamilyEx = (m_dwSignature >> 16) & 0xFF0;

	nModel = (m_dwSignature >> 4) & 0xF;  // retrieve model
	if( nModel == 15 )	// retrieve extended model
		nModelEx = (m_dwSignature >> 12) & 0xF;

	nStepping = (m_dwSignature) & 0xF;    // retrieve stepping

	m_Cpu.m_nNumberCpus = stCpuInfo.dwNumberOfProcessors;
	m_Cpu.m_nFamily = nFamily;
	m_Cpu.m_nFamilyEx = nFamilyEx;
	m_Cpu.m_nModel = nModel;
	m_Cpu.m_nModelEx = nModelEx;
	m_Cpu.m_nStepping = nStepping;
}

//***************************************************************************
// @brief 다양한 측정 방식을 이용하여 CPU 클럭 속도를 감지합니다.
//***************************************************************************
void CCpuInfo::DetectCpuSpeed()
{
	DWORD dwStartingPriority = GetPriorityClass(GetCurrentProcess());
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	__int64 nSpeed = CalculateCpuSpeed();
	if( nSpeed == 0 )
	{
		nSpeed = GetCpuSpeedFromRegistry();
		if( nSpeed == 0 )
		{
			nSpeed = CalculateCpuSpeedMethod2();
		}
	}

	SetPriorityClass(GetCurrentProcess(), dwStartingPriority);

	// if it's still 0 at this point, god help us all
	if( nSpeed == 0 )
		nSpeed = -1;

	m_Cpu.m_nSpeed = nSpeed;
}

//***************************************************************************
// @brief 최고 지원 CPUID 기능 ID 및 제조사(Vendor)명을 가져옵니다.
// @return 최고 지원 CPUID 값 (실패 시 0)
//***************************************************************************
DWORD CCpuInfo::GetHighestCpuId()
{
	DWORD dwHighest = 0;
	char szTemp[CPU_VENDOR_STRLEN] = { 0, };

	if( !cpu_vendor(&dwHighest, szTemp) ) return 0;

	if( dwHighest != 0 )
	{
#ifdef _UNICODE
		int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szTemp, -1, NULL, 0);
		if( nLength == 0 || CPU_VENDOR_STRLEN < nLength ) return false;
		if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)szTemp, -1, m_Cpu.m_tszVendorName, nLength) == 0 ) return false;
#else
		strncpy_s(m_Cpu.m_tszVendorName, CPU_VENDOR_STRLEN, szTemp, _TRUNCATE);
#endif
	}

	return dwHighest;
}

//***************************************************************************
// @brief CPU 시그니처 및 주요 기능 플래그 레지스터 값을 수집합니다.
//***************************************************************************
void CCpuInfo::GetCpuIdentification()
{
	DWORD dwEax = 1, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	m_dwSignature = dwEax;
	m_dwFeatureEbx = dwEbx;
	m_dwFeatureEcx = dwEcx;
	m_dwFeatures = dwEdx;
}

//***************************************************************************
// @brief Intel CPU의 캐시 정보를 수집합니다.
//***************************************************************************
void CCpuInfo::GetIntelCacheInfo()
{
	DWORD dwEax = 2, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	m_dwEax1 = dwEax;
	m_dwEbx1 = dwEbx;
	m_dwEcx1 = dwEcx;
	m_dwEdx1 = dwEdx;
}

//***************************************************************************
// @brief AMD CPU의 L1 캐시 정보를 수집합니다.
//***************************************************************************
void CCpuInfo::GetAmdL1CacheInfo()
{
	DWORD dwEax = AMD_L1CACHE_FEATURE, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	m_dwEax1 = dwEax;
	m_dwEbx1 = dwEbx;
	m_dwEcx1 = dwEcx;
	m_dwEdx1 = dwEdx;
}

//***************************************************************************
// @brief AMD CPU의 L2 캐시 정보를 수집합니다.
//***************************************************************************
void CCpuInfo::GetAmdL2CacheInfo()
{
	DWORD dwEax = AMD_L2CACHE_FEATURE, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	m_dwEax2 = dwEax;
	m_dwEbx2 = dwEbx;
	m_dwEcx2 = dwEcx;
	m_dwEdx2 = dwEdx;
}

//***************************************************************************
// @brief 최대 지원 확장 기능 CPUID 레지스터 값을 반환합니다.
// @return 최대 지원 확장 기능 값
//***************************************************************************
DWORD CCpuInfo::GetLargestExtendedFeature()
{
	DWORD dwEax = 0x80000000, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	return dwEax;
}

//***************************************************************************
// @brief 확장 기능 레지스터 값을 수집합니다.
//***************************************************************************
void CCpuInfo::GetExtendedFeature()
{
	unsigned long dwEax = AMD_EXTENDED_FEATURE, dwEbx = 0, dwEcx = 0, dwEdx = 0;

	cpu_id(&dwEax, &dwEbx, &dwEcx, &dwEdx);

	m_dwExtendedFeatureEdx = dwEdx;
}

//***************************************************************************
// @brief CPU 브랜드 이름을 수집하고 멤버 변수에 저장합니다.
//***************************************************************************
void CCpuInfo::GetNameString()
{
	union {
		char szName[48 + 1];
		long long buffer[6];
	};

#ifdef _WIN64
	szName[48] = 0;
	buffer[0] = cpu_brand_part0();
	buffer[1] = cpu_brand_part1();
	buffer[2] = cpu_brand_part2();
	buffer[3] = cpu_brand_part3();
	buffer[4] = cpu_brand_part4();
	buffer[5] = cpu_brand_part5();
#else
	if( !cpu_brand(szName) ) return;
#endif


#ifdef _UNICODE
	int nLength = MultiByteToWideChar(CP_ACP, 0, (LPSTR)szName, -1, NULL, 0);
	if( nLength == 0 || CPU_GENNAME_STRLEN < nLength ) return;
	if( MultiByteToWideChar(CP_ACP, 0, (LPSTR)szName, -1, m_Cpu.m_tszProcessorName, nLength) == 0 ) return;
#else
	strncpy_s(m_Cpu.m_tszProcessorName, CPU_GENNAME_STRLEN, szName, _TRUNCATE);
#endif
}

//***************************************************************************
// @brief 구형 Intel CPU 명칭을 감지하고 분류합니다.
//***************************************************************************
void CCpuInfo::GetOldIntelName()
{
	BOOL	bIsCeleron = false;
	BOOL	bIsXeon = false;
	int		nFamily = 0;
	int		nModel = 0;
	TCHAR	tszCpuName[CPU_GENNAME_STRLEN];

	struct brand_entry
	{
		long	lBrandValue;
		const TCHAR* pszBrand;
	};

	struct brand_entry brand_table[BRANDTABLESIZE] =
	{
		1, _T("Genuine Intel Celeron(TM) processor"),
		2, _T("Genuine Intel Pentium(R) III processor"),
		3, _T("Genuine Intel Pentium(R) III Xeon(TM) processor"),
		8, _T("Genuine Intel Pentium(R) 4 processor")
	};

	tszCpuName[0] = '\0';
	if( GetHighestCpuId() < NAMESTRING_FEATURE )
	{
		nFamily = (m_dwSignature >> 8) & 0xF;
		nModel = (m_dwSignature >> 4) & 0xF;

		switch( nFamily )
		{
		case 4:		// 486
			switch( nModel )
			{
			case 0:
			case 1:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel486(TM) DX processor"));
				break;
			case 2:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel486(TM) SX processor"));
				break;
			case 3:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("IntelDX2(TM) processor"));
				break;
			case 4:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel486(TM) processor"));
				break;
			case 5:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("IntelSX2(TM) processor"));
				break;
			case 7:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Writeback Enhanced IntelDX2(TM) processor"));
				break;
			case 8:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("IntelDX4(TM) processor"));
				break;
			default:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel 486 processor"));
				break;
			}
			break;
		case 5:		// pentium
			_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) processor"));
			break;
		case 6:		// pentium II and family
			switch( nModel )
			{
			case 1:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) Pro processor"));
				break;
			case 3:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) II processor, model 3"));
				break;
			case 5:
			case 7:
				bIsCeleron = false;
				bIsXeon = false;

				GetCeleronAndXeon(m_dwSignature, &bIsCeleron, &bIsXeon, true);
				GetCeleronAndXeon(m_dwFeatureEbx, &bIsCeleron, &bIsXeon);
				GetCeleronAndXeon(m_dwFeatureEcx, &bIsCeleron, &bIsXeon);
				GetCeleronAndXeon(m_dwFeatures, &bIsCeleron, &bIsXeon);

				if( bIsCeleron )
				{
					_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Celeron(TM) processor, model 5"));
				}
				else
				{
					if( bIsXeon )
					{
						if( nModel == 5 )
						{
							_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) II Xeon(TM) processor"));
						}
						else
						{
							_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) III Xeon(TM) processor"));
						}
					}
					else
					{
						if( nModel == 5 )
						{
							_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) II processor, model 5"));
						}
						else
						{
							_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) III processor"));
						}
					}
				}
				break;
			case 6:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Celeron(TM) processor, model 6"));
				break;
			case 8:
				_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Intel Pentium(R) III Coppermine processor"));
				break;
			default:
			{
				int nBrandIndex = 0;

				while( (nBrandIndex < BRANDTABLESIZE) && ((long)(m_dwFeatureEbx & 0xff) != brand_table[nBrandIndex].lBrandValue) )
				{
					nBrandIndex++;
				}

				if( nBrandIndex < BRANDTABLESIZE )
				{
					_tcscpy_s(tszCpuName, _countof(tszCpuName), brand_table[nBrandIndex].pszBrand);
				}
				else
				{
					_tcscpy_s(tszCpuName, _countof(tszCpuName), _T("Unknown Genuine Intel processor"));
				}
				break;
			}
			}
		}

		if( (m_dwSignature & MMX_FLAG) == MMX_FLAG )
		{
			if( tszCpuName && _tcslen(tszCpuName) > 0 )
				_stprintf_s(m_Cpu.m_tszProcessorName, _countof(m_Cpu.m_tszProcessorName), _T("%s with MMX"), tszCpuName);
			else m_Cpu.m_tszProcessorName[0] = '\0';
		}
	}
}

//***************************************************************************
// @brief 캐시 레지스터 값을 기반으로 셀러론 및 제온 프로세서 여부를 감지합니다.
// @param dwRegisterCache 검사할 레지스터 값
// @param pbIsCeleron Celeron 판별 결과를 저장할 포인터
// @param pbIsXeon Xeon 판별 결과를 저장할 포인터
// @param bIsEax EAX 레지스터 여부 (기본값: false)
//***************************************************************************
void CCpuInfo::GetCeleronAndXeon(DWORD dwRegisterCache, BOOL* pbIsCeleron, BOOL* pbIsXeon, BOOL bIsEax)
{
	DWORD dwCacheTemp;

	dwCacheTemp = dwRegisterCache & 0xFF000000;
	if( dwCacheTemp == 0x40000000 )
	{
		*pbIsCeleron = true;
	}
	if( (dwCacheTemp >= 0x44000000) && (dwCacheTemp <= 0x45000000) )
	{
		*pbIsXeon = true;
	}

	dwCacheTemp = dwRegisterCache & 0xFF0000;
	if( dwCacheTemp == 0x400000 )
	{
		*pbIsCeleron = true;
	}
	if( (dwCacheTemp >= 0x440000) && (dwCacheTemp <= 0x450000) )
	{
		*pbIsXeon = true;
	}

	dwCacheTemp = dwRegisterCache & 0xFF00;
	if( dwCacheTemp == 0x4000 )
	{
		*pbIsCeleron = true;
	}
	if( (dwCacheTemp >= 0x4400) && (dwCacheTemp <= 0x4500) )
	{
		*pbIsXeon = true;
	}

	if( !bIsEax )
	{
		dwCacheTemp = dwRegisterCache & 0xFF;     // possibly not needed for m_dwCacheEax
		if( dwCacheTemp == 0x40000000 )
		{
			*pbIsCeleron = true;
		}
		if( (dwCacheTemp >= 0x44000000) && (dwCacheTemp <= 0x45000000) )
		{
			*pbIsXeon = true;
		}
	}
}

//***************************************************************************
// @brief QueryPerformanceCounter를 활용해 CPU 속도를 계산합니다.
// @return 계산된 CPU 클럭 속도 (MHz)
//***************************************************************************
__int64 CCpuInfo::CalculateCpuSpeed() const
{
	__int64	nStartTicks = 0;
	__int64	nEndTicks = 0;
	__int64	nTotalTicks = 0;
	__int64 n64Frequency;
	__int64 n64Start;
	__int64 n64Stop;
	__int64 n64TotalTicks;
	__int64 n64CpuSpeed;

	if( QueryPerformanceFrequency((LARGE_INTEGER*)&n64Frequency) )
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&n64Start);
		nStartTicks = GetTimeStamp();

		Sleep(300);

		nEndTicks = GetTimeStamp();

		QueryPerformanceCounter((LARGE_INTEGER*)&n64Stop);
		nTotalTicks = nEndTicks - nStartTicks;

		n64TotalTicks = nTotalTicks;
		n64CpuSpeed = n64TotalTicks / ((1000000 * (n64Stop - n64Start)) / n64Frequency);
	}
	else
	{
		n64CpuSpeed = 0;
	}

	return n64CpuSpeed;
}

//***************************************************************************
// @brief timeGetTime을 이용한 두 번째 측정 알고리즘으로 CPU 속도를 계산합니다.
// @return 계산된 CPU 클럭 속도 (MHz)
//***************************************************************************
__int64 CCpuInfo::CalculateCpuSpeedMethod2() const
{
	int   nTimeStart = 0;
	int   nTimeStop = 0;
	__int64 nStartTicks = 0;
	__int64 nEndTicks = 0;
	__int64 nTotalTicks = 0;
	__int64 nCpuSpeed = 0;

	nTimeStart = timeGetTime();

	for( ;;)
	{
		nTimeStop = timeGetTime();

		if( (nTimeStop - nTimeStart) > 1 )
		{
			nStartTicks = GetTimeStamp();
			break;
		}
	}

	nTimeStart = nTimeStop;

	for( ;;)
	{
		nTimeStop = timeGetTime();
		if( (nTimeStop - nTimeStart) > 500 )    // one-half second
		{
			nEndTicks = GetTimeStamp();
			break;
		}
	}

	nTotalTicks = nEndTicks - nStartTicks;
	nCpuSpeed = nTotalTicks / 500000;

	return (nCpuSpeed);
}

//***************************************************************************
// @brief Windows 레지스트리에서 표기된 CPU 속도 정보를 읽어옵니다.
// @return 레지스트리에 등록된 CPU 속도 (MHz)
//***************************************************************************
__int64 CCpuInfo::GetCpuSpeedFromRegistry() const
{
	HKEY	hKey;

	DWORD	dwSpeed = 0;
	DWORD	dwType = 0;
	DWORD	dwSpeedSize = 0;
	long	lRetCode = 0;

	// Get the processor speed from registry
	lRetCode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Hardware\\Description\\System\\CentralProcessor\\0"), 0, KEY_QUERY_VALUE, &hKey);
	if( lRetCode == ERROR_SUCCESS )
	{
		lRetCode = ::RegQueryValueEx(hKey, _T("~MHz"), NULL, &dwType, (LPBYTE)&dwSpeed, &dwSpeedSize);
		if( lRetCode == ERROR_SUCCESS )
		{
			//----------------------------------------------------------
			// this function was modified so that it doesn't actually
			// modify the object in any way. it's more of a utility
			// function like calculateCpuSpeed() and 
			// calculateCpuSpeedMethod2()
			//
		}
		else
		{
			// explicity make the speed 0 just in case RegQueryValueEx puts a value in dwSpeed
			dwSpeed = 0;
		}
	}
	else
	{
		dwSpeed = 0;
	}

	// Make sure to close the reg key
	RegCloseKey(hKey);

	return (dwSpeed);
}

//***************************************************************************
// @brief 타임스탬프 카운터(RDTSC) 값을 가져옵니다.
// @return 타임스탬프 카운터 값
//***************************************************************************
__int64 CCpuInfo::GetTimeStamp() const
{
	return __rdtsc();
}

//***************************************************************************
// @brief MMX 명령 집합 지원 여부를 확인합니다.
// @return 지원 시 TRUE, 미지원 시 FALSE
//***************************************************************************
BOOL CCpuInfo::IsMMXSupported() const
{
	return ((m_Cpu.m_dwFeatures & SUPPORT_MMX) == SUPPORT_MMX);
}

//***************************************************************************
// @brief SSE 명령 집합 지원 여부를 확인합니다.
// @return 지원 시 TRUE, 미지원 시 FALSE
//***************************************************************************
BOOL CCpuInfo::IsSSESupported() const
{
	return ((m_Cpu.m_dwFeatures & SUPPORT_SSE) == SUPPORT_SSE);
}

//***************************************************************************
// @brief SSE2 명령 집합 지원 여부를 확인합니다.
// @return 지원 시 TRUE, 미지원 시 FALSE
//***************************************************************************
BOOL CCpuInfo::IsSSE2Supported() const
{
	return ((m_Cpu.m_dwFeatures & SUPPORT_SSE2) == SUPPORT_SSE2);
}

//***************************************************************************
// @brief 3DNow! 명령 집합 지원 여부를 확인합니다.
// @return 지원 시 TRUE, 미지원 시 FALSE
//***************************************************************************
BOOL CCpuInfo::Is3DNowSupported() const
{
	return ((m_Cpu.m_dwFeatures & SUPPORT_3DNOW) == SUPPORT_3DNOW);
}