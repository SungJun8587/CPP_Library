
//***************************************************************************
// WmiHardwareInfo.cpp: implementation of the WMI-based Hardware Information Classes.
//
//***************************************************************************

#include "pch.h"
#include "WmiHardwareInfo.h"

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
// CWmiBiosInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiBiosInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_Bios를 0으로 초기화합니다.
//***************************************************************************
CWmiBiosInfo::CWmiBiosInfo()
{
    ZeroMemory(&m_Bios, sizeof(HWINFO_BIOS));
}

//***************************************************************************
// @brief CWmiBiosInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 클래스 객체 해제 시 필요한 정리를 수행합니다.
//***************************************************************************
CWmiBiosInfo::~CWmiBiosInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 BIOS 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_BIOS 클래스에서 제조사, 버전, 시리얼 번호 등의 속성을 추출합니다.
//***************************************************************************
BOOL CWmiBiosInfo::GetInformation(CWmi& Wmi)
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
// CWmiMainBoardInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiMainBoardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_MainBoard를 0으로 초기화합니다.
//***************************************************************************
CWmiMainBoardInfo::CWmiMainBoardInfo()
{
    ZeroMemory(&m_MainBoard, sizeof(HWINFO_MAINBOARD));
}

//***************************************************************************
// @brief CWmiMainBoardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 클래스 객체 해제 시 필요한 정리를 수행합니다.
//***************************************************************************
CWmiMainBoardInfo::~CWmiMainBoardInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 메인보드 정보를 수집합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_BaseBoard 클래스에서 메인보드 제품명, 시리얼, 제조사 정보를 수집합니다.
//***************************************************************************
BOOL CWmiMainBoardInfo::GetInformation(CWmi& Wmi)
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
// CWmiMemoryInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiMemoryInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 메모리 정보 구조체 m_Memory를 초기화합니다.
//***************************************************************************
CWmiMemoryInfo::CWmiMemoryInfo()
{
    ZeroMemory(&m_Memory, sizeof(HWINFO_MEMORY));
}

//***************************************************************************
// @brief CWmiMemoryInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장된 HWINFO_RAM 동적 할당 포인터 객체들을 모두 해제합니다.
//***************************************************************************
CWmiMemoryInfo::~CWmiMemoryInfo()
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
BOOL CWmiMemoryInfo::GetInformation(CWmi& Wmi)
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
_tstring CWmiMemoryInfo::FormFactorFormatDesc(DWORD dwFormFactor) const
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
_tstring CWmiMemoryInfo::MemoryTypeFormatDesc(DWORD dwMemoryType) const
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
// CWmiHdDiskInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiHdDiskInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 물리 하드디스크 정보 관리를 위한 객체를 초기화합니다.
//***************************************************************************
CWmiHdDiskInfo::CWmiHdDiskInfo()
{
}

//***************************************************************************
// @brief CWmiHdDiskInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장된 물리 하드디스크 동적 객체 메모리를 모두 해제합니다.
//***************************************************************************
CWmiHdDiskInfo::~CWmiHdDiskInfo()
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
BOOL CWmiHdDiskInfo::GetInformation(CWmi& Wmi)
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
// CWmiDriveInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiDriveInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 논리 드라이브 관리를 위한 인스턴스를 생성합니다.
//***************************************************************************
CWmiDriveInfo::CWmiDriveInfo()
{
}

//***************************************************************************
// @brief CWmiDriveInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내의 로지컬 드라이브 포인터를 순회하며 할당을 해제합니다.
//***************************************************************************
CWmiDriveInfo::~CWmiDriveInfo()
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
BOOL CWmiDriveInfo::GetInformation(CWmi& Wmi)
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
// CWmiNetworkCardInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiNetworkCardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 네트워크 카드를 다루는 객체를 생성합니다.
//***************************************************************************
CWmiNetworkCardInfo::CWmiNetworkCardInfo()
{
}

