# 메모리 모듈 설계 문서

## 1. 개요

이 모듈은 게임 서버처럼 고빈도 할당/해제가 발생하는 환경에서 OS 기본 힙(`malloc`/`new`)의 성능·경합 문제를 회피하기 위한 커스텀 메모리 관리 계층입니다.

핵심 아이디어는 세 가지입니다.

1. **풀링(Pooling)** — 크기별로 미리 만들어둔 메모리 풀에서 블록을 재사용해 `malloc`/`free` 호출 자체를 줄인다.
2. **Lock-free** — 멀티스레드 환경에서 풀 접근 시 뮤텍스 없이 Windows `SLIST`(Interlocked Singly Linked List)로 경합을 최소화한다.
3. **전략 교체 가능성(Strategy 패턴)** — 실서비스용 풀 할당자, 디버그용 오버런 탐지 할당자, 그리고 실제 raw 메모리 소스(mimalloc 등)를 상황에 따라 갈아 끼울 수 있게 계층을 분리한다.

## 2. 전체 구조

```
사용자 코드 (게임 로직, 패킷, 세션 등)
        │
        │  xnew<T>() / xdelete() / StlAllocator<T>
        ▼
   PoolAllocator  ──────────────► gpMemory (CMemory 싱글턴)
        │                              │
        │                              │ 크기별 O(1) 라우팅 (ComputePoolIndex)
        │                              ▼
        │                        CMemoryPool (크기별, Lock-free SLIST)
        │                              │
        │                              │ 풀이 비었을 때만
        │                              ▼
        │                        RawAllocator (raw 메모리 소스)
        │                              │
        │              ┌───────────────┼───────────────┐
        │              ▼               ▼               ▼
        │         mimalloc         jemalloc         malloc (fallback)
        │
        └── (부트스트랩 전용) BaseAllocator ──► RawAllocator

디버그 빌드(_STOMP): CMemory::Allocate/Release가 StompAllocator로 대체됨
                     (VirtualAlloc 기반 페이지 가드, 오버런 즉시 크래시)
```

## 3. 계층별 역할

### 3.1 RawAllocator — 최하위 raw 메모리 소스

`RawAllocator.h` 하나의 네임스페이스로, `Alloc`/`Free`/`AllocAligned`/`FreeAligned` 네 함수를 제공합니다. 내부에서 컴파일 타임 매크로(`MIMALLOC_H`, `JEMALLOC_H_`, `TCMALLOC_TCMALLOC_H_`)로 실제 라이브러리를 분기하고, 아무것도 정의되지 않으면 표준 `malloc`/`_aligned_malloc`으로 폴백합니다.

- **런타임 분기가 아닌 컴파일 타임 분기**이므로 매 호출마다 오버헤드가 없습니다.
- 라이브러리마다 정렬 할당 함수의 인자 순서가 다른데(`mimalloc`은 `(size, alignment)`, `jemalloc`/`tcmalloc`은 `(alignment, size)`), 이 차이를 `AllocAligned` 내부에서 흡수해 상위 계층은 신경 쓸 필요가 없습니다.
- 실제로 mimalloc 등을 사용하려면 해당 라이브러리 헤더(`<mimalloc.h>` 등)를 이 파일보다 먼저(또는 `pch.h`에) include해서 매크로가 정의되게 해야 합니다. 헤더만 바꿔치기하면 프로젝트 전체의 raw 할당 소스가 교체됩니다.

### 3.2 BaseAllocator — 메모리 모듈과 무관한 범용 raw 할당 유틸리티

`RawAllocator`에 그대로 위임하는 최하위 계층 할당자입니다. **`CMemory`/`CMemoryPool` 등 메모리 모듈 자체는 이 클래스를 상속받지 않습니다.** 이 두 클래스는 프로그램 시작 시 단 한 번(혹은 `POOL_COUNT`번)만 생성되는 부트스트랩 객체라, 이 할당을 raw 경로로 격리하는 것보다 전역 `new`/`delete`를 그대로 쓰는 단순함을 우선했습니다.

대신 `BaseAllocator`는 **메모리 모듈과 무관한 다른 기능 클래스들이 필요할 때 가져다 쓰는 범용 유틸리티**로 존재합니다. 두 가지 방식으로 쓸 수 있습니다.

1. **정적 메서드 직접 호출**: `BaseAllocator::Alloc(size)` / `BaseAllocator::Release(ptr)` — raw 버퍼가 그때그때 필요할 때
2. **상속(믹스인)**: `class SomeClass : public BaseAllocator` 형태로 상속받으면, 클래스 내부에 정의된 `operator new`/`delete`가 자동으로 `RawAllocator` 경유로 오버라이드됩니다. `new SomeClass()`처럼 평범하게 쓴 코드가 별도 API 없이 raw 할당 경로를 타게 됩니다.

```cpp
class SomeLargeObject : public BaseAllocator
{
public:
	// ... 나머지 멤버
};
```

**언제 쓰는가**: 이 경로는 `CMemoryPool`/TLS 캐시를 거치지 않으므로, 고빈도로 생성/파괴되는 객체(`xnew`가 적합)보다는 **크기가 크거나 드물게 생성되는 객체를 풀과 분리하고 싶을 때** 적합합니다.

**주의할 점 — placement new 호환성**: `xnew<T>()`는 내부적으로 `new(memory)Type(...)` 형태의 placement new를 사용합니다. 클래스가 `operator new(size_t)`를 멤버로 선언하면 컴파일러가 그 클래스 스코프에서 전역 placement new를 가려버리므로, `BaseAllocator`에는 `operator new(size_t, void*)`/`operator delete(void*, void*)` placement 오버로드도 함께 정의되어 있습니다. 다만 애초에 `BaseAllocator` 상속은 풀을 거치지 않는 raw 경로를 원할 때 쓰는 것이므로, 그런 클래스를 `xnew`로 생성할 일 자체가 거의 없습니다 — 방어적으로만 남겨둔 것입니다.

