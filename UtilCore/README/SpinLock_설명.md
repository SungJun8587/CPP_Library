# SpinLock / RWSpinLock 라이브러리 설계 문서

C++17 기반의 락 프리(lock-free) 스핀락 2종(`SpinLock`, `RWSpinLock`)에 대한 설계 배경, 내부 구현, 동시성 이론, 사용법을 정리한 문서입니다.

---

## 1. 개요

| 구분 | `SpinLock` | `RWSpinLock` |
|---|---|---|
| 상태 표현 | `std::atomic<bool>` 1바이트 | `std::atomic<int32_t>` 비트마스크 인코딩 |
| 동시 접근 | 상호 배제(1명만 진입) | Reader 다수 동시 진입 + Writer 단독 진입 |
| 공정성 | 없음(선착순 보장 X) | Writer 기아(starvation) 방지 장치 있음 |
| 백오프 전략 | Preset 기반 적응형 백오프 | Preset 기반 적응형 백오프 |
| Guard | `SpinLockGuard` (RAII) | `ReadLockGuard` / `WriteLockGuard` / `CustomLockGuard` |
| 메모리 배치 | 정확히 1개 캐시라인(64바이트)으로 고정 | 정확히 1개 캐시라인(64바이트)으로 고정 |

두 락 모두 캐시라인(`kCacheLineSize`, 보통 64바이트) 경계에 정렬(`alignas`)되고, 명시적 패딩 필드로 크기 자체를 캐시라인 크기에 고정해 **false sharing**을 방지하도록 설계되어 있습니다.

```cpp
// SpinLock
alignas(kCacheLineSize) std::atomic<bool> _locked{ false };
char _padding[kCacheLineSize - sizeof(std::atomic<bool>)]{};

// RWSpinLock
alignas(kCacheLineSize) std::atomic<int32_t> _state{ 0 };
char _padding[kCacheLineSize - sizeof(std::atomic<int32_t>)]{};
```

`alignas`만으로도 클래스 정렬은 강제되지만, 정렬과 크기(sizeof)는 별개입니다. 패딩 필드를 명시적으로 두면 "이 객체는 정확히 1개 캐시라인만 차지한다"는 설계 의도가 코드 자체에 강제됩니다 — 나중에 멤버가 추가되어 캐시라인을 초과하면 패딩 배열의 크기 계산이 어긋나 컴파일 에러로 드러납니다:

```cpp
static_assert(sizeof(std::atomic<bool>) <= kCacheLineSize,
    "std::atomic<bool> exceeds the configured cache line size");
static_assert(sizeof(SpinLock<SpinLockPreset::Default>) == kCacheLineSize,
    "SpinLock must occupy exactly one cache line");
```

`kCacheLineSize`가 부호 없는 `std::size_t`이므로, `sizeof(atomic<T>) > kCacheLineSize`인 극단적 상황에서는 패딩 배열 크기 계산이 음수가 아니라 거대한 양수로 언더플로우됩니다. 앞의 `static_assert(sizeof(atomic<T>) <= kCacheLineSize)`가 그 경우를 먼저 명확한 에러 메시지로 걸러냅니다.

`alignas(kCacheLineSize)`는 클래스 선언 자체에 붙어 있어 **객체의 시작 주소**가 캐시라인 경계에 오는 것을 보장하고, 패딩 필드는 **객체의 크기**를 캐시라인 크기에 고정합니다 — 이 둘은 서로 다른 것을 보장하므로 중복이 아니라 상호 보완적입니다. 다만 `std::hardware_destructive_interference_size`가 64보다 큰 플랫폼(일부 ARM 구성 등)에서는 `kCacheLineSize` 자체가 커지므로 패딩 배열과 객체 전체 크기도 그만큼 커진다는 점은 참고할 필요가 있습니다 — 어떤 플랫폼에서든 "정확히 1개 캐시라인"이라는 불변식 자체는 유지됩니다.

---

## 2. 왜 스핀락인가 — Mutex와의 차이