//***************************************************************************
// @brief CWmiNetworkCardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내부의 네트워크 카드 객체 메모리를 수동 해제합니다.
//***************************************************************************
CWmiNetworkCardInfo::~CWmiNetworkCardInfo()
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
BOOL CWmiNetworkCardInfo::GetInformation(CWmi& Wmi)
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
// CWmiCdromInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiCdromInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail CD-ROM 드라이브 제어 클래스를 초기화합니다.
//***************************************************************************
CWmiCdromInfo::CWmiCdromInfo()
{
}

//***************************************************************************
// @brief CWmiCdromInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector 내부의 CD-ROM 정보 메모리를 자동 비웁니다.
//***************************************************************************
CWmiCdromInfo::~CWmiCdromInfo()
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
BOOL CWmiCdromInfo::GetInformation(CWmi& Wmi)
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
// CWmiKeyBoardInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiKeyBoardInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_KeyBoard를 초기화합니다.
//***************************************************************************
CWmiKeyBoardInfo::CWmiKeyBoardInfo()
{
    ZeroMemory(&m_KeyBoard, sizeof(HWINFO_KEYBOARD));
}

//***************************************************************************
// @brief CWmiKeyBoardInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 키보드 정보 클래스의 객체 소멸 처리를 수행합니다.
//***************************************************************************
CWmiKeyBoardInfo::~CWmiKeyBoardInfo()
{
}

//***************************************************************************
// @brief WMI 및 Win32 API를 기반으로 키보드 정보를 조회합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_Keyboard의 설명 정보와 Win32 API인 GetKeyboardType 결과값을 융합 수집합니다.
//***************************************************************************
BOOL CWmiKeyBoardInfo::GetInformation(CWmi& Wmi)
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
void CWmiKeyBoardInfo::DetectKbType()
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
// CWmiMouseInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiMouseInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 멤버 변수 m_Mouse를 초기화합니다.
//***************************************************************************
CWmiMouseInfo::CWmiMouseInfo()
{
    ZeroMemory(&m_Mouse, sizeof(HWINFO_MOUSE));
}

//***************************************************************************
// @brief CWmiMouseInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail 마우스 클래스 소멸 처리를 수행합니다.
//***************************************************************************
CWmiMouseInfo::~CWmiMouseInfo()
{
}

//***************************************************************************
// @brief WMI를 통해 마우스/포인팅 장치 정보를 검색합니다.
// @param Wmi WMI 모듈 참조
// @return BOOL 성공 시 TRUE, 실패 시 FALSE
// @detail Win32_PointingDevice 클래스를 조회해 마우스 제품명과 제조사 항목을 읽어옵니다.
//***************************************************************************
BOOL CWmiMouseInfo::GetInformation(CWmi& Wmi)
{
    int nIndex = Wmi.ExecQuery(_T("Win32_PointingDevice"));
    if( nIndex < 0 ) return FALSE;

    FetchWmiString(Wmi, 0, _T("Name"), m_Mouse.m_tszName);
    FetchWmiString(Wmi, 0, _T("Manufacturer"), m_Mouse.m_tszManufacturer);
    FetchWmiString(Wmi, 0, _T("Description"), m_Mouse.m_tszDescription);

    return TRUE;
}

//***************************************************************************
// CWmiMonitorInfo
//***************************************************************************

//***************************************************************************
// @brief CWmiMonitorInfo 클래스 생성자입니다.
// @param 없음
// @return 없음
// @detail 디스플레이 모니터 정보 집합체를 초기화합니다.
//***************************************************************************
CWmiMonitorInfo::CWmiMonitorInfo()
{
}

//***************************************************************************
// @brief CWmiMonitorInfo 클래스 소멸자입니다.
// @param 없음
// @return 없음
// @detail std::vector에 저장되어 있는 모니터 정보 구조체들을 메모리 해제합니다.
//***************************************************************************
CWmiMonitorInfo::~CWmiMonitorInfo()
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
BOOL CWmiMonitorInfo::GetInformation(CWmi& Wmi)
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