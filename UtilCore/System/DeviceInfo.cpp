
//***************************************************************************
// DeviceInfo.cpp: implementation of Non-WMI Hardware Information Classes.
//
//***************************************************************************

#include "pch.h"
#include "DeviceInfo.h"

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <devguid.h>
#include <iphlpapi.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace 
{
    //***************************************************************************
    // @struct RawDeviceInfo
    // @brief SetupAPI로 얻는 공통 속성(Description/Manufacturer/HardwareId)만 담는
    //        내부 임시 구조체. 카테고리별 HWINFO_* 구조체는 필드 이름이 서로 달라서
    //        (ProductName vs Description 등) 이 값을 각자 알맞은 필드로 옮겨 담습니다.
    //***************************************************************************
    struct RawDeviceInfo
    {
        TCHAR desc[256];
        TCHAR mfg[128];
        TCHAR hwid[256];
    };

    //***************************************************************************
    // @brief SMBIOS/드라이버에서 얻은 ANSI 문자열을 TCHAR 버퍼로 복사합니다.
    // @param dst      [out] 복사받을 TCHAR 버퍼
    // @param dstCount [in]  dst의 문자 개수(바이트 아님)
    // @param src      [in]  원본 ANSI(char*) 문자열
    // @return 없음
    // @detail SMBIOS 문자열은 항상 ANSI라, UNICODE 빌드일 때만 실제 변환이 발생함.
    //***************************************************************************
    void CopyToTChar(TCHAR* dst, size_t dstCount, const char* src)
    {
#ifdef UNICODE
        MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dstCount);
#else
        strcpy_s(dst, dstCount, src);
#endif
    }

    //***************************************************************************
    // @brief 지정한 장치 클래스(Display/Media/CDROM/Keyboard/Mouse/Monitor)에 속한
    //        장치를 전부 열거하여 FriendlyName/HardwareID/Manufacturer를 채웁니다.
    // @param classGuid [in]  열거할 SetupAPI 장치 클래스 GUID
    // @param outList   [out] 결과를 채워 넣을 RawDeviceInfo 벡터
    // @return bool 열거 자체의 성공 여부 (장치 0개도 true - 클래스 열거 시도 성공)
    //***************************************************************************
    bool EnumRawDevicesByClass(const GUID& classGuid, std::vector<RawDeviceInfo>& outList)
    {
        HDEVINFO hDevInfo = SetupDiGetClassDevsA(&classGuid, NULL, NULL, DIGCF_PRESENT);
        if( hDevInfo == INVALID_HANDLE_VALUE )
        {
            return false;
        }

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for( DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++ )
        {
            RawDeviceInfo raw = { 0 };
            char buffer[512];

            if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME,
                NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
            {
                CopyToTChar(raw.desc, _countof(raw.desc), buffer);
            }
            else if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
                NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
            {
                CopyToTChar(raw.desc, _countof(raw.desc), buffer);
            }

            if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
            {
                CopyToTChar(raw.hwid, _countof(raw.hwid), buffer);
            }

            if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_MFG,
                NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
            {
                CopyToTChar(raw.mfg, _countof(raw.mfg), buffer);
            }

            outList.push_back(raw);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
        return true;
    }
}

//***************************************************************************
// CDriveInfo
//***************************************************************************

CDriveInfo::CDriveInfo()
{
}

CDriveInfo::~CDriveInfo()
{
    for( HWINFO_DRIVE* pDrive : m_sDriveArray )
    {
        delete pDrive;
    }
    m_sDriveArray.clear();
}

