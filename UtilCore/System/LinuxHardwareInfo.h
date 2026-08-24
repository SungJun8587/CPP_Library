
//***************************************************************************
// LinuxHardwareInfo.h : interface for the Linux(sysfs/procfs/CPUID)-based
//                       hardware information classes.
//
// WmiHardwareInfo.h(Windows/WMI)의 리눅스 대응입니다. 같은 HwInfoStructs.h의
// HWINFO_* 구조체를 그대로 공유하고, 각 클래스의 퍼블릭 인터페이스(메서드 이름)도
// Windows non-WMI 클래스(CSmbiosBiosInfo/CHdDiskInfo/CPciInfo 등)와 최대한
// 동일하게 맞췄습니다 - 호출부 코드가 플랫폼별로 클래스 이름만 바뀌고 나머지는
// 그대로 재사용되도록 하기 위함입니다.
//
// 데이터 출처:
//   CLinuxCpuInfo        - CPUID(GCC/Clang 내장함수) + sysfs(코어수/캐시)
//   CLinuxBiosInfo        - /sys/class/dmi/id/*
//   CLinuxMainBoardInfo   - /sys/class/dmi/id/*
//   CLinuxMemoryInfo      - /proc/meminfo(전체 통계) + /sys/firmware/dmi/tables/DMI
//                           (슬롯별 정보, Windows의 GetSystemFirmwareTable('RSMB')와
//                           완전히 같은 raw SMBIOS 바이트 - root 권한 필요)
//   CLinuxDiskInfo        - /sys/block/*  (root 불필요 - Windows IOCTL 버전과 다른 점)
//   CLinuxPciInfo         - /sys/bus/pci/devices/*  (root 불필요)
//***************************************************************************

#ifndef __LINUXHARDWAREINFO_H__
#define __LINUXHARDWAREINFO_H__

#if defined(_WIN32)
#error "LinuxHardwareInfo.h는 Linux 전용입니다. Windows 빌드에는 System/CpuInfo.h, System/SmbiosHardwareInfo.h, System/PciInfo.h를 쓰세요."
#endif

#include <vector>

#ifndef __HWINFOSTRUCTS_H__
#include <System/HwInfoStructs.h>
#endif

//***************************************************************************
// @class CLinuxCpuInfo
// @brief CPUID(GCC/Clang 내장함수 __get_cpuid_count)와 sysfs로 CPU 정보를 수집하는
//        클래스입니다. CCpuInfo(Windows non-WMI)의 리눅스 대응.
// @details 물리 코어 수/L2·L3 캐시 크기는 CPUID Leaf4를 직접 파싱하는 대신
//          /sys/devices/system/cpu/의 topology/cache 정보를 사용합니다 - 커널이
//          이미 파싱해 둔 값이라 벤더별 CPUID 리프 차이를 직접 다루는 것보다 안전.
//          MMX/SSE/SSE2/3DNow! 지원 여부는 CCpuInfo와 동일한 커스텀 비트 스킴
//          (SUPPORT_MMX=0x0001 등)으로 m_dwFeatures를 채워 인터페이스 호환성을 맞춤.
//***************************************************************************
class CLinuxCpuInfo
{
public:
    CLinuxCpuInfo();
    ~CLinuxCpuInfo();

    BOOL GetInformation();

    unsigned int GetSpeedMHz() const { return (unsigned int)m_Cpu.m_nSpeed; }
    const TCHAR* GetProcessorName() const { return m_Cpu.m_tszProcessorName; }
    const TCHAR* GetVendorName() const { return m_Cpu.m_tszVendorName; }
    const TCHAR* GetProcessorId() const { return m_Cpu.m_tszProcessorId; }
    int GetNumberOfProcessors() const { return m_Cpu.m_nNumberCpus; }
    int GetCPUFamily() const { return m_Cpu.m_nFamily; }
    int GetCPUModel() const { return m_Cpu.m_nModel; }
    int GetCPUStepping() const { return m_Cpu.m_nStepping; }
    unsigned int GetNumberOfCores() const { return m_Cpu.m_dwNumberOfCores; }
    unsigned int GetL2CacheSize() const { return m_Cpu.m_dwL2CacheSize; }
    unsigned int GetL3CacheSize() const { return m_Cpu.m_dwL3CacheSize; }

    BOOL IsMMXSupported() const;
    BOOL IsSSESupported() const;
    BOOL IsSSE2Supported() const;
    BOOL Is3DNowSupported() const;

private:
    HWINFO_CPU m_Cpu;
};


//***************************************************************************
// @class CLinuxBiosInfo
// @brief /sys/class/dmi/id/{bios_vendor,bios_version,bios_date}와
//        /sys/class/dmi/id/product_serial을 읽는 클래스입니다.
//        CSmbiosBiosInfo(Windows non-WMI)의 리눅스 대응 - 메서드명 동일.
// @details product_serial은 배포판에 따라 root 권한이 필요할 수 있습니다
//          (0400으로 잠긴 경우 흔함) - 실패해도 나머지 필드는 정상 수집됩니다.
//***************************************************************************
class CLinuxBiosInfo
{
public:
    CLinuxBiosInfo();
    ~CLinuxBiosInfo();

    BOOL GetInformation();

    const TCHAR* GetManufacturer() const { return m_Bios.m_tszManufacturer; }
    const TCHAR* GetVersion() const { return m_Bios.m_tszVersion; }
    const TCHAR* GetReleaseDate() const { return m_Bios.m_tszReleaseDate; }
    const TCHAR* GetSerialNumber() const { return m_Bios.m_tszSerialNumber; }

private:
    HWINFO_BIOS m_Bios;
};


