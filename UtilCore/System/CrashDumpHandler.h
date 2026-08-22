
//***************************************************************************
// CrashDumpHandler.h : interface for the CrashDumpHandler class.
//
//***************************************************************************

#ifndef __CRASHDUMPHANDLER_H__
#define __CRASHDUMPHANDLER_H__

#pragma once

#include <cstddef>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <dbghelp.h>
#include <tchar.h>
#else
#include <csignal>
#include <atomic>

// Windows 전용 제네릭-텍스트 매크로(TCHAR/_T)를 POSIX 빌드에서도 동일한 API로
// 쓰기 위한 셔임. 프로젝트 공용 헤더에 이미 TCHAR가 정의되어 있다면 중복 정의를 피함.
#ifndef _T
#define _T(x) x
#endif
#ifndef TCHAR
typedef char TCHAR;
#endif

// POSIX에는 표준 와이드 문자 파일시스템 API(opendir/stat/execve 등)가 없어
// 이 클래스의 Linux 구현은 TCHAR가 char라는 전제 위에서 동작합니다.
// 셔임이 다른 곳(예: 프로젝트 공용 헤더)에서 wchar_t 등으로 재정의되면
// 여기서 컴파일 에러로 즉시 드러나도록 강제합니다.
static_assert(sizeof(TCHAR) == sizeof(char), "POSIX 빌드에서 TCHAR는 반드시 char여야 합니다 (표준 와이드 파일시스템 API 없음)");
#endif

//***************************************************************************
// @brief   덤프 기록 직후 호출되는 사용자 콜백 타입
// @param   p_bDumpSucceeded 덤프 기록 성공 여부
// @warning Linux: 이 콜백은 시그널 핸들러 컨텍스트에서 직접 호출되므로 내부에서
//          async-signal-safe 함수만 사용해야 함 (malloc/new, std::string, iostream,
//          mutex, printf류 전부 금지 — write() 등 제한된 함수만 안전).
//          Windows: SEH 필터 컨텍스트라 상대적으로 제약이 적지만, 짧고 예외를
//          던지지 않는 작업만 수행할 것을 권장.
//***************************************************************************
typedef void (*CrashCallback)(bool p_bDumpSucceeded);

//***************************************************************************
// @brief   프로세스 예외 발생 시 크래시 덤프(.dmp / core)를 자동 생성하는 클래스
// @detail  Windows 및 Linux 환경을 모두 지원하며, 싱글톤 패턴으로 동작합니다.
//          프로그램 초기화 시점에 initialize()를 호출하여 예외 핸들러를 등록합니다.
//          핸들러 본체는 async-signal-safe(POSIX) / 힙 비할당 원칙을 따르도록
//          설계되어, 힙이 손상된 상태에서 크래시가 발생해도 데드락 없이 동작합니다.
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
    // @param   p_tszDumpDir      덤프 파일을 저장할 디렉터리 (없으면 생성 시도)
    // @param   p_tszBaseName     덤프 파일 기본 이름 (확장자 제외)
    // @param   p_bFullMemory    전체 힙/메모리 포함 여부 (true: Full Dump, false: Mini Dump)
    // @param   p_nRetainCount   보관할 최신 덤프 파일 개수. 0이면 정리하지 않음.
    //                           초과분은 initialize() 호출 시점(일반 컨텍스트)에서 정리됨
    // @param   p_pCallback      덤프 기록 직후 호출할 콜백 (필요 없으면 nullptr)
    //***************************************************************************
    void initialize(
        const TCHAR* p_tszDumpDir = _T("."),
        const TCHAR* p_tszBaseName = _T("crash_dump"),
        bool p_bFullMemory = true,
        int p_nRetainCount = 10,
        CrashCallback p_pCallback = nullptr);

private:
    CrashDumpHandler() = default;
    ~CrashDumpHandler() = default;
    CrashDumpHandler(const CrashDumpHandler&) = delete;
    CrashDumpHandler& operator=(const CrashDumpHandler&) = delete;

    static constexpr size_t MAX_PATH_LEN = 512;

    // 고정 크기 버퍼: 핸들러 내부에서 동적 문자열 생성(힙 할당)을 피하기 위함
    TCHAR m_tszDumpDir[MAX_PATH_LEN] = _T(".");
    TCHAR m_tszBaseName[MAX_PATH_LEN] = _T("crash_dump");
    bool m_bFullMemory = true;
    int m_nRetainCount = 10;
    CrashCallback m_pCallback = nullptr;

    //***************************************************************************
    // @brief   보관 개수(m_nRetainCount)를 초과하는 오래된 덤프 파일을 정리합니다.
    // @detail  initialize() 호출 시점(일반 실행 컨텍스트)에서만 호출되므로
    //          디렉터리 탐색 등 signal-safe하지 않은 API를 사용해도 무방합니다.
    //***************************************************************************
    void cleanupOldDumps();

#if defined(_WIN32) || defined(_WIN64)
    //***************************************************************************
    // @brief   Windows 시스템의 Unhandled Exception 발생 시 호출되는 콜백 함수
    //***************************************************************************
    static LONG WINAPI winUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers);

    // PID + TickCount를 덤프 파일명에 반영해 매 크래시마다 고유한 파일을 생성
    // (기존엔 CREATE_ALWAYS로 매번 같은 파일을 덮어써서, 여러 번 크래시하면
    //  가장 마지막 것만 남는 문제가 있었음)
    static void buildDumpFilePath(TCHAR* p_tszOut, size_t p_nOutSize);

    static volatile LONG s_nHandling; // 재진입 방지 (InterlockedCompareExchange)
#else
    //***************************************************************************
    // @brief   Linux 시스템의 비정상 종료 시그널 수신 시 호출되는 핸들러 함수
    //***************************************************************************
    static void linuxSignalHandler(int nSignalNumber, siginfo_t* pInfo, void* pContext);

    // gcore의 절대 경로. initialize() 시점(시그널 컨텍스트 밖)에 미리 탐색해 캐시해둠으로써
    // 실제 시그널 핸들러에서는 PATH 탐색(execlp) 없이 execve만 호출하도록 함
    char m_szGcorePath[MAX_PATH_LEN] = "";

    // dumpDir + "/" + baseName을 initialize() 시점에 미리 합쳐 캐시 (gcore -o 인자).
    // gcore는 여기에 ".<pid>"를 자동으로 덧붙여 저장하므로 시그널 핸들러에서
    // 별도의 문자열 조합 작업이 필요 없음
    char m_szGcoreOPath[MAX_PATH_LEN] = "";

    void resolveGcorePath();

    static std::atomic<int> s_nHandling; // 재진입 방지 (멀티스레드 동시 크래시 대비)

    static constexpr size_t ALT_STACK_SIZE = 256 * 1024;
    static stack_t s_altStack;
    static char s_altStackBuf[ALT_STACK_SIZE]; // 스택 오버플로 시에도 핸들러가 동작하도록 하는 대체 스택
#endif
};

#endif // ndef __CRASHDUMPHANDLER_H__