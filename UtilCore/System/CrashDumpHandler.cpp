
//***************************************************************************
// CrashDumpHandler.cpp : implementation of the CrashDumpHandler class.
//
//***************************************************************************

#include "pch.h"
#include "CrashDumpHandler.h"

#include <cstring>
#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "dbghelp.lib")
#include <vector>
#include <algorithm>
#include <string>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <string>

extern char** environ; // execve 호출 시 현재 환경변수 전달용
#endif

namespace {
    //***************************************************************************
    // @brief   char/TCHAR(wchar_t) 공용 signal-safe 문자열 복사 (널 종료 보장)
    //***************************************************************************
    template <typename T>
    void SafeStrCopy(T* p_pDst, size_t p_nDstSize, const T* p_pSrc) {
        if( p_pDst == nullptr || p_nDstSize == 0 ) return;
        if( p_pSrc == nullptr ) { p_pDst[0] = static_cast<T>(0); return; }

        size_t i = 0;
        for( ; i < p_nDstSize - 1 && p_pSrc[i] != static_cast<T>(0); ++i ) {
            p_pDst[i] = p_pSrc[i];
        }
        p_pDst[i] = static_cast<T>(0);
    }

    //***************************************************************************
    // @brief   char/TCHAR 공용 signal-safe strlen
    //***************************************************************************
    template <typename T>
    size_t SafeStrLen(const T* p_pStr) {
        size_t n = 0;
        while( p_pStr[n] != static_cast<T>(0) ) ++n;
        return n;
    }

    //***************************************************************************
    // @brief   char/TCHAR 공용 signal-safe 부호 없는 정수 -> 문자열 변환
    //          (snprintf/_stprintf류는 POSIX async-signal-safe 함수 목록에 없어
    //          시그널 핸들러 내부에서 사용을 피함)
    //***************************************************************************
    template <typename T>
    int SafeUIntToStr(unsigned long p_nVal, T* p_pBuf, int p_nBufSize) {
        T szTmp[24];
        int nIdx = 0;

        if( p_nVal == 0 ) {
            szTmp[nIdx++] = static_cast<T>('0');
        }
        while( p_nVal > 0 && nIdx < (int)(sizeof(szTmp) / sizeof(T)) ) {
            szTmp[nIdx++] = static_cast<T>('0' + (p_nVal % 10));
            p_nVal /= 10;
        }

        int nLen = 0;
        while( nIdx > 0 && nLen < p_nBufSize - 1 ) {
            p_pBuf[nLen++] = szTmp[--nIdx];
        }
        p_pBuf[nLen] = static_cast<T>(0);
        return nLen;
    }
}

//***************************************************************************
// @brief   CrashDumpHandler 싱글톤 인스턴스를 반환합니다.
// @return  CrashDumpHandler& 싱글톤 객체 참조
//***************************************************************************
CrashDumpHandler& CrashDumpHandler::getInstance() {
    static CrashDumpHandler instance;
    return instance;
}

#if defined(_WIN32) || defined(_WIN64)
volatile LONG CrashDumpHandler::s_nHandling = 0;
#else
std::atomic<int> CrashDumpHandler::s_nHandling{ 0 };
stack_t CrashDumpHandler::s_altStack{};
char CrashDumpHandler::s_altStackBuf[CrashDumpHandler::ALT_STACK_SIZE];

//***************************************************************************
// @brief   PATH 탐색 없이 알려진 후보 경로들에서 gcore 실행 파일을 찾아 캐시합니다.
// @detail  일반 실행 컨텍스트(시그널 핸들러 밖)에서 호출되므로 access() 등
//          async-signal-safe가 아닌 함수를 사용해도 무방합니다.
//***************************************************************************
void CrashDumpHandler::resolveGcorePath() {
    static const char* const candidates[] = {
        "/usr/bin/gcore",
        "/usr/local/bin/gcore",
        "/bin/gcore",
        "/opt/homebrew/bin/gcore",
    };

    m_szGcorePath[0] = '\0';
    for( const char* szCandidate : candidates ) {
        if( access(szCandidate, X_OK) == 0 ) {
            SafeStrCopy(m_szGcorePath, MAX_PATH_LEN, szCandidate);
            break;
        }
    }

    if( m_szGcorePath[0] == '\0' ) {
        std::fprintf(stderr,
            "[CrashDumpHandler] gcore not found in known paths. "
            "Crash dump will fall back to default core dump only.\n");
    }
}
#endif

