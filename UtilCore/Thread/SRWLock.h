
//***************************************************************************
//  SRWLock.h : interface for the CSRWLock class. 
//
// - 읽기(공유) 락 : 여러 스레드가 동시에 획득 가능
// - 쓰기(독점) 락 : 한 스레만 획득 가능, 읽기도 블록
// - SRWLock은 재진입(recursive) 불가 — 같은 스레드 중첩 잠금 시 데드락
// - ReadLock → WriteLock 승격 불가 — 데드락 유발
//***************************************************************************

#ifndef __SRWLOCK_H__
#define __SRWLOCK_H__

#pragma once

#include <windows.h>
#include <cassert>

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
    void ExclusiveLock(const char* name = nullptr);
    [[nodiscard]] bool TryExclusiveLock(const char* name = nullptr);
    void ExclusiveUnLock(const char* name = nullptr);

    // ── 읽기 락 (Shared) ─────────────────────────────────────
    void SharedLock(const char* name = nullptr);
    [[nodiscard]] bool TrySharedLock(const char* name = nullptr);
    void SharedUnLock(const char* name = nullptr);

    // ── 조건 변수 연동 ────────────────────────────────────────
    SRWLOCK* NativeHandle() { return &_srwLock; }

private:
    SRWLOCK _srwLock;   // Windows SRWLock 핸들 (포인터 크기, 8 bytes)[cite: 4]
};


//***************************************************************************
// ExclusiveLockGuard — 쓰기 락 RAII 가드
//***************************************************************************
class ExclusiveLockGuard
{
public:
    explicit ExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;
    ~ExclusiveLockGuard() noexcept;

    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;    // 대상 SRWLock 참조[cite: 4]
    const char* _name;    // 데드락 프로파일러에 전달할 락 이름
};


//***************************************************************************
// SharedLockGuard — 읽기 락 RAII 가드
//***************************************************************************
class SharedLockGuard
{
public:
    explicit SharedLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;
    ~SharedLockGuard() noexcept;

    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;

private:
    CSRWLock& _lock;    // 대상 SRWLock 참조[cite: 4]
    const char* _name;    // 데드락 프로파일러에 전달할 락 이름
};


//***************************************************************************
// TryExclusiveLockGuard — 쓰기 락 Try RAII 가드
//***************************************************************************
class TryExclusiveLockGuard
{
public:
    explicit TryExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;
    ~TryExclusiveLockGuard() noexcept;

    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TryExclusiveLockGuard(const TryExclusiveLockGuard&) = delete;
    TryExclusiveLockGuard& operator=(const TryExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;     // 대상 SRWLock 참조[cite: 4]
    const char* _name;     // 데드락 프로파일러에 전달할 락 이름
    bool        _acquired; // 락 획득 성공 여부 플래그[cite: 4]
};


//***************************************************************************
// TrySharedLockGuard — 읽기 락 Try RAII 가드
//***************************************************************************
class TrySharedLockGuard
{
public:
    explicit TrySharedLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;
    ~TrySharedLockGuard() noexcept;

    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TrySharedLockGuard(const TrySharedLockGuard&) = delete;
    TrySharedLockGuard& operator=(const TrySharedLockGuard&) = delete;

private:
    CSRWLock& _lock;     // 대상 SRWLock 참조[cite: 4]
    const char* _name;     // 데드락 프로파일러에 전달할 락 이름
    bool        _acquired; // 락 획득 성공 여부 플래그[cite: 4]
};

// 프로젝트 매크로 연동 추상화 레이어
enum class SRWLockType { Read, Write };

template <typename LockObj>
class CSRWCustomLockGuard
{
public:
    CSRWCustomLockGuard(LockObj& lock, SRWLockType type, const char* name) noexcept
        : _lock(lock), _type(type), _name(name)
    {
        if( _type == SRWLockType::Write ) _lock.ExclusiveLock(_name);
        else                             _lock.SharedLock(_name);
    }
    ~CSRWCustomLockGuard() noexcept
    {
        if( _type == SRWLockType::Write ) _lock.ExclusiveUnLock(_name);
        else                             _lock.SharedUnLock(_name);
    }
private:
    LockObj& _lock; // 대상 락 객체 참조
    SRWLockType   _type; // 락 타입 (읽기 또는 쓰기)
    const char* _name; // 데드락 프로파일러에 전달할 락 이름
};

#define SRW_USE_LOCK          mutable CSRWLock _lock
#define SRW_WRITE_LOCK        CSRWCustomLockGuard<CSRWLock> __write_lock_guard__(_lock, SRWLockType::Write, __func__)
#define SRW_READ_LOCK         CSRWCustomLockGuard<CSRWLock> __read_lock_guard__(_lock, SRWLockType::Read, __func__)

#endif // __SRWLOCK_H__