**확장 정렬(over-alignment) 지원**: `alignas(32)` 이상(SIMD용 AVX 데이터 등)으로 선언되어 기본 정렬(보통 16바이트)을 넘어서는 클래스가 상속받는 경우를 위해, `BaseAllocator`는 C++17 확장 정렬 오버로드(`operator new(size_t, std::align_val_t)` 등)도 함께 제공합니다. 컴파일러는 `alignof(T)`가 `__STDCPP_DEFAULT_NEW_ALIGNMENT__`를 넘으면 이 오버로드를 자동으로 선택합니다. 이 오버로드가 없다면 컴파일 자체는 되지만, 정렬 정보가 유실된 채 일반 `operator new(size_t)`로 조용히 폴백되어 반환된 메모리가 실제로는 요청한 정렬을 만족하지 못하는(경고 없는 미정의 동작) 상황으로 이어질 수 있습니다 — SIMD 연산 도중의 크래시처럼 원인을 찾기 어려운 형태로 나타납니다. 이 오버로드는 `RawAllocator::AllocAligned`로 위임해 요청한 정렬을 실제로 보장합니다.

### 3.3 StompAllocator — 디버그 전용 오버런 탐지

메모리 오버런(버퍼 오버플로우)을 즉시 크래시로 잡아내는 진단 도구입니다.

- 요청 크기를 담을 데이터 페이지 수를 계산하고, 미리 예약해 둔 아레나(대형 가상 주소 공간) 안에서 그만큼을 커밋합니다.
- 데이터를 마지막 데이터 페이지의 **끝**에 딱 맞춰 배치합니다. 이렇게 하면 할당 크기를 1바이트라도 초과해서 쓰는 순간 다음 미커밋 페이지에 접근해 그 자리에서 Access Violation이 발생합니다.
- 요청 크기를 그대로 쓰지 않고 16바이트 단위로 올림한 뒤 배치 위치를 계산합니다. `MemoryHeader`가 16바이트 정렬을 요구하는데, 페이지 끝에서 곧바로 `size`만큼 물러난 위치는 `size`가 16의 배수가 아니면 정렬이 깨지기 때문입니다. 이 올림 때문에 실제 데이터 끝과 페이지 끝 사이에 최대 15바이트의 여유가 생겨, 그 범위 안의 아주 작은 오버런은 즉시 크래시로 잡히지 않을 수 있습니다(그 이상은 여전히 즉시 크래시).
- `CMemory::Allocate`/`Release` 내부의 `_STOMP` 매크로 분기로만 활성화되며, 평소 빌드에서는 꺼져 있어야 합니다.
- 페이지 단위 가드 자체가 목적이므로 `RawAllocator`로 대체하지 않고 `VirtualAlloc`/`VirtualFree`를 직접 사용합니다.

**아레나 구조**: 프로세스 시작 후 첫 `StompAllocator::Alloc` 호출 시점에 큰 가상 주소 공간(기본 256GB)을 `call_once`로 한 번만 예약(`MEM_RESERVE`)해 둡니다. "가상 주소 공간 예약"이라는 상대적으로 무거운 커널 작업이 프로세스 생애 동안 단 한 번만 일어나므로, `_STOMP`를 켠 채로 대량의 할당/해제가 반복되는 통합 테스트에서도 예약 자체를 반복하지 않습니다.

**영역 구조 — 메타데이터 1페이지 + 데이터 N페이지**: 각 할당은 `[메타데이터 1페이지][데이터 페이지들]` 형태로 이루어집니다. 데이터는 항상 첫 데이터 페이지 안(페이지 크기 미만의 오프셋)에서 시작해 마지막 데이터 페이지의 끝에서 정확히 끝나므로, `Release`가 데이터 포인터의 페이지 시작 주소를 역산하면 언제나 "첫 데이터 페이지의 시작"이 나오고, 그 바로 앞 페이지가 메타데이터 페이지입니다. 이 성질 덕분에 크기 조회를 위한 별도의 해시맵 조회 없이 포인터 산술만으로 메타데이터를 즉시 찾습니다. 메타데이터 페이지에는 이 영역의 데이터 크기와 이중 반납 탐지용 원자적 플래그(`freed`)가 들어 있으며, 한번 커밋되면 프로세스 종료 시까지 디커밋되지 않습니다(재사용 대기 중에도 유효한 SLIST 링크 노드로 계속 쓰이기 위함) — 디커밋되는 것은 오직 데이터 페이지뿐이라 use-after-free 탐지 능력은 그대로 유지됩니다.

**완전 락프리 재사용**: 디커밋된 영역은 버려지지 않고, 데이터 크기별로 별도의 Lock-free SLIST(`CMemoryPool`과 동일한 방식)에 등록되어 같은 크기의 다음 `Alloc`이 재사용합니다. 메타데이터 페이지가 항상 상주하므로 그 메타데이터 자체를 SLIST 노드로 재사용할 수 있어, 반납/재사용 경로 전체가 락 없이 동작합니다. "지금까지 한 번도 등장한 적 없는 새로운 크기"를 위한 SLIST 헤더를 맵에 처음 등록하는 순간에만 `shared_mutex`의 배타 락을 아주 짧게 잡고(이중 확인 잠금 패턴), 이미 존재하는 크기에 대한 이후의 모든 반복 조회는 공유 락(읽기)만으로 끝납니다. 이 재사용 구조 덕분에, 장시간 실행되는 스트레스 테스트에서도 예약된 아레나가 데이터 크기 종류 수에 비례해서만 소모되고, 반복되는 할당/해제 횟수에는 비례해서 소모되지 않습니다.

