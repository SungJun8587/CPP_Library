
//***************************************************************************
// SmbiosHardwareInfo.cpp: implementation of the SMBIOS-based hardware information classes.
//
//***************************************************************************

#include "pch.h"
#include "SmbiosHardwareInfo.h"

#include <winioctl.h>
#include <setupapi.h>

// GetSmbiosString64.asm
extern "C" {
    int get_smbios_instance_count_64(int type);
    int get_smbios_string_instance_64(int type, int offset, int instance, char* buffer, unsigned int buffer_size);
    int get_smbios_word_64(int type, int offset, int instance, unsigned short* out_value);
}

// MemDiskDetail64.asm
extern "C" {
    int get_disk_detail_info_64(unsigned int drive_index, void* out_buffer, unsigned int buffer_size);
    int get_disk_total_bytes_64(unsigned int drive_index, unsigned __int64* out_total_bytes);
}

namespace
{
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
}


//***************************************************************************
// CSmbiosBiosInfo
//***************************************************************************

CSmbiosBiosInfo::CSmbiosBiosInfo()
{
}

CSmbiosBiosInfo::~CSmbiosBiosInfo()
{
}

//***************************************************************************
// @brief SMBIOS Type 0/1을 직접 조회하여 BIOS 정보를 채웁니다.
// @return BOOL 하나 이상의 필드를 수집했으면 TRUE
// @detail get_smbios_string_instance_64(GetSmbiosString64.asm)로 Type0(BIOS)의
//         Manufacturer/Version/ReleaseDate, Type1(System)의 SerialNumber를 읽음.
//***************************************************************************
BOOL CSmbiosBiosInfo::GetInformation()
{
    char buf[128];
    BOOL bAny = FALSE;

    // Type 0 (BIOS Information)
    if( get_smbios_string_instance_64(0, 0x04, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_Bios.m_tszManufacturer, _countof(m_Bios.m_tszManufacturer), buf);
        bAny = TRUE;
    }
    if( get_smbios_string_instance_64(0, 0x05, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_Bios.m_tszVersion, _countof(m_Bios.m_tszVersion), buf);
        bAny = TRUE;
    }
    if( get_smbios_string_instance_64(0, 0x08, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_Bios.m_tszReleaseDate, _countof(m_Bios.m_tszReleaseDate), buf);
        bAny = TRUE;
    }

    // Type 1 (System Information) - Serial Number
    if( get_smbios_string_instance_64(1, 0x07, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_Bios.m_tszSerialNumber, _countof(m_Bios.m_tszSerialNumber), buf);
        bAny = TRUE;
    }

    return bAny;
}


//***************************************************************************
// CSmbiosMainBoardInfo
//***************************************************************************

CSmbiosMainBoardInfo::CSmbiosMainBoardInfo()
{
}

CSmbiosMainBoardInfo::~CSmbiosMainBoardInfo()
{
}

//***************************************************************************
// @brief SMBIOS Type 2를 직접 조회하여 메인보드 정보를 채웁니다.
// @return BOOL 하나 이상의 필드를 수집했으면 TRUE
// @detail get_smbios_string_instance_64로 Type2(Base Board)의 Manufacturer/Product/
//         Version/SerialNumber를 읽음.
//***************************************************************************
BOOL CSmbiosMainBoardInfo::GetInformation()
{
    char buf[128];
    BOOL bAny = FALSE;

    // Type 2 (Base Board Information)
    if( get_smbios_string_instance_64(2, 0x04, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_MainBoard.m_tszManufacturer, _countof(m_MainBoard.m_tszManufacturer), buf);
        bAny = TRUE;
    }
    if( get_smbios_string_instance_64(2, 0x05, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_MainBoard.m_tszProduct, _countof(m_MainBoard.m_tszProduct), buf);
        bAny = TRUE;
    }
    if( get_smbios_string_instance_64(2, 0x06, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_MainBoard.m_tszVersion, _countof(m_MainBoard.m_tszVersion), buf);
        bAny = TRUE;
    }
    if( get_smbios_string_instance_64(2, 0x07, 0, buf, sizeof(buf)) )
    {
        CopyToTChar(m_MainBoard.m_tszSerialNumber, _countof(m_MainBoard.m_tszSerialNumber), buf);
        bAny = TRUE;
    }

    return bAny;
}


//***************************************************************************
// CSmbiosMemoryInfo
//***************************************************************************

CSmbiosMemoryInfo::CSmbiosMemoryInfo()
{
}

CSmbiosMemoryInfo::~CSmbiosMemoryInfo()
{
    for( HWINFO_RAM* pRam : m_sRamArray )
    {
        delete pRam;
    }
    m_sRamArray.clear();
}

