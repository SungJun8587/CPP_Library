
//***************************************************************************
//  SpinLock.h : interface for the SpinLock class. 
// 
//***************************************************************************

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <exception>

#ifndef __CACHEALIGNMENT_H__
#include <Thread/CacheAlignment.h>
#endif

//***************************************************************************
//  Platform: CPU pause hint
//***************************************************************************
#if defined(_MSC_VER)
#include <intrin.h>
#define SPINLOCK_PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_PAUSE() __builtin_ia32_pause()
#elif defined(__arm__) || defined(__aarch64__)
#define SPINLOCK_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
#define SPINLOCK_PAUSE() std::this_thread::yield()
#endif
#else
#define SPINLOCK_PAUSE() std::this_thread::yield()
#endif

//***************************************************************************
//  Platform: debugger break
//***************************************************************************
#if defined(_MSC_VER)
#define SPINLOCK_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_DEBUG_BREAK() __asm__ __volatile__("int3")
#elif defined(__arm__) || defined(__aarch64__)
#define SPINLOCK_DEBUG_BREAK() __asm__ __volatile__("brk #0")
#else
#include <csignal>
#define SPINLOCK_DEBUG_BREAK() ::raise(SIGTRAP)
#endif
#else
#include <csignal>
#define SPINLOCK_DEBUG_BREAK() ::raise(SIGTRAP)
#endif

//***************************************************************************
//  SPINLOCK_FATAL(msg)
//***************************************************************************
#define SPINLOCK_FATAL(msg)                                         \
    do {                                                            \
        ::fprintf(stderr,                                           \
            "[SPINLOCK FATAL] %s:%d  %s\n", __FILE__, __LINE__, msg); \
        ::fflush(stderr);                                           \
        SPINLOCK_DEBUG_BREAK();                                     \
        std::terminate();                                           \
    } while (false)

//***************************************************************************
//  [1] SpinLock
//***************************************************************************
struct SpinLockPreset
{
    struct LightWeight { static constexpr uint32_t MaxPauseBackoff = 256;  static constexpr uint32_t MaxYieldCount = 16; };
    struct Default { static constexpr uint32_t MaxPauseBackoff = 1024; static constexpr uint32_t MaxYieldCount = 64; };
    struct HeavyContention { static constexpr uint32_t MaxPauseBackoff = 4096; static constexpr uint32_t MaxYieldCount = 128; };
    struct OverSubscribed { static constexpr uint32_t MaxPauseBackoff = 32;   static constexpr uint32_t MaxYieldCount = 8; };
};

//***************************************************************************
// @brief 스핀 락(SpinLock)을 제공하는 템플릿 클래스입니다.
// @detail 캐시라인 크기로 정렬 및 패딩되어 false sharing을 방지하며, 백오프 정책을 지원합니다.
//***************************************************************************
template <typename Preset = SpinLockPreset::Default>
class alignas(kCacheLineSize) SpinLock
{
public:
    SpinLock() noexcept = default;
    ~SpinLock() noexcept = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;
    SpinLock(SpinLock&&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;

    //***************************************************************************
    // @brief 락을 획득할 때까지 스핀 대기합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void Lock(const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 락 획득을 비블로킹 방식으로 시도합니다.
    // @return 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool TryLock() noexcept;

    //***************************************************************************
    // @brief 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void Unlock(const char* name = nullptr) noexcept;

private:
    // kCacheLineSize는 std::size_t(부호 없는 타입)이므로, sizeof(std::atomic<bool>)가
    // kCacheLineSize를 넘어서면 아래 뺄셈이 음수가 아니라 거대한 양수로 언더플로우된다.
    // 이 static_assert로 그 상황을 먼저 명확한 에러 메시지로 걸러낸다.
    static_assert(sizeof(std::atomic<bool>) <= kCacheLineSize,
        "std::atomic<bool> exceeds the configured cache line size");

    std::atomic<bool> _locked{ false };		// 락 점유 상태를 나타내는 아토믹 불리언 변수
    // 클래스 선언의 alignas(kCacheLineSize)는 객체의 "시작 주소" 정렬만 보장한다.
    // 객체의 "크기" 자체를 캐시라인 크기에 고정하려면 별도로 패딩이 필요하다
    // (정렬만으로는 sizeof가 캐시라인의 배수가 되는 것까지는 보장하지 않음).
    // 참고: hardware_destructive_interference_size가 64보다 큰 플랫폼에서는
    // 이 패딩 배열과 객체 전체 크기도 그만큼 커진다 — 의도된 동작이다.
    char _padding[kCacheLineSize - sizeof(std::atomic<bool>)]{};	// 캐시라인 크기 맞춤용 패딩 배열
};

static_assert(sizeof(SpinLock<SpinLockPreset::Default>) == kCacheLineSize,
    "SpinLock must occupy exactly one cache line");

using SpinLockDefault = SpinLock<SpinLockPreset::Default>;
using SpinLockLight = SpinLock<SpinLockPreset::LightWeight>;
using SpinLockHeavy = SpinLock<SpinLockPreset::HeavyContention>;
using SpinLockOverSubscribed = SpinLock<SpinLockPreset::OverSubscribed>;

//***************************************************************************
// @brief SpinLock 객체의 RAII 기반 가드 클래스입니다.
// @detail 객체 생성 시 락을 획득하고 소멸 시 자동으로 해제합니다.
//***************************************************************************
template <typename Preset = SpinLockPreset::Default>
class SpinLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 대상 락을 획득합니다.
    // @param lock 관리할 SpinLock 객체
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    explicit SpinLockGuard(SpinLock<Preset>& lock, const char* name = nullptr) noexcept
        : _lock(lock), _name(name)
    {
        _lock.Lock(_name);
    }

