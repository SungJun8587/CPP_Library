
//***************************************************************************
// HardwareInfo.cpp: implementation of the Hardware Information class.
//
//***************************************************************************

#include "pch.h"
#include "HardwareInfo.h"
#include <comdef.h>
#include <vector>

//***************************************************************************
// Helper Functions & RAII Wrappers
//***************************************************************************
namespace
{
    //***************************************************************************
    // @brief VARIANT 자원의 자동 해제를 담당하는 RAII 구조체입니다.
    // @param 없음
    // @return 없음
    // @detail 생성 시 VariantInit, 소멸 시 VariantClear를 호출하여 메모리 누수를 방지합니다.
    //***************************************************************************
    struct CVariantGuard : public VARIANT
    {
        CVariantGuard() { VariantInit(this); }
        ~CVariantGuard() { VariantClear(this); }
    };

    //***************************************************************************
    // @brief WMI 객체에서 문자열(BSTR) 속성을 추출합니다.
    // @param Wmi WMI 객체 참조
    // @param nIndex 쿼리 결과의 인덱스
    // @param pcszProp 가져올 WMI 속성 이름
    // @param dest 속성값을 저장할 출력 버퍼
    // @return void
    // @detail BSTR 변환 시 _bstr_t를 사용하여 안전하게 TCHAR 문자열로 복사합니다.
    //***************************************************************************
    template <size_t N>
    void FetchWmiString(CWmi& Wmi, int nIndex, const TCHAR* pcszProp, TCHAR(&dest)[N])
    {
        CVariantGuard vt;
        Wmi.GetProperties(nIndex, pcszProp, vt);

        if( vt.vt == VT_BSTR && vt.bstrVal != nullptr )
        {
            _bstr_t bstr(vt.bstrVal);
            _tcsncpy_s(dest, N, static_cast<LPCTSTR>(bstr), _TRUNCATE);
        }
    }

    //***************************************************************************
    // @brief WMI 객체에서 64비트 정수형 속성을 추출합니다.
    // @param Wmi WMI 객체 참조
    // @param nIndex 쿼리 결과의 인덱스
    // @param pcszProp 가져올 WMI 속성 이름
    // @return __int64 파싱된 64비트 정수값 (실패 시 0)
    // @detail BSTR 형태로 반환된 용량/크기 데이터를 검증 후 __int64로 변환합니다.
    //***************************************************************************
    __int64 FetchWmiInt64(CWmi& Wmi, int nIndex, const TCHAR* pcszProp)
    {
        CVariantGuard vt;
        Wmi.GetProperties(nIndex, pcszProp, vt);

        if( vt.vt == VT_BSTR && vt.bstrVal != nullptr )
        {
            _bstr_t bstr(vt.bstrVal);
            LPCTSTR ptszStr = static_cast<LPCTSTR>(bstr);
            if( ptszStr && *ptszStr != _T('\0') && std::all_of(ptszStr, ptszStr + _tcslen(ptszStr), ::_istdigit) )
            {
                return _ttoi64(ptszStr);
            }
        }
        return 0;
    }
}

//***************************************************************************
// @brief 바이트 단위의 데이터를 KB, MB, GB, TB 형태의 포맷 문자열로 변환합니다.
// @param nData 변환할 바이트 단위 크기 데이터
// @param ptszFormat 포맷팅된 결과 문자열을 저장할 버퍼
// @return void
// @detail 1024 기준으로 단위를 반복 계산하며 오버플로를 방지하기 위해 배열 한계를 제어합니다.
//***************************************************************************
void ChangeDataFormat(const __int64& nData, TCHAR* ptszFormat)
{
    const int NUMFORMATTERS = 5;
    double dblBase = static_cast<double>(nData);
    int nNumConversions = 0;
    const TCHAR* tszFormatters[NUMFORMATTERS] = { _T(" bytes"), _T(" KB"), _T(" MB"), _T(" GB"), _T(" TB") };

    while( dblBase >= 1024.0 && nNumConversions < (NUMFORMATTERS - 1) )
    {
        dblBase /= 1024.0;
        nNumConversions++;
    }

    _stprintf_s(ptszFormat, NUMERIC_STRING_LEN, _T("%0.2f%s"), dblBase, tszFormatters[nNumConversions]);
}

//***************************************************************************
// CBiosInfo
//***************************************************************************

//***************************************************************************
// @brief CBiosInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_Bios를 0으로 초기화합니다.
//***************************************************************************
CBiosInfo::CBiosInfo()
{
    ZeroMemory(&m_Bios, sizeof(HWINFO_BIOS));
}

//***************************************************************************
// @brief CBiosInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 클래스 객체 해제 시 필요한 정리를 수행합니다.
//***************************************************************************
CBiosInfo::~CBiosInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 BIOS 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_BIOS 클래스에서 제조사, 버전, 시리얼 번호 등의 속성을 추출합니다.
//***************************************************************************
BOOL CBiosInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_BIOS"));
    if( nIndex < 0 ) return FALSE;

    FetchWmiString(Wmi, 0, _T("Manufacturer"), m_Bios.m_tszManufacturer);
    FetchWmiString(Wmi, 0, _T("SMBIOSBIOSVersion"), m_Bios.m_tszSmVersion);
    FetchWmiString(Wmi, 0, _T("Version"), m_Bios.m_tszVersion);
    FetchWmiString(Wmi, 0, _T("IdentificationCode"), m_Bios.m_tszIdentificationCode);
    FetchWmiString(Wmi, 0, _T("SerialNumber"), m_Bios.m_tszSerialNumber);
    FetchWmiString(Wmi, 0, _T("ReleaseDate"), m_Bios.m_tszReleaseDate);

    return TRUE;
}

//***************************************************************************
// CMainBoardInfo
//***************************************************************************

//***************************************************************************
// @brief CMainBoardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_MainBoard를 0으로 초기화합니다.
//***************************************************************************
CMainBoardInfo::CMainBoardInfo()
{
    ZeroMemory(&m_MainBoard, sizeof(HWINFO_MAINBOARD));
}

