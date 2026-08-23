//***************************************************************************
// NativeHwInfo.cpp : non-WMI 하드웨어 정보 클래스 전용 테스트 콘솔 프로그램.
//
// CWmi/CoInitializeEx 등 COM/WMI 초기화를 전혀 하지 않습니다 - 이 프로그램에서
// 다루는 14개 클래스는 전부 WMI 없이 CPUID/SMBIOS/IOCTL/SetupAPI만으로 동작하는
// 것이 요점이라, WMI 서비스가 죽어있는 환경을 흉내내는 셈입니다.
//
// 대상 클래스:
//   CpuInfo.h           - CCpuInfo (CPUID)
//   SmbiosHardwareInfo.h - CSmbiosBiosInfo, CSmbiosMainBoardInfo,
//                          CSmbiosMemoryInfo, CHdDiskInfo (SMBIOS/IOCTL)
//   DeviceInfo.h         - CDriveInfo, CVideoCardInfo, CSoundCardInfo,
//                          CNetworkCardInfo, CCdromInfo, CKeyBoardInfo,
//                          CMouseInfo, CMonitorInfo (SetupAPI/WinAPI)
//   PciInfo.h            - CPciInfo (SetupAPI + PciInfo64.asm/86.asm)
//
// pch.h가 이미 System/CpuInfo.h, System/PciInfo.h를 포함하지만(README 4.1절),
// System/DeviceInfo.h, System/SmbiosHardwareInfo.h는 SystemInfoTool.cpp가 안 써서
// pch.h에 없습니다 - 이 파일에서 직접 include합니다. 실제 프로젝트의 폴더 배치가
// 다르면 include 경로를 맞춰 조정해 주세요.
//
// SystemInfoTool.cpp와 동일하게, _DEBUG 빌드에서는 함수(=섹션)마다 끝에
// PauseConsole()/ClearConsoleScreen()을 실행합니다. 실패([FAIL])해도 그 결과를
// 읽을 시간이 필요하니, 함수 중간에 return으로 빠지는 대신 끝까지 흐른 뒤
// 마지막에 한 번만 pause/clear 하도록 각 함수를 구성했습니다.
//***************************************************************************

#include <pch.h>

#include <System/DeviceInfo.h>
#include <System/SmbiosHardwareInfo.h>

#include <iostream>

//***************************************************************************
// @brief 클래스 이름과 GetInformation() 반환값을 [OK]/[FAIL] 형태로 출력합니다.
//***************************************************************************
static void PrintResult(const TCHAR* pszClassName, BOOL bResult)
{
    _tprintf(_T("%-24s : %s\n"), pszClassName, bResult ? _T("[OK]") : _T("[FAIL]"));
}

static void PrintSeparator(const TCHAR* pszTitle)
{
    _tcout << _T("===========================================================================\n");
    _tcout << _T("== ") << pszTitle << _T("\n");
    _tcout << _T("===========================================================================\n\n");
}

// 바이트 값을 GB 단위 문자열로 간단 변환 (WmiHardwareInfo.h의 ChangeDataFormat은
// WMI 헤더 체인을 끌고 오므로, 이 non-WMI 전용 도구에서는 쓰지 않고 직접 계산합니다)
static double ToGB(__int64 nBytes)
{
    return (double)nBytes / (1024.0 * 1024.0 * 1024.0);
}

//***************************************************************************
// @brief 각 Test*() 함수 끝에서 호출 - _DEBUG 빌드에서만 일시정지 + 화면 클리어.
//***************************************************************************
static void PauseAndClear()
{
    _tprintf(_T("\n"));
    PauseConsole();
    ClearConsoleScreen();
}

