# 동기화 프리미티브 라이브러리 구조 분석
### SpinLock / RWSpinLock / PlatformLock / SRWLock / DeadLockProfiler

---

## 1. 개요

이 라이브러리는 세 개의 계층으로 구성된다.

```
[3] CDeadLockProfiler        — 락 이름 기반 획득 순서 그래프, 사이클(데드락) 탐지
[2] PLock / PRWLock          — 플랫폼별 최적 프리미티브를 선택하는 통합 인터페이스
[1] SpinLock<Preset>         — 크로스 플랫폼용 스핀락 (atomic<bool>)
    RWSpinLock<Preset>       — 크로스 플랫폼용 읽기/쓰기 스핀락 (atomic<int32_t> 비트필드)
    CSRWLock                 — Windows 네이티브 SRWLOCK 래퍼
```

`PLock`/`PRWLock`은 `_WIN32`/`_WIN64` 정의 여부에 따라 내부 구현체를 `CSRWLock` 또는
`SpinLockDefault`/`RWSpinLockDefault`로 컴파일 타임에 교체한다. 모든 계층의 Lock/Unlock API는
`const char* name = nullptr` 매개변수를 받아 `CDeadLockProfiler`로 전달하며, 이는 디버그 빌드에서
`USE_GPDEADLOCKPROFILER && _DEBUG`가 정의된 경우에만 활성화된다.

---

## 2. SpinLock.h — [1] 기본 계층

### 2.1 SpinLock\<Preset\>

- `SpinLockPreset`에 4가지 프리셋(`LightWeight`, `Default`, `HeavyContention`, `OverSubscribed`)을
  정의하고, 각각 `MaxPauseBackoff`/`MaxYieldCount`를 지정해 백오프 전략을 상황별로 튜닝한다.
- `alignas(kCacheLineSize)` + 명시적 `_padding` 배열로 객체 크기를 캐시라인 1개(`kCacheLineSize`)로
  고정한다. `static_assert`로 (a) `atomic<bool>`이 캐시라인보다 커서 패딩 계산이 언더플로우하는 경우,
  (b) 최종 `sizeof(SpinLock)`가 캐시라인 크기와 정확히 일치하는지를 각각 검증한다. 정렬(alignas)은
  시작 주소만 보장하고 크기는 보장하지 않는다는 전제를 명시적으로 문서화해 두었다.
- 복사/이동 생성자·대입 전부 `delete` — 락은 고유 주소를 가져야 하므로 값 의미론을 원천 차단.
- `Lock`/`Unlock`은 프로파일러 추적용 `name` 매개변수를 받고, `TryLock`은 받지 않는다(비블로킹
  경로는 데드락 사이클과 무관하므로 profiler에 보고하지 않는 설계로 보인다).
- `SPIN_USE_LOCK`/`SPIN_LOCK` 매크로로 클래스에 손쉽게 락 필드와 RAII 가드를 부여.

### 2.2 RWSpinLock\<Preset\>

- 단일 `atomic<int32_t> _state`에 4개 필드를 비트로 압축:
  - `WRITE_LOCKED` (bit0), `READER_COUNT` (bit1~15, `READER_ONE`씩 증감),
    `WRITER_WAITING` (bit16~31, `WRITER_ONE`씩 증감).
- Writer-waiting 카운트를 상위 16비트에 별도로 두어, 활성 리더가 있는 동안에도 "쓰기 대기 중"임을
  표시할 수 있게 하는 구조 — 리더 기아(reader starvation) 방지용 우선순위 신호로 사용된다
  (`TryWriteLock` 언더플로우 버그를 이 필드에서 수정한 이력 있음, `/areas/spinlock-library.md` 참고).
- `SpinLock`과 동일하게 캐시라인 정렬 + 패딩 + `static_assert` 2종 적용.
- 읽기/쓰기 각각에 대해 `Lock`/`TryLock`/`Unlock` 3종 API를 대칭적으로 제공.
- `CustomLockGuard<LockObj>` : `LockType::Read`/`Write` 열거값으로 락 타입을 런타임 분기하는
  범용 RAII 가드. 매크로 `RWSPIN_WRITE_LOCK`/`RWSPIN_READ_LOCK`이 `__func__`를 이름으로 자동 바인딩.
- 매크로 네임스페이스가 `SPIN_*` / `RWSPIN_*`로 분리되어 있어 한 클래스 안에 SpinLock과 RWSpinLock을
  동시에 멤버로 둬도 매크로 충돌이 없다.

---

## 3. SRWLock.h / .cpp — Windows 네이티브 래퍼