//***************************************************************************
// @brief GlobalMemoryStatusEx 및 SMBIOS Type17을 통해 전체 메모리 상태 및
//        RAM 모듈 리스트를 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail GlobalMemoryStatusEx로 전체 통계를 m_Memory에 Byte 단위로 직접 저장하고
//         (WMI 버전은 KB 저장), get_smbios_instance_count_64/get_smbios_string_instance_64/
//         get_smbios_word_64로 슬롯별 Locator/Manufacturer/Size/Speed를 구합니다.
//***************************************************************************
BOOL CSmbiosMemoryInfo::GetInformation()
{
    for( HWINFO_RAM* pRam : m_sRamArray )
    {
        delete pRam;
    }
    m_sRamArray.clear();

    BOOL bAny = FALSE;

    // 전체 메모리 통계 - WMI 아님, GlobalMemoryStatusEx (원래도 WMI 버전과 같은 값)
    MEMORYSTATUSEX ms = { sizeof(ms) };
    if( GlobalMemoryStatusEx(&ms) )
    {
        m_Memory.m_nTotalMemSize = (__int64)ms.ullTotalPhys;
        m_Memory.m_nPhysicalMemSize = (__int64)ms.ullAvailPhys;
        m_Memory.m_nTotalVirtualMemSize = (__int64)ms.ullTotalPageFile; // WMI TotalVirtualMemorySize와 개념상 동일값
        m_Memory.m_nFreeVirtualMemSize = (__int64)ms.ullAvailPageFile;
        m_Memory.m_nTotalPageFileSize = (__int64)ms.ullTotalPageFile;
        m_Memory.m_nFreePageFileSize = (__int64)ms.ullAvailPageFile;
        bAny = TRUE;
    }

    // 슬롯별 정보 - SMBIOS Type 17 (Memory Device)
    int count = get_smbios_instance_count_64(17);
    for( int i = 0; i < count; i++ )
    {
        HWINFO_RAM* pRam = new HWINFO_RAM();

        char buf[64] = { 0 };
        if( get_smbios_string_instance_64(17, 0x10, i, buf, sizeof(buf)) ) // Device Locator
        {
            CopyToTChar(pRam->m_tszDeviceLocator, _countof(pRam->m_tszDeviceLocator), buf);
        }
        if( get_smbios_string_instance_64(17, 0x17, i, buf, sizeof(buf)) ) // Manufacturer (Sm 전용 필드)
        {
            CopyToTChar(pRam->m_tszManufacturer, _countof(pRam->m_tszManufacturer), buf);
        }

        unsigned short sizeRaw = 0, speedRaw = 0;
        // Size(0x0C): 0=빈 슬롯, 0x7FFF=Extended Size(0x1C, DWORD, SMBIOS 2.7+) 참조 필요 - 미구현이라 0으로 남김
        if( get_smbios_word_64(17, 0x0C, i, &sizeRaw) && sizeRaw != 0 && sizeRaw != 0x7FFF )
        {
            pRam->m_nCapacity = (__int64)sizeRaw * 1024 * 1024;
        }
        if( get_smbios_word_64(17, 0x15, i, &speedRaw) ) // Speed, MT/s
        {
            pRam->m_dwSpeed = speedRaw;
        }
        // m_dwFormFactor/m_dwMemoryType/m_tszBankLabel/m_tszName/*Desc는 미구현 - 기본값(0/빈 문자열) 유지

        m_sRamArray.push_back(pRam);
        bAny = TRUE;
    }

    return bAny;
}


//***************************************************************************
// CHdDiskInfo
//***************************************************************************

CHdDiskInfo::CHdDiskInfo()
{
}

CHdDiskInfo::~CHdDiskInfo()
{
    for( HWINFO_HDDISK* pDisk : m_sHdDiskArray )
    {
        delete pDisk;
    }
    m_sHdDiskArray.clear();
}