`std::mutex`는 경합 시 커널을 통해 스레드를 잠재웁니다(sleep/wake). 이는 컨텍스트 스위칭 비용(수 마이크로초)이 발생하므로, **락을 아주 짧게, 아주 자주** 잡는 게임 서버의 핫패스(예: 세션당 초당 수천 회의 패킷 큐 접근)에서는 오히려 손해입니다.

스핀락은 락이 풀릴 때까지 CPU를 계속 돌리며(busy-wait) 대기합니다. 임계구역이 매우 짧다면(수십~수백 ns) 컨텍스트 스위칭 없이 그냥 몇 번 루프 도는 편이 훨씬 빠릅니다. 대신 임계구역이 길어지거나 스레드 수가 코어 수를 초과하면 CPU만 낭비하므로, **"짧은 임계구역 + 코어 수 이내의 동시성"**이라는 전제가 성립할 때만 유효합니다.

---

## 3. TTAS(Test-and-Test-and-Set)와 캐시 코히런시

### 3.1 순수 CAS 스핀의 문제

가장 단순한 스핀락은 대기 중에도 계속 `compare_exchange`(CAS)를 시도합니다. 문제는 CAS가 **항상 쓰기 의도(RFO, Read-For-Ownership)**를 동반한다는 점입니다. MESI 프로토콜 하에서 캐시라인이 `Shared` 상태였더라도 CAS 시도 순간 `Modified`를 요구하며, 이는 해당 캐시라인을 가진 다른 모든 코어의 캐시를 무효화(Invalidate)시킵니다.

락을 대기 중인 스레드 N개가 전부 CAS를 반복하면, 매 시도마다 캐시라인 소유권이 코어 사이를 핑퐁처럼 오가며(cache line ping-pong) 인터커넥트 트래픽이 폭증합니다. 정작 락을 쥔 스레드는 임계구역 실행을 위해 그 캐시라인을 읽어야 하는데, 대기자들의 RFO 폭격 때문에 오히려 락 소유자의 진행이 늦어지는 역설이 발생합니다.

### 3.2 TTAS: Load로 관찰, CAS는 승산 있을 때만

이 라이브러리는 `SpinLockDetail::SpinWait`을 통해 TTAS 패턴을 구현합니다:

```cpp
while (true)
{
    SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
        [this]() noexcept { return _locked.load(std::memory_order_relaxed); }
    );
    if (TryLock()) return;
}
```

핵심은 대기 루프에서 **순수 `load()`만 반복**한다는 것입니다. `load()`는 캐시라인을 `Shared` 상태로 유지하므로, 락이 풀리기 전까지는 로컬 캐시에서 계속 읽기만 하고 인터커넥트에 트래픽을 만들지 않습니다. 락이 실제로 풀려서(`false` 관측) `load()`가 대기를 벗어난 뒤에야 `TryLock()`(CAS)을 단 한 번 시도합니다. "승산이 있어 보일 때만 비싼 CAS를 쓴다"는 것이 TTAS의 요지입니다.

`RWSpinLock`의 `ReadLock`/`WriteLock`도 동일한 원칙을 따릅니다 — `SpinWait`의 조건식은 전부 `load(std::memory_order_relaxed)`이고, 실제 상태 변경(`fetch_add`, `compare_exchange_strong`)은 대기가 끝난 뒤 딱 한 번만 시도합니다.

---

## 4. 적응형 백오프(Adaptive Backoff)

`SpinLockDetail::SpinWait`은 3단계로 대기 강도를 높여갑니다:

```cpp
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
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            yieldCount = 0;
        }
    }
}
```

| 단계 | 동작 | 목적 |
|---|---|---|
| 1단계 | `PAUSE` 명령을 지수적으로 증가시키며 반복(`1 → 2 → 4 → ... → MaxPauseBackoff`) | 짧은 경합에서 지연을 최소화하면서도, 스핀 도중 파이프라인 투기 실행(speculation) 낭비와 메모리 서브시스템 부하를 줄임 |
| 2단계 | `std::this_thread::yield()` | 실행 큐의 다른 스레드에게 타임슬라이스를 양보 — 락 소유자가 다른 스레드라면 더 빨리 스케줄링되게 유도 |
| 3단계 | `sleep_for(1μs)` 후 카운터 리셋 | OS 스케줄러에 명시적으로 개입을 요청, 2단계를 계속 반복하는 무한 루프 방지 |

