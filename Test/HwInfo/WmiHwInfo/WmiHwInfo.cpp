//***************************************************************************
// WmiHwInfo.cpp : WMI 기반 하드웨어 정보 클래스 전용 테스트 콘솔 프로그램.
//
// NativeHwInfo.cpp의 짝입니다 - 저쪽은 CWmi/COM을 전혀 안 쓰는 게 요점이었다면,
// 이쪽은 System/WmiHardwareInfo.h의 CWmi* 클래스 14개(CWmiProcessorInfo 포함 -
// SystemInfoTool.cpp는 안 쓰지만 라이브러리엔 있는 클래스)를 전부 실제로 호출해서
// WMI 경로가 정상 동작하는지 확인합니다.
//
// SystemInfoTool.cpp와 달리 winmgmt 서비스 자동 시작 설정, 로그 파일 저장은
// 하지 않습니다 - 빠르게 콘솔에서 결과만 확인하는 용도의 최소 구성입니다.
// WMI 서비스가 멈춰 있으면 Wmi.Connect()에서 바로 실패로 드러납니다.
//
// NativeHwInfo.cpp와 동일하게, _DEBUG 빌드에서는 함수(=섹션)마다 끝에
// PauseConsole()/ClearConsoleScreen()을 실행합니다. 실패([FAIL])해도 그 결과를
// 읽을 시간이 필요하니, 함수 중간에 return으로 빠지는 대신 끝까지 흐른 뒤
// 마지막에 한 번만 pause/clear 하도록 각 함수를 구성했습니다.
//***************************************************************************

#include <pch.h>

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

// NativeHwInfo.cpp와 동일한 이유로 ChangeDataFormat() 대신 직접 계산합니다.
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
// 1. CWmiProcessorInfo
//***************************************************************************
static void TestWmiProcessorInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiProcessorInfo (Win32_Processor)"));

    CWmiProcessorInfo ProcessorInfo;
    BOOL bResult = ProcessorInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiProcessorInfo"), bResult);
    if( bResult )
    {
        const std::vector<HWINFO_CPU*>* psArray = ProcessorInfo.GetProcessorArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_CPU* pCpu = (*psArray)[i];
                _tprintf(_T("  VendorName        = %s\n"), pCpu->m_tszVendorName);
                _tprintf(_T("  ProcessorName     = %s\n"), pCpu->m_tszProcessorName);
                _tprintf(_T("  ProcessorId       = %s\n"), pCpu->m_tszProcessorId);
                _tprintf(_T("  Speed             = %I64u MHz\n"), pCpu->m_nSpeed);
                _tprintf(_T("  Processors(Logic) = %u\n"), pCpu->m_nNumberCpus);
                _tprintf(_T("  Processors(Core)  = %u\n"), pCpu->m_dwNumberOfCores);
                _tprintf(_T("  Family/Model/Step = %d / %d / %d\n"), pCpu->m_nFamily, pCpu->m_nModel, pCpu->m_nStepping);
                _tprintf(_T("  L2/L3 Cache       = %u KB / %u KB\n"), pCpu->m_dwL2CacheSize, pCpu->m_dwL3CacheSize);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 2-1. CWmiBiosInfo
//***************************************************************************
static void TestWmiBiosInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiBiosInfo (Win32_BIOS)"));

    CWmiBiosInfo BiosInfo;
    BOOL bBiosResult = BiosInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiBiosInfo"), bBiosResult);
    if( bBiosResult )
    {
        _tprintf(_T("  Manufacturer      = %s\n"), BiosInfo.GetManufacturer());
        _tprintf(_T("  SmVersion         = %s\n"), BiosInfo.GetSmVersion());
        _tprintf(_T("  Version           = %s\n"), BiosInfo.GetVersion());
        _tprintf(_T("  IdentificationCode= %s (WMI 자체가 보통 비워두는 필드 - README 3.9절 참고)\n"), BiosInfo.GetIdentificationCode());
        _tprintf(_T("  SerialNumber      = %s\n"), BiosInfo.GetSerialNumber());
        _tprintf(_T("  ReleaseDate       = %s\n"), BiosInfo.GetReleaseDate());
    }

    PauseAndClear();
}