//***************************************************************************
// @brief   보관 개수(m_nRetainCount)를 초과하는 오래된 덤프 파일을 정리합니다.
//***************************************************************************
void CrashDumpHandler::cleanupOldDumps() {
    if( m_nRetainCount <= 0 ) return;

#if defined(_WIN32) || defined(_WIN64)
    TCHAR tszPattern[MAX_PATH_LEN];
    SafeStrCopy(tszPattern, MAX_PATH_LEN, m_tszDumpDir);
    size_t nLen = SafeStrLen(tszPattern);
    if( nLen > 0 && tszPattern[nLen - 1] != _T('\\') && tszPattern[nLen - 1] != _T('/') ) {
        tszPattern[nLen++] = _T('\\');
        tszPattern[nLen] = 0;
    }
    SafeStrCopy(tszPattern + nLen, MAX_PATH_LEN - nLen, m_tszBaseName);
    _tcscat_s(tszPattern, MAX_PATH_LEN, _T("_*.dmp"));

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(tszPattern, &fd);
    if( hFind == INVALID_HANDLE_VALUE ) return;

    std::vector<std::pair<std::basic_string<TCHAR>, FILETIME>> files;
    do {
        if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
        TCHAR tszFull[MAX_PATH_LEN];
        SafeStrCopy(tszFull, MAX_PATH_LEN, m_tszDumpDir);
        size_t n = SafeStrLen(tszFull);
        if( n > 0 && tszFull[n - 1] != _T('\\') && tszFull[n - 1] != _T('/') ) {
            tszFull[n++] = _T('\\'); tszFull[n] = 0;
        }
        SafeStrCopy(tszFull + n, MAX_PATH_LEN - n, fd.cFileName);
        files.emplace_back(tszFull, fd.ftLastWriteTime);
    } while( FindNextFile(hFind, &fd) );
    FindClose(hFind);

    if( (int)files.size() <= m_nRetainCount ) return;

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return CompareFileTime(&a.second, &b.second) < 0; // 오래된 것이 앞으로
        });

    size_t nToDelete = files.size() - static_cast<size_t>(m_nRetainCount);
    for( size_t i = 0; i < nToDelete; ++i ) {
        DeleteFile(files[i].first.c_str());
    }
#else
    DIR* pDir = opendir(m_tszDumpDir);
    if( pDir == nullptr ) return;

    const size_t nPrefixLen = SafeStrLen(m_tszBaseName);
    std::vector<std::pair<std::string, time_t>> files;

    struct dirent* pEntry;
    while( (pEntry = readdir(pDir)) != nullptr ) {
        // pEntry->d_name은 POSIX dirent 규격상 항상 char* (와이드 버전 없음).
        // static_assert로 TCHAR==char가 보장되므로 이 비교는 안전함.
        bool bPrefixMatch = true;
        for( size_t i = 0; i < nPrefixLen; ++i ) {
            if( pEntry->d_name[i] == '\0' || pEntry->d_name[i] != m_tszBaseName[i] ) {
                bPrefixMatch = false;
                break;
            }
        }
        if( !bPrefixMatch ) continue;

        std::string szFullPath = std::string(m_tszDumpDir) + "/" + pEntry->d_name;
        struct stat st;
        if( stat(szFullPath.c_str(), &st) == 0 ) {
            files.emplace_back(szFullPath, st.st_mtime);
        }
    }
    closedir(pDir);

    if( (int)files.size() <= m_nRetainCount ) return;

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.second < b.second; // 오래된 것이 앞으로
        });

    size_t nToDelete = files.size() - static_cast<size_t>(m_nRetainCount);
    for( size_t i = 0; i < nToDelete; ++i ) {
        unlink(files[i].first.c_str());
    }
#endif
}

