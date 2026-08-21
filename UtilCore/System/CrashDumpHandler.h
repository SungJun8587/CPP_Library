
//***************************************************************************
// CrashDumpHandler.h : interface for the CrashDumpHandler class.
//
//***************************************************************************

#ifndef __CRASHDUMPHANDLER_H__
#define __CRASHDUMPHANDLER_H__

#pragma once

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <dbghelp.h>
#else
#include <csignal>
#endif

//***************************************************************************
// @brief   프로세스 예외 발생 시 크래시 덤프(.dmp / core)를 자동 생성하는 클래스
// @detail  Windows 및 Linux 환경을 모두 지원하며, 싱글톤 패턴으로 동작합니다.
//          프로그램 초기화 시점에 initialize()를 호출하여 예외 핸들러를 등록합니다.
//***************************************************************************
class CrashDumpHandler {
public:
    //***************************************************************************
    // @brief   CrashDumpHandler 싱글톤 인스턴스를 반환합니다.
    // @return  CrashDumpHandler& 싱글톤 객체 참조
    //***************************************************************************
    static CrashDumpHandler& getInstance();

    //***************************************************************************
    // @brief   예외 핸들러를 등록하고 덤프 수집 환경을 초기화합니다.
    // @param   p_szDumpPath 생성할 덤프 파일의 경로 및 파일 이름
    // @param   p_bFullMemory 전체 힙/메모리 포함 여부 (true: Full Dump, false: Mini Dump)
    //***************************************************************************
    void initialize(const std::string& p_szDumpPath = "crash_dump.dmp", bool p_bFullMemory = true);

private:
    CrashDumpHandler() = default;
    ~CrashDumpHandler() = default;
    CrashDumpHandler(const CrashDumpHandler&) = delete;
    CrashDumpHandler& operator=(const CrashDumpHandler&) = delete;

    std::string m_szDumpPath = "crash_dump.dmp"; // 생성될 덤프 파일 경로
    bool m_bFullMemory = true;                   // 풀 메모리 덤프 수집 여부

#if defined(_WIN32) || defined(_WIN64)
    //***************************************************************************
    // @brief   Windows 시스템의 Unhandled Exception 발생 시 호출되는 콜백 함수
    // @param   pExceptionPointers 예외 정보 포인터
    // @return  LONG 예외 처리 결과 (EXCEPTION_EXECUTE_HANDLER)
    //***************************************************************************
    static LONG WINAPI winUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers);
#else
    //***************************************************************************
    // @brief   Linux 시스템의 비정상 종료 시그널 수신 시 호출되는 핸들러 함수
    // @param   nSignalNumber 수신된 시그널 번호 (SIGSEGV, SIGABRT 등)
    //***************************************************************************
    static void linuxSignalHandler(int nSignalNumber);
#endif
};

#endif // ndef __CRASHDUMPHANDLER_H__