- 이중 반납 탐지: `Release`는 메타데이터의 `freed` 플래그를 `exchange(1)`으로 원자적으로 확인/설정합니다. 이미 반납된 상태(1)라면 즉시 `ASSERT_CRASH`로 잡아내며, 이 검사 자체가 락 없이 이루어집니다.
- 주소 재사용과 진단 능력의 트레이드오프: 디커밋된 데이터 페이지를 재사용한다는 것은, 아주 오래전에 반납된 뒤에도 그 주소를 들고 있는 진짜 댕글링 포인터가 그 주소가 다른 할당으로 재사용된 시점 이후에 접근하면 크래시 대신 조용한 오염으로 이어질 수 있다는 뜻입니다. 다만 이는 아레나 없이 `VirtualFree(MEM_RELEASE)`로 완전히 반환하는 방식에서도 OS가 해제된 가상 주소를 이후 다른 `VirtualAlloc` 호출에 재사용할 수 있었던 것과 동일한 성격의 특성입니다.

**사용 시점**: 메모리 손상 버그(오버런, 댕글링 포인터로 인한 오염 등)를 추적할 때만 `_STOMP`를 켜고 재현 → 문제 지점에서 즉시 크래시 → 콜스택 확인 → 원인 수정 후 다시 끔.

### 3.4 PoolAllocator — 실서비스 핫패스

평상시 게임 로직 코드가 사용하는 유일한 경로입니다. `gpMemory`(전역 `CMemory` 싱글턴)에게 위임하며, `xnew`/`xdelete`, `StlAllocator`를 통해 자동으로 이 경로를 타게 됩니다.

`xnew<Type>(args...)`는 생성자 인자를 `std::forward` 대신 `static_cast<Args&&>(args)...`로 직접 전달합니다. 의미상 완전히 동일한 perfect forwarding이지만, 라이브러리 함수 호출 형태 자체를 없애 초당 수백만 번 불릴 수 있는 이 경로에서 인라인 여부를 컴파일러 재량에 맡기지 않습니다.

`StlAllocator<T>`는 표준 Allocator 요구사항(`value_type`, `allocate`, `deallocate`)에 더해, 서로 다른 타입 간 리바인딩 생성자와 대입 연산자도 갖추고 있습니다. 상태가 없는 allocator라 실제로 할 일은 없지만, 일부 STL 구현체(특히 구버전 MSVC)가 이 대입 연산자를 요구하기 때문에 명시적으로 정의해 둔 것입니다.

### 3.5 CMemoryPool — 크기별 Lock-free 프리리스트

하나의 고정 크기(`_allocSize`)만 다루는 free-list입니다.

- **`Push`**: 블록을 "미사용"으로 표시(`allocSize = 0`)한 뒤 `InterlockedPushEntrySList`로 SLIST에 되돌립니다.
- **`Pop`**: SLIST에서 블록을 꺼냅니다. 비어있으면 `RawAllocator::AllocAligned`로 새로 할당합니다. 즉 풀은 "상한 없이 필요할 때마다 늘어나는" 동적 프리리스트입니다.
- `MemoryHeader`가 Windows `SLIST_ENTRY`를 상속하고 있어서, 별도의 `next` 포인터 없이 헤더 자체가 링크드리스트 노드로 재사용됩니다 — 메모리 오버헤드를 최소화하는 설계입니다.
- `_useCount`/`_reserveCount`는 각각 "현재 사용 중", "현재 대기 중"인 블록 수를 추적하는 통계용 원자 카운터입니다.
- **캐시 라인 격리**: 클래스 전체를 64바이트(일반적인 CPU 캐시 라인 크기) 경계에 정렬해, 서로 다른 `CMemoryPool` 인스턴스끼리 캐시 라인을 공유하는 false sharing을 방지합니다. 또한 SLIST 헤더(원자 연산으로 매번 갱신)와 순수 통계 카운터(`_useCount`/`_reserveCount`)를 서로 다른 캐시 라인에 두어, 통계를 읽는 동작이 SLIST Pop/Push의 원자 연산과 캐시 라인을 두고 경합하지 않게 합니다.

**`MemoryHeader::allocSize`가 `atomic<int32>`인 이유**: 이 값은 단순 통계가 아니라 "이 블록이 살아있는지(>0), 반납되었는지(0)"를 판별하는 이중 반납(double free) 탐지 수단입니다. 두 스레드가 극단적인 타이밍에 같은 포인터를 동시에 반납하려는 경쟁 상황에서도 탐지가 놓치지 않으려면 "값을 읽고 0인지 확인한 뒤 0으로 바꾸는" 과정 자체가 원자적이어야 합니다. 일반 `int32`였다면 두 스레드가 동시에 "아직 0이 아님"을 확인하고 둘 다 반납을 진행해버리는 TOCTOU(Time-Of-Check-To-Time-Of-Use) 경쟁이 이론상 가능합니다. `exchange()`로 "읽고 0으로 바꾸기"를 단일 원자 연산으로 묶으면, 두 스레드 중 정확히 하나만 원래 값(>0)을 받고 나머지는 이미 0이 된 값을 받게 되어 이중 반납을 경쟁 상태 없이 감지합니다(`CMemory::Release`, `CObjectPool::Push`에서 이 방식 사용). `atomic<int32>`는 대부분의 플랫폼에서 일반 `int32`와 크기/정렬이 동일하고 lock-free이므로, 헤더 크기나 `SLIST_ALIGNMENT` 요구사항에 영향을 주지 않습니다.

**`MemoryHeader` 생성자가 경쟁 상태 없이 안전한 이유**: `AttachHeader`가 raw 메모리 위에 `MemoryHeader(size)`를 placement new로 얹는 시점에는, 이 헤더를 가리키는 다른 포인터가 아직 어디에도 존재하지 않습니다(방금 `RawAllocator`/`InterlockedPopEntrySList`에서 이 스레드가 단독으로 받아온 블록이기 때문). 따라서 `allocSize` 필드의 최초 초기화 자체는 원자 연산일 필요가 없고, 이 블록이 SLIST에 올라가거나 사용자에게 반환되어 다른 스레드가 접근할 수 있게 된 "이후" 시점부터만 atomic 연산(`exchange` 등)으로 접근이 보호되면 충분합니다.

### 3.6 CMemory — 사이즈별 라우터

