# SpinLock / RWSpinLock 라이브러리 상세 설계 문서

C++17 기반 락프리 스핀락 2종(`SpinLock`, `RWSpinLock`)의 설계 배경, 내부 구현, 동시성 이론, 검증·운용 지침을 다룬다.

---

## 1. 전체 구조

| 구분 | `SpinLock` | `RWSpinLock` |
|---|---|---|
| 상태 표현 | `std::atomic<bool>` | `std::atomic<int32_t>` 비트마스크 |
| 동시 접근 | 상호 배제 | Reader 다수 + Writer 단독 |
| 공정성 | 없음 | Reader→Writer 기아 방지 (Writer 간 순서는 비보장) |
| Guard | `SpinLockGuard` | `ReadLockGuard` / `WriteLockGuard` / `CustomLockGuard` |
| 메모리 배치 | 1캐시라인 고정 | 1캐시라인 고정 |
| 프로파일링 훅 | `_DEBUG && __THREADMANAGER_H__`에서 `name` 인자로 데드락 프로파일러 연동 | 동일 |

두 락 모두 캐시라인 경계 정렬(`alignas`) + 명시적 패딩으로 정확히 한 캐시라인을 점유한다. 이 설계의 목적은 단일하다: **락 객체 자체가 인접 데이터와 캐시라인을 공유하지 않도록 강제해 false sharing의 가능성을 원천 차단**하는 것이다.

```cpp
class alignas(kCacheLineSize) SpinLock { ... };
static_assert(sizeof(SpinLock<...>) == kCacheLineSize, "...");
```

`alignas`와 패딩은 서로 다른 것을 보장한다는 점이 중요하다.

- `alignas(kCacheLineSize)` → 객체의 **시작 주소**가 캐시라인 경계에 오도록 강제한다.
- `char _padding[kCacheLineSize - sizeof(...)]` → 객체의 **크기(sizeof)** 자체를 캐시라인 배수로 고정한다.

이 둘 중 하나만 있으면 배열로 락을 선언했을 때 문제가 생긴다. `alignas`만 있고 패딩이 없다면 `SpinLock<Preset> locks[8]` 배열에서 `locks[0]`은 캐시라인 경계에 오지만, `sizeof(SpinLock<Preset>)`이 캐시라인보다 작을 경우 `locks[1]`이 `locks[0]`과 같은 캐시라인에 겹쳐 들어갈 수 있다. 두 필드가 함께 있어야 배열의 **모든** 원소가 각자의 캐시라인을 독점한다.

`static_assert(sizeof(std::atomic<bool>) <= kCacheLineSize, ...)`가 패딩 계산보다 먼저 오는 이유도 명확하다. `kCacheLineSize`는 부호 없는 `std::size_t`이므로, 만약 원자 타입의 크기가 캐시라인을 초과하는 극단적 상황(비표준 확장 등)이 온다면 `kCacheLineSize - sizeof(...)` 뺄셈이 음수 대신 거대한 양수로 언더플로우된다. 이는 `char _padding[매우 큰 수]`라는 사실상 컴파일 실패 또는 메모리 낭비로 이어지므로, 그 상황을 뺄셈이 실행되기 전에 명확한 에러 메시지로 먼저 걸러낸다.

또한 `std::hardware_destructive_interference_size`가 64바이트보다 큰 플랫폼(일부 ARM big.LITTLE 구성 등)에서는 `kCacheLineSize` 자체가 커지므로, 패딩과 객체 전체 크기도 그에 비례해 커진다. "정확히 1개 캐시라인"이라는 불변식은 플랫폼에 무관하게 유지되지만, 그 절대 크기는 플랫폼마다 달라질 수 있다는 뜻이다.

---

## 2. 스핀락을 택한 이유 — Mutex 대비 트레이드오프

`std::mutex`가 경합을 만나면 커널 퓨텍스(futex)를 통해 스레드를 슬립시킨다. 이 경로의 비용은 단순 시스템 콜 비용을 넘어선다: 컨텍스트 스위칭 시 레지스터 세이브/리스토어, TLB flush 가능성, 캐시 워밍업 재수행까지 포함하면 수 마이크로초 단위의 지연이 발생한다.

게임 서버의 핫패스 — 예를 들어 세션별 패킷 큐에 초당 수천 회 접근하는 코드 — 에서 임계구역 자체는 수십~수백 나노초에 불과한 경우가 많다. 이런 상황에서 매번 뮤텍스의 슬립/웨이크 비용을 지불하는 것은 "일 자체보다 대기 방식에 드는 비용이 더 큰" 역설을 만든다.

스핀락은 락이 풀릴 때까지 CPU를 계속 점유하며(busy-wait) 대기한다. 임계구역이 짧다면 컨텍스트 스위칭 없이 몇 번의 루프만으로 락을 획득할 수 있어 훨씬 빠르다. 대신 다음 두 조건이 깨지면 손해로 전환된다.

1. **임계구역이 길어질 때**: 대기자들이 아무 일도 못 하면서 CPU 사이클만 소모한다.
2. **스레드 수가 논리 코어 수를 초과할 때**: 락을 쥔 스레드가 스케줄러에 의해 밀려나 있는 동안, 대기자들이 헛되이 스핀을 돌려도 락 소유자는 실행 재개를 못 하므로 지연이 오히려 증폭된다(이른바 "lock holder preemption" 문제).

따라서 이 라이브러리는 "짧은 임계구역 + 스레드 수가 코어 수 이내인 동시성"이라는 전제 위에서만 유효하다는 것을 설계 전제로 깔고 있으며, `OverSubscribed` 프리셋은 두 번째 조건이 깨진 상황을 완화하기 위한 장치다(§4에서 다룬다).

---

## 3. TTAS와 캐시 코히런시

### 3.1 순수 CAS 스핀이 왜 나쁜가

