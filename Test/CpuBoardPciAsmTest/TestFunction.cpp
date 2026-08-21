
//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"

//***************************************************************************
// 1. C-Style 전역 CPU API 테스트
//***************************************************************************
void Test_GlobalCFunctions()
{
    _tcout << _T("[Test] C-Style Global CPU APIs...\n");

    // 1-1. cpu_id_supported 테스트
    int is_supported = cpu_id_supported();
    assert(is_supported == 1 && "CPUID indicator must be supported on target platform.");
    _tcout << _T("  - cpu_id_supported(): ") << is_supported << _T("\n");

    // 1-2. cpu_id 테스트
    unsigned long eax = 1, ebx = 0, ecx = 0, edx = 0;
    cpu_id(&eax, &ebx, &ecx, &edx);
    assert((eax != 0 || edx != 0) && "cpu_id() should return valid register values.");
    _tcout << _T("  - cpu_id(Leaf 1) EAX: 0x") << std::hex << eax << _T(", EDX: 0x") << edx << std::dec << _T("\n");

    // 1-3. cpu_vendor 테스트
    unsigned long highest_leaf = 0;
    char vendor_buf[16] = { 0 };
    int vendor_res = cpu_vendor(&highest_leaf, vendor_buf);
    assert(vendor_res == 1 && "cpu_vendor() must return 1 (Success).");
    assert(highest_leaf > 0 && "Highest CPUID leaf must be greater than 0.");
    assert(strlen(vendor_buf) > 0 && "Vendor string must not be empty.");
   _tcout << "  - cpu_vendor(): " << vendor_buf << " (Highest Leaf: " << highest_leaf << ")\n";

    // 1-4. cpu_brand_part0 ~ part5 및 cpu_brand 테스트
    char brand_buf[64] = { 0 };
    int brand_res = cpu_brand(brand_buf);
    if( brand_res ) {
       _tcout << "  - cpu_brand(): " << brand_buf << "\n";
    }

    // 1-5. cpu_cache_size_kb 테스트 (L1, L2, L3)
    unsigned int l1_cache = cpu_cache_size_kb(1);
    unsigned int l2_cache = cpu_cache_size_kb(2);
    unsigned int l3_cache = cpu_cache_size_kb(3);
    _tcout << _T("  - Cache Sizes -> L1: ") << l1_cache << _T("KB, L2: ") << l2_cache << _T("KB, L3: ") << l3_cache << _T("KB\n");

    // 1-6. cpu_core_type 테스트 (0x20: E-Core, 0x40: P-Core, 0: Legacy/Non-hybrid)
    unsigned int core_type = cpu_core_type();
    _tcout << _T("  - cpu_core_type(): 0x") << std::hex << core_type << std::dec << _T("\n");

    // 1-7. cpu_read_tsc 테스트
    uint64_t tsc1 = cpu_read_tsc();
    Sleep(10);
    uint64_t tsc2 = cpu_read_tsc();
    assert(tsc2 > tsc1 && "TSC counter must increment over time.");
    _tcout << _T("  - cpu_read_tsc() Delta: ") << (tsc2 - tsc1) << _T(" cycles\n\n");
}

//***************************************************************************
// 2. CpuID Helper 클래스 테스트
//***************************************************************************
void Test_CpuIDClass()
{
    _tcout << _T("[Test] CpuID Class Interface...\n");

    // Leaf 0 (Vendor Info & Basic Max Leaf)
    CpuID cpuid_leaf0(0, 0);
    assert(cpuid_leaf0.EAX() > 0 && "EAX from Leaf 0 must give max standard function ID.");

    // Leaf 1 (Processor Info & Feature Bits)
    CpuID cpuid_leaf1(1, 0);
    _tcout << _T("  - CpuID(1,0) -> EAX: 0x") << std::hex << cpuid_leaf1.EAX()
        << _T(", EBX: 0x") << cpuid_leaf1.EBX()
        << _T(", ECX: 0x") << cpuid_leaf1.ECX()
        << _T(", EDX: 0x") << cpuid_leaf1.EDX() << std::dec << _T("\n\n");
}

