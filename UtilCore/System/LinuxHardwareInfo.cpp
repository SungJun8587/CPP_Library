
//***************************************************************************
// LinuxHardwareInfo.cpp : implementation of the Linux(sysfs/procfs/CPUID)-based
//                         hardware information classes.
//***************************************************************************

#include "pch.h"
#include "LinuxHardwareInfo.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <sys/sysinfo.h>
#include <cpuid.h>

#define SUPPORT_MMX     0x0001
#define SUPPORT_3DNOW   0x0002
#define SUPPORT_SSE     0x0004
#define SUPPORT_SSE2    0x0008

namespace
{
    //***************************************************************************
    // @brief sysfs/procfs에서 얻은 ANSI 문자열을 TCHAR 버퍼로 복사합니다.
    // @detail Windows 쪽 SmbiosHardwareInfo.cpp의 CopyToTChar와 동일한 목적 -
    //         UNICODE 빌드일 때만 실제 변환이 발생함(mbstowcs 기반, glibc 표준 API).
    //***************************************************************************
    void CopyToTChar(TCHAR* dst, size_t dstCount, const char* src)
    {
#ifdef UNICODE
        mbstowcs(dst, src, dstCount - 1);
        dst[dstCount - 1] = 0;
#else
        strncpy(dst, src, dstCount - 1);
        dst[dstCount - 1] = 0;
#endif
    }

    //***************************************************************************
    // @brief 파일의 첫 줄을 읽어 앞뒤 공백/개행을 제거한 뒤 반환합니다.
    // @return 성공 시 true, 파일이 없거나(권한 부족 포함) 비어있으면 false
    //***************************************************************************
    bool ReadFirstLine(const char* path, std::string& out)
    {
        std::ifstream f(path);
        if( !f.is_open() ) return false;

        std::string line;
        if( !std::getline(f, line) ) return false;

        while( !line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t') )
            line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if( start == std::string::npos ) return false;
        line = line.substr(start);

        if( line.empty() ) return false;
        out = line;
        return true;
    }

    bool ReadSysfsString(const char* path, TCHAR* dst, size_t dstCount)
    {
        std::string line;
        if( !ReadFirstLine(path, line) ) return false;
        CopyToTChar(dst, dstCount, line.c_str());
        return true;
    }

    //***************************************************************************
    // @brief 16진수 문자열(예: "0x8086", "8086")을 담은 파일을 읽어 정수로 변환합니다.
    //***************************************************************************
    bool ReadSysfsHex(const char* path, unsigned long& out)
    {
        std::string line;
        if( !ReadFirstLine(path, line) ) return false;
        out = strtoul(line.c_str(), nullptr, 16);
        return true;
    }

    bool ReadSysfsUint64(const char* path, unsigned __int64& out)
    {
        std::string line;
        if( !ReadFirstLine(path, line) ) return false;
        out = strtoull(line.c_str(), nullptr, 10);
        return true;
    }
}


//***************************************************************************
// CLinuxCpuInfo
//***************************************************************************

CLinuxCpuInfo::CLinuxCpuInfo()
{
}

CLinuxCpuInfo::~CLinuxCpuInfo()
{
}

BOOL CLinuxCpuInfo::IsMMXSupported() const { return (m_Cpu.m_dwFeatures & SUPPORT_MMX) == SUPPORT_MMX; }
BOOL CLinuxCpuInfo::IsSSESupported() const { return (m_Cpu.m_dwFeatures & SUPPORT_SSE) == SUPPORT_SSE; }
BOOL CLinuxCpuInfo::IsSSE2Supported() const { return (m_Cpu.m_dwFeatures & SUPPORT_SSE2) == SUPPORT_SSE2; }
BOOL CLinuxCpuInfo::Is3DNowSupported() const { return (m_Cpu.m_dwFeatures & SUPPORT_3DNOW) == SUPPORT_3DNOW; }

//***************************************************************************
// @brief CPUID + sysfs로 CPU 정보를 수집합니다.
// @return BOOL 정보 수집 성공 여부
// @detail Vendor/Brand 문자열, Family/Model/Stepping, MMX/SSE/SSE2/3DNow!는
//         CPUID(__get_cpuid_count)에서, 물리 코어 수/L2·L3 캐시/클럭 속도는
//         sysfs에서 얻습니다 - Windows CCpuInfo가 CPUID Leaf4를 직접 파싱하는
//         대신 이쪽은 커널이 이미 정리해 둔 sysfs 값을 그대로 씁니다.
//***************************************************************************
BOOL CLinuxCpuInfo::GetInformation()
{
#if !(defined(__x86_64__) || defined(__i386__))
    return FALSE; // ARM 등 비-x86 아키텍처는 CPUID 자체가 없음 - 별도 구현 필요
#else
    unsigned int eax, ebx, ecx, edx;

    // Vendor 문자열 (Leaf 0) - EBX-EDX-ECX 순서로 12바이트
    if( !__get_cpuid(0, &eax, &ebx, &ecx, &edx) ) return FALSE;

    char vendor[13] = { 0 };
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = 0;
    CopyToTChar(m_Cpu.m_tszVendorName, _countof(m_Cpu.m_tszVendorName), vendor);

    // Signature / Feature Flags (Leaf 1)
    if( __get_cpuid(1, &eax, &ebx, &ecx, &edx) )
    {
        m_Cpu.m_nFamily = (eax >> 8) & 0xF;
        if( m_Cpu.m_nFamily == 15 )
            m_Cpu.m_nFamilyEx = (eax >> 16) & 0xFF0;
        m_Cpu.m_nModel = (eax >> 4) & 0xF;
        if( m_Cpu.m_nModel == 15 )
            m_Cpu.m_nModelEx = (eax >> 12) & 0xF;
        m_Cpu.m_nStepping = eax & 0xF;

        DWORD dwFeatures = 0;
        if( edx & (1u << 23) ) dwFeatures |= SUPPORT_MMX;
        if( edx & (1u << 25) ) dwFeatures |= SUPPORT_SSE;
        if( edx & (1u << 26) ) dwFeatures |= SUPPORT_SSE2;
        m_Cpu.m_dwFeatures = dwFeatures;

        // ProcessorId: Windows 버전과 동일하게 EDX:EAX를 16진수로 결합
        char tszId[32];
        snprintf(tszId, sizeof(tszId), "%08X%08X", edx, eax);
        CopyToTChar(m_Cpu.m_tszProcessorId, _countof(m_Cpu.m_tszProcessorId), tszId);
    }

    // 3DNow! 지원 여부 (확장 Leaf 0x80000001, EDX bit31)
    if( __get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx) )
    {
        if( edx & (1u << 31) ) m_Cpu.m_dwFeatures |= SUPPORT_3DNOW;
    }