가장 단순한 스핀락 구현은 대기 루프 안에서도 계속 `compare_exchange`를 시도한다. 문제는 CAS가 성공 여부와 무관하게 **항상 쓰기 의도(RFO, Read-For-Ownership)를 캐시 코히런시 프로토콜에 전파**한다는 점이다.

MESI(Modified-Exclusive-Shared-Invalid) 프로토콜 하에서, 여러 코어가 같은 캐시라인을 `Shared` 상태로 들고 있더라도 한 코어가 CAS를 시도하는 순간 그 라인을 `Modified`로 승격시켜야 한다. 이는 다른 모든 코어가 들고 있던 사본을 `Invalid`로 만든다. 대기 중인 스레드 N개가 전부 CAS를 반복하면, 매 시도마다 캐시라인 소유권이 코어 사이를 오가며(cache line ping-pong) 코어 간 인터커넥트(QPI/UPI/Infinity Fabric 등)에 트래픽이 집중된다.

더 나쁜 것은, 정작 락을 쥔 스레드가 임계구역을 실행하려고 그 캐시라인(락 상태 자체 혹은 인접 데이터)을 읽으려 할 때도 같은 인터커넥트 경합에 걸린다는 점이다. 대기자들의 실패할 것이 뻔한 CAS 폭격이 오히려 락 소유자의 임계구역 실행을 늦추는 역설이 발생한다 — 대기자 수가 늘수록 시스템 전체 처리량이 떨어지는 것도 이 때문이다.

### 3.2 TTAS(Test-and-Test-and-Set): Load로 관찰, CAS는 승산 있을 때만

```cpp
while (true)
{
    SpinLockDetail::SpinWait<Preset::MaxPauseBackoff, Preset::MaxYieldCount>(
        [this]() noexcept { return _locked.load(std::memory_order_relaxed); }
    );
    if (TryLock()) return;
}
```

대기 루프에서는 `load()`만 반복한다. `load()`는 캐시라인을 `Shared` 상태로 유지하므로, 락이 풀리기 전까지는 각 코어가 자신의 로컬 캐시에서만 값을 읽고 인터커넥트에는 아무 트래픽도 만들지 않는다. `load()`가 "락 해제됨(`false`)"을 관측해 대기를 벗어난 뒤에야 비로소 `TryLock()`(CAS)을 시도한다.

이 패턴이 "Test(관찰) - Test-and-Set(시도)"이라는 이름의 유래다: 값을 읽어서(Test) 승산이 있어 보일 때만 비싼 원자적 갱신(Test-and-Set)을 쓴다. `RWSpinLock`의 `ReadLock`/`WriteLock`도 동일 원칙을 따른다 — `SpinWait`에 전달되는 조건 람다는 예외 없이 `relaxed` `load()`이고, 실제 상태 변경 연산(`fetch_add`, `compare_exchange_strong`)은 대기가 끝난 뒤 단 한 번만 실행된다.

여러 스레드가 동시에 락 해제를 관측하고 동시에 CAS로 몰릴 수는 있지만(이른바 "thundering herd"), TTAS는 이 경우의 수를 줄여줄 뿐 완전히 없애지는 못한다. 이 잔여 경합을 흡수하는 것이 다음 절의 적응형 백오프다.

---

## 4. 적응형 백오프 3단계

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

### 4.1 1단계 — PAUSE 지수 백오프

`backoff`를 1부터 시작해 매 라운드 2배씩 늘리며(`1 → 2 → 4 → ... → MaxPauseBackoff`) `SPINLOCK_PAUSE()`를 그만큼 반복 실행한다. 값을 늘리다가 `MaxPauseBackoff`의 절반을 넘으면 그 이상은 늘리지 않고 상한에서 고정한다(`backoff = (backoff <= MaxPauseBackoff/2) ? backoff*2 : MaxPauseBackoff`) — 오버플로우 없이 자연스럽게 상한에 수렴하는 클램프다.

x86의 `PAUSE`, ARM의 `YIELD` 명령은 두 가지 효과를 낸다.

- 스핀 루프 내에서 CPU가 메모리 순서 위반을 예측 실행(speculative execution)하려는 시도를 억제해, 파이프라인 플러시로 인한 전력 낭비와 지연을 줄인다.
- 하이퍼스레딩(SMT) 환경에서 같은 물리 코어를 공유하는 형제 논리 스레드에게 실행 자원(디코더, 실행 유닛)을 양보하는 힌트로 동작한다.

즉 `PAUSE` 없이 순수 `while(){}`로 스핀하는 것보다, 같은 스핀이라도 `PAUSE`를 섞는 편이 전력·지연 양면에서 유리하다. 지수적으로 횟수를 늘리는 이유는, 경합이 짧게 끝날 것으로 기대되는 초반에는 최대한 빨리 재시도하되, 경합이 예상보다 길어지면 점점 더 오래 "가만히" 있어 인터커넥트 부하를 줄이기 위함이다.

### 4.2 2단계 — yield

PAUSE 상한에 도달하면 `std::this_thread::yield()`를 최대 `MaxYieldCount`회까지 호출한다. 이는 OS 스케줄러에게 "지금 이 스레드의 남은 타임슬라이스를 다른 실행 가능한 스레드에게 넘겨도 된다"는 힌트를 준다. 락 소유자가 다른 코어에서 실행 중이라면 PAUSE만으로 충분하지만, 락 소유자가 (오버섭스크립션 등으로) 같은 코어의 실행 대기열에 있다면 yield가 그 스레드에게 실행 기회를 앞당겨 줄 수 있다.

### 4.3 3단계 — sleep_for(1μs)