//***************************************************************************
// 2-2. CWmiMainBoardInfo
//***************************************************************************
static void TestWmiMainBoardInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiMainBoardInfo (Win32_BaseBoard)"));

    CWmiMainBoardInfo MainBoardInfo;
    BOOL bMbResult = MainBoardInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiMainBoardInfo"), bMbResult);
    if( bMbResult )
    {
        _tprintf(_T("  Description   = %s\n"), MainBoardInfo.GetDescription());
        _tprintf(_T("  Manufacturer  = %s\n"), MainBoardInfo.GetManufacturer());
        _tprintf(_T("  Product       = %s\n"), MainBoardInfo.GetProduct());
        _tprintf(_T("  SerialNumber  = %s\n"), MainBoardInfo.GetSerialNumber());
    }

    PauseAndClear();
}

//***************************************************************************
// 3. CWmiMemoryInfo
//***************************************************************************
static void TestWmiMemoryInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiMemoryInfo (Win32_PhysicalMemory + Win32_OperatingSystem)"));

    CWmiMemoryInfo MemoryInfo;
    BOOL bResult = MemoryInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiMemoryInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  RamCount          = %u\n"), MemoryInfo.GetRamCount());
        _tprintf(_T("  TotalMemSize      = %.2f GB\n"), ToGB(MemoryInfo.GetTotalMemSize()));
        _tprintf(_T("  PhysicalMemSize   = %.2f GB\n"), ToGB(MemoryInfo.GetPhysicalMemSize()));
        _tprintf(_T("  UsedMemSize       = %.2f GB (%.1f%%)\n"), ToGB(MemoryInfo.GetUseMemSize()), MemoryInfo.GetPercentUsedRam() * 100.0);
        _tprintf(_T("  TotalVirtualMem   = %.2f GB\n"), ToGB(MemoryInfo.GetTotalVirtualMemSize()));
        _tprintf(_T("  TotalPageFile     = %.2f GB\n"), ToGB(MemoryInfo.GetTotalPageFile()));

        const std::vector<HWINFO_RAM*>* psRamArray = MemoryInfo.GetRamArray();
        if( psRamArray )
        {
            for( size_t i = 0; i < psRamArray->size(); ++i )
            {
                HWINFO_RAM* pRam = (*psRamArray)[i];
                _tprintf(_T("  RAM[%zu] %s / %s / %I64d MB / %u MHz [%s]\n"),
                    i + 1, pRam->m_tszDeviceLocator, pRam->m_tszBankLabel,
                    pRam->m_nCapacity / (1024 * 1024), pRam->m_dwSpeed, pRam->m_tszMemoryTypeDesc);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 4. CWmiHdDiskInfo
//***************************************************************************
static void TestWmiHdDiskInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiHdDiskInfo (Win32_DiskDrive)"));

    CWmiHdDiskInfo HdDiskInfo;
    BOOL bResult = HdDiskInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiHdDiskInfo"), bResult);
    if( bResult )
    {
        const std::vector<HWINFO_HDDISK*>* psArray = HdDiskInfo.GetHdDiskArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_HDDISK* pDisk = (*psArray)[i];
                _tprintf(_T("  Disk[%zu] %s (%s) %.2f GB S/N=%s\n"),
                    i + 1, pDisk->m_tszModel, pDisk->m_tszBusType, ToGB(pDisk->m_nTotalSize), pDisk->m_tszSerialNumber);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 5. CWmiDriveInfo
//***************************************************************************
static void TestWmiDriveInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiDriveInfo (Win32_LogicalDisk)"));

    CWmiDriveInfo DriveInfo;
    BOOL bResult = DriveInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiDriveInfo"), bResult);
    if( bResult )
    {
        _tprintf(_T("  DriveCount    = %u\n"), DriveInfo.GetDriveCount());
        _tprintf(_T("  TotalSpace    = %.2f GB\n"), ToGB(DriveInfo.GetTotalSpaceSize()));
        _tprintf(_T("  FreeSpace     = %.2f GB\n"), ToGB(DriveInfo.GetFreeSpaceSize()));

        const std::vector<HWINFO_DRIVE*>* psArray = DriveInfo.GetDriveArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_DRIVE* pDrive = (*psArray)[i];
                _tprintf(_T("  Drive[%zu] %s (%s) %.2f GB free / %.2f GB\n"),
                    i + 1, pDrive->m_tszName, pDrive->m_tszFileSystem, ToGB(pDrive->m_nFreeSpace), ToGB(pDrive->m_nTotalSpace));
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 6-1. CWmiVideoCardInfo
//***************************************************************************
static void TestWmiVideoCardInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiVideoCardInfo (Win32_VideoController)"));

    CWmiVideoCardInfo VideoCardInfo;
    BOOL bVideoResult = VideoCardInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiVideoCardInfo"), bVideoResult);
    if( bVideoResult )
    {
        const std::vector<HWINFO_VIDEOCARD*>* psArray = VideoCardInfo.GetVideoCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_VIDEOCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Video[%zu] %s (%s) ChipType=%s VRAM=%ld MB\n"),
                    i + 1, pCard->m_tszDescription, pCard->m_tszAdapterString, pCard->m_tszChipType, pCard->m_lMemorySize);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 6-2. CWmiSoundCardInfo
//***************************************************************************
static void TestWmiSoundCardInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiSoundCardInfo (Win32_SoundDevice)"));

    CWmiSoundCardInfo SoundCardInfo;
    BOOL bSoundResult = SoundCardInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiSoundCardInfo"), bSoundResult);
    if( bSoundResult )
    {
        const std::vector<HWINFO_SOUNDCARD*>* psArray = SoundCardInfo.GetSoundCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_SOUNDCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Sound[%zu] %s (%s) VolCtrl=%s\n"),
                    i + 1, pCard->m_tszProductName, pCard->m_tszCompanyName, pCard->m_bHasVolCtrl ? _T("Y") : _T("N"));
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 6-3. CWmiNetworkCardInfo
//***************************************************************************
static void TestWmiNetworkCardInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiNetworkCardInfo (Win32_NetworkAdapter)"));

    CWmiNetworkCardInfo NetworkCardInfo;
    BOOL bNetResult = NetworkCardInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiNetworkCardInfo"), bNetResult);
    if( bNetResult )
    {
        const std::vector<HWINFO_NETWORKCARD*>* psArray = NetworkCardInfo.GetNetworkCardArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_NETWORKCARD* pCard = (*psArray)[i];
                _tprintf(_T("  Network[%zu] %s\n"), i + 1, pCard->m_tszDescription);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 7. CWmiPciInfo
//***************************************************************************
static void TestWmiPciInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiPciInfo (Win32_PnPEntity)"));

    CWmiPciInfo PciInfo;
    BOOL bResult = PciInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiPciInfo"), bResult);
    if( bResult )
    {
        const std::vector<HWINFO_PCIDEVICE*>* psArray = PciInfo.GetPciDeviceArray();
        if( psArray )
        {
            _tprintf(_T("  총 %zu개 장치 (전부 출력하면 너무 길어서 앞 10개만 표시)\n"), psArray->size());
            _tprintf(_T("  Bus/Device/Function/ClassCode는 WMI에 대응 속성이 없어 항상 0/Unknown입니다 (README 3.9절 ⑩ 참고)\n"));
            for( size_t i = 0; i < psArray->size() && i < 10; ++i )
            {
                HWINFO_PCIDEVICE* pDev = (*psArray)[i];
                _tprintf(_T("  PCI[%zu] VEN_%04X&DEV_%04X (%s) - %s\n"),
                    i + 1, pDev->m_wVendorId, pDev->m_wDeviceId, pDev->m_tszVendorName, pDev->m_tszDescription);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-1. CWmiCdromInfo
//***************************************************************************
static void TestWmiCdromInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiCdromInfo (Win32_CDROMDrive)"));

    CWmiCdromInfo CdromInfo;
    BOOL bCdromResult = CdromInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiCdromInfo"), bCdromResult);
    if( bCdromResult )
    {
        const std::vector<HWINFO_CDROM*>* psArray = CdromInfo.GetCdromArray();
        if( psArray )
        {
            for( size_t i = 0; i < psArray->size(); ++i )
            {
                HWINFO_CDROM* pItem = (*psArray)[i];
                _tprintf(_T("  Cdrom[%zu] %s (%s)\n"), i + 1, pItem->m_tszName, pItem->m_tszManufacturer);
            }
        }
    }

    PauseAndClear();
}

//***************************************************************************
// 8-2. CWmiKeyBoardInfo
//***************************************************************************
static void TestWmiKeyboardInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiKeyBoardInfo (Win32_Keyboard)"));

    CWmiKeyBoardInfo KeyBoardInfo;
    BOOL bKbResult = KeyBoardInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiKeyBoardInfo"), bKbResult);
    if( bKbResult )
    {
        _tprintf(_T("  Description = %s\n"), KeyBoardInfo.GetDescription());
        _tprintf(_T("  Type        = %s\n"), KeyBoardInfo.GetType());
    }

    PauseAndClear();
}

//***************************************************************************
// 8-3. CWmiMouseInfo
//***************************************************************************
static void TestWmiMouseInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiMouseInfo (Win32_PointingDevice)"));

    CWmiMouseInfo MouseInfo;
    BOOL bMouseResult = MouseInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiMouseInfo"), bMouseResult);
    if( bMouseResult )
    {
        _tprintf(_T("  Name        = %s\n"), MouseInfo.GetName());
        _tprintf(_T("  Manufacturer = %s\n"), MouseInfo.GetManufacturer());
        _tprintf(_T("  Description = %s\n"), MouseInfo.GetDescription());
    }

    PauseAndClear();
}

//***************************************************************************
// 8-4. CWmiMonitorInfo
//***************************************************************************
static void TestWmiMonitorInfo(CWmi& Wmi)
{
    PrintSeparator(_T("CWmiMonitorInfo (Win32_DesktopMonitor)"));

    CWmiMonitorInfo MonitorInfo;
    BOOL bMonitorResult = MonitorInfo.GetInformation(Wmi);
    PrintResult(_T("CWmiMonitorInfo"), bMonitorResult);
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
//
int main()
{
    InitUtf8Console();

    _tprintf(_T("***************************************************************************\n"));
    _tprintf(_T("\t\t\tWMI 하드웨어 정보 클래스 테스트\n"));
    _tprintf(_T("***************************************************************************\n\n"));

    // COM 라이브러리 초기화 - SystemInfoTool.cpp 3.6절과 동일한 최소 절차.
    // winmgmt 서비스 자동 시작 설정/로그 파일 저장은 이 테스트 도구에서는 생략합니다.
    HRESULT hrCom = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if( FAILED(hrCom) )
    {
        _tprintf(_T("CoInitializeEx 실패. HRESULT: 0x%08X\n"), hrCom);
        return -1;
    }

    HRESULT hrSec = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    if( FAILED(hrSec) )
    {
        _tprintf(_T("CoInitializeSecurity 실패. HRESULT: 0x%08X\n"), hrSec);
        CoUninitialize();
        return -1;
    }

    {
        CWmi Wmi;

        if( !Wmi.Connect() )
        {
            _tprintf(_T("WMI 연결 실패 - winmgmt 서비스가 실행 중인지 확인하세요.\n"));
            CoUninitialize();
            return -1;
        }

        TestWmiProcessorInfo(Wmi);
        TestWmiBiosInfo(Wmi);
        TestWmiMainBoardInfo(Wmi);
        TestWmiMemoryInfo(Wmi);
        TestWmiHdDiskInfo(Wmi);
        TestWmiDriveInfo(Wmi);
        TestWmiVideoCardInfo(Wmi);
        TestWmiSoundCardInfo(Wmi);
        TestWmiNetworkCardInfo(Wmi);
        TestWmiPciInfo(Wmi);
        TestWmiCdromInfo(Wmi);
        TestWmiKeyboardInfo(Wmi);
        TestWmiMouseInfo(Wmi);
        TestWmiMonitorInfo(Wmi);

    } // Wmi 소멸 (COM이 아직 살아있는 상태에서 안전하게 Release)

    CoUninitialize();

    _tprintf(_T("\n***************************************************************************\n"));
    _tprintf(_T("테스트 완료. [FAIL]이 있으면 해당 WMI 클래스(Win32_* 쿼리)가\n"));
    _tprintf(_T("이 시스템/환경에서 지원되는지 확인하세요.\n"));

#ifdef _DEBUG
    PauseConsole();
#endif

    return 0;
}