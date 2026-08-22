
//***************************************************************************
// HwInfoStructs.h : WmiHardwareInfo(WMI)와 SmHardwareInfo(non-WMI)가 공유하는
//                   하드웨어 정보 데이터 구조체 모음.
//***************************************************************************

#ifndef __HWINFOSTRUCTS_H__
#define __HWINFOSTRUCTS_H__

#include <windows.h>
#include <tchar.h>

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

//***************************************************************************
// @struct  _HWINFO_BIOS
// @brief 시스템 BIOS의 세부 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_BIOS
{
public:
    _HWINFO_BIOS()
    {
        m_tszManufacturer[0] = '\0';
        m_tszSmVersion[0] = '\0';
        m_tszVersion[0] = '\0';
        m_tszIdentificationCode[0] = '\0';
        m_tszSerialNumber[0] = '\0';
        m_tszReleaseDate[0] = '\0';
    }

    TCHAR	m_tszManufacturer[BIOS_MANUFACTURER_STRLEN];            // BIOS 제조사 이름
    TCHAR	m_tszSmVersion[BIOS_SMVERSION_STRLEN];                  // SMBIOS 버전 문자열 (Sm 쪽 미구현 - RawSMBIOSData 헤더 바이트 필요)
    TCHAR	m_tszVersion[BIOS_VERSION_STRLEN];                      // BIOS 버전 번호
    TCHAR	m_tszIdentificationCode[BIOS_IDENTIFICATIONCODE_STRLEN];// BIOS 식별 코드 (Sm 쪽 미구현 - WMI 전용 파생 필드)
    TCHAR	m_tszSerialNumber[BIOS_SERIALNUMBER_STRLEN];            // BIOS 시리얼 번호 (Sm 쪽은 SMBIOS Type1 System Serial)
    TCHAR	m_tszReleaseDate[BIOS_RELEASEDATE_STRLEN];              // BIOS 출시일

} HWINFO_BIOS, * PHWINFO_BIOS;


//***************************************************************************
// @struct  _HWINFO_MAINBOARD
// @brief 메인보드(마더보드)의 제원 및 식별 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MAINBOARD
{
public:
    _HWINFO_MAINBOARD()
    {
        m_tszProduct[0] = '\0';
        m_tszSerialNumber[0] = '\0';
        m_tszManufacturer[0] = '\0';
        m_tszDescription[0] = '\0';
        m_tszVersion[0] = '\0';
    }

    TCHAR	m_tszProduct[MAINBOARD_PRODUCT_STRLEN];          // 메인보드 제품명/모델명
    TCHAR	m_tszSerialNumber[MAINBOARD_SERIALNUMBER_STRLEN];// 메인보드 고유 시리얼 번호
    TCHAR	m_tszManufacturer[MAINBOARD_MANUFACTURER_STRLEN];// 메인보드 제조사 이름
    TCHAR	m_tszDescription[MAINBOARD_DESCRIPTION_STRLEN];  // 메인보드 장치 상세 설명 (Sm 쪽 미구현 - 대신 m_tszVersion 사용)
    TCHAR	m_tszVersion[128];                                // [Sm 전용] SMBIOS Type2 offset 0x06. WMI 쪽은 항상 빈 문자열.

} HWINFO_MAINBOARD, * PHWINFO_MAINBOARD;


//***************************************************************************
// @struct  _HWINFO_RAM
// @brief 개별 RAM 모듈 슬롯의 규격 및 속성 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_RAM
{
public:
    _HWINFO_RAM()
    {
        m_nCapacity = 0;
        m_dwFormFactor = 0;
        m_dwMemoryType = 0;
        m_dwSpeed = 0;

        m_tszBankLabel[0] = '\0';
        m_tszName[0] = '\0';
        m_tszDeviceLocator[0] = '\0';
        m_tszFormFactorDesc[0] = '\0';
        m_tszMemoryTypeDesc[0] = '\0';
        m_tszManufacturer[0] = '\0';
    }

    __int64		m_nCapacity;                                    // 개별 메모리 용량 (Byte)
    DWORD		m_dwFormFactor;                                 // SMBIOS FormFactor ID 코드 (Sm 쪽 미구현)
    DWORD		m_dwMemoryType;                                 // SMBIOS MemoryType ID 코드 (Sm 쪽 미구현)
    DWORD		m_dwSpeed;                                      // 메모리 동작 속도 (MHz/MT/s)
    TCHAR		m_tszBankLabel[RAM_BANKLABEL_STRLEN];           // 메모리 은행 레이블 (Sm 쪽 미구현)
    TCHAR		m_tszName[RAM_NAME_STRLEN];                     // 메모리 장치 이름 (Sm 쪽 미구현)
    TCHAR		m_tszDeviceLocator[RAM_DEVICELOCATOR_STRLEN];   // 메인보드 내 슬롯 위치
    TCHAR		m_tszFormFactorDesc[RAM_FORMFACTORDESC_STRLEN]; // 폼팩터 문자열 설명 (Sm 쪽 미구현)
    TCHAR		m_tszMemoryTypeDesc[RAM_MEMORYTYPEDESC_STRLEN]; // 메모리 타입 문자열 설명 (Sm 쪽 미구현)
    TCHAR		m_tszManufacturer[64];                          // [Sm 전용] SMBIOS Type17 offset 0x17. WMI 쪽은 항상 빈 문자열.

} HWINFO_RAM, * PHWINFO_RAM;