yield 한도까지 소진하면 `sleep_for(1μs)` 후 `yieldCount`를 0으로 리셋해 2단계로 돌아간다. 지속시간을 `0`이 아닌 명시적 양수로 잡은 이유가 핵심이다: 일부 libc 구현에서 `sleep_for(0ns)`는 내부적으로 `nanosleep(0)`을 호출하고, 이것이 다시 `sched_yield()`와 동일하게 처리되는 경우가 있다. 그렇게 되면 3단계가 2단계와 사실상 구분되지 않아 "OS 스케줄러의 명시적 개입을 유도한다"는 3단계 고유의 목적이 무력화된다. 1마이크로초라는 양의 지속시간을 주면 구현체가 실제 타이머 기반 슬립 경로를 타게 되어, 오래 대기하는 스레드에 대해 스케줄러가 확실히 개입할 시간을 벌어준다.

### 4.4 프리셋별 곡선

| 프리셋 | MaxPauseBackoff | MaxYieldCount | 의도 |
|---|---|---|---|
| `LightWeight` | 256 | 16 | 경합이 거의 없다고 예상되는 락(예: 초기화 1회성 락)에서 백오프 단계 진입을 빠르게 |
| `Default` | 1024 | 64 | 범용 |
| `HeavyContention` | 4096 | 128 | 다수 스레드가 자주 부딪히는 락(예: 전역 리소스 카운터)에서 PAUSE 단계를 길게 유지해 인터커넥트 부하를 최대한 억제 |
| `OverSubscribed` | 32 | 8 | 스레드 수가 코어 수를 초과하는 환경에서, 무의미한 PAUSE 스핀을 빨리 포기하고 yield/sleep으로 넘어가 락 소유자에게 실행 기회를 양보 |

`OverSubscribed`가 다른 세 프리셋과 근본적으로 다른 목적을 갖는다는 점이 중요하다. 나머지 세 프리셋은 "코어가 충분한 상황에서 얼마나 참을성 있게 스핀할 것인가"를 조절하지만, `OverSubscribed`는 "코어가 부족한 상황에서 얼마나 빨리 스핀을 포기하고 양보할 것인가"를 조절한다. 코어가 부족하면 대기자의 스핀 자체가 락 소유자의 실행 기회를 빼앗는 가해자가 되므로, 이 프리셋은 임계값을 낮춰 최대한 빨리 3단계(명시적 sleep)로 넘어가게 설계되어 있다.

---

## 5. RWSpinLock: 32비트 하나에 인코딩된 상태 머신

### 5.1 비트 레이아웃

```
비트:   31 ............ 16 | 15 ............ 1 | 0
필드:   Writer 대기 카운트   | Reader 카운트      | Write 배타 플래그
        (16 bit, max 65535) | (15 bit, max 32767)| (1 bit)
```

```cpp
namespace RWSpinLockBits
{
    inline constexpr int32_t WRITE_LOCKED        = 0x00000001; // bit 0
    inline constexpr int32_t READER_COUNT_MASK   = 0x0000FFFE; // bit 1~15
    inline constexpr int32_t READER_ONE          = 0x00000002; // reader +1 단위
    inline constexpr int32_t WRITER_WAITING_MASK = 0xFFFF0000; // bit 16~31
    inline constexpr int32_t WRITER_ONE          = 0x00010000; // writer 대기 +1 단위
}
```

단일 `atomic<int32_t>`에 세 필드를 함께 담는 이유는 원자성 때문이다. 만약 "reader 수", "writer 대기 수", "쓰기 락 여부"를 세 개의 개별 원자 변수로 나눈다면, 그 세 변수 사이에는 원자성이 없다 — 어떤 스레드가 A를 갱신하고 B를 갱신하기 직전 사이의 틈을 다른 스레드가 관측할 수 있다. 하나의 워드로 합치면 단일 `load`/`fetch_add`/`compare_exchange`가 세 필드의 상태 전이 전체를 원자적으로 처리한다.

`RWSpinLockBits`가 별도 네임스페이스로 분리되고 `.inl` 내부에서 매번 `RWSpinLockBits::` 접두사로 정규화 참조되는 것도 설계상 이유가 있다. `SpinLock.inl`은 `SpinLock.h`에 `#include`되어, 그 헤더를 쓰는 모든 번역 단위에 그대로 펼쳐진다. 만약 여기서 `using namespace RWSpinLockBits`를 쓰면 그 심볼들이 헤더를 인클루드하는 모든 소스 파일의 전역 네임스페이스로 새어나간다. 완전한 정규화 참조를 쓰면 이 오염을 원천적으로 막을 수 있다.

### 5.2 Reader의 진입 — 낙관적 증가 후 검증(Optimistic-then-Verify)

```cpp
const int32_t prev = _state.fetch_add(RWSpinLockBits::READER_ONE, std::memory_order_acquire);
if ((prev & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) == 0)
{
    return; // 성공
}
_state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed); // 롤백
```

TOCTOU(Time-Of-Check-Time-Of-Use) 경쟁을 피하는 방식이 핵심이다. "값을 읽어서 조건을 확인한 뒤 별도로 카운트를 올리는" 2단계 방식이라면, 확인과 실행 사이에 다른 스레드가 끼어들어 확인했던 조건이 무효화될 수 있다. 이 코드는 순서를 뒤집어 **먼저 카운트를 원자적으로 올리고(`fetch_add`), 그 연산이 반환하는 "올리기 직전의 상태"(`prev`)를 사후에 검증**한다.

`fetch_add` 자체가 원자적 read-modify-write이므로, `prev`에 writer 관련 비트가 없었다면 "정확히 이 reader가 카운트를 올린 그 순간에는 writer가 없었다"는 것이 100% 확정된다(그 이후에 writer가 도착할 수는 있지만, 그건 이 reader의 락 획득 순서가 그 writer보다 앞섰다는 의미일 뿐 오류가 아니다). 반대로 `prev`에 writer 관련 비트가 서 있었다면, 이 reader의 진입 자체가 writer와 겹친 것이므로 방금 올린 카운트를 롤백(`fetch_sub`)하고 `SpinWait`으로 돌아가 재시도한다.

### 5.3 Writer의 2단계 진입 — 선(先) 등록을 통한 기아 방지

