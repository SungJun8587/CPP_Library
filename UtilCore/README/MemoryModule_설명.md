# 메모리 모듈 설계 문서

## 1. 개요

이 모듈은 게임 서버처럼 고빈도 할당/해제가 발생하는 환경에서 OS 기본 힙(`malloc`/`new`)의 성능·경합 문제를 회피하기 위한 커스텀 메모리 관리 계층입니다.

핵심 아이디어는 세 가지입니다.

① **풀링(Pooling)** — 크기별로 미리 만들어둔 메모리 풀에서 블록을 재사용해 `malloc`/`free` 호출 자체를 줄인다.

② **Lock-free** — 멀티스레드 환경에서 풀 접근 시 뮤텍스 없이 Windows `SLIST`(Interlocked Singly Linked List)로 경합을 최소화한다.

③ **전략 교체 가능성(Strategy 패턴)** — 실서비스용 풀 할당자, 디버그용 오버런 탐지 할당자, 그리고 실제 raw 메모리 소스(mimalloc 등)를 상황에 따라 갈아 끼울 수 있게 계층을 분리한다.

## 2. 전체 구조

```
사용자 코드 (게임 로직, 패킷, 세션 등)
        │
        │  xnew<T>() / xdelete() / MakeShared<T>() / StlAllocator<T>
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
        │              ┌───────────────┼───────────────┬───────────────┐
        │              ▼               ▼               ▼               ▼
        │         mimalloc         jemalloc         tcmalloc      malloc (fallback)
        │
        └── (부트스트랩 전용) BaseAllocator ──► RawAllocator

디버그 빌드(_STOMP): CMemory::Allocate/Release가 StompAllocator로 대체됨
                     (VirtualAlloc 기반 페이지 가드, 오버런 즉시 크래시)

USE_GPMEMORY 미정의 빌드: PoolAllocator가 gpMemory를 전혀 참조하지 않고
                         ::operator new/delete로 직접 위임됨 (풀 시스템 자체가 빠짐)
```

## 3. 계층별 역할

### 3.1 RawAllocator — 최하위 raw 메모리 소스

`RawAllocator.h` 하나의 네임스페이스로, `Alloc`/`Free`/`AllocAligned`/`FreeAligned` 네 함수를 제공합니다. 내부에서 컴파일 타임 매크로(`MIMALLOC_H`, `JEMALLOC_H_`, `TCMALLOC_TCMALLOC_H_`)로 실제 라이브러리를 분기하고, 아무것도 정의되지 않으면 표준 `malloc`/`_aligned_malloc`으로 폴백합니다.

① **런타임 분기가 아닌 컴파일 타임 분기**이므로 매 호출마다 오버헤드가 없습니다.

② 라이브러리마다 정렬 할당 함수의 인자 순서가 다른데(`mimalloc`은 `(size, alignment)`, `jemalloc`/`tcmalloc`은 `(alignment, size)`), 이 차이를 `AllocAligned` 내부에서 흡수해 상위 계층은 신경 쓸 필요가 없습니다.

③ 실제로 mimalloc 등을 사용하려면 해당 라이브러리 헤더(`<mimalloc.h>` 등)를 이 파일보다 먼저(또는 `pch.h`에) include해서 매크로가 정의되게 해야 합니다. 헤더만 바꿔치기하면 프로젝트 전체의 raw 할당 소스가 교체됩니다.

### 3.2 BaseAllocator — 메모리 모듈과 무관한 범용 raw 할당 유틸리티

`RawAllocator`에 그대로 위임하는 최하위 계층 할당자입니다. **`CMemory`/`CMemoryPool` 등 메모리 모듈 자체는 이 클래스를 상속받지 않습니다.** 이 두 클래스는 프로그램 시작 시 단 한 번(혹은 `POOL_COUNT`번)만 생성되는 부트스트랩 객체라, 이 할당을 raw 경로로 격리하는 것보다 전역 `new`/`delete`를 그대로 쓰는 단순함을 우선했습니다.

대신 `BaseAllocator`는 **메모리 모듈과 무관한 다른 기능 클래스들이 필요할 때 가져다 쓰는 범용 유틸리티**로 존재합니다. 두 가지 방식으로 쓸 수 있습니다.

① **정적 메서드 직접 호출**: `BaseAllocator::Alloc(size)` / `BaseAllocator::Release(ptr)` — raw 버퍼가 그때그때 필요할 때

② **상속(믹스인)**: `class SomeClass : public BaseAllocator` 형태로 상속받으면, 클래스 내부에 정의된 `operator new`/`delete`가 자동으로 `RawAllocator` 경유로 오버라이드됩니다. `new SomeClass()`처럼 평범하게 쓴 코드가 별도 API 없이 raw 할당 경로를 타게 됩니다.

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

① 요청 크기를 담을 데이터 페이지 수를 계산하고, 미리 예약해 둔 아레나(대형 가상 주소 공간) 안에서 그만큼을 커밋합니다.

② 데이터를 마지막 데이터 페이지의 **끝**에 딱 맞춰 배치합니다. 이렇게 하면 할당 크기를 1바이트라도 초과해서 쓰는 순간 다음 미커밋 페이지에 접근해 그 자리에서 Access Violation이 발생합니다.

③ 요청 크기를 그대로 쓰지 않고 16바이트 단위로 올림한 뒤 배치 위치를 계산합니다. `MemoryHeader`가 16바이트 정렬을 요구하는데, 페이지 끝에서 곧바로 `size`만큼 물러난 위치는 `size`가 16의 배수가 아니면 정렬이 깨지기 때문입니다. 이 올림 때문에 실제 데이터 끝과 페이지 끝 사이에 최대 15바이트의 여유가 생겨, 그 범위 안의 아주 작은 오버런은 즉시 크래시로 잡히지 않을 수 있습니다(그 이상은 여전히 즉시 크래시).

④ `CMemory::Allocate`/`Release` 내부의 `_STOMP` 매크로 분기로만 활성화되며, 평소 빌드에서는 꺼져 있어야 합니다.

⑤ 페이지 단위 가드 자체가 목적이므로 `RawAllocator`로 대체하지 않고 `VirtualAlloc`/`VirtualFree`를 직접 사용합니다.

**아레나 구조**: 프로세스 시작 후 첫 `StompAllocator::Alloc` 호출 시점에 큰 가상 주소 공간(기본 256GB)을 `call_once`로 한 번만 예약(`MEM_RESERVE`)해 둡니다. "가상 주소 공간 예약"이라는 상대적으로 무거운 커널 작업이 프로세스 생애 동안 단 한 번만 일어나므로, `_STOMP`를 켠 채로 대량의 할당/해제가 반복되는 통합 테스트에서도 예약 자체를 반복하지 않습니다.

**영역 구조 — 데이터 N페이지 + 전용 가드 페이지, 메타데이터는 힙**: 각 할당은 아레나 안에서 `[데이터 페이지들][가드 페이지 1개]` 형태로 예약됩니다. 이 중 실제로 커밋(`MEM_COMMIT`)되는 것은 데이터 페이지뿐이며, 가드 페이지는 주소 공간만 예약된 채 절대 커밋되지 않습니다. 데이터는 항상 마지막 데이터 페이지의 끝에서 정확히 끝나므로, 그 바로 뒤는 곧바로 가드 페이지입니다. 아레나는 단조 증가하는 범프 커서로 영역을 나눠주는데, 이 커서가 가드 페이지 몫까지 함께 전진시켜 그 자리를 다음 할당에 절대 재사용하지 않으므로, "다음 할당이 아직 이 자리를 커밋하지 않았는가"라는 타이밍에 기대지 않고도 데이터 영역 끝을 1바이트라도 넘는 접근은 항상 접근 위반(Access Violation)으로 크래시합니다. (아레나 없이 가드 페이지만으로 자연히 보호받던 이전 세대 설계는, 장시간 실행되는 서버에서 인접 영역이 나중에 다른 할당으로 커밋되면 그 자리를 침범하는 오버런이 크래시 대신 조용한 데이터 오염으로 이어질 수 있었습니다 — 전용 가드 페이지는 이 타이밍 의존성을 구조적으로 없앱니다.)

각 영역의 메타데이터(`RegionMeta` — 데이터 시작 주소, 데이터 영역 크기, 이중 반납 탐지 플래그, Fallback 여부)는 아레나 내부가 아니라 `RawAllocator`를 통해 일반 힙에서 별도로 할당됩니다. `AllocationHeader`가 사용자 포인터 바로 앞에서 이 `RegionMeta*`를 직접 들고 있으므로, `Release`는 페이지 시작 주소를 역산하는 대신 헤더에서 포인터 하나만 읽으면 곧바로 메타데이터에 도달합니다. 메타데이터를 힙에 두는 덕분에 `Release` 시 데이터 페이지 전체를 예외 없이 `MEM_DECOMMIT`할 수 있어(예전처럼 "재사용을 위해 영구 상주해야 하는 페이지"가 아레나 안에 남지 않음), 반납된 영역의 물리 메모리를 실제로 OS에 돌려줍니다. 재사용을 기다리는 `RegionMeta` 자체는 데이터 크기별 free-list에 대기하는 동안 계속 힙에 남아있지만, 그 크기는 페이지 단위가 아니라 포인터 몇 개 수준(수십 바이트)이라 이전 세대 대비 상주 비용이 훨씬 낮습니다.

**완전 락프리 재사용**: 데이터 페이지는 디커밋되어 돌아가지만, 그 페이지를 가리키던 `RegionMeta`는 버려지지 않고 데이터 크기별 Lock-free SLIST(`CMemoryPool`과 동일한 방식)에 등록되어 같은 크기의 다음 `Alloc`이 재사용합니다. `RegionMeta`가 힙에 상주해 항상 유효하므로 그 자체를 SLIST 노드로 재사용할 수 있어, 반납/재사용 경로 전체가 락 없이 동작합니다. "지금까지 한 번도 등장한 적 없는 새로운 크기"를 위한 SLIST 헤더를 맵에 처음 등록하는 순간에만 `shared_mutex`의 배타 락을 아주 짧게 잡고(이중 확인 잠금 패턴), 이미 존재하는 크기에 대한 이후의 모든 반복 조회는 공유 락(읽기)만으로 끝납니다. 이 재사용 구조 덕분에, 장시간 실행되는 스트레스 테스트에서도 예약된 아레나가 데이터 크기 종류 수에 비례해서만 소모되고, 반복되는 할당/해제 횟수에는 비례해서 소모되지 않습니다.

① 이중 반납 탐지: `Release`는 메타데이터의 `freed` 플래그를 `exchange(1)`으로 원자적으로 확인/설정합니다. 이미 반납된 상태(1)라면 즉시 `ASSERT_CRASH`로 잡아내며, 이 검사 자체가 락 없이 이루어집니다.