    //***************************************************************************
    // @brief 소멸자. 락을 자동으로 해제합니다.
    //***************************************************************************
    ~SpinLockGuard() noexcept { _lock.Unlock(_name); }

    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;
private:
    SpinLock<Preset>& _lock;	// 관리 중인 SpinLock 객체 레퍼런스
    const char* _name;			// 프로파일링 추적용 락 이름
};

#define SPIN_USE_LOCK    mutable SpinLock<SpinLockPreset::LightWeight> _lock
#define SPIN_LOCK         SpinLockGuard<SpinLockPreset::LightWeight> __spin_lock_guard__(_lock, __func__)

//***************************************************************************
//  [2] RWSpinLock
//***************************************************************************
struct RWSpinLockPreset
{
    struct ReadHeavy { static constexpr uint32_t MaxPauseBackoff = 512;  static constexpr uint32_t MaxYieldCount = 32; };
    struct Default { static constexpr uint32_t MaxPauseBackoff = 1024; static constexpr uint32_t MaxYieldCount = 64; };
    struct WriteContention { static constexpr uint32_t MaxPauseBackoff = 2048; static constexpr uint32_t MaxYieldCount = 128; };
};

namespace RWSpinLockBits
{
    inline constexpr int32_t WRITE_LOCKED = 0x00000001;
    inline constexpr int32_t READER_COUNT_MASK = 0x0000FFFE;
    inline constexpr int32_t READER_ONE = 0x00000002;
    inline constexpr int32_t WRITER_WAITING_MASK = static_cast<int32_t>(0xFFFF0000u);
    inline constexpr int32_t WRITER_ONE = 0x00010000;
} // namespace RWSpinLockBits

//***************************************************************************
// @brief 읽기/쓰기 스핀 락(RWSpinLock)을 제공하는 템플릿 클래스입니다.
// @detail 다중 읽기와 단일 쓰기를 지원하며 캐시라인 정렬 및 비트 필드 기반 상태 관리를 사용합니다.
//***************************************************************************
template <typename Preset = RWSpinLockPreset::Default>
class alignas(kCacheLineSize) RWSpinLock
{
public:
    RWSpinLock() noexcept = default;
    ~RWSpinLock() noexcept = default;

    RWSpinLock(const RWSpinLock&) = delete;
    RWSpinLock& operator=(const RWSpinLock&) = delete;
    RWSpinLock(RWSpinLock&&) = delete;
    RWSpinLock& operator=(RWSpinLock&&) = delete;

    // ── Reader API ──────────────────────────
    //***************************************************************************
    // @brief 읽기 락을 획득합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void ReadLock(const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 읽기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 프로파일링 추적용 락 이름
    // @return 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool TryReadLock(const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 읽기 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void ReadUnlock(const char* name = nullptr) noexcept;

    // ── Writer API ──────────────────────────
    //***************************************************************************
    // @brief 쓰기 락을 획득합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void WriteLock(const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 쓰기 락 획득을 비블로킹 방식으로 시도합니다.
    // @param name 프로파일링 추적용 락 이름
    // @return 락 획득 성공 시 true, 실패 시 false
    //***************************************************************************
    [[nodiscard]] bool TryWriteLock(const char* name = nullptr) noexcept;

    //***************************************************************************
    // @brief 쓰기 락을 해제합니다.
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    void WriteUnlock(const char* name = nullptr) noexcept;

private:
    // SpinLock과 동일한 이유로, 언더플로우 가능성을 먼저 명확한 에러로 걸러낸다.
    static_assert(sizeof(std::atomic<int32_t>) <= kCacheLineSize,
        "std::atomic<int32_t> exceeds the configured cache line size");

    std::atomic<int32_t> _state{ 0 };		// 락의 상태를 관리하는 비트 필드 아토믹 변수
    // SpinLock과 동일한 이유(정렬과 크기는 별개 보장)로 명시적 패딩을 둔다.
    // hardware_destructive_interference_size가 64보다 큰 플랫폼에서는
    // 이 패딩과 객체 전체 크기도 그만큼 커진다 — 의도된 동작이다.
    char _padding[kCacheLineSize - sizeof(std::atomic<int32_t>)]{};	// 캐시라인 크기 맞춤용 패딩 배열
};