```cpp
// 1단계: 선(先) 등록
const int32_t prev = _state.fetch_add(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed);
if (((prev + RWSpinLockBits::WRITER_ONE) & RWSpinLockBits::WRITER_WAITING_MASK) == 0)
{
    _state.fetch_sub(RWSpinLockBits::WRITER_ONE, std::memory_order_relaxed);
    SPINLOCK_FATAL("RWSpinLock::WriteLock - writer waiting count overflow (max 65535)");
}

// 2단계: 관찰 후 획득
SpinLockDetail::SpinWait<...>(
    [this]() noexcept
    {
        int32_t expected = _state.load(std::memory_order_relaxed);
        if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0) return true;
        return !_state.compare_exchange_strong(
            expected, expected | RWSpinLockBits::WRITE_LOCKED,
            std::memory_order_acquire, std::memory_order_relaxed);
    }
);
```

Writer가 reader보다 한 단계를 더 거치는 이유는 **기아 방지**다. 만약 writer가 단순히 "reader가 0명이고 다른 writer도 없을 때 CAS로 쓰기 락을 잡는다"는 순진한 전략만 쓴다면, read-heavy 워크로드에서 다음과 같은 시나리오가 반복될 수 있다.

```
t0: reader A 진입 (READER_COUNT = 1)
t1: writer W가 CAS 시도 → reader 있음 → 실패, 재시도
t2: reader A 이탈 직전, reader B 진입 (READER_COUNT 여전히 ≥ 1)
t3: writer W가 CAS 시도 → 또 실패
... (reader들이 끊임없이 겹쳐 들어오면 W는 이론상 무한 대기)
```

이 라이브러리는 이를 막기 위해 writer가 락을 실제로 잡기 전에 **"나는 쓰기를 원한다"는 의사부터 먼저 상태에 새긴다**(`fetch_add(WRITER_ONE)`). 이 등록이 있으면 `WRITER_WAITING_MASK`가 0이 아니게 되고, §5.2의 reader 진입 조건(`(prev & (WRITER_WAITING_MASK | WRITE_LOCKED)) == 0`)이 실패하게 되어 **이 시점 이후 도착하는 모든 새 reader의 진입이 차단**된다. 이미 진입해 있던 reader들은 자연스럽게 빠져나가고, reader 카운트가 0이 되는 순간 writer가 CAS로 `WRITE_LOCKED`를 세워 락을 획득한다.

이 메커니즘이 보장하는 것은 정확히 "**reader에 의한 writer 기아 방지**"다. 즉, writer가 대기를 선언한 이후에는 유한한 시간 내에(현재 진행 중인 reader들이 다 빠지는 즉시) 반드시 락을 얻는다. 다만 이것이 여러 writer 사이의 순서까지 보장하지는 않는다는 점은 §5.4에서 별도로 다룬다.

오버플로우 가드에서 `SPINLOCK_FATAL` 호출 **전에** `fetch_sub`로 방금 등록한 카운트를 롤백하는 순서도 의도적이다. `SPINLOCK_FATAL`은 기본적으로 `std::terminate()`로 프로세스를 종료시키지만, 만약 커스텀 terminate 핸들러가 설치되어 있어 예외적으로 실행이 계속되는 경우까지 고려하면, 상태를 항상 일관되게 유지해 두는 편이 안전하다.

### 5.4 TryWriteLock — 카운터 필드와 플래그 필드를 구분해서 합성

```cpp
int32_t expected = _state.load(std::memory_order_relaxed);
if ((expected & (RWSpinLockBits::READER_COUNT_MASK | RWSpinLockBits::WRITE_LOCKED)) != 0)
    return false;

const int32_t desired = expected + RWSpinLockBits::WRITER_ONE + RWSpinLockBits::WRITE_LOCKED;

if (!_state.compare_exchange_strong(expected, desired,
        std::memory_order_acquire, std::memory_order_relaxed))
    return false;

return true;
```

`TryWriteLock`은 블로킹이 허용되지 않으므로 §5.3의 "선등록 후 대기" 2단계 구조를 그대로 쓸 수 없다. 대신 "소유 가능 상태 확인 + 획득"을 CAS 한 번으로 합친다.

여기서 `desired` 값을 만들 때 `WRITER_ONE`과 `WRITE_LOCKED`를 **덧셈(+)**으로 합성하는 것이 핵심 디테일이다. 이 선택의 이유는 두 필드의 성격이 다르기 때문이다.

- `WRITE_LOCKED`는 **단일 비트 플래그**다. 이 함수의 가드 조건(`(expected & WRITE_LOCKED) != 0 → return false`)에서 이미 0임이 확인되었으므로, OR로 세우든 덧셈으로 더하든 결과는 같다.
- `WRITER_ONE`은 **여러 writer가 동시에 누적시킬 수 있는 다중 비트 카운터 필드**다. `WriteUnlock()`이 항상 `WRITER_ONE + WRITE_LOCKED`를 함께 `fetch_sub`하도록 구현되어 있으므로, 획득 시에도 반드시 대칭적으로 이 카운터를 증가시켜야 한다. 만약 이미 다른 스레드가 `WriteLock()`으로 대기 카운트를 올려둔 상태(`expected`의 상위 16비트가 0이 아닌 상태)에서 단순 비트 OR로 `WRITER_ONE`을 세운다면, 두 스레드의 등록분이 같은 비트 위치에 겹쳐 하나로 뭉개진다.

이 뭉개짐이 실제로 어떤 사고로 이어지는지 추적해 보면:

```
1) writer W1이 WriteLock()으로 선등록 → WRITER_WAITING = 1
2) writer W2가 TryWriteLock() 호출, 만약 OR로 세팅했다면:
   desired = expected | WRITER_ONE | WRITE_LOCKED
   → WRITER_WAITING 필드가 이미 1(0b...0001)이므로 OR해도 여전히 1로 "뭉개짐"
   → 사실상 W2의 등록이 누락된 것과 같은 결과
3) 이후 W1과 W2가 각각 WriteUnlock()을 호출하면 각각 WRITER_ONE을 감산 →
   총 2번 감산되지만 실제로는 1번만 증가했으므로 WRITER_WAITING 필드가 언더플로우
4) 언더플로우된 상위 16비트가 큰 값으로 남아 WRITER_WAITING_MASK가 항상 0이 아니게 됨
   → 이후 도착하는 모든 ReadLock()이 §5.2 조건에 걸려 영구 대기
5) 대기 중인 reader들이 재시도를 반복하며 fetch_add/fetch_sub를 계속 반복 →
   특정 타이밍에 READER_COUNT_MASK 필드가 오버플로우해 SPINLOCK_FATAL 크래시까지 이어질 수 있음
```

따라서 산술 덧셈(`+`)으로 합성하는 것은 단순한 스타일 선택이 아니라, 이미 등록되어 있는 다른 writer의 카운트를 보존한 채로 정확히 +1만 반영하기 위한 필수 조건이다.

`TryWriteLock`은 `try_lock` 계열 API의 표준적인 기대치에 맞춰 **CAS를 단 한 번만 시도하고 실패하면 즉시 `false`를 반환**한다(`SpinLock::TryLock`, `TryReadLock`과 동일한 스타일이며, 내부에 재시도 루프가 없다). CAS 실패가 실제로 다른 스레드가 락을 쥐고 있어서인지, 아니면 다른 writer의 무해한 카운트 갱신과 우연히 타이밍이 겹쳐서인지는 구분하지 않는다 — 다시 시도할지 여부는 항상 호출자의 판단에 맡긴다.

**공정성 정책의 의도적 범위**: 이 함수의 진입 가드는 `READER_COUNT_MASK`와 `WRITE_LOCKED`만 검사하고, `WRITER_WAITING_MASK`는 확인하지 않는다. 즉 이미 `WriteLock()`으로 등록해 reader가 빠지기를 기다리는 다른 writer가 있어도, 그 순간 reader가 0명이라면 `TryWriteLock()`을 호출한 스레드가 그 사이를 비집고 들어가 락을 가져갈 수 있다. 이는 실수가 아니라 명확히 의도된 설계다 — 이 라이브러리가 보장하는 공정성의 범위는 §5.3에서 설명한 **"reader에 의한 writer 기아 방지"까지**이며, **여러 writer 사이의 도착 순서 자체는 애초에 보장 대상이 아니다.** 여러 writer 간 엄격한 FIFO 순서가 필요한 자원이라면 해당 락에 대해서는 `TryWriteLock()`을 섞어 쓰지 말고 `WriteLock()`만 사용해야 한다.

### 5.5 TryReadLock — 성공 시에만 프로파일러 기록

```cpp
const int32_t prev = _state.fetch_add(RWSpinLockBits::READER_ONE, std::memory_order_acquire);
if ((prev & (RWSpinLockBits::WRITER_WAITING_MASK | RWSpinLockBits::WRITE_LOCKED)) == 0)
{
    ...
#if defined(_DEBUG) && defined(__THREADMANAGER_H__)
    if (name) gpDeadLockProfiler->PushLock(name);
#endif
    return true;
}
_state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
return false;
```

`TryReadLock`과 `TryWriteLock` 모두 **락 획득에 실제로 성공했을 때만** `PushLock`을 호출한다. 실패한 시도까지 프로파일러 이력에 남기면 "이 스레드가 이 락을 들고 있다"는 프로파일러의 스택 상태가 실제 락 소유 상태와 어긋나게 되어, 데드락 탐지 자체가 오작동할 수 있기 때문이다.

### 5.6 오버플로우 가드 — Fail-Fast 철학

```cpp
if (((prev + RWSpinLockBits::READER_ONE) & RWSpinLockBits::READER_COUNT_MASK) == 0)
{
    _state.fetch_sub(RWSpinLockBits::READER_ONE, std::memory_order_relaxed);
    SPINLOCK_FATAL("RWSpinLock::ReadLock - reader count overflow (max 32767)");
}
```

reader 카운트(15비트, 최대 32767)나 writer 대기 카운트(16비트, 최대 65535)가 인접 필드를 침범하기 직전이 되면, 방금 반영한 카운트 변경을 롤백한 뒤 `SPINLOCK_FATAL`로 그 자리에서 프로세스를 중단시킨다. "조용히 잘못된 상태로 계속 실행되도록 두는 것"보다 "확실하게, 그리고 즉시 죽이는 것"이 디버깅 용이성과 안전성 양쪽에서 더 낫다는 fail-fast 철학이다.

3만 개 이상의 스레드가 동시에 같은 락에서 read를 대기하는 상황은 정상적인 서버 부하로 보기 어렵고, 대개는 락을 과도하게 세분화하지 못했거나 특정 락으로 접근이 과도하게 쏠리는 설계 결함의 징후다. 이 어서션이 실제로 걸린다면 카운트를 늘리는 방향이 아니라, 해당 락 사용 패턴 자체(샤딩 부족, 락 스코프 과다 등)를 재검토해야 한다.

---

## 6. 메모리 순서(Memory Ordering) 설계