//***************************************************************************
// 1. CCpuInfo (CpuInfo.h)
//***************************************************************************
static void TestCpuInfo()
{
    PrintSeparator(_T("CCpuInfo (CPUID)"));

    CCpuInfo CpuInfo;
    BOOL bResult = CpuInfo.GetInformation();
    PrintResult(_T("CCpuInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  VendorName        = %s\n"), CpuInfo.GetVendorName());
        _tprintf(_T("  ProcessorName     = %s\n"), CpuInfo.GetProcessorName());
        _tprintf(_T("  ProcessorId       = %s\n"), CpuInfo.GetProcessorId());
        _tprintf(_T("  Speed             = %u MHz\n"), CpuInfo.GetSpeedMHz());
        _tprintf(_T("  Processors(Logic) = %d\n"), CpuInfo.GetNumberOfProcessors());
        _tprintf(_T("  Processors(Core)  = %u\n"), CpuInfo.GetNumberOfCores());
        _tprintf(_T("  Family/Model/Step = %d / %d / %d\n"), CpuInfo.GetCPUFamily(), CpuInfo.GetCPUModel(), CpuInfo.GetCPUStepping());
        _tprintf(_T("  L2/L3 Cache       = %u KB / %u KB\n"), CpuInfo.GetL2CacheSize(), CpuInfo.GetL3CacheSize());
        _tprintf(_T("  MMX/SSE/SSE2/3DN  = %s / %s / %s / %s\n"),
            CpuInfo.IsMMXSupported() ? _T("Y") : _T("N"),
            CpuInfo.IsSSESupported() ? _T("Y") : _T("N"),
            CpuInfo.IsSSE2Supported() ? _T("Y") : _T("N"),
            CpuInfo.Is3DNowSupported() ? _T("Y") : _T("N"));
    }

    PauseAndClear();
}

//***************************************************************************
// 2. CSmbiosBiosInfo (SmbiosHardwareInfo.h)
//***************************************************************************
static void TestSmbiosBiosInfo()
{
    PrintSeparator(_T("CSmbiosBiosInfo (SMBIOS Type0/Type1)"));

    CSmbiosBiosInfo BiosInfo;
    BOOL bResult = BiosInfo.GetInformation();
    PrintResult(_T("CSmbiosBiosInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  Manufacturer   = %s\n"), BiosInfo.GetManufacturer());
        _tprintf(_T("  Version        = %s\n"), BiosInfo.GetVersion());
        _tprintf(_T("  ReleaseDate    = %s\n"), BiosInfo.GetReleaseDate());
        _tprintf(_T("  SerialNumber   = %s\n"), BiosInfo.GetSerialNumber());
    }

    PauseAndClear();
}

//***************************************************************************
// 3. CSmbiosMainBoardInfo (SmbiosHardwareInfo.h)
//***************************************************************************
static void TestSmbiosMainBoardInfo()
{
    PrintSeparator(_T("CSmbiosMainBoardInfo (SMBIOS Type2)"));

    CSmbiosMainBoardInfo MainBoardInfo;
    BOOL bResult = MainBoardInfo.GetInformation();
    PrintResult(_T("CSmbiosMainBoardInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  Manufacturer   = %s\n"), MainBoardInfo.GetManufacturer());
        _tprintf(_T("  Product        = %s\n"), MainBoardInfo.GetProduct());
        _tprintf(_T("  Version        = %s\n"), MainBoardInfo.GetVersion());
        _tprintf(_T("  SerialNumber   = %s\n"), MainBoardInfo.GetSerialNumber());
    }

    PauseAndClear();
}

