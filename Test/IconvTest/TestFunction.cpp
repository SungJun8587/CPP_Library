
//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"
#include <windows.h> // OutputDebugString 사용을 위해 필요
#include <cassert>

// 헬퍼 함수: 빌드 모드에 따라 출력 방식을 분기
inline void PrintString(const _tstring& message)
{
#ifdef _DEBUG
#ifdef _UNICODE
	OutputDebugString(message.c_str());
#else
	OutputDebugStringA(message.c_str());
#endif
#else
	// 릴리즈 모드: Win32 API를 이용한 안전한 파일 쓰기
	HANDLE hFile = CreateFile(_T("log.txt"), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE )
	{
#ifdef _UNICODE
		// 1. 유니코드(UTF-16)를 UTF-8로 변환
		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, NULL, 0, NULL, NULL);
		if( utf8Length > 0 )
		{
			std::string utf8Message(utf8Length - 1, '\0'); // null 문자 제외
			WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, &utf8Message[0], utf8Length, NULL, NULL);

			DWORD written = 0;
			WriteFile(hFile, utf8Message.c_str(), static_cast<DWORD>(utf8Message.length()), &written, NULL);
		}
#else
		// 멀티바이트 모드인 경우 그대로 기록
		DWORD written = 0;
		WriteFile(hFile, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
#endif
		CloseHandle(hFile);
	}
#endif
}

void TestIconvUtil()
{
	// 1. ANSI(CP949) <-> UTF-8 변환 테스트
	{
		PrintString(_T("[Test] Ansi <-> Utf8\n"));
		std::string original = "안녕하세요 .!@#$^&*()%% 0123456789 ABCDEF ghijklmn 成功(성공) 繁體字(번체자) 简体字(간체자)";
		std::string utf8 = Iconv::CIconvUtil::ConvertEncoding(original, "CP949", "UTF-8");
		std::string ansi = Iconv::CIconvUtil::ConvertEncoding(utf8, "UTF-8", "CP949");

#ifdef _UNICODE
		// _tstring 변환을 위한 간이 처리 (출력용)
		int wlen = MultiByteToWideChar(CP_ACP, 0, original.c_str(), -1, NULL, 0);
		std::wstring wOriginal(wlen - 1, L'\0');
		MultiByteToWideChar(CP_ACP, 0, original.c_str(), -1, &wOriginal[0], wlen);

		int wlenUtf8 = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
		std::wstring wUtf8(wlenUtf8 - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wUtf8[0], wlenUtf8);

		PrintString(_T(" - 원본(ANSI): ") + wOriginal + _T("\n"));
		PrintString(_T(" - UTF-8: ") + wUtf8 + _T("\n"));
#else
		PrintString(_T(" - 원본(ANSI): ") + original + _T("\n"));
		PrintString(_T(" - UTF-8: ") + utf8 + _T("\n"));
#endif

		assert(original == ansi);
		PrintString(_T(" -> Ansi <-> Utf8 테스트 성공!\n\n"));
	}

	// 2. ANSI(CP949) <-> WChar 변환 테스트
	{
		PrintString(_T("[Test] Ansi <-> WChar\n"));
		std::string original = "안녕하세요 .!@#$^&*()%% 0123456789 ABCDEF ghijklmn 成功(성공) 繁體字(번체자) 简体字(간체자)";
		std::wstring wchars = Iconv::CIconvUtil::ConvertEncodingW(original, "CP949", "UTF-16LE");
		std::string ansi = Iconv::CIconvUtil::ConvertEncoding(wchars, "UTF-16LE", "CP949");

#ifdef _UNICODE
		int wlen = MultiByteToWideChar(CP_ACP, 0, original.c_str(), -1, NULL, 0);
		std::wstring wOriginal(wlen - 1, L'\0');
		MultiByteToWideChar(CP_ACP, 0, original.c_str(), -1, &wOriginal[0], wlen);

		PrintString(_T(" - 원본(ANSI): ") + wOriginal + _T("\n"));
		PrintString(_T(" - WChar: ") + wchars + _T("\n"));
#else
		PrintString(_T(" - 원본(ANSI): ") + original + _T("\n"));
#endif

		assert(original == ansi);
		PrintString(_T(" -> Ansi <-> WChar 테스트 성공!\n\n"));
	}

	// 3. WChar <-> UTF-8 변환 테스트
	{
		PrintString(_T("[Test] WChar <-> Utf8\n"));
		std::wstring original = L"안녕하세요 .!@#$^&*()%% 0123456789 ABCDEF ghijklmn 成功(성공) 繁體字(번체자) 简体字(간체자)";
		std::string utf8 = Iconv::CIconvUtil::ConvertEncoding(original, "UTF-16LE", "UTF-8");
		std::wstring wchars = Iconv::CIconvUtil::ConvertEncodingW(utf8, "UTF-8", "UTF-16LE");

		PrintString(_T(" - 원본(WChar): ") + original + _T("\n"));

#ifdef _UNICODE
		int wlenUtf8 = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
		std::wstring wUtf8(wlenUtf8 - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wUtf8[0], wlenUtf8);

		PrintString(_T(" - UTF-8: ") + wUtf8 + _T("\n"));
#endif

		assert(original == wchars);
		PrintString(_T(" -> WChar <-> Utf8 테스트 성공!\n\n"));
	}
}