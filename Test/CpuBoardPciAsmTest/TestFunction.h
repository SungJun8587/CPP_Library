
//***************************************************************************
// TestFunction.h : interface for the Test Functions.
//
//***************************************************************************

#ifndef __TESTFUNCTION_H__
#define __TESTFUNCTION_H__

void Test_GlobalCFunctions();
void Test_CpuIDClass();
void Test_CCpuInfoClass();

void Test_BoardInfoClass();

void Test_PciClassifyDevice();
void Test_PciParseVendorName();
void Test_PciScanDevices();
void Test_PciGetGpuList();
void Test_PciGetNvmeList();

#endif // ndef __TESTFUNCTION_H__