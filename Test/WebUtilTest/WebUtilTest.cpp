// WebUtilTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "TestFunction.h"

int main()
{
#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

	_tcout.imbue(std::locale("korean")); // 유니코드 출력 설정
	_tcout << _T("================ WebUtilTest 시작 ================\n\n");

	TestWebUtil();

	_tcout << _T("================ 모든 WebUtilTest 완료 ================\n");

	system("pause");

	return 0;
}