② **Fallback 영역의 해제**: 아레나가 고갈되어 `VirtualAlloc`으로 직접 예약·커밋한 Fallback 영역은 재사용 대상이 아니므로, `Release`는 해당 가상 주소 범위를 `VirtualFree(MEM_RELEASE)`로 완전히 반환하고 그 영역의 `RegionMeta`도(힙에 있으므로) 함께 `delete`합니다. 반면 일반 아레나 할당은 데이터 페이지만 `MEM_DECOMMIT`하고 `RegionMeta`는 free-list에 등록해 재사용을 기다립니다. `Cleanup()`을 호출하면 이때 아직 free-list에 남아 재사용을 기다리던 `RegionMeta`들도 모두 순회하며 힙에서 해제한 뒤, 마지막으로 아레나 전체를 `VirtualFree`합니다.

③ 주소 재사용과 진단 능력의 트레이드오프: 디커밋된 데이터 페이지를 재사용한다는 것은, 아주 오래전에 반납된 뒤에도 그 주소를 들고 있는 진짜 댕글링 포인터가 그 주소가 다른 할당으로 재사용된 시점 이후에 접근하면 크래시 대신 조용한 오염으로 이어질 수 있다는 뜻입니다. 다만 이는 아레나 없이 `VirtualFree(MEM_RELEASE)`로 완전히 반환하는 방식에서도 OS가 해제된 가상 주소를 이후 다른 `VirtualAlloc` 호출에 재사용할 수 있었던 것과 동일한 성격의 특성입니다.

④ **`Cleanup()` 이후 재사용 차단(`s_isCleanedUp`)**: `Cleanup()`은 `s_arenaBase`/`s_arenaCursor`를 초기 상태(`nullptr`)로 되돌리지만, `s_arenaInitFlag`(`once_flag`)는 건드리지 않습니다. 만약 재사용 방지 장치가 없다면 `Cleanup()` 이후에 다시 호출된 `Alloc()`이 `EnsureArenaInitialized()`의 `call_once`를 이미 소진된 것으로 보고 재초기화를 건너뛴 채, 이미 `nullptr`로 되돌아간 `s_arenaBase` 근방을 유효한 아레나 주소로 오인해 몇 번의 호출 뒤에야 원인 파악이 어려운 형태로 크래시할 수 있습니다. 이를 막기 위해 별도의 `atomic<bool> s_isCleanedUp` 플래그를 두어 `Cleanup()` 마지막에 `true`로 설정하고, `Alloc()`/`Release()` 진입 시점과 `Cleanup()` 자신의 중복 호출 여부를 `ASSERT_CRASH`로 즉시 검증합니다. `Cleanup()` 이후에는 `StompAllocator`를 다시 쓸 수 없다는 계약을, 재현하기 어려운 우연한 크래시 대신 위반 즉시 명확한 크래시로 드러내는 장치입니다.

**사용 시점**: 메모리 손상 버그(오버런, 댕글링 포인터로 인한 오염 등)를 추적할 때만 `_STOMP`를 켜고 재현 → 문제 지점에서 즉시 크래시 → 콜스택 확인 → 원인 수정 후 다시 끔.

### 3.4 PoolAllocator — 실서비스 핫패스

평상시 게임 로직 코드가 사용하는 유일한 경로입니다. `gpMemory`(전역 `CMemory` 싱글턴)에게 위임하며, `xnew`/`xdelete`, `StlAllocator`를 통해 자동으로 이 경로를 타게 됩니다.

**`USE_GPMEMORY` 매크로 게이트**: `PoolAllocator::Alloc`/`Release`는 `#if defined(USE_GPMEMORY)`로 감싸져 있어, 이 매크로가 정의된 빌드에서만 실제로 `gpMemory->Allocate`/`Release`(풀 경로)로 위임합니다. 정의되지 않은 빌드에서는 `::operator new`/`::operator delete`로 직접 폴백하여, 이 모듈이 `CMemory`/`gpMemory`를 전혀 참조하지 않고도 컴파일·동작할 수 있습니다(예: 메모리 풀 시스템 전체를 아직 링크하지 않은 초기 통합 단계나, 풀 경로를 배제하고 싶은 단위 테스트 빌드). 이 매크로가 꺼져 있으면 `xnew`/`xdelete`/`StlAllocator`가 여전히 동일한 API로 동작하되 내부적으로는 표준 힙을 그대로 쓰게 되므로, 풀링/락프리/TLS 캐시 이점은 전부 사라집니다.

`xnew<Type>(args...)`는 생성자 인자를 `std::forward` 대신 `static_cast<Args&&>(args)...`로 직접 전달합니다. 의미상 완전히 동일한 perfect forwarding이지만, 라이브러리 함수 호출 형태 자체를 없애 초당 수백만 번 불릴 수 있는 이 경로에서 인라인 여부를 컴파일러 재량에 맡기지 않습니다.

`StlAllocator<T>`는 표준 Allocator 요구사항(`value_type`, `allocate`, `deallocate`)에 더해, 서로 다른 타입 간 리바인딩 생성자와 대입 연산자도 갖추고 있습니다. 상태가 없는 allocator라 실제로 할 일은 없지만, 일부 STL 구현체(특히 구버전 MSVC)가 이 대입 연산자를 요구하기 때문에 명시적으로 정의해 둔 것입니다.

**확장 정렬 할당(`AllocAligned`/`ReleaseAligned`)도 gpMemory 풀을 그대로 경유**: `gpMemory`(`CMemory`)는 내부적으로 `SLIST_ALIGNMENT`(16바이트) 단위로만 블록을 관리하므로, `alignof(T)`가 그보다 큰 타입(`alignas(32)` 이상의 SIMD 데이터 등)은 그 정렬을 직접 보장해 줄 수 없습니다. 그렇다고 이런 타입을 raw 정렬 할당으로 완전히 우회시키면, 캐시 라인 정렬이 잦은 이 프로젝트(`CMemoryPool`, `TlsBucket` 등)의 특성상 반복 할당/해제되는 타입 상당수가 풀링 이점을 아예 못 받게 됩니다. 대신 `PoolAllocator::AllocAligned`는 "요청 크기 + 정렬 여유분 + 원본 포인터 저장 공간"만큼을 `gpMemory->Allocate()`로 통상 크기 할당받은 뒤, 반환 직전 주소를 요청 정렬에 맞춰 올림 보정하고 그 앞 8바이트에 원본 포인터를 저장해 둡니다. `ReleaseAligned`는 그 8바이트를 역산해 원본 포인터로 `gpMemory->Release()`를 호출합니다. 이 덕분에 정렬 요구가 큰 타입도 `gpMemory` 크기별 풀을 그대로 타 반복 할당/해제 패턴에서 풀링 이점을 얻으며, 대가는 `alignment + 8`바이트 수준의 약간의 오버헤드입니다.

**전역 `MakeShared` — `allocate_shared` + `StlAllocator` 기반 `shared_ptr`**: `Allocator.h`는 특정 타입 전용 풀 없이 임의의 `Type`에 바로 쓸 수 있는 전역 템플릿 함수 `MakeShared<Type>(args...)`도 제공합니다.

```cpp
template<typename Type, typename... Args>
std::shared_ptr<Type> MakeShared(Args&&... args)
{
	return std::allocate_shared<Type>(StlAllocator<Type>(), std::forward<Args>(args)...);
}
```

`std::allocate_shared`와 `StlAllocator<Type>`을 결합해, 객체와 참조 카운트 컨트롤 블록을 하나의 블록으로 묶어 `gpMemory`의 크기별 공유 풀에서 단 1회만 할당합니다. 어떤 `Type`이든 그 타입 전용 풀을 미리 준비해 둘 필요 없이 바로 쓸 수 있다는 것이 장점입니다.

### 3.5 CMemoryPool — 크기별 Lock-free 프리리스트

하나의 고정 크기(`_allocSize`)만 다루는 free-list입니다.

① **`Push`**: 블록을 "미사용"으로 표시(`allocSize = 0`)한 뒤 `InterlockedPushEntrySList`로 SLIST에 되돌립니다.

② **`Pop`**: SLIST에서 블록을 꺼냅니다. 비어있으면 `RawAllocator::AllocAligned`로 새로 할당합니다. 즉 풀은 "상한 없이 필요할 때마다 늘어나는" 동적 프리리스트입니다.

③ `MemoryHeader`가 Windows `SLIST_ENTRY`를 상속하고 있어서, 별도의 `next` 포인터 없이 헤더 자체가 링크드리스트 노드로 재사용됩니다 — 메모리 오버헤드를 최소화하는 설계입니다.

④ `_poolCheckedOutCount`/`_poolReserveCount`는 각각 "이 풀의 SLIST 밖으로 나간(반납되지 않은) 블록 수", "현재 이 풀의 SLIST에 대기 중인 블록 수"를 추적하는 통계용 원자 카운터입니다. 이 두 값은 어디까지나 "이 풀 하나의 SLIST 출입"만 반영하는 풀 단위 지표라는 점에 유의해야 합니다 — 상위 계층인 `CMemory`가 TLS 배치 캐시로 블록을 미리 꺼내 쌓아두므로, `_poolCheckedOutCount`에는 아직 사용자에게 전달되지 않고 TLS 캐시에 대기 중인 블록도 포함됩니다. 실제로 "지금 살아있는 할당이 몇 개인가"가 필요하면 이 풀 단위 카운터 대신 `CMemory::GetLiveAllocationCount()`(3.6절)를 사용해야 합니다.

⑤ **캐시 라인 격리**: 클래스 전체를 64바이트(일반적인 CPU 캐시 라인 크기) 경계에 정렬해, 서로 다른 `CMemoryPool` 인스턴스끼리 캐시 라인을 공유하는 false sharing을 방지합니다. 또한 SLIST 헤더(원자 연산으로 매번 갱신)와 순수 통계 카운터(`_poolCheckedOutCount`/`_poolReserveCount`)를 서로 다른 캐시 라인에 두어, 통계를 읽는 동작이 SLIST Pop/Push의 원자 연산과 캐시 라인을 두고 경합하지 않게 합니다.

⑥ **`WarmUpPush` — 워밍업 전용 반납 경로**: `CMemory::WarmUp()`(3.6절/5-1절)이 미리 만든 블록을 채워 넣을 때는 `Push()` 대신 `WarmUpPush()`를 씁니다. 둘 다 `allocSize=0` 표시 후 SLIST에 넣고 `_poolReserveCount`를 증가시키는 것은 같지만, `Push()`는 "이미 나가 있던 블록이 돌아왔다"는 뜻으로 `_poolCheckedOutCount`도 함께 감소시키는 반면, `WarmUpPush()`는 애초에 한 번도 나간 적 없는 블록이므로 `_poolCheckedOutCount`를 건드리지 않습니다. 워밍업에 일반 `Push()`를 썼다면 아직 아무도 꺼내가지 않은 블록 때문에 `_poolCheckedOutCount`가 음수로 내려가 통계가 의미를 잃었을 것입니다.

