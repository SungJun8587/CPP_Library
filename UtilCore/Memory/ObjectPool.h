
//***************************************************************************
// ObjectPool.h : Generic Object Pool Template for High-Performance Allocation
//
//***************************************************************************

#ifndef UC_OBJECTPOOL_H
#define UC_OBJECTPOOL_H

#include <Memory/MemoryPool.h>
#include <Memory/Allocator.h>

//***************************************************************************
// @class CObjectPool
// @brief 타입별 독점 CMemoryPool을 제공하는 제네릭 오브젝트 풀 템플릿 클래스입니다.
//
// @details
// gpMemory(CMemory)의 크기 구간별 공유 풀과 달리, Type마다 정확히 sizeof(Type) 크기의
// 풀을 독점적으로 사용합니다. 특정 타입이 압도적으로 많이 생성/파괴되어
// 전용 풀로 분리하는 것이 유리할 때 xnew/xdelete 대신 사용합니다.
//***************************************************************************
template<typename Type>
class CObjectPool
{
public:
	//***************************************************************************
	// @brief 타입 전용 풀에서 메모리를 할당받아 객체를 생성합니다.
	// @details _STOMP 빌드에서는 풀을 거치지 않고 StompAllocator로 대체되어
	//          오버런을 즉시 크래시로 탐지합니다.
	// @param args Type 생성자에 전달할 가변 인자
	// @return 생성된 Type 객체 포인터
	//***************************************************************************
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
	// @brief 객체를 소멸시키고 메모리를 타입 전용 풀에 반납합니다.
	// @param obj Pop()으로 생성된 객체 포인터
	//***************************************************************************
	static void Push(Type* obj)
	{
		MemoryHeader* header = MemoryHeader::DetachHeader(obj);

#ifdef _STOMP
		obj->~Type();
		StompAllocator::Release(header);
#else
		const size_t originalSize = header->allocSize.exchange(0); // 이중 반납 원자적 탐지
		ASSERT_CRASH(originalSize != 0);

		obj->~Type();
		GetPool().Push(header); // CMemoryPool::Push가 allocSize=0을 다시 한번 보장
#endif
	}

private:
	//***************************************************************************
	// @brief 타입 전용 CMemoryPool 인스턴스를 Meyer's Singleton 형태로 반환합니다.
	// @return 이 Type 전용 CMemoryPool 참조
	//***************************************************************************
	static CMemoryPool& GetPool()
	{
		static CMemoryPool s_pool{ s_allocSize };
		return s_pool;
	}

private:
	static constexpr size_t s_allocSize = sizeof(Type) + sizeof(MemoryHeader); // Type 하나당 필요한 전체 블록 크기 (헤더 포함)
};

#endif // ndef UC_OBJECTPOOL_H