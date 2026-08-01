
//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"
#include <windows.h> // OutputDebugString 사용을 위해 필요

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

void TestWebUtil()
{
	_tcout.imbue(std::locale("korean")); // 유니코드 출력 설정
	_tcout << _T("================ WebUtil 테스트 시작 ================\n\n");

	// 1. Base64 인코딩 / 디코딩 테스트
	{
		PrintString(_T("[Test] Base64Enc / Base64Dec\n"));
		_tstring original = _T("Hello, WebUtil! 한글, 繁體字 (번체자) / 简体字 (간체자) 테스트");
		_tstring encoded = Base64Enc(original);

		Base64DecodeResult decResult;
		_tstring decoded = Base64Dec(encoded, &decResult);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(decResult == Base64DecodeResult::Success);
		assert(original == decoded);
		PrintString(_T(" -> Base64 테스트 성공!\n\n"));
	}

	// 2. URL 인코딩 / 디코딩 테스트
	{
		PrintString(_T("[Test] UrlEncode / UrlDecode\n"));
		_tstring original = _T("https://example.com/search?q=홍길동&hanja=繁體字/简体字&name=test user");
		_tstring encoded = UrlEncode(original, CP_UTF8);
		_tstring decoded = UrlDecode(encoded, CP_UTF8);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(original == decoded);
		PrintString(_T(" -> UrlEncode/Decode 테스트 성공!\n\n"));
	}

	// 3. URL Path 인코딩 테스트
	{
		PrintString(_T("[Test] UrlPathEncode\n"));
		_tstring original = _T("/path with space/繁體字폴더/简体字폴더/file.txt");
		_tstring encoded = UrlPathEncode(original);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" -> UrlPathEncode 테스트 성공!\n\n"));
	}

	// 4. HTML 인코딩 / 디코딩 테스트
	{
		PrintString(_T("[Test] HtmlEncode / HtmlDecode\n"));
		_tstring original = _T("<script>alert('XSS & \\\"") _T("繁體字/简体字") _T(" attack\\\");</script>");
		_tstring encoded = HtmlEncode(original);
		_tstring decoded = HtmlDecode(encoded);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(original == decoded);
		PrintString(_T(" -> HtmlEncode/Decode 테스트 성공!\n\n"));
	}

	// 5. JavaScript Escape / UnEscape 테스트
	{
		PrintString(_T("[Test] Escape / UnEscape\n"));
		_tstring original = _T("안녕 Hello 繁體字 简体字@*-_+./");
		_tstring encoded = Escape(original);
		_tstring decoded = UnEscape(encoded);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(original == decoded);
		PrintString(_T(" -> Escape/UnEscape 테스트 성공!\n\n"));
	}

	// 6. JavaScript EncodeURI / DecodeURI 테스트
	{
		PrintString(_T("[Test] EncodeURI / DecodeURI\n"));
		_tstring original = _T("http://ko.wikipedia.org/wiki/고양이?name=야옹이&hanja=繁體字/简体字");
		_tstring encoded = EncodeURI(original, false); // 공백을 %20으로 처리
		_tstring decoded = DecodeURI(encoded);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(original == decoded);
		PrintString(_T(" -> EncodeURI/DecodeURI 테스트 성공!\n\n"));
	}

	// 7. JavaScript EncodeURIComponent / DecodeURIComponent 테스트
	{
		PrintString(_T("[Test] EncodeURIComponent / DecodeURIComponent\n"));
		_tstring original = _T("param=繁體字/简体字/특수문자?&=");
		_tstring encoded = EncodeURIComponent(original);
		_tstring decoded = DecodeURIComponent(encoded);

		PrintString(_T(" - 원본: ") + original + _T("\n"));
		PrintString(_T(" - 인코딩: ") + encoded + _T("\n"));
		PrintString(_T(" - 디코딩: ") + decoded + _T("\n"));

		assert(original == decoded);
		PrintString(_T(" -> EncodeURIComponent/DecodeURIComponent 테스트 성공!\n\n"));
	}

	_tcout << _T("================ 모든 WebUtil 테스트 완료 ================\n");
}