//***************************************************************************
// @brief Win32 API를 통해 각 논리 드라이브의 공간 및 파일 시스템을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GetLogicalDrives로 드라이브 문자를 얻고, DRIVE_FIXED인 것만 GetVolumeInformation/
//         GetDiskFreeSpaceEx로 파일시스템과 용량을 조회하며 m_Drives에 전체 합계를 누적합니다.
//***************************************************************************
BOOL CDriveInfo::GetInformation()
{
    for( HWINFO_DRIVE* pDrive : m_sDriveArray )
    {
        delete pDrive;
    }
    m_sDriveArray.clear();
    m_Drives = HWINFO_DRIVES();

    DWORD drives = GetLogicalDrives();
    if( drives == 0 )
    {
        return FALSE;
    }

    for( int i = 0; i < 26; i++ )
    {
        if( !(drives & (1 << i)) )
        {
            continue;
        }

        TCHAR tszRoot[4] = { (TCHAR)(_T('A') + i), _T(':'), _T('\\'), _T('\0') };

        if( GetDriveType(tszRoot) != DRIVE_FIXED ) // 고정 드라이브만 (원격/이동식/CD-ROM 제외)
        {
            continue;
        }

        HWINFO_DRIVE* pDrive = new HWINFO_DRIVE();
        _tcscpy_s(pDrive->m_tszName, _countof(pDrive->m_tszName), tszRoot);

        TCHAR fsName[32] = { 0 };
        if( GetVolumeInformation(tszRoot, NULL, 0, NULL, NULL, NULL, fsName, _countof(fsName)) )
        {
            _tcscpy_s(pDrive->m_tszFileSystem, _countof(pDrive->m_tszFileSystem), fsName);
        }

        ULARGE_INTEGER freeAvail, total, totalFree;
        if( GetDiskFreeSpaceEx(tszRoot, &freeAvail, &total, &totalFree) )
        {
            pDrive->m_nTotalSpace = (__int64)total.QuadPart;
            pDrive->m_nFreeSpace = (__int64)totalFree.QuadPart;
            m_Drives.m_nTotalSpace += pDrive->m_nTotalSpace;
            m_Drives.m_nFreeSpace += pDrive->m_nFreeSpace;
        }

        m_sDriveArray.push_back(pDrive);
        m_Drives.m_dwDriveCount++;
    }

    return !m_sDriveArray.empty();
}

//***************************************************************************
// CVideoCardInfo
//***************************************************************************

CVideoCardInfo::CVideoCardInfo()
{
}

