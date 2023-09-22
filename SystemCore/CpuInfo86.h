
//***************************************************************************
// CpuInfo86.h: interface for the CpuInfo86 Functions.
//
//***************************************************************************

#ifndef __CPUINFO86_H__
#define __CPUINFO86_H__

extern "C" int __stdcall cpu_id_supported(); // returns true if CPUID is supported
extern "C" void __stdcall cpu_id(DWORD* eax, DWORD* ebx, DWORD* ecx, DWORD* edx); // eax, ebx, ecx and edx are [in/out] parameters
extern "C" int __stdcall cpu_vendor(DWORD* highestcpuid, char* vendorname);
extern "C" int __stdcall cpu_brand(char brand[]); // returns true if the brand was copied to the parameter; [brand] should be at least 48 bytes
extern "C" int __stdcall cpu_avx(); // returns true if AVX is supported
extern "C" int __stdcall cpu_avx2(); // returns true if AVX2 is supported
extern "C" int __stdcall cpu_mmx(); // returns true if MMX is supported
extern "C" int __stdcall cpu_sse(); // returns true if SSE is supported
extern "C" int __stdcall cpu_sse2(); // returns true if SSE2 is supported
extern "C" int __stdcall cpu_sse3(); // returns true if SSE3 is supported
extern "C" int __stdcall cpu_ssse3(); // returns true if SSSE3 is supported
extern "C" int __stdcall cpu_sse41(); // returns true if SSE41 is supported
extern "C" int __stdcall cpu_sse42(); // returns true if SSE42 is supported
extern "C" int __stdcall cpu_logical_processor_count(); // returns the number of logical processors if HT is a feature
extern "C" int __stdcall cpu_hyperthreading(); // returns true if HT is a feature

#endif // ndef __CPUINFO86_H__