//***************************************************************************
// @brief   예외 핸들러를 등록하고 덤프 수집 환경을 초기화합니다.
//***************************************************************************
void CrashDumpHandler::initialize(
    const TCHAR* p_tszDumpDir,
    const TCHAR* p_tszBaseName,
    bool p_bFullMemory,
    int p_nRetainCount,
    CrashCallback p_pCallback) {

    if( p_tszDumpDir != nullptr ) SafeStrCopy(m_tszDumpDir, MAX_PATH_LEN, p_tszDumpDir);
    if( p_tszBaseName != nullptr ) SafeStrCopy(m_tszBaseName, MAX_PATH_LEN, p_tszBaseName);
    m_bFullMemory = p_bFullMemory;
    m_nRetainCount = p_nRetainCount;
    m_pCallback = p_pCallback;

#if defined(_WIN32) || defined(_WIN64)
    CreateDirectory(m_tszDumpDir, nullptr); // 이미 존재하면 실패해도 무시

    cleanupOldDumps();

    SetUnhandledExceptionFilter(winUnhandledExceptionFilter);
#else
    // 디렉터리가 없으면 생성 시도 (권한 0755)
    mkdir(m_tszDumpDir, 0755);

    cleanupOldDumps();

    // Linux: Core Dump 제한 해제
    struct rlimit core_limits;
    core_limits.rlim_cur = RLIM_INFINITY;
    core_limits.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &core_limits);

    // Linux: 전체 메모리 포함을 위한 coredump_filter 설정 (0x7f = All segments)
    if( m_bFullMemory ) {
        int nFd = open("/proc/self/coredump_filter", O_WRONLY);
        if( nFd >= 0 ) {
            const char szFilter[] = "0x7f\n";
            write(nFd, szFilter, sizeof(szFilter) - 1);
            close(nFd);
        }
    }

    // gcore 절대 경로를 미리 탐색해 캐시 (시그널 핸들러에서 PATH 탐색을 피하기 위함)
    resolveGcorePath();

    // gcore -o에 넘길 최종 경로(디렉터리+베이스네임)를 미리 조합해 캐시
    // (gcore가 여기에 ".<pid>"를 자동으로 덧붙여 파일을 생성함)
    {
        size_t nLen = 0;
        SafeStrCopy(m_szGcoreOPath, MAX_PATH_LEN, m_tszDumpDir);
        nLen = SafeStrLen(m_szGcoreOPath);
        if( nLen > 0 && m_szGcoreOPath[nLen - 1] != '/' ) {
            m_szGcoreOPath[nLen++] = '/';
            m_szGcoreOPath[nLen] = '\0';
        }
        SafeStrCopy(m_szGcoreOPath + nLen, MAX_PATH_LEN - nLen, m_tszBaseName);
    }

    // 스택 오버플로로 인한 SIGSEGV에서도 핸들러가 실행되도록 대체 스택 등록
    s_altStack.ss_sp = s_altStackBuf;
    s_altStack.ss_size = sizeof(s_altStackBuf);
    s_altStack.ss_flags = 0;
    sigaltstack(&s_altStack, nullptr);

    // 비정상 종료 시그널 등록
    // SA_ONSTACK   : 위에서 등록한 대체 스택에서 핸들러 실행
    // SA_RESETHAND : 핸들러 진입과 동시에 해당 시그널을 기본 동작(SIG_DFL)으로 복원
    //                -> 처리 후 raise()만 하면 정상적으로 core dump/종료되며,
    //                   같은 시그널이 핸들러 실행 중 재발생해도 무한 재귀에 빠지지 않음
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = linuxSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
#endif

    // 여기는 시그널/예외 컨텍스트 밖이므로 일반 I/O 사용 가능
#if defined(_WIN32) || defined(_WIN64)
    _tprintf(_T("[CrashDumpHandler] Initialized. Dir: %s, Base: %s (Full Memory: %s, Retain: %d)\n"),
        m_tszDumpDir, m_tszBaseName, m_bFullMemory ? _T("ON") : _T("OFF"), m_nRetainCount);
