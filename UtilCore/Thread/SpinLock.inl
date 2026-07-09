// SpinLock.inl
#pragma once

// 빌드 시스템이 이 파일을 독립 소스 파일로 잘못 직접 컴파일하는 것을 완벽히 차단합니다.
#ifndef __SPINLOCK_H__
    #error "SpinLock.inl 파일은 직접 컴파일할 수 없습니다. SpinLock.h를 인클루드하세요."
#endif

// [FIX] 이 .inl은 SpinLock.h에 #include 되어 그 헤더를 쓰는 모든 번역 단위에
// 그대로 펼쳐지므로, 여기서 using namespace를 쓰면 전역 네임스페이스가
// RWSpinLockBits의 심볼로 오염된다. 대신 각 사용처에 RWSpinLockBits:: 를 명시한다.

namespace SpinLockDetail
{
    template <uint32_t MaxPauseBackoff, uint32_t MaxYieldCount, typename Predicate>
    inline void SpinWait(Predicate&& shouldWait) noexcept
    {
        uint32_t backoff    = 1;
        uint32_t yieldCount = 0;
        while (shouldWait())
        {
            if (backoff <= MaxPauseBackoff)
            {
                for (uint32_t i = 0; i < backoff; ++i) SPINLOCK_PAUSE();
                backoff = (backoff <= MaxPauseBackoff / 2) ? backoff * 2 : MaxPauseBackoff;
            }
            else if (yieldCount < MaxYieldCount)
            {
                std::this_thread::yield();
                ++yieldCount;
            }
            else
            {
                // [FIX] sleep_for(0ns)는 구현에 따라 std::this_thread::yield()와
                // 동일하게 동작할 수 있어(예: 일부 libc의 nanosleep(0) 처리) 3단계가
                // 2단계와 사실상 구분되지 않을 수 있다. 실제 OS 슬립 경로를 타도록
                // 아주 짧은 양(+)의 지속시간으로 바꾼다.
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                yieldCount = 0;
            }
        }
    }
} // namespace SpinLockDetail

// ════════════════════════════════════════════════════════════
//  SpinLock 구현
// ════════════════════════════════════════════════════════════

template <typename Preset>
void SpinLock<Preset>::Lock(const char* name) noexcept
{
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PushLock(name);
#endif

    while (true)
    {
        SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
            [this]() noexcept { return _locked.load(std::memory_order_relaxed); }
        );
        if (TryLock()) return;
    }
}

template <typename Preset>
bool SpinLock<Preset>::TryLock() noexcept
{
    bool expected = false;
    return _locked.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed);
}

template <typename Preset>
void SpinLock<Preset>::Unlock(const char* name) noexcept
{
    _locked.store(false, std::memory_order_release);
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PopLock(name);
#endif
}

// ════════════════════════════════════════════════════════════
//  RWSpinLock 구현
// ════════════════════════════════════════════════════════════

// ── Reader ───────────────────────────────────────────────────

template <typename Preset>
void RWSpinLock<Preset>::ReadLock(const char* name) noexcept
{
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PushLock(name);
#endif

    while (true)
    {
        SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
            [this]() noexcept
            {
                const int32_t s = _state.load(std::memory_order_relaxed);
                return (s & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0;
            }
        );

        const int32_t prev = _state.fetch_add(RWSpinLockBits::READER_ONE, std::memory_order_acquire);
        if ((prev & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) == 0)
        {
            if (((prev + RWSpinLockBits::READER_ONE) & RWSpinLockBits::READER_COUNT_MASK) == 0)
            {
                _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
                SPINLOCK_FATAL("RWSpinLock::ReadLock - reader count overflow (max 32767)");
            }
            return;
        }
        _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
    }
}

template <typename Preset>
bool RWSpinLock<Preset>::TryReadLock(const char* name) noexcept
{
    const int32_t s = _state.load(std::memory_order_relaxed);
    if ((s & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0) return false;

    const int32_t prev = _state.fetch_add(RWSpinLockBits::READER_ONE, std::memory_order_acquire);
    if ((prev & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) == 0)
    {
        if (((prev + RWSpinLockBits::READER_ONE) & RWSpinLockBits::READER_COUNT_MASK) == 0)
        {
            _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
            SPINLOCK_FATAL("RWSpinLock::TryReadLock - reader count overflow (max 32767)");
        }
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
        // 락 획득 성공 시에만 프로파일러에 이력을 기록한다.
        if (name) gpDeadLockProfiler->PushLock(name);
#endif
        return true;
    }
    _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
    return false;
}

template <typename Preset>
void RWSpinLock<Preset>::ReadUnlock(const char* name) noexcept
{
    _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_release);
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PopLock(name);
#endif
}

