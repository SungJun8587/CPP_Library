
//***************************************************************************
// SmbiosHardwareInfo.cpp: implementation of the SMBIOS/IOCTL-based hardware information classes.
//
//***************************************************************************

#include "pch.h"
#include "SmbiosHardwareInfo.h"

#include <winioctl.h>

// GetSmbiosString64.asm
extern "C" {
    int smbios_cache_open_64();
    void smbios_cache_close_64();
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
//         네 번의 호출을 smbios_cache_open_64/close_64로 감싸서 SMBIOS 테이블을
//         한 번만 fetch합니다(원래는 호출마다 매번 새로 가져왔음).
//***************************************************************************
BOOL CSmbiosBiosInfo::GetInformation()
{
    char buf[128];
    BOOL bAny = FALSE;

    smbios_cache_open_64();

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

    smbios_cache_close_64();

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
//         Version/SerialNumber를 읽음. smbios_cache_open_64/close_64로 네 번의
//         호출이 SMBIOS 테이블을 한 번만 fetch하도록 묶습니다.
//***************************************************************************
BOOL CSmbiosMainBoardInfo::GetInformation()
{
    char buf[128];
    BOOL bAny = FALSE;

    smbios_cache_open_64();

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

    smbios_cache_close_64();

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
//         슬롯이 N개면 원래 최대 4N+1번 SMBIOS 테이블을 새로 fetch했는데,
//         smbios_cache_open_64/close_64로 감싸서 한 번만 fetch하도록 바꿨습니다.
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

    smbios_cache_open_64();

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
        if( get_smbios_string_instance_64(17, 0x17, i, buf, sizeof(buf)) ) // Manufacturer
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

    smbios_cache_close_64();

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