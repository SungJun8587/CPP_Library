
//***************************************************************************
// WmiHardwareInfo.h: interface for the WMI-based Hardware Information Classes.
//
//***************************************************************************

#ifndef __WMIHARDWAREINFO_H__
#define __WMIHARDWAREINFO_H__

#include <vector>

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
// 하드웨어 정보 데이터 구조체(HWINFO_BIOS 등)는 SmHardwareInfo.h(non-WMI 버전)와
// 공유하기 위해 HwInfoStructs.h로 이동했습니다.
//***************************************************************************
#ifndef __HWINFOSTRUCTS_H__
#include <System/HwInfoStructs.h>
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
// @class CWmiBiosInfo
// @brief 시스템 BIOS 정보를 WMI를 통해 수집하고 조회하는 관리 클래스입니다.
//***************************************************************************
class CWmiBiosInfo
{
public:
	CWmiBiosInfo();
	~CWmiBiosInfo();

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
// @class CWmiMainBoardInfo
// @brief 메인보드(마더보드) 제원을 WMI를 통해 수집하고 조회하는 관리 클래스입니다.
//***************************************************************************
class CWmiMainBoardInfo
{
public:
	CWmiMainBoardInfo();
	~CWmiMainBoardInfo();

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
// @class CWmiMemoryInfo
// @brief 시스템 메모리 용량 및 개별 RAM 모듈 슬롯 정보를 관리하는 클래스입니다.
//***************************************************************************
class CWmiMemoryInfo
{
public:
	CWmiMemoryInfo();
	~CWmiMemoryInfo();

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
// @class CWmiHdDiskInfo
// @brief 물리 하드디스크 및 SSD 장치들의 제원을 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CWmiHdDiskInfo
{
public:
	CWmiHdDiskInfo();
	~CWmiHdDiskInfo();

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
// @class CWmiDriveInfo
// @brief 시스템 내 논리 파티션 드라이브의 용량을 수집 및 계산하는 클래스입니다.
//***************************************************************************
class CWmiDriveInfo
{
public:
	CWmiDriveInfo();
	~CWmiDriveInfo();

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
// @class CWmiNetworkCardInfo
// @brief 시스템 내 네트워크 인터페이스 카드를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CWmiNetworkCardInfo
{
public:
	CWmiNetworkCardInfo();
	~CWmiNetworkCardInfo();

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
// @class CWmiCdromInfo
// @brief 광학 드라이브(CD/DVD-ROM) 장치 정보를 수집하고 관리하는 클래스입니다.
//***************************************************************************
class CWmiCdromInfo
{
public:
	CWmiCdromInfo();
	~CWmiCdromInfo();

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
// @class CWmiKeyBoardInfo
// @brief 입력 장치 중 키보드의 세부 속성을 판별하고 보유하는 클래스입니다.
//***************************************************************************
class CWmiKeyBoardInfo
{
public:
	CWmiKeyBoardInfo();
	~CWmiKeyBoardInfo();

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
// @class CWmiMouseInfo
// @brief 마우스 포인팅 디바이스의 정보를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CWmiMouseInfo
{
public:
	CWmiMouseInfo();
	~CWmiMouseInfo();

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
// @class CWmiMonitorInfo
// @brief 연결된 모니터 디바이스 정보를 수집 및 관리하는 클래스입니다.
//***************************************************************************
class CWmiMonitorInfo
{
public:
	CWmiMonitorInfo();
	~CWmiMonitorInfo();

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

#endif // ndef __WMIHARDWAREINFO_H__