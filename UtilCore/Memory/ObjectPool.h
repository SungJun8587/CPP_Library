
//***************************************************************************
// ObjectPool.h : Generic Object Pool Template for High-Performance Allocation
//
// 설명 : 타입 하나당 전용 CMemoryPool을 갖는 제네릭 오브젝트 풀.
//        gpMemory(CMemory)의 크기 구간별 공유 풀과 달리, Type마다 정확히
//        sizeof(Type) 크기의 풀을 독점적으로 사용합니다. 특정 타입이
//        압도적으로 많이 생성/파괴되어 그 타입 전용 풀로 분리하는 것이
//        유리할 때 xnew/xdelete 대신 사용합니다.
//***************************************************************************

#ifndef __OBJECTPOOL_H__
#define __OBJECTPOOL_H__

#pragma once

#ifndef	__MEMORYPOOL_H__
#include <Memory/MemoryPool.h>
#endif

#ifndef __ALLOCATOR_H__
#include <Memory/Allocator.h> // MakeShared에서 사용하는 StlAllocator
#endif

template<typename Type>
class CObjectPool
{
public:
	// 설명 : 이 타입 전용 풀에서 블록을 꺼내 placement new로 Type 객체를
	//        생성합니다. _STOMP 빌드에서는 풀을 거치지 않고
	//        StompAllocator로 대체되어 오버런을 즉시 크래시로 탐지합니다.
	// 매개변수 : args - Type 생성자에 전달할 가변 인자
	// 반환값   : 생성된 Type 객체 포인터
	template<typename... Args>
	static Type* Pop(Args&&... args)
	{
#ifdef _STOMP
		MemoryHeader* ptr = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(s_allocSize));
		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(ptr, s_allocSize));
#else
		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(GetPool().Pop(), s_allocSize));
#endif		
		new(memory)Type(static_cast<Args&&>(args)...); // placement new
		return memory;
	}

	// 설명 : 객체를 파괴하고 블록을 이 타입 전용 풀에 반납합니다.
	//
	//        header->allocSize.exchange(0)로 "값을 읽고 0으로 바꾸기"를
	//        단일 원자 연산으로 처리합니다. 같은 포인터로 이 함수가 두
	//        번(혹은 서로 다른 두 스레드에서 동시에) 호출되면, 정확히
	//        한 번만 원래 값을 받고 나머지는 이미 0이 된 값을 받으므로
	//        그 즉시 ASSERT_CRASH로 걸립니다. CMemoryPool::Push는
	//        allocSize를 무조건 0으로 덮어쓰기만 하고 이미 0인지
	//        확인하지 않으므로, 이 검증이 없으면 같은 포인터가 두 번
	//        Push되어 SLIST 노드가 자기 자신을 가리키는 순환 구조로
	//        꼬여 이후 여러 Pop()이 같은 메모리 블록을 동시에 소유하는
	//        조용한 오염으로 이어질 수 있습니다.
	//
	//        검증을 obj->~Type() 호출보다 먼저 수행하므로, 이미 한 번
	//        파괴된 객체의 소멸자가 다시 호출되는 이중 소멸까지 함께
	//        방지됩니다.
	// 매개변수 : obj - Pop()으로 생성된 객체 포인터
	static void Push(Type* obj)
	{
		MemoryHeader* header = MemoryHeader::DetachHeader(obj);

#ifdef _STOMP
		obj->~Type();
		StompAllocator::Release(header);
#else
		const int32 originalSize = header->allocSize.exchange(0); // 이중 반납 원자적 탐지
		ASSERT_CRASH(originalSize != 0);

		obj->~Type();
		GetPool().Push(header); // CMemoryPool::Push가 allocSize=0을 다시 한번 보장
#endif
	}

	// 설명 : Pop()으로 생성한 객체를, 이 Type 전용 풀이 아니라 gpMemory의
	//        크기별 공유 풀을 거치는 shared_ptr로 만들어 반환합니다.
	//
	//        { Pop(...), Push } 형태로 커스텀 삭제자를 지정해 shared_ptr을
	//        만들면, shared_ptr 생성자가 참조 카운트 제어 블록을 위해
	//        내부적으로 별도의 힙 할당을 한 번 더 일으킵니다(객체 1회 +
	//        제어 블록 1회 = 총 2회 할당). std::allocate_shared는 객체와
	//        제어 블록을 하나의 블록으로 묶어 단 한 번만 할당하므로 이
	//        중복 할당을 없앱니다.
	//
	//        [주의] 이 경로로 만든 객체는 이 Type 전용 CMemoryPool
	//        (GetPool())이 아니라 gpMemory의 크기별 공유 풀을 사용합니다.
	//        객체와 제어 블록을 합친 크기는 표준 라이브러리 구현에 따라
	//        달라지고 sizeof(Type)와 다르므로, 이 Type 전용 고정 크기
	//        풀에 그대로 흘려보내면 크기가 맞지 않아 위험합니다. 대신
	//        StlAllocator<Type>을 넘겨 gpMemory가 실제 필요한 크기에
	//        맞는 공유 풀을 알아서 찾도록 합니다. 즉 Pop()/Push()로 만든
	//        객체는 이 Type 전용 풀을, MakeShared()로 만든 객체는
	//        gpMemory의 공유 풀을 사용하는 것으로 역할이 나뉩니다.
	// 매개변수 : args - Type 생성자에 전달할 가변 인자
	// 반환값   : Type에 대한 shared_ptr
	template<typename... Args>
	static shared_ptr<Type> MakeShared(Args&&... args)
	{
		return allocate_shared<Type>(StlAllocator<Type>(), static_cast<Args&&>(args)...);
	}

private:
	// 설명 : 이 타입 전용 CMemoryPool을 함수 지역 정적 변수(Meyer's
	//        Singleton)로 접근합니다. C++11부터 함수 지역 정적 변수의
	//        동적 초기화는 "최초 사용 시점에 정확히 한 번, 스레드 안전"
	//        하게 이루어짐이 표준으로 보장되므로, 클래스 정적 멤버로
	//        선언했을 때 발생할 수 있는 정적 초기화 순서 문제(다른
	//        번역 단위의 전역/정적 객체 생성자에서 이 풀을 먼저 참조하는
	//        경우 초기화 순서가 보장되지 않아 아직 생성되지 않은
	//        CMemoryPool을 사용하게 되는 문제)를 원천적으로 피합니다.
	// 반환값 : 이 Type 전용 CMemoryPool에 대한 참조
	static CMemoryPool& GetPool()
	{
		static CMemoryPool s_pool{ s_allocSize };
		return s_pool;
	}

	// Type 하나(헤더 포함) 당 필요한 전체 블록 크기. sizeof()는 컴파일
	// 타임에 확정되는 상수 표현식이라 constexpr로 선언 가능하며, 이
	// 경우 별도의 클래스 밖 정의(out-of-class definition)가 필요 없어
	// s_pool과 달리 초기화 순서 문제 자체가 발생하지 않습니다.
	static constexpr int32 s_allocSize = sizeof(Type) + sizeof(MemoryHeader);
};

#endif // ndef __OBJECTPOOL_H__