    // Brand 문자열 (확장 Leaf 0x80000002~04) - 각 리프가 EAX/EBX/ECX/EDX 4바이트씩,
    // 리프당 16바이트 * 3리프 = 48바이트
    char brand[49] = { 0 };
    for( int leaf = 0; leaf < 3; leaf++ )
    {
        if( !__get_cpuid(0x80000002 + leaf, &eax, &ebx, &ecx, &edx) ) break;
        memcpy(brand + leaf * 16 + 0, &eax, 4);
        memcpy(brand + leaf * 16 + 4, &ebx, 4);
        memcpy(brand + leaf * 16 + 8, &ecx, 4);
        memcpy(brand + leaf * 16 + 12, &edx, 4);
    }
    brand[48] = 0;
    CopyToTChar(m_Cpu.m_tszProcessorName, _countof(m_Cpu.m_tszProcessorName), brand);

    // 논리 프로세서 수 - /sys/devices/system/cpu/online 파싱(예: "0-15") 대신
    // 디렉터리 개수를 세는 편이 간단하고 안전함
    {
        int nLogical = 0;
        DIR* dir = opendir("/sys/devices/system/cpu");
        if( dir )
        {
            struct dirent* entry;
            while( (entry = readdir(dir)) != nullptr )
            {
                unsigned int idx;
                if( sscanf(entry->d_name, "cpu%u", &idx) == 1 )
                {
                    char path[128];
                    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/topology/core_id", idx);
                    struct stat st;
                    if( stat(path, &st) == 0 ) nLogical++; // 실제 온라인 코어만 카운트
                }
            }
            closedir(dir);
        }
        m_Cpu.m_nNumberCpus = nLogical;
    }