CVideoCardInfo::~CVideoCardInfo()
{
    for( HWINFO_VIDEOCARD* p : m_sVideoCardArray )
    {
        delete p;
    }
    m_sVideoCardArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 시스템에 장착된 그래픽 카드 정보를 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_DISPLAY로 열거하고, 드라이버 레지스트리 값
//         HardwareInformation.qwMemorySize로 VRAM 크기(MB, 원본과 단위 통일)를
//         추가로 읽습니다. Manufacturer/HardwareId는 HWINFO_VIDEOCARD의 Sm 전용 필드.
//***************************************************************************
BOOL CVideoCardInfo::GetInformation()
{
    for( HWINFO_VIDEOCARD* p : m_sVideoCardArray )
    {
        delete p;
    }
    m_sVideoCardArray.clear();

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_DISPLAY, NULL, NULL, DIGCF_PRESENT);
    if( hDevInfo == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for( DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++ )
    {
        HWINFO_VIDEOCARD* pCard = new HWINFO_VIDEOCARD();
        char buffer[512];

        if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
        {
            CopyToTChar(pCard->m_tszDescription, _countof(pCard->m_tszDescription), buffer);
        }
        else if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
        {
            CopyToTChar(pCard->m_tszDescription, _countof(pCard->m_tszDescription), buffer);
        }

        if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
        {
            CopyToTChar(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), buffer);
        }

        if( SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_MFG, NULL, (PBYTE)buffer, sizeof(buffer), NULL) )
        {
            CopyToTChar(pCard->m_tszManufacturer, _countof(pCard->m_tszManufacturer), buffer);
        }

        // VRAM 크기 (드라이버 레지스트리 값 HardwareInformation.qwMemorySize, Byte -> MB 변환)
        HKEY hDrvKey = SetupDiOpenDevRegKey(hDevInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if( hDrvKey != INVALID_HANDLE_VALUE )
        {
            DWORD64 qwMem = 0;
            DWORD dataLen = sizeof(qwMem);
            DWORD regType = 0;
            if( RegQueryValueExA(hDrvKey, "HardwareInformation.qwMemorySize", NULL, &regType, (LPBYTE)&qwMem, &dataLen) == ERROR_SUCCESS && regType == REG_QWORD )
            {
                pCard->m_lMemorySize = (long)(qwMem / (1024 * 1024));
            }
            RegCloseKey(hDrvKey);
        }

        m_sVideoCardArray.push_back(pCard);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return !m_sVideoCardArray.empty();
}


//***************************************************************************
// CSoundCardInfo
//***************************************************************************

CSoundCardInfo::CSoundCardInfo()
{
}

CSoundCardInfo::~CSoundCardInfo()
{
    for( HWINFO_SOUNDCARD* p : m_sSoundCardArray )
    {
        delete p;
    }
    m_sSoundCardArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 오디오 장치 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_MEDIA로 열거한 뒤 Description/Manufacturer를 HWINFO_SOUNDCARD의
//         ProductName/CompanyName 필드로 옮겨 담습니다 (원본 WMI 버전과 필드명이 다름).
//***************************************************************************
BOOL CSoundCardInfo::GetInformation()
{
    for( HWINFO_SOUNDCARD* p : m_sSoundCardArray )
    {
        delete p;
    }
    m_sSoundCardArray.clear();

    std::vector<RawDeviceInfo> raw;
    if( !EnumRawDevicesByClass(GUID_DEVCLASS_MEDIA, raw) )
    {
        return FALSE;
    }

    for( const RawDeviceInfo& r : raw )
    {
        HWINFO_SOUNDCARD* pCard = new HWINFO_SOUNDCARD();
        _tcsncpy_s(pCard->m_tszProductName, _countof(pCard->m_tszProductName), r.desc, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszCompanyName, _countof(pCard->m_tszCompanyName), r.mfg, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), r.hwid, _TRUNCATE);
        m_sSoundCardArray.push_back(pCard);
    }

    return !m_sSoundCardArray.empty();
}

//***************************************************************************
// CNetworkCardInfo
//***************************************************************************

CNetworkCardInfo::CNetworkCardInfo()
{
}

CNetworkCardInfo::~CNetworkCardInfo()
{
    for( HWINFO_NETWORKCARD* p : m_sNetworkCardArray )
    {
        delete p;
    }
    m_sNetworkCardArray.clear();
}

//***************************************************************************
// @brief GetAdaptersAddresses를 통해 네트워크 어댑터 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail 어댑터 Description과 MAC 주소(HWINFO_NETWORKCARD의 Sm 전용 필드
//         m_tszHardwareId 자리)를 채웁니다.
//***************************************************************************
BOOL CNetworkCardInfo::GetInformation()
{
    for( HWINFO_NETWORKCARD* p : m_sNetworkCardArray )
    {
        delete p;
    }
    m_sNetworkCardArray.clear();

    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &bufLen);
    if( bufLen == 0 )
    {
        return FALSE;
    }

    std::vector<BYTE> buf(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    if( GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &bufLen) != NO_ERROR )
    {
        return FALSE;
    }

    for( PIP_ADAPTER_ADDRESSES p = pAddresses; p != NULL; p = p->Next )
    {
        HWINFO_NETWORKCARD* pCard = new HWINFO_NETWORKCARD();

#ifdef UNICODE
        _tcsncpy_s(pCard->m_tszDescription, _countof(pCard->m_tszDescription), p->Description, _TRUNCATE);
#else
        WideCharToMultiByte(CP_UTF8, 0, p->Description, -1,
            pCard->m_tszDescription, sizeof(pCard->m_tszDescription), NULL, NULL);
#endif

        if( p->PhysicalAddressLength == 6 )
        {
            _stprintf_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId),
                _T("%02X-%02X-%02X-%02X-%02X-%02X"),
                p->PhysicalAddress[0], p->PhysicalAddress[1], p->PhysicalAddress[2],
                p->PhysicalAddress[3], p->PhysicalAddress[4], p->PhysicalAddress[5]);
        }

        m_sNetworkCardArray.push_back(pCard);
    }

    return TRUE;
}


//***************************************************************************
// CCdromInfo
//***************************************************************************

CCdromInfo::CCdromInfo()
{
}

CCdromInfo::~CCdromInfo()
{
    for( HWINFO_CDROM* p : m_sCdromArray )
    {
        delete p;
    }
    m_sCdromArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 장착된 CD-ROM 드라이브 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_CDROM으로 열거. HWINFO_CDROM::m_tszName(드라이브 문자)은
//         미구현이라 빈 문자열로 남습니다.
//***************************************************************************
BOOL CCdromInfo::GetInformation()
{
    for( HWINFO_CDROM* p : m_sCdromArray )
    {
        delete p;
    }
    m_sCdromArray.clear();

    std::vector<RawDeviceInfo> raw;
    if( !EnumRawDevicesByClass(GUID_DEVCLASS_CDROM, raw) )
    {
        return FALSE;
    }

    for( const RawDeviceInfo& r : raw )
    {
        HWINFO_CDROM* pCard = new HWINFO_CDROM();
        _tcsncpy_s(pCard->m_tszDescription, _countof(pCard->m_tszDescription), r.desc, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszManufacturer, _countof(pCard->m_tszManufacturer), r.mfg, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), r.hwid, _TRUNCATE);
        m_sCdromArray.push_back(pCard);
    }

    return !m_sCdromArray.empty();
}