"요청 크기 → `_pools` 배열 인덱스"를 테이블 조회가 아니라 `ComputePoolIndex()`의 분기 없는(branchless) 수식 계산으로 O(1)에 구합니다. `MAX_ALLOC_SIZE+1`개 항목짜리 배열로 조회하는 방식도 가능하지만, 이런 크기의 테이블은 다른 코드가 L1/L2 캐시를 심하게 오염시킨 상태에서 참조하면 캐시 미스가 날 수 있습니다. 수식 계산은 애초에 테이블 자체가 없으므로 이 캐시 미스 가능성이 구조적으로 없습니다.

- 32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는 256단위로 총 `POOL_COUNT`개의 풀을 생성합니다.
- 예를 들어 1~32바이트 요청은 모두 32바이트 풀로 라우팅됩니다(내부 단편화를 일부 감수하는 대신 풀 개수를 줄여 관리 비용을 낮춤).
- `MAX_ALLOC_SIZE`(4096바이트)를 초과하는 요청은 풀을 거치지 않고 `RawAllocator::AllocAligned`/`FreeAligned`로 직접 처리합니다.
- `Allocate`/`Release`는 데이터 포인터 앞의 `MemoryHeader`를 attach/detach하며, 헤더의 `allocSize` 값을 보고 "풀 반납 대상인지, raw 해제 대상인지"를 판별합니다.
- `ComputePoolIndex`는 세 구간(32/128/256 단위, 경계 1024/2048) 각각에 대해 "이 구간에 속하는 길이"를 `ClampMin`/`ClampMax`로 0~구간폭 사이에 클램핑한 뒤, 각 구간 단위로 올림한 개수를 모두 더하는 방식입니다. `allocSize`가 실제로 속하지 않는 구간은 클램핑으로 기여분이 자동으로 0이 되므로, `if`/`else`로 어느 구간인지 먼저 판별하는 과정 자체가 없습니다. `ClampMin`/`ClampMax`는 단순 비교-삼항 형태라 Release 최적화 시 조건부 이동(cmov)으로 컴파일되어 실제 분기(jmp)가 발생하지 않으며, 표준 `std::min`/`std::max` 대신 이 이름을 쓰는 이유는 `Windows.h`가 `NOMINMAX` 미정의 시 `min`/`max`를 매크로로 정의해 이름이 충돌할 수 있기 때문입니다. 이 수식은 생성자의 세 단계 구간 생성 로직과 반드시 일치해야 하므로, 생성자가 풀을 만들 때마다 `ComputePoolIndex()`의 결과와 실제 `_pools` 인덱스가 같은지 `ASSERT_CRASH`로 교차 검증합니다.
  - 예) `allocSize=48` → 1구간 길이 `len1=48` → `count1=(48+31)>>5=2` → `poolIndex=2-1=1` (0번 풀=32B, 1번 풀=64B이므로 48B 요청은 64B 풀로 올림 배정됨 — 생성자의 "32단위로 올림 배정"과 동일한 결과)
- `Allocate`는 `size`가 0 이하이거나, `size + sizeof(MemoryHeader)` 계산이 `int32` 범위를 넘어설 정도로 비정상적으로 큰 경우를 `int64` 산술로 먼저 검증한 뒤 `ASSERT_CRASH`로 걸러냅니다. 이 검증이 없으면 오버플로로 `allocSize`가 음수가 되어 풀 인덱스 계산이 잘못된 값을 가리키는 메모리 오염으로 이어질 수 있습니다.
- `Release`는 `header->allocSize`를 `exchange(0)`으로 원자적으로 읽고 표시합니다. 같은 포인터로 `Release`가 동시에 두 번 호출되어도(경쟁 상태 포함) 둘 중 정확히 하나만 원래 값을 받고 나머지는 이미 0이 된 값을 받아 `ASSERT_CRASH`로 즉시 걸립니다.
- **생성자의 구간별 명시적 시작**: 32~1024/1024~2048/2048~4096 세 구간을 만드는 `for`문 세 개가 모두 같은 `size` 변수를 공유합니다. 만약 두 번째·세 번째 구간이 "이전 구간이 끝난 값에서 이어서 시작"했다면(`size += 128`처럼 증가만 하고 재설정하지 않으면), 이전 구간 종료값(예: 1056)에서 그대로 이어받아 시작하게 되어 풀 크기가 32/128/256 단위 경계와 어긋나는 값(1056, 1184, ...)이 생성됩니다. 그래서 두 번째 구간은 항상 `1024 + 128`에서, 세 번째 구간은 항상 `2048 + 256`에서 명시적으로 다시 시작합니다. 이 경계 어긋남이 다시 발생하더라도 곧바로 드러나도록, 풀을 생성할 때마다 그 크기에 대한 `ComputePoolIndex()` 결과가 실제 `_pools` 인덱스와 일치하는지 `ASSERT_CRASH`로 즉시 검증합니다.


### 3.7 CObjectPool — 타입 전용 오브젝트 풀

`gpMemory`의 크기 구간별 공유 풀과 달리, `Type`마다 정확히 `sizeof(Type)` 크기의 `CMemoryPool`을 독점적으로 갖는 템플릿입니다. 특정 타입이 압도적으로 많이 생성/파괴되어 그 타입 전용 풀로 분리하는 것이 유리할 때 `xnew`/`xdelete` 대신 사용합니다.