//***************************************************************************
// @brief IOCTL_STORAGE_QUERY_PROPERTY를 통해 장착된 모든 물리 디스크 정보를 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail PhysicalDrive0부터 순차 조회하며 get_disk_detail_info_64가 실패하는
//         시점(해당 인덱스의 디스크가 없거나 관리자 권한 부족)에 중단합니다.
//         SerialNumber/BusType은 HWINFO_HDDISK의 Sm 전용 필드에 저장합니다.
//***************************************************************************
BOOL CHdDiskInfo::GetInformation()
{
    for( HWINFO_HDDISK* pDisk : m_sHdDiskArray )
    {
        delete pDisk;
    }
    m_sHdDiskArray.clear();

    for( unsigned int i = 0; i < 16; i++ )
    {
        BYTE buffer[1024] = { 0 };
        if( !get_disk_detail_info_64(i, buffer, sizeof(buffer)) )
        {
            break; // 해당 인덱스의 PhysicalDrive가 없거나(또는 관리자 권한 부족)
        }

        PSTORAGE_DEVICE_DESCRIPTOR devDesc = (PSTORAGE_DEVICE_DESCRIPTOR)buffer;
        HWINFO_HDDISK* pDisk = new HWINFO_HDDISK();

        TCHAR tszModel[128] = { 0 };
        if( devDesc->VendorIdOffset != 0 && buffer[devDesc->VendorIdOffset] != 0 )
        {
            CopyToTChar(tszModel, _countof(tszModel), (char*)&buffer[devDesc->VendorIdOffset]);
            _tcscat_s(tszModel, _countof(tszModel), _T(" "));
        }
        if( devDesc->ProductIdOffset != 0 && buffer[devDesc->ProductIdOffset] != 0 )
        {
            TCHAR tszProduct[64] = { 0 };
            CopyToTChar(tszProduct, _countof(tszProduct), (char*)&buffer[devDesc->ProductIdOffset]);
            _tcscat_s(tszModel, _countof(tszModel), tszProduct);
        }
        _tcscpy_s(pDisk->m_tszModel, _countof(pDisk->m_tszModel), tszModel);

        if( devDesc->SerialNumberOffset != 0 && buffer[devDesc->SerialNumberOffset] != 0 )
        {
            CopyToTChar(pDisk->m_tszSerialNumber, _countof(pDisk->m_tszSerialNumber),
                (char*)&buffer[devDesc->SerialNumberOffset]);
        }
        else
        {
            _tcscpy_s(pDisk->m_tszSerialNumber, _countof(pDisk->m_tszSerialNumber), _T("N/A"));
        }

        const char* busName = "Unknown";
        switch( devDesc->BusType )
        {
        case BusTypeSata: busName = "SATA"; break;
        case BusTypeNvme: busName = "NVMe"; break;
        case BusTypeUsb:  busName = "USB";  break;
        case BusTypeScsi: busName = "SCSI"; break;
        case BusTypeSas:  busName = "SAS";  break;
        }
        CopyToTChar(pDisk->m_tszBusType, _countof(pDisk->m_tszBusType), busName);

        unsigned __int64 totalBytes = 0;
        get_disk_total_bytes_64(i, &totalBytes);
        pDisk->m_nTotalSize = (__int64)totalBytes;

        m_sHdDiskArray.push_back(pDisk);
    }

    return !m_sHdDiskArray.empty();
}


//***************************************************************************
// CSmPciInfo
//***************************************************************************

namespace {
    struct PciVendorEntry { WORD id; const TCHAR* name; };

    // 확실히 검증된 것 위주로만 등재 (불확실한 ID를 잘못 매핑하는 것보다 빈 문자열이 낫다고 판단).
    // 매칭 안 되면 m_tszVendorName은 빈 문자열 - 호출부는 vendorId(숫자)로 직접 판별 가능.
    const PciVendorEntry g_PciVendorTable[] = {
        { 0x8086, _T("Intel") },
        { 0x8087, _T("Intel") },
        { 0x1022, _T("AMD") },
        { 0x1002, _T("AMD/ATI") },
        { 0x10DE, _T("NVIDIA") },
        { 0x10EC, _T("Realtek") },
        { 0x14E4, _T("Broadcom") },
        { 0x168C, _T("Qualcomm Atheros") },
        { 0x1B21, _T("ASMedia") },
        { 0x1106, _T("VIA Technologies") },
        { 0x1039, _T("SiS") },
        { 0x144D, _T("Samsung") },
        { 0x1344, _T("Micron") },
        { 0x1C5C, _T("SK hynix") },
        { 0x1B4B, _T("Marvell") },
        { 0x15AD, _T("VMware") },
        { 0x80EE, _T("Oracle VirtualBox") },
        { 0x1414, _T("Microsoft (Hyper-V)") },
        { 0x1AB8, _T("Parallels") },
        { 0x1179, _T("Toshiba") },
    };

    //***************************************************************************
    // @brief PCI Vendor ID를 내장 소규모 테이블에서 찾아 회사명 문자열로 변환합니다.
    // @param vendorId [in] PCI Vendor ID
    // @return const TCHAR* 매칭된 회사명, 없으면 빈 문자열
    // @detail PciInfo.h의 pci_parse_vendor_name(asm, 원본 미제공) 대체.
    //***************************************************************************
    const TCHAR* LookupPciVendorName(WORD vendorId)
    {
        for( const auto& e : g_PciVendorTable )
        {
            if( e.id == vendorId )
            {
                return e.name;
            }
        }
        return _T("");
    }