//***************************************************************************
// @brief CMainBoardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 클래스 객체 해제 시 필요한 정리를 수행합니다.
//***************************************************************************
CMainBoardInfo::~CMainBoardInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 메인보드 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_BaseBoard 클래스에서 메인보드 제품명, 시리얼, 제조사 정보를 수집합니다.
//***************************************************************************
BOOL CMainBoardInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_BaseBoard"));
    if( nIndex < 0 ) return FALSE;

    FetchWmiString(Wmi, 0, _T("Product"), m_MainBoard.m_tszProduct);
    FetchWmiString(Wmi, 0, _T("SerialNumber"), m_MainBoard.m_tszSerialNumber);
    FetchWmiString(Wmi, 0, _T("Manufacturer"), m_MainBoard.m_tszManufacturer);
    FetchWmiString(Wmi, 0, _T("Description"), m_MainBoard.m_tszDescription);

    return TRUE;
}

//***************************************************************************
// CMemoryInfo
//***************************************************************************

//***************************************************************************
// @brief CMemoryInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 메모리 정보 구조체 m_Memory를 초기화합니다.
//***************************************************************************
CMemoryInfo::CMemoryInfo()
{
    ZeroMemory(&m_Memory, sizeof(HWINFO_MEMORY));
}

//***************************************************************************
// @brief CMemoryInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장된 HWINFO_RAM 동적 할당 포인터 객체들을 모두 해제합니다.
//***************************************************************************
CMemoryInfo::~CMemoryInfo()
{
    for( HWINFO_RAM* pRam : m_sRamArray )
    {
        delete pRam;
    }
    m_sRamArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 시스템 RAM 및 가상 메모리 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_PhysicalMemory와 Win32_OperatingSystem 정보를 이용해 메모리 모듈별 스펙 및 용량을 std::vector에 저장합니다.
//         전체 물리 메모리 총량은 GlobalMemoryStatusEx로 별도 산출합니다(슬롯별 Capacity 합산이 아님).
//***************************************************************************
BOOL CMemoryInfo::GetInformation(CWmi& Wmi)
{
    DWORD dwRamCount = 0;
    int nIndex = Wmi.ExecQuery(_T("Win32_PhysicalMemory"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        HWINFO_RAM* pRam = new HWINFO_RAM;

        FetchWmiString(Wmi, i, _T("BankLabel"), pRam->m_tszBankLabel);
        FetchWmiString(Wmi, i, _T("Name"), pRam->m_tszName);
        FetchWmiString(Wmi, i, _T("DeviceLocator"), pRam->m_tszDeviceLocator);

        // [수정] 개별 슬롯의 Capacity는 표시용으로만 저장하고, 전체 메모리 총량에는
        // 더 이상 합산하지 않습니다. LPDDR4/온보드(임베디드) 메모리는 물리 다이 1개가
        // 컨트롤러/채널 단위로 여러 인스턴스로 보고되는 경우가 있어, 슬롯별 Capacity를
        // 그대로 합치면 실제 실장 용량보다 과다 집계될 수 있습니다.
        // 전체 물리 메모리 총량은 아래에서 GlobalMemoryStatusEx()로 단일 조회합니다.
        pRam->m_nCapacity = FetchWmiInt64(Wmi, i, _T("Capacity"));

        // 1. FormFactor 처리 (VariantChangeType으로 VT_I4 변환 시도)
        CVariantGuard vtFormFactor;
        Wmi.GetProperties(i, _T("FormFactor"), vtFormFactor);

        // VT_UI2, VT_I2 등 다양한 정수 형식을 VT_I4로 안전하게 변환
        if( SUCCEEDED(VariantChangeType(&vtFormFactor, &vtFormFactor, 0, VT_I4)) )
        {
            pRam->m_dwFormFactor = vtFormFactor.lVal;
            _tcsncpy_s(pRam->m_tszFormFactorDesc, _countof(pRam->m_tszFormFactorDesc), FormFactorFormatDesc(vtFormFactor.lVal).c_str(), _TRUNCATE);
        }

        // 2. MemoryType 처리 (SMBIOSMemoryType 우선 조회 후 MemoryType 폴백)
        CVariantGuard vtSmbiosMemoryType;
        Wmi.GetProperties(i, _T("SMBIOSMemoryType"), vtSmbiosMemoryType);

        // [수정] 폴백 조회 시 이미 값이 담긴 VARIANT를 재사용하지 않고 별도의
        // CVariantGuard를 사용합니다. CWmi::GetProperties(VARIANT&)는 내부에서
        // VariantClear() 없이 IWbemClassObject::Get()을 바로 호출하므로, 동일
        // VARIANT를 재사용해 두 번째 Get()을 호출하면 첫 번째 결과가 BSTR 등
        // 할당된 리소스를 갖고 있던 경우 해제되지 않고 덮어써질 수 있습니다.
        CVariantGuard vtMemoryType;
        const VARIANT* pvtMemoryType = &vtSmbiosMemoryType;

        // SMBIOSMemoryType이 없거나 VT_EMPTY/VT_NULL인 경우 기존 MemoryType 조회
        if( vtSmbiosMemoryType.vt == VT_EMPTY || vtSmbiosMemoryType.vt == VT_NULL || vtSmbiosMemoryType.lVal == 0 )
        {
            Wmi.GetProperties(i, _T("MemoryType"), vtMemoryType);
            pvtMemoryType = &vtMemoryType;
        }

        CVariantGuard vtMemoryTypeConverted;
        if( SUCCEEDED(VariantChangeType(&vtMemoryTypeConverted, const_cast<VARIANT*>(pvtMemoryType), 0, VT_I4)) )
        {
            pRam->m_dwMemoryType = vtMemoryTypeConverted.lVal;
            _tcsncpy_s(pRam->m_tszMemoryTypeDesc, _countof(pRam->m_tszMemoryTypeDesc), MemoryTypeFormatDesc(vtMemoryTypeConverted.lVal).c_str(), _TRUNCATE);
        }

        // 3. Speed 처리
        CVariantGuard vtSpeed;
        Wmi.GetProperties(i, _T("Speed"), vtSpeed);
        if( SUCCEEDED(VariantChangeType(&vtSpeed, &vtSpeed, 0, VT_I4)) )
        {
            pRam->m_dwSpeed = vtSpeed.lVal;
        }

        dwRamCount++;
        m_sRamArray.push_back(pRam);
    }

    m_Memory.m_dwRamCount = dwRamCount;

    // [수정] 전체 물리 메모리 총량은 슬롯별 Capacity 합산 대신
    // GlobalMemoryStatusEx()로 한 번에 조회합니다.
    MEMORYSTATUSEX statex;
    ZeroMemory(&statex, sizeof(statex));
    statex.dwLength = sizeof(statex);
    if( GlobalMemoryStatusEx(&statex) )
    {
        m_Memory.m_nTotalMemSize = static_cast<__int64>(statex.ullTotalPhys);
    }

    nIndex = Wmi.ExecQuery(_T("Win32_OperatingSystem"));
    if( nIndex < 0 ) return FALSE;

    m_Memory.m_nPhysicalMemSize += FetchWmiInt64(Wmi, 0, _T("FreePhysicalMemory"));
    m_Memory.m_nTotalVirtualMemSize += FetchWmiInt64(Wmi, 0, _T("TotalVirtualMemorySize"));
    m_Memory.m_nFreeVirtualMemSize += FetchWmiInt64(Wmi, 0, _T("FreeVirtualMemory"));
    m_Memory.m_nTotalPageFileSize += FetchWmiInt64(Wmi, 0, _T("SizeStoredInPagingFiles"));
    m_Memory.m_nFreePageFileSize += FetchWmiInt64(Wmi, 0, _T("FreeSpaceInPagingFiles"));

    return TRUE;
}

//***************************************************************************
// @brief FormFactor 열거형 ID 값을 문자열 설명으로 변환합니다. (SMBIOS 3.7+ 반영)
// @param dwFormFactor WMI FormFactor 코드값
// @return _tstring 폼팩터 규격명 문자열
//***************************************************************************
_tstring CMemoryInfo::FormFactorFormatDesc(DWORD dwFormFactor) const
{
    switch( dwFormFactor )
    {
    case 0: return _T("Unknown (Onboard / Embedded)");
    case 1:  return _T("Other");
    case 2:  return _T("SIP");
    case 3:  return _T("DIP");
    case 4:  return _T("ZIP");
    case 5:  return _T("SOJ");
    case 6:  return _T("Proprietary");
    case 7:  return _T("SIMM");
    case 8:  return _T("DIMM");
    case 9:  return _T("TSOP");
    case 10: return _T("PGA");
    case 11: return _T("RIMM");
    case 12: return _T("SODIMM");
    case 13: return _T("SRIMM");
    case 14: return _T("SMD");
    case 15: return _T("SSMP");
    case 16: return _T("QFP");
    case 17: return _T("TQFP");
    case 18: return _T("SOIC");
    case 19: return _T("LCC");
    case 20: return _T("PLCC");
    case 21: return _T("BGA");
    case 22: return _T("FPBGA");
    case 23: return _T("LGA");
    case 24: return _T("FB-DIMM");
    case 25: return _T("Die");
    case 26: return _T("CAMM");
    default: return _T("Unknown");
    }
}

//***************************************************************************
// @brief MemoryType 열거형 ID 값을 문자열 설명으로 변환합니다. (SMBIOS 3.7+ 반영)
// @param dwMemoryType WMI MemoryType / SMBIOSMemoryType 코드값
// @return _tstring 메모리 규격명 문자열
//***************************************************************************
_tstring CMemoryInfo::MemoryTypeFormatDesc(DWORD dwMemoryType) const
{
    switch( dwMemoryType )
    {
    case 0:  return _T("Unknown");
    case 1:  return _T("Other");
    case 2:  return _T("DRAM");
    case 3:  return _T("Synchronous DRAM");
    case 4:  return _T("Cache DRAM");
    case 5:  return _T("EDO");
    case 6:  return _T("EDRAM");
    case 7:  return _T("VRAM");
    case 8:  return _T("SRAM");
    case 9:  return _T("RAM");
    case 10: return _T("ROM");
    case 11: return _T("Flash");
    case 12: return _T("EEPROM");
    case 13: return _T("FEPROM");
    case 14: return _T("EPROM");
    case 15: return _T("CDRAM");
    case 16: return _T("3DRAM");
    case 17: return _T("SDRAM");
    case 18: return _T("SGRAM");
    case 19: return _T("RDRAM");
    case 20: return _T("DDR");
    case 21: return _T("DDR2");
    case 22: return _T("DDR2 FB-DIMM");
    case 24: return _T("DDR3");
    case 25: return _T("FBD2");
    case 26: return _T("DDR4");
    case 27: return _T("LPDDR");
    case 28: return _T("LPDDR2");
    case 29: return _T("LPDDR3");
    case 30: return _T("LPDDR4");
    case 31: return _T("Logical non-volatile device");
    case 32: return _T("HBM");
    case 33: return _T("HBM2");
    case 34: return _T("DDR5");
    case 35: return _T("LPDDR5");
    case 36: return _T("HBM3");
    default: return _T("Unknown");
    }
}

//***************************************************************************
// CHdDiskInfo
//***************************************************************************

//***************************************************************************
// @brief CHdDiskInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 물리 하드디스크 정보 관리를 위한 객체를 초기화합니다.
//***************************************************************************
CHdDiskInfo::CHdDiskInfo()
{
}

//***************************************************************************
// @brief CHdDiskInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장된 물리 하드디스크 동적 객체 메모리를 모두 해제합니다.
//***************************************************************************
CHdDiskInfo::~CHdDiskInfo()
{
    for( HWINFO_HDDISK* pHdDisk : m_sHdDiskArray )
    {
        delete pHdDisk;
    }
    m_sHdDiskArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 물리 디스크 드라이브 목록과 용량을 추출합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_DiskDrive 쿼리를 수행하여 검색된 각 하드디스크 정보를 std::vector에 저장합니다.
//***************************************************************************
BOOL CHdDiskInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_DiskDrive"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        HWINFO_HDDISK* pHdDisk = new HWINFO_HDDISK;

        FetchWmiString(Wmi, i, _T("Model"), pHdDisk->m_tszModel);
        FetchWmiString(Wmi, i, _T("Name"), pHdDisk->m_tszName);
        FetchWmiString(Wmi, i, _T("Manufacturer"), pHdDisk->m_tszManufacturer);
        FetchWmiString(Wmi, i, _T("Description"), pHdDisk->m_tszDescription);

        pHdDisk->m_nTotalSize = FetchWmiInt64(Wmi, i, _T("Size"));

        m_sHdDiskArray.push_back(pHdDisk);
    }

    return TRUE;
}

//***************************************************************************
// CDriveInfo
//***************************************************************************

//***************************************************************************
// @brief CDriveInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 논리 드라이브 관리를 위한 인스턴스를 생성합니다.
//***************************************************************************
CDriveInfo::CDriveInfo()
{
}

//***************************************************************************
// @brief CDriveInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내의 로지컬 드라이브 포인터를 순회하며 할당을 해제합니다.
//***************************************************************************
CDriveInfo::~CDriveInfo()
{
    for( HWINFO_DRIVE* pDrive : m_sDriveArray )
    {
        delete pDrive;
    }
    m_sDriveArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 논리 디스크(로컬 드라이브) 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail drivetype = 3(고정 디스크)인 항목만 추려서 용량 및 남은 공간을 구하고 vector에 푸시합니다.
//***************************************************************************
BOOL CDriveInfo::GetInformation(CWmi& Wmi)
{
    DWORD dwDriveCount = 0;
    int nIndex = Wmi.ExecQuery(_T("Win32_LogicalDisk WHERE drivetype = 3"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        HWINFO_DRIVE* pDrive = new HWINFO_DRIVE;

        FetchWmiString(Wmi, i, _T("Name"), pDrive->m_tszName);
        FetchWmiString(Wmi, i, _T("FileSystem"), pDrive->m_tszFileSystem);

        pDrive->m_nTotalSpace = FetchWmiInt64(Wmi, i, _T("Size"));
        m_Drives.m_nTotalSpace += pDrive->m_nTotalSpace;

        pDrive->m_nFreeSpace = FetchWmiInt64(Wmi, i, _T("FreeSpace"));
        m_Drives.m_nFreeSpace += pDrive->m_nFreeSpace;

        dwDriveCount++;
        m_sDriveArray.push_back(pDrive);
    }

    m_Drives.m_dwDriveCount = dwDriveCount;
    return TRUE;
}

//***************************************************************************
// CSoundCardInfo
//***************************************************************************

//***************************************************************************
// @brief CSoundCardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_SoundCard를 0으로 초기화합니다.
//***************************************************************************
CSoundCardInfo::CSoundCardInfo()
{
    ZeroMemory(&m_SoundCard, sizeof(HWINFO_SOUNDCARD));
}

//***************************************************************************
// @brief CSoundCardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 사운드 카드 객체 종료 시 필요한 정리를 수행합니다.
//***************************************************************************
CSoundCardInfo::~CSoundCardInfo()
{
}

//***************************************************************************
// @brief waveOut API를 통해 사운드 카드의 가용성 및 상세 속성을 가져옵니다.
// @param 없음
// @return BOOL 사운드 카드가 존재하면 TRUE, 없으면 FALSE
// @detail waveOutGetDevCaps를 사용하여 사운드 장치의 이름, 제조사 코드 및 볼륨 제어 지원 유무를 확인합니다.
//***************************************************************************
BOOL CSoundCardInfo::GetInformation()
{
    UINT uWavNumDevices = waveOutGetNumDevs();
    BOOL bIsInstalled = (uWavNumDevices > 0) ? TRUE : FALSE;

    if( bIsInstalled )
    {
        WAVEOUTCAPS wavCaps;
        if( waveOutGetDevCaps(0, &wavCaps, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR )
        {
            _tcsncpy_s(m_SoundCard.m_tszProductName, _countof(m_SoundCard.m_tszProductName), wavCaps.szPname, _TRUNCATE);

            _tcsncpy_s(m_SoundCard.m_tszCompanyName, _countof(m_SoundCard.m_tszCompanyName), GetAudioDevCompanyName(wavCaps.wMid).c_str(), _TRUNCATE);

            m_SoundCard.m_bHasSeparateLRVolCtrl = (wavCaps.dwSupport & WAVECAPS_VOLUME) ? TRUE : FALSE;
            m_SoundCard.m_bHasVolCtrl = (wavCaps.dwSupport & AUXCAPS_VOLUME) ? TRUE : FALSE;
        }
    }

    return bIsInstalled;
}

//***************************************************************************
// @brief 오디오 제조사 ID 코드를 기업 이름 문자열로 해석합니다.
// @param nCompany Windows Multimedia Manufacturer ID
// @return _tstring 오디오 제조사명 문자열
//***************************************************************************
_tstring CSoundCardInfo::GetAudioDevCompanyName(int nCompany) const
{
    switch( nCompany )
    {
    case MM_MICROSOFT:      return _T("Microsoft Corporation");
    case MM_CREATIVE:       return _T("Creative Labs, Inc");
    case MM_MEDIAVISION:    return _T("Media Vision, Inc.");
    case MM_FUJITSU:        return _T("Fujitsu Corp.");
    case MM_ARTISOFT:       return _T("Artisoft, Inc.");
    case MM_TURTLE_BEACH:   return _T("Turtle Beach, Inc.");
    case MM_IBM:            return _T("IBM Corporation");
    case MM_VOCALTEC:       return _T("Vocaltec LTD.");
    case MM_ROLAND:         return _T("Roland");
    case MM_DSP_SOLUTIONS:  return _T("DSP Solutions, Inc.");
    case MM_NEC:            return _T("NEC");
    case MM_ATI:            return _T("ATI");
    case MM_WANGLABS:       return _T("Wang Laboratories, Inc");
    case MM_TANDY:          return _T("Tandy Corporation");
    case MM_VOYETRA:        return _T("Voyetra");
    case MM_ANTEX:          return _T("Antex Electronics Corporation");
    case MM_ICL_PS:         return _T("ICL Personal Systems");
    case MM_INTEL:          return _T("Intel Corporation");
    case MM_GRAVIS:         return _T("Advanced Gravis");
    case MM_VAL:            return _T("Video Associates Labs, Inc.");
    case MM_INTERACTIVE:    return _T("InterActive Inc");
    case MM_YAMAHA:         return _T("Yamaha Corporation of America");
    case MM_EVEREX:         return _T("Everex Systems, Inc");
    case MM_ECHO:           return _T("Echo Speech Corporation");
    case MM_SIERRA:         return _T("Sierra Semiconductor Corp");
    case MM_CAT:            return _T("Computer Aided Technologies");
    case MM_APPS:           return _T("APPS Software International");
    case MM_DSP_GROUP:      return _T("DSP Group, Inc");
    case MM_MELABS:         return _T("MicroEngineering Labs");
    case MM_COMPUTER_FRIENDS: return _T("Computer Friends, Inc.");
    case MM_ESS:            return _T("ESS Technology");
    case MM_AUDIOFILE:      return _T("Audio, Inc.");
    case MM_MOTOROLA:       return _T("Motorola, Inc.");
    case MM_CANOPUS:        return _T("Canopus, co., Ltd.");
    case MM_EPSON:          return _T("Seiko Epson Corporation");
    case MM_TRUEVISION:     return _T("Truevision");
    case MM_AZTECH:         return _T("Aztech Labs, Inc.");
    case MM_VIDEOLOGIC:     return _T("Videologic");
    case MM_SCALACS:        return _T("SCALACS");
    case MM_KORG:           return _T("Korg Inc.");
    case MM_APT:            return _T("Audio Processing Technology");
    case MM_ICS:            return _T("Integrated Circuit Systems, Inc.");
    case MM_ITERATEDSYS:    return _T("Iterated Systems, Inc.");
    case MM_METHEUS:        return _T("Metheus");
    case MM_LOGITECH:       return _T("Logitech, Inc.");
    case MM_WINNOV:         return _T("Winnov, Inc.");
    case MM_NCR:            return _T("NCR Corporation");
    case MM_EXAN:           return _T("EXAN");
    case MM_AST:            return _T("AST Research Inc.");
    case MM_WILLOWPOND:     return _T("Willow Pond Corporation");
    case MM_SONICFOUNDRY:   return _T("Sonic Foundry");
    case MM_VITEC:          return _T("Vitec Multimedia");
    case MM_MOSCOM:         return _T("MOSCOM Corporation");
    case MM_SILICONSOFT:    return _T("Silicon Soft, Inc.");
    case MM_SUPERMAC:       return _T("Supermac");
    case MM_AUDIOPT:        return _T("Audio Processing Technology");
    case MM_SPEECHCOMP:     return _T("Speech Compression");
    case MM_AHEAD:          return _T("Ahead, Inc.");
    case MM_DOLBY:          return _T("Dolby Laboratories");
    case MM_OKI:            return _T("OKI");
    case MM_AURAVISION:     return _T("AuraVision Corporation");
    case MM_OLIVETTI:       return _T("Ing C. Olivetti & C., S.p.A.");
    case MM_IOMAGIC:        return _T("I/O Magic Corporation");
    case MM_MATSUSHITA:     return _T("Matsushita Electric Industrial Co., LTD.");
    case MM_CONTROLRES:     return _T("Control Resources Limited");
    case MM_XEBEC:          return _T("Xebec Multimedia Solutions Limited");
    case MM_NEWMEDIA:       return _T("New Media Corporation");
    case MM_NMS:            return _T("Natural MicroSystems");
    case MM_LYRRUS:         return _T("Lyrrus Inc.");
    case MM_COMPUSIC:       return _T("Compusic");
    case MM_OPTI:           return _T("OPTI Computers Inc.");
    case MM_ADLACC:         return _T("Adlib Accessories Inc.");
    case MM_COMPAQ:         return _T("Compaq Computer Corp.");
    case MM_DIALOGIC:       return _T("Dialogic Corporation");
    case MM_INSOFT:         return _T("InSoft, Inc.");
    case MM_MPTUS:          return _T("M.P. Technologies, Inc.");
    case MM_WEITEK:         return _T("Weitek");
    case MM_LERNOUT_AND_HAUSPIE: return _T("Lernout & Hauspie");
    case MM_QCIAR:          return _T("Quanta Computer Inc.");
    case MM_APPLE:          return _T("Apple Computer, Inc.");
    case MM_DIGITAL:        return _T("Digital Equipment Corporation");
    case MM_MOTU:           return _T("Mark of the Unicorn");
    case MM_WORKBIT:        return _T("Workbit Corporation");
    case MM_OSITECH:        return _T("Ositech Communications Inc.");
    case MM_MIRO:           return _T("miro Computer Products AG");
    case MM_CIRRUSLOGIC:    return _T("Cirrus Logic");
    case MM_ISOLUTION:      return _T("ISOLUTION B.V.");
    case MM_HORIZONS:       return _T("Horizons Technology, Inc");
    case MM_CONCEPTS:       return _T("Computer Concepts Ltd");
    case MM_VTG:            return _T("Voice Technologies Group, Inc.");
    case MM_RADIUS:         return _T("Radius");
    case MM_ROCKWELL:       return _T("Rockwell International");
    case MM_OPCODE:         return _T("Opcode Systems");
    case MM_VOXWARE:        return _T("Voxware Inc");
    case MM_NORTHERN_TELECOM: return _T("Northern Telecom Limited");
    case MM_APICOM:         return _T("APICOM");
    case MM_GRANDE:         return _T("Grande Software");
    case MM_ADDX:           return _T("ADDX");
    case MM_WILDCAT:        return _T("Wildcat Canyon Software");
    case MM_RHETOREX:       return _T("Rhetorex Inc");
    case MM_BROOKTREE:      return _T("Brooktree Corporation");
    case MM_ENSONIQ:        return _T("ENSONIQ Corporation");
    case MM_FAST:           return _T("///FAST Multimedia AG");
    case MM_NVIDIA:         return _T("NVidia Corporation");
    case MM_OKSORI:         return _T("OKSORI Co., Ltd.");
    case MM_DIACOUSTICS:    return _T("DiAcoustics, Inc.");
    case MM_GULBRANSEN:     return _T("Gulbransen, Inc.");
    case MM_KAY_ELEMETRICS: return _T("Kay Elemetrics, Inc.");
    case MM_CRYSTAL:        return _T("Crystal Semiconductor Corporation");
    case MM_SPLASH_STUDIOS: return _T("Splash Studios");
    case MM_QUARTERDECK:    return _T("Quarterdeck Corporation");
    case MM_TDK:            return _T("TDK Corporation");
    case MM_DIGITAL_AUDIO_LABS: return _T("Digital Audio Labs, Inc.");
    case MM_SEERSYS:        return _T("Seer Systems, Inc.");
    case MM_PICTURETEL:     return _T("PictureTel Corporation");
    case MM_ATT_MICROELECTRONICS: return _T("AT&T Microelectronics");
    case MM_OSPREY:         return _T("Osprey Technologies, Inc.");
    case MM_MEDIATRIX:      return _T("Mediatrix Peripherals");
    case MM_SOUNDESIGNS:    return _T("SounDesignS M.C.S. Ltd.");
    case MM_ALDIGITAL:      return _T("A.L. Digital Ltd.");
    case MM_SPECTRUM_SIGNAL_PROCESSING: return _T("Spectrum Signal Processing, Inc.");
    case MM_ECS:            return _T("Electronic Courseware Systems, Inc.");
    case MM_AMD:            return _T("AMD");
    case MM_COREDYNAMICS:   return _T("Core Dynamics");
    case MM_CANAM:          return _T("CANAM Computers");
    case MM_SOFTSOUND:      return _T("Softsound, Ltd.");
    case MM_NORRIS:         return _T("Norris Communications, Inc.");
    case MM_DDD:            return _T("Danka Data Devices");
    case MM_EUPHONICS:      return _T("EuPhonics");
    case MM_PRECEPT:        return _T("Precept Software, Inc.");
    case MM_CRYSTAL_NET:    return _T("Crystal Net Corporation");
    case MM_CHROMATIC:      return _T("Chromatic Research, Inc");
    case MM_VOICEINFO:      return _T("Voice Information Systems, Inc");
    case MM_VIENNASYS:      return _T("Vienna Systems");
    case MM_CONNECTIX:      return _T("Connectix Corporation");
    case MM_GADGETLABS:     return _T("Gadget Labs LLC");
    case MM_FRONTIER:       return _T("Frontier Design Group LLC");
    case MM_VIONA:          return _T("Viona Development GmbH");
    case MM_CASIO:          return _T("Casio Computer Co., LTD");
    case MM_DIAMONDMM:      return _T("Diamond Multimedia");
    case MM_S3:             return _T("S3");
    case MM_FRAUNHOFER_IIS: return _T("Fraunhofer");
    default:                return _T("Unknown");
    }
}

//***************************************************************************
// CVideoCardInfo
//***************************************************************************

//***************************************************************************
// @brief CVideoCardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 비디오 카드 객체 생성을 담당합니다.
//***************************************************************************
CVideoCardInfo::CVideoCardInfo()
{
}

//***************************************************************************
// @brief CVideoCardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장된 비디오 카드 객체들을 순회하며 동적 할당 해제합니다.
//***************************************************************************
CVideoCardInfo::~CVideoCardInfo()
{
    for( HWINFO_VIDEOCARD* pVideoCard : m_sVideoCardArray )
    {
        delete pVideoCard;
    }
    m_sVideoCardArray.clear();
}

//***************************************************************************
// @brief 레지스트리 경로 탐색을 통해 그래픽 카드 정보를 분석/가져옵니다.
// @param 없음
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Windows 레지스트리의 DEVICEMAP\\VIDEO에서 디바이스 CLSID를 파싱하여 그래픽 드라이버명, 메모리 크기 등을 파악하고 std::vector에 저장합니다.
//***************************************************************************
BOOL CVideoCardInfo::GetInformation()
{
    const TCHAR* szKeyPath = IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) ? WIN_DEVICEMAP_VIDEO_KEY : NT_DEVICEMAP_VIDEO_KEY;
    const TCHAR* szRegex = IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) ? WIN_CONTROL_VIDEO_REGEX : NT_CONTROL_VIDEO_REGEX;
    const TCHAR* szControl = IsWindowVersion(-1, -1, VER_PLATFORM_WIN32_WINDOWS) ? WIN_CONTROL_VIDEO_KEY : NT_CONTROL_VIDEO_KEY;

    HKEY hSubKey = NULL;
    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyPath, 0, KEY_READ, &hSubKey) != ERROR_SUCCESS )
    {
        return FALSE;
    }

    DWORD dwValueNumber = 0;
    FILETIME ftLastWrite;
    if( RegQueryInfoKey(hSubKey, NULL, 0, 0, NULL, NULL, NULL, &dwValueNumber, NULL, NULL, NULL, &ftLastWrite) == ERROR_SUCCESS )
    {
        for( DWORD i = 0; i < dwValueNumber; i++ )
        {
            TCHAR tszDeviceName[REGISTRY_NAME_STRLEN] = { 0 };
            TCHAR tszDeviceValue[REGISTRY_VALUE_STRLEN] = { 0 };
            // [수정] RegEnumValue의 lpcchValueName은 '문자 개수' 단위입니다. sizeof()(바이트)를
            // 그대로 넘기면 UNICODE 빌드에서 실제 버퍼 크기의 2배 값이 전달되어,
            // 값 이름이 길 경우 스택 버퍼 오버플로우로 이어질 수 있습니다.
            DWORD dwNameLen = _countof(tszDeviceName);
            DWORD dwValueLen = sizeof(tszDeviceValue);

            if( RegEnumValue(hSubKey, i, tszDeviceName, &dwNameLen, NULL, NULL, (LPBYTE)tszDeviceValue, &dwValueLen) == ERROR_SUCCESS )
            {
                TCHAR* ptszDeviceClsid = _tcsstr(tszDeviceValue, szRegex);
                if( ptszDeviceClsid != NULL )
                {
                    ptszDeviceClsid += _tcslen(szRegex);

                    TCHAR tszSubKey[REGISTRY_KEY_STRLEN] = { 0 };
                    _stprintf_s(tszSubKey, _countof(tszSubKey), _T("%s\\%s"), szControl, ptszDeviceClsid);

                    HKEY hKeyProperty = NULL;
                    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, tszSubKey, 0, KEY_READ, &hKeyProperty) == ERROR_SUCCESS )
                    {
                        DWORD dwPropValueNumber = 0;
                        if( RegQueryInfoKey(hKeyProperty, NULL, 0, 0, NULL, NULL, NULL, &dwPropValueNumber, NULL, NULL, NULL, &ftLastWrite) == ERROR_SUCCESS )
                        {
                            TCHAR tszDeviceDesc[REGISTRY_VALUE_STRLEN] = { 0 };
                            TCHAR tszDriverDesc[REGISTRY_VALUE_STRLEN] = { 0 };
                            TCHAR tszAdapterString[REGISTRY_VALUE_STRLEN] = { 0 };
                            TCHAR tszChipType[REGISTRY_VALUE_STRLEN] = { 0 };
                            TCHAR tszDacType[REGISTRY_VALUE_STRLEN] = { 0 };
                            TCHAR tszDisplayDrivers[REGISTRY_VALUE_STRLEN] = { 0 };
                            long  lMemorySize = -1;

                            for( DWORD j = 0; j < dwPropValueNumber; j++ )
                            {
                                TCHAR tszVideoName[REGISTRY_NAME_STRLEN] = { 0 };
                                TCHAR tszVideoValue[REGISTRY_VALUE_STRLEN] = { 0 };
                                DWORD dwType = 0;
                                // [수정] 마찬가지로 문자 개수 단위로 전달합니다.
                                dwNameLen = _countof(tszVideoName);
                                dwValueLen = sizeof(tszVideoValue);

                                if( RegEnumValue(hKeyProperty, j, tszVideoName, &dwNameLen, NULL, &dwType, (LPBYTE)tszVideoValue, &dwValueLen) == ERROR_SUCCESS )
                                {
                                    if( dwType == REG_SZ || dwType == REG_MULTI_SZ )
                                    {
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_DEVICEDESC_NAME) == 0 )             _tcsncpy_s(tszDeviceDesc, tszVideoValue, _TRUNCATE);
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_DRIVERDESC_NAME) == 0 )             _tcsncpy_s(tszDriverDesc, tszVideoValue, _TRUNCATE);
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_ADAPTERSTRING_NAME) == 0 )          _tcsncpy_s(tszAdapterString, tszVideoValue, _TRUNCATE);
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_CHIPTYPE_NAME) == 0 )               _tcsncpy_s(tszChipType, tszVideoValue, _TRUNCATE);
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_DACTYPE_NAME) == 0 )                _tcsncpy_s(tszDacType, tszVideoValue, _TRUNCATE);
                                        if( _tcscmp(tszVideoName, WIN_VIDEO_INSTALLEDDISPLAYDRIVERS_NAME) == 0 ) _tcsncpy_s(tszDisplayDrivers, tszVideoValue, _TRUNCATE);
                                    }
                                }

                                if( _tcscmp(tszVideoName, WIN_VIDEO_MEMORYSIZE_NAME) == 0 )
                                {
                                    DWORD dwMemorySize = 0;
                                    dwValueLen = sizeof(dwMemorySize);
                                    if( RegQueryValueEx(hKeyProperty, tszVideoName, NULL, NULL, (LPBYTE)&dwMemorySize, &dwValueLen) == ERROR_SUCCESS )
                                    {
                                        lMemorySize = dwMemorySize / (1024 * 1024);
                                    }
                                }
                            }

                            if( _tcslen(tszAdapterString) > 0 )
                            {
                                HWINFO_VIDEOCARD* pVideoCard = new HWINFO_VIDEOCARD;

                                TCHAR tszDescription[REGISTRY_VALUE_STRLEN] = { 0 };
                                if( _tcslen(tszDeviceDesc) > 0 )      _tcsncpy_s(tszDescription, tszDeviceDesc, _TRUNCATE);
                                else if( _tcslen(tszDriverDesc) > 0 ) _tcsncpy_s(tszDescription, tszDriverDesc, _TRUNCATE);

                                _tcsncpy_s(pVideoCard->m_tszDescription, tszDescription, _TRUNCATE);
                                _tcsncpy_s(pVideoCard->m_tszAdapterString, tszAdapterString, _TRUNCATE);
                                _tcsncpy_s(pVideoCard->m_tszChipType, tszChipType, _TRUNCATE);
                                _tcsncpy_s(pVideoCard->m_tszDacType, tszDacType, _TRUNCATE);
                                _tcsncpy_s(pVideoCard->m_tszDisplayDrivers, tszDisplayDrivers, _TRUNCATE);
                                pVideoCard->m_lMemorySize = lMemorySize;

                                bool bIsAdd = true;
                                for( HWINFO_VIDEOCARD* pExistCard : m_sVideoCardArray )
                                {
                                    if( _tcscmp(pExistCard->m_tszAdapterString, pVideoCard->m_tszAdapterString) == 0 )
                                    {
                                        bIsAdd = false;
                                        break;
                                    }
                                }

                                if( bIsAdd ) m_sVideoCardArray.push_back(pVideoCard);
                                else delete pVideoCard;
                            }
                        }
                        RegCloseKey(hKeyProperty);
                    }
                }
            }
        }
        RegCloseKey(hSubKey);
    }

    return TRUE;
}

