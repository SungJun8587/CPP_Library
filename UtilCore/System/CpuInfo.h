
//***************************************************************************
// CpuInfo.h: x86/x64 CPU Hardware Information Library & CCpuInfo Class Interface
//
//***************************************************************************

#ifndef __CPUINFO_H__
#define __CPUINFO_H__

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

#include <cstdint>
#include <intrin.h>
#include <mmsystem.h> // timeGetTime 정의 헤더 추가

#pragma comment(lib, "winmm.lib") // timeGetTime 링커 라이브러리 연동

// ==========================================================================
// 1. C-Style Global CPU API Interfaces
// ==========================================================================
extern "C" {
	//***************************************************************************
	// @brief   CPU가 CPUID 지시어를 지원하는지 여부를 검사합니다.
	// @param   없음
	// @return  int - 1: 지원함, 0: 미지원
	// @detail  EFLAGS 레지스터의 ID 비트(Bit 21)를 토글하여 CPUID 지원 여부를 판별합니다.
	//***************************************************************************
	int cpu_id_supported();

	//***************************************************************************
	// @brief   CPUID 지시어를 실행하고 결과를 레지스터별 포인터로 반환합니다.
	// @param   eax [in/out] EAX 레지스터 값 포인터
	// @param   ebx [in/out] EBX 레지스터 값 포인터
	// @param   ecx [in/out] ECX 레지스터 값 포인터
	// @param   edx [in/out] EDX 레지스터 값 포인터
	// @return  없음
	// @detail  입력받은 레지스터 값을 EAX/EBX/ECX/EDX에 로드한 후 CPUID를 수행하고 결과를 다시 할당합니다.
	//***************************************************************************
	void cpu_id(unsigned long* eax, unsigned long* ebx, unsigned long* ecx, unsigned long* edx);

	//***************************************************************************
	// @brief   CPU 제조사 문자열(Vendor String)을 취득합니다.
	// @param   highestcpuid [out] 지원하는 최대 CPUID Leaf 값 수신 포인터
	// @param   vendorname   [out] 제조사 이름(최소 13바이트) 버퍼 포인터
	// @return  int - 1: 성공, 0: 실패
	// @detail  CPUID Leaf 0을 수행하여 EBX-EDX-ECX 레지스터에 할당된 제조사 식별 문자열을 추출합니다.
	//***************************************************************************
	int cpu_vendor(unsigned long* highestcpuid, char* vendorname);

	//***************************************************************************
	// @brief   CPU 브랜드 이름 파트 0을 취득합니다.
	// @param   없음
	// @return  long long - CPU Brand String의 앞선 8바이트 데이터
	// @detail  CPUID Extended Leaf 80000002h의 EAX, EBX 값을 결합하여 반환합니다.
	//***************************************************************************
	long long cpu_brand_part0();
	long long cpu_brand_part1();
	long long cpu_brand_part2();
	long long cpu_brand_part3();
	long long cpu_brand_part4();
	long long cpu_brand_part5();

	//***************************************************************************
	// @brief   CPU 브랜드 이름 전체(최대 48자)를 취득합니다.
	// @param   brandname [out] 브랜드 이름 버퍼 포인터
	// @return  int - 1: 성공, 0: 실패
	//***************************************************************************
	int cpu_brand(char* brandname);

	//***************************************************************************
	// @brief   지정한 레벨의 CPU 캐시 크기를 계산합니다.
	// @param   level [in] 탐색할 캐시 레벨 (1: L1 Data, 2: L2, 3: L3)
	// @return  unsigned int - 캐시 크기 (KB 단위, 탐색 실패 시 0)
	// @detail  CPUID Leaf 4(Deterministic Cache Parameters)를 루프 순회하며 (Ways+1)*(Partitions+1)*(Line_Size+1)*(Sets+1) 공식을 통해 크기를 계산합니다.
	//***************************************************************************
	unsigned int cpu_cache_size_kb(unsigned int level);

	//***************************************************************************
	// @brief   현재 명령어를 실행 중인 코어의 하이브리드 코어 유형을 판별합니다.
	// @param   없음
	// @return  unsigned int - 0x20: Efficient Core (E-Core), 0x40: Performance Core (P-Core), 0: 일반 CPU
	// @detail  CPUID Leaf 1Ah(Hybrid Core Native Model ID)의 EAX[31:24] 비트를 읽어 코어의 아키텍처 타입을 구분합니다.
	//***************************************************************************
	unsigned int cpu_core_type();

	//***************************************************************************
	// @brief   CPU의 실시간 타임스탬프 카운터(TSC)를 읽습니다.
	// @param   없음
	// @return  uint64_t - 전원 공급 이후 누적된 64비트 CPU 클록 사이클 수
	// @detail  RDTSC 지시어를 실행하여 EDX:EAX에 저장된 64비트 카운터 값을 취득합니다.
	//***************************************************************************
	uint64_t cpu_read_tsc();
}

