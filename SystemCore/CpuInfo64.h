
//***************************************************************************
// CpuInfo64.h: interface for the CpuInfo64 Functions.
//
//***************************************************************************

#ifndef __CPUINFO64_H__
#define __CPUINFO64_H__

//***************************************************************************
// ������������ �Ķ���͸� ������ �� 6�� �������͸� ���(���ڰ� 7�� �̻��� �����ʹ� ������ ���� ���)
//	- RDI : ù��° ����
//	- RSI : �ι�° ����
//	- RDX : ����° ����
//	- RCX : �׹�° ����
//	- R8 : �ټ���° ����(�߰��� ���� ��������)
//	- R9 : ������° ����(�߰��� ���� ��������)
//	- [rsp + 8] : �ϰ���° ����
//	- [rsp + 16] : ������° ����
// 
// ������� �������� �޸� �������͸� 4������ ���(���ڰ� 5�� �̻��� �����ʹ� ������ ���� ���)
//	- RCX : ù��° ����
//	- RDX : �ι�° ����
//	- R8 : ����° ����(�߰��� ���� ��������)
//	- R9 : �׹�° ����(�߰��� ���� ��������)
//	- [rsp + 8] : �ټ���° ����
//	- [rsp + 16] : ������° ����
//
//***************************************************************************

extern "C" int cpu_id_supported(); // returns true if CPUID is supported
extern "C" void cpu_id(unsigned long* eax, unsigned long* ebx, unsigned long* ecx, unsigned long* edx);
extern "C" int cpu_vendor(unsigned long* highestcpuid, char* vendorname);
extern "C" long long cpu_brand_part0(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" long long cpu_brand_part1(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" long long cpu_brand_part2(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" long long cpu_brand_part3(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" long long cpu_brand_part4(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" long long cpu_brand_part5(); // returns 8 byte string if brand is supported, or 0 if not supported
extern "C" int cpu_avx(); // returns true if AVX is supported
extern "C" int cpu_avx2(); // returns true if AVX2 is supported
extern "C" int cpu_mmx(); // returns true if MMX is supported
extern "C" int cpu_sse(); // returns true if SSE is supported
extern "C" int cpu_sse2(); // returns true if SSE2 is supported
extern "C" int cpu_sse3(); // returns true if SSE3 is supported
extern "C" int cpu_ssse3(); // returns true if SSSE3 is supported
extern "C" int cpu_sse41(); // returns true if SSE41 is supported
extern "C" int cpu_sse42(); // returns true if SSE42 is supported
extern "C" int cpu_logical_processor_count(); // returns the number of logical processors
extern "C" int cpu_hyperthreading(); // returns true if HT is a feature

#endif // ndef __CPUINFO64_H__