- **이중 반납 방어**: `Push()`는 소멸자를 호출하기 전에 먼저 헤더의 `allocSize`가 0이 아닌지 확인합니다. `CMemoryPool::Push`는 `allocSize`를 무조건 0으로 덮어쓰기만 할 뿐 이미 0인지 확인하지 않으므로, 같은 포인터가 두 번 `Push`되면 SLIST 노드가 자기 자신을 가리키는 순환 구조로 꼬여 서로 다른 두 번의 `Pop()`이 같은 메모리 블록을 동시에 소유하는 조용한 오염으로 이어질 수 있습니다. 검증을 먼저 하고 소멸자를 나중에 호출하는 순서 덕분에, 이미 파괴된 객체의 소멸자가 다시 호출되는 이중 소멸까지 함께 방지됩니다.
- **타입 전용 풀의 지연 초기화**: `CMemoryPool` 인스턴스는 클래스 정적 멤버가 아니라 함수 지역 정적 변수(`GetPool()` 내부의 Meyer's Singleton)로 관리됩니다. C++11부터 함수 지역 정적 변수의 동적 초기화는 최초 사용 시점에 정확히 한 번, 스레드 안전하게 이루어짐이 표준으로 보장되므로, 다른 번역 단위의 전역/정적 객체 생성자에서 이 풀을 먼저 참조하더라도 초기화 순서 문제가 발생하지 않습니다.
- **`_STOMP` 연동**: `_STOMP` 빌드에서는 풀을 거치지 않고 `StompAllocator`로 대체되므로, `StompAllocator`의 아레나 최적화(3.3절)를 자동으로 함께 사용합니다.
- **TLS 배치 캐시 없음**: `CMemory`와 달리 스레드 로컬 배치 캐시가 없어, `Pop()`/`Push()`가 매번 `CMemoryPool`의 원자 연산(`InterlockedPop/PushEntrySList`)을 직접 호출합니다. 이 풀을 쓰는 타입이 초당 수만~수십만 번 생성/파괴되는 극단적 핫패스라면 멀티코어 경합이 그대로 남아있다는 뜻입니다. 이는 결함이 아니라 의도적으로 단순하게 유지한 설계이며, 실제 사용 타입의 생성 빈도가 그 정도로 높아지면 그때 `CMemory`와 동일한 TLS 배치 캐시를 얹는 것을 고려할 수 있습니다.
- **`MakeShared`는 이 타입 전용 풀을 쓰지 않음**: `Pop`/`Push`가 `{ Pop(...), Push }` 형태의 커스텀 삭제자로 `shared_ptr`을 만들면, 참조 카운트 제어 블록을 위한 힙 할당이 객체 할당과 별도로(총 2회) 발생합니다. `MakeShared`는 대신 `std::allocate_shared<Type>(StlAllocator<Type>(), args...)`를 사용해 객체와 제어 블록을 하나의 블록으로 묶어 단 한 번만 할당합니다. 다만 이 결합된 블록의 크기는 `sizeof(Type)`이 아니라 (제어 블록 + `Type`)이며 표준 라이브러리 구현마다 다르므로, 이 타입 전용의 고정 크기 풀(`GetPool()`)에는 그대로 흘려보낼 수 없습니다(풀의 고정 블록 크기를 넘어서는 버퍼 오버플로우가 될 수 있음). 그래서 `StlAllocator<Type>`을 통해 `gpMemory`의 크기별 공유 풀이 실제 필요한 크기에 맞는 풀을 알아서 찾도록 합니다. 즉 `Pop()`/`Push()`로 만든 객체는 이 타입 전용 풀을, `MakeShared()`로 만든 객체는 `gpMemory`의 공유 풀을 사용하는 것으로 역할이 나뉩니다.

## 4. 실행 흐름 예시

### 4.1 일반적인 객체 생성/파괴

```cpp
Player* p = xnew<Player>();   // PoolAllocator::Alloc → gpMemory->Allocate
...
xdelete(p);                    // 소멸자 호출 → PoolAllocator::Release → gpMemory->Release
```

1. `xnew`가 `PoolAllocator::Alloc(sizeof(Player))` 호출
2. `PoolAllocator`가 `gpMemory->Allocate(size)`로 위임
3. `CMemory::Allocate`가 `allocSize`를 계산해 `ComputePoolIndex(allocSize)`로 담당 풀 인덱스를 찾음
4. 해당 `CMemoryPool::Pop()` — SLIST에 여유 블록이 있으면 즉시 반환, 없으면 `RawAllocator`로 신규 할당
5. `MemoryHeader::AttachHeader`로 헤더를 얹고 데이터 포인터 반환

### 4.2 디버깅 시 (오버런 의심)

빌드 설정에서 `_STOMP` 매크로를 정의하면, 4번 단계가 `StompAllocator::Alloc`으로 완전히 대체되어 풀을 거치지 않고 페이지 가드 방식으로 동작합니다.

## 5. raw 할당 라이브러리 교체 방법 (mimalloc 예시)

`RawAllocator`는 헤더 include 여부로 라이브러리를 감지하므로, mimalloc을 실제로 사용하려면:

```cpp
// RawAllocator.h를 include하는 지점(혹은 pch.h)보다 먼저
#include <mimalloc.h>   // 이 include로 MIMALLOC_H 매크로가 정의되어 분기가 활성화됨
#include "RawAllocator.h"
```

그리고 vcpkg/NuGet 등으로 mimalloc 라이브러리를 설치하고 링커 설정(`mimalloc.lib`)을 맞춰야 합니다. 이 매크로 하나로 `BaseAllocator`, `CMemoryPool`, `CMemory`의 대형 할당 경로가 전부 mimalloc으로 전환됩니다.

## 5-1. 스레드 로컬(TLS) 캐시

### 왜 필요한가

`CMemoryPool::Push`/`Pop`은 매 호출마다 `InterlockedPushEntrySList`/`InterlockedPopEntrySList`(원자적 CAS)를 수행합니다. Lock-free라 데드락은 없지만, 여러 코어가 동시에 같은 SLIST 헤더를 두드리면 그 캐시라인을 서로 뺏고 뺏기는 현상(cache line ping-pong)이 발생해 원자 연산 자체가 느려집니다. 멀티스레드 서버에서 초당 수만 건의 `Allocate`/`Release`가 발생하면 이 경합이 실제 병목이 됩니다.

### 어떻게 해결하는가

`CMemory`가 스레드마다 독립적인 로컬 free-list(`TlsBucket`)를 두고, 대부분의 요청을 이 로컬 캐시에서 처리합니다.

- **`Allocate`**: 로컬 캐시가 비어 있을 때만 전역 `CMemoryPool`에서 이 크기 구간에 맞는 배치 개수만큼 한 번에 당겨와 로컬을 채웁니다. 이후 그만큼의 `Allocate`는 전역 풀을 전혀 건드리지 않고 로컬에서 처리됩니다.
- **`Release`**: 블록을 로컬 캐시에 반납만 합니다. 로컬 캐시가 이 크기 구간의 상한을 넘으면 절반을 전역 풀에 배치로 돌려줍니다.
- 결과적으로 원자 연산 호출 빈도가 배치 개수에 반비례해 크게 줄어듭니다.

**배치 크기는 풀마다 균일하지 않고 블록 크기 구간별로 차등 적용됩니다.**

| 블록 크기(헤더 포함) | 배치 충전 개수 | 로컬 캐시 상한 | 의도 |
|---|---|---|---|
| ~128B 이하 | 64 | 256 | 소형/고빈도 구간 — 원자 연산 절감 최우선 |
| ~1024B 이하 | 32 | 128 | 중형 구간 — 절충 |
| 1024B 초과 | 4 | 16 | 대형/저빈도 구간 — 상주 메모리 절약 우선 |

작고 자주 쓰이는 크기(32B, 64B 등)는 배치를 크게 잡아 경합을 최대한 줄이고, 크고 드물게 쓰이는 크기(2KB, 4KB 등)는 배치를 작게 잡아 스레드마다 불필요하게 큰 메모리가 상주하는 것을 막습니다. `CMemory::DetermineTlsBatchSize`/`DetermineTlsMaxCount`가 생성자에서 미리 계산해 `_tlsBatchSizeTable`/`_tlsMaxCountTable`에 저장해 두므로, `Allocate`/`Release` 핫패스는 조건 분기 없이 배열 조회만으로 즉시 사용합니다.

### 구현상 핵심 포인트

- **`MemoryHeader`가 이미 `SLIST_ENTRY`(= `Next` 포인터 하나)를 상속**하고 있어, 이 필드를 그대로 재사용해 로컬 free-list를 연결했습니다. 그 결과 `MemoryHeader`, `CMemoryPool` 두 파일은 전혀 수정하지 않고 `CMemory` 계층에만 캐시를 얹을 수 있었습니다.
- **`ComputePoolIndex`**: 크기 → `_pools` 배열 인덱스를 테이블 없이 수식으로 계산합니다. TLS 캐시(`_tlsCache.buckets[poolIndex]`)를 배열로 바로 인덱싱하기 위한 용도이며, 이 인덱스 하나로 풀 포인터(`_pools[poolIndex]`)와 TLS 버킷(`_tlsCache.buckets[poolIndex]`), 배치/상한 테이블(`_tlsBatchSizeTable[poolIndex]`)까지 모두 조회할 수 있습니다.
- **`thread_local TlsCache` 소멸자**: 스레드가 종료되면 MSVC가 자동으로 `TlsCache`의 소멸자를 호출합니다. 이 소멸자에서 로컬에 남은 모든 블록을 원래 속했던 전역 `CMemoryPool`로 되돌립니다. 이 처리가 없으면 스레드가 죽을 때마다 로컬 캐시에 있던 메모리가 다른 스레드에서 재사용되지 못하고 사실상 누수처럼 방치됩니다.
- **`TlsCache`는 `CMemory`의 nested class**이므로 C++11부터 `CMemory`의 private 멤버(`_pools`)에 별도의 `friend` 선언 없이 접근할 수 있습니다.
- **`DrainBuckets`의 `gpMemory` null 체크**: 정해진 파괴 순서 규약을 지키지 않는 스레드(뒤에서 설명할 "그 외 스레드")가 프로세스 종료 시점 근처에 뒤늦게 종료되어 `gpMemory`가 이미 파괴된 뒤 `TlsCache` 소멸자가 호출되는 극단적인 상황을 대비해, `gpMemory`가 `nullptr`이면 반납을 시도하지 않고 조용히 반환합니다. 이 경우 남은 블록은 정상 반납되지 못하지만(누수), 프로세스 종료 시점이므로 OS가 결국 회수하며, use-after-free로 크래시가 나는 것보다 안전합니다.
- **`TlsBucket`의 정렬(`alignas(16)`)**: `TlsBucket`은 `thread_local`이라 스레드 간 false sharing은 원천적으로 없지만, 한 스레드가 `DrainBuckets` 등에서 `buckets[POOL_COUNT]` 배열을 순회할 때의 캐시 지역성은 별개 문제입니다. `sizeof(TlsBucket)`은 16바이트로 캐시 라인(64바이트)의 정확한 약수지만, 포인터 멤버 때문에 자연 정렬이 8바이트뿐이라 배열 시작 주소가 16바이트로 정렬되지 않으면 4번째 원소마다 캐시 라인 경계를 걸치게 됩니다. `alignas(16)`을 붙이면 크기가 이미 16의 배수라 패딩 없이(메모리 낭비 없이) 모든 원소가 항상 하나의 캐시 라인 안에 완전히 들어가도록 보장됩니다.

### 워밍업(Warm-up)

`CMemoryPool::Pop()`은 풀이 비어 있을 때만 `RawAllocator`로 새로 raw 할당을 받아옵니다. 즉 서버 구동 직후 동시 접속이 몰리는 시점에는, 자주 쓰이는 크기의 블록들이 아직 한 번도 채워진 적이 없어 그 시점에 몰아서 raw 할당이 발생하며 짧은 지연 스파이크가 생길 수 있습니다.

`CMemory::WarmUp(allocDataSize, count)`를 호출하면 지정한 크기의 풀에 raw 블록을 `count`개 미리 만들어 채워 넣습니다. 서버 시작 시점에 실제로 자주 생성되는 주요 크기(예: 패킷 헤더, 세션 객체 등)만 선별적으로 호출하는 것을 권장합니다. 모든 풀을 일괄적으로 채우면 시작 시점에 쓰이지도 않을 메모리를 크게 낭비할 수 있으므로, 워밍업은 항상 실측 기반으로 대상을 좁혀서 적용해야 합니다.

## 5-2. 전역 시스템 파괴 순서 (BaseGlobal / ThreadManager 연동)

`thread_local TlsCache`는 스레드 종료 시 전역 `CMemory`(`gpMemory`)의 풀을 참조합니다. 따라서 전역 시스템들의 생성/파괴 순서가 정확해야 안전합니다.

- **워커 스레드 경로**: `CThreadManager`의 소멸자가 `JoinThreads()`를 호출 → 각 워커 스레드가 종료되며 `thread_local TlsCache` 소멸자가 자동 실행되어 `gpMemory`를 참조합니다. 이 때문에 `gpThreadManager`는 항상 `gpMemory`보다 먼저 파괴되어야 합니다.
- **메인 스레드 경로**: `CThreadManager`는 워커 스레드만 관리하고 메인 스레드는 아무도 join하지 않습니다. 메인 스레드도 자신만의 `TlsCache` 인스턴스를 가지고 있는데, 이건 `main()`이 완전히 return한 뒤에야 소멸됩니다. 그래서 `gpMemory`를 delete하기 직전, 메인 스레드에서 `CMemory::FlushCurrentThreadCache()`를 명시적으로 호출해 로컬 캐시를 먼저 비웁니다.
- **그 외 스레드 경로**: 서드파티 라이브러리 콜백 스레드 등 `CThreadManager`를 거치지 않고 만들어진 스레드가 `PoolAllocator`(`xnew`/`StlAllocator` 등)를 사용한다면, 메인 스레드와 동일한 규칙이 적용됩니다 — 그 스레드도 종료 전에 `CMemory::FlushCurrentThreadCache()`를 직접 호출해야 하고, 호출 시점까지 `gpMemory`가 살아있어야 합니다. 이 규칙을 지킬 수 없는 스레드라면 `DrainBuckets()`가 `gpMemory`의 `nullptr` 여부를 확인해 최소한 크래시 대신 누수로 완화되도록 방어되어 있습니다.

이를 위해 세 가지 장치가 맞물려 동작합니다.

**1) 파괴 순서 — 생성의 역순, `CMemory`는 항상 마지막**

