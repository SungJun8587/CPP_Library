
//***************************************************************************
// PciInfo.h: PCI Bus Enumeration & Device Search Library
//
//***************************************************************************

#ifndef __PCIINFO_H__
#define __PCIINFO_H__

#include <cstdint>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "setupapi.lib")

//***************************************************************************
// @brief   PCI 장치의 주요 분류(Class) 구분 열거형입니다.
// @detail  Base Class, Sub Class, ProgIF의 조합을 바탕으로 장치 유형을 정의합니다.
//***************************************************************************
enum class PciDeviceClass {
    Unknown = 0,    // 기타/미분류 장치
    GPU,            // Display Controller (Base Class 0x03)
    NVMe            // Mass Storage - NVMe Controller (Base Class 0x01, Sub Class 0x08, ProgIF 0x02)
};

//***************************************************************************
// @brief   스캔된 PCI 디바이스의 상세 정보를 담는 구조체입니다.
// @detail  PCI Configuration Header 정보와 버스 위치, 제조사명을 포함합니다.
//***************************************************************************
struct PciDeviceInfo {
    uint8_t  bus;               // PCI Bus 번호
    uint8_t  device;            // PCI Device 번호
    uint8_t  function;          // PCI Function 번호
    uint16_t vendor_id;         // Vendor ID (제조사 식별자)
    uint16_t device_id;         // Device ID (장치 식별자)
    uint8_t  base_class;        // Base Class Code
    uint8_t  sub_class;         // Sub Class Code
    uint8_t  prog_if;           // Programming Interface Code
    PciDeviceClass type;        // 디바이스 분류 유형
    char     vendor_name[32];   // 파싱된 제조사 이름 문자열
    char     device_desc[64];   // OS 디바이스 설명 문자열
};

extern "C" {
    //***************************************************************************
    // @brief   PCI Vendor ID를 바탕으로 알려진 제조사 이름을 파싱합니다.
    // @param   vendor_id   [in] PCI Vendor ID (예: 0x10DE = NVIDIA)
    // @param   buffer      [out] 파싱된 이름을 전달받을 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  없음
    // @detail  어셈블리 레벨에서 Vendor ID 매칭 테이블을 확인하여 텍스트 이름을 복사합니다.
    //***************************************************************************
    void pci_parse_vendor_name(uint16_t vendor_id, char* buffer, uint32_t buffer_size);

    //***************************************************************************
    // @brief   Class Code를 분석하여 디바이스 종류(GPU/NVMe 등)를 분류합니다.
    // @param   base_class [in] Base Class Code
    // @param   sub_class  [in] Sub Class Code
    // @param   prog_if    [in] Programming Interface Code
    // @return  uint32_t - 0: Unknown, 1: GPU, 2: NVMe
    // @detail  어셈블리 내부 비교 루틴을 통해 PCI 디바이스의 하드웨어 타입을 판별합니다.
    //***************************************************************************
    uint32_t pci_classify_device(uint8_t base_class, uint8_t sub_class, uint8_t prog_if);

    //***************************************************************************
    // @brief   시스템 내의 활성화된 모든 PCI 장치를 스캔합니다.
    // @param   out_devices [out] 결과 장치 정보 배열 포인터
    // @param   max_count   [in] 수집할 최대 장치 개수
    // @return  int - 탐색된 총 PCI 장치 수
    // @detail  SetupAPI를 호출하여 PCI 버스를 스캔하고 Configuration 파라미터 및 하드웨어 ID를 수집합니다.
    //***************************************************************************
    int pci_scan_devices(PciDeviceInfo* out_devices, uint32_t max_count);

    //***************************************************************************
    // @brief   시스템 내에 장착된 GPU(디스플레이 어댑터) 목록만 추출합니다.
    // @param   out_gpus  [out] GPU 정보 수신 배열 포인터
    // @param   max_count [in] 수집할 최대 GPU 개수
    // @return  int - 탐색된 GPU 장치 수
    // @detail  PCI 스캔 결과 중 PciDeviceClass::GPU 타입으로 식별된 장치만 필터링합니다.
    //***************************************************************************
    int pci_get_gpu_list(PciDeviceInfo* out_gpus, uint32_t max_count);

    //***************************************************************************
    // @brief   시스템 내에 장착된 NVMe 컨트롤러 목록만 추출합니다.
    // @param   out_nvmes [out] NVMe 정보 수신 배열 포인터
    // @param   max_count [in] 수집할 최대 NVMe 개수
    // @return  int - 탐색된 NVMe 컨트롤러 수
    // @detail  PCI 스캔 결과 중 PciDeviceClass::NVMe 타입으로 식별된 장치만 필터링합니다.
    //***************************************************************************
    int pci_get_nvme_list(PciDeviceInfo* out_nvmes, uint32_t max_count);
}

#endif // ndef __PCIINFO_H__