static_assert(sizeof(RWSpinLock<RWSpinLockPreset::Default>) == kCacheLineSize,
    "RWSpinLock must occupy exactly one cache line");

using RWSpinLockDefault = RWSpinLock<RWSpinLockPreset::Default>;
using RWSpinLockReadHeavy = RWSpinLock<RWSpinLockPreset::ReadHeavy>;
using RWSpinLockWriteContention = RWSpinLock<RWSpinLockPreset::WriteContention>;

//***************************************************************************
// @brief 읽기 락 전용 RAII 가드 클래스입니다.
// @detail 객체 생성 시 읽기 락을 획득하고 소멸 시 자동으로 해제합니다.
//***************************************************************************
template <typename Preset = RWSpinLockPreset::Default>
class ReadLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 대상 락의 읽기 락을 획득합니다.
    // @param lock 관리할 RWSpinLock 객체
    //***************************************************************************
    explicit ReadLockGuard(RWSpinLock<Preset>& lock) noexcept : _lock(lock) { _lock.ReadLock(); }

    //***************************************************************************
    // @brief 소멸자. 읽기 락을 자동으로 해제합니다.
    //***************************************************************************
    ~ReadLockGuard() noexcept { _lock.ReadUnlock(); }

    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;
private:
    RWSpinLock<Preset>& _lock;	// 관리 중인 RWSpinLock 객체 레퍼런스
};

//***************************************************************************
// @brief 쓰기 락 전용 RAII 가드 클래스입니다.
// @detail 객체 생성 시 쓰기 락을 획득하고 소멸 시 자동으로 해제합니다.
//***************************************************************************
template <typename Preset = RWSpinLockPreset::Default>
class WriteLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 대상 락의 쓰기 락을 획득합니다.
    // @param lock 관리할 RWSpinLock 객체
    //***************************************************************************
    explicit WriteLockGuard(RWSpinLock<Preset>& lock) noexcept : _lock(lock) { _lock.WriteLock(); }

    //***************************************************************************
    // @brief 소멸자. 쓰기 락을 자동으로 해제합니다.
    //***************************************************************************
    ~WriteLockGuard() noexcept { _lock.WriteUnlock(); }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;
private:
    RWSpinLock<Preset>& _lock;	// 관리 중인 RWSpinLock 객체 레퍼런스
};

// 프로젝트 매크로 연동 추상화 레이어 (LockQueue 등에서 활용 가능)
enum class LockType { Read, Write };

//***************************************************************************
// @brief 읽기 또는 쓰기 락을 동적으로 선택하여 관리하는 커스텀 가드 클래스입니다.
// @detail 지정된 락 타입에 따라 적절한 읽기/쓰기 락을 획득하고 해제합니다.
//***************************************************************************
template <typename LockObj>
class CustomLockGuard
{
public:
    //***************************************************************************
    // @brief 가드 객체를 생성하고 지정된 타입에 따라 락을 획득합니다.
    // @param lock 관리할 락 객체
    // @param type 락 종류 (Read 또는 Write)
    // @param name 프로파일링 추적용 락 이름
    //***************************************************************************
    CustomLockGuard(LockObj& lock, LockType type, const char* name) noexcept
        : _lock(lock), _type(type), _name(name)
    {
        // [FIX] 프로파일러에 함수 이름을 정상 전달하도록 _name 매개변수를 바인딩합니다.
        if( _type == LockType::Write ) _lock.WriteLock(_name);
        else                          _lock.ReadLock(_name);
    }

    //***************************************************************************
    // @brief 소멸자. 지정된 락 타입을 확인하여 락을 자동으로 해제합니다.
    //***************************************************************************
    ~CustomLockGuard() noexcept
    {
        // [FIX] 프로파일러에 함수 이름을 정상 전달하도록 _name 매개변수를 바인딩합니다.
        if( _type == LockType::Write ) _lock.WriteUnlock(_name);
        else                          _lock.ReadUnlock(_name);
    }
private:
    LockObj& _lock;	// 관리 중인 락 객체 레퍼런스
    LockType   _type;	// 락 타입 (Read/Write)
    const char* _name;	// 프로파일링 추적용 락 이름
};

#define RWSPIN_USE_LOCK           mutable RWSpinLock<RWSpinLockPreset::Default> _lock
#define RWSPIN_WRITE_LOCK         CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                             __write_lock_guard__(_lock, LockType::Write, __func__)
#define RWSPIN_READ_LOCK          CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                             __read_lock_guard__(_lock, LockType::Read, __func__)

#include "SpinLock.inl"

#endif // ndef __SPINLOCK_H__