    //***************************************************************************
    // @brief Class Code를 분석하여 디바이스 종류(GPU/NVMe 등)를 분류합니다.
    // @param baseClass [in] Base Class Code
    // @param subClass  [in] Sub Class Code
    // @param progIf    [in] Programming Interface Code (현재 미사용)
    // @return SmPciDeviceClass 분류 결과
    // @detail PciInfo.cpp의 pci_classify_device(asm, 원본 미제공) 대체 - 단순
    //         비교라 asm일 이유가 없어 인라인 C++로 재작성.
    //***************************************************************************
    SmPciDeviceClass ClassifyPci(BYTE baseClass, BYTE subClass, BYTE /*progIf*/)
    {
        if( baseClass == 0x03 ) // Display Controller
        {
            return SmPciDeviceClass::GPU;
        }
        if( baseClass == 0x01 && subClass == 0x08 ) // Mass Storage / NVM
        {
            return SmPciDeviceClass::NVMe;
        }
        return SmPciDeviceClass::Unknown;
    }
}

CSmPciInfo::CSmPciInfo()
{
}

CSmPciInfo::~CSmPciInfo()
{
    for( SMHWINFO_PCIDEVICE* p : m_sPciArray )
    {
        delete p;
    }
    m_sPciArray.clear();
}

//***************************************************************************
// @brief SetupAPI를 통해 PCI 버스에 연결된 모든 장치를 수집합니다.
// @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
// @detail SPDRP_BUSNUMBER/SPDRP_ADDRESS로 Bus:Device:Function을, SPDRP_HARDWAREID
//         문자열 파싱으로 Vendor/Device ID를, SPDRP_COMPATIBLEIDS로 Class Code를
//         읽고 LookupPciVendorName/ClassifyPci로 후처리합니다.
//***************************************************************************
BOOL CSmPciInfo::GetInformation()
{
    for( SMHWINFO_PCIDEVICE* p : m_sPciArray )
    {
        delete p;
    }
    m_sPciArray.clear();

    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, _T("PCI"), NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if( hDevInfo == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for( DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++ )
    {
        SMHWINFO_PCIDEVICE* pDev = new SMHWINFO_PCIDEVICE();

        // Bus / Device / Function
        DWORD busNum = 0, address = 0;
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_BUSNUMBER,
            NULL, (PBYTE)&busNum, sizeof(busNum), NULL) )
        {
            pDev->bus = (BYTE)busNum;
        }
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_ADDRESS,
            NULL, (PBYTE)&address, sizeof(address), NULL) )
        {
            pDev->device = (BYTE)((address >> 16) & 0xFFFF);
            pDev->function = (BYTE)(address & 0xFFFF);
        }

        // Vendor ID / Device ID (Hardware ID 문자열 파싱: "PCI\VEN_10DE&DEV_2487&...")
        TCHAR hwIdBuf[512] = { 0 };
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID,
            NULL, (PBYTE)hwIdBuf, sizeof(hwIdBuf), NULL) )
        {
            unsigned int vId = 0, dId = 0;
            if( _stscanf_s(hwIdBuf, _T("PCI\\VEN_%04X&DEV_%04X"), &vId, &dId) == 2 )
            {
                pDev->vendorId = (WORD)vId;
                pDev->deviceId = (WORD)dId;
            }
        }

        // Class Code (Compatible ID 문자열 파싱: "PCI\CC_030000")
        TCHAR compIdBuf[512] = { 0 };
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_COMPATIBLEIDS,
            NULL, (PBYTE)compIdBuf, sizeof(compIdBuf), NULL) )
        {
            TCHAR* pCC = _tcsstr(compIdBuf, _T("CC_"));
            if( pCC )
            {
                unsigned int ccVal = 0;
                if( _stscanf_s(pCC, _T("CC_%06X"), &ccVal) == 1 )
                {
                    pDev->baseClass = (BYTE)((ccVal >> 16) & 0xFF);
                    pDev->subClass = (BYTE)((ccVal >> 8) & 0xFF);
                    pDev->progIf = (BYTE)(ccVal & 0xFF);
                }
            }
        }

        // 디바이스 설명
        TCHAR descBuf[128] = { 0 };
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
            NULL, (PBYTE)descBuf, sizeof(descBuf), NULL) )
        {
            _tcscpy_s(pDev->m_tszDescription, _countof(pDev->m_tszDescription), descBuf);
        }

        _tcscpy_s(pDev->m_tszVendorName, _countof(pDev->m_tszVendorName), LookupPciVendorName(pDev->vendorId));
        pDev->type = ClassifyPci(pDev->baseClass, pDev->subClass, pDev->progIf);

        m_sPciArray.push_back(pDev);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return !m_sPciArray.empty();
}