- `CSRWLock`은 Win32 `SRWLOCK`을 감싼 얇은 래퍼로, `AcquireSRWLockExclusive/Shared`,
  `TryAcquireSRWLockExclusive/Shared`, `ReleaseSRWLockExclusive/Shared`를 그대로 호출한다.
- 헤더 주석에 SRWLock 자체의 제약(비재진입, Read→Write 승격 불가)을 명시해 호출부의 오용을 방지.
- 소멸자는 `_DEBUG`에서만 `TryExclusiveLock()`으로 "현재 아무도 잠그지 않았는가"를 확인한다.
  성공하면(=잠겨 있지 않았음을 의미) 그 자리에서 획득한 락을 즉시 해제해 원상태로 되돌리고,
  실패하면 `assert`로 "잠긴 채로 소멸됨"을 알린다 — 획득 자체가 검사 수단으로 쓰이는 패턴이다.
- 프로파일러 연동은 **성공한 획득에 한해서만** `PushLock`을 호출하고(`ExclusiveLock`은 블로킹이므로
  항상 성공 후 호출, `TryExclusiveLock`은 성공 시에만 호출), 해제 시엔 항상 `PopLock`을 호출해
  Push/Pop 짝을 맞춘다.
- 4종 RAII 가드(`ExclusiveLockGuard`, `SharedLockGuard`, `TryExclusiveLockGuard`,
  `TrySharedLockGuard`) + `CSRWCustomLockGuard` + `SRW_WRITE_LOCK`/`SRW_READ_LOCK` 매크로로
  `RWSpinLock`과 동일한 사용 패턴을 Windows 네이티브 락에도 제공한다. 세 계층(SPIN/RWSPIN/SRW)의
  매크로·가드 인터페이스가 의도적으로 대칭을 이루고 있다.

---

## 4. PlatformLock.h — [2] 통합 계층

### 4.1 PLock (Exclusive 전용)

| 플랫폼 | 내부 구현 |
|---|---|
| Windows | `CSRWLock`의 **Exclusive** API만 사용 (Shared는 쓰지 않음) |
| 그 외 | `SpinLockDefault` (`SpinLock<SpinLockPreset::Default>`) |

`PLock`은 읽기/쓰기 구분이 필요 없는 일반 임계 구역 보호용이며, Windows에서도 SRWLock을
단일 배타 락처럼만 사용한다 — 이는 SRWLock이 pthread mutex보다 가볍고 OS 지원 백오프를
활용할 수 있어, 크로스 플랫폼 스핀락과 대등한 "기본 락"으로 선택된 것으로 보인다.

### 4.2 PRWLock (Read/Write)

| 플랫폼 | 내부 구현 |
|---|---|
| Windows | `CSRWLock` (Shared/Exclusive 모두 사용) |
| 그 외 | `RWSpinLockDefault` |

### 4.3 공통 설계

- 두 클래스 모두 복사/이동 전면 금지, `noexcept` 전제.
- `#if PLATFORM_LOCK_WINDOWS` 분기 내부에서 미사용 `name` 매개변수를 `(void)name`으로
  명시적으로 무시 처리 — Windows 경로는 profiler 연동을 `CSRWLock` 내부에 위임하고 있어
  `PLock`/`PRWLock` 자체는 이름을 직접 사용하지 않지만, 시그니처 일관성을 위해 매개변수는 유지.
- `PLockGuard`, `PRReadLockGuard`, `PRWriteLockGuard` 3종 RAII 가드가 하위 계층과 동일한
  이름 전달 패턴을 유지해, 상위 코드에서는 플랫폼을 몰라도 동일한 방식으로 프로파일링 태그를 남길 수 있다.

---

## 5. DeadLockProfiler.h / .cpp — [3] 락 순서 사이클 탐지

### 5.1 자료구조

- `thread_local CStack<int32> LLockStack` : 스레드별 "현재 보유 중인 락" 스택(이름을 정수 id로
  변환해 저장).
- `_nameToId` / `_idToName` : 락 이름(문자열) ↔ 정수 id 매핑, 최초 등장 시 자동 발급.
- `_lockHistory : CMap<int32, CSet<int32>>` : **"이름" 단위** 락 간 인접 리스트. 즉 락
  *인스턴스*가 아니라 락 *이름(카테고리)* 간의 획득 순서 그래프를 전역적으로 누적한다.

### 5.2 PushLock / PopLock

