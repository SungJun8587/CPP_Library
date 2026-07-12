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
        │                              │ 크기별 O(1) 라우팅 (_poolIndexTable)
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

**정렬(alignment) 관련 주의**: `operator new`가 호출하는 `RawAllocator::Alloc`은 정렬을 보장하지 않는 일반 할당 함수입니다. 64비트 환경에서는 표준 `malloc`/`mi_malloc` 등이 보통 16바이트 정렬을 기본 보장하므로 실질적인 문제는 거의 없지만, 이보다 더 큰 정렬 요구사항(`alignas` 등)을 가진 클래스가 상속받는다면 `RawAllocator::AllocAligned`를 쓰는 별도 오버로드가 필요합니다.

### 3.3 StompAllocator — 디버그 전용 오버런 탐지

메모리 오버런(버퍼 오버플로우)을 즉시 크래시로 잡아내는 진단 도구입니다.

- 요청 크기를 담을 페이지 수를 계산하고, `VirtualAlloc`으로 그 페이지들을 예약/커밋합니다.
- 데이터를 페이지의 **끝**에 딱 맞춰 배치합니다. 이렇게 하면 할당 크기를 1바이트라도 초과해서 쓰는 순간 다음 미매핑 페이지에 접근해 그 자리에서 Access Violation이 발생합니다.
- `CMemory::Allocate`/`Release` 내부의 `_STOMP` 매크로 분기로만 활성화되며, 평소 빌드에서는 꺼져 있어야 합니다(할당마다 최소 페이지 하나(4KB)를 소모하므로 매우 비효율적).
- 페이지 단위 가드 자체가 목적이므로 `RawAllocator`로 대체하지 않고 `VirtualAlloc`을 직접 사용합니다.

**사용 시점**: 메모리 손상 버그(오버런, 댕글링 포인터로 인한 오염 등)를 추적할 때만 `_STOMP`를 켜고 재현 → 문제 지점에서 즉시 크래시 → 콜스택 확인 → 원인 수정 후 다시 끔.

### 3.4 PoolAllocator — 실서비스 핫패스

평상시 게임 로직 코드가 사용하는 유일한 경로입니다. `gpMemory`(전역 `CMemory` 싱글턴)에게 위임하며, `xnew`/`xdelete`, `StlAllocator`를 통해 자동으로 이 경로를 타게 됩니다.

### 3.5 CMemoryPool — 크기별 Lock-free 프리리스트

하나의 고정 크기(`_allocSize`)만 다루는 free-list입니다.

- **`Push`**: 블록을 "미사용"으로 표시(`allocSize = 0`)한 뒤 `InterlockedPushEntrySList`로 SLIST에 되돌립니다.
- **`Pop`**: SLIST에서 블록을 꺼냅니다. 비어있으면 `RawAllocator::AllocAligned`로 새로 할당합니다. 즉 풀은 "상한 없이 필요할 때마다 늘어나는" 동적 프리리스트입니다.
- `MemoryHeader`가 Windows `SLIST_ENTRY`를 상속하고 있어서, 별도의 `next` 포인터 없이 헤더 자체가 링크드리스트 노드로 재사용됩니다 — 메모리 오버헤드를 최소화하는 설계입니다.
- `_useCount`/`_reserveCount`는 각각 "현재 사용 중", "현재 대기 중"인 블록 수를 추적하는 통계용 원자 카운터입니다.
- **캐시 라인 격리**: 클래스 전체를 64바이트(일반적인 CPU 캐시 라인 크기) 경계에 정렬해, 서로 다른 `CMemoryPool` 인스턴스끼리 캐시 라인을 공유하는 false sharing을 방지합니다. 또한 SLIST 헤더(원자 연산으로 매번 갱신)와 순수 통계 카운터(`_useCount`/`_reserveCount`)를 서로 다른 캐시 라인에 두어, 통계를 읽는 동작이 SLIST Pop/Push의 원자 연산과 캐시 라인을 두고 경합하지 않게 합니다.

### 3.6 CMemory — 사이즈별 라우터

`_poolIndexTable[MAX_ALLOC_SIZE + 1]` 배열을 이용해 "요청 크기 → `_pools` 배열 인덱스"를 O(1)로 찾습니다.

- 32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는 256단위로 총 `POOL_COUNT`개의 풀을 생성합니다.
- 예를 들어 1~32바이트 요청은 모두 32바이트 풀로 라우팅됩니다(내부 단편화를 일부 감수하는 대신 풀 개수를 줄여 관리 비용을 낮춤).
- `MAX_ALLOC_SIZE`(4096바이트)를 초과하는 요청은 풀을 거치지 않고 `RawAllocator::AllocAligned`/`FreeAligned`로 직접 처리합니다.
- `Allocate`/`Release`는 데이터 포인터 앞의 `MemoryHeader`를 attach/detach하며, 헤더의 `allocSize` 값을 보고 "풀 반납 대상인지, raw 해제 대상인지"를 판별합니다.
- `_poolIndexTable`은 `int16`으로 저장합니다. `POOL_COUNT`는 수십 개 수준이라 `int16` 범위로 충분하고, `int32` 대비 테이블 크기를 절반으로 줄여 조회 시 캐시 적중률을 높입니다.
- `Allocate`는 `size`가 0 이하이거나, `size + sizeof(MemoryHeader)` 계산이 `int32` 범위를 넘어설 정도로 비정상적으로 큰 경우를 `int64` 산술로 먼저 검증한 뒤 `ASSERT_CRASH`로 걸러냅니다. 이 검증이 없으면 오버플로로 `allocSize`가 음수가 되어 `_poolIndexTable`을 잘못된 인덱스로 참조하는 메모리 오염으로 이어질 수 있습니다.