//***************************************************************************
// 4. CSmbiosMemoryInfo (SmbiosHardwareInfo.h)
//***************************************************************************
static void TestSmbiosMemoryInfo()
{
    PrintSeparator(_T("CSmbiosMemoryInfo (GlobalMemoryStatusEx + SMBIOS Type17)"));

    CSmbiosMemoryInfo MemoryInfo;
    BOOL bResult = MemoryInfo.GetInformation();
    PrintResult(_T("CSmbiosMemoryInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  RamCount          = %u\n"), MemoryInfo.GetRamCount());
        _tprintf(_T("  TotalMemSize      = %.2f GB\n"), ToGB(MemoryInfo.GetTotalMemSize()));
        _tprintf(_T("  PhysicalMemSize   = %.2f GB\n"), ToGB(MemoryInfo.GetPhysicalMemSize()));
        _tprintf(_T("  UsedMemSize       = %.2f GB (%.1f%%)\n"), ToGB(MemoryInfo.GetUseMemSize()), MemoryInfo.GetPercentUsedRam() * 100.0);

        const std::vector<HWINFO_RAM*>* psRamArray = MemoryInfo.GetRamArray();
        if( psRamArray )
        {
            for( size_t i = 0; i < psRamArray->size(); ++i )
            {
                HWINFO_RAM* pRam = (*psRamArray)[i];
                _tprintf(_T("  RAM[%zu] %s / %s / %I64d MB / %u MT/s\n"),
                    i + 1, pRam->m_tszDeviceLocator, pRam->m_tszManufacturer,
                    pRam->m_nCapacity / (1024 * 1024), pRam->m_dwSpeed);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 5. CHdDiskInfo (SmbiosHardwareInfo.h) - 관리자 권한 필요
//***************************************************************************
static void TestHdDiskInfo()
{
    PrintSeparator(_T("CHdDiskInfo (IOCTL_STORAGE_QUERY_PROPERTY, 관리자 권한 필요)"));

    CHdDiskInfo HdDiskInfo;
    BOOL bResult = HdDiskInfo.GetInformation();
    PrintResult(_T("CHdDiskInfo"), bResult);
    if( bResult )
    {
        const std::vector<HWINFO_HDDISK*>* psHdDiskArray = HdDiskInfo.GetHdDiskArray();
        if( psHdDiskArray )
        {
            for( size_t i = 0; i < psHdDiskArray->size(); ++i )
            {
                HWINFO_HDDISK* pDisk = (*psHdDiskArray)[i];
                _tprintf(_T("  Disk[%zu] %s (%s) %.2f GB S/N=%s\n"),
                    i + 1, pDisk->m_tszModel, pDisk->m_tszBusType, ToGB(pDisk->m_nTotalSize), pDisk->m_tszSerialNumber);
            }
        }
    }
    else
    {
        _tprintf(_T("  (관리자 권한으로 실행했는지 확인하세요 - CreateFileA(\\\\.\\PhysicalDriveN)가\n"));
        _tprintf(_T("   비관리자 프로세스에서는 항상 실패합니다)\n"));
    }

    PauseAndClear();
}

//***************************************************************************
// 6. CDriveInfo (DeviceInfo.h)
//***************************************************************************
static void TestDriveInfo()
{
    PrintSeparator(_T("CDriveInfo (GetLogicalDrives + GetDiskFreeSpaceEx)"));

    CDriveInfo DriveInfo;
    BOOL bResult = DriveInfo.GetInformation();
    PrintResult(_T("CDriveInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  DriveCount    = %u\n"), DriveInfo.GetDriveCount());
        _tprintf(_T("  TotalSpace    = %.2f GB\n"), ToGB(DriveInfo.GetTotalSpaceSize()));
        _tprintf(_T("  FreeSpace     = %.2f GB\n"), ToGB(DriveInfo.GetFreeSpaceSize()));

        const std::vector<HWINFO_DRIVE*>* psDriveArray = DriveInfo.GetDriveArray();
        if( psDriveArray )
        {
            for( size_t i = 0; i < psDriveArray->size(); ++i )
            {
                HWINFO_DRIVE* pDrive = (*psDriveArray)[i];
                _tprintf(_T("  Drive[%zu] %s (%s) %.2f GB free / %.2f GB\n"),
                    i + 1, pDrive->m_tszName, pDrive->m_tszFileSystem, ToGB(pDrive->m_nFreeSpace), ToGB(pDrive->m_nTotalSpace));
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 7-1. CVideoCardInfo (DeviceInfo.h)
//***************************************************************************
static void TestVideoCardInfo()
{
    PrintSeparator(_T("CVideoCardInfo (SetupAPI/EnumDisplayDevices)"));

    CVideoCardInfo VideoCardInfo;
    BOOL bVideoResult = VideoCardInfo.GetInformation();
    PrintResult(_T("CVideoCardInfo"), bVideoResult);
    if( bVideoResult )
    {
        const std::vector<HWINFO_VIDEOCARD*>* psArray = VideoCardInfo.GetVideoCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_VIDEOCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Video[%zu] %s (%s) VRAM=%ld MB\n"), i + 1, pCard->m_tszDescription, pCard->m_tszManufacturer, pCard->m_lMemorySize);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 7-2. CSoundCardInfo (DeviceInfo.h)
//***************************************************************************
static void TestSoundCardInfo()
{
    PrintSeparator(_T("CSoundCardInfo (SetupAPI/waveOutGetDevCaps)"));

    CSoundCardInfo SoundCardInfo;
    BOOL bSoundResult = SoundCardInfo.GetInformation();
    PrintResult(_T("CSoundCardInfo"), bSoundResult);
    if( bSoundResult )
    {
        const std::vector<HWINFO_SOUNDCARD*>* psArray = SoundCardInfo.GetSoundCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_SOUNDCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Sound[%zu] %s (%s)\n"), i + 1, pCard->m_tszProductName, pCard->m_tszCompanyName);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 7-3. CNetworkCardInfo (DeviceInfo.h)
//***************************************************************************
static void TestNetworkCardInfo()
{
    PrintSeparator(_T("CNetworkCardInfo (IP Helper API / GetAdaptersAddresses)"));

    CNetworkCardInfo NetworkCardInfo;
    BOOL bNetResult = NetworkCardInfo.GetInformation();
    PrintResult(_T("CNetworkCardInfo"), bNetResult);
    if( bNetResult )
    {
        const std::vector<HWINFO_NETWORKCARD*>* psArray = NetworkCardInfo.GetNetworkCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_NETWORKCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Network[%zu] %s [%s]\n"), i + 1, pCard->m_tszDescription, pCard->m_tszHardwareId);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-1. CCdromInfo (DeviceInfo.h)
//***************************************************************************
static void TestCdromInfo()
{
    PrintSeparator(_T("CCdromInfo (SetupAPI)"));

    CCdromInfo CdromInfo;
    BOOL bCdromResult = CdromInfo.GetInformation();
    PrintResult(_T("CCdromInfo"), bCdromResult);
    if( bCdromResult )
    {
        const std::vector<HWINFO_CDROM*>* psArray = CdromInfo.GetCdromArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_CDROM* pItem = (*psArray)[i];
                _tprintf(_T("  Cdrom[%zu] %s (%s)\n"), i + 1, pItem->m_tszDescription, pItem->m_tszManufacturer);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-2. CKeyBoardInfo (DeviceInfo.h)
//***************************************************************************
static void TestKeyboardInfo()
{
    PrintSeparator(_T("CKeyBoardInfo (SetupAPI)"));

    CKeyBoardInfo KeyBoardInfo;
    BOOL bKbResult = KeyBoardInfo.GetInformation();
    PrintResult(_T("CKeyBoardInfo"), bKbResult);
    if( bKbResult )
    {
        const std::vector<HWINFO_KEYBOARD*>* psArray = KeyBoardInfo.GetKeyBoardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_KEYBOARD* pItem = (*psArray)[i];
                _tprintf(_T("  Keyboard[%zu] %s\n"), i + 1, pItem->m_tszDescription);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-3. CMouseInfo (DeviceInfo.h)
//***************************************************************************
static void TestMouseInfo()
{
    PrintSeparator(_T("CMouseInfo (SetupAPI)"));

    CMouseInfo MouseInfo;
    BOOL bMouseResult = MouseInfo.GetInformation();
    PrintResult(_T("CMouseInfo"), bMouseResult);
    if( bMouseResult )
    {
        const std::vector<HWINFO_MOUSE*>* psArray = MouseInfo.GetMouseArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_MOUSE* pItem = (*psArray)[i];
                _tprintf(_T("  Mouse[%zu] %s (%s)\n"), i + 1, pItem->m_tszDescription, pItem->m_tszManufacturer);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-4. CMonitorInfo (DeviceInfo.h)
//***************************************************************************
static void TestMonitorInfo()
{
    PrintSeparator(_T("CMonitorInfo (SetupAPI)"));

    CMonitorInfo MonitorInfo;
    BOOL bMonitorResult = MonitorInfo.GetInformation();
    PrintResult(_T("CMonitorInfo"), bMonitorResult);
    if( bMonitorResult )
    {
        const std::vector<HWINFO_MONITOR*>* psArray = MonitorInfo.GetMonitorArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_MONITOR* pItem = (*psArray)[i];
                _tprintf(_T("  Monitor[%zu] %s (%s)\n"), i + 1, pItem->m_tszDescription, pItem->m_tszManufacturer);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 9. CPciInfo (PciInfo.h)
//***************************************************************************
static void TestPciInfo()
{
    PrintSeparator(_T("CPciInfo (SetupAPI + PciInfo64.asm/86.asm)"));

    CPciInfo PciInfo;
    BOOL bResult = PciInfo.GetInformation();
    PrintResult(_T("CPciInfo"), bResult);
    if( bResult )
    {
        const std::vector<HWINFO_PCIDEVICE*>* psArray = PciInfo.GetPciDeviceArray();
        if( psArray )
        {
            _tprintf(_T("  총 %zu개 장치 (전부 출력하면 너무 길어서 앞 10개만 표시)\n"), psArray->size());
            for( size_t i = 0; i < psArray->size() && i < 10; ++i )
            {
                HWINFO_PCIDEVICE* pDev = (*psArray)[i];
                const TCHAR* pszType = (pDev->m_eType == PciDeviceClass::GPU) ? _T("GPU")
                    : (pDev->m_eType == PciDeviceClass::NVMe) ? _T("NVMe") : _T("");
                _tprintf(_T("  PCI[%zu] %02X:%02X.%X VEN_%04X&DEV_%04X (%s) %s - %s\n"),
                    i + 1, pDev->m_byBus, pDev->m_byDevice, pDev->m_byFunction,
                    pDev->m_wVendorId, pDev->m_wDeviceId, pDev->m_tszVendorName, pszType, pDev->m_tszDescription);
            }

            // GPU/NVMe로 분류된 장치 수만 따로 카운트 (분류 로직 자체가 잘 동작하는지 빠르게 확인용)
            int nGpuCount = 0, nNvmeCount = 0;
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                if( (*psArray)[i]->m_eType == PciDeviceClass::GPU ) nGpuCount++;
                else if( (*psArray)[i]->m_eType == PciDeviceClass::NVMe ) nNvmeCount++;
            }
            _tprintf(_T("  분류 결과: GPU %d개, NVMe %d개\n"), nGpuCount, nNvmeCount);
        }
    }

    PauseAndClear();
}

//***************************************************************************
//
int main()
{
    InitUtf8Console();

    _tprintf(_T("***************************************************************************\n"));
    _tprintf(_T("\tnon-WMI 하드웨어 정보 클래스 테스트 (CWmi/COM 초기화 없음)\n"));
    _tprintf(_T("***************************************************************************\n\n"));

    TestCpuInfo();
    TestSmbiosBiosInfo();
    TestSmbiosMainBoardInfo();
    TestSmbiosMemoryInfo();
    TestHdDiskInfo();
    TestDriveInfo();
    TestVideoCardInfo();
    TestSoundCardInfo();
    TestNetworkCardInfo();
    TestCdromInfo();
    TestKeyboardInfo();
    TestMouseInfo();
    TestMonitorInfo();
    TestPciInfo();

    _tprintf(_T("\n***************************************************************************\n"));
    _tprintf(_T("테스트 완료. [FAIL]이 있으면 해당 클래스의 @details 주석을 확인하세요\n"));
    _tprintf(_T("(예: CHdDiskInfo는 관리자 권한 필요, CVideoCardInfo 등 일부 필드는 원래 미구현).\n"));

#ifdef _DEBUG
    PauseConsole();
#endif

    return 0;
}