- `PushLock`: 이름→id 확인/등록 후, 현재 스레드가 이미 보유 중인 락(스택 top)이 있다면
  `prevId → newId` 방향의 간선을 그래프에 추가한다. **직전에 잡고 있던 락과의 관계만** 기록하며,
  스택 전체 원소와의 모든 쌍을 기록하지는 않는다 — 중첩 락 순서만으로 사이클이 존재하면 전이적으로
  드러나기 때문에 필요/충분한 최소 정보만 남기는 설계.
- 간선이 **새로 추가된 경우에만** `CheckCycle()`을 실행한다 — 이미 관측된 순서쌍에 대해서는
  매번 전체 그래프를 재검사하지 않도록 하는 최적화. 그래프가 안정화된 이후에는 새로운 lock-pair
  조합이 나타날 때만 비용이 발생한다.
- `PopLock`: 스택이 비었거나(`MULTIPLE_UNLOCK`) top이 예상 id와 다르면(`INVALID_UNLOCK`)
  `CRASH`로 즉시 중단 — 언락 순서 오류(LIFO 위반)를 조기에 잡아낸다.

### 5.3 CheckCycle / Dfs — 사이클(데드락 가능성) 탐지

- 매 호출 시 `_discoveredOrder`/`_finished`/`_parent` 배열을 락 개수만큼 재초기화하고,
  모든 노드에 대해 `Dfs`를 실행하는 고전적 **DFS 기반 방향 그래프 사이클 탐지**(discover time +
  finished 플래그로 back-edge 판별) 알고리즘이다.
- `Dfs(here)`가 인접 노드 `there`를 방문할 때:
  - 미방문이면 `_parent[there]=here` 기록 후 재귀 진입 (tree edge).
  - `discoveredOrder[here] < discoveredOrder[there]`이면 순방향 간선(자손) — 무시.
  - 그 외, `there`가 아직 `_finished`되지 않았다면 **역방향 간선(조상)** — 즉 사이클 발견.
    `_parent` 체인을 역추적해 락 이름 순서를 출력하고 `CRASH("DEADLOCK_DETECTED")`로 강제 종료한다.
- 이 메커니즘은 "실제 데드락 발생"을 기다리지 않고, **락 A→B, B→A처럼 상충하는 획득 순서가
  프로그램 어디선가 한 번이라도 함께 관측되면** 잠재적 데드락으로 간주해 즉시 크래시시키는
  정적/동적 하이브리드 방식이다(락 인스턴스가 달라도 이름이 같으면 동일 노드로 취급되므로,
  같은 이름의 락을 서로 다른 잠금 순서로 중첩 사용하는 패턴을 걸러내는 데 초점이 맞춰져 있다).

### 5.4 SRWLock.cpp와의 연동

- `USE_GPDEADLOCKPROFILER && _DEBUG`가 정의된 빌드에서만 `gpDeadLockProfiler`(외부 전역 인스턴스,
  `extern` 선언만 존재 — 실체는 다른 TU에서 정의된다고 가정)를 통해 `PushLock`/`PopLock`을 호출한다.
- `name`이 `nullptr`이면 프로파일링을 건너뛴다 — 이름을 지정하지 않고 사용하는 락(예: 핫패스에서
  오버헤드를 피하고 싶은 경우)은 사이클 탐지 대상에서 자연스럽게 제외된다.

---

## 6. 계층 간 설계 일관성 요약

| 항목 | SpinLock | RWSpinLock | CSRWLock | PLock/PRWLock |
|---|---|---|---|---|
| 복사/이동 | 금지 | 금지 | 금지 | 금지 |
| 캐시라인 정렬 | O (패딩+static_assert) | O (패딩+static_assert) | 해당 없음(OS 핸들) | 해당 없음(멤버 위임) |
| profiler name 전달 | Lock/Unlock만 | 전 API | 전 API(성공 시만 Push) | 전 API(시그니처만, Windows는 내부 위임) |
| RAII 가드 | SpinLockGuard | ReadLockGuard/WriteLockGuard/CustomLockGuard | 4종 개별 가드 + CSRWCustomLockGuard | PLockGuard/PRReadLockGuard/PRWriteLockGuard |
| 전용 매크로 | `SPIN_*` | `RWSPIN_*` | `SRW_*` | 없음(가드 직접 사용) |

세 계층 모두 "이름 매개변수를 받아 profiler에 전달"하는 동일한 계약(contract)을 유지하고 있어,
상위 계층(`PLock`/`PRWLock`)에서 하위 구현체를 완전히 교체해도 호출부 코드와 데드락 탐지
연동 방식은 변하지 않는다. `CDeadLockProfiler`는 이 계약에 의존해 락 종류에 상관없이
이름 기반으로 전역 획득 순서 그래프를 구성한다.
