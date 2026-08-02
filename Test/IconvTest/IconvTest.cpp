// IconvTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include <iostream>
#include "TestFunction.h"

int main()
{
#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	_tcout.imbue(std::locale("korean")); // 유니코드 출력 설정
	_tcout << _T("================ IconvTest 시작 ================\n\n");

	TestIconvUtil();

	_tcout << _T("================ 모든 IconvTest 완료 ================\n");

	system("pause");
}