//***************************************************************************
// 3. CCpuInfo 클래스 전체 멤버 함수 테스트
//***************************************************************************
void Test_CCpuInfoClass()
{
    _tcout << _T("[Test] CCpuInfo Class Full Inspection...\n");

    CCpuInfo cpuInfo;

    // 3-1. GetInformation() 실행 테스트 (내부에서 DetectCpuGenInfo, DetectCpuDescInfo, DetectCpuSpeed 등 전역 호출)
    BOOL bRet = cpuInfo.GetInformation();
    assert(bRet == TRUE && "GetInformation() must succeed on valid hardware.");

    // 3-2. GetNameString() 직접 호출 테스트
    cpuInfo.GetNameString();

    // 3-3. Getter 퍼블릭 메서드 반환값 검증 및 출력
    unsigned int speedMHz = cpuInfo.GetSpeedMHz();
    const TCHAR* szProcessorName = cpuInfo.GetProcessorName();
    const TCHAR* szVendorName = cpuInfo.GetVendorName();
    int numProcessors = cpuInfo.GetNumberOfProcessors();
    int family = cpuInfo.GetCPUFamily();
    int model = cpuInfo.GetCPUModel();
    int stepping = cpuInfo.GetCPUStepping();

    assert(numProcessors > 0 && "Number of logical processors must be > 0.");
    assert(_tcslen(szVendorName) > 0 && "Vendor name string should not be empty.");

    _tcout << _T("  - Vendor Name     : ") << szVendorName << _T("\n");
    _tcout << _T("  - Processor Name  : ") << szProcessorName << _T("\n");
    _tcout << _T("  - Speed (MHz)     : ") << speedMHz << _T(" MHz\n");
    _tcout << _T("  - Processors Count: ") << numProcessors << _T("\n");
    _tcout << _T("  - Signature Info  : Family ") << family << _T(", Model ") << model << _T(", Stepping ") << stepping << _T("\n");

    // 3-4. 명령어 집합 지원 플래그 메서드 검증
    BOOL bMMX = cpuInfo.IsMMXSupported();
    BOOL bSSE = cpuInfo.IsSSESupported();
    BOOL bSSE2 = cpuInfo.IsSSE2Supported();
    BOOL b3DNow = cpuInfo.Is3DNowSupported();

    _tcout << _T("  - Instruction Set Flags:\n");
    _tcout << _T("    * MMX   : ") << (bMMX ? _T("Supported") : _T("Not Supported")) << _T("\n");
    _tcout << _T("    * SSE   : ") << (bSSE ? _T("Supported") : _T("Not Supported")) << _T("\n");
    _tcout << _T("    * SSE2  : ") << (bSSE2 ? _T("Supported") : _T("Not Supported")) << _T("\n");
    _tcout << _T("    * 3DNow!: ") << (b3DNow ? _T("Supported") : _T("Not Supported")) << _T("\n\n");
}


