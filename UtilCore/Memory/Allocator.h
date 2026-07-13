
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

#ifndef __RAWALLOCATOR_H__
#include <RawAllocator.h>
#endif

#ifndef __MEMORY_H__
#include <Memory.h>
#endif

class CMemory;
extern CMemory* gpMemory;

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

		   [정렬(alignment) 관련 주의] operator new에서 호출하는
		   RawAllocator::Alloc은 정렬을 보장하지 않는 일반 할당 함수입니다.
		   64비트 환경에서는 malloc/mimalloc 등이 보통 16바이트 정렬을
		   기본 보장하지만, 이보다 더 큰 정렬 요구사항(alignas 등)을 가진
		   클래스가 상속받는다면 RawAllocator::AllocAligned를 쓰는 별도
		   오버로드가 필요합니다.
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
		   활성화되며, 평소 빌드에서는 사용되지 않습니다(할당당 최소
		   페이지 하나를 소모하므로 매우 비효율적 - 디버깅 전용).
-------------------*/
class StompAllocator
{
	enum { PAGE_SIZE = 0x1000 };

public:
	// 설명 : 요청 크기를 페이지 경계에 맞춰 예약/커밋하고, 데이터를
	//        페이지 끝에 배치하여 반환합니다. (오버런 즉시 감지 목적)
	// 매개변수 : size - 할당할 바이트 크기
	// 반환값   : 페이지 끝에 정렬된 데이터 시작 포인터
	static void* Alloc(int32 size);

	// 설명 : Alloc()으로 예약된 페이지 전체를 해제합니다.
	//        전달된 ptr로부터 페이지 시작 주소를 역산하여 VirtualFree합니다.
	// 매개변수 : ptr - Alloc()이 반환했던 데이터 포인터
	static void		Release(void* ptr);
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
// 매개변수 : args - Type 생성자에 전달할 가변 인자
// 반환값   : 생성된 Type 객체 포인터
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	new(memory)Type(forward<Args>(args)...); // placement new
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
	return shared_ptr<Type>{ xnew<Type>(forward<Args>(args)...), xdelete<Type> };
}

#endif // ndef __ALLOCATOR_H__