`PAUSE`(x86) / `yield`(ARM) 명령은 스핀 루프에서 파이프라인의 메모리 순서 위반 예측(memory order violation speculation)을 억제해 전력 소비를 낮추고, 하이퍼스레딩 환경에서 같은 물리 코어의 형제 스레드에게 실행 자원을 양보하는 효과도 있습니다. 이것이 단순 `while(){}` 스핀보다 `PAUSE` 삽입 스핀이 표준적으로 권장되는 이유입니다.

3단계의 지속시간을 `0`이 아닌 짧은 양수(`1μs`)로 잡은 이유는, 플랫폼에 따라 `sleep_for(0)`이 `std::this_thread::yield()`와 동일하게 처리되어(예: 일부 libc의 `nanosleep(0)` 구현) 2단계와 3단계가 사실상 구분되지 않을 수 있기 때문입니다. 양의 지속시간을 주면 구현체가 실제 타이머 기반 슬립 경로를 타서, 오래 대기하는 스레드에게 OS 스케줄러가 개입할 명확한 시간을 벌어줍니다.

Preset(`LightWeight` / `Default` / `HeavyContention` / `OverSubscribed`)은 이 세 파라미터의 임계값을 조정해, 예상 경합 수준에 맞는 백오프 곡선을 선택할 수 있게 합니다. 예를 들어 `OverSubscribed`(스레드 수 > 코어 수)는 `MaxPauseBackoff`를 낮게 잡아 CPU 스핀보다 빨리 `yield`로 넘어가게 되어 있습니다 — 코어가 부족한 상황에서 무의미한 스핀을 오래 지속하면 다른 스레드의 실행 기회만 빼앗기 때문입니다.

---

## 5. RWSpinLock: 32비트 하나에 인코딩된 상태 머신

### 5.1 비트 레이아웃

```
비트:   31 ... 16 | 15 ... 1        | 0
필드:  Writer 대기 | Reader 카운트    | Write 배타 플래그
       (16 bit)    | (15 bit)        | (1 bit)
```

```cpp
namespace RWSpinLockBits
{
    inline constexpr int32_t WRITE_LOCKED        = 0x00000001; // bit 0
    inline constexpr int32_t READER_COUNT_MASK   = 0x0000FFFE; // bit 1~15
    inline constexpr int32_t READER_ONE          = 0x00000002; // reader +1
    inline constexpr int32_t WRITER_WAITING_MASK = 0xFFFF0000; // bit 16~31
    inline constexpr int32_t WRITER_ONE          = 0x00010000; // writer 대기 +1
}
```

단일 `atomic<int32_t>` 하나로 "현재 읽는 스레드 수", "쓰기 중 여부", "쓰기를 기다리는 스레드 수"를 동시에 표현합니다. 이렇게 하나의 워드에 다중 필드를 우겨넣는 이유는 명확합니다 — 상태를 여러 개의 원자 변수로 나누면 그 사이에 원자성이 깨지는(다른 스레드가 중간 상태를 관측하는) 타이밍이 생기지만, 단일 CAS는 전체 상태 전이를 하나의 원자적 연산으로 만들어 줍니다.

`RWSpinLockBits`는 별도 네임스페이스로 분리되어 있고, `SpinLock.inl` 내부에서는 항상 `RWSpinLockBits::` 접두사를 붙여 참조합니다. `.inl`이 `SpinLock.h`를 통해 다른 소스 파일에 그대로 펼쳐진다는 점을 감안하면, 여기서 네임스페이스 전체를 여는 것보다 완전한 정규화(qualified) 참조를 쓰는 편이 인클루드하는 쪽의 전역 네임스페이스를 오염시키지 않아 더 안전합니다.

