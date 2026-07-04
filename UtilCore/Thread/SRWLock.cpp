
//***************************************************************************
// SRWLock.cpp: implementation of the CSRWLock class.
//
//***************************************************************************

#include "pch.h"
#include "SRWLock.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CSRWLock::CSRWLock()
{
    // SRWLock 초기화 (동적 할당 불필요, 스택/멤버 변수로 사용 가능)
    InitializeSRWLock(&_srwLock);
}

CSRWLock::~CSRWLock()
{
    // SRWLock은 별도 Destroy API가 없음
    // 잠긴 상태로 소멸되면 UB — 디버그 빌드에서만 감지
#ifdef _DEBUG
    // TryWriteLock 성공 == 현재 아무도 잠그지 않은 상태
    const bool notLocked = TryExclusiveLock();
    assert(notLocked && "RWLock destroyed while locked");
    if (notLocked) ExclusiveUnLock();
#endif
}

//***************************************************************************
// 모든 읽기/쓰기 스레드가 해제될 때까지 블록
void CSRWLock::ExclusiveLock()
{
    AcquireSRWLockExclusive(&_srwLock);
}

//***************************************************************************
// 쓰기 락 비블로킹 시도 — 즉시 획득 가능하면 true, 아니면 false
// [[nodiscard]] : 반환값 무시 시 컴파일러 경고 발생
bool CSRWLock::TryExclusiveLock()
{
    return TryAcquireSRWLockExclusive(&_srwLock) != FALSE;
}

//***************************************************************************
// 쓰기 락 해제 — ExclusiveLock / TryExclusiveLock 성공 후 반드시 호출
void CSRWLock::ExclusiveUnLock()
{
    ReleaseSRWLockExclusive(&_srwLock);
}

//***************************************************************************
// 다른 읽기 스레드와 동시 획득 가능, 쓰기 스레드가 있으면 블록
void CSRWLock::SharedLock()
{
    AcquireSRWLockShared(&_srwLock);
}

//***************************************************************************
// 읽기 락 비블로킹 시도 — 즉시 획득 가능하면 true, 아니면 false
// [[nodiscard]] : 반환값 무시 시 컴파일러 경고 발생
bool CSRWLock::TrySharedLock()
{
    return TryAcquireSRWLockShared(&_srwLock) != FALSE;
}

//***************************************************************************
// 읽기 락 해제 — SharedLock / TrySharedLock 성공 후 반드시 호출
void CSRWLock::SharedUnLock()
{
    ::ReleaseSRWLockShared(&_srwLock);
}


//***************************************************************************
// ── ExclusiveLockGuard 구현 ─────────────────────────────────
//
// 생성 시 WriteLock 획득, 소멸 시 WriteUnlock 자동 호출
// 예외 발생 시에도 락 해제 보장
//***************************************************************************

//***************************************************************************
// 생성자에서 쓰기 락 획득 (블로킹)
ExclusiveLockGuard::ExclusiveLockGuard(CSRWLock& lock) : _lock(lock)
{
    _lock.ExclusiveLock();
}

//***************************************************************************
// 소멸자에서 쓰기 락 해제
ExclusiveLockGuard::~ExclusiveLockGuard()
{
    _lock.ExclusiveUnLock();
}


//***************************************************************************
// ── SharedLockGuard 구현 ────────────────────────────────────
//
// 생성 시 ReadLock 획득, 소멸 시 ReadUnlock 자동 호출
// 예외 발생 시에도 락 해제 보장
//***************************************************************************

//***************************************************************************
// 생성자에서 읽기 락 획득 (블로킹)
SharedLockGuard::SharedLockGuard(CSRWLock& lock) : _lock(lock)
{
    _lock.SharedLock();
}

//***************************************************************************
// 소멸자에서 읽기 락 해제
SharedLockGuard::~SharedLockGuard()
{
    _lock.SharedUnLock();
}


//***************************************************************************
// ── TryExclusiveLockGuard 구현 ──────────────────────────────
//
// 생성 시 TryWriteLock 시도, 획득 성공 시 소멸자에서 WriteUnlock 자동 호출
// IsAcquired()로 획득 여부 확인 후 임계 구역 진입
//***************************************************************************

//***************************************************************************
// 생성자에서 쓰기 락 비블로킹 시도
TryExclusiveLockGuard::TryExclusiveLockGuard(CSRWLock& lock)
    : _lock(lock), _acquired(lock.TryExclusiveLock())
{
}

//***************************************************************************
// 소멸자에서 획득 성공한 경우에만 쓰기 락 해제
TryExclusiveLockGuard::~TryExclusiveLockGuard()
{
    if (_acquired)
        _lock.ExclusiveUnLock();
}


//***************************************************************************
// ── TrySharedLockGuard 구현 ─────────────────────────────────
//
// 생성 시 TryReadLock 시도, 획득 성공 시 소멸자에서 ReadUnlock 자동 호출
// IsAcquired()로 획득 여부 확인 후 임계 구역 진입
//***************************************************************************

//***************************************************************************
// 생성자에서 읽기 락 비블로킹 시도
TrySharedLockGuard::TrySharedLockGuard(CSRWLock& lock)
    : _lock(lock), _acquired(lock.TrySharedLock())
{
}

//***************************************************************************
// 소멸자에서 획득 성공한 경우에만 읽기 락 해제
TrySharedLockGuard::~TrySharedLockGuard()
{
    if (_acquired)
        _lock.SharedUnLock();
}