    // 물리 코어 수 - (physical_package_id, core_id) 조합의 고유 개수
    {
        DIR* dir = opendir("/sys/devices/system/cpu");
        std::vector<unsigned long long> seen; // (package<<32)|core 조합 저장
        if( dir )
        {
            struct dirent* entry;
            while( (entry = readdir(dir)) != nullptr )
            {
                unsigned int idx;
                if( sscanf(entry->d_name, "cpu%u", &idx) != 1 ) continue;

                char pkgPath[160], corePath[160];
                snprintf(pkgPath, sizeof(pkgPath), "/sys/devices/system/cpu/cpu%u/topology/physical_package_id", idx);
                snprintf(corePath, sizeof(corePath), "/sys/devices/system/cpu/cpu%u/topology/core_id", idx);

                unsigned long pkgId = 0, coreId = 0;
                std::string sPkg, sCore;
                if( !ReadFirstLine(pkgPath, sPkg) || !ReadFirstLine(corePath, sCore) ) continue;
                pkgId = strtoul(sPkg.c_str(), nullptr, 10);
                coreId = strtoul(sCore.c_str(), nullptr, 10);

                unsigned long long key = ((unsigned long long)pkgId << 32) | coreId;
                bool bDup = false;
                for( unsigned long long k : seen ) { if( k == key ) { bDup = true; break; } }
                if( !bDup ) seen.push_back(key);
            }
            closedir(dir);
        }
        m_Cpu.m_dwNumberOfCores = (DWORD)seen.size();
    }

