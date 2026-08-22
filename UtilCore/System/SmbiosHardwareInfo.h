
//***************************************************************************
// SmbiosHardwareInfo.h : interface for the SMBIOS-based hardware information classes.
//
//***************************************************************************

#ifndef __SMBIOSHARDWAREINFO_H__
#define __SMBIOSHARDWAREINFO_H__

#include <tchar.h>
#include <vector>

//***************************************************************************
// 하드웨어 정보 데이터 구조체(HWINFO_BIOS 등)는 SmHardwareInfo.h(non-WMI 버전)와
// 공유하기 위해 HwInfoStructs.h로 이동했습니다.
//***************************************************************************
#ifndef __HWINFOSTRUCTS_H__
#include <System/HwInfoStructs.h>
#endif

//***************************************************************************
// @class CSmbiosBiosInfo
// @brief SMBIOS Type 0(BIOS Information)/Type 1(System Information)을 직접 읽어
//        BIOS 정보를 수집하는 클래스입니다. CBiosInfo(WMI 버전)의 non-WMI 대응.
// @details HWINFO_BIOS를 그대로 채우되, WMI 전용 파생 필드(SmVersion, IdentificationCode)는
//          SMBIOS로 직접 못 얻어 빈 문자열로 남습니다. GetSerialNumber()는 SMBIOS
//          Type1(System)의 시리얼 (보통 "BIOS 시리얼"로 통용).
//***************************************************************************
class CSmbiosBiosInfo
{
public:
    CSmbiosBiosInfo();
    ~CSmbiosBiosInfo();

    //***************************************************************************
    // @brief SMBIOS Type 0/1을 직접 조회하여 BIOS 정보를 채웁니다.
    // @return BOOL 하나 이상의 필드를 수집했으면 TRUE
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 BIOS 제조사 이름을 반환합니다.
    // @return const TCHAR* 제조사 이름 문자열 포인터
    //***************************************************************************
    const TCHAR* GetManufacturer() const
    {
        return m_Bios.m_tszManufacturer;
    }

    //***************************************************************************
    // @brief 수집된 BIOS 버전을 반환합니다.
    // @return const TCHAR* BIOS 버전 문자열 포인터
    //***************************************************************************
    const TCHAR* GetVersion() const
    {
        return m_Bios.m_tszVersion;
    }

    //***************************************************************************
    // @brief 수집된 BIOS 출시일을 반환합니다.
    // @return const TCHAR* 출시일 문자열 포인터
    //***************************************************************************
    const TCHAR* GetReleaseDate() const
    {
        return m_Bios.m_tszReleaseDate;
    }

    //***************************************************************************
    // @brief 수집된 시스템 시리얼 번호를 반환합니다 (SMBIOS Type1).
    // @return const TCHAR* 시리얼 번호 문자열 포인터
    //***************************************************************************
    const TCHAR* GetSerialNumber() const
    {
        return m_Bios.m_tszSerialNumber;
    }

private:
    HWINFO_BIOS m_Bios;
};


//======================= MainBoard =======================

//***************************************************************************
// @class CSmbiosMainBoardInfo
// @brief SMBIOS Type 2(Base Board Information)를 직접 읽는 클래스입니다.
//        CMainBoardInfo(WMI 버전)의 non-WMI 대응.
// @details GetDescription()에 정확히 대응하는 SMBIOS 필드가 없어 대신
//          GetVersion()(HWINFO_MAINBOARD의 Sm 전용 필드, Type2 offset 0x06)을 제공함.
//***************************************************************************
class CSmbiosMainBoardInfo
{
public:
    CSmbiosMainBoardInfo();
    ~CSmbiosMainBoardInfo();

    //***************************************************************************
    // @brief SMBIOS Type 2를 직접 조회하여 메인보드 정보를 채웁니다.
    // @return BOOL 하나 이상의 필드를 수집했으면 TRUE
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 메인보드 제조사 이름을 반환합니다.
    // @return const TCHAR* 제조사 이름 문자열 포인터
    //***************************************************************************
    const TCHAR* GetManufacturer() const
    {
        return m_MainBoard.m_tszManufacturer;
    }

    //***************************************************************************
    // @brief 수집된 메인보드 제품명을 반환합니다.
    // @return const TCHAR* 제품명 문자열 포인터
    //***************************************************************************
    const TCHAR* GetProduct() const
    {
        return m_MainBoard.m_tszProduct;
    }

    //***************************************************************************
    // @brief 수집된 메인보드 버전을 반환합니다 (HWINFO_MAINBOARD의 Sm 전용 필드).
    // @return const TCHAR* 버전 문자열 포인터
    //***************************************************************************
    const TCHAR* GetVersion() const
    {
        return m_MainBoard.m_tszVersion;
    }