//***************************************************************************
// CNetworkCardInfo
//***************************************************************************

//***************************************************************************
// @brief CNetworkCardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 네트워크 카드를 다루는 객체를 생성합니다.
//***************************************************************************
CNetworkCardInfo::CNetworkCardInfo()
{
}

//***************************************************************************
// @brief CNetworkCardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내부의 네트워크 카드 객체 메모리를 수동 해제합니다.
//***************************************************************************
CNetworkCardInfo::~CNetworkCardInfo()
{
    for( HWINFO_NETWORKCARD* pNetworkCard : m_sNetworkCardArray )
    {
        delete pNetworkCard;
    }
    m_sNetworkCardArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 활성화된 네트워크 어댑터를 검색합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_NetworkAdapterConfiguration 클래스에서 IPEnabled 속성이 참인 어댑터만 선별하여 저장합니다.
//***************************************************************************
BOOL CNetworkCardInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_NetworkAdapterConfiguration"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        CVariantGuard vtProp;
        Wmi.GetProperties(i, _T("IPEnabled"), vtProp);

        // [수정] VT_BOOL 값은 VARIANT.boolVal(VARIANT_BOOL)에 저장됩니다. VT_UI1용
        // 멤버인 bVal을 사용하면 의도한 필드가 아니며 이식성도 없습니다.
        if( vtProp.vt == VT_BOOL && vtProp.boolVal != VARIANT_FALSE )
        {
            HWINFO_NETWORKCARD* pNetworkCard = new HWINFO_NETWORKCARD;
            FetchWmiString(Wmi, i, _T("Description"), pNetworkCard->m_tszDescription);
            m_sNetworkCardArray.push_back(pNetworkCard);
        }
    }

    return TRUE;
}