//***************************************************************************
// @struct  _HWINFO_MEMORY
// @brief 시스템 전반의 물리 및 가상 메모리 통계 수치를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MEMORY
{
public:
    _HWINFO_MEMORY()
    {
        m_dwRamCount = 0;
        m_nTotalMemSize = 0;
        m_nPhysicalMemSize = 0;
        m_nTotalVirtualMemSize = 0;
        m_nFreeVirtualMemSize = 0;
        m_nTotalPageFileSize = 0;
        m_nFreePageFileSize = 0;
    }

    DWORD		m_dwRamCount;          // 감지된 RAM 모듈의 개수
    __int64		m_nTotalMemSize;       // 전체 장착된 물리 메모리 총량 (Byte)
    __int64		m_nPhysicalMemSize;    // 현재 사용 가능한 물리 메모리 크기 (WMI: KB, Sm: Byte - 각 클래스 getter에서 단위 처리)
    __int64		m_nTotalVirtualMemSize;// 총 가상 메모리 크기 (WMI: KB, Sm: Byte)
    __int64		m_nFreeVirtualMemSize; // 여유 가상 메모리 크기 (WMI: KB, Sm: Byte)
    __int64		m_nTotalPageFileSize;  // 총 페이징 파일 크기 (WMI: KB, Sm: Byte)
    __int64		m_nFreePageFileSize;   // 여유 페이징 파일 크기 (WMI: KB, Sm: Byte)

} HWINFO_MEMORY, * PHWINFO_MEMORY;


//***************************************************************************
// @struct  _HWINFO_HDDISK
// @brief 물리적 하드디스크/SSD 스토리지의 기본 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_HDDISK
{
public:
    _HWINFO_HDDISK()
    {
        m_nTotalSize = 0;

        m_tszModel[0] = '\0';
        m_tszName[0] = '\0';
        m_tszManufacturer[0] = '\0';
        m_tszDescription[0] = '\0';
        m_tszSerialNumber[0] = '\0';
        m_tszBusType[0] = '\0';
    }

    __int64		m_nTotalSize;                             // 물리 디스크 전체 저장 용량 (Byte)

    TCHAR	m_tszModel[HDDISK_MODEL_STRLEN];              // 스토리지 모델명
    TCHAR	m_tszName[HDDISK_NAME_STRLEN];                // 디스크 장치 식별 이름 (Sm 쪽 미구현)
    TCHAR	m_tszManufacturer[HDDISK_MANUFACTURER_STRLEN];// 스토리지 제조사 이름 (Sm 쪽 미구현 - Model에 벤더가 포함됨)
    TCHAR	m_tszDescription[HDDISK_DESCRIPTION_STRLEN];  // 스토리지 인터페이스/설명 (Sm 쪽 미구현)
    TCHAR	m_tszSerialNumber[64];                         // [Sm 전용] IOCTL SerialNumberOffset. WMI 쪽은 항상 빈 문자열.
    TCHAR	m_tszBusType[16];                              // [Sm 전용] 예: NVMe/SATA/USB. WMI 쪽은 항상 빈 문자열.

} HWINFO_HDDISK, * PHWINFO_HDDISK;


