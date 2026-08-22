
//***************************************************************************
// DeviceInfo.h : interface for Non-WMI Hardware Information Classes.
//
//***************************************************************************

#ifndef __DEVICEINFO_H__
#define __DEVICEINFO_H__

#include <vector>

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

//***************************************************************************
// 하드웨어 정보 데이터 구조체(HWINFO_BIOS 등)는 SmHardwareInfo.h(non-WMI 버전)와
// 공유하기 위해 HwInfoStructs.h로 이동했습니다.
//***************************************************************************
#ifndef __HWINFOSTRUCTS_H__
#include <System/HwInfoStructs.h>
#endif


//***************************************************************************
// @class CDriveInfo
// @brief GetLogicalDrives + GetDiskFreeSpaceEx로 논리 드라이브를 조회하는 클래스입니다.
//        CDriveInfo(WMI 버전)의 non-WMI 대응. 로컬 고정 드라이브(DRIVE_FIXED)만 포함.
//        HWINFO_DRIVE는 WMI 버전과 필드가 완전히 동일함(추가 필드 없음).
//***************************************************************************
class CDriveInfo
{
public:
    CDriveInfo();
    ~CDriveInfo();

    //***************************************************************************
    // @brief Win32 API를 통해 각 논리 드라이브의 공간 및 파일 시스템을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 감지된 논리 드라이브의 개수를 반환합니다.
    // @return DWORD 논리 드라이브 수
    //***************************************************************************
    DWORD GetDriveCount() const
    {
        return (DWORD)m_sDriveArray.size();
    }

    //***************************************************************************
    // @brief 전체 논리 드라이브 공간의 총합을 반환합니다.
    // @return const __int64 총 용량 합계 (Byte)
    //***************************************************************************
    const __int64 GetTotalSpaceSize() const
    {
        return m_Drives.m_nTotalSpace;
    }

    //***************************************************************************
    // @brief 전체 논리 드라이브 여유 공간의 총합을 반환합니다.
    // @return const __int64 여유 용량 합계 (Byte)
    //***************************************************************************
    const __int64 GetFreeSpaceSize() const
    {
        return m_Drives.m_nFreeSpace;
    }

    //***************************************************************************
    // @brief 전체 논리 드라이브의 사용 중인 공간의 총합을 반환합니다.
    // @return const __int64 사용 중인 용량 합계 (Byte)
    //***************************************************************************
    const __int64 GetUsedSpaceSize() const
    {
        return m_Drives.m_nTotalSpace - m_Drives.m_nFreeSpace;
    }

    //***************************************************************************
    // @brief 각 논리 드라이브별 상세 정보 구조체 포인터 배열을 반환합니다.
    // @return const std::vector<HWINFO_DRIVE*>* 논리 드라이브 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_DRIVE*>* GetDriveArray() const
    {
        return &m_sDriveArray;
    }

private:
    HWINFO_DRIVES m_Drives;
    std::vector<HWINFO_DRIVE*> m_sDriveArray;
};


//***************************************************************************
// @class CVideoCardInfo
// @brief SetupAPI(GUID_DEVCLASS_DISPLAY)로 디스플레이 어댑터를 열거하는 클래스입니다.
//        CVideoCardInfo(HardwareInfo.h)의 non-WMI 대응 (원본도 이미 non-WMI: 레지스트리
//        기반. 이 클래스는 SetupAPI 기반이라 방식만 다름).
// @details ChipType/DacType/AdapterString/DisplayDrivers는 SetupAPI 표준 속성으로
//          못 얻어 빈 문자열로 남음. Manufacturer/HardwareId는 HWINFO_VIDEOCARD의
//          Sm 전용 필드. m_lMemorySize는 원본과 단위(MB) 통일.
//***************************************************************************
class CVideoCardInfo
{
public:
    CVideoCardInfo();
    ~CVideoCardInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 시스템에 장착된 그래픽 카드 정보를 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 그래픽 카드 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_VIDEOCARD*>* 그래픽 카드 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_VIDEOCARD*>* GetVideoCardArray() const
    {
        return &m_sVideoCardArray;
    }

private:
    std::vector<HWINFO_VIDEOCARD*> m_sVideoCardArray;
};


//***************************************************************************
// @class CSoundCardInfo
// @brief SetupAPI(GUID_DEVCLASS_MEDIA)로 오디오 장치를 열거하는 클래스입니다.
//        CSoundCardInfo(HardwareInfo.h)의 non-WMI 대응.
// @details 원본은 waveOutGetDevCaps로 장치 1개(볼륨 제어 지원 bool 포함)만 조회하는
//          구조였지만, 이 클래스는 SetupAPI로 전체 오디오 장치를 배열로 조회함
//          (HasVolCtrl/HasSeparateLRVolCtrl은 SetupAPI 표준 속성에 없어 미제공,
//          HardwareId는 HWINFO_SOUNDCARD의 Sm 전용 필드).
//***************************************************************************
class CSoundCardInfo
{
public:
    CSoundCardInfo();
    ~CSoundCardInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 오디오 장치 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 사운드 카드 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_SOUNDCARD*>* 사운드 카드 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_SOUNDCARD*>* GetSoundCardArray() const
    {
        return &m_sSoundCardArray;
    }

private:
    std::vector<HWINFO_SOUNDCARD*> m_sSoundCardArray;
};


