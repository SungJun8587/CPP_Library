
//***************************************************************************
// Allocator.h
//
// 설명 : 프로젝트에서 사용하는 세 가지 할당 전략(Strategy 패턴)과
//        STL/커스텀 new-delete 연동 유틸리티를 정의합니다.
//
//        - BaseAllocator  : RawAllocator 기반 순수 할당 (풀 시스템 부트스트랩용)
//        - StompAllocator : 메모리 오버런 탐지용 디버그 전용 할당자
//        - PoolAllocator  : 실서비스 핫패스에서 사용하는 풀 기반 할당자
//
//        평상시 게임 로직 코드는 PoolAllocator(→ xnew/xdelete/StlAllocator)만
//        사용하면 되고, BaseAllocator/StompAllocator는 각각 특수한 상황
//        (부트스트랩, 디버깅)에서만 내부적으로 쓰입니다.
//***************************************************************************

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#pragma once

#include "RawAllocator.h"
#include <new>           // std::align_val_t (BaseAllocator의 확장 정렬 new/delete)
#include <unordered_map> // StompAllocator의 크기별 free-list 맵
#include <shared_mutex>  // StompAllocator의 크기별 free-list 맵 보호(읽기 다중/쓰기 단일 락)

/*-------------------
	BaseAllocator

	설명 : RawAllocator(mimalloc/jemalloc/tcmalloc/malloc 중 컴파일 타임에
		   선택된 라이브러리)에 그대로 위임하는 최하위 계층 할당자.
		   CMemory/CMemoryPool 등 메모리 모듈 자체는 이 클래스를 상속받지
		   않으며(부트스트랩 객체는 단순함을 위해 그대로 전역 new/delete를
		   사용), 이 클래스는 메모리 모듈과 무관한 다른 기능 클래스들이
		   "풀을 거치지 않는 raw 할당 경로"가 필요할 때 가져다 쓰는 범용
		   유틸리티 성격의 클래스입니다.

		   두 가지 방식으로 쓸 수 있습니다.
		   1) 정적 메서드 직접 호출 : BaseAllocator::Alloc(size) /
			  BaseAllocator::Release(ptr) - raw 버퍼가 필요할 때
		   2) 상속(믹스인)          : class SomeClass : public BaseAllocator
			  형태로 상속받으면, 클래스 내부에 정의된 operator new/delete가
			  자동으로 RawAllocator 경유로 오버라이드됩니다. 이렇게 하면
			  "new SomeClass()"처럼 평범하게 쓴 코드도 별도 API 없이 raw
			  할당 경로를 타게 됩니다. 고빈도로 생성/파괴되는 객체보다는,
			  크기가 크거나 드물게 생성되는 객체를 풀과 분리하고 싶을 때
			  적합합니다.
			  BaseAllocator는 데이터 멤버가 없고 상속받는 함수들도 모두
			  non-virtual이라 vptr 등 추가 오버헤드 없이 상속됩니다.

		   [주의] xnew<T>()는 내부적으로 placement new(new(memory)Type(...))
		   를 사용합니다. 클래스가 operator new(size_t)를 멤버로 선언하면
		   컴파일러가 그 클래스 스코프의 전역 placement new를 가려버리므로,
		   BaseAllocator를 상속한 클래스를 xnew로 생성하려 하면 컴파일
		   에러가 납니다. 이를 막기 위해 placement new/delete 오버로드도
		   함께 정의하여 원래의 전역 placement new와 동일하게 동작하도록
		   유지합니다. (다만 애초에 BaseAllocator 상속은 풀을 거치지 않는
		   raw 경로를 원할 때 쓰는 것이므로, 그런 클래스를 xnew로 생성할
		   일 자체가 거의 없습니다 - 방어적으로만 남겨둠)

		   [확장 정렬(over-alignment) 지원] `alignas(32)` 이상(SIMD용
		   AVX 데이터 등)으로 선언되어 기본 정렬(보통 16바이트)을 넘어서는
		   클래스가 상속받는 경우를 위해, C++17 확장 정렬 오버로드
		   (`operator new(size_t, align_val_t)` 등)도 함께 정의합니다.
		   컴파일러는 `alignof(T)`가 `__STDCPP_DEFAULT_NEW_ALIGNMENT__`를
		   넘으면 이 오버로드를 자동으로 선택하며, 이 오버로드가 없으면
		   컴파일은 되지만 정렬 정보가 유실된 채 일반 `operator new(size_t)`
		   로 조용히 폴백되어 반환된 메모리가 실제로는 요청한 정렬을
		   만족하지 못하는(경고 없는 미정의 동작) 상황이 생길 수 있습니다.
		   이 오버로드는 `RawAllocator::AllocAligned`로 위임해 요청한
		   정렬을 실제로 보장합니다.
-------------------*/
class BaseAllocator
{
public:
	// 설명 : raw 메모리를 size 바이트만큼 할당합니다.
	// 매개변수 : size - 할당할 바이트 크기
	// 반환값   : 할당된 메모리 포인터
	static void* Alloc(int32 size);