`BaseGlobal::Destroy()`는 `gpThreadManager`를 `gpMemory`보다 먼저 파괴합니다. `Init()`은 `gpMemory`를 가장 먼저 만듭니다.

**2) `CMemory::FlushCurrentThreadCache()` — 메인 스레드용 수동 반납 진입점**

`thread_local` 소멸자는 "그 스레드가 실제로 종료될 때"만 호출되므로, 메인 스레드처럼 언제 종료될지 보장할 수 없는 스레드를 위해 동일한 반납 로직을 명시적으로 호출 가능한 정적 함수로 노출해 둡니다. `BaseGlobal::Destroy()`가 `gpMemory`를 delete하기 직전 이 함수를 호출합니다.

**3) `CThreadManager::DestroyTLS()`에서 명시적 flush — 이중 안전장치**

워커 스레드는 자연 종료 시 `TlsCache` 소멸자가 자동으로 캐시를 비워주지만, `DestroyTLS()`(콜백 실행 직후, 스레드 종료 직전 훅)에서도 `CMemory::FlushCurrentThreadCache()`를 명시적으로 한 번 더 호출합니다. 이렇게 하면:
- 캐시 반납 시점이 "콜백이 끝나는 순간"으로 코드상 명확히 고정되고,
- 여러 `thread_local` 객체 간의 암묵적인 소멸 순서에 대한 의존을 줄일 수 있습니다.
- `CMemory` 모듈이 포함되지 않은 빌드에서도 `ThreadManager`가 독립적으로 컴파일되도록 `#ifdef __MEMORY_H__` 가드로 감싸져 있습니다.

