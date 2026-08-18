
//***************************************************************************
// SRWLock.cpp: implementation of the CSRWLock class.
//
//***************************************************************************

#include "pch.h"
#include "SRWLock.h"

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
#include "DeadLockProfiler.h"
extern CDeadLockProfiler* gpDeadLockProfiler; // 외부 프로파일러 인스턴스 선언 가정
#endif

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief CSRWLock 객체를 생성하고 내부 Windows SRWLock 핸들을 초기화합니다.
//***************************************************************************
CSRWLock::CSRWLock()
{
    // SRWLock 초기화 (동적 할당 불필요, 스택/멤버 변수로 사용 가능)
    ::InitializeSRWLock(&_srwLock);
}

//***************************************************************************
// @brief CSRWLock 객체를 소멸합니다.
//***************************************************************************
CSRWLock::~CSRWLock()
{
    // SRWLock은 별도 Destroy API가 없음
    // 잠긴 상태로 소멸되면 UB — 디버그 빌드에서만 감지
#ifdef _DEBUG
    // TryExclusiveLock 성공 == 현재 아무도 잠그지 않은 상태
    const bool notLocked = TryExclusiveLock();
    assert(notLocked && "RWLock destroyed while locked");
    if( notLocked ) ExclusiveUnLock();
#endif
}

//***************************************************************************
// @brief 모든 읽기/쓰기 스레드가 해제될 때까지 블로킹 대기하며 쓰기 락을 획득합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
void CSRWLock::ExclusiveLock(const char* name)
{
#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PushLock(name);
#endif

    ::AcquireSRWLockExclusive(&_srwLock);
}

//***************************************************************************
// @brief 쓰기 락 획득을 비블로킹 방식으로 시도합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
// @return 쓰기 락 획득 성공 시 true, 실패 시 false
//***************************************************************************
[[nodiscard]] bool CSRWLock::TryExclusiveLock(const char* name)
{
    if( ::TryAcquireSRWLockExclusive(&_srwLock) == FALSE )
        return false;

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PushLock(name);
#endif
    return true;
}

//***************************************************************************
// @brief 쓰기 락을 해제합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
void CSRWLock::ExclusiveUnLock(const char* name)
{
    ::ReleaseSRWLockExclusive(&_srwLock);

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PopLock(name);
#endif
}

//***************************************************************************
// @brief 다른 읽기 스레드와 동시 획득 가능하며, 쓰기 스레드가 있으면 블로킹 대기합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
void CSRWLock::SharedLock(const char* name)
{
#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PushLock(name);
#endif

    ::AcquireSRWLockShared(&_srwLock);
}

//***************************************************************************
// @brief 읽기 락 획득을 비블로킹 방식으로 시도합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
// @return 읽기 락 획득 성공 시 true, 실패 시 false
//***************************************************************************
[[nodiscard]] bool CSRWLock::TrySharedLock(const char* name)
{
    if( ::TryAcquireSRWLockShared(&_srwLock) == FALSE )
        return false;

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PushLock(name);
#endif
    return true;
}

//***************************************************************************
// @brief 읽기 락을 해제합니다.
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
void CSRWLock::SharedUnLock(const char* name)
{
    ::ReleaseSRWLockShared(&_srwLock);

#if defined(USE_GPDEADLOCKPROFILER) && defined(_DEBUG)
    if( name && gpDeadLockProfiler ) gpDeadLockProfiler->PopLock(name);
#endif
}


//***************************************************************************
// ExclusiveLockGuard 구현
//
// 생성 시 WriteLock 획득, 소멸 시 WriteUnlock 자동 호출
// 예외 발생 시에도 락 해제 보장
//***************************************************************************

//***************************************************************************
// @brief 생성자에서 쓰기 락을 블로킹 방식으로 획득합니다.
// @param lock 관리할 CSRWLock 객체 참조
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
ExclusiveLockGuard::ExclusiveLockGuard(CSRWLock& lock, const char* name) noexcept
    : _lock(lock), _name(name)
{
    _lock.ExclusiveLock(_name);
}

//***************************************************************************
// @brief 소멸자에서 쓰기 락을 자동으로 해제합니다.
//***************************************************************************
ExclusiveLockGuard::~ExclusiveLockGuard() noexcept
{
    _lock.ExclusiveUnLock(_name);
}


//***************************************************************************
// SharedLockGuard 구현
//
// 생성 시 ReadLock 획득, 소멸 시 ReadUnlock 자동 호출
// 예외 발생 시에도 락 해제 보장
//***************************************************************************

//***************************************************************************
// @brief 생성자에서 읽기 락을 블로킹 방식으로 획득합니다.
// @param lock 관리할 CSRWLock 객체 참조
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
SharedLockGuard::SharedLockGuard(CSRWLock& lock, const char* name) noexcept
    : _lock(lock), _name(name)
{
    _lock.SharedLock(_name);
}

//***************************************************************************
// @brief 소멸자에서 읽기 락을 자동으로 해제합니다.
//***************************************************************************
SharedLockGuard::~SharedLockGuard() noexcept
{
    _lock.SharedUnLock(_name);
}


//***************************************************************************
// TryExclusiveLockGuard 구현
//
// 생성 시 TryWriteLock 시도, 획득 성공 시 소멸자에서 WriteUnlock 자동 호출
// IsAcquired()로 획득 여부 확인 후 임계 구역 진입
//***************************************************************************

//***************************************************************************
// @brief 생성자에서 쓰기 락 비블로킹 획득을 시도합니다.
// @param lock 관리할 CSRWLock 객체 참조
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
TryExclusiveLockGuard::TryExclusiveLockGuard(CSRWLock& lock, const char* name) noexcept
    : _lock(lock), _name(name), _acquired(lock.TryExclusiveLock(_name))
{
}

//***************************************************************************
// @brief 소멸자에서 락 획득에 성공했던 경우에만 쓰기 락을 해제합니다.
//***************************************************************************
TryExclusiveLockGuard::~TryExclusiveLockGuard() noexcept
{
    if( _acquired )
        _lock.ExclusiveUnLock(_name);
}


//***************************************************************************
// TrySharedLockGuard 구현
//
// 생성 시 TryReadLock 시도, 획득 성공 시 소멸자에서 ReadUnlock 자동 호출
// IsAcquired()로 획득 여부 확인 후 임계 구역 진입
//***************************************************************************

//***************************************************************************
// @brief 생성자에서 읽기 락 비블로킹 획득을 시도합니다.
// @param lock 관리할 CSRWLock 객체 참조
// @param name 데드락 프로파일러에 전달할 락 이름
//***************************************************************************
TrySharedLockGuard::TrySharedLockGuard(CSRWLock& lock, const char* name) noexcept
    : _lock(lock), _name(name), _acquired(lock.TrySharedLock(_name))
{
}

//***************************************************************************
// @brief 소멸자에서 락 획득에 성공했던 경우에만 읽기 락을 해제합니다.
//***************************************************************************
TrySharedLockGuard::~TrySharedLockGuard() noexcept
{
    if( _acquired )
        _lock.SharedUnLock(_name);
}