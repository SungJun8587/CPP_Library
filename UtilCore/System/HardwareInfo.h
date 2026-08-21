
//***************************************************************************
// HardwareInfo.h: interface for the Hardware Information Class.
//
//***************************************************************************

#ifndef __HARDWAREINFO_H__
#define __HARDWAREINFO_H__

#include <vector>

#ifndef _INC_MMSYSTEM
#include <mmsystem.h>
#endif

#ifndef _INC_MMREG
#include <mmreg.h>
#endif

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

#ifndef __WMI_H__
#include <System/Wmi.h>
#endif

#ifndef __OSINFO_H__
#include <System/OsInfo.h>
#endif

//***************************************************************************
// @brief 바이트 단위의 정수 데이터를 읽기 좋은 데이터 크기 포맷 문자열로 변환합니다.
// @param   nData 변환할 데이터 크기 (Byte)
// @param   ptszFormat 변환된 문자열을 전달받을 버퍼 포인터
// @return  void
// @details KB, MB, GB, TB 등의 적절한 단위로 변환하여 문자열 버퍼에 저장합니다.
//***************************************************************************
void ChangeDataFormat(const __int64& nData, TCHAR* ptszFormat);


//***************************************************************************
// @struct  _HWINFO_BIOS
// @brief 시스템 BIOS의 세부 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_BIOS
{
public:
	_HWINFO_BIOS() {
		m_tszManufacturer[0] = '\0';
		m_tszSmVersion[0] = '\0';
		m_tszVersion[0] = '\0';
		m_tszIdentificationCode[0] = '\0';
		m_tszSerialNumber[0] = '\0';
		m_tszReleaseDate[0] = '\0';
	}

	TCHAR	m_tszManufacturer[BIOS_MANUFACTURER_STRLEN];            // BIOS 제조사 이름
	TCHAR	m_tszSmVersion[BIOS_SMVERSION_STRLEN];                  // SMBIOS 버전 문자열
	TCHAR	m_tszVersion[BIOS_VERSION_STRLEN];                      // BIOS 버전 번호
	TCHAR	m_tszIdentificationCode[BIOS_IDENTIFICATIONCODE_STRLEN];// BIOS 식별 코드
	TCHAR	m_tszSerialNumber[BIOS_SERIALNUMBER_STRLEN];            // BIOS 시리얼 번호
	TCHAR	m_tszReleaseDate[BIOS_RELEASEDATE_STRLEN];              // BIOS 출시일

} HWINFO_BIOS, * PHWINFO_BIOS;


