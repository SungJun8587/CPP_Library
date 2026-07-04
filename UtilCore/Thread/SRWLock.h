#pragma once
#ifndef __SRWLOCK_H__
#define __SRWLOCK_H__

#include <windows.h>

// ============================================================
// RWLock — Windows SRWLock 기반 읽기/쓰기 락 클래스
//
// - 읽기(공유) 락 : 여러 스레드가 동시에 획득 가능
// - 쓰기(독점) 락 : 한 스레만 획득 가능, 읽기도 블록
// - SRWLock은 재진입(recursive) 불가 — 같은 스레드 중첩 잠금 시 데드락
// - ReadLock → WriteLock 승격 불가 — 데드락 유발
// ============================================================

class CSRWLock
{
public:
    CSRWLock();
    ~CSRWLock();

    // 복사/이동 금지 — 락 객체는 고정된 주소를 가져야 함
    CSRWLock(const CSRWLock&) = delete;
    CSRWLock& operator=(const CSRWLock&) = delete;
    CSRWLock(CSRWLock&&) = delete;
    CSRWLock& operator=(CSRWLock&&) = delete;

    // ── 쓰기 락 (Exclusive) ──────────────────────────────────
    void ExclusiveLock();
    [[nodiscard]] bool TryExclusiveLock();
    void ExclusiveUnLock();

    // ── 읽기 락 (Shared) ─────────────────────────────────────
    void SharedLock();
    [[nodiscard]] bool TrySharedLock();
    void SharedUnLock();

    // ── 조건 변수 연동 ────────────────────────────────────────
    SRWLOCK* NativeHandle() { return &_srwLock; }

private:
    SRWLOCK _srwLock;   // Windows SRWLock 핸들 (포인터 크기, 8 bytes)
};


// ============================================================
// ExclusiveLockGuard — 쓰기 락 RAII 가드
// ============================================================
class ExclusiveLockGuard
{
public:
    explicit ExclusiveLockGuard(CSRWLock& lock);
    ~ExclusiveLockGuard();

    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;
};


// ============================================================
// SharedLockGuard — 읽기 락 RAII 가드
// ============================================================
class SharedLockGuard
{
public:
    explicit SharedLockGuard(CSRWLock& lock);
    ~SharedLockGuard();

    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;

private:
    CSRWLock& _lock;
};


// ============================================================
// TryExclusiveLockGuard — 쓰기 락 Try RAII 가드
// ============================================================
class TryExclusiveLockGuard
{
public:
    explicit TryExclusiveLockGuard(CSRWLock& lock);
    ~TryExclusiveLockGuard();

    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TryExclusiveLockGuard(const TryExclusiveLockGuard&) = delete;
    TryExclusiveLockGuard& operator=(const TryExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;
    bool      _acquired;
};


// ============================================================
// TrySharedLockGuard — 읽기 락 Try RAII 가드
// ============================================================
class TrySharedLockGuard
{
public:
    explicit TrySharedLockGuard(CSRWLock& lock);
    ~TrySharedLockGuard();

    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TrySharedLockGuard(const TrySharedLockGuard&) = delete;
    TrySharedLockGuard& operator=(const TrySharedLockGuard&) = delete;

private:
    CSRWLock& _lock;
    bool      _acquired;
};

#endif // __SRWLOCK_H__