⑦ **`GetBlockSize()`**: 이 풀이 다루는 고정 블록 크기(`_allocSize`)를 그대로 반환하는 조회 함수입니다. `CMemory::WarmUp()`이 사용자가 넘긴 데이터 크기를 실제 풀의 전체 블록 크기(헤더 포함, 32/128/256 단위로 올림된 값)로 정확히 치환해 `RawAllocator::AllocAligned()`를 호출하는 데 씁니다.

**`MemoryHeader::allocSize`가 `atomic<size_t>`인 이유**: 이 값은 단순 통계가 아니라 "이 블록이 살아있는지(>0), 반납되었는지(0)"를 판별하는 이중 반납(double free) 탐지 수단입니다. 두 스레드가 극단적인 타이밍에 같은 포인터를 동시에 반납하려는 경쟁 상황에서도 탐지가 놓치지 않으려면 "값을 읽고 0인지 확인한 뒤 0으로 바꾸는" 과정 자체가 원자적이어야 합니다. 일반 `size_t`였다면 두 스레드가 동시에 "아직 0이 아님"을 확인하고 둘 다 반납을 진행해버리는 TOCTOU(Time-Of-Check-To-Time-Of-Use) 경쟁이 이론상 가능합니다. `exchange()`로 "읽고 0으로 바꾸기"를 단일 원자 연산으로 묶으면, 두 스레드 중 정확히 하나만 원래 값(>0)을 받고 나머지는 이미 0이 된 값을 받게 되어 이중 반납을 경쟁 상태 없이 감지합니다(`CMemory::Release`, `CObjectPool::Push`에서 이 방식 사용). `size_t`를 쓰는 이유는 이 값이 헤더를 포함한 전체 할당 크기를 그대로 담기 때문으로, `int32` 등으로 좁히면 대형 할당 경로(`MAX_ALLOC_SIZE` 초과분)에서 값이 잘릴 수 있습니다. `MemoryHeader`는 `DECLSPEC_ALIGN(SLIST_ALIGNMENT)`(16바이트) 제약상 어차피 16바이트로 패딩되므로, `atomic<size_t>`(x64 기준 8바이트)를 쓰더라도 헤더 크기나 정렬 요구사항에는 영향이 없습니다.

**`MemoryHeader` 생성자가 경쟁 상태 없이 안전한 이유**: `AttachHeader`가 raw 메모리 위에 `MemoryHeader(size)`를 placement new로 얹는 시점에는, 이 헤더를 가리키는 다른 포인터가 아직 어디에도 존재하지 않습니다(방금 `RawAllocator`/`InterlockedPopEntrySList`에서 이 스레드가 단독으로 받아온 블록이기 때문). 따라서 `allocSize` 필드의 최초 초기화 자체는 원자 연산일 필요가 없고, 이 블록이 SLIST에 올라가거나 사용자에게 반환되어 다른 스레드가 접근할 수 있게 된 "이후" 시점부터만 atomic 연산(`exchange` 등)으로 접근이 보호되면 충분합니다.

### 3.6 CMemory — 사이즈별 라우터

"요청 크기 → `_pools` 배열 인덱스"를 테이블 조회가 아니라 `ComputePoolIndex()`의 분기 없는(branchless) 수식 계산으로 O(1)에 구합니다. `MAX_ALLOC_SIZE+1`개 항목짜리 배열로 조회하는 방식도 가능하지만, 이런 크기의 테이블은 다른 코드가 L1/L2 캐시를 심하게 오염시킨 상태에서 참조하면 캐시 미스가 날 수 있습니다. 수식 계산은 애초에 테이블 자체가 없으므로 이 캐시 미스 가능성이 구조적으로 없습니다.

① 32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는 256단위로 총 `POOL_COUNT`개의 풀을 생성합니다.

② 예를 들어 1~32바이트 요청은 모두 32바이트 풀로 라우팅됩니다(내부 단편화를 일부 감수하는 대신 풀 개수를 줄여 관리 비용을 낮춤).

③ `MAX_ALLOC_SIZE`(4096바이트)를 초과하는 요청은 풀을 거치지 않고 `RawAllocator::AllocAligned`/`FreeAligned`로 직접 처리합니다.

④ `Allocate`/`Release`는 데이터 포인터 앞의 `MemoryHeader`를 attach/detach하며, 헤더의 `allocSize` 값을 보고 "풀 반납 대상인지, raw 해제 대상인지"를 판별합니다.

⑤ `ComputePoolIndex`는 세 구간(32/128/256 단위, 경계 1024/2048) 각각에 대해 "이 구간에 속하는 길이"를 `(std::min)`/`(std::max)`를 조합한 클램핑 식으로 0~구간폭 사이에 제한한 뒤, 각 구간 단위로 올림한 개수를 모두 더하는 방식입니다(`len1 = min(size, 1024)`, `len2 = min(max(size-1024, 0), 1024)`, `len3 = max(size-2048, 0)`). `allocSize`가 실제로 속하지 않는 구간은 클램핑으로 기여분이 자동으로 0이 되므로, `if`/`else`로 어느 구간인지 먼저 판별하는 과정 자체가 없습니다. 이 비교-삼항 형태의 클램핑 식은 Release 최적화 시 조건부 이동(cmov)으로 컴파일되어 실제 분기(jmp)가 발생하지 않는 경향이 있으며, `std::min`/`std::max`를 괄호로 감싸(`(std::min)`) 호출하는 이유는 `Windows.h`가 `NOMINMAX` 미정의 시 `min`/`max`를 매크로로 정의해 이름이 충돌할 수 있기 때문입니다. 이 수식은 생성자의 세 단계 구간 생성 로직과 반드시 일치해야 하므로, 생성자가 풀을 만들 때마다 `ComputePoolIndex()`의 결과와 실제 `_pools` 인덱스가 같은지 `ASSERT_CRASH`로 교차 검증합니다.
   - 예) `allocSize=48` → 1구간 길이 `len1=48` → `count1=(48+31)>>5=2` → `poolIndex=2-1=1` (0번 풀=32B, 1번 풀=64B이므로 48B 요청은 64B 풀로 올림 배정됨 — 생성자의 "32단위로 올림 배정"과 동일한 결과)

⑥ `Allocate`는 `size`가 0 이하인 경우와, `size + sizeof(MemoryHeader)`를 더하는 산술 자체가 오버플로하지 않는지(`size`가 `SIZE_MAX - sizeof(MemoryHeader)`를 넘는지)를 먼저 `ASSERT_CRASH`로 걸러냅니다. `allocSize`가 헤더 포함 크기를 그대로 담는 `size_t`이므로 이 계산 전체가 `size_t` 산술로 이루어지며, 이 검증이 없으면 오버플로로 `allocSize`가 실제보다 훨씬 작은 값으로 랩어라운드되어 풀 인덱스 계산이 잘못된 값을 가리키는 메모리 오염으로 이어질 수 있습니다.

⑦ `Release`는 `header->allocSize`를 `exchange(0)`으로 원자적으로 읽고 표시합니다. 같은 포인터로 `Release`가 동시에 두 번 호출되어도(경쟁 상태 포함) 둘 중 정확히 하나만 원래 값을 받고 나머지는 이미 0이 된 값을 받아 `ASSERT_CRASH`로 즉시 걸립니다.

⑧ **생성자의 구간별 명시적 시작**: 32~1024/1024~2048/2048~4096 세 구간을 만드는 `for`문 세 개가 모두 같은 `size` 변수를 공유합니다. 만약 두 번째·세 번째 구간이 "이전 구간이 끝난 값에서 이어서 시작"했다면(`size += 128`처럼 증가만 하고 재설정하지 않으면), 이전 구간 종료값(예: 1056)에서 그대로 이어받아 시작하게 되어 풀 크기가 32/128/256 단위 경계와 어긋나는 값(1056, 1184, ...)이 생성됩니다. 그래서 두 번째 구간은 항상 `1024 + 128`에서, 세 번째 구간은 항상 `2048 + 256`에서 명시적으로 다시 시작합니다. 이 경계 어긋남이 다시 발생하더라도 곧바로 드러나도록, 풀을 생성할 때마다 그 크기에 대한 `ComputePoolIndex()` 결과가 실제 `_pools` 인덱스와 일치하는지 `ASSERT_CRASH`로 즉시 검증합니다.

**살아있는 할당 수 추적(`GetLiveAllocationCount`)**: 각 `CMemoryPool`의 `_poolCheckedOutCount`(3.5절)는 TLS 배치 충전 시점에 갱신되는 풀 단위 통계라, TLS 캐시에 아직 대기 중인(사용자에게 전달되지 않은) 블록까지 포함해 "실제 살아있는 할당 수"로 쓰기에는 부정확합니다. 이를 위해 `CMemory`는 별도의 `atomic<int64> _liveAllocationCount`를 두고, `Allocate()`가 데이터 포인터를 반환하기 직전과 `Release()`가 유효한 반납을 확인한 직후에만 정확히 하나씩 증감시킵니다(`_STOMP` 경로 포함, TLS 캐시 히트 여부와 무관). `CMemory::GetLiveAllocationCount()`로 조회할 수 있으며, 메모리 누수 여부를 모니터링하거나(프로세스 종료 시점에 0이 아니면 누수 의심) 부하 테스트에서 순간 할당량을 관찰하는 용도로 사용합니다. `WarmUp()`으로 미리 채워넣기만 하고 아직 아무도 `Allocate()`하지 않은 블록은 이 카운터에 포함되지 않습니다.

### 3.7 CObjectPool — 타입 전용 오브젝트 풀

`gpMemory`의 크기 구간별 공유 풀과 달리, `Type`마다 정확히 `sizeof(Type)` 크기의 `CMemoryPool`을 독점적으로 갖는 템플릿입니다. 특정 타입이 압도적으로 많이 생성/파괴되어 그 타입 전용 풀로 분리하는 것이 유리할 때 `xnew`/`xdelete` 대신 사용합니다.

① **정렬 제약(`static_assert`)**: `alignof(Type)`이 `SLIST_ALIGNMENT`(16바이트)를 넘는 타입은 지원하지 않으며, 클래스 정의부의 `static_assert`로 컴파일 타임에 즉시 걸립니다. `MemoryHeader::AttachHeader()`가 16-정렬된 헤더 주소에서 정확히 `sizeof(MemoryHeader)`(16바이트)만 전진시켜 데이터 포인터를 만드는 구조라, `alignof(Type)==32`처럼 그 이상의 정렬을 요구하는 타입은 이 단순 전진만으로 정렬을 보장받지 못하기 때문입니다(header가 16-정렬이어도 header+16이 반드시 32-정렬이라는 보장은 없음). 이런 타입은 `xnew`/전역 `MakeShared`(3.4절 `PoolAllocator::AllocAligned` 경유)를 대신 사용해야 합니다.

② **`s_allocSize` 계산과 `AlignUp`**: `s_allocSize`는 단순히 `sizeof(Type) + sizeof(MemoryHeader)`가 아니라 그 값을 `SLIST_ALIGNMENT`(16) 단위로 올림(`AlignUp`)한 값입니다. `sizeof(Type)`이 16의 배수라는 보장이 없어(예: `int64` 두 개 + `int32` 하나면 `sizeof==24`), 올림 없이는 그런 타입에서 `CMemoryPool` 생성자의 `allocSize % SLIST_ALIGNMENT == 0` 요구가 깨져 풀 생성 자체가 `ASSERT_CRASH`로 즉시 실패합니다.

