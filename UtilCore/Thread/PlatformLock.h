
//***************************************************************************
//  PlatformLock.h : 플랫폼별(Windows SRWLock vs 크로스플랫폼 SpinLock)
//                   통합 동기화 객체 (PLock / PRWLock) 인터페이스 정의
//***************************************************************************

#ifndef __PLATFORMLOCK_H__
#define __PLATFORMLOCK_H__

#pragma once

#include "SpinLock.h"

#if defined(_WIN32) || defined(_WIN64)
#include "SRWLock.h"
#define PLATFORM_LOCK_WINDOWS 1
#else
#define PLATFORM_LOCK_WINDOWS 0
#endif

//***************************************************************************
// @class PLock
// @brief 플랫폼 독립적인 단독(Exclusive) 락 객체.
//
// @details
// 내부적으로 Windows 플랫폼에서는 네이티브 `CSRWLock`(쓰기 락)을 사용하고,
// 그 외 크로스 플랫폼 환경에서는 최적화된 `SpinLockDefault`를 사용하여
// 운영체제 환경에 구애받지 않고 동일한 인터페이스로 단독 임계 구역을 보호합니다.
//
// 주요 특징:
//  - 플랫폼별 최적의 동기화 프리미티브 자동 선택
//  - 복사 및 이동 생성자/대입 연산자 원천 차단 (유일한 락 주소 보장)
//  - 디버그 환경 프로파일러 추적 이름(name) 전달 지원
//***************************************************************************
class PLock
{
public:
    PLock() noexcept = default;
    ~PLock() noexcept = default;

    PLock(const PLock&) = delete;
    PLock& operator=(const PLock&) = delete;
    PLock(PLock&&) = delete;
    PLock& operator=(PLock&&) = delete;

    //***************************************************************************
    // @brief 락을 획득합니다. (블로킹 대기)
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void Lock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.ExclusiveLock();
#else
        _spinLock.Lock(name);
#endif
    }

    //***************************************************************************
    // @brief 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    // @return true: 락 획득 성공, false: 락 획득 실패 (즉시 반환)
    [[nodiscard]] bool TryLock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        return _srwLock.TryExclusiveLock();
#else
        (void)name;
        return _spinLock.TryLock();
#endif
    }

    //***************************************************************************
    // @brief 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void Unlock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.ExclusiveUnLock();
#else
        _spinLock.Unlock(name);
#endif
    }

private:
#if PLATFORM_LOCK_WINDOWS
    CSRWLock        _srwLock;   // Windows 환경용 네이티브 SRWLock
#else
    SpinLockDefault _spinLock;  // 크로스 플랫폼용 스핀 락
#endif
};


//***************************************************************************
// @class PRWLock
// @brief 플랫폼 독립적인 읽기/쓰기(Read-Write) 락 객체.
//
// @details
// 다중 읽기 및 단일 쓰기 패턴을 지원하는 동기화 객체로, Windows 환경에서는
// 네이티브 `CSRWLock`을 활용하고 기타 플랫폼에서는 `RWSpinLockDefault`를 사용합니다.
// 읽기 작업이 빈번하고 쓰기가 드문 환경에서 높은 동시성을 제공합니다.
//
// 주요 특징:
//  - Reader / Writer 권한 분리 제어 (Shared / Exclusive)
//  - 비블로킹 Try 계열 API 제공
//  - RAII 가드 클래스와 연동하여 예외 안전성 보장
//***************************************************************************
class PRWLock
{
public:
    PRWLock() noexcept = default;
    ~PRWLock() noexcept = default;

    PRWLock(const PRWLock&) = delete;
    PRWLock& operator=(const PRWLock&) = delete;
    PRWLock(PRWLock&&) = delete;
    PRWLock& operator=(PRWLock&&) = delete;

    // ── Reader API ──────────────────────────