//***************************************************************************
// @struct  _HWINFO_MAINBOARD
// @brief 메인보드(마더보드)의 제원 및 식별 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MAINBOARD
{
public:
	_HWINFO_MAINBOARD() {
		m_tszProduct[0] = '\0';
		m_tszSerialNumber[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszProduct[MAINBOARD_PRODUCT_STRLEN];          // 메인보드 제품명/모델명
	TCHAR	m_tszSerialNumber[MAINBOARD_SERIALNUMBER_STRLEN];// 메인보드 고유 시리얼 번호
	TCHAR	m_tszManufacturer[MAINBOARD_MANUFACTURER_STRLEN];// 메인보드 제조사 이름
	TCHAR	m_tszDescription[MAINBOARD_DESCRIPTION_STRLEN];  // 메인보드 장치 상세 설명

} HWINFO_MAINBOARD, * PHWINFO_MAINBOARD;


//***************************************************************************
// @struct  _HWINFO_RAM
// @brief 개별 RAM 모듈 슬롯의 규격 및 속성 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_RAM
{
public:
	_HWINFO_RAM() {
		m_nCapacity = 0;
		m_dwFormFactor = 0;
		m_dwMemoryType = 0;
		m_dwSpeed = 0;

		m_tszBankLabel[0] = '\0';
		m_tszName[0] = '\0';
		m_tszDeviceLocator[0] = '\0';
		m_tszFormFactorDesc[0] = '\0';
		m_tszMemoryTypeDesc[0] = '\0';
	}

	__int64		m_nCapacity;                                    // 개별 메모리 용량 (Byte)
	DWORD		m_dwFormFactor;                                 // SMBIOS FormFactor ID 코드
	DWORD		m_dwMemoryType;                                 // SMBIOS MemoryType ID 코드
	DWORD		m_dwSpeed;                                      // 메모리 동작 속도 (MHz)
	TCHAR		m_tszBankLabel[RAM_BANKLABEL_STRLEN];           // 메모리 은행 레이블
	TCHAR		m_tszName[RAM_NAME_STRLEN];                     // 메모리 장치 이름
	TCHAR		m_tszDeviceLocator[RAM_DEVICELOCATOR_STRLEN];   // 메인보드 내 슬롯 위치
	TCHAR		m_tszFormFactorDesc[RAM_FORMFACTORDESC_STRLEN]; // 폼팩터 문자열 설명 (예: DIMM, CAMM)
	TCHAR		m_tszMemoryTypeDesc[RAM_MEMORYTYPEDESC_STRLEN]; // 메모리 타입 문자열 설명 (예: DDR4, DDR5)

} HWINFO_RAM, * PHWINFO_RAM;


//***************************************************************************
// @struct  _HWINFO_MEMORY
// @brief 시스템 전반의 물리 및 가상 메모리 통계 수치를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MEMORY
{
public:
	_HWINFO_MEMORY() {
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
	__int64		m_nPhysicalMemSize;    // 현재 사용 가능한 물리 메모리 크기 (KB)
	__int64		m_nTotalVirtualMemSize;// 총 가상 메모리 크기 (KB)
	__int64		m_nFreeVirtualMemSize; // 여유 가상 메모리 크기 (KB)
	__int64		m_nTotalPageFileSize;  // 총 페이징 파일 크기 (KB)
	__int64		m_nFreePageFileSize;   // 여유 페이징 파일 크기 (KB)

} HWINFO_MEMORY, * PHWINFO_MEMORY;


//***************************************************************************
// @struct  _HWINFO_HDDISK
// @brief 물리적 하드디스크/SSD 스토리지의 기본 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_HDDISK
{
public:
	_HWINFO_HDDISK() {
		m_nTotalSize = 0;

		m_tszModel[0] = '\0';
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	__int64		m_nTotalSize;                             // 물리 디스크 전체 저장 용량 (Byte)

	TCHAR	m_tszModel[HDDISK_MODEL_STRLEN];              // 스토리지 모델명
	TCHAR	m_tszName[HDDISK_NAME_STRLEN];                // 디스크 장치 식별 이름
	TCHAR	m_tszManufacturer[HDDISK_MANUFACTURER_STRLEN];// 스토리지 제조사 이름
	TCHAR	m_tszDescription[HDDISK_DESCRIPTION_STRLEN];  // 스토리지 인터페이스/설명

} HWINFO_HDDISK, * PHWINFO_HDDISK;


//***************************************************************************
// @struct  _HWINFO_DRIVE
// @brief 논리 파티션 드라이브(C:, D: 등)의 용량 및 파일 시스템 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_DRIVE
{
public:
	_HWINFO_DRIVE() {
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
	_HWINFO_DRIVES() {
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
	_HWINFO_SOUNDCARD() {
		m_bHasVolCtrl = false;
		m_bHasSeparateLRVolCtrl = false;

		m_tszProductName[0] = '\0';
		m_tszCompanyName[0] = '\0';
	}

	BOOL	m_bHasVolCtrl;                           // 볼륨 제어 지원 여부
	BOOL	m_bHasSeparateLRVolCtrl;                 // 좌/우 채널 독립 볼륨 제어 지원 여부

	TCHAR	m_tszProductName[SOUNDCARD_PRODUCTNAME_STRLEN];// 오디오 장치 제품명
	TCHAR	m_tszCompanyName[SOUNDCARD_COMPANYNAME_STRLEN];// 오디오 제조사명

} HWINFO_SOUNDCARD, * PHWINFO_SOUNDCARD;


//***************************************************************************
// @struct  _HWINFO_VIDEOCARD
// @brief 그래픽 카드(디스플레이 어댑터)의 제원 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_VIDEOCARD
{
public:
	_HWINFO_VIDEOCARD() {
		m_lMemorySize = 0;

		m_tszDescription[0] = '\0';
		m_tszAdapterString[0] = '\0';
		m_tszChipType[0] = '\0';
		m_tszDacType[0] = '\0';
		m_tszDisplayDrivers[0] = '\0';
	}

	long	m_lMemorySize;                                     // 그래픽 메모리(VRAM) 크기 (MB)

	TCHAR	m_tszDescription[VIDEOCARD_DESCRIPTION_STRLEN];    // 그래픽 카드 디바이스 설명
	TCHAR	m_tszAdapterString[VIDEOCARD_ADAPTERSTRING_STRLEN];// 어댑터 명칭 문자열
	TCHAR	m_tszChipType[VIDEOCARD_CHIPTYPE_STRLEN];          // GPU 칩셋 종류
	TCHAR	m_tszDacType[VIDEOCARD_DACTYPE_STRLEN];            // DAC 유형
	TCHAR	m_tszDisplayDrivers[VIDEOCARD_DISPLAYDRIVERS_STRLEN]; // 설치된 드라이버 파일명

} HWINFO_VIDEOCARD, * PHWINFO_VIDEOCARD;


//***************************************************************************
// @struct  _HWINFO_NETWORKCARD
// @brief 네트워크 어댑터(LAN 카드)의 명칭 및 기본 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_NETWORKCARD
{
public:
	_HWINFO_NETWORKCARD() {
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszDescription[NETWORKCARD_DESCRIPTION_STRLEN]; // 네트워크 어댑터 설명 및 모델명

} HWINFO_NETWORKCARD, * PHWINFO_NETWORKCARD;


//***************************************************************************
// @struct  _HWINFO_CDROM
// @brief CD/DVD/Blu-ray 등 광학 드라이브 장치 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_CDROM
{
public:
	_HWINFO_CDROM() {
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszName[CDROM_NAME_STRLEN];                // CD-ROM 드라이브 장치명
	TCHAR	m_tszManufacturer[CDROM_MANUFACTURER_STRLEN];// CD-ROM 제조사 이름
	TCHAR	m_tszDescription[CDROM_DESCRIPTION_STRLEN];  // CD-ROM 드라이브 상세 설명

} HWINFO_CDROM, * PHWINFO_CDROM;


//***************************************************************************
// @struct  _HWINFO_KEYBOARD
// @brief 시스템 키보드 장치의 상세 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_KEYBOARD
{
public:
	_HWINFO_KEYBOARD() {
		m_tszDescription[0] = '\0';
		m_tszType[0] = '\0';
	}

	TCHAR	m_tszDescription[KEYBOARD_DESCRIPTION_STRLEN]; // 키보드 장치 설명
	TCHAR	m_tszType[KEYBOARD_TYPE_STRLEN];               // 키보드 배열 및 인터페이스 유형

} HWINFO_KEYBOARD, * PHWINFO_KEYBOARD;


//***************************************************************************
// @struct  _HWINFO_MOUSE
// @brief 마우스 및 포인팅 디바이스의 세부 제원을 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MOUSE
{
public:
	_HWINFO_MOUSE() {
		m_tszName[0] = '\0';
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszName[MOUSE_NAME_STRLEN];                // 마우스 장치 명칭
	TCHAR	m_tszManufacturer[MOUSE_MANUFACTURER_STRLEN];// 마우스 제조사 이름
	TCHAR	m_tszDescription[MOUSE_DESCRIPTION_STRLEN];  // 마우스 장치 설명

} HWINFO_MOUSE, * PHWINFO_MOUSE;


//***************************************************************************
// @struct  _HWINFO_MONITOR
// @brief 디스플레이 모니터 장치의 식별 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _HWINFO_MONITOR
{
public:
	_HWINFO_MONITOR() {
		m_tszManufacturer[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszManufacturer[MONITOR_MANUFACTURER_STRLEN];// 모니터 제조사 이름
	TCHAR	m_tszDescription[MONITOR_DESCRIPTION_STRLEN];  // 모니터 모델 및 디바이스 설명

} HWINFO_MONITOR, * PHWINFO_MONITOR;


//***************************************************************************
// @class CBiosInfo
// @brief 시스템 BIOS 정보를 WMI를 통해 수집하고 조회하는 관리 클래스입니다.
//***************************************************************************
class CBiosInfo
{
public:
	CBiosInfo();
	~CBiosInfo();

	//***************************************************************************
	// @brief WMI 객체를 이용해 BIOS 제원 정보를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_BIOS 클래스를 쿼리하여 제조사, 버전, 시리얼 번호 등을 기록합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 수집된 BIOS 제조사 이름을 반환합니다.
	// @return  const TCHAR* 제조사 이름 문자열 포인터
	//***************************************************************************
	const TCHAR* GetManufacturer() const {
		return m_Bios.m_tszManufacturer;
	}

	//***************************************************************************
	// @brief 수집된 SMBIOS 버전을 반환합니다.
	// @return  const TCHAR* SMBIOS 버전 문자열 포인터
	//***************************************************************************
	const TCHAR* GetSmVersion() const {
		return m_Bios.m_tszSmVersion;
	}

	//***************************************************************************
	// @brief 수집된 BIOS 버전을 반환합니다.
	// @return  const TCHAR* BIOS 버전 문자열 포인터
	//***************************************************************************
	const TCHAR* GetVersion() const {
		return m_Bios.m_tszVersion;
	}

	//***************************************************************************
	// @brief 수집된 BIOS 식별 코드를 반환합니다.
	// @return  const TCHAR* 식별 코드 문자열 포인터
	//***************************************************************************
	const TCHAR* GetIdentificationCode() const {
		return m_Bios.m_tszIdentificationCode;
	}

	//***************************************************************************
	// @brief 수집된 BIOS 시리얼 번호를 반환합니다.
	// @return  const TCHAR* 시리얼 번호 문자열 포인터
	//***************************************************************************
	const TCHAR* GetSerialNumber() const {
		return m_Bios.m_tszSerialNumber;
	}

	//***************************************************************************
	// @brief 수집된 BIOS 출시일을 반환합니다.
	// @return  const TCHAR* 출시일 문자열 포인터
	//***************************************************************************
	const TCHAR* GetReleaseDate() const {
		return m_Bios.m_tszReleaseDate;
	}

private:
	HWINFO_BIOS	m_Bios; // BIOS 수집 정보 데이터 구조체
};


//***************************************************************************
// @class CMainBoardInfo
// @brief 메인보드(마더보드) 제원을 WMI를 통해 수집하고 조회하는 관리 클래스입니다.
//***************************************************************************
class CMainBoardInfo
{
public:
	CMainBoardInfo();
	~CMainBoardInfo();

	//***************************************************************************
	// @brief WMI 객체를 이용해 메인보드 정보를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_BaseBoard 클래스에서 제품명, 제조사, 시리얼 번호 등을 수집합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 수집된 메인보드 설명을 반환합니다.
	// @return  const TCHAR* 메인보드 설명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetDescription() const {
		return m_MainBoard.m_tszDescription;
	}

	//***************************************************************************
	// @brief 수집된 메인보드 제조사 이름을 반환합니다.
	// @return  const TCHAR* 제조사 이름 문자열 포인터
	//***************************************************************************
	const TCHAR* GetManufacturer() const {
		return m_MainBoard.m_tszManufacturer;
	}

	//***************************************************************************
	// @brief 수집된 메인보드 제품명을 반환합니다.
	// @return  const TCHAR* 제품명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetProduct() const {
		return m_MainBoard.m_tszProduct;
	}

	//***************************************************************************
	// @brief 수집된 메인보드 시리얼 번호를 반환합니다.
	// @return  const TCHAR* 시리얼 번호 문자열 포인터
	//***************************************************************************
	const TCHAR* GetSerialNumber() const {
		return m_MainBoard.m_tszSerialNumber;
	}

private:
	HWINFO_MAINBOARD	m_MainBoard; // 메인보드 수집 정보 데이터 구조체
};


//***************************************************************************
// @class CMemoryInfo
// @brief 시스템 메모리 용량 및 개별 RAM 모듈 슬롯 정보를 관리하는 클래스입니다.
//***************************************************************************
class CMemoryInfo
{
public:
	CMemoryInfo();
	~CMemoryInfo();

	//***************************************************************************
	// @brief WMI 및 Win32 API를 통해 전체 메모리 상태 및 RAM 모듈 리스트를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details GlobalMemoryStatusEx 및 Win32_PhysicalMemory를 사용해 세부 제원을 구합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 감지된 RAM 모듈의 총 개수를 반환합니다.
	// @return  DWORD 장착된 RAM 개수
	//***************************************************************************
	DWORD GetRamCount() const {
		return m_Memory.m_dwRamCount;
	}

	//***************************************************************************
	// @brief 시스템 전체 물리 메모리 용량을 반환합니다.
	// @return  const __int64 전체 메모리 크기 (Byte)
	//***************************************************************************
	const __int64 GetTotalMemSize() const {
		return m_Memory.m_nTotalMemSize;
	}

	//***************************************************************************
	// @brief 현재 사용 가능한 실제 물리 메모리 용량을 반환합니다.
	// @return  const __int64 가용 물리 메모리 크기 (Byte)
	//***************************************************************************
	const __int64 GetPhysicalMemSize() const {
		return m_Memory.m_nPhysicalMemSize * 1024;
	}

	//***************************************************************************
	// @brief 현재 점유하여 사용 중인 물리 메모리 용량을 반환합니다.
	// @return  const __int64 사용 중인 메모리 크기 (Byte)
	//***************************************************************************
	const __int64 GetUseMemSize() const {
		return m_Memory.m_nTotalMemSize - (m_Memory.m_nPhysicalMemSize * 1024);
	}

	//***************************************************************************
	// @brief 전체 물리 메모리 대비 현재 사용량의 비율을 계산하여 반환합니다.
	// @return  const double 메모리 사용율 (0.0 ~ 1.0). 전체 메모리 크기를 알 수 없는
	//          경우(0인 경우) 0-나눗셈을 피하기 위해 0.0을 반환합니다.
	//***************************************************************************
	const double GetPercentUsedRam() const {
		if( m_Memory.m_nTotalMemSize == 0 ) return 0.0;
		return (double)(m_Memory.m_nTotalMemSize - (m_Memory.m_nPhysicalMemSize * 1024)) / (double)m_Memory.m_nTotalMemSize;
	}

	//***************************************************************************
	// @brief 시스템 전체 가상 메모리 용량을 반환합니다.
	// @return  const __int64 총 가상 메모리 크기 (Byte)
	//***************************************************************************
	const __int64 GetTotalVirtualMemSize() const {
		return m_Memory.m_nTotalVirtualMemSize * 1024;
	}

	//***************************************************************************
	// @brief 여유 가상 메모리 용량을 반환합니다.
	// @return  const __int64 가용 가상 메모리 크기 (Byte)
	//***************************************************************************
	const __int64 GetFreeVirtualMemSize() const {
		return m_Memory.m_nFreeVirtualMemSize * 1024;
	}

	//***************************************************************************
	// @brief 총 페이징 파일 크기를 반환합니다.
	// @return  const __int64 총 페이징 파일 크기 (Byte)
	//***************************************************************************
	const __int64 GetTotalPageFile() const {
		return m_Memory.m_nTotalPageFileSize * 1024;
	}

	//***************************************************************************
	// @brief 남은 페이징 파일 크기를 반환합니다.
	// @return  const __int64 여유 페이징 파일 크기 (Byte)
	//***************************************************************************
	const __int64 GetFreePageFile() const {
		return m_Memory.m_nFreePageFileSize * 1024;
	}

	//***************************************************************************
	// @brief 슬롯별 장착된 RAM 모듈 정보 포인터 배열을 반환합니다.
	// @return  const std::vector<HWINFO_RAM*>* RAM 정보 구조체 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_RAM*>* GetRamArray() const {
		return &m_sRamArray;
	}

private:
	//***************************************************************************
	// @brief FormFactor ID 코드를 해석하여 폼팩터 명칭 문자열을 반환합니다. (SMBIOS 3.7+ 반영)
	// @param   dwFormFactor SMBIOS FormFactor ID
	// @return  _tstring DIMM, SODIMM, CAMM 등의 폼팩터 이름
	//***************************************************************************
	_tstring FormFactorFormatDesc(DWORD dwFormFactor) const;

	//***************************************************************************
	// @brief MemoryType ID 코드를 해석하여 메모리 규격 문자열을 반환합니다. (SMBIOS 3.7+ 반영)
	// @param   dwMemoryType SMBIOS MemoryType ID
	// @return  _tstring DDR4, DDR5, LPDDR5 등의 규격 문자열
	//***************************************************************************
	_tstring MemoryTypeFormatDesc(DWORD dwMemoryType) const;

private:
	HWINFO_MEMORY	m_Memory;               // 전체 메모리 요약 통계 구조체
	std::vector<HWINFO_RAM*> m_sRamArray;  // 장착된 RAM 모듈별 포인터 배열
};


//***************************************************************************
// @class CHdDiskInfo
// @brief 물리 하드디스크 및 SSD 장치들의 제원을 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CHdDiskInfo
{
public:
	CHdDiskInfo();
	~CHdDiskInfo();

	//***************************************************************************
	// @brief WMI를 통해 장착된 모든 물리 디스크 스토리지 정보를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_DiskDrive 클래스에서 각 물리 디스크의 모델, 제조사, 용량을 수집합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 수집된 물리 디스크 정보 구조체 배열의 포인터를 반환합니다.
	// @return  const std::vector<HWINFO_HDDISK*>* 물리 디스크 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_HDDISK*>* GetHdDiskArray() const {
		return &m_sHdDiskArray;
	}

private:
	std::vector<HWINFO_HDDISK*> m_sHdDiskArray; // 물리 디스크 정보 포인터 배열
};


//***************************************************************************
// @class CDriveInfo
// @brief 시스템 내 논리 파티션 드라이브의 용량을 수집 및 계산하는 클래스입니다.
//***************************************************************************
class CDriveInfo
{
public:
	CDriveInfo();
	~CDriveInfo();

	//***************************************************************************
	// @brief Win32 API 및 WMI를 통해 각 논리 드라이브의 공간 및 파일 시스템을 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details 논리 파티션 목록을 순회하며 전체 및 남은 사용량을 합산/기록합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 감지된 논리 드라이브의 개수를 반환합니다.
	// @return  DWORD 논리 드라이브 수
	//***************************************************************************
	DWORD GetDriveCount() const {
		return m_Drives.m_dwDriveCount;
	}

	//***************************************************************************
	// @brief 전체 논리 드라이브 공간의 총합을 반환합니다.
	// @return  const __int64 총 용량 합계 (Byte)
	//***************************************************************************
	const __int64 GetTotalSpaceSize() const {
		return m_Drives.m_nTotalSpace;
	}

	//***************************************************************************
	// @brief 전체 논리 드라이브 여유 공간의 총합을 반환합니다.
	// @return  const __int64 여유 용량 합계 (Byte)
	//***************************************************************************
	const __int64 GetFreeSpaceSize() const {
		return m_Drives.m_nFreeSpace;
	}

	//***************************************************************************
	// @brief 전체 논리 드라이브의 사용 중인 공간의 총합을 반환합니다.
	// @return  const __int64 사용 중인 용량 합계 (Byte)
	//***************************************************************************
	const __int64 GetUsedSpaceSize() const {
		return m_Drives.m_nTotalSpace - m_Drives.m_nFreeSpace;
	}

	//***************************************************************************
	// @brief 각 논리 드라이브별 상세 정보 구조체 포인터 배열을 반환합니다.
	// @return  const std::vector<HWINFO_DRIVE*>* 논리 드라이브 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_DRIVE*>* GetDriveArray() const {
		return &m_sDriveArray;
	}

private:
	HWINFO_DRIVES	m_Drives;               // 드라이브 전체 합산 요약 정보
	std::vector<HWINFO_DRIVE*> m_sDriveArray; // 논리 드라이브별 상세 정보 배열
};


//***************************************************************************
// @class CSoundCardInfo
// @brief 사운드 장치 및 오디오 드라이버 정보를 관리하는 클래스입니다.
//***************************************************************************
class CSoundCardInfo
{
public:
	CSoundCardInfo();
	~CSoundCardInfo();

	//***************************************************************************
	// @brief Windows 멀티미디어 API(waveOut)를 통해 사운드 카드 제원 정보를 수집합니다.
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details waveOutGetDevCaps API를 호출하여 오디오 디바이스 기능 및 제어 속성을 파악합니다.
	//***************************************************************************
	BOOL GetInformation();

	//***************************************************************************
	// @brief 볼륨 제어 기능 지원 여부를 반환합니다.
	// @return  BOOL 지원 여부
	//***************************************************************************
	BOOL HasVolCtrl() const {
		return m_SoundCard.m_bHasVolCtrl;
	}

	//***************************************************************************
	// @brief 좌/우 채널 독립 볼륨 제어 지원 여부를 반환합니다.
	// @return  BOOL 지원 여부
	//***************************************************************************
	BOOL HasSeparateLRVolCtrl() const {
		return m_SoundCard.m_bHasSeparateLRVolCtrl;
	}

	//***************************************************************************
	// @brief 오디오 장치의 제품명을 반환합니다.
	// @return  const TCHAR* 제품명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetProductName() const {
		return m_SoundCard.m_tszProductName;
	}

	//***************************************************************************
	// @brief 오디오 장치의 제조사 이름을 반환합니다.
	// @return  const TCHAR* 제조사명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetCompanyName() const {
		return m_SoundCard.m_tszCompanyName;
	}

private:
	//***************************************************************************
	// @brief 멀티미디어 제조사 ID(Manufacturer ID)를 기업 명칭 문자열로 해석합니다.
	// @param   nCompany Windows Multimedia ID 수치 코드
	// @return  _tstring 매핑된 기업 이름 문자열
	//***************************************************************************
	_tstring GetAudioDevCompanyName(int nCompany) const;

private:
	HWINFO_SOUNDCARD	m_SoundCard; // 사운드 카드 정보 데이터 구조체
};


//***************************************************************************
// @class CVideoCardInfo
// @brief 디스플레이 어댑터(그래픽 카드) 제원을 관리하는 클래스입니다.
//***************************************************************************
class CVideoCardInfo
{
public:
	CVideoCardInfo();
	~CVideoCardInfo();

	//***************************************************************************
	// @brief 시스템에 장착된 그래픽 카드 정보를 수집합니다.
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details EnumDisplayDevices API 또는 레지스트리를 조회하여 디스플레이 어댑터 정보를 수집합니다.
	//***************************************************************************
	BOOL GetInformation();

	//***************************************************************************
	// @brief 수집된 그래픽 카드 정보 구조체 배열의 포인터를 반환합니다.
	// @return  const std::vector<HWINFO_VIDEOCARD*>* 그래픽 카드 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_VIDEOCARD*>* GetVideoCardArray() const {
		return &m_sVideoCardArray;
	}

private:
	std::vector<HWINFO_VIDEOCARD*> m_sVideoCardArray; // 그래픽 카드 정보 포인터 배열
};


//***************************************************************************
// @class CNetworkCardInfo
// @brief 시스템 내 네트워크 인터페이스 카드를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CNetworkCardInfo
{
public:
	CNetworkCardInfo();
	~CNetworkCardInfo();

	//***************************************************************************
	// @brief WMI를 이용하여 네트워크 카드 명칭 정보를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_NetworkAdapter 또는 관련 클래스에서 어댑터 명칭을 구합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 네트워크 카드 정보 구조체 배열의 포인터를 반환합니다.
	// @return  const std::vector<HWINFO_NETWORKCARD*>* 네트워크 카드 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_NETWORKCARD*>* GetNetworkCardArray() const {
		return &m_sNetworkCardArray;
	}

private:
	std::vector<HWINFO_NETWORKCARD*> m_sNetworkCardArray; // 네트워크 카드 포인터 배열
};


//***************************************************************************
// @class CCdromInfo
// @brief 광학 드라이브(CD/DVD-ROM) 장치 정보를 수집하고 관리하는 클래스입니다.
//***************************************************************************
class CCdromInfo
{
public:
	CCdromInfo();
	~CCdromInfo();

	//***************************************************************************
	// @brief WMI를 통해 장착된 CD-ROM 드라이브 제원을 탐색합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_CDROMDrive 클래스를 이용하여 광학 드라이브 속성을 조사합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 광학 드라이브 정보 구조체 배열의 포인터를 반환합니다.
	// @return  const std::vector<HWINFO_CDROM*>* CD-ROM 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_CDROM*>* GetCdromArray() const {
		return &m_sCdromArray;
	}

private:
	std::vector<HWINFO_CDROM*> m_sCdromArray; // CD-ROM 정보 포인터 배열
};


//***************************************************************************
// @class CKeyBoardInfo
// @brief 입력 장치 중 키보드의 세부 속성을 판별하고 보유하는 클래스입니다.
//***************************************************************************
class CKeyBoardInfo
{
public:
	CKeyBoardInfo();
	~CKeyBoardInfo();

	//***************************************************************************
	// @brief WMI 및 Win32 API를 사용해 키보드 유형 및 장치 설명을 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details GetKeyboardType API 및 Win32_Keyboard 클래스를 활용해 파악합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 키보드 장치 설명을 반환합니다.
	// @return  const TCHAR* 설명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetDescription() const {
		return m_KeyBoard.m_tszDescription;
	}

	//***************************************************************************
	// @brief 키보드 배열 유형을 반환합니다.
	// @return  const TCHAR* 키보드 유형 문자열 포인터
	//***************************************************************************
	const TCHAR* GetType() const {
		return m_KeyBoard.m_tszType;
	}

private:
	//***************************************************************************
	// @brief Win32 API를 통해 하드웨어 키보드 하위 유형을 탐지합니다.
	// @return  void
	// @details GetKeyboardType API 결과값을 파싱하여 m_KeyBoard 구조체에 할당합니다.
	//***************************************************************************
	void DetectKbType();

private:
	HWINFO_KEYBOARD	m_KeyBoard; // 키보드 정보 데이터 구조체
};