③ **이중 반납 방어**: `Push()`는 소멸자를 호출하기 전에 먼저 헤더의 `allocSize`가 0이 아닌지 확인합니다. `CMemoryPool::Push`는 `allocSize`를 무조건 0으로 덮어쓰기만 할 뿐 이미 0인지 확인하지 않으므로, 같은 포인터가 두 번 `Push`되면 SLIST 노드가 자기 자신을 가리키는 순환 구조로 꼬여 서로 다른 두 번의 `Pop()`이 같은 메모리 블록을 동시에 소유하는 조용한 오염으로 이어질 수 있습니다. 검증을 먼저 하고 소멸자를 나중에 호출하는 순서 덕분에, 이미 파괴된 객체의 소멸자가 다시 호출되는 이중 소멸까지 함께 방지됩니다.

④ **타입 전용 풀의 지연 초기화**: `CMemoryPool` 인스턴스는 클래스 정적 멤버가 아니라 함수 지역 정적 변수(`GetPool()` 내부의 Meyer's Singleton)로 관리됩니다. C++11부터 함수 지역 정적 변수의 동적 초기화는 최초 사용 시점에 정확히 한 번, 스레드 안전하게 이루어짐이 표준으로 보장되므로, 다른 번역 단위의 전역/정적 객체 생성자에서 이 풀을 먼저 참조하더라도 초기화 순서 문제가 발생하지 않습니다.

⑤ **`_STOMP` 연동**: `_STOMP` 빌드에서는 풀을 거치지 않고 `StompAllocator`로 대체되므로, `StompAllocator`의 아레나 최적화(3.3절)를 자동으로 함께 사용합니다.

⑥ **TLS 배치 캐시 없음**: `CMemory`와 달리 스레드 로컬 배치 캐시가 없어, `Pop()`/`Push()`가 매번 `CMemoryPool`의 원자 연산(`InterlockedPop/PushEntrySList`)을 직접 호출합니다. 이 풀을 쓰는 타입이 초당 수만~수십만 번 생성/파괴되는 극단적 핫패스라면 멀티코어 경합이 그대로 남아있다는 뜻입니다. 이는 결함이 아니라 의도적으로 단순하게 유지한 설계이며, 실제 사용 타입의 생성 빈도가 그 정도로 높아지면 그때 `CMemory`와 동일한 TLS 배치 캐시를 얹는 것을 고려할 수 있습니다.

⑦ **`MakeShared()` — 타입 전용 풀 기반 `shared_ptr`**: `CObjectPool<Type>`은 자체 `MakeShared()`를 제공합니다. 내부적으로 `Pop(args...)`로 객체를 만든 뒤, `shared_ptr<Type>(포인터, &CObjectPool<Type>::Push)` 형태로 소멸 시 `Push()`가 호출되도록 커스텀 deleter를 지정합니다. 의도적으로 `std::allocate_shared()`를 쓰지 않는데, `allocate_shared`는 컨트롤 블록과 `Type` 객체를 하나로 묶어 (컨트롤 블록 + `Type`) 크기의 블록 하나를 할당하며 이 크기는 표준 라이브러리 구현마다 다릅니다. `GetPool()`은 `s_allocSize`(= `Type` + `MemoryHeader`)로 고정된 블록만 다루는 풀이라 그 가변 크기를 그대로 흘려보낼 수 없기 때문입니다. 그 대가로 `Type` 객체(타입 전용 풀)와 컨트롤 블록(기본 힙, `std::shared_ptr`의 기본 할당 경로)이 분리되어 총 2회 할당이 발생하지만, 대신 `Type` 자체는 다른 타입과 풀을 공유하지 않고 내부 단편화 없이 전용 풀에 남습니다. 3.4절의 전역 `MakeShared<Type>()`(1회 할당, `gpMemory` 공유 풀)과의 선택 기준은 6절/7-1절 비교를 참고하십시오.

## 4. 실행 흐름 예시

### 4.1 일반적인 객체 생성/파괴

```cpp
Player* p = xnew<Player>();   // PoolAllocator::Alloc → gpMemory->Allocate
...
xdelete(p);                    // 소멸자 호출 → PoolAllocator::Release → gpMemory->Release
```

① `xnew`가 `PoolAllocator::Alloc(sizeof(Player))` 호출

② `PoolAllocator`가 `gpMemory->Allocate(size)`로 위임

③ `CMemory::Allocate`가 `allocSize`를 계산해 `ComputePoolIndex(allocSize)`로 담당 풀 인덱스를 찾음

④ 해당 `CMemoryPool::Pop()` — SLIST에 여유 블록이 있으면 즉시 반환, 없으면 `RawAllocator`로 신규 할당

⑤ `MemoryHeader::AttachHeader`로 헤더를 얹고 데이터 포인터 반환

`shared_ptr`이 필요하다면 `xnew`/`xdelete` 쌍 대신 전역 `MakeShared<Player>()`를 쓸 수 있습니다(3.4절) — 객체와 참조 카운트 컨트롤 블록이 하나로 묶여 `gpMemory` 공유 풀에서 단 1회에 할당됩니다.

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

① **`Allocate`**: 로컬 캐시가 비어 있을 때만 전역 `CMemoryPool`에서 이 크기 구간에 맞는 배치 개수만큼 한 번에 당겨와 로컬을 채웁니다. 이후 그만큼의 `Allocate`는 전역 풀을 전혀 건드리지 않고 로컬에서 처리됩니다.

② **`Release`**: 블록을 로컬 캐시에 반납만 합니다. 로컬 캐시가 이 크기 구간의 상한을 넘으면 절반을 전역 풀에 배치로 돌려줍니다.

③ 결과적으로 원자 연산 호출 빈도가 배치 개수에 반비례해 크게 줄어듭니다.

**배치 크기는 풀마다 균일하지 않고 블록 크기 구간별로 차등 적용됩니다.**

| 블록 크기(헤더 포함) | 배치 충전 개수 | 로컬 캐시 상한 | 의도 |
|---|---|---|---|
| ~128B 이하 | 64 | 256 | 소형/고빈도 구간 — 원자 연산 절감 최우선 |
| ~1024B 이하 | 32 | 128 | 중형 구간 — 절충 |
| 1024B 초과 | 4 | 16 | 대형/저빈도 구간 — 상주 메모리 절약 우선 |

작고 자주 쓰이는 크기(32B, 64B 등)는 배치를 크게 잡아 경합을 최대한 줄이고, 크고 드물게 쓰이는 크기(2KB, 4KB 등)는 배치를 작게 잡아 스레드마다 불필요하게 큰 메모리가 상주하는 것을 막습니다. `CMemory::DetermineTlsBatchSize`/`DetermineTlsMaxCount`가 생성자에서 미리 계산해 `_tlsBatchSizeTable`/`_tlsMaxCountTable`에 저장해 두므로, `Allocate`/`Release` 핫패스는 조건 분기 없이 배열 조회만으로 즉시 사용합니다.

### 구현상 핵심 포인트

① **`MemoryHeader`가 이미 `SLIST_ENTRY`(= `Next` 포인터 하나)를 상속**하고 있어, 이 필드를 그대로 재사용해 로컬 free-list를 연결했습니다. 그 결과 `MemoryHeader`, `CMemoryPool` 두 파일은 전혀 수정하지 않고 `CMemory` 계층에만 캐시를 얹을 수 있었습니다.

② **`ComputePoolIndex`**: 크기 → `_pools` 배열 인덱스를 테이블 없이 수식으로 계산합니다. TLS 캐시(`_tlsCache.buckets[poolIndex]`)를 배열로 바로 인덱싱하기 위한 용도이며, 이 인덱스 하나로 풀 포인터(`_pools[poolIndex]`)와 TLS 버킷(`_tlsCache.buckets[poolIndex]`), 배치/상한 테이블(`_tlsBatchSizeTable[poolIndex]`)까지 모두 조회할 수 있습니다.

③ **`thread_local TlsCache` 소멸자**: 스레드가 종료되면 MSVC가 자동으로 `TlsCache`의 소멸자를 호출합니다. 이 소멸자에서 로컬에 남은 모든 블록을 원래 속했던 전역 `CMemoryPool`로 되돌립니다. 이 처리가 없으면 스레드가 죽을 때마다 로컬 캐시에 있던 메모리가 다른 스레드에서 재사용되지 못하고 사실상 누수처럼 방치됩니다.

④ **`TlsCache`는 `CMemory`의 nested class**이므로 C++11부터 `CMemory`의 private 멤버(`_pools`)에 별도의 `friend` 선언 없이 접근할 수 있습니다.

⑤ **`DrainBuckets`의 `gpMemory` null 체크**: 정해진 파괴 순서 규약을 지키지 않는 스레드(뒤에서 설명할 "그 외 스레드")가 프로세스 종료 시점 근처에 뒤늦게 종료되어 `gpMemory`가 이미 파괴된 뒤 `TlsCache` 소멸자가 호출되는 극단적인 상황을 대비해, `gpMemory`가 `nullptr`이면 반납을 시도하지 않고 조용히 반환합니다. 이 경우 남은 블록은 정상 반납되지 못하지만(누수), 프로세스 종료 시점이므로 OS가 결국 회수하며, use-after-free로 크래시가 나는 것보다 안전합니다.

⑥ **`DrainBuckets`도 `USE_GPMEMORY`로 게이트됨**: 함수 본문 전체가 `#if defined(USE_GPMEMORY)`로 감싸여 있어, 이 매크로가 꺼진 빌드에서는 반납 로직 자체가 컴파일되지 않고 빈 함수가 됩니다. `PoolAllocator`가 애초에 `gpMemory`를 참조하지 않는 빌드(위 3.4절)에서는 TLS 캐시에 쌓일 블록 자체가 없으므로 자연스러운 대칭입니다.

⑦ **`TlsBucket`의 정렬(`alignas(16)`)**: `TlsBucket`은 `thread_local`이라 스레드 간 false sharing은 원천적으로 없지만, 한 스레드가 `DrainBuckets` 등에서 `buckets[POOL_COUNT]` 배열을 순회할 때의 캐시 지역성은 별개 문제입니다. `sizeof(TlsBucket)`은 16바이트로 캐시 라인(64바이트)의 정확한 약수지만, 포인터 멤버 때문에 자연 정렬이 8바이트뿐이라 배열 시작 주소가 16바이트로 정렬되지 않으면 4번째 원소마다 캐시 라인 경계를 걸치게 됩니다. `alignas(16)`을 붙이면 크기가 이미 16의 배수라 패딩 없이(메모리 낭비 없이) 모든 원소가 항상 하나의 캐시 라인 안에 완전히 들어가도록 보장됩니다.

### 워밍업(Warm-up)

`CMemoryPool::Pop()`은 풀이 비어 있을 때만 `RawAllocator`로 새로 raw 할당을 받아옵니다. 즉 서버 구동 직후 동시 접속이 몰리는 시점에는, 자주 쓰이는 크기의 블록들이 아직 한 번도 채워진 적이 없어 그 시점에 몰아서 raw 할당이 발생하며 짧은 지연 스파이크가 생길 수 있습니다.

`CMemory::WarmUp(allocDataSize, count)`를 호출하면 지정한 크기의 풀에 raw 블록을 `count`개 미리 만들어 채워 넣습니다. 서버 시작 시점에 실제로 자주 생성되는 주요 크기(예: 패킷 헤더, 세션 객체 등)만 선별적으로 호출하는 것을 권장합니다. 모든 풀을 일괄적으로 채우면 시작 시점에 쓰이지도 않을 메모리를 크게 낭비할 수 있으므로, 워밍업은 항상 실측 기반으로 대상을 좁혀서 적용해야 합니다.

내부적으로는 `ComputePoolIndex()`로 담당 풀을 찾은 뒤 그 풀의 `GetBlockSize()`로 실제 전체 블록 크기(헤더 포함, 32/128/256 단위로 올림된 값)를 조회하고, 그 크기만큼 `RawAllocator::AllocAligned()`로 raw 블록을 만들어 `pool->WarmUpPush()`(일반 `Push()`가 아님, 3.5절)로 채워 넣습니다. `WarmUpPush()`를 쓰는 이유는 이 블록들이 아직 아무에게도 나간 적이 없어 `_poolCheckedOutCount`를 감소시킬 대상이 아니기 때문입니다.

## 5-2. 전역 시스템 파괴 순서 (BaseGlobal / ThreadManager 연동)

`thread_local TlsCache`는 스레드 종료 시 전역 `CMemory`(`gpMemory`)의 풀을 참조합니다. 따라서 전역 시스템들의 생성/파괴 순서가 정확해야 안전합니다.

① **워커 스레드 경로**: `CThreadManager`의 소멸자가 `JoinThreads()`를 호출 → 각 워커 스레드가 종료되며 `thread_local TlsCache` 소멸자가 자동 실행되어 `gpMemory`를 참조합니다. 이 때문에 `gpThreadManager`는 항상 `gpMemory`보다 먼저 파괴되어야 합니다.

② **메인 스레드 경로**: `CThreadManager`는 워커 스레드만 관리하고 메인 스레드는 아무도 join하지 않습니다. 메인 스레드도 자신만의 `TlsCache` 인스턴스를 가지고 있는데, 이건 `main()`이 완전히 return한 뒤에야 소멸됩니다. 그래서 `gpMemory`를 delete하기 직전, 메인 스레드에서 `CMemory::FlushCurrentThreadCache()`를 명시적으로 호출해 로컬 캐시를 먼저 비웁니다.

③ **그 외 스레드 경로**: 서드파티 라이브러리 콜백 스레드 등 `CThreadManager`를 거치지 않고 만들어진 스레드가 `PoolAllocator`(`xnew`/`StlAllocator` 등)를 사용한다면, 메인 스레드와 동일한 규칙이 적용됩니다 — 그 스레드도 종료 전에 `CMemory::FlushCurrentThreadCache()`를 직접 호출해야 하고, 호출 시점까지 `gpMemory`가 살아있어야 합니다. 이 규칙을 지킬 수 없는 스레드라면 `DrainBuckets()`가 `gpMemory`의 `nullptr` 여부를 확인해 최소한 크래시 대신 누수로 완화되도록 방어되어 있습니다.

이를 위해 세 가지 장치가 맞물려 동작합니다.

**1) 생성/파괴 순서 — 생성의 역순, `CMemory`는 항상 처음이자 마지막**

