
//***************************************************************************
// TestFunction.h : interface for the Test Functions.
//
//***************************************************************************

#ifndef __TESTFUNCTION_H__
#define __TESTFUNCTION_H__

void Print(const char* pszContent, const char* pszBuffer);
void PrintW(const wchar_t* pwszContent, const wchar_t* pwszBuffer);
void PrintT(const TCHAR* ptszContent, const TCHAR* ptszBuffer);


void TestStringFunc();
void TestMemBuffer();
void TestString();

#endif // ndef __TESTFUNCTION_H__
