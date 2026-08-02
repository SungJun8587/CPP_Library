
//***************************************************************************
// ShellUtilTest.cpp : Defines the entry point for the console application.
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

	_tcout.imbue(std::locale("korean")); // 유니코드 출력 설정
	_tcout << _T("================ ShellUtilTest 시작 ================\n\n");

	DirectoryAndFileOperations();
	RegistryOperations();
	FileHandleDuplicate();
	ProductKeyExtract();
	SHDirectory();
	GetRegistry();

	_tcout << _T("================ 모든 ShellUtilTest 완료 ================\n");

	system("pause");

	return 0;
}