//***************************************************************************
// @struct  _HWINFO_DRIVE
// @brief 논리 파티션 드라이브(C:, D: 등)의 용량 및 파일 시스템 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_DRIVE
{
public:
    _HWINFO_DRIVE()
    {
        m_nTotalSpace = 0;
        m_nFreeSpace = 0;

        m_tszName[0] = '\0';
        m_tszFileSystem[0] = '\0';
    }

    __int64		m_nTotalSpace;                      // 논리 드라이브 전체 용량 (Byte)
    __int64		m_nFreeSpace;                       // 논리 드라이브 남은 여유 용량 (Byte)

    TCHAR	m_tszName[DRIVE_NAME_STRLEN];             // 드라이브 문자와 경로 (예: C:\)
    TCHAR	m_tszFileSystem[DRIVE_FILESYSTEM_STRLEN]; // 파일 시스템 방식 (예: NTFS, FAT32)

} HWINFO_DRIVE, * PHWINFO_DRIVE;


//***************************************************************************
// @struct  _HWINFO_DRIVES
// @brief 시스템 전체 논리 드라이브의 합산 통계 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_DRIVES
{
public:
    _HWINFO_DRIVES()
    {
        m_dwDriveCount = 0;
        m_nTotalSpace = 0;
        m_nFreeSpace = 0;
    }

    DWORD		m_dwDriveCount;// 시스템 내 마운트된 논리 드라이브 총 개수
    __int64		m_nTotalSpace; // 전체 논리 드라이브 용량의 합계 (Byte)
    __int64		m_nFreeSpace;  // 전체 논리 드라이브 여유 용량의 합계 (Byte)

} HWINFO_DRIVES, * PHWINFO_DRIVES;


