
//***************************************************************************
// CpuInfo64.h: interface for the CpuInfo64 Functions.
//
//***************************************************************************

#ifndef __CPUINFO64_H__
#define __CPUINFO64_H__

//***************************************************************************
// 리눅스에서는 파라미터를 전달할 때 6개 레지스터를 사용(인자가 7개 이상일 경우부터는 스택을 같이 사용)
//	- RDI : 첫번째 인자
//	- RSI : 두번째 인자
//	- RDX : 세번째 인자
//	- RCX : 네번째 인자
//	- R8 : 다섯번째 인자(추가된 범용 레지스터)
//	- R9 : 여섯번째 인자(추가된 범용 레지스터)
//	- [rsp + 8] : 일곱번째 인자
//	- [rsp + 16] : 여덟번째 인자
// 
// 윈도우는 리눅스와 달리 레지스터를 4개까지 사용(인자가 5개 이상일 경우부터는 스택을 같이 사용)
//	- RCX : 첫번째 인자
//	- RDX : 두번째 인자
//	- R8 : 세번째 인자(추가된 범용 레지스터)
//	- R9 : 네번째 인자(추가된 범용 레지스터)
//	- [rsp + 8] : 다섯번째 인자
//	- [rsp + 16] : 여섯번째 인자
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
