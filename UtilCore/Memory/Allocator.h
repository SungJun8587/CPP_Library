
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

#ifndef	__RAWALLOCATOR_H__
#include <Memory/RawAllocator.h>
#endif

#include <new>           // std::align_val_t (BaseAllocator의 확장 정렬 new/delete)
#include <unordered_map> // StompAllocator의 크기별 free-list 맵
#include <shared_mutex>  // StompAllocator의 크기별 free-list 맵 보호(읽기 다중/쓰기 단일 락)
#include <mutex>         // StompAllocator의 아레나 1회 초기화(once_flag/call_once)

/***************************************************************************
	BaseAllocator

	설명 : RawAllocator에 그대로 위임하는 최하위 계층 할당자. CMemory/
		   CMemoryPool 등 메모리 모듈 자체는 이 클래스를 상속받지 않으며
		   (부트스트랩 객체는 단순함을 위해 전역 new/delete 사용), 메모리
		   모듈과 무관한 다른 기능 클래스가 "풀을 거치지 않는 raw 할당
		   경로"가 필요할 때 가져다 쓰는 범용 유틸리티입니다.

		   정적 메서드(Alloc/Release) 직접 호출, 또는 상속(믹스인)으로
		   operator new/delete를 자동 오버라이드하는 두 가지 방식으로
		   쓸 수 있습니다. 데이터 멤버가 없고 함수도 모두 non-virtual이라
		   상속 오버헤드가 없습니다.

		   placement new/delete 오버로드는 xnew() 호환성을 위한 것이고,
		   확장 정렬(align_val_t) 오버로드는 alignas(32) 이상으로 선언된
		   클래스(SIMD 데이터 등)가 상속받을 때 정렬 요구사항이 조용히
		   무시되는 것을 막기 위한 것입니다.
***************************************************************************/
class BaseAllocator
{
public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);

	//***************************************************************************
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

	//***************************************************************************
	// 설명 : alignof(T)가 기본 정렬을 넘는 클래스가 이 클래스를 상속받을 때
	//        컴파일러가 자동 선택하는 확장 정렬 오버로드. 이 오버로드가
	//        없으면 정렬 정보가 유실된 채 일반 operator new(size_t)로
	//        조용히 폴백될 수 있어(경고 없는 정렬 위반), 명시적으로
	//        RawAllocator::AllocAligned로 위임해 정렬을 보장합니다.
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

/***************************************************************************
	StompAllocator

	설명 : 메모리 오버런을 그 자리에서 크래시로 잡아내는 디버그 전용
		   할당자. 데이터를 페이지 끝에 딱 맞춰 배치해, 할당 크기를 넘어
		   쓰는 순간 다음 미커밋 페이지에서 즉시 Access Violation이
		   나도록 합니다. CMemory::Allocate/Release의 _STOMP 매크로
		   분기로만 활성화됩니다.

		   아레나(예약된 대형 가상 주소 공간)를 최초 1회만 예약해 두고
		   그 안에서 커밋/디커밋만 반복하며, 반납된 영역은 크기별
		   Lock-free free-list(shared_mutex로 보호되는 맵에서 조회)로
		   재사용됩니다. 메타데이터 페이지는 재사용 대기 중에도 유효한
		   SLIST 노드로 쓰이도록 디커밋하지 않고 상주시킵니다.
***************************************************************************/
class StompAllocator
{
	enum { PAGE_SIZE = 0x1000 };

	// 아레나로 예약할 가상 주소 공간 크기. 64비트 프로세스 기준으로
	// 가상 주소 공간만 소모하며 물리 메모리는 실제 커밋 시점까지 전혀
	// 쓰지 않으므로 넉넉하게 잡아 둠(기본 256GB).
	static constexpr int64 ARENA_RESERVE_SIZE = static_cast<int64>(256) * 1024 * 1024 * 1024;

	/***************************************************************************
		RegionMeta

		설명 : 각 할당 영역의 맨 앞 1페이지에 위치하는 메타데이터.
			   SLIST_ENTRY를 상속해, 재사용 대기 중에는 이 구조체 자체가
			   크기별 free-list의 SLIST 노드로 쓰입니다.
	***************************************************************************/
	struct RegionMeta : public SLIST_ENTRY
	{
		int8* dataPagesBase;    // 데이터 페이지 영역의 시작 주소
		int64			dataRegionSize; // 데이터 페이지 영역 크기(바이트, PAGE_SIZE의 배수)
		atomic<int32>	freed;          // 0 = 사용 중, 1 = 반납됨 (이중 반납 탐지용)
	};

public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);