// ── Writer ───────────────────────────────────────────────────

template <typename Preset>
void RWSpinLock<Preset>::WriteLock(const char* name) noexcept
{
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PushLock(name);
#endif

    const int32_t prev = _state.fetch_add(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed);
    if (((prev + RWSpinLockBits::WRITER_ONE) & RWSpinLockBits::WRITER_WAITING_MASK) == 0)
    {
        // [FIX] ReadLock/TryReadLock의 오버플로우 처리와 일관되게, FATAL 호출 전에
        // 방금 더한 대기 카운트를 롤백한다. SPINLOCK_FATAL은 기본적으로
        // std::terminate()로 프로세스를 종료시키지만, 커스텀 terminate 핸들러가
        // 설치되어 실행이 계속되는 예외적인 경우까지 고려한 방어적 조치다.
        _state.fetch_sub(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed);
        SPINLOCK_FATAL("RWSpinLock::WriteLock - writer waiting count overflow (max 65535)");
    }

    SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
        [this]() noexcept
        {
            int32_t expected = _state.load(std::memory_order_relaxed);
            if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0) return true;

            return !_state.compare_exchange_strong(
                expected, expected | RWSpinLockBits::WRITE_LOCKED,
                std::memory_order_acquire, std::memory_order_relaxed);
        }
    );
}

template <typename Preset>
bool RWSpinLock<Preset>::TryWriteLock(const char* name) noexcept
{
    // WriteUnlock()은 항상 (WRITER_ONE + WRITE_LOCKED)를 fetch_sub 하므로,
    // TryWriteLock()도 WRITER_ONE(대기 카운터 필드)을 반드시 함께 반영해야 한다.
    // 다만 WRITER_ONE은 다중 비트로 누적되는 "카운터" 필드이므로 OR가 아닌
    // 산술 덧셈으로 증가시켜야 한다. 이미 다른 스레드가 WriteLock()으로
    // 대기 카운트를 올려둔 상태(expected != 0)에서 OR를 쓰면 비트가 겹쳐
    // 카운트가 뭉개지고, 이후 WriteUnlock()에서 상위 16비트가 언더플로우되어
    // ReadLock 영구 블록 및 SPINLOCK_FATAL 크래시로 이어질 수 있다.
    //
    // try_lock류 API는 "단 한 번 시도하고 실패하면 즉시 반환"이 표준적인 기대
    // 동작이다(SpinLock::TryLock, TryReadLock과 동일한 스타일). 따라서 CAS
    // 실패 시 재시도하지 않고 그 자리에서 false를 반환한다 — 설령 그 실패가
    // 다른 writer의 무해한 카운트 갱신과 우연히 겹쳐서 발생한 것이라 해도,
    // 그 판단은 호출자가 다시 TryWriteLock()을 호출할지 말지로 결정하게 둔다.
    //
    // [공정성 정책] 아래 가드는 READER_COUNT_MASK와 WRITE_LOCKED만 검사하고
    // WRITER_WAITING_MASK는 보지 않는다. 즉 이미 WriteLock()으로 등록해 대기
    // 중인(reader가 빠지길 기다리는) 다른 writer가 있어도, 그 순간 reader가
    // 0명이라면 TryWriteLock()을 호출한 스레드가 새치기해서 락을 가져갈 수
    // 있다. 이 라이브러리가 보장하는 공정성은 "reader에 의한 writer 기아
    // 방지"(§WriteLock 참고)까지이며, 여러 writer 사이의 도착 순서는 원래
    // 보장 대상이 아니다 — 이는 의도된 동작이며 실수가 아니다. 여러 writer
    // 간의 엄격한 FIFO 순서가 필요한 경우 TryWriteLock() 대신 WriteLock()만
    // 사용해야 한다.
    int32_t expected = _state.load(std::memory_order_relaxed);
    if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0)
        return false;

    const int32_t desired = expected + RWSpinLockBits::WRITER_ONE + RWSpinLockBits::WRITE_LOCKED;
    if (!_state.compare_exchange_strong(
            expected, desired,
            std::memory_order_acquire, std::memory_order_relaxed))
        return false;

#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    // 락 획득 성공 시에만 프로파일러에 이력을 기록한다.
    if (name) gpDeadLockProfiler->PushLock(name);
#endif
    return true;
}

template <typename Preset>
void RWSpinLock<Preset>::WriteUnlock(const char* name) noexcept
{
    _state.fetch_sub(RWSpinLockBits::WRITER_ONE + RWSpinLockBits::WRITE_LOCKED, std::memory_order_release);
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PopLock(name);
#endif
}