### 5.2 Reader의 진입/이탈

`ReadLock`은 "writer가 대기 중이거나 쓰기 중이 아니면" reader 카운트를 늘립니다. 다만 TOCTOU(Time-Of-Check-Time-Of-Use) 경쟁을 피하기 위해, **먼저 카운트를 올린 뒤(`fetch_add`) 결과를 재검증**하는 2단계 패턴을 씁니다:

```cpp
const int32_t prev = _state.fetch_add(RWSpinLockBits::READER_ONE, std::memory_order_acquire);
if ((prev & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) == 0)
{
    // 진입 시점에 writer가 없었음이 확정 → 성공
    return;
}
_state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed); // 롤백
```

`fetch_add` 시점의 이전 상태(`prev`)를 검사하는 것이 핵심입니다. `fetch_add` 자체가 원자적이므로, 만약 `prev`에 writer 관련 비트가 없었다면 그 reader는 "writer 없음"이 보장된 순간에 카운트를 올린 것이 확정됩니다. 반대로 writer가 껴 있었다면 즉시 롤백(`fetch_sub`)하고 `SpinWait`으로 되돌아갑니다.

### 5.3 Writer의 2단계 진입 — 기아 방지

`WriteLock`은 reader보다 한 단계가 더 있습니다:

```cpp
const int32_t prev = _state.fetch_add(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed);
if (((prev + RWSpinLockBits::WRITER_ONE) & RWSpinLockBits::WRITER_WAITING_MASK) == 0)
{
    _state.fetch_sub(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed); // 오버플로우 시 등록 취소
    SPINLOCK_FATAL("RWSpinLock::WriteLock - writer waiting count overflow (max 65535)");
}

SpinLockDetail::SpinWait<...>(
    [this]() noexcept
    {
        int32_t expected = _state.load(std::memory_order_relaxed);
        if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0) return true; // 계속 대기

        return !_state.compare_exchange_strong(
            expected, expected | RWSpinLockBits::WRITE_LOCKED,
            std::memory_order_acquire, std::memory_order_relaxed);
    }
);
```

1. **선(先) 등록**: 실제로 락을 잡기도 전에 `WRITER_ONE`을 먼저 더해 "나는 쓰기를 원한다"는 의사를 상태에 새깁니다.
2. **관찰 대기**: 이 등록 덕분에 `WRITER_WAITING_MASK`가 0이 아니게 되고, 이는 §5.2의 `ReadLock` 진입 조건(`(prev & WRITER_WAITING_MASK) == 0`)을 실패시켜 **새로운 reader의 진입을 즉시 차단**합니다.
3. **획득**: reader 카운트가 0이 되고 다른 writer도 없는 순간을 포착해 `WRITE_LOCKED` 비트를 CAS로 세웁니다.

이 설계의 목적은 **writer 기아 방지**입니다. Reader가 끊임없이 들어오는 read-heavy 워크로드에서, writer가 "reader가 0명일 때만 카운트 증가 시도"라는 순진한 전략을 쓰면 영원히 기회를 못 잡을 수 있습니다(reader 진입 → writer 시도 실패 → reader 이탈 전 또 다른 reader 진입 → 반복). 여기서는 writer가 **의사를 먼저 선언**함으로써, 이후 도착하는 reader들의 진입 자체를 막아버려 "현재 진행 중인 reader들이 다 빠지면 writer가 반드시 다음 차례를 가져간다"는 순서를 보장합니다.

대기 카운트가 상한을 넘어서는 극단적 상황에서는 `SPINLOCK_FATAL`로 즉시 프로세스를 중단시키기 전에, 방금 등록한 `WRITER_ONE`을 롤백합니다. `SPINLOCK_FATAL`은 기본적으로 `std::terminate()`를 호출해 프로세스를 종료시키지만, 커스텀 terminate 핸들러가 설치되어 실행이 계속되는 예외적인 경우까지 고려해 상태를 항상 일관되게 유지하는 방어적 설계입니다.