private:
	static void EnsureArenaInitialized();
	static SLIST_HEADER* GetOrCreateSizeClassFreeList(int64 dataRegionSize);

	// 아레나의 시작 주소 (예약 완료 후 프로세스 종료까지 고정)
	static int8* s_arenaBase;
	// 아레나를 새로 확장할 때 다음 할당이 사용할 오프셋. 여러 스레드가
	// 동시에 밀어도 겹치지 않도록 원자적으로(fetch_add) 사용
	static atomic<int8*>	s_arenaCursor;
	// 아레나 초기화가 정확히 한 번만 일어나도록 보장
	static once_flag		s_arenaInitFlag;

	// 데이터 크기별 free-list 맵. shared_mutex로 보호되어, 이미 등록된
	// 크기의 조회는 공유 락(읽기)만으로 처리됩니다.
	static shared_mutex									s_sizeClassMapLock;
	static unordered_map<int64, unique_ptr<SLIST_HEADER>>	s_sizeClassFreeLists;
};

/***************************************************************************
	PoolAllocator

	설명 : 실서비스 핫패스(패킷, 세션, 게임 오브젝트 등 고빈도 할당/해제)에서
		   사용하는 할당자. 내부적으로 전역 싱글턴 gpMemory(CMemory*)의
		   사이즈별 메모리 풀(CMemoryPool)에서 블록을 꺼내 쓰고 반납합니다.

		   xnew/xdelete, StlAllocator가 항상 이 경로를 통해 메모리를
		   주고받으므로, 게임 로직 코드에서 직접 malloc/new를 쓰는 대신
		   이 계층을 거치게 하는 것이 이 프로젝트의 기본 원칙입니다.
***************************************************************************/
class PoolAllocator
{
public:
	static void*	Alloc(int32 size);
	static void		Release(void* ptr);
};

/***************************************************************************
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
***************************************************************************/
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

	//***************************************************************************
	// 설명 : 서로 다른 타입의 StlAllocator를 이 인스턴스에 대입할 때 사용.
	//        내부 상태가 없는 무상태 allocator이므로 실제로 할 일은
	//        없지만, 표준 Allocator 요구사항 및 일부 STL 구현체의
	//        요구를 명시적으로 충족시킵니다.
	template<typename Other>
	StlAllocator<T>& operator=(const StlAllocator<Other>&)
	{
		return *this;
	}

	//***************************************************************************
	// 설명 : count개의 T 객체를 담을 메모리를 PoolAllocator로부터 할당받습니다.
	// 매개변수 : count - 할당할 원소 개수
	// 반환값   : T 타입 배열로 사용할 메모리 포인터
	T* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	//***************************************************************************
	// 설명 : allocate()로 받은 메모리를 PoolAllocator에 반납합니다.
	// 매개변수 : ptr   - 반납할 메모리 포인터
	//           count - (미사용) 원소 개수. PoolAllocator/CMemoryPool이
	//                   allocSize를 헤더에서 자체적으로 추적하므로 불필요.
	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}
};


/***************************************************************************
	xnew / xdelete / MakeShared

	설명 : PoolAllocator 기반 placement new/delete 유틸리티 함수 모음.
		   게임 로직에서 객체를 생성/파괴할 때 new/delete 대신 이 함수들을
		   사용해야 메모리 풀 경로(성능 이점)를 타게 됩니다.
***************************************************************************/

//***************************************************************************
// 설명 : PoolAllocator로 Type 크기만큼 메모리를 확보한 뒤, 그 자리에
//        placement new로 Type 객체를 생성합니다. 인자 전달에는
//        std::forward와 의미상 동일한 static_cast<Args&&>(args)...를
//        직접 사용해, 초당 수백만 번 호출될 수 있는 핫패스에서 라이브러리
//        함수 호출 형태 자체를 없앱니다.
// 매개변수 : args - Type 생성자에 전달할 가변 인자
// 반환값   : 생성된 Type 객체 포인터
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	new(memory)Type(static_cast<Args&&>(args)...); // placement new
	return memory;
}

//***************************************************************************
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

//***************************************************************************
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