### 최종 파괴 순서

```
1. gpThreadManager 파괴  (워커 스레드 join → 각 워커의 TlsCache 자동 반납)
2. gpDeadLockProfiler / gpJobTimer / gpGlobalQueue 파괴
3. CMemory::FlushCurrentThreadCache()  (메인 스레드 캐시 수동 반납)
4. gpMemory 파괴  (반드시 마지막)
```

### 전제조건 (코드로 자동 보장되지 않는 부분)

- `BaseGlobal::Init()`/`Destroy()`는 **항상 메인 스레드에서, 그리고 각각 한 번씩만** 호출된다는 전제 위에서 동작합니다. 다른 스레드에서 `Destroy()`를 호출하면 `FlushCurrentThreadCache()`가 엉뚱한 스레드의 캐시를 비우게 되어 의미가 없어집니다.
- 비정상 종료 경로(크래시, 강제 종료 시그널, `TerminateThread`)는 `Destroy()`를 거치지 않으므로 이 보장의 적용을 받지 않습니다. 다만 이 경우 프로세스 자체가 종료되며 OS가 메모리를 일괄 회수하므로 실무적 영향은 제한적입니다.
- 모든 파일의 클래스명은 `CMemory`/`CMemoryPool`로 통일되어 있습니다(`gpMemory` 타입도 `CMemory*`). 파일명 자체는 프로젝트 관례(`ThreadManager.h` → `CThreadManager`처럼 클래스만 `C` 접두사를 붙이고 파일명은 접두사 없이 유지)에 맞춰 `Memory.h/cpp`, `MemoryPool.h/cpp`를 그대로 사용합니다.

## 6. 설계상 트레이드오프 및 유의점