// ==========================================================================
// 2. Class Definitions and Data Structures
// ==========================================================================

//***************************************************************************
// @brief CPU 하드웨어 상세 정보를 저장하는 구조체입니다.
// @details CPU의 속도, 프로세서 수, 패밀리/모델/스테핑 정보, 기능 플래그 및 제조사/프로세서 명칭 등의 정보를 보유합니다.
//***************************************************************************
typedef struct _HWINFO_CPU
{
	//***************************************************************************
	// @brief _HWINFO_CPU 구조체의 기본 생성자입니다.
	// @details 모든 멤버 변수(속도, 카운트, 시그니처, 이름 문자열 등)를 초기화합니다.
	//***************************************************************************
	_HWINFO_CPU() {
		m_nSpeed = 0;
		m_nNumberCpus = 0;
		m_nFamily = 0;
		m_nModel = 0;
		m_nStepping = 0;
		m_nFamilyEx = 0;
		m_nModelEx = 0;
		m_dwFeatures = 0;

		m_tszVendorName[0] = '\0';
		m_tszProcessorName[0] = '\0';
	}

	unsigned __int64	m_nSpeed; // CPU 속도 (MHz)
	int			m_nNumberCpus; // 프로세서 개수
	int			m_nFamily; // CPU 패밀리
	int			m_nModel; // CPU 모델
	int			m_nStepping; // CPU 스테핑
	int			m_nFamilyEx; // 확장 패밀리
	int			m_nModelEx; // 확장 모델

	DWORD		m_dwFeatures; // CPU 기능 플래그

	TCHAR	m_tszVendorName[CPU_VENDOR_STRLEN]; // 제조사 이름
	TCHAR	m_tszProcessorName[CPU_GENNAME_STRLEN]; // 프로세서 이름
} HWINFO_CPU, * PHWINFO_CPU;


//***************************************************************************
// @brief CPUID 명령어 수행을 위한 래퍼 클래스입니다.
// @details x86/x64 환경에서 CPUID 어셈블리 명령 또는 내장 함수(__cpuidex)를 실행하고, 그 결과로 얻은 레지스터(EAX, EBX, ECX, EDX) 값을 관리합니다.
//***************************************************************************
class CpuID
{
public:
	//***************************************************************************
	// @brief CPUID 명령을 실행하여 레지스터 값을 가져오는 생성자입니다.
	// @details 플랫폼(WIN32 및 인라인 어셈블리)에 맞추어 cpuid 레지스터 값을 계산 및 저장합니다.
	// @param funcId CPUID 기능 ID
	// @param subFuncId CPUID 서브 기능 ID
	//***************************************************************************
	explicit CpuID(unsigned funcId, unsigned subFuncId)
	{
#ifdef _WIN32
		__cpuidex((int*)regs, (int)funcId, (int)subFuncId);
#else
		asm volatile
			("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
				: "a" (funcId), "c" (subFuncId));
		// ECX is set to zero for CPUID function 4
#endif
	}

	//***************************************************************************
	// @brief EAX 레지스터 값을 반환합니다.
	// @details 저장된 CPUID 결과 중 EAX 값의 참조를 반환합니다.
	// @return EAX 레지스터 값
	//***************************************************************************
	const uint32_t& EAX() const {
		return regs[0];
	}

	//***************************************************************************
	// @brief EBX 레지스터 값을 반환합니다.
	// @details 저장된 CPUID 결과 중 EBX 값의 참조를 반환합니다.
	// @return EBX 레지스터 값
	//***************************************************************************
	const uint32_t& EBX() const {
		return regs[1];
	}

	//***************************************************************************
	// @brief ECX 레지스터 값을 반환합니다.
	// @details 저장된 CPUID 결과 중 ECX 값의 참조를 반환합니다.
	// @return ECX 레지스터 값
	//***************************************************************************
	const uint32_t& ECX() const {
		return regs[2];
	}

	//***************************************************************************
	// @brief EDX 레지스터 값을 반환합니다.
	// @details 저장된 CPUID 결과 중 EDX 값의 참조를 반환합니다.
	// @return EDX 레지스터 값
	//***************************************************************************
	const uint32_t& EDX() const {
		return regs[3];
	}

private:
	uint32_t regs[4]; // 레지스터 배열 (EAX, EBX, ECX, EDX)
};

//***************************************************************************
// @brief CPU 하드웨어 정보의 탐지 및 조회를 담당하는 클래스입니다.
// @details CPU 제조사, 모델, 클럭 속도, 지원 명령어 집합(MMX, SSE, SSE2, 3DNow!) 등 다양한 정보를 수집하고 외부 인터페이스로 제공합니다.
//***************************************************************************
class CCpuInfo
{
public:
	CCpuInfo();
	~CCpuInfo();

public:
	BOOL GetInformation();

