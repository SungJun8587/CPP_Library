// CpuBoardPciAsmTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <pch.h>
#include "TestFunction.h"

//***************************************************************************
// @brief std::error_code의 메시지를 현재 프로젝트의 문자셋(_UNICODE 여부)에 맞는
//        _tstring 타입으로 안전하게 변환하는 헬퍼 함수
// @param ec 변환할 std::error_code 객체
// @return _UNICODE 환경에서는 std::wstring, 멀티바이트 환경에서는 std::string으로 변환된 문자열
//***************************************************************************
inline _tstring ErrorToString(const std::error_code& ec)
{
#ifdef _UNICODE
    std::string narrow = ec.message();
    return _tstring(narrow.begin(), narrow.end());
#else
    return ec.message();
#endif
}

//***************************************************************************
// @brief std::exception의 메시지를 현재 프로젝트의 문자셋(_UNICODE 여부)에 맞는
//        _tstring 타입으로 안전하게 변환하는 헬퍼 함수
// @param e 변환할 std::exception 객체
// @return _UNICODE 환경에서는 std::wstring, 멀티바이트 환경에서는 std::string으로 변환된 문자열
//***************************************************************************
inline _tstring ExceptionToString(const std::exception& e)
{
#ifdef _UNICODE
    std::string narrow = e.what();
    return _tstring(narrow.begin(), narrow.end());
#else
    return e.what();
#endif
}

int main()
{
#ifdef	_MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    InitUtf8Console();

    _tcout << _T("==================================================\n");
    _tcout << _T("      CCpuInfo & CPU Global API Unit Test         \n");
    _tcout << _T("==================================================\n\n");

    try {
        Test_GlobalCFunctions();
        Test_CpuIDClass();
        Test_CCpuInfoClass();

        _tcout << _T("[SUCCESS] All CPU Info tests passed successfully.\n\n");
    }
    catch( const std::exception& e ) {
        _tcerr << _T("[FAIL] Exception caught during test: ") << ExceptionToString(e) << std::endl << std::endl;
        return -1;
    }

    PauseConsole(); 
    ClearConsoleScreen();

    _tcout << _T("==================================================\n");
    _tcout << _T("       BoardInfo C-APIs Unit Test Suite           \n");
    _tcout << _T("==================================================\n\n");

    try {
        Test_BoardInfoClass();
        _tcout << _T("[SUCCESS] All BoardInfo API tests passed successfully!\n\n");
    }
    catch( const std::exception& e ) {
        _tcerr << _T("[FAIL] Exception caught: ") << ExceptionToString(e) << std::endl << std::endl;
        return -1;
    }
    
    PauseConsole();
    ClearConsoleScreen();

    _tcout << _T("========================================================\n");
    _tcout << _T("              PciInfo API 통합 테스트 실행             \n");
    _tcout << _T("========================================================\n\n");

    try {
        Test_PciClassifyDevice();
        Test_PciParseVendorName();
        Test_PciScanDevices();
        Test_PciGetGpuList();
        Test_PciGetNvmeList();

        _tcout << _T("========================================================\n");
        _tcout << _T("   모든 PciInfo API 함수 테스트가 성공적으로 완료됨!   \n");
        _tcout << _T("========================================================\n");
    }
    catch( const std::exception& e ) {
        _tcerr << _T("[오류 발생]: ") << e.what() << _T("\n");
        return -1;
    }

    CloseConsole();
}