`BaseGlobal::Init()`은 `gpMemory` → `gpGlobalQueue` → `gpJobTimer` → `gpDeadLockProfiler` → `gpThreadManager` 순으로 생성하고, `Destroy()`는 이 순서를 그대로 뒤집어(정확한 LIFO로) 파괴합니다(아래 "생성 순서"/"파괴 순서" 참고).

각 전역 포인터의 선언 자체가 자신이 속한 헤더의 인클루드 가드 매크로(`UC_MEMORY_H`, `UC_GLOBALQUEUE_H`, `UC_JOBTIMER_H`, `UC_THREADMANAGER_H`)로 게이트됩니다 — 해당 헤더가 이 번역 단위에 포함되지 않았다면 그 전역 포인터는 컴파일조차 되지 않아, 시스템 하나를 통째로 링크에서 빼는 것이 가능합니다. `gpDeadLockProfiler`는 여기에 `USE_GPDEADLOCKPROFILER && _DEBUG` 조건이 추가로 걸려 디버그 빌드에서만 생성됩니다. `BaseGlobal.h`의 전역 포인터 선언 순서도 실제 생성 순서와 동일하게(`Memory → GlobalQueue → JobTimer → DeadLockProfiler → ThreadManager`) 맞춰져 있습니다.

이 헤더 가드 게이팅은 3.4절의 `USE_GPMEMORY` 매크로와는 별개의 장치입니다 — `UC_MEMORY_H`는 "`gpMemory` 전역 변수 자체가 존재하는가"를 결정하고, `USE_GPMEMORY`는 "이미 존재하는 `gpMemory`를 `PoolAllocator`가 실제로 사용할 것인가"를 결정합니다.

`Destroy()`의 각 `delete gpX;` 뒤에는 `gpX = nullptr;`가 따라와, `if (gpX != nullptr)` 가드와 합쳐져 두 가지를 함께 보장합니다: `Init()`이 일부만 성공하고 중단된 상황에서도 안전하게 정리할 수 있고, `Destroy()`가 실수로 두 번 호출되더라도(두 번째 호출은 모든 가드에서 걸려 아무 것도 하지 않으므로) 이중 해제(double-free)로 이어지지 않습니다. 다만 `Init()` 쪽에는 대칭되는 가드가 없어, `Destroy()` 없이 `Init()`을 두 번 호출하면 이전 인스턴스가 `delete` 없이 덮어써져 누수됩니다 — `Init()`/`Destroy()`가 각각 정확히 한 번씩만 호출된다는 아래 전제조건 ①에 기대고 있는 부분입니다.

**2) `CMemory::FlushCurrentThreadCache()` — 메인 스레드용 수동 반납 진입점**

`thread_local` 소멸자는 "그 스레드가 실제로 종료될 때"만 호출되므로, 메인 스레드처럼 언제 종료될지 보장할 수 없는 스레드를 위해 동일한 반납 로직을 명시적으로 호출 가능한 정적 함수로 노출해 둡니다. `BaseGlobal::Destroy()`가 `gpMemory`를 delete하기 직전 이 함수를 호출합니다.

**3) `CThreadManager::DestroyTLS()`에서 명시적 flush — 이중 안전장치**

워커 스레드는 자연 종료 시 `TlsCache` 소멸자가 자동으로 캐시를 비워주지만, `DestroyTLS()`(콜백 실행 직후, 스레드 종료 직전 훅)에서도 `CMemory::FlushCurrentThreadCache()`를 명시적으로 한 번 더 호출합니다. 이렇게 하면:
① 캐시 반납 시점이 "콜백이 끝나는 순간"으로 코드상 명확히 고정되고,

② 여러 `thread_local` 객체 간의 암묵적인 소멸 순서에 대한 의존을 줄일 수 있습니다.

③ `CMemory` 모듈이 포함되지 않은 빌드에서도 `ThreadManager`가 독립적으로 컴파일되도록 `#ifdef UC_MEMORY_H` 가드로 감싸져 있습니다.

### 생성 순서

```
① gpMemory 생성  (다른 모든 전역 시스템이 내부적으로 xnew/xdelete를 쓸 수 있도록 가장 먼저)

② gpGlobalQueue 생성

③ gpJobTimer 생성

④ gpDeadLockProfiler 생성  (USE_GPDEADLOCKPROFILER && _DEBUG 빌드에서만)

⑤ gpThreadManager 생성
```

### 파괴 순서

```
① gpThreadManager 파괴  (워커 스레드 join → 각 워커의 TlsCache 자동 반납)

② gpDeadLockProfiler / gpJobTimer / gpGlobalQueue 파괴

③ CMemory::FlushCurrentThreadCache()  (메인 스레드 캐시 수동 반납)

④ gpMemory 파괴  (반드시 마지막)
```

①~④ 각 단계의 `delete`는 모두 `if (gpX != nullptr) { delete gpX; gpX = nullptr; }` 형태입니다. 이 파괴 순서는 위 "생성 순서"를 정확히 뒤집은 것(엄격한 LIFO)이라, 어떤 시스템도 자신보다 나중에 생성된 시스템이 이미 파괴된 상태에서 자기 소멸자를 실행하는 일이 없습니다.

### 전제조건 (코드로 자동 보장되지 않는 부분)

① `BaseGlobal::Init()`/`Destroy()`는 **항상 메인 스레드에서, 그리고 각각 한 번씩만** 호출된다는 전제 위에서 동작합니다. 다른 스레드에서 `Destroy()`를 호출하면 `FlushCurrentThreadCache()`가 엉뚱한 스레드의 캐시를 비우게 되어 의미가 없어집니다.

② 비정상 종료 경로(크래시, 강제 종료 시그널, `TerminateThread`)는 `Destroy()`를 거치지 않으므로 이 보장의 적용을 받지 않습니다. 다만 이 경우 프로세스 자체가 종료되며 OS가 메모리를 일괄 회수하므로 실무적 영향은 제한적입니다.

③ 모든 파일의 클래스명은 `CMemory`/`CMemoryPool`로 통일되어 있습니다(`gpMemory` 타입도 `CMemory*`). 파일명 자체는 프로젝트 관례(`ThreadManager.h` → `CThreadManager`처럼 클래스만 `C` 접두사를 붙이고 파일명은 접두사 없이 유지)에 맞춰 `Memory.h/cpp`, `MemoryPool.h/cpp`를 그대로 사용합니다.

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
| `USE_GPMEMORY` 빌드 분기 | `PoolAllocator`(및 `DrainBuckets`)가 이 매크로로 게이트되어 있어, 정의 여부에 따라 `xnew`/`StlAllocator`가 풀 경로를 탈지 표준 힙(`::operator new`/`delete`)으로 직접 갈지가 전체 빌드 단위로 갈림. 매크로를 잘못 끈 채 배포하면 풀링/락프리/TLS 캐시 이점이 조용히 전부 사라지므로 빌드 구성 관리가 필요함 |

## 7. 사용 가이드 요약

① **평소 코드**: `xnew`/`xdelete`, `MakeShared`, `StlAllocator`만 사용 — 내부적으로 전부 `PoolAllocator` 경로를 탐.

② **STL 컨테이너가 필요할 때**: `std::vector` 등을 직접 쓰지 않고 `Containers.h`의 `CVector`/`CList`/`CMap` 등을 사용 — 기본 할당자가 이미 `StlAllocator`로 연결되어 있어 풀 경로를 자동으로 탐(8절 참고).