| 연산 | Ordering | 근거 |
|---|---|---|
| `SpinLock::TryLock` 성공 | `acquire` | 락 획득 이후의 모든 메모리 읽기가 이 시점 이전으로 재배치되지 않도록 보장 — 이전 소유자가 `release`로 커밋한 쓰기를 확실히 관측 |
| `RWSpinLock::ReadLock`/`TryReadLock` 진입(`fetch_add`) | `acquire` | writer가 `release`로 커밋한 이전 임계구역의 쓰기를 reader가 확실히 관측 |
| `WriteLock`/`TryWriteLock`의 최종 CAS | `acquire` | 이전 writer 혹은 마지막 reader들의 쓰기를 확실히 관측한 뒤에 임계구역 진입 |
| `Unlock` / `ReadUnlock` / `WriteUnlock` | `release` | 임계구역 내 모든 쓰기가 이 시점 이전에 완료된 것처럼 다음 획득자에게 보이도록 커밋 |
| `SpinWait` 내부 관찰용 `load` | `relaxed` | 단순히 "값이 바뀌었는가"만 폴링하는 것이므로 순서 보장 불필요 — `acquire` 대비 훨씬 저렴 |
| 실패 시 롤백(`fetch_sub`) | `relaxed` | 임계구역에 진입하지 못하고 되돌리는 연산이므로 happens-before 관계를 맺을 필요가 없음 |
| `WriteLock` 1단계 선등록(`fetch_add(WRITER_ONE)`) | `relaxed` | 이 시점에는 아직 락을 획득한 것이 아니라 "의사 표시"만 하는 것이므로 가시성 보장이 불필요 — 실제 acquire는 2단계 CAS에서 일어남 |

이는 표준적인 **acquire-release 페어링** 패턴이다. `release`로 커밋된 임계구역의 부작용은, 그 값을 `acquire`로 읽는 다음 스레드에게 happens-before 관계로 정확히 전파된다. 반면 대기 루프의 폴링까지 `acquire`로 하면 스핀할 때마다 불필요한 메모리 배리어 비용이 들어간다 — 아직 락을 획득하지도 못한 상태에서 강한 순서를 강제할 이유가 없기 때문이다. "실제로 락을 획득하는 바로 그 순간에만 `acquire`를 배치한다"는 것이 이 구현 전체를 관통하는 최적화 원칙이다.

한 가지 짚어볼 만한 지점은 `WriteLock`의 1단계가 `relaxed`인 이유다. 이 단계는 아직 임계구역에 진입한 게 아니라 "나는 쓰기를 원한다"는 카운터를 하나 올리는 것뿐이므로, 이 자체에는 어떤 메모리 가시성 보장도 필요 없다. 실제로 다른 스레드의 쓰기 결과를 관측해야 하는 시점은 2단계의 CAS이고, 거기서만 `acquire`가 걸려 있다.

---

## 7. RAII Guard와 매크로 계층

```cpp
#define USE_LOCK    mutable RWSpinLock<RWSpinLockPreset::Default> _lock
#define WRITE_LOCK  CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                      __write_lock_guard__(_lock, LockType::Write, __func__)
#define READ_LOCK   CustomLockGuard<RWSpinLock<RWSpinLockPreset::Default>> \
                      __read_lock_guard__(_lock, LockType::Read, __func__)
```

`USE_LOCK`으로 멤버 락을 선언하고, 함수 진입부에 `WRITE_LOCK;` 또는 `READ_LOCK;` 한 줄만 추가하면 스코프 종료 시(정상 반환이든 예외를 통한 탈출이든) 자동으로 언락된다. `Lock`/`ReadLock`/`WriteLock`/`TryReadLock`/`TryWriteLock`이 모두 `noexcept`로 선언되어 있어, 락 관련 연산 자체가 예외를 던지지 않는다는 전제 위에서 RAII 가드의 소멸자도 안전하게 `noexcept`로 유지된다.

`__func__`(C++11 표준 예약 식별자, `static const char[]`)가 자동으로 프로파일러 이름 인자로 전달된다는 점도 설계 의도가 있다. MSVC 전용 확장인 `__FUNCTION__` 대신 표준 식별자를 쓴 덕분에 컴파일러 간 이식성이 확보되며, `_DEBUG && __THREADMANAGER_H__`가 정의된 빌드에서는 어느 함수가 어떤 락을 얼마나 오래 쥐고 있는지 데드락 프로파일러(`gpDeadLockProfiler`)로 추적할 수 있다. 릴리스 빌드에서는 이 매크로 조건이 거짓이므로 `PushLock`/`PopLock` 호출 자체가 통째로 컴파일되지 않아 런타임 오버헤드가 전혀 없다.

`CustomLockGuard`가 `LockType` 열거값으로 읽기/쓰기를 런타임에 선택하는 구조인 이유는, `WRITE_LOCK`/`READ_LOCK` 매크로가 컴파일 타임에 결정된 리터럴을 넘기더라도 내부적으로는 `if (_type == LockType::Write) ... else ...`라는 단일 코드 경로로 두 가드 타입을 통합해, 별도의 `WriteLockGuard`/`ReadLockGuard` 클래스와 중복 없이 프로파일러 연동 로직을 한 곳에서 관리하기 위함이다. 실제로 `WriteLockGuard`/`ReadLockGuard`는 각각 단일 락 타입 전용 RAII 가드로 별도 존재하며, `CustomLockGuard`는 매크로 계층에서 사용하는 범용 버전이다.

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

`GetSessionCount()`가 `const` 멤버 함수인데도 `READ_LOCK`으로 `_lock`을 갱신할 수 있는 이유는 `USE_LOCK` 매크로가 `_lock`을 `mutable`로 선언하기 때문이다. 논리적 상수성(logical constness)을 유지하면서도 락 상태 자체는 갱신 가능해야 하는 전형적인 상황이다.

---

## 8. 플랫폼 추상화 매크로

| 매크로 | MSVC | GCC/Clang (x86/x64) | GCC/Clang (ARM) | 그 외 |
|---|---|---|---|---|
| `SPINLOCK_PAUSE()` | `_mm_pause()` | `__builtin_ia32_pause()` | 인라인 어셈블리 `yield` | `std::this_thread::yield()` |
| `SPINLOCK_DEBUG_BREAK()` | `__debugbreak()` | 인라인 어셈블리 `int3` | 인라인 어셈블리 `brk #0` | `::raise(SIGTRAP)` |

`SPINLOCK_FATAL(msg)`는 이 두 매크로를 조합한 상위 매크로다.