#else
    std::fprintf(stdout, "[CrashDumpHandler] Initialized. Dir: %s, Base: %s (Full Memory: %s, Retain: %d)\n",
        m_tszDumpDir, m_tszBaseName, m_bFullMemory ? "ON" : "OFF", m_nRetainCount);
#endif
}

#if defined(_WIN32) || defined(_WIN64)
//***************************************************************************
// @brief   PID + TickCount를 조합해 매 크래시마다 고유한 덤프 파일 경로를 생성합니다.
//***************************************************************************
void CrashDumpHandler::buildDumpFilePath(TCHAR* p_tszOut, size_t p_nOutSize) {
    CrashDumpHandler& inst = getInstance();

    SafeStrCopy(p_tszOut, p_nOutSize, inst.m_tszDumpDir);
    size_t nLen = SafeStrLen(p_tszOut);
    if( nLen > 0 && p_tszOut[nLen - 1] != _T('\\') && p_tszOut[nLen - 1] != _T('/') ) {
        p_tszOut[nLen++] = _T('\\');
        p_tszOut[nLen] = 0;
    }

    SafeStrCopy(p_tszOut + nLen, p_nOutSize - nLen, inst.m_tszBaseName);
    nLen = SafeStrLen(p_tszOut);
    p_tszOut[nLen++] = _T('_'); p_tszOut[nLen] = 0;

    TCHAR tszNum[24];
    SafeUIntToStr(static_cast<unsigned long>(GetCurrentProcessId()), tszNum, 24);
    SafeStrCopy(p_tszOut + nLen, p_nOutSize - nLen, tszNum);
    nLen = SafeStrLen(p_tszOut);
    p_tszOut[nLen++] = _T('_'); p_tszOut[nLen] = 0;

    SafeUIntToStr(static_cast<unsigned long>(GetTickCount64()), tszNum, 24);
    SafeStrCopy(p_tszOut + nLen, p_nOutSize - nLen, tszNum);
    nLen = SafeStrLen(p_tszOut);

    SafeStrCopy(p_tszOut + nLen, p_nOutSize - nLen, _T(".dmp"));
}

