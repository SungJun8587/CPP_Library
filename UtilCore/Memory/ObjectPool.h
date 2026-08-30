
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
//
// alignof(Type)이 SLIST_ALIGNMENT(16바이트)를 넘는 타입은 지원하지 않습니다.
// MemoryHeader::AttachHeader()가 16-정렬된 헤더 주소에서 정확히 16바이트만
// 전진시켜 데이터 포인터를 만드는 구조라, 그 이상의 정렬을 요구하는 타입에는
// 이 전진만으로 정렬을 보장할 수 없어 안전하지 않습니다(예: alignof(Type)==32면
// header가 16-정렬이어도 header+16이 반드시 32-정렬이라는 보장이 없음). 이런
// 타입은 xnew/전역 MakeShared를 대신 사용하십시오 — 그쪽은 PoolAllocator::
// AllocAligned로 요청 정렬을 직접 보장합니다.
//***************************************************************************
template<typename Type>
class CObjectPool
{
	static_assert(alignof(Type) <= SLIST_ALIGNMENT, "CObjectPool은 SLIST_ALIGNMENT(16바이트)를 넘는 정렬을 요구하는 타입을 지원하지 않습니다. xnew/전역 MakeShared를 사용하십시오.");

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
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(s_allocSize));

		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(header, s_allocSize));
#else
		MemoryHeader* header = GetPool().Pop();

		Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(header, s_allocSize));
#endif

		try
		{
			::new(memory) Type(std::forward<Args>(args)...);
		}
		catch( ... )
		{
#ifdef _STOMP
			StompAllocator::Release(header);
#else
			GetPool().Push(header);
#endif
			throw;
		}

		return memory;
	}

	//***************************************************************************
	// @brief 객체를 소멸시키고 메모리를 타입 전용 풀에 반납합니다.
	// @param obj Pop()으로 생성된 객체
	//***************************************************************************
	static void Push(Type* obj)
	{
		if( obj == nullptr )
			return;

		MemoryHeader* header = MemoryHeader::DetachHeader(obj);

#ifdef _STOMP
		obj->~Type();
		StompAllocator::Release(header);
#else
		// CMemory::Release()/CMemoryPool::Push()와 동일하게 relaxed로 통일합니다.
		// 반납된 블록이 실제로 재순환에 들어가는 시점의 동기화는 SLIST
		// 자체의 강한 펜스가 이미 제공하므로, 여기서는 relaxed로 충분합니다
		// (이 이중 반납 탐지 지점의 memory order는 3.5절에 문서화된 근거 참고).
		const size_t originalSize = header->allocSize.exchange(0, std::memory_order_relaxed); // 이중 반납 원자적 탐지

		ASSERT_CRASH(originalSize != 0);

		obj->~Type();
		GetPool().Push(header); // CMemoryPool::Push가 allocSize=0을 다시 한번 보장
#endif
	}

	//***************************************************************************
	// @brief 타입 전용 ObjectPool 기반 shared_ptr을 생성합니다.
	//
	// @details
	// Pop()으로 Type 객체를 생성한 뒤, shared_ptr의 마지막 참조가 해제되면
	// CObjectPool<Type>::Push()를 통해 해당 객체를 전용 풀로 반환합니다.
	//
	// std::allocate_shared()를 사용하지 않습니다.
	// ObjectPool은 Type 객체의 메모리를 자체적으로 관리해야 하기 때문입니다.
	//
	// [이런 기능에 사용하면 유리합니다]
	// - I/O 요청 컨텍스트(CRioEvent, IOCP OVERLAPPED 확장 구조체 등):
	//   Send/Receive 호출마다, 즉 초당 수만~수십만 번 생성/파괴되는 타입
	// - 패킷 파싱 후 디스패치용 Job/이벤트 객체(CJob 등): 세션 수 × 초당
	//   패킷 수만큼 생성/파괴되는 타입
	// - 송신 데이터 청크(CSendBufferChunk 등): 송신마다 쪼개져 나가는
	//   고정 크기 소형 객체
	//
	// shared_ptr 컨트롤 블록은 별도로 할당되어 총 2회 할당이 발생하는
	// 대신, 다른 타입과 풀을 공유하지 않고 내부 단편화도 없습니다.
	// 전역 ::MakeShared<Type>()과의 선택 기준은 Allocator.h의
	// ::MakeShared 주석을 참고하십시오.
	//
	// @param args Type 생성자에 전달할 가변 인자
	// @return CObjectPool<Type>에서 관리되는 shared_ptr<Type>
	//***************************************************************************
	template<typename... Args>
	static std::shared_ptr<Type> MakeShared(Args&&... args)
	{
		return std::shared_ptr<Type>(Pop(std::forward<Args>(args)...), &CObjectPool<Type>::Push);
	}

private:
	//***************************************************************************
	// @brief size를 alignment의 배수로 올림합니다(alignment는 2의 거듭제곱).
	// @details
	// sizeof(Type)이 16의 배수라는 보장이 없어(예: int64 두 개+int32 하나면
	// sizeof==24), s_allocSize(sizeof(Type)+sizeof(MemoryHeader))도 16의
	// 배수가 아닐 수 있습니다. 그런데 CMemoryPool 생성자는 allocSize가
	// SLIST_ALIGNMENT(16)의 배수일 것을 ASSERT_CRASH로 요구하므로, 이
	// 올림 없이는 그런 크기의 Type에서 풀 생성 자체가 크래시합니다.
	//***************************************************************************
	static constexpr size_t AlignUp(size_t size, size_t alignment)
	{
		const size_t remainder = size % alignment;
		return remainder == 0 ? size : size + (alignment - remainder);
	}

	//***************************************************************************
	// @brief 타입 전용 CMemoryPool 인스턴스를 Meyer's Singleton 형태로 반환합니다.
	// @details
	// 함수 지역 정적 변수라 다른 정적/전역 shared_ptr<Type>이 이 풀보다
	// 나중에(정적 소멸 순서상) 파괴되면, 그 shared_ptr의 소멸자가 이미
	// 파괴된 s_pool을 Push()로 다시 건드릴 위험이 있습니다. gpMemory와
	// 마찬가지로, 이 타입의 static/전역 shared_ptr을 정적 소멸 순서에
	// 걸쳐 들고 있지 않도록 호출부에서 주의해야 합니다.
	// @return 이 Type 전용 CMemoryPool 참조
	//***************************************************************************
	static CMemoryPool& GetPool()
	{
		static CMemoryPool s_pool{ s_allocSize };
		return s_pool;
	}

private:
	static constexpr size_t s_allocSize = AlignUp(sizeof(Type) + sizeof(MemoryHeader), SLIST_ALIGNMENT); // Type 하나당 필요한 전체 블록 크기 (SLIST_ALIGNMENT 배수로 올림)
};

#endif // ndef UC_OBJECTPOOL_H