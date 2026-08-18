
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

//***************************************************************************
// @brief Windows 네이티브 SRWLock을 래핑하여 읽기/쓰기 동기화를 제공하는 클래스입니다.
// @detail 독점(쓰기) 락과 공유(읽기) 락을 지원하며, RAII 가드 및 데드락 프로파일러 연동 기능을 제공합니다.
//***************************************************************************
class CSRWLock
{
public:
    //***************************************************************************
    // @brief CSRWLock 객체를 생성하고 내부 Windows SRWLock 핸들을 초기화합니다.
    //***************************************************************************
    CSRWLock();

    //***************************************************************************
    // @brief CSRWLock 객체를 소멸합니다.
    // @detail 잠긴 상태로 소멸되는 경우 디버그 빌드에서 어서트가 발생합니다.
    //***************************************************************************
    ~CSRWLock();

    // 복사/이동 금지 — 락 객체는 고정된 주소를 가져야 함
    CSRWLock(const CSRWLock&) = delete;
    CSRWLock& operator=(const CSRWLock&) = delete;
    CSRWLock(CSRWLock&&) = delete;
    CSRWLock& operator=(CSRWLock&&) = delete;

    // ── 쓰기 락 (Exclusive) ──────────────────────────────────
    //***************************************************************************
    // @brief 모든 읽기/쓰기 스레드가 해제될 때까지 블로킹 대기하며 쓰기 락을 획득합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    void ExclusiveLock(const char* name = nullptr);

    //***************************************************************************
    // @brief 쓰기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    // @return 쓰기 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool TryExclusiveLock(const char* name = nullptr);

    //***************************************************************************
    // @brief 쓰기 락을 해제합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    void ExclusiveUnLock(const char* name = nullptr);

    // ── 읽기 락 (Shared) ─────────────────────────────────────
    //***************************************************************************
    // @brief 다른 읽기 스레드와 동시 획득 가능하며, 쓰기 스레드가 있으면 블로킹 대기합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    void SharedLock(const char* name = nullptr);

    //***************************************************************************
    // @brief 읽기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    // @return 읽기 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool TrySharedLock(const char* name = nullptr);

    //***************************************************************************
    // @brief 읽기 락을 해제합니다.
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    void SharedUnLock(const char* name = nullptr);

    // ── 조건 변수 연동 ────────────────────────────────────────
    //***************************************************************************
    // @brief 내부 Windows SRWLOCK 핸들 포인터를 반환합니다.
    // @return Windows SRWLOCK 포인터
    //***************************************************************************
    SRWLOCK* NativeHandle() { return &_srwLock; }

private:
    SRWLOCK _srwLock;   // Windows SRWLock 핸들 (포인터 크기, 8 bytes)
};


//***************************************************************************
// @brief 쓰기 락 전용 RAII 가드 클래스입니다.
// @detail 객체 생성 시 쓰기 락을 획득하고 소멸 시 자동으로 해제합니다.
//***************************************************************************
class ExclusiveLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 대상 락의 쓰기 락을 획득합니다.
    // @param lock 관리할 CSRWLock 객체 참조
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    explicit ExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 소멸자. 쓰기 락을 자동으로 해제합니다.
    //***************************************************************************
    ~ExclusiveLockGuard() noexcept;

    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;    // 대상 SRWLock 참조
    const char* _name;  // 데드락 프로파일러에 전달할 락 이름
};


//***************************************************************************
// @brief 읽기 락 전용 RAII 가드 클래스입니다.
// @detail 객체 생성 시 읽기 락을 획득하고 소멸 시 자동으로 해제합니다.
//***************************************************************************
class SharedLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 대상 락의 읽기 락을 획득합니다.
    // @param lock 관리할 CSRWLock 객체 참조
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    explicit SharedLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 소멸자. 읽기 락을 자동으로 해제합니다.
    //***************************************************************************
    ~SharedLockGuard() noexcept;

    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;

private:
    CSRWLock& _lock;    // 대상 SRWLock 참조
    const char* _name;  // 데드락 프로파일러에 전달할 락 이름
};