    //***************************************************************************
    // @brief 수집된 메인보드 시리얼 번호를 반환합니다.
    // @return const TCHAR* 시리얼 번호 문자열 포인터
    //***************************************************************************
    const TCHAR* GetSerialNumber() const
    {
        return m_MainBoard.m_tszSerialNumber;
    }

private:
    HWINFO_MAINBOARD m_MainBoard;
};


//======================= Memory (RAM) =======================

//***************************************************************************
// @class CSmbiosMemoryInfo
// @brief GlobalMemoryStatusEx(전체 통계) + SMBIOS Type17(슬롯별 정보)로 메모리 정보를
//        수집하는 클래스입니다. CMemoryInfo(WMI 버전)의 non-WMI 대응.
// @details 전체 통계는 원래도 WMI가 아니라 GlobalMemoryStatusEx 기반이라 WMI 버전과
//          값이 사실상 동일함. m_Memory에는 Byte 단위로 직접 저장하므로(WMI 버전은
//          KB 저장 후 getter에서 *1024) 이 클래스의 getter는 *1024를 하지 않음.
//          슬롯별 배열(HWINFO_RAM)만 SMBIOS 의존, Manufacturer는 Sm 전용 필드.
//***************************************************************************
class CSmbiosMemoryInfo
{
public:
    CSmbiosMemoryInfo();
    ~CSmbiosMemoryInfo();

    //***************************************************************************
    // @brief GlobalMemoryStatusEx 및 SMBIOS Type17을 통해 전체 메모리 상태 및
    //        RAM 모듈 리스트를 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 감지된 RAM 모듈의 총 개수를 반환합니다.
    // @return DWORD 장착된 RAM 개수
    //***************************************************************************
    DWORD GetRamCount() const
    {
        return (DWORD)m_sRamArray.size();
    }

    //***************************************************************************
    // @brief 시스템 전체 물리 메모리 용량을 반환합니다.
    // @return const __int64 전체 메모리 크기 (Byte)
    //***************************************************************************
    const __int64 GetTotalMemSize() const
    {
        return m_Memory.m_nTotalMemSize;
    }

    //***************************************************************************
    // @brief 현재 사용 가능한 실제 물리 메모리 용량을 반환합니다.
    // @return const __int64 가용 물리 메모리 크기 (Byte)
    //***************************************************************************
    const __int64 GetPhysicalMemSize() const
    {
        return m_Memory.m_nPhysicalMemSize;
    }

    //***************************************************************************
    // @brief 현재 점유하여 사용 중인 물리 메모리 용량을 반환합니다.
    // @return const __int64 사용 중인 메모리 크기 (Byte)
    //***************************************************************************
    const __int64 GetUseMemSize() const
    {
        return m_Memory.m_nTotalMemSize - m_Memory.m_nPhysicalMemSize;
    }

    //***************************************************************************
    // @brief 전체 물리 메모리 대비 현재 사용량의 비율을 계산하여 반환합니다.
    // @return const double 메모리 사용율 (0.0 ~ 1.0). 전체 메모리 크기를 알 수 없는
    //         경우(0인 경우) 0-나눗셈을 피하기 위해 0.0을 반환합니다.
    //***************************************************************************
    const double GetPercentUsedRam() const
    {
        if( m_Memory.m_nTotalMemSize == 0 )
        {
            return 0.0;
        }
        return (double)(m_Memory.m_nTotalMemSize - m_Memory.m_nPhysicalMemSize) / (double)m_Memory.m_nTotalMemSize;
    }

    //***************************************************************************
    // @brief 시스템 전체 가상 메모리 용량을 반환합니다.
    // @return const __int64 총 가상 메모리 크기 (Byte)
    //***************************************************************************
    const __int64 GetTotalVirtualMemSize() const
    {
        return m_Memory.m_nTotalVirtualMemSize;
    }

    //***************************************************************************
    // @brief 여유 가상 메모리 용량을 반환합니다.
    // @return const __int64 가용 가상 메모리 크기 (Byte)
    //***************************************************************************
    const __int64 GetFreeVirtualMemSize() const
    {
        return m_Memory.m_nFreeVirtualMemSize;
    }

    //***************************************************************************
    // @brief 총 페이징 파일 크기를 반환합니다.
    // @return const __int64 총 페이징 파일 크기 (Byte)
    //***************************************************************************
    const __int64 GetTotalPageFile() const
    {
        return m_Memory.m_nTotalPageFileSize;
    }

    //***************************************************************************
    // @brief 남은 페이징 파일 크기를 반환합니다.
    // @return const __int64 여유 페이징 파일 크기 (Byte)
    //***************************************************************************
    const __int64 GetFreePageFile() const
    {
        return m_Memory.m_nFreePageFileSize;
    }

