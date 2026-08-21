
//***************************************************************************
// CrashDumpHandler.cpp : implementation of the CrashDumpHandler class.
//
//***************************************************************************

#include "pch.h"
#include "CrashDumpHandler.h"

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "dbghelp.lib")
#else
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <sys/resource.h>
#endif

//***************************************************************************
// @brief   CrashDumpHandler 싱글톤 인스턴스를 반환합니다.
// @return  CrashDumpHandler& 싱글톤 객체 참조
//***************************************************************************
CrashDumpHandler& CrashDumpHandler::getInstance() {
    static CrashDumpHandler instance;
    return instance;
}

//***************************************************************************
// @brief   예외 핸들러를 등록하고 덤프 수집 환경을 초기화합니다.
// @param   p_szDumpPath 생성할 덤프 파일의 경로 및 파일 이름
// @param   p_bFullMemory 전체 힙/메모리 포함 여부 (true: Full Dump, false: Mini Dump)
//***************************************************************************
void CrashDumpHandler::initialize(const std::string& p_szDumpPath, bool p_bFullMemory) {
    m_szDumpPath = p_szDumpPath;
    m_bFullMemory = p_bFullMemory;

#if defined(_WIN32) || defined(_WIN64)
    SetUnhandledExceptionFilter(winUnhandledExceptionFilter);
#else
    // Linux: Core Dump 제한 해제
    struct rlimit core_limits;
    core_limits.rlim_cur = RLIM_INFINITY;
    core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    // Linux: 전체 메모리 포함을 위한 coredump_filter 설정 (0x7F = All segments)
    if( m_bFullMemory ) {
        std::ofstream filter("/proc/self/coredump_filter");
        if( filter.is_open() ) {
            filter << "0x7f";
            filter.close();
        }
    }

    // 비정상 종료 시그널 등록
    std::signal(SIGSEGV, linuxSignalHandler);
    std::signal(SIGABRT, linuxSignalHandler);
    std::signal(SIGFPE, linuxSignalHandler);
    std::signal(SIGILL, linuxSignalHandler);
#endif

    std::cout << "[CrashDumpHandler] Initialized. Path: " << m_szDumpPath
        << " (Full Memory: " << (m_bFullMemory ? "ON" : "OFF") << ")" << std::endl;
}

#if defined(_WIN32) || defined(_WIN64)
//***************************************************************************
// @brief   Windows 시스템의 Unhandled Exception 발생 시 호출되는 콜백 함수
// @param   pExceptionPointers 예외 정보 포인터
// @return  LONG 예외 처리 결과 (EXCEPTION_EXECUTE_HANDLER)
//***************************************************************************
LONG WINAPI CrashDumpHandler::winUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers) {
    std::string szPath = getInstance().m_szDumpPath;
    bool bIsFull = getInstance().m_bFullMemory;

    HANDLE hFile = CreateFileA(
        szPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if( hFile != INVALID_HANDLE_VALUE ) {
        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = pExceptionPointers;
        exceptionInfo.ClientPointers = FALSE;

        MINIDUMP_TYPE dumpType = MiniDumpNormal;
        if( bIsFull ) {
            dumpType = static_cast<MINIDUMP_TYPE>(
                MiniDumpWithFullMemory |     // 모든 접근 가능한 힙/스택 메모리 포함
                MiniDumpWithHandleData |     // 커널 핸들 정보 포함
                MiniDumpWithThreadInfo       // 스레드 상세 정보 포함
                );
        }

        BOOL isDumpWritten = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            dumpType,
            &exceptionInfo,
            NULL,
            NULL
        );

        CloseHandle(hFile);

        if( isDumpWritten ) {
            std::cerr << "[CrashDumpHandler] Dump saved to: " << szPath << std::endl;
        }
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#else
//***************************************************************************
// @brief   Linux 시스템의 비정상 종료 시그널 수신 시 호출되는 핸들러 함수
// @param   nSignalNumber 수신된 시그널 번호 (SIGSEGV, SIGABRT 등)
//***************************************************************************
void CrashDumpHandler::linuxSignalHandler(int nSignalNumber) {
    std::string szPath = getInstance().m_szDumpPath;
    std::cerr << "[CrashDumpHandler] Signal caught (" << nSignalNumber << ")." << std::endl;

    std::string szCommand = "gcore -o " + szPath + " " + std::to_string(getpid());
    int nResult = std::system(szCommand.c_str());

    if( nResult == 0 ) {
        std::cerr << "[CrashDumpHandler] Linux Dump saved using gcore." << std::endl;
    }
    else {
        std::cerr << "[CrashDumpHandler] Fallback to default core dump." << std::endl;
    }

    std::signal(nSignalNumber, SIG_DFL);
    std::raise(nSignalNumber);
}
#endif