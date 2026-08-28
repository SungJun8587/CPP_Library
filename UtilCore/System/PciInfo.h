
//***************************************************************************
// PciInfo.h : interface for the PCI bus enumeration class.
//
// SetupAPI로 PCI 버스 장치를 열거하되(대부분의 데이터는 SetupAPI), 벤더명 조회/
// Class Code 분류만 PciInfo64.asm/PciInfo32.asm에 위임합니다. DeviceInfo.h의
// 나머지 클래스는 asm 의존이 전혀 없어서, asm을 쓰는 이 클래스만 별도 파일로
// 분리했습니다.
//***************************************************************************

#ifndef UC_PCIINFO_H
#define UC_PCIINFO_H

#include <vector>

#include <System/HwInfoStructs.h>

//***************************************************************************
// @class CPciInfo
// @brief SetupAPI로 PCI 버스의 모든 장치를 열거하는 클래스입니다. WMI 버전에는
//        직접 대응하는 클래스가 없는 신규 추가 - CVideoCardInfo 등이 주는 드라이버
//        문자열보다 신뢰도 높은 숫자 Vendor/Device ID, 버스 위치, Class Code를 제공함.
// @details 벤더명 조회(pci_parse_vendor_name)와 Class Code 분류(pci_classify_device)는
//          PciInfo64.asm/PciInfo32.asm 구현을 그대로 사용합니다.
//***************************************************************************
class CPciInfo
{
public:
    CPciInfo();
    ~CPciInfo();

    //***************************************************************************
    // @brief SetupAPI를 통해 PCI 버스에 연결된 모든 장치를 수집합니다.
    // @return BOOL 정보 수집 성공 여부 (TRUE: 성공, FALSE: 실패)
    //***************************************************************************
    BOOL GetInformation();

    //***************************************************************************
    // @brief 수집된 PCI 장치 정보 구조체 배열의 포인터를 반환합니다.
    // @return const std::vector<HWINFO_PCIDEVICE*>* PCI 장치 포인터 벡터
    //***************************************************************************
    const std::vector<HWINFO_PCIDEVICE*>* GetPciDeviceArray() const
    {
        return &m_sPciArray;
    }

private:
    std::vector<HWINFO_PCIDEVICE*> m_sPciArray;
};

#endif // ndef UC_PCIINFO_H
