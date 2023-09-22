
//***************************************************************************
// CpuInfo.h: interface for the CCpuInfo Class.
//
//***************************************************************************

#ifndef __CPUINFO_H__
#define __CPUINFO_H__

#include <intrin.h>

//***************************************************************************
// DWORD dwEax = 0, dwEbx = 0, dwEcx = 0, dwEdx = 0;
// std::array<int, 4> cpui;
//
// Calling __cpuid with 0x0 as the function_id argument
// gets the number of the highest valid function ID.
// __cpuid(cpui.data(), 0);
// dwEax = cpui[0];
// dwEbx = cpui[1];
// dwEcx = cpui[2];
// dwEdx = cpui[3];
//


//***************************************************************************
//
typedef struct _HWINFO_CPU
{
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

	unsigned __int64	m_nSpeed;
	int			m_nNumberCpus;
	int			m_nFamily;
	int			m_nModel;
	int			m_nStepping;
	int			m_nFamilyEx;
	int			m_nModelEx;
	
	DWORD		m_dwFeatures;

	TCHAR	m_tszVendorName[ CPU_VENDOR_STRLEN ];
	TCHAR	m_tszProcessorName[ CPU_GENNAME_STRLEN ];
} HWINFO_CPU, *PHWINFO_CPU;


//***************************************************************************
//
class CpuID 
{
public:
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

	const uint32_t& EAX() const {
		return regs[0];
	}
	const uint32_t& EBX() const {
		return regs[1];
	}
	const uint32_t& ECX() const {
		return regs[2];
	}
	const uint32_t& EDX() const {
		return regs[3];
	}

private:
	uint32_t regs[4];
};

//***************************************************************************
//
class CCpuInfo  
{
public:
	CCpuInfo();
	~CCpuInfo();

public:
	BOOL GetInformation();

	unsigned int GetSpeedMHz() const {
		return (unsigned int)(m_Cpu.m_nSpeed);
	}
	TCHAR* GetProcessorName() const {
		return (TCHAR *)m_Cpu.m_tszProcessorName;
	}
	TCHAR* GetVendorName() const {
		return (TCHAR *)m_Cpu.m_tszVendorName;
	}
	int	GetNumberOfProcessors() const {
		return m_Cpu.m_nNumberCpus;
	}
	int GetCPUFamily() const {
		return m_Cpu.m_nFamily;
	}
	int GetCPUModel() const {
		return m_Cpu.m_nModel;
	}
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

	void	GetCeleronAndXeon( DWORD dwRegisterCache, BOOL *pbIsCeleron, BOOL *pbIsXeon, BOOL bIsEax = false );

	__int64	CalculateCpuSpeed() const;
	__int64	CalculateCpuSpeedMethod2() const;
	__int64	GetCpuSpeedFromRegistry() const;
	__int64	GetTimeStamp() const;

private:
	DWORD	m_dwSignature;
	DWORD	m_dwFeatureEbx;
	DWORD	m_dwFeatureEcx;
	DWORD	m_dwFeatures;
	DWORD   m_dwExtendedFeatures;

	DWORD	m_dwEax1;
	DWORD	m_dwEbx1;
	DWORD	m_dwEcx1;
	DWORD	m_dwEdx1;
	
	DWORD	m_dwEax2;
	DWORD	m_dwEbx2;
	DWORD	m_dwEcx2;
	DWORD	m_dwEdx2;

	HWINFO_CPU	m_Cpu;
};

#endif // ndef __CPUINFO_H__