//***************************************************************************
// CKeyBoardInfo
//***************************************************************************

CKeyBoardInfo::CKeyBoardInfo()
{
}

CKeyBoardInfo::~CKeyBoardInfo()
{
    for( HWINFO_KEYBOARD* p : m_sKeyBoardArray )
    {
        delete p;
    }
    m_sKeyBoardArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 키보드 장치 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_KEYBOARD로 열거. HWINFO_KEYBOARD::m_tszType(GetKeyboardType
//         기반 유형 판별)은 미구현이라 빈 문자열로 남습니다.
//***************************************************************************
BOOL CKeyBoardInfo::GetInformation()
{
    for( HWINFO_KEYBOARD* p : m_sKeyBoardArray )
    {
        delete p;
    }
    m_sKeyBoardArray.clear();

    std::vector<RawDeviceInfo> raw;
    if( !EnumRawDevicesByClass(GUID_DEVCLASS_KEYBOARD, raw) )
    {
        return FALSE;
    }

    for( const RawDeviceInfo& r : raw )
    {
        HWINFO_KEYBOARD* pCard = new HWINFO_KEYBOARD();
        _tcsncpy_s(pCard->m_tszDescription, _countof(pCard->m_tszDescription), r.desc, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), r.hwid, _TRUNCATE);
        m_sKeyBoardArray.push_back(pCard);
    }

    return !m_sKeyBoardArray.empty();
}


//***************************************************************************
// CMouseInfo
//***************************************************************************

CMouseInfo::CMouseInfo()
{
}

CMouseInfo::~CMouseInfo()
{
    for( HWINFO_MOUSE* p : m_sMouseArray )
    {
        delete p;
    }
    m_sMouseArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 마우스 장치 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_MOUSE로 열거. HWINFO_MOUSE::m_tszName은 미구현이라
//         빈 문자열로 남습니다.
//***************************************************************************
BOOL CMouseInfo::GetInformation()
{
    for( HWINFO_MOUSE* p : m_sMouseArray )
    {
        delete p;
    }
    m_sMouseArray.clear();

    std::vector<RawDeviceInfo> raw;
    if( !EnumRawDevicesByClass(GUID_DEVCLASS_MOUSE, raw) )
    {
        return FALSE;
    }

    for( const RawDeviceInfo& r : raw )
    {
        HWINFO_MOUSE* pCard = new HWINFO_MOUSE();
        _tcsncpy_s(pCard->m_tszDescription, _countof(pCard->m_tszDescription), r.desc, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszManufacturer, _countof(pCard->m_tszManufacturer), r.mfg, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), r.hwid, _TRUNCATE);
        m_sMouseArray.push_back(pCard);
    }

    return !m_sMouseArray.empty();
}


//***************************************************************************
// CMonitorInfo
//***************************************************************************

CMonitorInfo::CMonitorInfo()
{
}

CMonitorInfo::~CMonitorInfo()
{
    for( HWINFO_MONITOR* p : m_sMonitorArray )
    {
        delete p;
    }
    m_sMonitorArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 연결된 모니터 장치 목록을 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GUID_DEVCLASS_MONITOR로 열거합니다.
//***************************************************************************
BOOL CMonitorInfo::GetInformation()
{
    for( HWINFO_MONITOR* p : m_sMonitorArray )
    {
        delete p;
    }
    m_sMonitorArray.clear();

    std::vector<RawDeviceInfo> raw;
    if( !EnumRawDevicesByClass(GUID_DEVCLASS_MONITOR, raw) )
    {
        return FALSE;
    }

    for( const RawDeviceInfo& r : raw )
    {
        HWINFO_MONITOR* pCard = new HWINFO_MONITOR();
        _tcsncpy_s(pCard->m_tszDescription, _countof(pCard->m_tszDescription), r.desc, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszManufacturer, _countof(pCard->m_tszManufacturer), r.mfg, _TRUNCATE);
        _tcsncpy_s(pCard->m_tszHardwareId, _countof(pCard->m_tszHardwareId), r.hwid, _TRUNCATE);
        m_sMonitorArray.push_back(pCard);
    }

    return !m_sMonitorArray.empty();
}
