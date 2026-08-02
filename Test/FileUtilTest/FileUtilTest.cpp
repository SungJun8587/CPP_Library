// FileUtilTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"
#include "TestFunction.h"

// 실제 변수 공간 생성
_tstring g_DirPath = _T("");

// 프로그램 시작 시 (또는 main 함수 초반부) 경로를 세팅해 줍니다.
void InitGlobalPath() {
	std::filesystem::path currentPath = std::filesystem::current_path();
#ifdef UNICODE
	g_DirPath = currentPath.wstring();
#else
	g_DirPath = currentPath.string();
#endif
}

int main()
{
#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	InitGlobalPath();

	//Win_SaveFileTest();
	//Win_GetFileEncodingTypeTest();
	//Win_ReadFileTest();
	//Win_ReadFileMapTest();

	//GetFileEncodingTypeTest();
	//ReadFileTest();
	//SaveFileTest();

	system("pause");
}