//***************************************************************************
// 1. BoardInfo API 전체 기능 단위 테스트
//***************************************************************************
void Test_BoardInfoClass()
{
    std::cout << "[Test] Starting SMBIOS Board & BIOS Information APIs Test...\n\n";

    constexpr unsigned int BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE] = { 0 };
    int result = 0;

    //***************************************************************************
    // 1. Motherboard Manufacturer (메인보드 제조사)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_board_manufacturer(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_board_manufacturer() should return 1 (Success).");
    assert(strlen(buffer) > 0 && "Board manufacturer string must not be empty.");
    std::cout << "  - Board Manufacturer : " << buffer << "\n";

    //***************************************************************************
    // 2. Motherboard Product Name (메인보드 모델명)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_board_product_name(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_board_product_name() should return 1 (Success).");
    assert(strlen(buffer) > 0 && "Board product name string must not be empty.");
    std::cout << "  - Board Product Name : " << buffer << "\n";

    //***************************************************************************
    // 3. Motherboard Serial Number (메인보드 시리얼 번호)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_board_serial_number(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_board_serial_number() should return 1 (Success).");
    std::cout << "  - Board Serial Number: " << buffer << "\n";

    //***************************************************************************
    // 4. BIOS Vendor (BIOS 제조사)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_bios_vendor(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_bios_vendor() should return 1 (Success).");
    assert(strlen(buffer) > 0 && "BIOS vendor string must not be empty.");
    std::cout << "  - BIOS Vendor        : " << buffer << "\n";

    //***************************************************************************
    // 5. BIOS Version (BIOS 버전)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_bios_version(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_bios_version() should return 1 (Success).");
    assert(strlen(buffer) > 0 && "BIOS version string must not be empty.");
    std::cout << "  - BIOS Version       : " << buffer << "\n";

    //***************************************************************************
    // 6. BIOS Release Date (BIOS 배포 날짜)
    //***************************************************************************
    memset(buffer, 0, BUFFER_SIZE);
    result = get_bios_release_date(buffer, BUFFER_SIZE);
    assert(result == 1 && "get_bios_release_date() should return 1 (Success).");
    assert(strlen(buffer) > 0 && "BIOS release date string must not be empty.");
    std::cout << "  - BIOS Release Date  : " << buffer << "\n\n";

    //***************************************************************************
    // 7. 예외 조건 테스트 (Invalid Parameters handling)
    //***************************************************************************
    std::cout << "[Test] Running Exception & Boundary Tests...\n";

    // NULL 버퍼 전달 시 0(실패) 반환 검증
    assert(get_board_manufacturer(nullptr, BUFFER_SIZE) == 0 && "Null buffer should return 0.");

    // 크기가 0인 버퍼 전달 시 0(실패) 반환 검증
    assert(get_bios_vendor(buffer, 0) == 0 && "Zero buffer size should return 0.");

    std::cout << "  - Boundary check passed (NULL/Zero size handling verified).\n\n";
}

//***************************************************************************
// Helper: PciDeviceInfo 단일 객체 출력용 함수
//***************************************************************************
void PrintDeviceInfo(const PciDeviceInfo& dev) {
    _tcout << _T("  - Bus/Dev/Fn   : [")
        << static_cast<int>(dev.bus) << _T(":")
        << static_cast<int>(dev.device) << _T(":")
        << static_cast<int>(dev.function) << _T("]\n");

    _tcout << _T("  - Vendor/Dev ID: 0x") << std::hex << std::uppercase << std::setfill(_T('0'))
        << std::setw(4) << dev.vendor_id << _T(" : 0x")
        << std::setw(4) << dev.device_id << std::dec << _T("\n");

    _tcout << _T("  - Class Code   : Base=0x") << std::hex << static_cast<int>(dev.base_class)
        << _T(", Sub=0x") << static_cast<int>(dev.sub_class)
        << _T(", ProgIF=0x") << static_cast<int>(dev.prog_if) << std::dec << _T("\n");

#ifdef _UNICODE
    _tcout << _T("  - Vendor Name  : ") << dev.vendor_name << _T("\n");
    _tcout << _T("  - Device Desc  : ") << dev.device_desc << _T("\n");
#else
    _tcout << _T("  - Vendor Name  : ") << dev.vendor_name << _T("\n");
    _tcout << _T("  - Device Desc  : ") << dev.device_desc << _T("\n");
#endif
    _tcout << _T("  --------------------------------------------------------\n");
}

//***************************************************************************
// 1. pci_classify_device() 테스트
//***************************************************************************
void Test_PciClassifyDevice() {
    _tcout << _T("[Test 1] pci_classify_device() 테스트 시작...\n");

    // GPU 테스트 (Base Class: 0x03)
    uint32_t type = pci_classify_device(0x03, 0x00, 0x00);
    assert(type == static_cast<uint32_t>(PciDeviceClass::GPU) && "Base class 0x03 must be classified as GPU.");

    // NVMe 테스트 (Base Class: 0x01, Sub Class: 0x08, ProgIF: 0x02)
    type = pci_classify_device(0x01, 0x08, 0x02);
    assert(type == static_cast<uint32_t>(PciDeviceClass::NVMe) && "Class 0x01/0x08/0x02 must be classified as NVMe.");

    // 알 수 없는 장치 테스트
    type = pci_classify_device(0x02, 0x00, 0x00);
    assert(type == static_cast<uint32_t>(PciDeviceClass::Unknown) && "Arbitrary class should be classified as Unknown.");

    _tcout << _T("  => pci_classify_device() 성공!\n\n");
}