//***************************************************************************
// @brief   Windows 시스템의 Unhandled Exception 발생 시 호출되는 콜백 함수
// @param   pExceptionPointers 예외 정보 포인터
// @return  LONG 예외 처리 결과 (EXCEPTION_EXECUTE_HANDLER)
//***************************************************************************
LONG WINAPI CrashDumpHandler::winUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers) {
    // 재진입 방지: 동시에 여러 스레드에서 예외가 발생해도 한 번만 처리
    if( InterlockedCompareExchange(&s_nHandling, 1, 0) != 0 ) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    CrashDumpHandler& inst = getInstance();

    TCHAR tszPath[MAX_PATH_LEN];
    buildDumpFilePath(tszPath, MAX_PATH_LEN);

    HANDLE hFile = CreateFile(
        tszPath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    bool bSucceeded = false;

    if( hFile == INVALID_HANDLE_VALUE ) {
        TCHAR tszMsg[300];
        _stprintf_s(tszMsg, 300, _T("[CrashDumpHandler] CreateFile failed. path=%s, err=%lu\n"),
            tszPath, GetLastError());
        OutputDebugString(tszMsg);
    }
    else {
        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = pExceptionPointers;
        exceptionInfo.ClientPointers = FALSE;

        MINIDUMP_TYPE dumpType = MiniDumpNormal;
        if( inst.m_bFullMemory ) {
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
        bSucceeded = (isDumpWritten != FALSE);

        TCHAR tszMsg[300];
        if( bSucceeded ) {
            _stprintf_s(tszMsg, 300, _T("[CrashDumpHandler] Dump saved to: %s\n"), tszPath);
        }
        else {
            _stprintf_s(tszMsg, 300, _T("[CrashDumpHandler] MiniDumpWriteDump failed. err=%lu\n"), GetLastError());
        }
        OutputDebugString(tszMsg);
    }

    if( inst.m_pCallback != nullptr ) {
        inst.m_pCallback(bSucceeded);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#else
//***************************************************************************
// @brief   Linux 시스템의 비정상 종료 시그널 수신 시 호출되는 핸들러 함수
// @detail  POSIX async-signal-safe 함수만 사용합니다 (write/fork/execve/waitpid/
//          raise/pause 등). std::string, iostream, malloc, std::system() 등
//          내부적으로 락/힙 할당을 수반하는 호출은 사용하지 않습니다 — 힙이
//          손상된 상태에서 크래시가 발생해도 핸들러가 데드락에 빠지지 않도록 하기 위함.
// @param   nSignalNumber 수신된 시그널 번호 (SIGSEGV, SIGABRT 등)
//***************************************************************************
void CrashDumpHandler::linuxSignalHandler(int nSignalNumber, siginfo_t* /*pInfo*/, void* /*pContext*/) {
    // 재진입 방지: 멀티스레드 동시 크래시 시 한 스레드만 덤프를 생성
    // 처리 중이 아닌 스레드는 pause()로 대기 (곧 SA_RESETHAND에 의해 프로세스가 종료됨)
    int nExpected = 0;
    if( !s_nHandling.compare_exchange_strong(nExpected, 1) ) {
        while( true ) {
            pause();
        }
    }

    CrashDumpHandler& inst = getInstance();
    bool bSucceeded = false;

    const char szMsg1[] = "[CrashDumpHandler] Signal caught.\n";
    write(STDERR_FILENO, szMsg1, sizeof(szMsg1) - 1);

    if( inst.m_szGcorePath[0] == '\0' ) {
        // initialize() 시점에 gcore를 찾지 못한 경우 fork/exec 자체를 시도하지 않음
        const char szMsgNoGcore[] = "[CrashDumpHandler] gcore path unresolved. Fallback to default core dump.\n";
        write(STDERR_FILENO, szMsgNoGcore, sizeof(szMsgNoGcore) - 1);
    }
    else {
        pid_t nPid = fork();
        if( nPid == 0 ) {
            // 자식 프로세스: gcore로 부모(크래시한 원본 프로세스)를 덤프
            // execve에 절대 경로를 직접 넘겨 PATH 탐색(execlp 내부 동작)을 피함 —
            // PATH 검색 로직은 POSIX가 async-signal-safe로 명시한 목록에 없음
            char szPidBuf[16];
            SafeUIntToStr(static_cast<unsigned long>(getppid()), szPidBuf, sizeof(szPidBuf));

            char* argv[] = {
                inst.m_szGcorePath,
                const_cast<char*>("-o"),
                inst.m_szGcoreOPath,
                szPidBuf,
                nullptr
            };

            execve(inst.m_szGcorePath, argv, environ);
            _exit(127); // execve 실패 시에만 도달
        }
        else if( nPid > 0 ) {
            int nStatus = 0;
            waitpid(nPid, &nStatus, 0);

            bSucceeded = WIFEXITED(nStatus) && WEXITSTATUS(nStatus) == 0;
            if( bSucceeded ) {
                const char szMsg2[] = "[CrashDumpHandler] Linux Dump saved using gcore.\n";
                write(STDERR_FILENO, szMsg2, sizeof(szMsg2) - 1);
            }
            else {
                const char szMsg3[] = "[CrashDumpHandler] gcore failed. Fallback to default core dump.\n";
                write(STDERR_FILENO, szMsg3, sizeof(szMsg3) - 1);
            }
        }
        else {
            const char szMsg4[] = "[CrashDumpHandler] fork() failed. Fallback to default core dump.\n";
            write(STDERR_FILENO, szMsg4, sizeof(szMsg4) - 1);
        }
    }

    if( inst.m_pCallback != nullptr ) {
        // 주의: 콜백 내부도 async-signal-safe 규칙을 지켜야 함 (헤더 주석 참고)
        inst.m_pCallback(bSucceeded);
    }

    // SA_RESETHAND에 의해 이미 해당 시그널의 동작이 SIG_DFL로 복원된 상태이므로
    // 재-raise하면 커널이 기본 처리(코어덤프 생성 및 프로세스 종료)를 수행함
    raise(nSignalNumber);
}
#endif