```cpp
#define SPINLOCK_FATAL(msg)                                         \
    do {                                                            \
        ::fprintf(stderr, "[SPINLOCK FATAL] %s:%d  %s\n", __FILE__, __LINE__, msg); \
        ::fflush(stderr);                                           \
        SPINLOCK_DEBUG_BREAK();                                     \
        std::terminate();                                           \
    } while (false)
```

실행 순서가 "로그 출력 → flush → 디버거 브레이크 → terminate"인 이유는 각각 목적이 다르다.

1. **`fprintf` + `fflush`**: 표준 에러로 즉시 flush해, 이후 프로세스가 어떻게 종료되든(디버거가 없는 프로덕션 환경 포함) 원인 로그가 유실 없이 남도록 한다.
2. **`SPINLOCK_DEBUG_BREAK()`**: 디버거가 붙어 있는 개발 환경이라면 여기서 실행이 멈춰, 크래시 이전 시점의 전체 콜스택과 각 스레드의 상태를 그대로 조사할 수 있다. 디버거가 없는 환경에서는 `SIGTRAP`이 기본 핸들러에 의해 처리되어 다음 줄로 넘어간다.
3. **`std::terminate()`**: 디버거가 없는 환경에서도 프로세스가 확실히 종료되도록 하는 최종 안전장치다.

`do { ... } while(false)` 래핑은 매크로가 `if (cond) SPINLOCK_FATAL(...); else ...` 같은 문맥에서도 세미콜론 하나로 정상적인 단일 문장처럼 동작하도록 하는 C 매크로의 표준 관용구다.

---

## 9. CacheAlignment.h — 캐시라인 유틸리티 분리의 의도

```cpp
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
#include <new>
inline constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kCacheLineSize = 64;
#endif

template <typename T>
struct alignas(kCacheLineSize) CachePaddedAtomic
{
    static_assert(sizeof(std::atomic<T>) <= kCacheLineSize, "...");
    std::atomic<T> value{};
    char _padding[kCacheLineSize - sizeof(std::atomic<T>)]{};
};
```

`kCacheLineSize`와 `CachePaddedAtomic<T>`가 `SpinLock.h`가 아니라 별도 헤더로 분리되어 있는 이유는, 이 유틸리티가 락 자체보다 더 범용적인 관심사이기 때문이다. `SpinLock`/`RWSpinLock`은 이 상수를 락 객체 자체의 크기를 고정하는 데 쓰지만, `CachePaddedAtomic<T>`는 락과 무관하게 **원자 변수 배열에서의 false sharing**을 방지하는 범용 도구다. 예를 들어 커넥션 풀에서 슬롯별로 독립된 참조 카운트를 배열로 관리할 때, 서로 다른 스레드가 인접한 배열 원소를 동시에 갱신하면 락이 전혀 관여하지 않아도 캐시라인 경합이 발생한다. 이런 상황에 `CachePaddedAtomic<T>[]`를 쓰면 각 원소가 독립된 캐시라인을 차지해 문제를 없앨 수 있다.

`std::hardware_destructive_interference_size`를 조건부로 사용하는 이유는 C++17 표준 라이브러리 기능이지만 컴파일러/표준 라이브러리 구현체에 따라 지원 시점이 다르기 때문이다. `__cpp_lib_hardware_interference_size` 기능 테스트 매크로로 지원 여부를 확인하고, 지원하지 않는 환경에서는 x86/x64의 통상적인 캐시라인 크기인 64바이트로 안전하게 폴백한다.

`kCachePaddedAtomicSizeCheck<T>`는 이 크기 불변식을 컴파일 타임 `constexpr bool`로 노출해, 이 유틸리티를 사용하는 다른 코드에서도 `static_assert(kCachePaddedAtomicSizeCheck<MyType>)`처럼 재사용할 수 있게 한 것이다.

---

## 10. 공정성 이론: 티켓락/MCS락과의 비교

현재 두 락은 모두 **unfair 스핀락**이다 — CAS 경쟁에서 이긴 스레드가 그냥 가져가는 구조라 대기 순서를 보장하지 않는다. `SpinLock`은 어떤 순서 보장도 없고, `RWSpinLock`의 writer는 §5.3의 선등록 메커니즘 덕분에 "reader에 의한 기아"에서만 예외적으로 보호된다(writer 간의 순서 자체는 여전히 비보장, §5.4 참고).

더 강한 공정성이 필요한 상황을 위해 두 대안을 검토할 수 있다.

**티켓락(Ticket Lock)**: `atomic<uint64_t>` 하나를 "발급 번호(ticket)"와 "현재 차례(serving)"로 분리하고, 스레드는 자신이 발급받은 번호가 현재 차례와 같아질 때까지 대기한다. `fetch_add`로 발급받는 순서가 곧 실행 순서이므로 완전한 FIFO를 보장한다. 다만 모든 대기자가 같은 `serving` 값을 스핀 대기하므로, §3.1에서 설명한 캐시라인 핑퐁 문제가 여전히 남는다 — `serving`이 갱신될 때마다 대기 중인 모든 스레드의 캐시가 무효화된다.

**MCS락(Mellor-Crummey & Scott, Queue 기반 락)**: 각 스레드가 자신만의 큐 노드를 스택 등에 두고 그 노드 안에서만 스핀하도록 구성한다. 락을 해제하는 스레드는 정확히 "다음 순서인 스레드의 노드"만 갱신하므로, 대기 중인 다른 스레드들의 캐시라인은 전혀 건드리지 않는다. 캐시라인 핑퐁이 없고 FIFO 공정성도 보장되지만, 노드를 연결 리스트로 관리해야 하므로 "32비트 하나로 상태를 압축한다"는 이 라이브러리의 설계 철학과는 다른 방향이며, 락 객체 자체의 메모리 풋프린트도 늘어난다.