### 5.4 TryWriteLock — CAS 재시도 루프

`TryWriteLock`은 블로킹 없이 즉시 성공/실패를 반환해야 하므로 §5.3의 "선등록 후 대기" 2단계 구조를 쓸 수 없습니다. 대신 CAS 재시도 루프 안에서, 소유자가 없는 것이 확인되는 즉시 `WRITER_ONE`과 `WRITE_LOCKED`를 **한 번의 CAS로 함께** 설정합니다:

```cpp
int32_t expected = _state.load(std::memory_order_relaxed);
if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0)
    return false;

// WRITER_ONE은 '카운터 필드'이므로 비트 뭉개짐을 방지하기 위해 OR(|)가 아닌 덧셈(+)을 쓴다.
// WRITE_LOCKED 비트는 위 조건문에서 이미 0임이 검증되었으므로 +로 합산해도 완전히 안전하다.
const int32_t desired = expected + RWSpinLockBits::WRITER_ONE + RWSpinLockBits::WRITE_LOCKED;

if (!_state.compare_exchange_strong(expected, desired,
        std::memory_order_acquire, std::memory_order_relaxed))
    return false; // 단 한 번만 시도하고, 실패하면 즉시 반환한다(try_lock 의미론)

return true;
```

`WriteUnlock()`이 항상 `WRITER_ONE + WRITE_LOCKED`를 함께 `fetch_sub`하도록 구현되어 있으므로, `TryWriteLock()`의 획득 경로도 반드시 두 필드를 함께 세팅해야 대칭이 맞습니다. 이때 `WRITER_ONE`은 여러 writer가 동시에 누적시킬 수 있는 **카운터 필드**이므로, 이미 다른 스레드가 `WriteLock()`으로 대기 카운트를 올려둔 상태(`expected != 0`)에서 단순 비트 OR를 쓰면 두 스레드의 등록분이 같은 비트에 겹쳐 하나로 뭉개집니다. 그래서 반드시 산술 덧셈으로 합산해, 이미 등록된 다른 writer의 카운트를 보존한 채 정확히 +1만 반영합니다.

`TryWriteLock`은 `try_lock`류 API의 표준적인 기대에 맞춰 **단 한 번의 CAS만 시도하고 실패하면 즉시 반환**합니다(`SpinLock::TryLock`, `TryReadLock`과 동일한 스타일) — 재시도 루프를 두지 않으므로, CAS 실패가 실제 소유 상태 때문이든 다른 writer의 무해한 카운트 갱신과 우연히 겹친 것이든 구분하지 않고 그 자리에서 `false`를 돌려줍니다. 다시 시도할지는 호출자의 몫입니다.

**공정성 정책**: 이 함수의 진입 가드는 `READER_COUNT_MASK`와 `WRITE_LOCKED`만 검사하고 `WRITER_WAITING_MASK`는 보지 않습니다. 즉 이미 `WriteLock()`으로 등록해 reader가 빠지길 기다리는 중인 다른 writer가 있어도, 그 순간 reader가 0명이라면 `TryWriteLock()`을 호출한 스레드가 새치기해서 락을 가져갈 수 있습니다. 이는 실수가 아니라 의도된 동작입니다 — 이 라이브러리가 보장하는 공정성의 범위는 §5.3에서 설명한 "reader에 의한 writer 기아 방지"까지이고, **여러 writer 사이의 도착 순서 자체는 원래 보장 대상이 아닙니다.** 여러 writer 간의 엄격한 FIFO 순서가 필요한 자원이라면 `TryWriteLock()`을 섞어 쓰지 말고 `WriteLock()`만 사용해야 합니다.

### 5.5 오버플로우 가드

Reader 카운트(15비트, 최대 32767)와 writer 대기 카운트(16비트, 최대 65535)는 각각 상한이 있습니다. 이 라이브러리는 필드가 상한을 넘어 인접 필드를 침범하기 직전에, 방금 반영한 카운트 변경을 롤백한 뒤 `SPINLOCK_FATAL`로 즉시 프로세스를 중단시킵니다:

