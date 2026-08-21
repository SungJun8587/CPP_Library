
//***************************************************************************
// PciInfo.cpp: Windows SetupAPI 기반 PCI 버스 장치 및 특수 클래스 열거 구현부
// 
//***************************************************************************

#include "pch.h"
#include "PciInfo.h"

extern "C" {

    //***************************************************************************
    // @brief   시스템 내의 활성화된 모든 PCI 장치를 스캔합니다.
    //***************************************************************************
    int pci_scan_devices(PciDeviceInfo* out_devices, uint32_t max_count) {
        if( !out_devices || max_count == 0 ) return 0;

        // 모든 장치 클래스에 대한 디바이스 정보 세트 가져오기
        HDEVINFO hDevInfo = SetupDiGetClassDevs(
            NULL,
            _T("PCI"),
            NULL,
            DIGCF_ALLCLASSES | DIGCF_PRESENT
        );

        if( hDevInfo == INVALID_HANDLE_VALUE ) {
            return 0;
        }

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        uint32_t count = 0;

        for( DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i ) {
            if( count >= max_count ) break;

            PciDeviceInfo dev = { 0 };

            // 1. Bus / Device / Function 번호 추출
            DWORD busNum = 0, address = 0;
            if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_BUSNUMBER, NULL, (PBYTE)&busNum, sizeof(busNum), NULL) ) {
                dev.bus = static_cast<uint8_t>(busNum);
            }

            if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_ADDRESS, NULL, (PBYTE)&address, sizeof(address), NULL) ) {
                dev.device = static_cast<uint8_t>((address >> 16) & 0xFFFF);
                dev.function = static_cast<uint8_t>(address & 0xFFFF);
            }

            // 2. Hardware ID 파싱 (Vendor ID, Device ID, Class Code)
            TCHAR hwIdBuf[512] = { 0 };
            if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)hwIdBuf, sizeof(hwIdBuf), NULL) ) {
                // Hardware ID 예시: "PCI\VEN_10DE&DEV_2487&SUBSYS_88031043&REV_A1"
                unsigned int vId = 0, dId = 0, bClass = 0, sClass = 0, pIf = 0;

                if( _stscanf_s(hwIdBuf, _T("PCI\\VEN_%04X&DEV_%04X"), &vId, &dId) == 2 ) {
                    dev.vendor_id = static_cast<uint16_t>(vId);
                    dev.device_id = static_cast<uint16_t>(dId);
                }

                // Compatible ID에서 Class Code 추출 시도
                TCHAR compIdBuf[512] = { 0 };
                if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_COMPATIBLEIDS, NULL, (PBYTE)compIdBuf, sizeof(compIdBuf), NULL) ) {
                    // 예시: "PCI\CC_030000"
                    TCHAR* pCC = _tcsstr(compIdBuf, _T("CC_"));
                    if( pCC ) {
                        unsigned int ccVal = 0;
                        if( _stscanf_s(pCC, _T("CC_%06X"), &ccVal) == 1 ) {
                            dev.base_class = static_cast<uint8_t>((ccVal >> 16) & 0xFF);
                            dev.sub_class = static_cast<uint8_t>((ccVal >> 8) & 0xFF);
                            dev.prog_if = static_cast<uint8_t>(ccVal & 0xFF);
                        }
                    }
                }
            }

            // 3. 디바이스 설명 문자열 가져오기
            TCHAR descBuf[128] = { 0 };
            if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)descBuf, sizeof(descBuf), NULL) ) {
#ifdef _UNICODE
                WideCharToMultiByte(CP_ACP, 0, descBuf, -1, dev.device_desc, sizeof(dev.device_desc), NULL, NULL);
#else
                strncpy_s(dev.device_desc, descBuf, sizeof(dev.device_desc) - 1);
#endif
            }

            // 4. ASM 함수를 이용한 제조사 파싱 및 디바이스 분류
            pci_parse_vendor_name(dev.vendor_id, dev.vendor_name, sizeof(dev.vendor_name));
            uint32_t typeVal = pci_classify_device(dev.base_class, dev.sub_class, dev.prog_if);
            dev.type = static_cast<PciDeviceClass>(typeVal);

            out_devices[count++] = dev;
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
        return count;
    }

    //***************************************************************************
    // @brief   시스템 내에 장착된 GPU 목록만 추출합니다.
    //***************************************************************************
    int pci_get_gpu_list(PciDeviceInfo* out_gpus, uint32_t max_count) {
        if( !out_gpus || max_count == 0 ) return 0;

        PciDeviceInfo tempBuffer[256];
        int total = pci_scan_devices(tempBuffer, 256);

        uint32_t gpuCount = 0;
        for( int i = 0; i < total; ++i ) {
            if( gpuCount >= max_count ) break;

            if( tempBuffer[i].type == PciDeviceClass::GPU || tempBuffer[i].base_class == 0x03 ) {
                out_gpus[gpuCount++] = tempBuffer[i];
            }
        }
        return gpuCount;
    }

    //***************************************************************************
    // @brief   시스템 내에 장착된 NVMe 컨트롤러 목록만 추출합니다.
    //***************************************************************************
    int pci_get_nvme_list(PciDeviceInfo* out_nvmes, uint32_t max_count) {
        if( !out_nvmes || max_count == 0 ) return 0;

        PciDeviceInfo tempBuffer[256];
        int total = pci_scan_devices(tempBuffer, 256);

        uint32_t nvmeCount = 0;
        for( int i = 0; i < total; ++i ) {
            if( nvmeCount >= max_count ) break;

            if( tempBuffer[i].type == PciDeviceClass::NVMe ||
                (tempBuffer[i].base_class == 0x01 && tempBuffer[i].sub_class == 0x08) ) {
                out_nvmes[nvmeCount++] = tempBuffer[i];
            }
        }
        return nvmeCount;
    }

}