	// 설명 : Alloc()으로 할당된 메모리를 해제합니다.
	// 매개변수 : ptr - 해제할 메모리 포인터
	static void		Release(void* ptr);

	// 설명 : 이 클래스(및 상속받는 파생 클래스)의 operator new/delete를
	//        RawAllocator 경유로 오버라이드합니다. 이 클래스를 상속받은
	//        기능 클래스는 "new SomeClass()" 같은 평범한 코드가 자동으로
	//        raw 할당 경로를 타게 됩니다.
	void* operator new(size_t size)
	{
		return RawAllocator::Alloc(size);
	}

	void* operator new[](size_t size)
	{
		return RawAllocator::Alloc(size);
	}

	void operator delete(void* ptr)
	{
		RawAllocator::Free(ptr);
	}

	void operator delete[](void* ptr)
	{
		RawAllocator::Free(ptr);
	}

	// placement new/delete : xnew 등에서 사용하는 배치 생성 구문과의
	// 호환성을 위해 전역 placement 버전과 동일하게 동작하도록 유지
	void* operator new(size_t, void* ptr)
	{
		return ptr;
	}

	void operator delete(void*, void*)
	{
		// placement delete는 생성자 예외 시에만 컴파일러가 호출하며,
		// 별도로 해제할 메모리가 없으므로 아무 동작도 하지 않음
	}