//***************************************************************************
// 2. pci_parse_vendor_name() 테스트
//***************************************************************************
void Test_PciParseVendorName() {
    _tcout << _T("[Test 2] pci_parse_vendor_name() 테스트 시작...\n");

    char vendor_buf[32] = { 0 };

    // NVIDIA Vendor ID (0x10DE)
    pci_parse_vendor_name(0x10DE, vendor_buf, sizeof(vendor_buf));
    _tcout << _T("  - Vendor ID 0x10DE => Name: ") << vendor_buf << _T("\n");
    assert(strlen(vendor_buf) > 0 && "Vendor name must not be empty.");

    // Intel Vendor ID (0x8086)
    memset(vendor_buf, 0, sizeof(vendor_buf));
    pci_parse_vendor_name(0x8086, vendor_buf, sizeof(vendor_buf));
    _tcout << _T("  - Vendor ID 0x8086 => Name: ") << vendor_buf << _T("\n");
    assert(strlen(vendor_buf) > 0 && "Vendor name must not be empty.");

    _tcout << _T("  => pci_parse_vendor_name() 성공!\n\n");
}

//***************************************************************************
// 3. pci_scan_devices() 테스트
//***************************************************************************
void Test_PciScanDevices() {
    _tcout << _T("[Test 3] pci_scan_devices() 테스트 시작...\n");

    constexpr uint32_t MAX_DEVICES = 128;
    std::vector<PciDeviceInfo> devices(MAX_DEVICES);

    int count = pci_scan_devices(devices.data(), MAX_DEVICES);
    _tcout << _T("  - 탐색된 총 PCI 장치 수: ") << count << _T(" 개\n");

    assert(count >= 0 && "pci_scan_devices() return count cannot be negative.");

    if( count > 0 ) {
        _tcout << _T("\n  [첫 번째 스캔된 장치 예시]\n");
        PrintDeviceInfo(devices[0]);
    }

    _tcout << _T("  => pci_scan_devices() 성공!\n\n");
}

//***************************************************************************
// 4. pci_get_gpu_list() 테스트
//***************************************************************************
void Test_PciGetGpuList() {
    _tcout << _T("[Test 4] pci_get_gpu_list() 테스트 시작...\n");

    constexpr uint32_t MAX_GPUS = 16;
    std::vector<PciDeviceInfo> gpus(MAX_GPUS);

    int count = pci_get_gpu_list(gpus.data(), MAX_GPUS);
    _tcout << _T("  - 탐색된 GPU 개수: ") << count << _T(" 개\n");

    assert(count >= 0 && "pci_get_gpu_list() return count cannot be negative.");

    for( int i = 0; i < count; ++i ) {
        _tcout << _T("  [GPU #") << (i + 1) << _T("]\n");
        PrintDeviceInfo(gpus[i]);
        assert(gpus[i].type == PciDeviceClass::GPU && "Listed device type must be GPU.");
    }

    _tcout << _T("  => pci_get_gpu_list() 성공!\n\n");
}

//***************************************************************************
// 5. pci_get_nvme_list() 테스트
//***************************************************************************
void Test_PciGetNvmeList() {
    _tcout << _T("[Test 5] pci_get_nvme_list() 테스트 시작...\n");

    constexpr uint32_t MAX_NVMES = 16;
    std::vector<PciDeviceInfo> nvmes(MAX_NVMES);

    int count = pci_get_nvme_list(nvmes.data(), MAX_NVMES);
    _tcout << _T("  - 탐색된 NVMe 컨트롤러 개수: ") << count << _T(" 개\n");

    assert(count >= 0 && "pci_get_nvme_list() return count cannot be negative.");

    for( int i = 0; i < count; ++i ) {
        _tcout << _T("  [NVMe #") << (i + 1) << _T("]\n");
        PrintDeviceInfo(nvmes[i]);
        assert(nvmes[i].type == PciDeviceClass::NVMe && "Listed device type must be NVMe.");
    }

    _tcout << _T("  => pci_get_nvme_list() 성공!\n\n");
}