//***************************************************************************
// CCdromInfo
//***************************************************************************

//***************************************************************************
// @brief CCdromInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail CD-ROM 드라이브 제어 클래스를 초기화합니다.
//***************************************************************************
CCdromInfo::CCdromInfo()
{
}

//***************************************************************************
// @brief CCdromInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내부의 CD-ROM 정보 메모리를 자동 비웁니다.
//***************************************************************************
CCdromInfo::~CCdromInfo()
{
    for( HWINFO_CDROM* pCdrom : m_sCdromArray )
    {
        delete pCdrom;
    }
    m_sCdromArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 CD-ROM/광학 드라이브 정보를 조회합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_CDROMDrive 쿼리를 실행하여 기기명, 제조사명, 설명 정보를 배열에 수집합니다.
//***************************************************************************
BOOL CCdromInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_CDROMDrive"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        HWINFO_CDROM* pCdrom = new HWINFO_CDROM;

        FetchWmiString(Wmi, i, _T("Name"), pCdrom->m_tszName);
        FetchWmiString(Wmi, i, _T("Manufacturer"), pCdrom->m_tszManufacturer);
        FetchWmiString(Wmi, i, _T("Description"), pCdrom->m_tszDescription);

        m_sCdromArray.push_back(pCdrom);
    }

    return TRUE;
}