| 항목 | 내용 |
|---|---|
| 내부 단편화 | 크기 구간을 단위로 묶어서 관리하므로, 요청 크기가 구간의 하한에 가까울수록 낭비되는 바이트가 생김 |
| 풀 무제한 성장 | `CMemoryPool::Pop()`이 비었을 때 상한 없이 계속 새로 할당하므로, 특정 시점에 몰린 대량 요청 이후 반납되지 않으면 풀 크기가 계속 커질 수 있음(트리밍 로직 없음) |
| `_STOMP` 오작동 위험 | 릴리즈 빌드에 실수로 `_STOMP`가 정의되면 성능이 크게 저하되므로 빌드 구성 관리 필요 |
| OOM 미처리 | `RawAllocator`가 반환하는 `nullptr`에 대해 `ASSERT_CRASH`로 방어하고 있으나, 이는 "조기에 알아채기" 목적이지 복구 로직은 아님 — 크래시로 이어짐 |
| 정렬 요구사항 | 풀 경로의 모든 raw 할당은 `SLIST_ALIGNMENT(16바이트)`로 정렬되어 SIMD 등 정렬이 필요한 데이터에도 안전. `BaseAllocator` 상속 경로도 C++17 확장 정렬 오버로드(`operator new(size_t, std::align_val_t)`)를 제공하므로, `alignas(32)` 이상(AVX 대상 등)을 요구하는 클래스가 상속받아도 `RawAllocator::AllocAligned`로 정렬이 보장됨(3.2절 참고) |
| TLS 캐시 상주 메모리 | 스레드마다 최대 `Σ(각 풀의 TLS 상한)`개 블록이 로컬에 상주할 수 있음. 크기 구간별로 배치/상한을 차등화(5-1절)해 균등 적용 대비 상주량을 낮췄지만, 스레드 수가 많은 서버에서는 여전히 전체 상주 메모리량이 늘어날 수 있음 |
| 정적 소멸 순서 문제 | `thread_local TlsCache`의 소멸자가 `gpMemory->_pools`를 참조하므로 전역 시스템 파괴 순서가 정확해야 함. `BaseGlobal::Destroy()`의 파괴 순서 + `CMemory::FlushCurrentThreadCache()` + `ThreadManager::DestroyTLS()`의 명시적 flush로 안전하게 관리됨(5-2절 참고). `CThreadManager` 밖에서 만들어진 스레드가 `PoolAllocator`를 쓰는 경우 동일한 규칙을 수동으로 지켜야 하며, 지키지 못하더라도 `DrainBuckets`의 `gpMemory` null 체크가 크래시를 누수로 완화함 |
| 입력 검증 범위 | `Allocate(size)`는 `size <= 0`이거나 오버플로가 발생할 정도로 큰 `size`를 `ASSERT_CRASH`로 걸러내지만, 이는 명백히 잘못된 호출부 버그를 조기에 드러내기 위한 것이지 임의의 악의적 입력을 견디는 방어 계층은 아님 |

## 7. 사용 가이드 요약

- **평소 코드**: `xnew`/`xdelete`, `MakeShared`, `StlAllocator`만 사용 — 내부적으로 전부 `PoolAllocator` 경로를 탐.
- **STL 컨테이너가 필요할 때**: `std::vector` 등을 직접 쓰지 않고 `Containers.h`의 `CVector`/`CList`/`CMap` 등을 사용 — 기본 할당자가 이미 `StlAllocator`로 연결되어 있어 풀 경로를 자동으로 탐(8절 참고).
- **메모리 손상 디버깅**: 빌드 설정에서 `_STOMP` 정의 → 재현 → 크래시 지점의 콜스택으로 원인 추적 → 수정 후 다시 끔.
- **raw 할당 라이브러리 변경**: `RawAllocator.h`를 건드릴 필요 없이, 원하는 라이브러리 헤더만 먼저 include하면 매크로 감지로 자동 전환됨.
- **서버 시작 시점 지연 스파이크 완화**: 자주 쓰이는 주요 크기에 한해 `CMemory::WarmUp(allocDataSize, count)`를 선별적으로 호출.
- **`CThreadManager` 밖에서 만든 스레드가 `PoolAllocator`를 쓸 경우**: 그 스레드 종료 직전 반드시 `CMemory::FlushCurrentThreadCache()`를 직접 호출.

## 8. Containers.h — STL 컨테이너 래퍼

표준 STL 컨테이너(`std::vector`, `std::map` 등)를 그대로 쓰면 기본 할당자가 전역 힙(`new`/`delete` 경유)이라 이 프로젝트의 풀 시스템을 타지 않습니다. `Containers.h`는 각 표준 컨테이너를 상속한 얇은 래퍼 클래스(`CVector`, `CList`, `CForwardList`, `CDeque`, `CQueue`, `CPriorityQueue`, `CStack`, `CSet`, `CMap`, `CMultiMap`, `CUnorderedSet`, `CUnorderedMap`)를 제공하며, 기본 템플릿 인자로 `StlAllocator<T>`를 지정해 둡니다. 그래서 `std::vector<T>` 대신 `CVector<T>`를 쓰기만 하면 내부 노드/버퍼 할당이 자동으로 `PoolAllocator` → `gpMemory`의 풀 경로를 타게 됩니다.

**구현 방식**: 각 래퍼는 대응하는 표준 컨테이너를 `public` 상속하고, `using Base::Base;`로 생성자만 그대로 끌어옵니다. 컨테이너 어댑터(`CQueue`, `CStack`)는 기본 내부 컨테이너로 `CDeque`(이미 `StlAllocator`가 연결된 래퍼)를 사용해, 어댑터 자신뿐 아니라 내부 컨테이너까지 풀 경로를 타도록 이어줍니다. `CPriorityQueue`만 클래스가 아니라 `using` 별칭인 이유는, `std::priority_queue`는 상속받아 얇게 감싸더라도 생성자 상속으로 얻을 실익이 적고(내부적으로 `make_heap` 등 별도 초기화가 필요해 단순 `using Base::Base`로는 어댑터들만큼 자연스럽게 감싸지지 않음), 별칭만으로 충분하기 때문입니다.

**사용법**: 각 클래스는 대응하는 `std::` 컨테이너와 사용법이 완전히 동일합니다(반복자, 멤버 함수 등 전부 상속됨). 커스텀 할당자가 필요 없는 일반적인 경우 템플릿 인자를 생략하면 자동으로 `StlAllocator`가 적용됩니다.

```cpp
CVector<int> v;              // std::vector<int, StlAllocator<int>>와 동일하게 동작
CMap<int, Player*> players;  // std::map<int, Player*, less<int>, StlAllocator<pair<const int, Player*>>>
```