```cpp
if (((prev + RWSpinLockBits::READER_ONE) & RWSpinLockBits::READER_COUNT_MASK) == 0)
{
    _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
    SPINLOCK_FATAL("RWSpinLock::ReadLock - reader count overflow (max 32767)");
}
```

이는 "조용히 잘못된 상태로 계속 실행되는 것"보다 "그 자리에서 확실하게 죽이는 것"이 디버깅과 안전성 양쪽에서 낫다는 fail-fast 철학입니다. 실제로 게임 서버에서 3만 개 이상의 스레드가 동시에 같은 락에서 read를 대기하는 상황은 사실상 설계 오류의 징후이므로, 이 어서션이 걸린다면 락 사용 패턴 자체를 재검토해야 합니다. 롤백을 먼저 수행하는 이유는 위 §5.3에서 설명한 것과 동일합니다 — `SPINLOCK_FATAL`이 프로세스를 종료시키기 전 순간에도 상태 일관성을 유지하기 위함입니다.

---

## 6. 메모리 순서(Memory Ordering) 설계

| 연산 | Ordering | 근거 |
|---|---|---|
| `TryLock` / `ReadLock`/`ReadUnlock`(진입) / `WriteLock`/`TryWriteLock`(CAS 성공) | `acquire` | 락 획득 후 임계구역의 모든 읽기가 이 시점 **이후**로 재배치되지 않도록 보장 — 락 소유자가 이전 소유자의 쓰기 결과를 확실히 관측 |
| `Unlock` / `ReadUnlock` / `WriteUnlock` | `release` | 임계구역 내 모든 쓰기가 이 시점 **이전**으로 완료된 것처럼 보이도록 보장 — 다음 락 획득자에게 정확히 전파 |
| `SpinWait` 내부의 관찰용 `load` | `relaxed` | 단순히 "값이 바뀌었는지"만 확인하는 폴링이므로 순서 보장이 불필요 — acquire보다 저렴 |
| Reader의 선등록/롤백(`fetch_add`/`fetch_sub`) 중 실패 경로 | `relaxed` | 진입에 실패해 즉시 롤백하는 카운트 변경은 어차피 임계구역에 진입하지 않으므로 happens-before 관계가 필요 없음 |

이 조합은 **acquire-release 페어링**이라는 표준 락 구현 패턴입니다. `release`로 커밋된 임계구역의 부작용(side effect)은, 그 값을 `acquire`로 읽는 다음 스레드에게 정확히 순서대로 보이게 됩니다. 반면 대기 루프의 폴링(`relaxed`)까지 `acquire`로 하면 매 스핀마다 불필요한 메모리 배리어 비용이 들어가므로, "실제로 락을 획득하는 그 순간"에만 `acquire`를 배치한 것이 이 구현의 핵심 최적화입니다.

---

## 7. RAII Guard와 매크로 계층

```cpp
#define USE_LOCK    mutable RWSpinLock<RWSpinLockPreset::Default> _lock
#define WRITE_LOCK  CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                      __write_lock_guard__(_lock, LockType::Write, __func__)
#define READ_LOCK   CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                      __read_lock_guard__(_lock, LockType::Read, __func__)
```

`USE_LOCK`으로 멤버 락을 선언하고, 함수 진입부에 `WRITE_LOCK;` 또는 `READ_LOCK;` 한 줄만 추가하면 해당 스코프 종료 시(예외 포함) 자동으로 언락되는 구조입니다. `__func__`(C++11 표준 예약 식별자)가 자동으로 프로파일러 이름으로 전달되어, `_DEBUG` && `__THREADMANAGER_H__` 빌드에서는 어느 함수가 락을 오래 쥐고 있는지 데드락 프로파일러로 추적할 수 있습니다. `__FUNCTION__`(MSVC 확장)이 아니라 표준 식별자를 쓴 덕분에 컴파일러 간 이식성도 확보됩니다.

