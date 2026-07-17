
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
#include <Memory/Allocator.h> 
#endif

template<typename Type>
class CObjectPool
{
public:
	//***************************************************************************
	// 설명 : 이 타입 전용 풀에서 블록을 꺼내 placement new로 Type 객체를
	//        생성합니다. _STOMP 빌드에서는 풀을 거치지 않고
	//        StompAllocator로 대체되어 오버런을 즉시 크래시로 탐지합니다.
	//
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

	//***************************************************************************
	// 설명 : 객체를 파괴하고 블록을 이 타입 전용 풀에 반납합니다.
	//
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

	//***************************************************************************
	// 설명 : Pop()으로 생성한 객체를, 이 Type 전용 풀이 아니라 gpMemory의
	//        크기별 공유 풀을 거치는 shared_ptr로 만들어 반환합니다.
	//
	// 매개변수 : args - Type 생성자에 전달할 가변 인자
	// 반환값   : Type에 대한 shared_ptr
	template<typename... Args>
	static shared_ptr<Type> MakeShared(Args&&... args)
	{
		return allocate_shared<Type>(StlAllocator<Type>(), static_cast<Args&&>(args)...);
	}

private:
	//***************************************************************************
	// 설명 : 이 타입 전용 CMemoryPool을 함수 지역 정적 변수(Meyer's Singleton)로 접근합니다.
	//
	// 반환값 : 이 Type 전용 CMemoryPool에 대한 참조
	static CMemoryPool& GetPool()
	{
		static CMemoryPool s_pool{ s_allocSize };
		return s_pool;
	}

	//***************************************************************************
	// Type 하나(헤더 포함) 당 필요한 전체 블록 크기. sizeof()는 컴파일
	// 타임에 확정되는 상수 표현식이라 constexpr로 선언 가능하며, 이
	// 경우 별도의 클래스 밖 정의(out-of-class definition)가 필요 없어
	// s_pool과 달리 초기화 순서 문제 자체가 발생하지 않습니다.
	static constexpr int32 s_allocSize = sizeof(Type) + sizeof(MemoryHeader);
};

#endif // ndef __OBJECTPOOL_H__