③ **메모리 손상 디버깅**: 빌드 설정에서 `_STOMP` 정의 → 재현 → 크래시 지점의 콜스택으로 원인 추적 → 수정 후 다시 끔.

④ **raw 할당 라이브러리 변경**: `RawAllocator.h`를 건드릴 필요 없이, 원하는 라이브러리 헤더만 먼저 include하면 매크로 감지로 자동 전환됨.

⑤ **서버 시작 시점 지연 스파이크 완화**: 자주 쓰이는 주요 크기에 한해 `CMemory::WarmUp(allocDataSize, count)`를 선별적으로 호출.

⑥ **`CThreadManager` 밖에서 만든 스레드가 `PoolAllocator`를 쓸 경우**: 그 스레드 종료 직전 반드시 `CMemory::FlushCurrentThreadCache()`를 직접 호출.

⑦ **메모리 풀 시스템을 아직 연결하지 않은 빌드(단위 테스트 등)**: `USE_GPMEMORY`를 정의하지 않으면 `xnew`/`xdelete`/`StlAllocator`가 `gpMemory` 없이도 표준 힙으로 동작함(대신 풀링/락프리 이점은 없음).

⑧ **`shared_ptr`이 필요할 때**: 이미 `CObjectPool<Type>`(타입 전용 풀)을 쓰고 있는 타입이라면 `CObjectPool<Type>::MakeShared()`를 사용 — 객체는 전용 풀에 남고 컨트롤 블록만 별도로 할당됩니다(2회 할당). 그 외 일반 타입은 3.4절의 전역 `MakeShared<Type>()`을 사용 — 객체+컨트롤 블록이 `gpMemory` 공유 풀에서 1회에 할당됩니다.

### 7-1. BaseAllocator / PoolAllocator / CObjectPool\<Type\> 비교