//***************************************************************************
// CKeyBoardInfo
//***************************************************************************

//***************************************************************************
// @brief CKeyBoardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_KeyBoard를 초기화합니다.
//***************************************************************************
CKeyBoardInfo::CKeyBoardInfo()
{
    ZeroMemory(&m_KeyBoard, sizeof(HWINFO_KEYBOARD));
}

//***************************************************************************
// @brief CKeyBoardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 키보드 정보 클래스의 객체 소멸 처리를 수행합니다.
//***************************************************************************
CKeyBoardInfo::~CKeyBoardInfo()
{
}

//***************************************************************************
// @brief WMI 및 Win32 API를 기반으로 키보드 정보를 조회합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_Keyboard의 설명 정보와 Win32 API인 GetKeyboardType 결과값을 융합 수집합니다.
//***************************************************************************
BOOL CKeyBoardInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_Keyboard"));
    if( nIndex < 0 ) return FALSE;

    FetchWmiString(Wmi, 0, _T("Description"), m_KeyBoard.m_tszDescription);
    DetectKbType();

    return TRUE;
}

//***************************************************************************
// @brief Win32 API(GetKeyboardType)를 이용해 연결된 키보드의 물리적 레이아웃 타입을 판별합니다.
// @param 없음
// @return void
// @detail 타입 번호(1~8)에 대응되는 정밀 키보드 명칭 문자열을 설정합니다.
//***************************************************************************
void CKeyBoardInfo::DetectKbType()
{
    int nRetType = ::GetKeyboardType(0);
    TCHAR tszKeyBoardType[KEYBOARD_TYPE_STRLEN] = { 0 };

    switch( nRetType )
    {
    case 1:  _tcsncpy_s(tszKeyBoardType, _T("IBM PC/XT or compatible (83-key)"), _TRUNCATE); break;
    case 2:  _tcsncpy_s(tszKeyBoardType, _T("Olivetti \"ICO\" (102-key)"), _TRUNCATE); break;
    case 3:  _tcsncpy_s(tszKeyBoardType, _T("IBM PC/AT (84-key) or similar"), _TRUNCATE); break;
    case 4:  _tcsncpy_s(tszKeyBoardType, _T("IBM enhanced (101- or 102-key)"), _TRUNCATE); break;
    case 5:  _tcsncpy_s(tszKeyBoardType, _T("Nokia 1050 and similar"), _TRUNCATE); break;
    case 6:  _tcsncpy_s(tszKeyBoardType, _T("Nokia 9140 and similar"), _TRUNCATE); break;
    case 7:  _tcsncpy_s(tszKeyBoardType, _T("Japanese"), _TRUNCATE); break;
    case 8:  _tcsncpy_s(tszKeyBoardType, _T("IBM PC/AT or compatible (101-key)"), _TRUNCATE); break;
    default: _tcsncpy_s(tszKeyBoardType, _T("Unknown"), _TRUNCATE); break;
    }

    _tcsncpy_s(m_KeyBoard.m_tszType, tszKeyBoardType, _TRUNCATE);
}