`Lock`/`ReadLock`/`WriteLock`/`TryReadLock`/`TryWriteLock`은 모두 `const char* name = nullptr` 파라미터를 받는 동일한 시그니처를 가지며, `TryReadLock`/`TryWriteLock`은 **락 획득에 실제로 성공했을 때만** `gpDeadLockProfiler->PushLock(name)`을 호출합니다. 실패한 시도까지 이력에 남기면 프로파일러 결과가 왜곡되기 때문입니다.

사용 예:

```cpp
class SessionManager
{
public:
    void AddSession(SessionRef session)
    {
        WRITE_LOCK;
        _sessions.push_back(session);
    }

    size_t GetSessionCount() const
    {
        READ_LOCK;
        return _sessions.size();
    }

private:
    USE_LOCK;
    std::vector<SessionRef> _sessions;
};
```

---

## 8. 설계 노트: 공정성(Fairness)과 티켓락/MCS락

현재 두 락은 모두 **unfair 스핀락**입니다 — CAS에서 이긴 스레드가 그냥 락을 가져가는 구조라, 대기 순서를 보장하지 않습니다. 경쟁이 아주 심한 환경에서는 특정 스레드가 계속 CAS 경쟁에서 밀려 이론상 무기한 대기할 수 있습니다(단, `RWSpinLock`의 writer는 §5.3의 "선(先) 등록" 메커니즘 덕분에 이 문제에서 예외입니다 — reader에 의한 writer 기아는 방지되어 있습니다).

더 강한 공정성이 필요하다면 다음 두 대안을 고려할 수 있습니다:

- **티켓락(Ticket Lock)**: `atomic<uint64_t>` 하나에 "발급 번호(ticket)"와 "현재 차례(serving)"를 분리해 두고, 스레드는 자신의 발급 번호가 현재 차례와 같아질 때까지 대기합니다. 완전한 FIFO 순서를 보장하지만, 모든 대기자가 같은 `serving` 값을 스핀 대기하므로 §3.1에서 설명한 캐시라인 핑퐁 문제가 여전히 남습니다.
- **MCS락(Mailbox/Queue 기반 락)**: 각 스레드가 자신의 큐 노드에서만 스핀하도록 만들어, 락 해제 시 앞사람이 정확히 다음 사람의 노드만 갱신합니다. 캐시라인 핑퐁이 없고 FIFO 공정성도 보장되지만, 노드를 스택에 두고 링크드 리스트로 관리해야 해서 지금 라이브러리의 "32비트 하나로 상태를 압축한다"는 설계 철학과는 다른 방향입니다.

이 라이브러리는 짧은 임계구역·낮은 경합을 전제로 한 저지연 최적화에 초점을 맞추고 있으므로, 지금 구조를 유지하되 공정성이 중요한 특정 자원(예: 전역 카운터, 드문 대형 자료구조 갱신)에 한해 티켓락/MCS락을 **별도 클래스로 분리 구현**해 선택적으로 쓰는 것을 권장합니다.

---

## 9. 사용상 주의사항

- **재진입 불가(Non-reentrant)**: 동일 스레드가 같은 락을 두 번 `Lock()`하면 즉시 데드락입니다. 재귀 호출 경로에 락 획득 함수가 있는지 항상 확인하세요.
- **Writer 내부에서 Reader 락 획득 금지**: `WriteLock()` 보유 중 같은 락에 `ReadLock()`을 호출하면 자기 자신이 세워둔 `WRITE_LOCKED` 비트에 막혀 데드락입니다.
- **임계구역은 짧게**: 스핀락은 짧은 임계구역을 전제로 설계되었습니다. I/O, 시스템 콜, 다른 락 대기 등 지연이 큰 작업을 임계구역 안에 넣지 마세요.
- **Preset 선택**: 코어 수 대비 스레드 수가 많다면(`OverSubscribed`) 낮은 백오프 상한을, read-heavy 워크로드라면 `ReadHeavy` preset을 사용해 튜닝하세요.