| 항목 | `BaseAllocator` | `PoolAllocator`(`xnew`/`StlAllocator`/전역 `MakeShared`) | `CObjectPool<Type>` |
|---|---|---|---|
| 실제 위임 대상 | `RawAllocator`(mimalloc 등)에 직결 | `gpMemory`(`CMemory`)의 크기 구간별 공유 풀 | `Type` 전용 `CMemoryPool` 1개(Meyer's Singleton) |
| 풀링 | 없음 — 매번 raw 할당/해제 | 있음 — 블록 재사용 | 있음 — `Type` 전용 블록만 재사용 |
| 원자 연산 경감(TLS 배치 캐시) | 해당 없음 | 있음 — 스레드별 배치 충전으로 SLIST 경합 대폭 감소(5-1절) | 없음 — `Pop`/`Push`마다 SLIST 원자 연산 직접 호출 |
| 지원 정렬 | `alignas(32)` 이상 확장 정렬도 `operator new(size_t, align_val_t)`로 완전 지원 | `SLIST_ALIGNMENT`(16B) 초과 시 `AllocAligned`가 `alignment+8`바이트 오버헤드로 지원 | `alignof(Type) <= 16`만 지원(초과 시 컴파일 에러, 3.7절) |
| `shared_ptr` | 직접 미지원(`std::shared_ptr<T>(new T, ...)` 등을 직접 구성) | 전역 `MakeShared<Type>()` — `allocate_shared`로 객체+컨트롤 블록을 묶어 `gpMemory` 공유 풀에서 1회 할당 | `CObjectPool<Type>::MakeShared()` — 객체는 타입 전용 풀에서, 컨트롤 블록은 기본 힙에서 따로 할당(총 2회) |
| 적합한 사용처 | ① `gpThreadManager`/`gpJobTimer`처럼 프로세스 시작 시 단 한 번만 생성되는 전역 매니저/싱글턴 객체<br>② 로그인/DB 등 외부 I/O로 어차피 지연이 큰 요청의 컨텍스트 객체<br>③ 월드맵/존 데이터, 설정 테이블처럼 서버 기동 시 1회 로드 후 프로세스 종료까지 유지되는 대형 객체<br>④ 리플레이/로그 덤프처럼 크기가 크고 생성 빈도가 낮은 버퍼 | ① 세션 객체(`CSession`), 플레이어/엔티티 객체 등 접속~해제 동안 유지되는 일반 게임 객체<br>② 패킷 디스패치용 Job/이벤트 객체, RPC 요청 컨텍스트<br>③ `CVector`/`CMap` 등 `Containers.h` 기반 컬렉션(인벤토리, 스킬 목록, 몬스터 AI 상태 등)<br>④ 채팅/알림처럼 가변 크기 페이로드를 담는 `StlAllocator` 기반 버퍼 | ① 초당 수만~수십만 개씩 생성/파괴되는 I/O 이벤트 구조체(`CRioEvent`, IOCP `OVERLAPPED` 확장)<br>② 패킷 파싱마다 새로 만들어지는 송신 청크(`CSendBufferChunk`)<br>③ 몬스터 투사체, 파티클, 충돌 판정용 임시 객체처럼 틱마다 대량 생성/소멸하는 단일 타입<br>④ 매치메이킹 큐 노드처럼 동일 타입이 압도적으로 자주 오가는 자료구조의 노드 |
| 비용/트레이드오프 | 풀링 이점 없음(할당/해제가 항상 raw 경로) | 크기 구간 공유로 인한 내부 단편화, TLS 캐시 상주 메모리 | 타입별 전용 풀이라 풀 개수가 늘어날수록 관리 비용 증가, 멀티코어 경합이 그대로 남음 |

### 7-2. 사용 예시

**`BaseAllocator`**

```cpp
// ① 정적 메서드 직접 호출 — raw 버퍼가 그때그때 필요할 때
void* buf = BaseAllocator::Alloc(1024);
BaseAllocator::Release(buf);

// ② 상속(믹스인) — new/delete가 자동으로 RawAllocator 경유
class CJobTimer : public BaseAllocator
{
    // ...
};

CJobTimer* timer = new CJobTimer();   // 풀을 거치지 않고 raw 직결
delete timer;
```

**`PoolAllocator`(`xnew`/`StlAllocator`/전역 `MakeShared`)**

```cpp
// xnew/xdelete — gpMemory의 크기 구간별 공유 풀 경유
CSession* session = xnew<CSession>(socket);
xdelete(session);

// StlAllocator 기반 컨테이너(Containers.h) — 기본 할당자가 이미 PoolAllocator 경로
CVector<int32> inventory;
inventory.push_back(itemId);

// 전역 MakeShared — 객체+컨트롤 블록을 gpMemory 공유 풀에서 1회 할당
std::shared_ptr<CPlayer> player = MakeShared<CPlayer>(playerId);
```

**`CObjectPool<Type>`**

```cpp
// 타입 전용 풀에서 Pop/Push — 초당 수만~수십만 회 생성/파괴되는 타입에 적합
CRioEvent* ev = CObjectPool<CRioEvent>::Pop();
// ... ev 사용 ...
CObjectPool<CRioEvent>::Push(ev);

// 타입 전용 풀 기반 shared_ptr(3.7절 ⑦) — 소멸 시 자동으로 Push() 호출
std::shared_ptr<CSendBufferChunk> chunk = CObjectPool<CSendBufferChunk>::MakeShared();
```

## 8. Containers.h — STL 컨테이너 래퍼

표준 STL 컨테이너(`std::vector`, `std::map` 등)를 그대로 쓰면 기본 할당자가 전역 힙(`new`/`delete` 경유)이라 이 프로젝트의 풀 시스템을 타지 않습니다. `Containers.h`는 각 표준 컨테이너를 상속한 얇은 래퍼 클래스(`CVector`, `CList`, `CForwardList`, `CDeque`, `CQueue`, `CPriorityQueue`, `CStack`, `CSet`, `CMap`, `CMultiMap`, `CUnorderedSet`, `CUnorderedMap`)를 제공하며, 기본 템플릿 인자로 `StlAllocator<T>`를 지정해 둡니다. 그래서 `std::vector<T>` 대신 `CVector<T>`를 쓰기만 하면 내부 노드/버퍼 할당이 자동으로 `PoolAllocator` → `gpMemory`의 풀 경로를 타게 됩니다.

**구현 방식**: 각 래퍼는 대응하는 표준 컨테이너를 `public` 상속하고, `using Base::Base;`로 생성자만 그대로 끌어옵니다. 컨테이너 어댑터(`CQueue`, `CStack`)는 기본 내부 컨테이너로 `CDeque`(이미 `StlAllocator`가 연결된 래퍼)를 사용해, 어댑터 자신뿐 아니라 내부 컨테이너까지 풀 경로를 타도록 이어줍니다. `CPriorityQueue`만 클래스가 아니라 `using` 별칭인 이유는, `std::priority_queue`는 상속받아 얇게 감싸더라도 생성자 상속으로 얻을 실익이 적고(내부적으로 `make_heap` 등 별도 초기화가 필요해 단순 `using Base::Base`로는 어댑터들만큼 자연스럽게 감싸지지 않음), 별칭만으로 충분하기 때문입니다.

**사용법**: 각 클래스는 대응하는 `std::` 컨테이너와 사용법이 완전히 동일합니다(반복자, 멤버 함수 등 전부 상속됨). 커스텀 할당자가 필요 없는 일반적인 경우 템플릿 인자를 생략하면 자동으로 `StlAllocator`가 적용됩니다.

```cpp
CVector<int> v;              // std::vector<int, StlAllocator<int>>와 동일하게 동작
CMap<int, Player*> players;  // std::map<int, Player*, less<int>, StlAllocator<pair<const int, Player*>>>
```

## 9. API 레퍼런스 (함수 / 파라미터 / 멤버 변수)

파라미터가 여러 개인 함수는 파라미터마다 별도의 행으로 나누고, 이어지는 파라미터 행은 `함수`/`반환값`/`함수 설명` 칸을 비워 하나의 함수에 속함을 표시합니다. 파라미터 칸에는 자료형과 변수명을 함께 적습니다(예: `size_t size`).

### 9.1 RawAllocator (네임스페이스)

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `IsPowerOfTwo` | `size_t alignment` | 검사할 정렬 값(바이트 단위) | `bool` | `alignment`가 2의 거듭제곱인지 검사(0은 `false`) |
| `Alloc` | `size_t size` | 할당할 바이트 크기 | `void*` (실패 시 `nullptr` 가능) | 정렬 요구 없는 raw 메모리 할당. 매크로에 따라 mimalloc/jemalloc/tcmalloc/malloc으로 컴파일 타임 분기 |
| `Free` | `void* ptr` | 해제할 포인터 | `void` | `Alloc`으로 받은 메모리 해제. `ptr == nullptr`이면 즉시 반환 |
| `AllocAligned` | `size_t size` | 할당할 바이트 크기 | `void*` (실패 시 `nullptr` 가능) | 정렬된 raw 메모리 할당. 라이브러리별 인자 순서 차이를 내부에서 흡수. 아무 라이브러리도 안 쓰고 Windows도 아닌 최후 C11 fallback은 `aligned_alloc`(size가 alignment의 배수여야 하는 제약으로 내부 단편화 발생) 대신 그런 제약이 없는 `posix_memalign`을 사용해 요청한 만큼만 정확히 할당 |
| | `size_t alignment` | 정렬 기준(바이트 단위, 2의 거듭제곱) | | |
| `FreeAligned` | `void* ptr` | 해제할 포인터 | `void` | `AllocAligned`로 받은 메모리 해제(플랫폼별 전용 해제 함수 사용) |

### 9.2 BaseAllocator

멤버 변수 없음(가상 함수도 없어 상속 오버헤드 없음).

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `static void* Alloc` | `size_t size` | 할당 바이트 크기 | `void*` | `RawAllocator::Alloc`에 위임 |
| `static void Release` | `void* ptr` | 해제할 포인터 | `void` | `RawAllocator::Free`에 위임 |
| `operator new` | `size_t size` | 할당할 바이트 크기 | `void*` | 단일 객체 할당 (RawAllocator 경유) |
| `operator new[]` | `size_t size` | 할당할 바이트 크기 | `void*` | 배열 할당 (RawAllocator 경유) |
| `operator delete` | `void* ptr` | 해제할 메모리 포인터 | `void` | 단일 객체 해제 |
| `operator delete[]` | `void* ptr` | 해제할 메모리 포인터 | `void` | 배열 해제 |
| `operator new` (placement) | `size_t`(이름 없음, 미사용) | 객체 크기, 사용되지 않음 | `void*` | 전달받은 `ptr` 그대로 반환 (`xnew` 호환용) |
| | `void* ptr` | 이미 할당되어 전달된 메모리 포인터 | | |
| `operator delete` (placement) | `void*`(이름 없음, 첫 번째) | 미사용 | `void` | 생성자 예외 시 컴파일러가 호출하는 짝 연산자, 본문 없음 |
| | `void*`(이름 없음, 두 번째) | 미사용 | | |
| `operator new` (확장 정렬) | `size_t size` | 할당할 바이트 크기 | `void*` | `RawAllocator::AllocAligned`로 위임 |
| | `std::align_val_t alignment` | 정렬 바이트 단위 | | |
| `operator new[]` (확장 정렬) | `size_t size` | 할당할 바이트 크기 | `void*` | 배열 버전 |
| | `std::align_val_t alignment` | 정렬 바이트 단위 | | |
| `operator delete` (확장 정렬) | `void* ptr` | 해제할 메모리 포인터 | `void` | `RawAllocator::FreeAligned`로 위임 |
| | `std::align_val_t alignment`(미사용) | 정렬 정보는 해제 시 불필요 | | |
| `operator delete[]` (확장 정렬) | `void* ptr` | 해제할 메모리 포인터 | `void` | 배열 버전 |
| | `std::align_val_t alignment`(미사용) | 정렬 정보는 해제 시 불필요 | | |

### 9.3 StompAllocator

**상수/enum**

| 이름 | 값 | 설명 |
|---|---|---|
| `PAGE_SIZE` | `0x1000` | 페이지 크기(4096바이트) |
| `ALIGNMENT` | `16` | `AllocationHeader`/사용자 데이터 배치 정렬 기준(바이트) |
| `ARENA_RESERVE_SIZE` | `256 * 1024 * 1024 * 1024` (256GB) | 예약할 가상 주소 공간 크기 |
| `ALLOCATION_MAGIC` | `0x5543414C4C4F4341` ("UCALLOCA") | `AllocationHeader::magic` 오염/이중 해제 탐지용 매직 넘버 |

**정적 멤버 변수**

| 변수 | 타입 | 설명 |
|---|---|---|
| `s_arenaBase` | `int8*` | 아레나 시작 주소 |
| `s_arenaCursor` | `atomic<int8*>` | 현재 커밋 커서 오프셋 |
| `s_arenaInitFlag` | `once_flag` | 아레나 1회 초기화 플래그 |
| `s_sizeClassMapLock` | `shared_mutex` | Free-List 맵 동기화 락 |
| `s_sizeClassFreeLists` | `unordered_map<size_t, unique_ptr<SLIST_HEADER>>` | 데이터 크기별 Lock-free Free-List 맵 |
| `s_isCleanedUp` | `atomic<bool>` | `Cleanup()` 호출 여부. `Cleanup()`이 `true`로 설정하며, 이후 `Alloc`/`Release`/중복 `Cleanup` 호출을 `ASSERT_CRASH`로 차단 |

**내부 구조체 `AllocationHeader`(사용자 포인터 바로 앞에 배치)**

| 필드 | 타입 | 설명 |
|---|---|---|
| `meta` | `RegionMeta*` | 이 할당이 속한 영역의 메타데이터(힙 할당) 포인터 |
| `magic` | `uint64` | `ALLOCATION_MAGIC` 값으로 채워지는 오염/유효성 탐지용 매직 넘버 |

**내부 구조체 `RegionMeta`(`SLIST_ENTRY` 상속, `RawAllocator`로 힙에서 할당)**

| 필드 | 타입 | 설명 |
|---|---|---|
| `dataPagesBase` | `int8*` | 데이터 페이지 영역 시작 주소 |
| `dataRegionSize` | `size_t` | 데이터 영역 크기(PAGE_SIZE 단위) |
| `freed` | `atomic<int32>` | 0=사용 중, 1=반납됨(이중 해제 탐지) |
| `isFallback` | `bool` | 아레나 고갈로 Direct `VirtualAlloc`한 Fallback 영역인지 여부 |

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `static void* Alloc` | `size_t size` | 요청 데이터 크기 | `void*` | 데이터 페이지 끝단 + 전용 가드 페이지로 보호되는 디버그 메모리 할당 |
| `static void Release` | `void* ptr` | 반납할 포인터 | `void` | 디버그 메모리 반납(일반: 데이터 페이지 DECOMMIT + Free-List 등록 / Fallback: 전체 `VirtualFree` + `RegionMeta` 힙 해제) |
| `static void EnsureArenaInitialized` | 없음 | — | `void` | 아레나 공간을 `call_once`로 최초 1회 예약 |
| `static SLIST_HEADER* GetOrCreateSizeClassFreeList` | `size_t dataRegionSize` | 데이터 영역 크기 | `SLIST_HEADER*` | 크기별 Free-List 헤더 조회/생성(이중 확인 잠금) |
| `static int8* ReserveArenaRegion` | `size_t size` | 예약받고자 하는 총 영역 크기(가드 페이지 포함) | `int8*`(고갈 시 `nullptr`) | 아레나 범프 커서를 CAS로 원자적으로 전진시켜 영역 예약 |
| `static void Cleanup` | 없음 | — | `void` | 각 크기별 free-list에 남은 `RegionMeta`를 모두 힙에서 해제한 뒤 아레나 전체를 `VirtualFree`. 이후 재사용 불가 |

### 9.4 PoolAllocator

멤버 변수 없음.

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `static void* Alloc` | `size_t size` | 요청 크기 | `void*` | `USE_GPMEMORY` 정의 시 `gpMemory->Allocate`, 아니면 `::operator new`로 폴백 |
| `static void* AllocAligned` | `size_t size` | 요청 크기 | `void*` | `USE_GPMEMORY` 정의 시 `gpMemory->Allocate`에 정렬 여유분+원본 포인터 저장 공간을 얹어 할당한 뒤 반환 주소를 정렬에 맞춰 올림 보정(gpMemory 풀 경유), 아니면 `::operator new(size, align_val_t)`로 폴백 |
| | `size_t alignment` | 요구 정렬 바이트 단위(2의 거듭제곱) | | |
| `static void Release` | `void* ptr` | 반납할 포인터 | `void` | `USE_GPMEMORY` 정의 시 `gpMemory->Release`, 아니면 `::operator delete`로 폴백 |
| `static void ReleaseAligned` | `void* ptr` | 반납할 포인터 | `void` | `USE_GPMEMORY` 정의 시 `ptr` 앞 8바이트에 저장된 원본 포인터를 역산해 `gpMemory->Release`, 아니면 `::operator delete(ptr, align_val_t)`로 폴백 |
| | `size_t alignment` | 할당 시 사용한 정렬 바이트 단위 | | |

### 9.5 StlAllocator\<T\>

상태 없음(멤버 변수 없음).

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `StlAllocator()` | 없음 | — | — | 기본 생성자 |
| `StlAllocator(const StlAllocator<Other>&)` | `const StlAllocator<Other>& other` | 다른 타입의 `StlAllocator` 인스턴스 | — | 리바인딩 변환 생성자 |
| `operator=` | `const StlAllocator<Other>& other` | 다른 타입의 `StlAllocator` 인스턴스 | `StlAllocator<T>&` | 리바인딩 대입 연산자 |
| `T* allocate` | `size_t count` | 원소 개수 | `T*` | `count * sizeof(T)` 오버플로 검사(`bad_array_new_length` throw) 후, `alignof(T) > alignof(std::max_align_t)`면 `PoolAllocator::AllocAligned(size, alignof(T))`, 아니면 `PoolAllocator::Alloc(size)` |
| `void deallocate` | `T* ptr` | 해제할 메모리 포인터 | `void` | `ptr == nullptr`이면 즉시 반환. `alignof(T) > alignof(std::max_align_t)`면 `PoolAllocator::ReleaseAligned(ptr, alignof(T))`, 아니면 `PoolAllocator::Release(ptr)` |
| | `size_t count`(미사용) | 원소 개수(`PoolAllocator::Release`가 크기를 별도 저장하므로 불필요) | | |
| `operator==` / `operator!=` | `const StlAllocator<U>& other` | 비교 대상 `StlAllocator` 인스턴스 | `bool` | 무상태이므로 항상 `true`/`false` 고정 반환 |

### 9.6 전역 유틸리티 함수 (xnew / xdelete / MakeShared)

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `xnew<Type>` | `Args&&... args` | 생성자에 전달할 가변 인자 | `Type*` | `alignof(Type) > alignof(std::max_align_t)`면 `PoolAllocator::AllocAligned(sizeof(Type), alignof(Type))`, 아니면 `PoolAllocator::Alloc(sizeof(Type))` 후 전역 placement `::new`로 생성자 호출. 생성자 예외 시 짝이 되는 `Release`/`ReleaseAligned`로 메모리를 되돌리고 재throw(Exception-Safe) |
| `xdelete<Type>` | `Type* obj` | 파괴할 객체 포인터 | `void` | `nullptr`이면 무시. 소멸자 호출 후, `xnew`가 할당했던 경로에 맞춰 `PoolAllocator::Release` 또는 `ReleaseAligned`로 반납 |
| `MakeShared<Type>` | `Args&&... args` | 생성자에 전달할 가변 인자 | `shared_ptr<Type>` | `std::allocate_shared<Type>(StlAllocator<Type>(), args...)` 기반. 객체+제어 블록을 하나로 묶어 `gpMemory` 공유 풀에서 단일 할당 |

### 9.7 MemoryHeader (`SLIST_ENTRY` 상속)

**멤버 변수**

| 변수 | 타입 | 설명 |
|---|---|---|
| `allocSize` | `atomic<size_t>` | 헤더 포함 전체 할당 크기. `0`이면 "풀에 반납된 상태"를 의미(이중 반납 탐지 플래그 겸용) |
| (상속) `Next` | `PSLIST_ENTRY`(`SLIST_ENTRY` 상속분) | SLIST/TLS 로컬 free-list 연결 포인터로 재사용 |

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `MemoryHeader(size_t size)` | `size_t size` | 헤더 포함 전체 크기 | — | `allocSize`를 `size`로 초기화하는 생성자 |
| `static void* AttachHeader` | `MemoryHeader* header` | 헤더를 얹을 원시 메모리 시작 주소 | `void*` | raw 메모리에 헤더를 placement new로 얹고 데이터 포인터 반환 |
| | `size_t size` | 헤더를 포함한 전체 할당 크기 | | |
| `static MemoryHeader* DetachHeader` | `void* ptr` | 데이터 포인터 | `MemoryHeader*` | 데이터 포인터에서 헤더 주소를 역산 |

### 9.8 CMemoryPool

**멤버 변수**

| 변수 | 타입 | 설명 |
|---|---|---|
| `_header` | `SLIST_HEADER` | Lock-free SLIST 헤더(Interlocked API로 조작) |
| `_allocSize` | `size_t` | 이 풀이 다루는 고정 블록 크기 |
| `_poolCheckedOutCount` | `atomic<int32>` | 이 풀의 SLIST 밖으로 나간(반납되지 않은) 블록 수(통계). TLS 캐시 대기분 포함 — `_header`와 다른 캐시 라인에 정렬 |
| `_poolReserveCount` | `atomic<int32>` | 현재 이 풀의 SLIST에 대기 중인 블록 수(통계) |

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `CMemoryPool(size_t allocSize)` | `size_t allocSize` | 고정 블록 크기 | — | `_allocSize` 설정, SLIST 헤더 초기화 |
| `~CMemoryPool()` | 없음 | — | — | SLIST에 남은 블록을 모두 꺼내 raw 해제 |
| `void Push` | `MemoryHeader* ptr` | 반납할 블록 | `void` | `allocSize=0` 표시 후 SLIST에 반납, `_poolCheckedOutCount--`/`_poolReserveCount++` |
| `void WarmUpPush` | `MemoryHeader* ptr` | 워밍업으로 채워넣을 블록 | `void` | `allocSize=0` 표시 후 SLIST에 반납, `_poolReserveCount++`만 수행(`_poolCheckedOutCount`는 건드리지 않음 — 한 번도 나간 적 없는 블록이므로) |
| `MemoryHeader* Pop` | 없음 | — | `MemoryHeader*` | SLIST에서 꺼내거나(비었으면 raw 신규 할당), `_poolCheckedOutCount++`(재사용 시 `_poolReserveCount--`) |
| `size_t GetBlockSize` | 없음 | — | `size_t` | 이 풀이 다루는 고정 블록 크기(`_allocSize`) 반환 |

### 9.9 CMemory

**enum**

| 이름 | 값 | 설명 |
|---|---|---|
| `POOL_COUNT` | `(1024/32)+(1024/128)+(2048/256)` | 생성되는 `CMemoryPool` 총 개수 |
| `MAX_ALLOC_SIZE` | `4096` | 이 크기를 초과하면 풀을 쓰지 않고 raw 할당 |

**멤버 변수**

| 변수 | 타입 | 설명 |
|---|---|---|
| `_pools` | `vector<CMemoryPool*>` | 생성된 모든 `CMemoryPool` 인스턴스 목록 |
| `_tlsBatchSizeTable` | `int16[POOL_COUNT]` | 풀 인덱스별 TLS 배치 충전 개수 |
| `_tlsMaxCountTable` | `int16[POOL_COUNT]` | 풀 인덱스별 TLS 로컬 캐시 상한 |
| `_tlsCache` (static thread_local) | `TlsCache` | 스레드마다 독립적인 로컬 캐시 인스턴스 |
| `_liveAllocationCount` (static) | `atomic<int64>` | `Allocate()`/`Release()` 호출 시점에만 직접 증감되는 실제 살아있는 할당 수(TLS 캐싱과 무관하게 정확) |

**중첩 구조체 `TlsBucket`**

| 필드 | 타입 | 설명 |
|---|---|---|
| `freeList` | `MemoryHeader*` | 로컬 free-list 시작 노드 |
| `count` | `int32` | 현재 로컬에 쌓인 블록 수 |

**중첩 구조체 `TlsCache`**

| 필드/함수 | 타입 | 설명 |
|---|---|---|
| `buckets` | `TlsBucket[POOL_COUNT]` | 풀 인덱스별 로컬 버킷 배열 |
| `~TlsCache()` | — | 스레드 종료 시 자동 호출, 남은 블록을 전역 풀로 반납(`DrainBuckets`) |

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `CMemory()` | 없음 | — | — | 3개 구간(32/128/256단위)의 `CMemoryPool` 생성 및 TLS 배치/상한 테이블 계산 |
| `~CMemory()` | 없음 | — | — | 모든 `CMemoryPool` delete |
| `void* Allocate` | `size_t size` | 요청 데이터 크기(헤더 제외) | `void*` | 헤더 포함 크기 계산 후 `_STOMP`/대형/TLS 경로로 분기해 할당 |
| `void Release` | `void* ptr` | `Allocate`가 반환한 포인터 | `void` | 헤더 역산 후 `_STOMP`/대형/TLS 경로로 분기해 반납 |
| `void WarmUp` | `size_t allocDataSize` | 워밍업할 풀의 데이터 크기(헤더 제외) | `void` | 지정 크기 풀에 raw 블록 `count`개 미리 채워 넣음 |
| | `size_t count` | 미리 채워 넣을 블록 개수 | | |
| `static void FlushCurrentThreadCache` | 없음 | — | `void` | 호출 스레드의 TLS 캐시를 즉시 전역 풀로 반납 |
| `static int64 GetLiveAllocationCount` | 없음 | — | `int64` | `Allocate()`했지만 아직 `Release()`되지 않은 블록 수(TLS 캐시 대기분 제외, 실제 살아있는 할당만) |
| `static int32 ComputePoolIndex` | `size_t allocSize` | 헤더 포함 전체 크기 | `int32` | 분기 없는 수식으로 `_pools`/`TlsCache::buckets` 인덱스 계산 |
| `static int32 DetermineTlsBatchSize` | `int32 allocSize` | 헤더 포함 전체 블록 크기 | `int32` | 크기 구간별 TLS 배치 충전 개수 결정(64/32/4) |
| `static int32 DetermineTlsMaxCount` | `int32 allocSize` | 헤더 포함 전체 블록 크기 | `int32` | `DetermineTlsBatchSize`의 4배로 로컬 캐시 상한 결정 |
| `static void DrainBuckets` | `TlsBucket* buckets` | 비워낼 `TlsBucket` 배열(크기 `POOL_COUNT`) | `void` | 버킷에 남은 블록을 원래 속했던 전역 풀로 반납(`gpMemory` null 체크 포함) |

### 9.10 CObjectPool\<Type\>

**정적 멤버 변수**

| 변수 | 타입 | 설명 |
|---|---|---|
| `s_allocSize` | `static constexpr size_t` | `AlignUp(sizeof(Type) + sizeof(MemoryHeader), SLIST_ALIGNMENT)` — Type 하나(헤더 포함) 당 블록 크기를 16바이트 단위로 올림한 값 |

**함수**

| 함수 | 파라미터 | 설명 | 반환값 | 함수 설명 |
|---|---|---|---|---|
| `static Type* Pop` | `Args&&... args` | 생성자에 전달할 가변 인자 | `Type*` | 타입 전용 풀(또는 `_STOMP`)에서 블록을 받아 placement new로 객체 생성 |
| `static void Push` | `Type* obj` | 파괴할 객체 포인터 | `void` | 이중 반납 원자적 확인 후 소멸자 호출, 타입 전용 풀에 반납 |
| `static shared_ptr<Type> MakeShared` | `Args&&... args` | 생성자에 전달할 가변 인자 | `shared_ptr<Type>` | `Pop(args...)`로 객체를 만든 뒤, 소멸자 대신 `CObjectPool<Type>::Push`를 커스텀 deleter로 쓰는 `shared_ptr` 구성. `allocate_shared`를 쓰지 않으므로 객체(타입 전용 풀)와 컨트롤 블록(기본 힙) 할당이 분리되어 2회 발생 |
| `static constexpr size_t AlignUp` (private) | `size_t size` | 올림 대상 크기 | `size_t` | `size`를 `alignment`의 배수로 올림(`alignment`는 2의 거듭제곱) |
| | `size_t alignment` | 정렬 단위 | | |
| `static CMemoryPool& GetPool` (private) | 없음 | — | `CMemoryPool&` | Meyer's Singleton으로 타입 전용 풀 지연 초기화 |

### 9.11 Containers.h — 컨테이너 래퍼 목록

| 클래스 | 감싸는 표준 컨테이너 | 기본 할당자 템플릿 인자 | 비고 |
|---|---|---|---|
| `CVector<T, Ax>` | `std::vector` | `StlAllocator<T>` | `using Base::Base`로 생성자만 상속 |
| `CList<T, Ax>` | `std::list` | `StlAllocator<T>` | 〃 |
| `CForwardList<T, Ax>` | `std::forward_list` | `StlAllocator<T>` | 〃 |
| `CDeque<T, Ax>` | `std::deque` | `StlAllocator<T>` | 〃 |
| `CQueue<T, Container>` | `std::queue` | 내부 컨테이너 기본값 `CDeque<T>` | 어댑터 자신 + 내부 컨테이너 모두 풀 경로 |
| `CPriorityQueue<T, Container, Pr>` | `std::priority_queue` | 내부 컨테이너 기본값 `CVector<T>` | 클래스가 아닌 `using` 별칭(생성자 상속 불필요) |
| `CStack<T, Container>` | `std::stack` | 내부 컨테이너 기본값 `CDeque<T>` | 어댑터 |
| `CSet<Kty, Pr, Alloc>` | `std::set` | `StlAllocator<Kty>` | — |
| `CMap<Kty, T, Pr, Alloc>` | `std::map` | `StlAllocator<pair<const Kty, T>>` | — |
| `CMultiMap<Kty, T, Pr, Alloc>` | `std::multimap` | `StlAllocator<pair<const Kty, T>>` | — |
| `CUnorderedSet<Kty, Hasher, Keyeq, Alloc>` | `std::unordered_set` | `StlAllocator<Kty>` | — |
| `CUnorderedMap<Kty, T, Hasher, Keyeq, Alloc>` | `std::unordered_map` | `StlAllocator<pair<const Kty, T>>` | — |