    //***************************************************************************
    // @brief 슬롯별 장착된 RAM 모듈 정보 포인터 배열을 반환합니다.
    // @return const std::vector<HWINFO_RAM*>* RAM 정보 구조체 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_RAM*>* GetRamArray() const
    {
        return &m_sRamArray;
    }

private:
    HWINFO_MEMORY m_Memory;
    std::vector<HWINFO_RAM*> m_sRamArray;
};

//***************************************************************************
// @class CHdDiskInfo
// @brief IOCTL_STORAGE_QUERY_PROPERTY로 PhysicalDrive0..N을 순회 조회하는 클래스입니다.
//        CHdDiskInfo(WMI 버전)의 non-WMI 대응.
// @details 관리자 권한 필요 - CreateFileA(\\.\PhysicalDriveN, GENERIC_READ|GENERIC_WRITE)가
//          비관리자 프로세스에서는 항상 실패하므로 비관리자 환경에서는 빈 배열이 됨.
//          SerialNumber/BusType은 HWINFO_HDDISK의 Sm 전용 필드.
//***************************************************************************
class CHdDiskInfo
{
public:
    CHdDiskInfo();
    ~CHdDiskInfo();

    //***************************************************************************
    // @brief IOCTL_STORAGE_QUERY_PROPERTY를 통해 장착된 모든 물리 디스크 정보를 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 물리 디스크 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_HDDISK*>* 물리 디스크 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_HDDISK*>* GetHdDiskArray() const
    {
        return &m_sHdDiskArray;
    }

private:
    std::vector<HWINFO_HDDISK*> m_sHdDiskArray;
};


//======================= PCI 버스 장치 =======================
// HardwareInfo.h에는 대응하는 클래스/구조체가 없는 신규 추가라 HwInfoStructs.h로
// 옮기지 않고 이 파일에 그대로 둠 (공유할 WMI 대응 타입 자체가 없음).
// BoardInfo.cpp/h는 CSmbiosBiosInfo/CSmbiosMainBoardInfo와 완전히 동일한 SMBIOS Type0/2 데이터를
// 다른 방식(순수 C++)으로 중복 구현한 것이라 통합하지 않음(폐기). PciInfo.cpp/h는 Bus/Device/
// Function, 숫자 Vendor/Device ID, Class Code 등 기존에 없던 정보라 통합함. 단, pci_parse_vendor_name/
// pci_classify_device(asm, 원본 미제공)는 각각 소규모 내장 벤더 테이블 / 인라인 C++ 로직으로 대체.

enum class SmPciDeviceClass { Unknown, GPU, NVMe };

//***************************************************************************
// @struct SMHWINFO_PCIDEVICE
// @brief PCI 버스 상의 개별 장치 정보. PciInfo.h의 PciDeviceInfo와 동일한 목적.
//***************************************************************************
struct SMHWINFO_PCIDEVICE
{
    SMHWINFO_PCIDEVICE()
    {
        bus = device = function = 0;
        vendorId = deviceId = 0;
        baseClass = subClass = progIf = 0;
        type = SmPciDeviceClass::Unknown;
        m_tszVendorName[0] = _T('\0');
        m_tszDescription[0] = _T('\0');
    }

    BYTE  bus;
    BYTE  device;
    BYTE  function;
    WORD  vendorId;
    WORD  deviceId;
    BYTE  baseClass;
    BYTE  subClass;
    BYTE  progIf;
    SmPciDeviceClass type;         // ClassifyPci()의 결과 (GPU/NVMe/Unknown)
    TCHAR m_tszVendorName[32];     // 내장 소규모 테이블 매칭 결과 - 없으면 빈 문자열(vendorId로 직접 확인 필요)
    TCHAR m_tszDescription[128];   // SPDRP_DEVICEDESC
};

//***************************************************************************
// @class CSmPciInfo
// @brief SetupAPI로 PCI 버스의 모든 장치를 열거하는 클래스입니다. HardwareInfo.h에는
//        직접 대응하는 클래스가 없는 신규 추가 - CSmVideoCardInfo 등이 주는 드라이버
//        문자열보다 신뢰도 높은 숫자 Vendor/Device ID, 버스 위치, Class Code를 제공함.
//***************************************************************************
class CSmPciInfo
{
public:
    CSmPciInfo();
    ~CSmPciInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 PCI 버스에 연결된 모든 장치를 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 PCI 장치 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<SMHWINFO_PCIDEVICE*>* PCI 장치 포인터 벡터
    //***************************************************************************
    const std::vector<SMHWINFO_PCIDEVICE*>* GetPciDeviceArray() const
    {
        return &m_sPciArray;
    }

private:
    std::vector<SMHWINFO_PCIDEVICE*> m_sPciArray;
};

#endif // ndef __SMBIOSHARDWAREINFO_H__