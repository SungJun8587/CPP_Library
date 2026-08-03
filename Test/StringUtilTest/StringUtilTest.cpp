
//***************************************************************************
// StringUtilTest.cpp : Defines the entry point for the console application.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"

//***************************************************************************
//
int main(int argc, char* argv[])
{
#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

	setlocale(LC_ALL, "");

	//TestStringFunc();
	//TestMemBuffer();
	TestString();

	return 0;
}