    // L2/L3 캐시 크기 (KB) - /sys/devices/system/cpu/cpu0/cache/index{N}/{level,size}
    {
        for( int idx = 0; idx < 8; idx++ )
        {
            char levelPath[128], sizePath[128];
            snprintf(levelPath, sizeof(levelPath), "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
            snprintf(sizePath, sizeof(sizePath), "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);

            std::string sLevel, sSize;
            if( !ReadFirstLine(levelPath, sLevel) ) break; // index가 더 이상 없으면 종료
            if( !ReadFirstLine(sizePath, sSize) ) continue;

            int level = atoi(sLevel.c_str());
            unsigned int sizeKb = (unsigned int)atoi(sSize.c_str()); // "256K" 형태, atoi가 숫자부분만 읽음

            if( level == 2 ) m_Cpu.m_dwL2CacheSize = sizeKb;
            else if( level == 3 ) m_Cpu.m_dwL3CacheSize = sizeKb;
        }
    }

    // 클럭 속도 (MHz) - cpuinfo_max_freq(kHz) 우선, 없으면 /proc/cpuinfo의 "cpu MHz"로 폴백
    {
        std::string sFreq;
        if( ReadFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", sFreq) )
        {
            m_Cpu.m_nSpeed = strtoull(sFreq.c_str(), nullptr, 10) / 1000; // kHz -> MHz
        }
        else
        {
            std::ifstream f("/proc/cpuinfo");
            std::string line;
            while( std::getline(f, line) )
            {
                if( line.rfind("cpu MHz", 0) == 0 )
                {
                    size_t colon = line.find(':');
                    if( colon != std::string::npos )
                    {
                        m_Cpu.m_nSpeed = (unsigned __int64)atof(line.c_str() + colon + 1);
                    }
                    break;
                }
            }
        }
    }

    return TRUE;
#endif
}


//***************************************************************************
// CLinuxBiosInfo
//***************************************************************************

CLinuxBiosInfo::CLinuxBiosInfo()
{
}

CLinuxBiosInfo::~CLinuxBiosInfo()
{
}

//***************************************************************************
// @brief /sys/class/dmi/id/*를 읽어 BIOS 정보를 채웁니다.
// @return BOOL 하나 이상의 필드를 수집했으면 TRUE
//***************************************************************************
BOOL CLinuxBiosInfo::GetInformation()
{
    BOOL bAny = FALSE;

    if( ReadSysfsString("/sys/class/dmi/id/bios_vendor", m_Bios.m_tszManufacturer, _countof(m_Bios.m_tszManufacturer)) ) bAny = TRUE;
    if( ReadSysfsString("/sys/class/dmi/id/bios_version", m_Bios.m_tszVersion, _countof(m_Bios.m_tszVersion)) ) bAny = TRUE;
    if( ReadSysfsString("/sys/class/dmi/id/bios_date", m_Bios.m_tszReleaseDate, _countof(m_Bios.m_tszReleaseDate)) ) bAny = TRUE;
    // product_serial은 배포판에 따라 root 전용(0400)일 수 있음 - 실패해도 나머지는 유지
    if( ReadSysfsString("/sys/class/dmi/id/product_serial", m_Bios.m_tszSerialNumber, _countof(m_Bios.m_tszSerialNumber)) ) bAny = TRUE;

    return bAny;
}


//***************************************************************************
// CLinuxMainBoardInfo
//***************************************************************************

CLinuxMainBoardInfo::CLinuxMainBoardInfo()
{
}

CLinuxMainBoardInfo::~CLinuxMainBoardInfo()
{
}

//***************************************************************************
// @brief /sys/class/dmi/id/*를 읽어 메인보드 정보를 채웁니다.
// @return BOOL 하나 이상의 필드를 수집했으면 TRUE
//***************************************************************************
BOOL CLinuxMainBoardInfo::GetInformation()
{
    BOOL bAny = FALSE;

    if( ReadSysfsString("/sys/class/dmi/id/board_vendor", m_MainBoard.m_tszManufacturer, _countof(m_MainBoard.m_tszManufacturer)) ) bAny = TRUE;
    if( ReadSysfsString("/sys/class/dmi/id/board_name", m_MainBoard.m_tszProduct, _countof(m_MainBoard.m_tszProduct)) ) bAny = TRUE;
    if( ReadSysfsString("/sys/class/dmi/id/board_version", m_MainBoard.m_tszVersion, _countof(m_MainBoard.m_tszVersion)) ) bAny = TRUE;
    if( ReadSysfsString("/sys/class/dmi/id/board_serial", m_MainBoard.m_tszSerialNumber, _countof(m_MainBoard.m_tszSerialNumber)) ) bAny = TRUE;

    return bAny;
}


//***************************************************************************
// CLinuxMemoryInfo
//***************************************************************************

namespace
{
    //***************************************************************************
    // SMBIOS raw 테이블(구조체 배열) 파싱 헬퍼 - Windows GetSmbiosString64.asm의
    // get_smbios_string_instance_64/get_smbios_instance_count_64/get_smbios_word_64와
    // 완전히 동일한 알고리즘을 C++로 재구현한 것입니다. 손상된 데이터를 만나도 버퍼
    // 밖을 읽지 않도록 매 구조체마다 formattedLen이 버퍼 끝을 넘지 않는지 확인합니다.
    //***************************************************************************

    bool GetStringFromStructure(const unsigned char* pStruct, unsigned char formattedLen, int stringIndex, std::string& out)
    {
        if( stringIndex == 0 ) return false;
        const unsigned char* p = pStruct + formattedLen;
        int current = 1;
        while( *p != 0 )
        {
            size_t len = strlen((const char*)p);
            if( current == stringIndex )
            {
                out.assign((const char*)p, len);
                return true;
            }
            p += len + 1;
            current++;
        }
        return false;
    }

    const unsigned char* NextStructure(const unsigned char* pStruct, unsigned char formattedLen, const unsigned char* pEnd)
    {
        const unsigned char* p = pStruct + formattedLen;
        while( p + 1 < pEnd && !(p[0] == 0 && p[1] == 0) ) p++;
        return p + 2;
    }

    bool GetSmbiosStringInstanceLinux(const std::vector<unsigned char>& table, int type, int offset, int instance, std::string& out)
    {
        if( table.size() < 4 ) return false;
        const unsigned char* p = table.data();
        const unsigned char* pEnd = table.data() + table.size();
        int matchCount = 0;

        while( p + 4 <= pEnd )
        {
            unsigned char curType = p[0];
            unsigned char formattedLen = p[1];
            if( curType == 127 ) break;
            if( formattedLen < 4 || p + formattedLen > pEnd ) break;

            if( curType == type )
            {
                if( matchCount == instance )
                {
                    if( (unsigned char)offset >= formattedLen ) return false;
                    return GetStringFromStructure(p, formattedLen, p[offset], out);
                }
                matchCount++;
            }
            p = NextStructure(p, formattedLen, pEnd);
        }
        return false;
    }

    int GetSmbiosInstanceCountLinux(const std::vector<unsigned char>& table, int type)
    {
        if( table.size() < 4 ) return 0;
        const unsigned char* p = table.data();
        const unsigned char* pEnd = table.data() + table.size();
        int count = 0;

        while( p + 4 <= pEnd )
        {
            unsigned char curType = p[0];
            unsigned char formattedLen = p[1];
            if( curType == 127 ) break;
            if( formattedLen < 4 || p + formattedLen > pEnd ) break;
            if( curType == type ) count++;
            p = NextStructure(p, formattedLen, pEnd);
        }
        return count;
    }

    bool GetSmbiosWordLinux(const std::vector<unsigned char>& table, int type, int offset, int instance, unsigned short& out)
    {
        if( table.size() < 4 ) return false;
        const unsigned char* p = table.data();
        const unsigned char* pEnd = table.data() + table.size();
        int matchCount = 0;

        while( p + 4 <= pEnd )
        {
            unsigned char curType = p[0];
            unsigned char formattedLen = p[1];
            if( curType == 127 ) break;
            if( formattedLen < 4 || p + formattedLen > pEnd ) break;

            if( curType == type )
            {
                if( matchCount == instance )
                {
                    if( (unsigned char)(offset + 1) >= formattedLen ) return false;
                    unsigned short v;
                    memcpy(&v, p + offset, 2);
                    out = v;
                    return true;
                }
                matchCount++;
            }
            p = NextStructure(p, formattedLen, pEnd);
        }
        return false;
    }

    //***************************************************************************
    // @brief /sys/firmware/dmi/tables/DMI 전체를 메모리로 읽어옵니다.
    // @return 읽기 성공 시 바이트 배열(비어있지 않음), 실패(주로 권한 부족) 시 빈 배열
    //***************************************************************************
    std::vector<unsigned char> ReadDmiTable()
    {
        std::vector<unsigned char> data;
        std::ifstream f("/sys/firmware/dmi/tables/DMI", std::ios::binary);
        if( !f.is_open() ) return data;
        data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return data;
    }
}

CLinuxMemoryInfo::CLinuxMemoryInfo()
{
}

CLinuxMemoryInfo::~CLinuxMemoryInfo()
{
    for( HWINFO_RAM* pRam : m_sRamArray )
    {
        delete pRam;
    }
    m_sRamArray.clear();
}

//***************************************************************************
// @brief /proc/meminfo 및 /sys/firmware/dmi/tables/DMI를 통해 전체 메모리 상태
//        및 RAM 모듈 리스트를 수집합니다.
// @return BOOL 정보 수집 성공 여부
// @detail m_nTotalVirtualMemSize/m_nTotalPageFileSize를 둘 다 SwapTotal로 채우는
//         것은 Windows CSmbiosMemoryInfo가 ullTotalPageFile을 두 필드에 똑같이
//         쓰는 것과 동일한 단순화를 그대로 따른 것입니다(플랫폼 간 동작 일관성).
//***************************************************************************
BOOL CLinuxMemoryInfo::GetInformation()
{
    for( HWINFO_RAM* pRam : m_sRamArray )
    {
        delete pRam;
    }
    m_sRamArray.clear();

    BOOL bAny = FALSE;

    // 전체 메모리 통계 - /proc/meminfo (kB 단위)
    {
        std::unordered_map<std::string, unsigned __int64> meminfo;
        std::ifstream f("/proc/meminfo");
        std::string line;
        while( std::getline(f, line) )
        {
            size_t colon = line.find(':');
            if( colon == std::string::npos ) continue;
            std::string key = line.substr(0, colon);
            unsigned __int64 val = strtoull(line.c_str() + colon + 1, nullptr, 10); // kB
            meminfo[key] = val;
        }

        if( !meminfo.empty() )
        {
            auto get = [&](const char* k) -> unsigned __int64 {
                auto it = meminfo.find(k);
                return (it != meminfo.end()) ? it->second * 1024ULL : 0ULL;
            };

            m_Memory.m_nTotalMemSize = (__int64)get("MemTotal");
            m_Memory.m_nPhysicalMemSize = (__int64)get("MemAvailable"); // 없으면 0 - 아래 폴백
            if( m_Memory.m_nPhysicalMemSize == 0 )
                m_Memory.m_nPhysicalMemSize = (__int64)get("MemFree"); // 오래된 커널 폴백

            unsigned __int64 swapTotal = get("SwapTotal");
            unsigned __int64 swapFree = get("SwapFree");
            m_Memory.m_nTotalVirtualMemSize = (__int64)swapTotal;
            m_Memory.m_nFreeVirtualMemSize = (__int64)swapFree;
            m_Memory.m_nTotalPageFileSize = (__int64)swapTotal;
            m_Memory.m_nFreePageFileSize = (__int64)swapFree;

            bAny = TRUE;
        }
    }

    // 슬롯별 정보 - SMBIOS Type 17 (Memory Device), root 필요
    std::vector<unsigned char> dmiTable = ReadDmiTable();
    if( !dmiTable.empty() )
    {
        int count = GetSmbiosInstanceCountLinux(dmiTable, 17);
        for( int i = 0; i < count; i++ )
        {
            HWINFO_RAM* pRam = new HWINFO_RAM();

            std::string s;
            if( GetSmbiosStringInstanceLinux(dmiTable, 17, 0x10, i, s) ) // Device Locator
                CopyToTChar(pRam->m_tszDeviceLocator, _countof(pRam->m_tszDeviceLocator), s.c_str());
            if( GetSmbiosStringInstanceLinux(dmiTable, 17, 0x17, i, s) ) // Manufacturer
                CopyToTChar(pRam->m_tszManufacturer, _countof(pRam->m_tszManufacturer), s.c_str());

            unsigned short sizeRaw = 0, speedRaw = 0;
            if( GetSmbiosWordLinux(dmiTable, 17, 0x0C, i, sizeRaw) && sizeRaw != 0 && sizeRaw != 0x7FFF )
                pRam->m_nCapacity = (__int64)sizeRaw * 1024 * 1024;
            if( GetSmbiosWordLinux(dmiTable, 17, 0x15, i, speedRaw) )
                pRam->m_dwSpeed = speedRaw;

            m_sRamArray.push_back(pRam);
            bAny = TRUE;
        }
    }

    return bAny;
}


//***************************************************************************
// CLinuxDiskInfo
//***************************************************************************

CLinuxDiskInfo::CLinuxDiskInfo()
{
}

CLinuxDiskInfo::~CLinuxDiskInfo()
{
    for( HWINFO_HDDISK* pDisk : m_sHdDiskArray )
    {
        delete pDisk;
    }
    m_sHdDiskArray.clear();
}

//***************************************************************************
// @brief /sys/block/*를 순회하여 물리 디스크 정보를 수집합니다.
// @return BOOL 정보 수집 성공 여부
// @detail /sys/block에는 파티션이 아니라 디스크 전체 단위만 최상위 항목으로
//         나열되므로(파티션은 /sys/block/sda/sda1처럼 하위 디렉터리) 별도의
//         파티션 필터링이 필요 없습니다.
//***************************************************************************
BOOL CLinuxDiskInfo::GetInformation()
{
    for( HWINFO_HDDISK* pDisk : m_sHdDiskArray )
    {
        delete pDisk;
    }
    m_sHdDiskArray.clear();

    DIR* dir = opendir("/sys/block");
    if( !dir ) return FALSE;

    struct dirent* entry;
    while( (entry = readdir(dir)) != nullptr )
    {
        if( entry->d_name[0] == '.' ) continue;
        if( strncmp(entry->d_name, "loop", 4) == 0 ) continue; // loop 장치 제외
        if( strncmp(entry->d_name, "ram", 3) == 0 ) continue;  // ramdisk 제외

        char base[128];
        snprintf(base, sizeof(base), "/sys/block/%s", entry->d_name);

        HWINFO_HDDISK* pDisk = new HWINFO_HDDISK();

        char path[192];
        std::string vendor, model;
        snprintf(path, sizeof(path), "%s/device/vendor", base);
        ReadFirstLine(path, vendor);
        snprintf(path, sizeof(path), "%s/device/model", base);
        ReadFirstLine(path, model);

        std::string full = vendor;
        if( !full.empty() && !model.empty() ) full += " ";
        full += model;
        if( full.empty() ) full = entry->d_name; // 모델명을 못 읽으면 최소한 디바이스명이라도
        CopyToTChar(pDisk->m_tszModel, _countof(pDisk->m_tszModel), full.c_str());

        std::string serial;
        snprintf(path, sizeof(path), "%s/device/serial", base);
        if( ReadFirstLine(path, serial) )
            CopyToTChar(pDisk->m_tszSerialNumber, _countof(pDisk->m_tszSerialNumber), serial.c_str());
        else
            CopyToTChar(pDisk->m_tszSerialNumber, _countof(pDisk->m_tszSerialNumber), "N/A");

        // BusType 추정: 디바이스명 접두사 + device 심볼릭 링크 경로의 "usb" 포함 여부.
        // Windows IOCTL 버전(STORAGE_ADAPTER_DESCRIPTOR)만큼 정확하지 않은 휴리스틱입니다.
        const char* busName = "Unknown";
        if( strncmp(entry->d_name, "nvme", 4) == 0 )
        {
            busName = "NVMe";
        }
        else
        {
            char linkPath[192];
            snprintf(linkPath, sizeof(linkPath), "%s/device", base);
            char resolved[512] = { 0 };
            ssize_t len = readlink(linkPath, resolved, sizeof(resolved) - 1);
            if( len > 0 )
            {
                resolved[len] = 0;
                if( strstr(resolved, "usb") != nullptr ) busName = "USB";
                else busName = "SATA/SCSI";
            }
        }
        CopyToTChar(pDisk->m_tszBusType, _countof(pDisk->m_tszBusType), busName);

        unsigned __int64 sectors = 0;
        snprintf(path, sizeof(path), "%s/size", base);
        ReadSysfsUint64(path, sectors);
        pDisk->m_nTotalSize = (__int64)(sectors * 512ULL); // /sys/block/*/size 단위는 항상 512바이트 섹터

        m_sHdDiskArray.push_back(pDisk);
    }
    closedir(dir);

    return !m_sHdDiskArray.empty();
}


//***************************************************************************
// CLinuxPciInfo
//***************************************************************************

namespace
{
    struct PciVendorEntry { unsigned short id; const char* name; };

    // PciInfo64.asm/PciInfo86.asm의 벤더 테이블과 동일 (20개, 같은 순서)
    const PciVendorEntry g_PciVendorTable[] = {
        { 0x8086, "Intel" }, { 0x8087, "Intel" },
        { 0x1022, "AMD" }, { 0x1002, "AMD/ATI" },
        { 0x10DE, "NVIDIA" }, { 0x10EC, "Realtek" },
        { 0x14E4, "Broadcom" }, { 0x168C, "Qualcomm Atheros" },
        { 0x1B21, "ASMedia" }, { 0x1106, "VIA Technologies" },
        { 0x1039, "SiS" }, { 0x144D, "Samsung" },
        { 0x1344, "Micron" }, { 0x1C5C, "SK hynix" },
        { 0x1B4B, "Marvell" }, { 0x15AD, "VMware" },
        { 0x80EE, "Oracle VirtualBox" }, { 0x1414, "Microsoft (Hyper-V)" },
        { 0x1AB8, "Parallels" }, { 0x1179, "Toshiba" },
    };

    const char* LookupPciVendorName(unsigned short vendorId)
    {
        for( const auto& e : g_PciVendorTable )
        {
            if( e.id == vendorId ) return e.name;
        }
        return "Unknown";
    }

    //***************************************************************************
    // @brief PCI Class Code를 분석하여 디바이스 종류를 분류합니다.
    // @detail PciInfo64.asm/86.asm의 pci_classify_device와 동일한 판정 기준.
    //***************************************************************************
    PciDeviceClass ClassifyPciLinux(BYTE baseClass, BYTE subClass, BYTE progIf)
    {
        if( baseClass == 0x03 ) return PciDeviceClass::GPU;
        if( baseClass == 0x01 && subClass == 0x08 && progIf == 0x02 ) return PciDeviceClass::NVMe;
        return PciDeviceClass::Unknown;
    }
}

CLinuxPciInfo::CLinuxPciInfo()
{
}

CLinuxPciInfo::~CLinuxPciInfo()
{
    for( HWINFO_PCIDEVICE* pDev : m_sPciArray )
    {
        delete pDev;
    }
    m_sPciArray.clear();
}

//***************************************************************************
// @brief /sys/bus/pci/devices/*를 순회하여 PCI 장치 정보를 수집합니다.
// @return BOOL 정보 수집 성공 여부
// @detail 디렉터리명("0000:03:00.0" 형식)에서 Bus/Device/Function을 직접
//         파싱하고, class 파일(예: "0x030000")에서 Base/Sub/ProgIf를 바로 읽습니다
//         - Windows처럼 문자열 파싱을 거칠 필요가 없어 오히려 더 간단합니다.
//***************************************************************************
BOOL CLinuxPciInfo::GetInformation()
{
    for( HWINFO_PCIDEVICE* pDev : m_sPciArray )
    {
        delete pDev;
    }
    m_sPciArray.clear();

    DIR* dir = opendir("/sys/bus/pci/devices");
    if( !dir ) return FALSE;

    struct dirent* entry;
    while( (entry = readdir(dir)) != nullptr )
    {
        if( entry->d_name[0] == '.' ) continue;

        unsigned int domain, bus, device, function;
        if( sscanf(entry->d_name, "%x:%x:%x.%x", &domain, &bus, &device, &function) != 4 )
            continue; // 예상한 형식이 아니면 스킵

        char base[192];
        snprintf(base, sizeof(base), "/sys/bus/pci/devices/%s", entry->d_name);

        HWINFO_PCIDEVICE* pDev = new HWINFO_PCIDEVICE();
        pDev->m_byBus = (BYTE)bus;
        pDev->m_byDevice = (BYTE)device;
        pDev->m_byFunction = (BYTE)function;

        char path[224];
        unsigned long vendorId = 0, deviceId = 0, classCode = 0;

        snprintf(path, sizeof(path), "%s/vendor", base);
        ReadSysfsHex(path, vendorId);
        snprintf(path, sizeof(path), "%s/device", base);
        ReadSysfsHex(path, deviceId);
        snprintf(path, sizeof(path), "%s/class", base);
        ReadSysfsHex(path, classCode); // 예: 0x030000 (BaseClass:SubClass:ProgIf)

        pDev->m_wVendorId = (WORD)vendorId;
        pDev->m_wDeviceId = (WORD)deviceId;
        pDev->m_byBaseClass = (BYTE)((classCode >> 16) & 0xFF);
        pDev->m_bySubClass = (BYTE)((classCode >> 8) & 0xFF);
        pDev->m_byProgIf = (BYTE)(classCode & 0xFF);
        pDev->m_eType = ClassifyPciLinux(pDev->m_byBaseClass, pDev->m_bySubClass, pDev->m_byProgIf);

        CopyToTChar(pDev->m_tszVendorName, _countof(pDev->m_tszVendorName), LookupPciVendorName(pDev->m_wVendorId));
        // m_tszDescription: pci.ids 데이터베이스 파싱이 필요해 미구현 - 빈 문자열로 남김

        m_sPciArray.push_back(pDev);
    }
    closedir(dir);

    return !m_sPciArray.empty();
}
