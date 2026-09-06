// ServerConfigTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "pch.h"

void MainClose()
{
	SERVER_CONFIG->ReleaseInstance();
	BaseGlobal::Destroy();
}

int main()
{
	//_CrtSetBreakAlloc(417);

#ifdef	_MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	InitUtf8Console();

	TCHAR tszTempArgv[FULLPATH_STRLEN] = { 0, };

	BaseGlobal::Init();

	//_sntprintf_s(tszTempArgv, FULLPATH_STRLEN, _TRUNCATE, _T("Config\\server_config_mssql.json"));
	_sntprintf_s(tszTempArgv, FULLPATH_STRLEN, _TRUNCATE, _T("Config\\server_config_mysql.json"));

	if( false == SERVER_CONFIG->Init(tszTempArgv) )
	{
		LOG_ERROR(_T("SERVER_CONFIG->Init Fail."));

		MainClose();
		return -1;
	}

	MainClose();
	CloseConsole();

	return 0;
}

