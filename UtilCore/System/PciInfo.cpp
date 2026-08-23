//***************************************************************************
// PciInfo.cpp: implementation of the PCI bus enumeration class.
//
//***************************************************************************

#include "pch.h"
#include "PciInfo.h"

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

#pragma comment(lib, "setupapi.lib")

// PciInfo64.asm / PciInfo32.asm
extern "C" {
    void pci_parse_vendor_name(unsigned short vendor_id, char* buffer, unsigned int buffer_size);
    unsigned int pci_classify_device(unsigned char base_class, unsigned char sub_class, unsigned char prog_if);
}

namespace
{
    //***************************************************************************
    // @brief SMBIOS/드라이버에서 얻은 ANSI 문자열을 TCHAR 버퍼로 복사합니다.
    // @param dst      [out] 복사받을 TCHAR 버퍼
    // @param dstCount [in]  dst의 문자 개수(바이트 아님)
    // @param src      [in]  원본 ANSI(char*) 문자열
    // @return 없음
    // @detail asm이 반환하는 문자열은 항상 ANSI라, UNICODE 빌드일 때만 실제 변환이 발생함.
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

CPciInfo::CPciInfo()
{
}

CPciInfo::~CPciInfo()
{
    for( HWINFO_PCIDEVICE* p : m_sPciArray )
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
//         읽고 pci_parse_vendor_name/pci_classify_device(PciInfo64.asm/32.asm)로 후처리합니다.
//***************************************************************************
BOOL CPciInfo::GetInformation()
{
    for( HWINFO_PCIDEVICE* p : m_sPciArray )
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
        HWINFO_PCIDEVICE* pDev = new HWINFO_PCIDEVICE();

        // Bus / Device / Function
        DWORD busNum = 0, address = 0;
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_BUSNUMBER,
            NULL, (PBYTE)&busNum, sizeof(busNum), NULL) )
        {
            pDev->m_byBus = (BYTE)busNum;
        }
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_ADDRESS,
            NULL, (PBYTE)&address, sizeof(address), NULL) )
        {
            pDev->m_byDevice = (BYTE)((address >> 16) & 0xFFFF);
            pDev->m_byFunction = (BYTE)(address & 0xFFFF);
        }

        // Vendor ID / Device ID (Hardware ID 문자열 파싱: "PCI\VEN_10DE&DEV_2487&...")
        TCHAR hwIdBuf[512] = { 0 };
        if( SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID,
            NULL, (PBYTE)hwIdBuf, sizeof(hwIdBuf), NULL) )
        {
            unsigned int vId = 0, dId = 0;
            if( _stscanf_s(hwIdBuf, _T("PCI\\VEN_%04X&DEV_%04X"), &vId, &dId) == 2 )
            {
                pDev->m_wVendorId = (WORD)vId;
                pDev->m_wDeviceId = (WORD)dId;
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
                    pDev->m_byBaseClass = (BYTE)((ccVal >> 16) & 0xFF);
                    pDev->m_bySubClass = (BYTE)((ccVal >> 8) & 0xFF);
                    pDev->m_byProgIf = (BYTE)(ccVal & 0xFF);
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

        char vendorBuf[32] = { 0 };
        pci_parse_vendor_name(pDev->m_wVendorId, vendorBuf, sizeof(vendorBuf));
        CopyToTChar(pDev->m_tszVendorName, _countof(pDev->m_tszVendorName), vendorBuf);
        pDev->m_eType = (PciDeviceClass)pci_classify_device(pDev->m_byBaseClass, pDev->m_bySubClass, pDev->m_byProgIf);

        m_sPciArray.push_back(pDev);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return !m_sPciArray.empty();
}