    //***************************************************************************
    // @brief 읽기(Shared) 락을 획득합니다. (블로킹 대기)
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void ReadLock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.SharedLock();
#else
        _rwSpinLock.ReadLock(name);
#endif
    }

    //***************************************************************************
    // @brief 읽기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    // @return true: 읽기 락 획득 성공, false: 락 획득 실패
    [[nodiscard]] bool TryReadLock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        return _srwLock.TrySharedLock();
#else
        return _rwSpinLock.TryReadLock(name);
#endif
    }

    //***************************************************************************
    // @brief 읽기 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void ReadUnlock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.SharedUnLock();
#else
        _rwSpinLock.ReadUnlock(name);
#endif
    }

    // ── Writer API ──────────────────────────

    //***************************************************************************
    // @brief 쓰기(Exclusive) 락을 획득합니다. (블로킹 대기)
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void WriteLock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.ExclusiveLock();
#else
        _rwSpinLock.WriteLock(name);
#endif
    }

    //***************************************************************************
    // @brief 쓰기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    // @return true: 쓰기 락 획득 성공, false: 락 획득 실패
    [[nodiscard]] bool TryWriteLock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        return _srwLock.TryExclusiveLock();
#else
        return _rwSpinLock.TryWriteLock(name);
#endif
    }

    //***************************************************************************
    // @brief 쓰기 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름 (기본값: nullptr)
    void WriteUnlock(const char* name = nullptr) noexcept
    {
#if PLATFORM_LOCK_WINDOWS
        (void)name;
        _srwLock.ExclusiveUnLock();
#else
        _rwSpinLock.WriteUnlock(name);
#endif
    }

private:
#if PLATFORM_LOCK_WINDOWS
    CSRWLock          _srwLock;     // Windows 환경용 네이티브 SRWLock
#else
    RWSpinLockDefault _rwSpinLock;  // 크로스 플랫폼용 읽기/쓰기 스핀 락
#endif
};


//***************************************************************************
// 3. RAII 가드 (Guard) 정의
//***************************************************************************

//***************************************************************************
// @class PLockGuard
// @brief PLock 객체의 생명주기를 관리하는 RAII 가드.
//***************************************************************************
class PLockGuard
{
public:
    explicit PLockGuard(PLock& lock, const char* name = nullptr) noexcept
        : _lock(lock), _name(name)
    {
        _lock.Lock(_name);
    }
    ~PLockGuard() noexcept { _lock.Unlock(_name); }

    PLockGuard(const PLockGuard&) = delete;
    PLockGuard& operator=(const PLockGuard&) = delete;

private:
    PLock& _lock;
    const char* _name;
};

//***************************************************************************
// @class PRReadLockGuard
// @brief PRWLock 객체의 읽기 락 생명주기를 관리하는 RAII 가드.
//***************************************************************************
class PRReadLockGuard
{
public:
    explicit PRReadLockGuard(PRWLock& lock, const char* name = nullptr) noexcept
        : _lock(lock), _name(name)
    {
        _lock.ReadLock(_name);
    }
    ~PRReadLockGuard() noexcept { _lock.ReadUnlock(_name); }

    PRReadLockGuard(const PRReadLockGuard&) = delete;
    PRReadLockGuard& operator=(const PRReadLockGuard&) = delete;

private:
    PRWLock& _lock;
    const char* _name;
};

//***************************************************************************
// @class PRWriteLockGuard
// @brief PRWLock 객체의 쓰기 락 생명주기를 관리하는 RAII 가드.
//***************************************************************************
class PRWriteLockGuard
{
public:
    explicit PRWriteLockGuard(PRWLock& lock, const char* name = nullptr) noexcept
        : _lock(lock), _name(name)
    {
        _lock.WriteLock(_name);
    }
    ~PRWriteLockGuard() noexcept { _lock.WriteUnlock(_name); }

    PRWriteLockGuard(const PRWriteLockGuard&) = delete;
    PRWriteLockGuard& operator=(const PRWriteLockGuard&) = delete;

private:
    PRWLock& _lock;
    const char* _name;
};

#endif // __PLATFORMLOCK_H__