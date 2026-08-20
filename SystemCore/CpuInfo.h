
//***************************************************************************
// CpuInfo.h: interface for the CCpuInfo Class.
//
//***************************************************************************

#ifndef __CPUINFO_H__
#define __CPUINFO_H__

#ifndef __SYSTEMBASEDEFINE_H__
#include <SystemBaseDefine.h>
#endif

#include <intrin.h>

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
	// @return CPU 속도 (MHz)
	//***************************************************************************
	unsigned int GetSpeedMHz() const {
		return (unsigned int)(m_Cpu.m_nSpeed);
	}

	//***************************************************************************
	// @brief 프로세서 이름을 반환합니다.
	// @details m_Cpu 구조체에 저장된 프로세서 명칭 포인터를 반환합니다.
	// @return 프로세서 이름 포인터
	//***************************************************************************
	TCHAR* GetProcessorName() const {
		return (TCHAR*)m_Cpu.m_tszProcessorName;
	}

	//***************************************************************************
	// @brief 제조사 이름을 반환합니다.
	// @details m_Cpu 구조체에 저장된 제조사 명칭 포인터를 반환합니다.
	// @return 제조사 이름 포인터
	//***************************************************************************
	TCHAR* GetVendorName() const {
		return (TCHAR*)m_Cpu.m_tszVendorName;
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

	DWORD	GetHighestCpuId();
	void	GetCpuIdentification();
	DWORD	GetLargestExtendedFeature();
	void	GetExtendedFeature();

	void	GetIntelCacheInfo();
	void	GetAmdL1CacheInfo();
	void	GetAmdL2CacheInfo();
	void	GetOldIntelName();

	void	GetCeleronAndXeon(DWORD dwRegisterCache, BOOL* pbIsCeleron, BOOL* pbIsXeon, BOOL bIsEax = false);

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