//***************************************************************************
// CMouseInfo
//***************************************************************************

//***************************************************************************
// @brief CMouseInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_Mouse를 초기화합니다.
//***************************************************************************
CMouseInfo::CMouseInfo()
{
    ZeroMemory(&m_Mouse, sizeof(HWINFO_MOUSE));
}

//***************************************************************************
// @brief CMouseInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 마우스 클래스 소멸 처리를 수행합니다.
//***************************************************************************
CMouseInfo::~CMouseInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 마우스/포인팅 장치 정보를 검색합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_PointingDevice 클래스를 조회해 마우스 제품명과 제조사 항목을 읽어옵니다.
//***************************************************************************
BOOL CMouseInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_PointingDevice"));
    if( nIndex < 0 ) return FALSE;

    FetchWmiString(Wmi, 0, _T("Name"), m_Mouse.m_tszName);
    FetchWmiString(Wmi, 0, _T("Manufacturer"), m_Mouse.m_tszManufacturer);
    FetchWmiString(Wmi, 0, _T("Description"), m_Mouse.m_tszDescription);

    return TRUE;
}

//***************************************************************************
// CMonitorInfo
//***************************************************************************

//***************************************************************************
// @brief CMonitorInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 디스플레이 모니터 정보 집합체를 초기화합니다.
//***************************************************************************
CMonitorInfo::CMonitorInfo()
{
}

//***************************************************************************
// @brief CMonitorInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장되어 있는 모니터 정보 구조체들을 메모리 해제합니다.
//***************************************************************************
CMonitorInfo::~CMonitorInfo()
{
    for( HWINFO_MONITOR* pMonitor : m_sMonitorArray )
    {
        delete pMonitor;
    }
    m_sMonitorArray.clear();
}

//***************************************************************************
// @brief WMI를 통해 연결된 모니터 장치 정보를 가져옵니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_DesktopMonitor를 검색하여 수집된 제조사 정보와 설명을 std::vector에 추가합니다.
//***************************************************************************
BOOL CMonitorInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_DesktopMonitor"));
    if( nIndex < 0 ) return FALSE;

    for( int i = 0; i < nIndex; i++ )
    {
        HWINFO_MONITOR* pMonitor = new HWINFO_MONITOR;

        FetchWmiString(Wmi, i, _T("MonitorManufacturer"), pMonitor->m_tszManufacturer);
        FetchWmiString(Wmi, i, _T("Description"), pMonitor->m_tszDescription);

        m_sMonitorArray.push_back(pMonitor);
    }

    return TRUE;
}