	// 설명 : C++17부터 컴파일러는, 타입의 정렬 요구사항(alignof(T))이
	//        __STDCPP_DEFAULT_NEW_ALIGNMENT__(보통 16바이트)를 넘으면
	//        일반 operator new(size_t) 대신 이 확장 정렬 버전을 우선
	//        찾습니다. 이 오버로드가 없으면 컴파일 자체는 되지만, 정렬
	//        정보가 유실된 채 일반 operator new(size_t)로 조용히
	//        폴백되어 반환된 메모리가 실제로는 요청한 정렬을 만족하지
	//        못할 수 있습니다. AVX(32/64바이트) 등 SIMD 데이터를 멤버로
	//        갖는, alignas(32) 이상으로 선언된 클래스가 BaseAllocator를
	//        상속받으면 이 상황에 해당하며, 결과는 컴파일 경고나 에러
	//        없이 발생하는 정렬 위반 미정의 동작(SIMD 命令에서의 크래시
	//        등)이라 원인을 찾기 어렵습니다. 이 오버로드를 명시적으로
	//        제공해 RawAllocator::AllocAligned로 요청한 정렬을 실제로
	//        보장합니다.
	void* operator new(size_t size, align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	void* operator new[](size_t size, align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	void operator delete(void* ptr, align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

	void operator delete[](void* ptr, align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}
};

/*-------------------
	StompAllocator

	설명 : 메모리 오버런(buffer overrun) 및 오버라이트 버그를 그 자리에서
		   크래시로 잡아내기 위한 디버그 전용 할당자.

		   페이지 단위(PAGE_SIZE)로 메모리를 예약하고, 실제 데이터를
		   페이지의 "끝"에 딱 맞춰 배치합니다. 이렇게 하면 할당된
		   크기를 단 1바이트라도 초과해서 쓰는 순간, 다음 미매핑
		   (커밋되지 않은) 페이지에 접근하게 되어 즉시 Access Violation이
		   발생합니다. 즉 오버런이 실제로 발생한 "그 지점"에서 바로
		   크래시가 나므로 콜스택으로 원인을 빠르게 추적할 수 있습니다.

		   CMemory::Allocate/Release 내부의 _STOMP 매크로 분기를 통해서만
		   활성화되며, 평소 빌드에서는 사용되지 않습니다.

		   [아레나 기반 구현] 매 Alloc/Release마다 VirtualAlloc(MEM_RESERVE
		   | MEM_COMMIT)과 VirtualFree(MEM_RELEASE)를 왕복하면, 통합
		   테스트처럼 _STOMP를 켠 채로 할당이 매우 빈번히 일어나는
		   상황에서 매번 "가상 주소 공간 예약"이라는 상대적으로 비용이
		   큰 커널 작업이 반복됩니다. 이를 줄이기 위해 프로세스당 큰
		   가상 주소 공간(ARENA_RESERVE_SIZE)을 최초 1회만 예약해 두고,
		   이후 각 Alloc은 이미 예약된 공간의 일부를 커밋만 하고, Release는
		   그 부분만 디커밋합니다.

		   [영역 구조 : 메타데이터 1페이지 + 데이터 N페이지]
		   각 할당은 [메타데이터 1페이지][데이터 페이지들] 형태로 이루어져
		   있습니다. 데이터 페이지에 쓰는 실제 사용자 데이터는 항상
		   첫 데이터 페이지 안(오프셋이 PAGE_SIZE 미만)에서 시작해 마지막
		   데이터 페이지의 끝에서 정확히 끝나므로, Release가 데이터
		   포인터의 페이지 시작 주소를 역산하면 언제나 "첫 데이터 페이지의
		   시작"이 나오고, 그 바로 앞 페이지가 메타데이터 페이지입니다.
		   이 성질 덕분에 크기 조회를 위한 별도의 해시맵 조회가 필요
		   없습니다 - 포인터 산술만으로 메타데이터를 즉시 찾습니다.

		   메타데이터 페이지에는 이 영역의 데이터 크기, 그리고 이중 반납
		   탐지용 원자적 플래그가 들어 있습니다. 메타데이터 페이지 자체는
		   한번 커밋되면 프로세스 종료 시까지 디커밋되지 않습니다(재사용
		   대기 중에도 유효한 SLIST 링크 노드로 계속 쓰이기 위함) - 오직
		   데이터 페이지만 Release 시 디커밋되어 use-after-free 탐지
		   능력을 그대로 유지합니다.

		   [완전 락프리 재사용] 디커밋된 영역은 버려지지 않고, 데이터
		   크기별로 별도의 Lock-free SLIST(CMemoryPool과 동일한 방식)에
		   등록되어 같은 크기의 다음 Alloc이 재사용합니다. 메타데이터
		   페이지가 항상 상주하므로 그 메타데이터 자체를 SLIST_ENTRY
		   노드로 재사용할 수 있어(디커밋된 데이터 페이지에는 링크 정보를
		   쓸 수 없지만, 상주하는 메타데이터 페이지에는 쓸 수 있음),
		   반납/재사용 경로 전체가 락 없이 동작합니다. 오직 "지금까지
		   한 번도 등장한 적 없는 새로운 크기"를 위한 SLIST_HEADER를 맵에
		   처음 등록하는 순간에만 아주 짧게 락을 잡습니다 - 이후 같은
		   크기의 반복적인 Alloc/Release는 그 SLIST_HEADER를 통해 완전히
		   락프리로 처리됩니다.

		   [주소 재사용과 진단 능력의 트레이드오프] 디커밋된 데이터
		   페이지를 재사용한다는 것은, 아주 오래전에 반납된 뒤에도 그
		   주소를 들고 있는 진짜 댕글링 포인터가 나중에 그 주소가 다른
		   할당으로 재사용된 시점 이후에 접근하면 크래시 대신 조용한
		   오염으로 이어질 수 있다는 뜻입니다. 다만 이는 아레나 도입
		   이전의 원래 구현(VirtualFree(MEM_RELEASE) 방식)에서도 OS가
		   해제된 가상 주소를 이후 다른 VirtualAlloc 호출에 재사용할 수
		   있었던 것과 동일한 성격의 특성입니다.
-------------------*/
class StompAllocator
{
	enum { PAGE_SIZE = 0x1000 };

	// 아레나로 예약할 가상 주소 공간 크기. 64비트 프로세스 기준으로
	// 가상 주소 공간만 소모하며 물리 메모리는 실제 커밋 시점까지 전혀
	// 쓰지 않으므로 넉넉하게 잡아 둠(기본 256GB).
	static constexpr int64 ARENA_RESERVE_SIZE = static_cast<int64>(256) * 1024 * 1024 * 1024;

	/*-----------------
		RegionMeta

		설명 : 각 할당 영역의 맨 앞 1페이지에 위치하는 메타데이터.
			   SLIST_ENTRY를 상속해, 재사용 대기 중일 때는 이 구조체
			   자체가 크기별 free-list의 SLIST 노드로 쓰입니다.
	------------------*/
	struct RegionMeta : public SLIST_ENTRY
	{
		int8* dataPagesBase;  // 데이터 페이지 영역의 시작 주소
		int64			dataRegionSize; // 데이터 페이지 영역 크기(바이트, PAGE_SIZE의 배수)
		atomic<int32>	freed;          // 0 = 사용 중, 1 = 반납됨 (이중 반납 탐지용)
	};

public:
	// 설명 : 요청 크기를 페이지 경계에 맞춰 데이터 페이지 수를 계산한 뒤,
	//        같은 크기로 반납되어 재사용 대기 중인 영역이 있으면 그 영역의
	//        데이터 페이지만 다시 커밋해 재사용하고, 없으면 아레나에서
	//        (메타데이터 1페이지 + 데이터 페이지들)만큼 새로 확보합니다.
	//        데이터는 마지막 데이터 페이지의 끝에 맞춰 배치되어, 오버런
	//        발생 시 즉시 다음 미커밋 페이지에서 크래시가 나도록 합니다.
	// 매개변수 : size - 할당할 바이트 크기
	// 반환값   : 페이지 끝에 정렬된 데이터 시작 포인터
	static void* Alloc(int32 size);

	// 설명 : 데이터 포인터로부터 메타데이터 페이지를 포인터 산술만으로
	//        찾아, 이중 반납 여부를 원자적으로 검사(락 없음)한 뒤 데이터
	//        페이지만 디커밋하고 같은 크기의 free-list에 등록합니다.
	// 매개변수 : ptr - Alloc()이 반환했던 데이터 포인터
	static void		Release(void* ptr);

private:
	// 설명 : 아레나(예약된 대형 가상 주소 공간)를 프로세스 생애 동안
	//        정확히 한 번만 예약합니다. 여러 스레드가 동시에 첫 Alloc을
	//        호출해도 call_once로 단 한 번만 VirtualAlloc(MEM_RESERVE)이
	//        일어나도록 보장합니다.
	static void EnsureArenaInitialized();

	// 설명 : 지정한 데이터 크기에 대응하는 크기별 free-list(SLIST_HEADER)를
	//        찾아 반환합니다. 처음 등장하는 크기라면 새로 만들어 맵에
	//        등록합니다 - 이 등록 과정에서만 락을 잡으며, 이미 존재하는
	//        크기에 대해서는 공유 락(읽기)만으로 조회가 끝납니다.
	// 매개변수 : dataRegionSize - 데이터 페이지 영역 크기
	// 반환값   : 해당 크기 전용 SLIST_HEADER 포인터
	static SLIST_HEADER* GetOrCreateSizeClassFreeList(int64 dataRegionSize);

	// 아레나의 시작 주소 (예약 완료 후에는 프로세스 종료까지 고정)
	static int8* s_arenaBase;
	// 재사용 가능한 영역이 없어 새로 확장할 때, 다음 Alloc이 커밋을
	// 시작할 오프셋 - 여러 스레드가 동시에 Alloc해도 서로 겹치지 않도록
	// 원자적으로 밀어서(fetch_add) 사용
	static atomic<int8*>	s_arenaCursor;
	// s_arenaBase/s_arenaCursor 초기화가 정확히 한 번만 일어나도록 보장
	static once_flag		s_arenaInitFlag;

	// 데이터 크기별 free-list 맵. 각 값은 그 크기 전용 Lock-free SLIST의
	// 헤더 포인터입니다. 공유 락(shared_mutex)으로 보호되어, 이미 존재
	// 하는 크기를 찾는 흔한 경우(반복적인 같은 타입 alloc/free)는 여러
	// 스레드가 동시에 읽기 락만으로 조회할 수 있고, 처음 보는 크기를
	// 등록하는 드문 경우에만 배타 락이 걸립니다.
	static shared_mutex									s_sizeClassMapLock;
	static unordered_map<int64, unique_ptr<SLIST_HEADER>>	s_sizeClassFreeLists;
};

/*-------------------
	PoolAllocator

	설명 : 실서비스 핫패스(패킷, 세션, 게임 오브젝트 등 고빈도 할당/해제)에서
		   사용하는 할당자. 내부적으로 전역 싱글턴 gpMemory(CMemory*)의
		   사이즈별 메모리 풀(CMemoryPool)에서 블록을 꺼내 쓰고 반납합니다.

		   xnew/xdelete, StlAllocator가 항상 이 경로를 통해 메모리를
		   주고받으므로, 게임 로직 코드에서 직접 malloc/new를 쓰는 대신
		   이 계층을 거치게 하는 것이 이 프로젝트의 기본 원칙입니다.
-------------------*/
class PoolAllocator
{
public:
	// 설명 : gpMemory를 통해 사이즈에 맞는 메모리 풀에서 블록을 꺼냅니다.
	// 매개변수 : size - 요청 크기(바이트)
	// 반환값   : 사용 가능한 메모리 블록 포인터
	static void* Alloc(int32 size);

	// 설명 : gpMemory를 통해 블록을 원래 속했던 메모리 풀에 반납합니다.
	// 매개변수 : ptr - 반납할 메모리 블록 포인터
	static void		Release(void* ptr);
};

/*-------------------
	StlAllocator

	설명 : std::vector, std::list 등 STL 컨테이너가 내부 노드/버퍼를
		   할당할 때 PoolAllocator를 거치도록 연결하는 어댑터 클래스.
		   STL의 Allocator 요구사항(value_type, allocate, deallocate,
		   템플릿 리바인딩 생성자, 관련 타입 간 대입 연산자)을 충족합니다.

		   관련 타입 간 변환 대입 연산자까지 갖추고 있는 이유는, 일부
		   STL 구현체(특히 구버전 MSVC)가 이 연산자를 요구하기 때문입니다.
		   상태가 없는 allocator라 실제로 할 일은 없지만, 명시적으로
		   정의해 두면 표준 Allocator 요구사항(CopyAssignable)을 더
		   안전하게 충족합니다.
-------------------*/
template<typename T>
class StlAllocator
{
public:
	using value_type = T;

	StlAllocator() {}

	// 설명 : 서로 다른 타입의 StlAllocator 간 변환(리바인딩)을 위한 생성자.
	//        예) StlAllocator<Node> 로부터 StlAllocator<T>를 생성할 때 필요.
	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) {}

	// 설명 : 서로 다른 타입의 StlAllocator를 이 인스턴스에 대입할 때 사용.
	//        내부 상태가 없는 무상태 allocator이므로 실제로 할 일은
	//        없지만, 표준 Allocator 요구사항 및 일부 STL 구현체의
	//        요구를 명시적으로 충족시킵니다.
	template<typename Other>
	StlAllocator<T>& operator=(const StlAllocator<Other>&)
	{
		return *this;
	}

	// 설명 : count개의 T 객체를 담을 메모리를 PoolAllocator로부터 할당받습니다.
	// 매개변수 : count - 할당할 원소 개수
	// 반환값   : T 타입 배열로 사용할 메모리 포인터
	T* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	// 설명 : allocate()로 받은 메모리를 PoolAllocator에 반납합니다.
	// 매개변수 : ptr   - 반납할 메모리 포인터
	//           count - (미사용) 원소 개수. PoolAllocator/CMemoryPool이
	//                   allocSize를 헤더에서 자체적으로 추적하므로 불필요.
	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}
};

/*-------------------
	xnew / xdelete / MakeShared

	설명 : PoolAllocator 기반 placement new/delete 유틸리티 함수 모음.
		   게임 로직에서 객체를 생성/파괴할 때 new/delete 대신 이 함수들을
		   사용해야 메모리 풀 경로(성능 이점)를 타게 됩니다.
-------------------*/

// 설명 : PoolAllocator로 Type 크기만큼 메모리를 확보한 뒤, 그 자리에
//        placement new로 Type 객체를 생성합니다.
//
//        인자 전달에 std::forward 대신 static_cast<Args&&>(args)...를
//        직접 씁니다. std::forward는 사실상 항상 static_cast<T&&>로만
//        구현된 아주 얇은 inline 함수라 어떤 최적화 수준에서도 인라인화
//        되지 않는 경우가 실질적으로 없지만, xnew는 초당 수백만 번
//        호출될 수 있는 극단적 핫패스이므로 라이브러리 함수 호출
//        형태를 아예 없애 인라인 여부를 컴파일러 재량에 맡기지 않도록
//        했습니다. 의미상 std::forward(perfect forwarding)와 완전히
//        동일합니다.
// 매개변수 : args - Type 생성자에 전달할 가변 인자
// 반환값   : 생성된 Type 객체 포인터
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	new(memory)Type(static_cast<Args&&>(args)...); // placement new
	return memory;
}

// 설명 : 객체의 소멸자를 명시적으로 호출한 뒤, 메모리를 PoolAllocator에
//        반납합니다. (placement new로 생성된 객체는 delete 대신 이 함수로
//        정리해야 함 - new/delete 짝이 아니라 malloc 스타일 수명 관리이기 때문)
// 매개변수 : obj - xnew()로 생성된 객체 포인터
template<typename Type>
void xdelete(Type* obj)
{
	obj->~Type();
	PoolAllocator::Release(obj);
}

// 설명 : xnew로 생성한 객체를, xdelete를 커스텀 삭제자로 사용하는
//        shared_ptr로 감싸서 반환합니다. 이를 통해 shared_ptr을 쓰면서도
//        메모리 풀 경로를 그대로 활용할 수 있습니다.
// 매개변수 : args - Type 생성자에 전달할 가변 인자
// 반환값   : Type에 대한 shared_ptr (소멸 시 PoolAllocator로 자동 반납)
template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
	return shared_ptr<Type>{ xnew<Type>(static_cast<Args&&>(args)...), xdelete<Type> };
}

#endif // ndef __ALLOCATOR_H__