//***************************************************************************
// @class CMouseInfo
// @brief 마우스 포인팅 디바이스의 정보를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CMouseInfo
{
public:
	CMouseInfo();
	~CMouseInfo();

	//***************************************************************************
	// @brief WMI를 이용해 마우스 제조사 및 설명 정보를 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_PointingDevice 클래스를 탐색하여 정보를 채웁니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 마우스 장치 명칭을 반환합니다.
	// @return  const TCHAR* 마우스 명칭 문자열 포인터
	//***************************************************************************
	const TCHAR* GetName() const {
		return m_Mouse.m_tszName;
	}

	//***************************************************************************
	// @brief 마우스 제조사 이름을 반환합니다.
	// @return  const TCHAR* 제조사명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetManufacturer() const {
		return m_Mouse.m_tszManufacturer;
	}

	//***************************************************************************
	// @brief 마우스 장치 설명을 반환합니다.
	// @return  const TCHAR* 설명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetDescription() const {
		return m_Mouse.m_tszDescription;
	}

private:
	HWINFO_MOUSE	m_Mouse; // 마우스 정보 데이터 구조체
};


//***************************************************************************
// @class CMonitorInfo
// @brief 연결된 모니터 디바이스 정보를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CMonitorInfo
{
public:
	CMonitorInfo();
	~CMonitorInfo();

	//***************************************************************************
	// @brief WMI를 통해 현재 연결된 모니터 장치들의 제원을 수집합니다.
	// @param   Wmi 초기화된 WMI 인터페이스 객체 참조
	// @return  BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
	// @details Win32_DesktopMonitor 또는 관련 디스플레이 WMI 클래스를 조회합니다.
	//***************************************************************************
	BOOL GetInformation(CWmi& Wmi);

	//***************************************************************************
	// @brief 수집된 모니터 정보 구조체 배열의 포인터를 반환합니다.
	// @return  const std::vector<HWINFO_MONITOR*>* 모니터 정보 포인터 벡터
	//***************************************************************************
	const std::vector<HWINFO_MONITOR*>* GetMonitorArray() const {
		return &m_sMonitorArray;
	}

private:
	std::vector<HWINFO_MONITOR*> m_sMonitorArray; // 모니터 정보 포인터 배열
};

#endif // ndef __HARDWAREINFO_H__