//***************************************************************************
// @class CLinuxMainBoardInfo
// @brief /sys/class/dmi/id/{board_vendor,board_name,board_version,board_serial}을
//        읽는 클래스입니다. CSmbiosMainBoardInfo(Windows non-WMI)의 리눅스 대응.
//***************************************************************************
class CLinuxMainBoardInfo
{
public:
    CLinuxMainBoardInfo();
    ~CLinuxMainBoardInfo();

    BOOL GetInformation();

    const TCHAR* GetManufacturer() const { return m_MainBoard.m_tszManufacturer; }
    const TCHAR* GetProduct() const { return m_MainBoard.m_tszProduct; }
    const TCHAR* GetVersion() const { return m_MainBoard.m_tszVersion; }
    const TCHAR* GetSerialNumber() const { return m_MainBoard.m_tszSerialNumber; }

private:
    HWINFO_MAINBOARD m_MainBoard;
};


//***************************************************************************
// @class CLinuxMemoryInfo
// @brief /proc/meminfo(전체 통계) + /sys/firmware/dmi/tables/DMI(슬롯별 정보,
//        SMBIOS Type17)로 메모리 정보를 수집하는 클래스입니다.
//        CSmbiosMemoryInfo(Windows non-WMI)의 리눅스 대응.
// @details /sys/firmware/dmi/tables/DMI는 대부분의 배포판에서 root 전용(0400)이라
//          비root 환경에서는 전체 통계(GetTotalMemSize 등)만 채워지고 슬롯별
//          배열(GetRamArray)은 비어있을 수 있습니다.
//***************************************************************************
class CLinuxMemoryInfo
{
public:
    CLinuxMemoryInfo();
    ~CLinuxMemoryInfo();

    BOOL GetInformation();

    DWORD GetRamCount() const { return (DWORD)m_sRamArray.size(); }
    const __int64 GetTotalMemSize() const { return m_Memory.m_nTotalMemSize; }
    const __int64 GetPhysicalMemSize() const { return m_Memory.m_nPhysicalMemSize; }
    const __int64 GetUseMemSize() const { return m_Memory.m_nTotalMemSize - m_Memory.m_nPhysicalMemSize; }
    const double GetPercentUsedRam() const
    {
        if( m_Memory.m_nTotalMemSize == 0 ) return 0.0;
        return (double)(m_Memory.m_nTotalMemSize - m_Memory.m_nPhysicalMemSize) / (double)m_Memory.m_nTotalMemSize;
    }
    const __int64 GetTotalVirtualMemSize() const { return m_Memory.m_nTotalVirtualMemSize; }
    const __int64 GetFreeVirtualMemSize() const { return m_Memory.m_nFreeVirtualMemSize; }
    const __int64 GetTotalPageFile() const { return m_Memory.m_nTotalPageFileSize; }
    const __int64 GetFreePageFile() const { return m_Memory.m_nFreePageFileSize; }

    const std::vector<HWINFO_RAM*>* GetRamArray() const { return &m_sRamArray; }

private:
    HWINFO_MEMORY m_Memory;
    std::vector<HWINFO_RAM*> m_sRamArray;
};


//***************************************************************************
// @class CLinuxDiskInfo
// @brief /sys/block/*를 순회하여 물리 디스크 정보를 수집하는 클래스입니다.
//        CHdDiskInfo(Windows non-WMI, IOCTL 기반)의 리눅스 대응.
// @details Windows CHdDiskInfo와 달리 root 권한이 필요 없습니다 - sysfs는 대부분
//          world-readable입니다. 단, BusType은 디바이스명 접두사(nvme/sd 등)와
//          device 심볼릭 링크 경로의 "usb" 포함 여부로 추정한 값이라 Windows IOCTL
//          버전(STORAGE_ADAPTER_DESCRIPTOR 기반 정확한 BusType)만큼 정밀하지 않습니다.
//***************************************************************************
class CLinuxDiskInfo
{
public:
    CLinuxDiskInfo();
    ~CLinuxDiskInfo();

    BOOL GetInformation();

    const std::vector<HWINFO_HDDISK*>* GetHdDiskArray() const { return &m_sHdDiskArray; }

private:
    std::vector<HWINFO_HDDISK*> m_sHdDiskArray;
};


//***************************************************************************
// @class CLinuxPciInfo
// @brief /sys/bus/pci/devices/*를 순회하여 PCI 장치 정보를 수집하는 클래스입니다.
//        CPciInfo(Windows non-WMI, SetupAPI+어셈블리 기반)의 리눅스 대응.
// @details Bus/Device/Function은 디렉터리명(도메인:버스:디바이스.펑션)에서 직접
//          파싱하고, Class Code는 sysfs class 파일에서 직접 읽으므로 Windows
//          버전보다 오히려 더 간단하고 root 권한도 필요 없습니다. VendorName은
//          PciInfo64.asm/86.asm과 동일한 20개 내장 벤더 테이블을 재사용합니다.
//          Description(장치 설명)은 pci.ids 데이터베이스 파싱이 필요해 미구현 -
//          빈 문자열로 남습니다.
//***************************************************************************
class CLinuxPciInfo
{
public:
    CLinuxPciInfo();
    ~CLinuxPciInfo();

    BOOL GetInformation();

    const std::vector<HWINFO_PCIDEVICE*>* GetPciDeviceArray() const { return &m_sPciArray; }

private:
    std::vector<HWINFO_PCIDEVICE*> m_sPciArray;
};

#endif // ndef __LINUXHARDWAREINFO_H__