//***************************************************************************
// @brief 쓰기 락 비블로킹 획득을 시도하는 RAII 가드 클래스입니다.
// @detail 객체 생성 시 비블로킹 쓰기 락 획득을 시도하며, 획득 성공 시에만 소멸 시 해제합니다.
//***************************************************************************
class TryExclusiveLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 쓰기 락 비블로킹 획득을 시도합니다.
    // @param lock 관리할 CSRWLock 객체 참조
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    explicit TryExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 소멸자. 락 획득에 성공했던 경우에만 쓰기 락을 해제합니다.
    //***************************************************************************
    ~TryExclusiveLockGuard() noexcept;

    //***************************************************************************
    // @brief 락 획득 성공 여부를 반환합니다.
    // @return 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TryExclusiveLockGuard(const TryExclusiveLockGuard&) = delete;
    TryExclusiveLockGuard& operator=(const TryExclusiveLockGuard&) = delete;

private:
    CSRWLock& _lock;    // 대상 SRWLock 참조
    const char* _name;  // 데드락 프로파일러에 전달할 락 이름
    bool _acquired;     // 락 획득 성공 여부 플래그
};


//***************************************************************************
// @brief 읽기 락 비블로킹 획득을 시도하는 RAII 가드 클래스입니다.
// @detail 객체 생성 시 비블로킹 읽기 락 획득을 시도하며, 획득 성공 시에만 소멸 시 해제합니다.
//***************************************************************************
class TrySharedLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 읽기 락 비블로킹 획득을 시도합니다.
    // @param lock 관리할 CSRWLock 객체 참조
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    explicit TrySharedLockGuard(CSRWLock& lock, const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 소멸자. 락 획득에 성공했던 경우에만 읽기 락을 해제합니다.
    //***************************************************************************
    ~TrySharedLockGuard() noexcept;

    //***************************************************************************
    // @brief 락 획득 성공 여부를 반환합니다.
    // @return 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool IsAcquired() const { return _acquired; }

    TrySharedLockGuard(const TrySharedLockGuard&) = delete;
    TrySharedLockGuard& operator=(const TrySharedLockGuard&) = delete;

private:
    CSRWLock& _lock;        // 대상 SRWLock 참조
    const char* _name;      // 데드락 프로파일러에 전달할 락 이름
    bool        _acquired;  // 락 획득 성공 여부 플래그
};

// 프로젝트 매크로 연동 추상화 레이어
enum class SRWLockType { Read, Write };

//***************************************************************************
// @brief 읽기 또는 쓰기 락을 동적으로 선택하여 관리하는 커스텀 가드 클래스입니다.
// @detail 지정된 락 타입에 따라 적절한 읽기/쓰기 락을 획득하고 해제합니다.
//***************************************************************************
template <typename LockObj>
class CSRWCustomLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 지정된 타입에 따라 락을 획득합니다.
    // @param lock 관리할 락 객체 참조
    // @param type 락 종류 (Read 또는 Write)
    // @param name 데드락 프로파일러에 전달할 락 이름
    //***************************************************************************
    CSRWCustomLockGuard(LockObj& lock, SRWLockType type, const char* name) noexcept
        : _lock(lock), _type(type), _name(name)
    {
        if( _type == SRWLockType::Write ) _lock.ExclusiveLock(_name);
        else                              _lock.SharedLock(_name);
    }

    //***************************************************************************
    // @brief 소멸자. 지정된 락 타입을 확인하여 락을 자동으로 해제합니다.
    //***************************************************************************
    ~CSRWCustomLockGuard() noexcept
    {
        if( _type == SRWLockType::Write ) _lock.ExclusiveUnLock(_name);
        else                              _lock.SharedUnLock(_name);
    }
private:
    LockObj& _lock;         // 대상 락 객체 참조
    SRWLockType _type;      // 락 타입 (읽기 또는 쓰기)
    const char* _name;      // 데드락 프로파일러에 전달할 락 이름
};

#define SRW_USE_LOCK        mutable CSRWLock _lock
#define SRW_WRITE_LOCK      CSRWCustomLockGuard<CSRWLock> __write_lock_guard__(_lock, SRWLockType::Write, __func__)
#define SRW_READ_LOCK       CSRWCustomLockGuard<CSRWLock> __read_lock_guard__(_lock, SRWLockType::Read, __func__)

#endif // ndef __SRWLOCK_H__