
//***************************************************************************
// ConsoleUtil.h : 콘솔 입출력 관련 공용 유틸리티 함수 모음
//
//	- InitUtf8Console(), ClearConsoleScreen(), PauseConsole()은 원래
//	  BaseMacro.h의 매크로 또는 각 .cpp 파일 내부의 static 함수로 흩어져
//	  있었으나, 다음 이유로 이 헤더 하나로 모아 inline 함수로 통일합니다.
//
//	  1. 매크로 대신 함수를 쓰는 이유
//	     1-1. ClearConsoleScreen()/PauseConsole()처럼 호출부가 많은(SystemInfoTool.cpp
//	          기준 17회) 로직을 매크로로 두면, 전처리기가 호출부마다 본문을 그대로
//	          복사해 바이너리 크기가 호출 횟수만큼 불어납니다. 함수로 두면 구현은
//	          한 곳에만 존재하고 호출부는 call 하나씩만 남습니다.
//	     1-2. 매크로는 이름이 같은 실제 함수와 뒤섞이면 어느 쪽이 호출되는지
//	          헷갈리는 문제가 있어(전처리기가 먼저 텍스트를 치환), 이름 충돌
//	          여지를 원천적으로 없앱니다.
//	  2. 헤더에 정의하면서도 여러 .cpp에서 문제없이 쓰기 위해
//	     2-1. 모든 함수에 inline을 붙여, 이 헤더를 include하는 모든 번역
//	          단위(Wmi.cpp, HardwareInfo.cpp, SystemInfoTool.cpp 등)에서
//	          각자 정의를 갖더라도 링커 단계에서 중복 정의(ODR 위반) 오류가
//	          나지 않도록 합니다.
//
//***************************************************************************

#ifndef __CONSOLEUTIL_H__
#define __CONSOLEUTIL_H__

#pragma once

#include <clocale>

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#endif

//***************************************************************************
// @brief   콘솔 입출력을 UTF-8로 초기화합니다.
// @detail  [Windows] 콘솔 기본 코드페이지가 ANSI(CP949/CP1252 등)이므로
//          SetConsoleOutputCP(CP_UTF8) 호출이 필수적입니다. MSVC 환경에서는
//          ".UTF8" 로캘 표기를 지원합니다.
//          [Linux/macOS] 터미널 기본값이 이미 UTF-8이므로 setlocale(LC_ALL, "")
//          호출만으로 충분합니다.
//***************************************************************************
inline void InitUtf8Console()
{
#if defined(_WIN32) || defined(_WIN64)
	std::setlocale(LC_ALL, ".UTF8");
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#else
	std::setlocale(LC_ALL, "");
#endif
}

//***************************************************************************
// @brief   콘솔 화면을 지우고 커서를 좌상단(0,0)으로 이동시킵니다.
// @detail  system("cls")를 대체합니다. 새 프로세스를 생성하지 않고, 현재 콘솔
//          버퍼 전체를 공백 문자와 기본 속성으로 채운 뒤 커서 위치만
//          초기화하는 방식으로 동일한 시각적 효과를 냅니다. Windows 전용이며,
//          비Windows 환경에서는 아무 동작도 하지 않습니다.
//***************************************************************************
inline void ClearConsoleScreen()
{
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if( hConsole == INVALID_HANDLE_VALUE ) return;

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if( !GetConsoleScreenBufferInfo(hConsole, &csbi) ) return;

	DWORD dwConSize = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
	COORD coordOrigin = { 0, 0 };
	DWORD dwWritten = 0;

	FillConsoleOutputCharacter(hConsole, _T(' '), dwConSize, coordOrigin, &dwWritten);
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordOrigin, &dwWritten);
	SetConsoleCursorPosition(hConsole, coordOrigin);
#endif
}

//***************************************************************************
// @brief   안내 메시지를 출력하고 키 입력 1회를 대기합니다.
// @detail  system("pause")를 대체합니다. cmd.exe를 새로 띄우지 않고, Windows에서는
//          _getch()로 Enter 없이 아무 키 입력이나 즉시 감지합니다. 비Windows
//          환경에서는 표준 입력에서 한 줄을 읽어 대기합니다.
//***************************************************************************
inline void PauseConsole()
{
#if defined(_WIN32) || defined(_WIN64)
	_tprintf(_T("계속하려면 아무 키나 누르세요 . . .\n"));
	_getch();
#else
	std::printf("계속하려면 Enter 키를 누르세요 . . .\n");
	std::getchar();
#endif
}

//***************************************************************************
// @brief   종료 메시지를 출력하고 키 입력 1회를 대기합니다.
// @detail  system("pause")를 대체합니다. Release 빌드 환경에서만 대기 동작을
//          수행하며, Debug 빌드에서는 곧바로 종료됩니다.
//***************************************************************************
inline void CloseConsole()
{
#if defined(NDEBUG) // Release 빌드인 경우에만 동작
#if defined(_WIN32) || defined(_WIN64)
	_tprintf(_T("이 창을 닫으려면 아무 키나 누르세요 . . .\n"));
	_getch();
#else
	std::printf("이 창을 닫으려면 Enter 키를 누르세요 . . .\n");
	std::getchar();
#endif
#endif
}

#endif // ndef __CONSOLEUTIL_H__