//***************************************************************************
// @struct  _HWINFO_SOUNDCARD
// @brief 사운드 카드의 오디오 장치 명칭 및 제어 기능을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_SOUNDCARD
{
public:
    _HWINFO_SOUNDCARD()
    {
        m_bHasVolCtrl = false;
        m_bHasSeparateLRVolCtrl = false;

        m_tszProductName[0] = '\0';
        m_tszCompanyName[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    BOOL	m_bHasVolCtrl;                           // 볼륨 제어 지원 여부 (Sm 쪽 미구현 - waveOutGetDevCaps 미사용)
    BOOL	m_bHasSeparateLRVolCtrl;                 // 좌/우 채널 독립 볼륨 제어 지원 여부 (Sm 쪽 미구현)

    TCHAR	m_tszProductName[SOUNDCARD_PRODUCTNAME_STRLEN];// 오디오 장치 제품명 (Sm 쪽은 SetupAPI FriendlyName/DeviceDesc)
    TCHAR	m_tszCompanyName[SOUNDCARD_COMPANYNAME_STRLEN];// 오디오 제조사명 (Sm 쪽은 SetupAPI Manufacturer)
    TCHAR	m_tszHardwareId[256];                          // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_SOUNDCARD, * PHWINFO_SOUNDCARD;


//***************************************************************************
// @struct  _HWINFO_VIDEOCARD
// @brief 그래픽 카드(디스플레이 어댑터)의 제원 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_VIDEOCARD
{
public:
    _HWINFO_VIDEOCARD()
    {
        m_lMemorySize = 0;

        m_tszDescription[0] = '\0';
        m_tszAdapterString[0] = '\0';
        m_tszChipType[0] = '\0';
        m_tszDacType[0] = '\0';
        m_tszDisplayDrivers[0] = '\0';
        m_tszManufacturer[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    long	m_lMemorySize;                                     // 그래픽 메모리(VRAM) 크기 (MB)

    TCHAR	m_tszDescription[VIDEOCARD_DESCRIPTION_STRLEN];    // 그래픽 카드 디바이스 설명
    TCHAR	m_tszAdapterString[VIDEOCARD_ADAPTERSTRING_STRLEN];// 어댑터 명칭 문자열 (Sm 쪽 미구현 - 레지스트리 DEVICEMAP\VIDEO 파싱 필요)
    TCHAR	m_tszChipType[VIDEOCARD_CHIPTYPE_STRLEN];          // GPU 칩셋 종류 (Sm 쪽 미구현)
    TCHAR	m_tszDacType[VIDEOCARD_DACTYPE_STRLEN];            // DAC 유형 (Sm 쪽 미구현)
    TCHAR	m_tszDisplayDrivers[VIDEOCARD_DISPLAYDRIVERS_STRLEN]; // 설치된 드라이버 파일명 (Sm 쪽 미구현)
    TCHAR	m_tszManufacturer[128];                            // [Sm 전용] SetupAPI SPDRP_MFG. WMI 쪽은 항상 빈 문자열.
    TCHAR	m_tszHardwareId[256];                              // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_VIDEOCARD, * PHWINFO_VIDEOCARD;


//***************************************************************************
// @struct  _HWINFO_NETWORKCARD
// @brief 네트워크 어댑터(LAN 카드)의 명칭 및 기본 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_NETWORKCARD
{
public:
    _HWINFO_NETWORKCARD()
    {
        m_tszDescription[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    TCHAR	m_tszDescription[NETWORKCARD_DESCRIPTION_STRLEN]; // 네트워크 어댑터 설명 및 모델명
    TCHAR	m_tszHardwareId[256];                             // [Sm 전용] MAC 주소가 들어감(다른 구조체와 의미 다름). WMI 쪽은 항상 빈 문자열.

} HWINFO_NETWORKCARD, * PHWINFO_NETWORKCARD;


//***************************************************************************
// @struct  _HWINFO_CDROM
// @brief CD/DVD/Blu-ray 등 광학 드라이브 장치 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_CDROM
{
public:
    _HWINFO_CDROM()
    {
        m_tszName[0] = '\0';
        m_tszManufacturer[0] = '\0';
        m_tszDescription[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    TCHAR	m_tszName[CDROM_NAME_STRLEN];                // CD-ROM 드라이브 장치명 (Sm 쪽 미구현 - 드라이브 문자 매핑 안 함)
    TCHAR	m_tszManufacturer[CDROM_MANUFACTURER_STRLEN];// CD-ROM 제조사 이름
    TCHAR	m_tszDescription[CDROM_DESCRIPTION_STRLEN];  // CD-ROM 드라이브 상세 설명
    TCHAR	m_tszHardwareId[256];                        // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_CDROM, * PHWINFO_CDROM;


//***************************************************************************
// @struct  _HWINFO_KEYBOARD
// @brief 시스템 키보드 장치의 상세 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_KEYBOARD
{
public:
    _HWINFO_KEYBOARD()
    {
        m_tszDescription[0] = '\0';
        m_tszType[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    TCHAR	m_tszDescription[KEYBOARD_DESCRIPTION_STRLEN]; // 키보드 장치 설명
    TCHAR	m_tszType[KEYBOARD_TYPE_STRLEN];               // 키보드 배열 및 인터페이스 유형 (Sm 쪽 미구현 - GetKeyboardType 미사용)
    TCHAR	m_tszHardwareId[256];                           // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_KEYBOARD, * PHWINFO_KEYBOARD;


//***************************************************************************
// @struct  _HWINFO_MOUSE
// @brief 마우스 및 포인팅 디바이스의 세부 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MOUSE
{
public:
    _HWINFO_MOUSE()
    {
        m_tszName[0] = '\0';
        m_tszManufacturer[0] = '\0';
        m_tszDescription[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    TCHAR	m_tszName[MOUSE_NAME_STRLEN];                // 마우스 장치 명칭 (Sm 쪽 미구현)
    TCHAR	m_tszManufacturer[MOUSE_MANUFACTURER_STRLEN];// 마우스 제조사 이름
    TCHAR	m_tszDescription[MOUSE_DESCRIPTION_STRLEN];  // 마우스 장치 설명
    TCHAR	m_tszHardwareId[256];                        // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_MOUSE, * PHWINFO_MOUSE;


//***************************************************************************
// @struct  _HWINFO_MONITOR
// @brief 디스플레이 모니터 장치의 식별 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MONITOR
{
public:
    _HWINFO_MONITOR()
    {
        m_tszManufacturer[0] = '\0';
        m_tszDescription[0] = '\0';
        m_tszHardwareId[0] = '\0';
    }

    TCHAR	m_tszManufacturer[MONITOR_MANUFACTURER_STRLEN];// 모니터 제조사 이름
    TCHAR	m_tszDescription[MONITOR_DESCRIPTION_STRLEN];  // 모니터 모델 및 디바이스 설명
    TCHAR	m_tszHardwareId[256];                          // [Sm 전용] SPDRP_HARDWAREID. WMI 쪽은 항상 빈 문자열.

} HWINFO_MONITOR, * PHWINFO_MONITOR;

#endif // ndef __HWINFOSTRUCTS_H__