//***************************************************************************
// @class CNetworkCardInfo
// @brief GetAdaptersAddresses로 네트워크 어댑터를 열거하는 클래스입니다.
//        CNetworkCardInfo(HardwareInfo.h)의 non-WMI 대응.
// @details HWINFO_NETWORKCARD의 Sm 전용 필드 m_tszHardwareId 자리에 MAC 주소가
//          들어감 (다른 구조체와 필드 의미가 다름).
//***************************************************************************
class CNetworkCardInfo
{
public:
    CNetworkCardInfo();
    ~CNetworkCardInfo();

    //***************************************************************************
    // @brief GetAdaptersAddresses를 통해 네트워크 어댑터 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 네트워크 카드 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_NETWORKCARD*>* 네트워크 카드 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_NETWORKCARD*>* GetNetworkCardArray() const
    {
        return &m_sNetworkCardArray;
    }

private:
    std::vector<HWINFO_NETWORKCARD*> m_sNetworkCardArray;
};


//***************************************************************************
// @class CCdromInfo
// @brief SetupAPI(GUID_DEVCLASS_CDROM)로 광학 드라이브를 열거하는 클래스입니다.
//        CCdromInfo(HardwareInfo.h)의 non-WMI 대응.
// @details HWINFO_CDROM의 Name(드라이브 문자 매핑)은 미구현, HardwareId는 Sm 전용 필드.
//***************************************************************************
class CCdromInfo
{
public:
    CCdromInfo();
    ~CCdromInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 장착된 CD-ROM 드라이브 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 광학 드라이브 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_CDROM*>* CD-ROM 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_CDROM*>* GetCdromArray() const
    {
        return &m_sCdromArray;
    }

private:
    std::vector<HWINFO_CDROM*> m_sCdromArray;
};


//***************************************************************************
// @class CKeyBoardInfo
// @brief SetupAPI(GUID_DEVCLASS_KEYBOARD)로 키보드 장치를 열거하는 클래스입니다.
//        CKeyBoardInfo(HardwareInfo.h)의 non-WMI 대응.
// @details 원본은 단일 장치(GetDescription/GetType)만 다뤘지만, 이 클래스는
//          SetupAPI로 검출되는 모든 키보드를 배열로 제공함. GetKeyboardType()
//          기반 유형 판별(HWINFO_KEYBOARD::m_tszType)은 미구현, HardwareId는 Sm 전용 필드.
//***************************************************************************
class CKeyBoardInfo
{
public:
    CKeyBoardInfo();
    ~CKeyBoardInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 키보드 장치 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 키보드 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_KEYBOARD*>* 키보드 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_KEYBOARD*>* GetKeyBoardArray() const
    {
        return &m_sKeyBoardArray;
    }

private:
    std::vector<HWINFO_KEYBOARD*> m_sKeyBoardArray;
};


//***************************************************************************
// @class CMouseInfo
// @brief SetupAPI(GUID_DEVCLASS_MOUSE)로 마우스 장치를 열거하는 클래스입니다.
//        CMouseInfo(HardwareInfo.h)의 non-WMI 대응 (원본은 단일 장치, 이 클래스는 배열).
// @details HWINFO_MOUSE의 Name은 미구현, HardwareId는 Sm 전용 필드.
//***************************************************************************
class CMouseInfo
{
public:
    CMouseInfo();
    ~CMouseInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 마우스 장치 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 마우스 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_MOUSE*>* 마우스 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_MOUSE*>* GetMouseArray() const
    {
        return &m_sMouseArray;
    }

private:
    std::vector<HWINFO_MOUSE*> m_sMouseArray;
};


//***************************************************************************
// @class CMonitorInfo
// @brief SetupAPI(GUID_DEVCLASS_MONITOR)로 모니터 장치를 열거하는 클래스입니다.
//        CMonitorInfo(HardwareInfo.h)의 non-WMI 대응.
// @details HardwareId는 HWINFO_MONITOR의 Sm 전용 필드.
//***************************************************************************
class CMonitorInfo
{
public:
    CMonitorInfo();
    ~CMonitorInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 연결된 모니터 장치 목록을 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 모니터 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_MONITOR*>* 모니터 정보 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_MONITOR*>* GetMonitorArray() const
    {
        return &m_sMonitorArray;
    }

private:
    std::vector<HWINFO_MONITOR*> m_sMonitorArray;
};

#endif // ndef __DEVICEINFO_H__