## 4. 실행 흐름 예시

### 4.1 일반적인 객체 생성/파괴

```cpp
Player* p = xnew<Player>();   // PoolAllocator::Alloc → gpMemory->Allocate
...
xdelete(p);                    // 소멸자 호출 → PoolAllocator::Release → gpMemory->Release
```

1. `xnew`가 `PoolAllocator::Alloc(sizeof(Player))` 호출
2. `PoolAllocator`가 `gpMemory->Allocate(size)`로 위임
3. `CMemory::Allocate`가 `allocSize`를 계산해 `_poolIndexTable[allocSize]`로 담당 풀 인덱스를 찾음
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
- **`_poolIndexTable`**: 크기 → `_pools` 배열 인덱스를 저장하는 테이블입니다. TLS 캐시(`_tlsCache.buckets[poolIndex]`)를 배열로 바로 인덱싱하기 위한 용도이며, 이 인덱스 하나로 풀 포인터(`_pools[poolIndex]`)와 TLS 버킷(`_tlsCache.buckets[poolIndex]`), 배치/상한 테이블(`_tlsBatchSizeTable[poolIndex]`)까지 모두 조회할 수 있습니다.
- **`thread_local TlsCache` 소멸자**: 스레드가 종료되면 MSVC가 자동으로 `TlsCache`의 소멸자를 호출합니다. 이 소멸자에서 로컬에 남은 모든 블록을 원래 속했던 전역 `CMemoryPool`로 되돌립니다. 이 처리가 없으면 스레드가 죽을 때마다 로컬 캐시에 있던 메모리가 다른 스레드에서 재사용되지 못하고 사실상 누수처럼 방치됩니다.
- **`TlsCache`는 `CMemory`의 nested class**이므로 C++11부터 `CMemory`의 private 멤버(`_pools`)에 별도의 `friend` 선언 없이 접근할 수 있습니다.
- **`DrainBuckets`의 `gpMemory` null 체크**: 정해진 파괴 순서 규약을 지키지 않는 스레드(뒤에서 설명할 "그 외 스레드")가 프로세스 종료 시점 근처에 뒤늦게 종료되어 `gpMemory`가 이미 파괴된 뒤 `TlsCache` 소멸자가 호출되는 극단적인 상황을 대비해, `gpMemory`가 `nullptr`이면 반납을 시도하지 않고 조용히 반환합니다. 이 경우 남은 블록은 정상 반납되지 못하지만(누수), 프로세스 종료 시점이므로 OS가 결국 회수하며, use-after-free로 크래시가 나는 것보다 안전합니다.

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
| 정렬 요구사항 | 풀 경로의 모든 raw 할당은 `SLIST_ALIGNMENT(16바이트)`로 정렬되어 SIMD 등 정렬이 필요한 데이터에도 안전. 다만 `BaseAllocator` 상속 경로(`operator new`)는 정렬을 보장하지 않는 일반 `Alloc`을 쓰므로, 16바이트보다 큰 정렬(`alignas(32)` 등 AVX 대상)을 요구하는 클래스가 상속받을 경우 별도의 `AllocAligned` 기반 오버로드가 필요함(3.2절 참고) |
| TLS 캐시 상주 메모리 | 스레드마다 최대 `Σ(각 풀의 TLS 상한)`개 블록이 로컬에 상주할 수 있음. 크기 구간별로 배치/상한을 차등화(5-1절)해 균등 적용 대비 상주량을 낮췄지만, 스레드 수가 많은 서버에서는 여전히 전체 상주 메모리량이 늘어날 수 있음 |
| 정적 소멸 순서 문제 | `thread_local TlsCache`의 소멸자가 `gpMemory->_pools`를 참조하므로 전역 시스템 파괴 순서가 정확해야 함. `BaseGlobal::Destroy()`의 파괴 순서 + `CMemory::FlushCurrentThreadCache()` + `ThreadManager::DestroyTLS()`의 명시적 flush로 안전하게 관리됨(5-2절 참고). `CThreadManager` 밖에서 만들어진 스레드가 `PoolAllocator`를 쓰는 경우 동일한 규칙을 수동으로 지켜야 하며, 지키지 못하더라도 `DrainBuckets`의 `gpMemory` null 체크가 크래시를 누수로 완화함 |
| 입력 검증 범위 | `Allocate(size)`는 `size <= 0`이거나 오버플로가 발생할 정도로 큰 `size`를 `ASSERT_CRASH`로 걸러내지만, 이는 명백히 잘못된 호출부 버그를 조기에 드러내기 위한 것이지 임의의 악의적 입력을 견디는 방어 계층은 아님 |

## 7. 사용 가이드 요약

- **평소 코드**: `xnew`/`xdelete`, `MakeShared`, `StlAllocator`만 사용 — 내부적으로 전부 `PoolAllocator` 경로를 탐.
- **메모리 손상 디버깅**: 빌드 설정에서 `_STOMP` 정의 → 재현 → 크래시 지점의 콜스택으로 원인 추적 → 수정 후 다시 끔.
- **raw 할당 라이브러리 변경**: `RawAllocator.h`를 건드릴 필요 없이, 원하는 라이브러리 헤더만 먼저 include하면 매크로 감지로 자동 전환됨.
- **서버 시작 시점 지연 스파이크 완화**: 자주 쓰이는 주요 크기에 한해 `CMemory::WarmUp(allocDataSize, count)`를 선별적으로 호출.
- **`CThreadManager` 밖에서 만든 스레드가 `PoolAllocator`를 쓸 경우**: 그 스레드 종료 직전 반드시 `CMemory::FlushCurrentThreadCache()`를 직접 호출.