	//***************************************************************************
	// @brief CPU 속도를 MHz 단위로 반환합니다.
	// @details 수집된 m_Cpu 구조체 내 속도 값을 unsigned int 형으로 변환하여 반환합니다.
	//          측정에 실패한 경우(m_nSpeed가 음수 센티널 값) 부호 없는 정수로 캐스팅 시
	//          거대한 값으로 둔갑하는 것을 막기 위해 0을 반환합니다.
	// @return CPU 속도 (MHz), 측정 실패 시 0
	//***************************************************************************
	unsigned int GetSpeedMHz() const {
		if( m_Cpu.m_nSpeed < 0 ) return 0;
		return (unsigned int)(m_Cpu.m_nSpeed);
	}

	//***************************************************************************
	// @brief 프로세서 이름을 반환합니다.
	// @details m_Cpu 구조체에 저장된 프로세서 명칭 포인터를 반환합니다.
	// @return 프로세서 이름 포인터
	//***************************************************************************
	const TCHAR* GetProcessorName() const {
		return m_Cpu.m_tszProcessorName;
	}

	//***************************************************************************
	// @brief 제조사 이름을 반환합니다.
	// @details m_Cpu 구조체에 저장된 제조사 명칭 포인터를 반환합니다.
	// @return 제조사 이름 포인터
	//***************************************************************************
	const TCHAR* GetVendorName() const {
		return m_Cpu.m_tszVendorName;
	}

	//***************************************************************************
	// @brief 논리 프로세서 개수를 반환합니다.
	// @details 시스템에서 인식된 논리 CPU 코어 수를 반환합니다.
	// @return 프로세서 개수
	//***************************************************************************
	int	GetNumberOfProcessors() const {
		return m_Cpu.m_nNumberCpus;
	}

	//***************************************************************************
	// @brief CPU Family 값을 반환합니다.
	// @details 추출된 CPU Family 식별자 값을 반환합니다.
	// @return CPU Family ID
	//***************************************************************************
	int GetCPUFamily() const {
		return m_Cpu.m_nFamily;
	}

	//***************************************************************************
	// @brief CPU Model 값을 반환합니다.
	// @details 추출된 CPU Model 식별자 값을 반환합니다.
	// @return CPU Model ID
	//***************************************************************************
	int GetCPUModel() const {
		return m_Cpu.m_nModel;
	}

	//***************************************************************************
	// @brief CPU Stepping 값을 반환합니다.
	// @details 추출된 CPU Stepping 식별자 값을 반환합니다.
	// @return CPU Stepping ID
	//***************************************************************************
	int GetCPUStepping() const {
		return m_Cpu.m_nStepping;
	}

	BOOL IsMMXSupported() const;
	BOOL IsSSESupported() const;
	BOOL IsSSE2Supported() const;
	BOOL Is3DNowSupported() const;

	void	GetNameString();

private:
	void	DetectCpuGenInfo();
	void	DetectCpuDescInfo();
	void	DetectCpuSpeed();
	void	DetectVendorName();

	std::string GetHighestCpuId(DWORD& dwHighest) const;
	void	GetCpuIdentification();
	DWORD	GetLargestExtendedFeature();
	void	GetExtendedFeature();

	void	GetIntelCacheInfo();
	void	GetAmdL1CacheInfo();
	void	GetAmdL2CacheInfo();
	_tstring GetOldIntelName() const;

	void	GetCeleronAndXeon(DWORD dwRegisterCache, BOOL* pbIsCeleron, BOOL* pbIsXeon, BOOL bIsEax = false) const;

	__int64	CalculateCpuSpeed() const;
	__int64	CalculateCpuSpeedMethod2() const;
	__int64	GetCpuSpeedFromRegistry() const;
	__int64	GetTimeStamp() const;

private:
	DWORD	m_dwSignature; // CPU 시그니처 (EAX 값)
	DWORD	m_dwFeatureEbx; // EBX 기능 플래그
	DWORD	m_dwFeatureEcx; // ECX 기능 플래그
	DWORD	m_dwFeatures; // EDX 기능 플래그
	DWORD	m_dwExtendedFeatureEdx; // 확장 기능 EDX 플래그

	DWORD	m_dwEax1; // 캐시 및 기능 레지스터 1 (EAX)
	DWORD	m_dwEbx1; // 캐시 및 기능 레지스터 1 (EBX)
	DWORD	m_dwEcx1; // 캐시 및 기능 레지스터 1 (ECX)
	DWORD	m_dwEdx1; // 캐시 및 기능 레지스터 1 (EDX)

	DWORD	m_dwEax2; // 캐시 및 기능 레지스터 2 (EAX)
	DWORD	m_dwEbx2; // 캐시 및 기능 레지스터 2 (EBX)
	DWORD	m_dwEcx2; // 캐시 및 기능 레지스터 2 (ECX)
	DWORD	m_dwEdx2; // 캐시 및 기능 레지스터 2 (EDX)

	HWINFO_CPU	m_Cpu; // CPU 종합 정보 구조체
};

#endif // ndef __CPUINFO_H__