이 라이브러리는 짧은 임계구역과 낮은 경합을 전제로 한 저지연 최적화에 초점을 맞추고 있으므로, 현재 구조를 유지하되 공정성이 특별히 중요한 특정 자원(예: 전역 카운터, 드물게 갱신되는 대형 자료구조)에 한해 티켓락이나 MCS락을 별도 클래스로 분리 구현해 선택적으로 조합하는 방식을 권장한다.

---

## 11. 사용상 주의사항

- **재진입 불가(Non-reentrant)**: 동일 스레드가 같은 `SpinLock`을 두 번 `Lock()`하거나, `RWSpinLock`에서 `WriteLock()` 보유 중 같은 락에 `ReadLock()`을 호출하면 즉시 데드락이다. 후자의 경우 자기 자신이 세워둔 `WRITE_LOCKED` 비트에 스스로 막히게 된다. 재귀 호출 경로 어딘가에 락 획득 함수가 다시 나타나지 않는지 항상 확인해야 한다.
- **임계구역은 짧게**: 스핀락은 짧은 임계구역을 전제로 설계되었다. I/O, 시스템 콜, 다른 락 대기, 힙 할당처럼 지연이 예측 불가능한 작업을 임계구역 안에 넣으면 다른 대기자들이 그만큼 CPU를 낭비하며 스핀하게 된다.
- **noexcept 계약**: 모든 락/언락 함수가 `noexcept`이므로, 임계구역 내부에서 예외가 던져지더라도 RAII 가드의 소멸자에서 `Unlock`/`ReadUnlock`/`WriteUnlock`이 정상적으로 호출되어 락이 확실히 풀린다. 다만 락 함수 내부 자체에서 예외가 발생할 여지가 있는 코드를 추가해서는 안 된다 — `noexcept` 함수 내부에서 예외가 새어나가면 `std::terminate()`가 즉시 호출된다.
- **Preset 선택 기준**: 코어 수 대비 스레드 수가 많은 환경(`OverSubscribed`)인지, read-heavy 워크로드(`ReadHeavy`)인지, 원래도 경합이 심한 전역 자원(`HeavyContention`)인지에 따라 프리셋을 선택해 백오프 곡선을 워크로드에 맞춘다.
- **TryWriteLock의 새치기 가능성**: §5.4에서 다룬 대로 `TryWriteLock()`은 이미 대기 중인 다른 writer를 추월할 수 있다. 여러 writer 간 순서가 중요한 자원에는 `TryWriteLock()`을 섞어 쓰지 않는다.

---

## 12. 검증 전략 제안

락 라이브러리는 정상 동작 시 눈에 띄지 않다가, 특정 타이밍에서만 재현되는 방식으로 문제가 드러나는 경우가 많다. 다음과 같은 검증을 병행하는 것이 안전하다.

- **ThreadSanitizer(TSan)**: `-fsanitize=thread`로 빌드해 데이터 레이스와 잘못된 메모리 순서 사용을 런타임에 탐지한다. 특히 `relaxed`와 `acquire`/`release`가 섞인 이 코드베이스에서는 순서 지정 실수가 TSan으로 가장 잘 드러난다.
- **스트레스 테스트**: 코어 수보다 많은 스레드로 짧은 임계구역을 매우 높은 빈도로 반복하며 카운터 정합성(예: reader 진입/이탈 횟수 합이 항상 0으로 수렴하는지)을 검증한다. `TryWriteLock`의 카운터 뭉개짐처럼 특정 타이밍 조합에서만 드러나는 버그는 장시간 반복 스트레스에서만 재현되는 경우가 많다.
- **오버플로우 경계 테스트**: reader 32767명, writer 대기 65535명 근처의 경계값을 의도적으로 유발해 `SPINLOCK_FATAL` 경로가 실제로 안전하게 동작하는지(롤백 후 종료, 상태 일관성 유지) 별도로 검증한다.
- **AddressSanitizer(ASan)와의 병행**: 스핀락 자체보다는 이를 사용하는 상위 자료구조(예: 락으로 보호되는 컨테이너)에서의 메모리 오류를 잡는 데 유용하다.

---

## 13. 요약

- `SpinLock`은 단일 bool 플래그 기반의 TTAS 배타적 락이며, `RWSpinLock`은 32비트 정수 하나에 reader 카운트·writer 대기 카운트·쓰기 락 플래그를 함께 인코딩해 원자적으로 관리한다.
- 두 락 모두 대기 루프에서는 `relaxed` load만 반복(TTAS)하고, 실제 상태 변경 시도(CAS/fetch_add)는 승산이 있을 때 단 한 번만 수행해 캐시라인 핑퐁을 최소화한다.
- PAUSE → yield → sleep의 3단계 적응형 백오프로 경합 강도에 따라 CPU 사용 패턴을 조절하며, 프리셋으로 워크로드별 튜닝이 가능하다.
- `RWSpinLock`은 writer의 "선등록 후 대기" 2단계 진입으로 reader에 의한 writer 기아를 방지하지만, writer 간의 순서 자체는 보장하지 않으며 `TryWriteLock()`은 이를 추월할 수 있다는 점이 명시적 설계 경계다.
- `TryWriteLock()`에서 카운터 필드(`WRITER_ONE`)와 플래그 필드(`WRITE_LOCKED`)를 산술 덧셈으로 구분해 합성하는 것, 오버플로우 가드에서 롤백 후 `SPINLOCK_FATAL`을 호출하는 순서는 모두 `WriteUnlock()`과의 상태 대칭성을 지키기 위한 핵심 불변조건이다.
- 캐시라인 정렬(`alignas`)과 명시적 패딩을 함께 사용해 객체의 시작 주소와 크기를 모두 캐시라인에 고정함으로써, 배열로 사용할 때도 인접 원소 간 false sharing이 발생하지 않는다.
- 현재 설계는 unfair 스핀락을 전제로 저지연에 최적화되어 있으며, 더 강한 공정성이 필요한 특정 자원에는 티켓락/MCS락을 별도로 조합하는 것을 권장한다.
