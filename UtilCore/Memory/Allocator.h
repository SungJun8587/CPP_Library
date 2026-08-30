
//***************************************************************************
// Allocator.h
//
// @brief 프로젝트에서 사용하는 세 가지 메모리 할당 전략(Base, Stomp, Pool) 및
//        STL/커스텀 new-delete 연동 유틸리티를 정의합니다.
//
// @details
// 프로젝트 할당 계층 구획:
//   - BaseAllocator  : RawAllocator 기반 순수 할당 (최하위 계층 및 부트스트랩용)
//   - StompAllocator : 메모리 오버런/유효 범위 이탈 탐지용 디버그 전용 할당자
//   - PoolAllocator  : 실서비스 핫패스에서 사용하는 속도 최적화 메모리 풀 할당자
//   - StlAllocator   : STL 컨테이너가 PoolAllocator를 타도록 연결하는 어댑터
//   - xnew/xdelete   : PoolAllocator 기반 객체 생성 및 파괴 유틸리티
//***************************************************************************

#ifndef UC_ALLOCATOR_H
#define UC_ALLOCATOR_H

#include <Memory/RawAllocator.h>

#include <new>           // std::align_val_t
#include <cstddef>
#include <limits>
#include <unordered_map> // StompAllocator의 크기별 free-list 맵
#include <shared_mutex>  // StompAllocator의 크기별 free-list 맵 보호 (SRWLock/shared_mutex)
#include <mutex>         // StompAllocator의 아레나 1회 초기화 (std::once_flag)
#include <memory>
#include <utility>

//***************************************************************************
// @class BaseAllocator
// @brief RawAllocator에 작업을 직접 위임하는 최하위 계층 범용 할당자.
//
// @details
// 메모리 모듈과 무관한 다른 기능 클래스가 풀(Pool)을 거치지 않는 Raw 할당 경로가
// 필요할 때 사용합니다. 정적 메서드 호출 또는 상속(믹스인)을 통해 operator new/delete를
// 자동으로 오버라이드할 수 있습니다.
// 데이터 멤버가 없고 가상 함수가 없어 상속 오버헤드가 발생하지 않습니다.
//***************************************************************************
class BaseAllocator
{
public:
	//***************************************************************************
	// @brief Raw 메모리를 지정한 크기만큼 할당받습니다.
	// @param size 할당할 바이트 크기
	// @return 할당된 메모리의 시작 포인터
	//***************************************************************************
	static void* Alloc(size_t size);

	//***************************************************************************  
	// @brief Alloc으로 할당받은 Raw 메모리를 해제합니다.  
	// @param ptr 해제할 메모리 포인터  
	//***************************************************************************  
	static void Release(void* ptr);

	//***************************************************************************  
	// @brief 단일 객체 메모리 할당 연산자 오버로드 (RawAllocator 경유)  
	// @param size 할당할 바이트 크기  
	// @return 할당된 메모리 포인터  
	//***************************************************************************  
	void* operator new(size_t size)
	{
		return RawAllocator::Alloc(size);
	}

	//***************************************************************************  
	// @brief 배열 메모리 할당 연산자 오버로드 (RawAllocator 경유)  
	// @param size 할당할 바이트 크기  
	// @return 할당된 메모리 포인터  
	//***************************************************************************  
	void* operator new[](size_t size)
	{
		return RawAllocator::Alloc(size);
	}

	//***************************************************************************  
	// @brief 단일 객체 메모리 해제 연산자 오버로드  
	// @param ptr 해제할 메모리 포인터  
	//***************************************************************************  
	void operator delete(void* ptr)
	{
		RawAllocator::Free(ptr);
	}

	//***************************************************************************  
	// @brief 배열 메모리 해제 연산자 오버로드  
	// @param ptr 해제할 메모리 포인터  
	//***************************************************************************  
	void operator delete[](void* ptr)
	{
		RawAllocator::Free(ptr);
	}

	//***************************************************************************  
	// @brief C++14 크기 지정(Sized) 단일 객체 메모리 해제 연산자  
	// @param ptr 객체 메모리 포인터  
	// @param size 객체 크기 (미사용)  
	//***************************************************************************  
	void operator delete(void* ptr, size_t /*size*/)
	{
		RawAllocator::Free(ptr);
	}

	//***************************************************************************  
	// @brief C++14 크기 지정(Sized) 배열 메모리 해제 연산자  
	// @param ptr 배열 메모리 포인터  
	// @param size 배열 크기 (미사용)  
	//***************************************************************************  
	void operator delete[](void* ptr, size_t /*size*/)
	{
		RawAllocator::Free(ptr);
	}

	//***************************************************************************  
	// @brief Placement new 연산자 오버로드 (xnew 호환용)  
	// @param size 객체 크기 (미사용)  
	// @param ptr 이미 할당되어 전달된 메모리 포인터  
	// @return 전달받은 메모리 포인터  
	//***************************************************************************  
	void* operator new(size_t, void* ptr)
	{
		return ptr;
	}

	//***************************************************************************  
	// @brief Placement delete 연산자 오버로드  
	// @details 생성자 수행 중 예외 발생 시 컴파일러가 자동 호출하는 짝 연산자입니다.  
	//***************************************************************************  
	void operator delete(void*, void*)
	{
	}

	//***************************************************************************  
	// @brief 확장 정렬(alignas)을 요구하는 단일 객체의 메모리 할당 연산자  
	// @param size 할당할 바이트 크기  
	// @param alignment 정렬 바이트 단위 (std::align_val_t)  
	// @return 정렬 조건이 보장된 메모리 포인터  
	//***************************************************************************  
	void* operator new(size_t size, std::align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	//***************************************************************************  
	// @brief 확장 정렬(alignas)을 요구하는 배열 객체의 메모리 할당 연산자  
	// @param size 할당할 바이트 크기  
	// @param alignment 정렬 바이트 단위 (std::align_val_t)  
	// @return 정렬 조건이 보장된 메모리 포인터  
	//***************************************************************************  
	void* operator new[](size_t size, std::align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	//***************************************************************************  
	// @brief 확장 정렬 메모리 해제 연산자 (단일 객체)  
	// @param ptr 해제할 메모리 포인터  
	//***************************************************************************  
	void operator delete(void* ptr, std::align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

	//***************************************************************************  
	// @brief 확장 정렬 메모리 해제 연산자 (배열 객체)  
	// @param ptr 해제할 메모리 포인터  
	//***************************************************************************  
	void operator delete[](void* ptr, std::align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

	//***************************************************************************  
	// @brief C++17 크기 지정 및 확장 정렬 메모리 해제 연산자 (단일 객체)  
	// @param ptr 객체 메모리 포인터  
	// @param size 객체 크기 (미사용)  
	// @param alignment 정렬 바이트 단위 (미사용)  
	//***************************************************************************  
	void operator delete(void* ptr, size_t /*size*/, std::align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

	//***************************************************************************  
	// @brief C++17 크기 지정 및 확장 정렬 메모리 해제 연산자 (배열 객체)  
	// @param ptr 배열 메모리 포인터  
	// @param size 배열 크기 (미사용)  
	// @param alignment 정렬 바이트 단위 (미사용)  
	//***************************************************************************  
	void operator delete[](void* ptr, size_t /*size*/, std::align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

};

//***************************************************************************
// @class StompAllocator
// @brief 메모리 오버런/Boundary 쓰기를 즉시 크래시로 탐지하는 디버그 전용 할당자.
//
// @details
// 할당된 데이터를 페이지 경계 끝에 배치하고 바로 다음 페이지에 PAGE_NOACCESS를
// 설정하여, 경계를 넘어선 쓰기/읽기 접근 발생 시 즉시 Access Violation을 유발합니다.
// 대형 가상 주소 공간 아레나(ARENA_RESERVE_SIZE)를 예약하여 활용합니다.
//***************************************************************************
class StompAllocator
{
	//***************************************************************************
	// @enum Anonymous Enum
	// @brief StompAllocator에서 사용하는 메모리 정렬 및 페이지 단위 상수를 정의합니다.
	//***************************************************************************
	enum : size_t
	{
		PAGE_SIZE = 0x1000,
		ALIGNMENT = 16
	};

	// 64비트 가상 주소 공간 예약 크기 (기본 256GB)  
	static constexpr size_t ARENA_RESERVE_SIZE = static_cast<size_t>(256ULL) * 1024ULL * 1024ULL * 1024ULL;

	static constexpr uint64 ALLOCATION_MAGIC = 0x5543414C4C4F4341ULL; // "UCALLOCA"  

	struct RegionMeta;

	//***************************************************************************  
	// @struct AllocationHeader  
	// @brief ptr 바로 앞에 정렬되어 위치하는 메타데이터 헤더 구조체.  
	//***************************************************************************  
	struct AllocationHeader
	{
		RegionMeta* meta;			// 메타데이터 구조체 포인터
		uint64		magic;			// 메모리 오염 탐지용 매직 넘버
	};

	//***************************************************************************  
	// @struct RegionMeta  
	// @brief 각 할당 영역의 메타데이터 및 Lock-Free Free-List 노드 구조체.  
	// @details
	// 아레나 내부(가드 대상 데이터 페이지)에 두지 않고 RawAllocator를 통해
	// 일반 힙에서 별도로 할당됩니다. 이전에는 이 구조체를 데이터 페이지
	// 앞의 전용 "메타 페이지"에 배치해, 해당 페이지가 Release() 이후에도
	// (SLIST 재사용 노드로 계속 유효해야 하므로) 프로세스 종료 시까지 영구
	// 커밋 상태로 남는 문제가 있었습니다. 힙으로 분리하면 Release() 시
	// 데이터 페이지 전체를 완전히 DECOMMIT할 수 있고, 재사용되지 않는
	// 소량의 RegionMeta(수십 바이트)만 힙에 남습니다.
	// SLIST_ENTRY를 상속해 Lock-Free Free-List 노드로 직접 쓰이므로,
	// Windows SLIST가 요구하는 MEMORY_ALLOCATION_ALIGNMENT(x64 기준
	// 16바이트) 경계 정렬을 DECLSPEC_ALIGN(ALIGNMENT)으로 강제합니다.
	// 이 타입을 담을 실제 raw 메모리도 그냥 RawAllocator::Alloc이 아니라
	// 반드시 RawAllocator::AllocAligned(sizeof(RegionMeta), ALIGNMENT)로
	// 받아와야 이 정렬이 실제로 보장됩니다(MemoryHeader와 동일한 이유).
	//***************************************************************************  
	DECLSPEC_ALIGN(ALIGNMENT)
		struct RegionMeta : public SLIST_ENTRY
	{
		int8* dataPagesBase;		// 데이터 페이지 영역 시작 주소  
		size_t dataRegionSize;      // 데이터 영역 크기 (PAGE_SIZE 단위)  
		atomic<int32> freed;        // 0: 사용 중, 1: 반납됨 (이중 해제 탐지용)  
		bool   isFallback;          // Arena 이외의 Direct Fallback 할당 여부  
	};

	static_assert(alignof(RegionMeta) >= ALIGNMENT, "RegionMeta alignment must satisfy SLIST alignment requirement.");

	static_assert(sizeof(AllocationHeader) == 16, "AllocationHeader must be 16 bytes.");
	static_assert(alignof(AllocationHeader) <= ALIGNMENT, "AllocationHeader alignment must fit allocator alignment.");

public:
	//***************************************************************************
	// @brief Page-aligned 기반 디버그 메모리를 할당합니다.
	// @param size 요청할 데이터 크기 (바이트)
	// @return 페이지 끝단에 맞춤 정렬된 메모리 포인터
	//***************************************************************************
	static void* Alloc(size_t size);

	//***************************************************************************  
	// @brief StompAllocator로 할당받은 디버그 메모리를 반납합니다.  
	// @param ptr 반납할 메모리 포인터  
	//***************************************************************************  
	static void Release(void* ptr);

	//***************************************************************************  
	// @brief 프로세스 종료 및 툴 환경 해제용 Cleanup 함수  
	// @details Free-List 내의 모듈을 해제하고 시스템 자원을 원상 복구합니다.  
	//***************************************************************************  
	static void Cleanup();

private:
	//***************************************************************************
	// @brief 가상 메모리 아레나 공간을 최초 1회만 스레드 안전하게 초기화합니다.
	//***************************************************************************
	static void EnsureArenaInitialized();

	//***************************************************************************  
	// @brief 크기 부류(Size Class)에 맞는 Free-List 헤더 포인터를 조회하거나 생성합니다.  
	// @param dataRegionSize 데이터 영역 크기  
	// @return 해당 크기 부류의 Lock-Free SLIST_HEADER 포인터  
	//***************************************************************************  
	static SLIST_HEADER* GetOrCreateSizeClassFreeList(size_t dataRegionSize);

	//***************************************************************************  
	// @brief Arena에서 연속된 가상 주소 영역을 안전하게 예약합니다. (CAS 적용)  
	// @return 성공 시 예약된 오프셋 포인터, 아레나 고갈 시 nullptr 반환  
	//***************************************************************************  
	static int8* ReserveArenaRegion(size_t size);

private:
	static int8* s_arenaBase;														// 아레나 시작 주소
	static atomic<int8*> s_arenaCursor;												// 현재 커서 오프셋
	static once_flag s_arenaInitFlag;												// 1회 초기화 플래그
	static shared_mutex s_sizeClassMapLock;											// Free-List 맵 동기화 락
	static unordered_map<size_t, unique_ptr<SLIST_HEADER>> s_sizeClassFreeLists;	// 크기별 Free-List 맵
	// Cleanup() 이후 재사용을 즉시·명확하게 걸러내기 위한 종료 플래그.
	// Cleanup()은 s_arenaBase/s_arenaCursor를 nullptr로 되돌리지만 s_arenaInitFlag(once_flag)는
	// 손대지 않으므로, 이 플래그가 없다면 Cleanup() 이후의 Alloc()이 EnsureArenaInitialized()의
	// call_once를 재실행시키지 못해 "예약된 적 없는 주소 0 근방을 유효한 아레나 위치로 오인"하는
	// 식으로 몇 번의 호출 뒤에야 원인 파악이 어려운 형태로 크래시할 수 있었습니다.
	static atomic<bool> s_isCleanedUp;												// Cleanup() 호출 여부
};

//***************************************************************************
// @class PoolAllocator
// @brief 고속 메모리 풀(CMemoryPool)을 활용하는 프로젝트 기본 핫패스 할당자.
// @details
// 전역 CMemory 메인 스레드/서버 풀에서 고정된 크기 블록을 빠르게 할당 및 해제하며,
// 단편화 방지 및 고성능 메모리 관리를 제공합니다.
//***************************************************************************
class PoolAllocator
{
public:
	//***************************************************************************
	// @brief CMemoryPool에서 적절한 크기의 메모리 블록을 할당받습니다.
	// @param size 할당 요청 크기 (바이트)
	// @return 메모리 블록 시작 포인터
	//***************************************************************************
	static void* Alloc(size_t size);

	//***************************************************************************  
	// @brief 지정된 정렬 조건을 만족하는 메모리 블록을 할당받습니다.  
	// @param size 할당 요청 크기 (바이트)  
	// @param alignment 요구 정렬 바이트 단위  
	// @return 정렬 조건이 보장된 메모리 블록 시작 포인터  
	//***************************************************************************  
	static void* AllocAligned(size_t size, size_t alignment);

	//***************************************************************************  
	// @brief 할당받은 메모리 블록을 CMemoryPool에 반납합니다.  
	// @param ptr 반납할 메모리 포인터  
	//***************************************************************************  
	static void Release(void* ptr);

	//***************************************************************************  
	// @brief 확장 정렬로 할당받은 메모리 블록을 반납합니다.  
	// @param ptr 반납할 메모리 포인터  
	// @param alignment 할당 시 사용한 정렬 바이트 단위  
	//***************************************************************************  
	static void ReleaseAligned(void* ptr, size_t alignment);
};

//***************************************************************************
// @class StlAllocator
// @brief std::vector, std::list 등 C++ STL 컨테이너가 PoolAllocator를 타도록 돕는 어댑터.
//
// @details
// C++ 표준 Allocator Concept 요구사항을 준수하도록 설계되어 있습니다.
// 무상태(Stateless) 할당자이므로 인스턴스 간 상태를 공유하지 않으며,
// 모든 StlAllocator 인스턴스는 동일하게 호환됩니다.
//***************************************************************************
template<typename T>
class StlAllocator
{
public:
	using value_type = T;

	//***************************************************************************  
	// @brief 기본 생성자  
	//***************************************************************************  
	StlAllocator() noexcept = default;

	//***************************************************************************  
	// @brief 서로 다른 타입의 StlAllocator 간 리바인딩(Rebind) 변환 생성자  
	//***************************************************************************  
	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) noexcept {}

	//***************************************************************************  
	// @brief 서로 다른 타입의 StlAllocator 대입 연산자 
	// @return StlAllocator 자기 자신 참조
	//***************************************************************************  
	template<typename Other>
	StlAllocator<T>& operator=(const StlAllocator<Other>&) noexcept
	{
		return *this;
	}

	//***************************************************************************  
	// @brief T 타입 객체 count개를 저장할 메모리를 PoolAllocator로부터 할당합니다.  
	// @param count 할당할 원소 개수  
	// @return 할당된 메모리의 T 타입 포인터  
	//***************************************************************************  
	T* allocate(size_t count)
	{
		if( count > (std::numeric_limits<size_t>::max)() / sizeof(T) )
			throw std::bad_array_new_length();

		const size_t size = count * sizeof(T);

		if constexpr( alignof(T) > alignof(std::max_align_t) )
		{
			return static_cast<T*>(PoolAllocator::AllocAligned(size, alignof(T)));
		}
		else
		{
			return static_cast<T*>(PoolAllocator::Alloc(size));
		}
	}

	//***************************************************************************  
	// @brief allocate()로 할당받은 메모리를 PoolAllocator에 반납합니다.  
	// @param ptr 해제할 메모리 포인터  
	// @param count (미사용) 원소 개수  
	//***************************************************************************  
	void deallocate(T* ptr, size_t /*count*/) noexcept
	{
		if( ptr == nullptr )
			return;

		if constexpr( alignof(T) > alignof(std::max_align_t) )
		{
			PoolAllocator::ReleaseAligned(ptr, alignof(T));
		}
		else
		{
			PoolAllocator::Release(ptr);
		}
	}

	//***************************************************************************  
	// @brief 할당자 동등성 비교 연산자 (C++ STL 컨테이너 호환성용)  
	// @return true
	// @details  
	// 상태가 없는(Stateless) 전역 메모리 할당자 기반이므로,  
	// 모든 StlAllocator 인스턴스끼리는 서로 교환/호환이 가능하여 항상 true/false를 반환합니다.  
	//***************************************************************************  
	template <typename U>
	bool operator==(const StlAllocator<U>&) const noexcept { return true; }

	//***************************************************************************
	// @brief 할당자 비동등성 비교 연산자 (C++ STL 컨테이너 호환성용)
	// @return false
	//***************************************************************************
	template <typename U>
	bool operator!=(const StlAllocator<U>&) const noexcept { return false; }
};

//***************************************************************************
// @brief PoolAllocator 기반 배치 생성(Placement New) 객체 할당 유틸리티.
//
// @details
// 메모리 풀에서 Type의 크기만큼 메모리를 가져온 후 Placement New를 수행합니다.
// 전역 ::new를 사용하여 Type 내의 멤버 placement new 오버로딩에 영향을 받지 않으며,
// 생성자 예외 발생 시 안전하게 메모리를 반납(Exception-Safe)합니다.
//
// @tparam Type 생성할 객체 타입
// @tparam Args 생성자 전달 가변 인자 타입
// @param args 생성자에 전달할 인자 목록
// @return 생성된 Type 객체의 포인터
//***************************************************************************
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = nullptr;

	// 1. Type 크기만큼의 raw 메모리를 확보합니다. 아직 Type 객체는 아니며,
	//    생성자가 실행되기 전까지는 단순 바이트 블록입니다.
	if constexpr( alignof(Type) > alignof(std::max_align_t) )
	{
		// 1-1. alignof(Type)이 기본 정렬(max_align_t)을 넘는 타입(SIMD 등
		//      alignas(32) 이상)은 PoolAllocator::AllocAligned로 정렬을
		//      보장받으며 할당합니다.
		memory = static_cast<Type*>(PoolAllocator::AllocAligned(sizeof(Type), alignof(Type)));
	}
	else
	{
		// 1-2. 그 외 일반적인 정렬 요구사항의 타입은 PoolAllocator::Alloc으로
		//      gpMemory 풀 경로를 그대로 탑니다.
		memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	}

	// 2. 확보한 메모리 위에 Type 생성자를 Placement New로 호출합니다.
	//    전역 ::new(memory)를 명시해, Type이 자체적으로 operator new(size_t, void*)를
	//    갖고 있더라도 그 오버로딩에 영향받지 않고 항상 표준 placement new가 선택되게 합니다.
	try
	{
		return ::new(memory) Type(std::forward<Args>(args)...);
	}
	catch( ... )
	{
		// 3. 생성자가 예외를 던지면 Type 객체는 완성되지 않은 상태이므로
		//    소멸자를 호출하지 않고, 1번에서 확보했던 메모리만 원래 경로로
		//    되돌려 누수를 막습니다(Exception-Safe).
		if constexpr( alignof(Type) > alignof(std::max_align_t) )
		{
			// 3-1. 1-1에서 AllocAligned로 받았던 경우 ReleaseAligned로 짝을 맞춥니다.
			PoolAllocator::ReleaseAligned(memory, alignof(Type));
		}
		else
		{
			// 3-2. 1-2에서 Alloc으로 받았던 경우 Release로 짝을 맞춥니다.
			PoolAllocator::Release(memory);
		}

		// 4. 원래 예외를 그대로 다시 던져 호출부가 실패를 알 수 있게 합니다.
		throw;
	}
}

//***************************************************************************
// @brief xnew()로 생성된 객체의 소멸자를 호출하고 메모리를 PoolAllocator에 반납합니다.
//
// @tparam Type 파괴할 객체 타입
// @param obj xnew()로 생성된 객체의 포인터
//***************************************************************************
template<typename Type>
void xdelete(Type* obj)
{
	// 1. nullptr은 아무 것도 하지 않고 즉시 반환합니다(표준 delete와 동일한 관례).
	if( obj == nullptr )
		return;

	// 2. 먼저 소멸자를 호출해 객체 상태를 정리합니다. 메모리를 먼저 반납하고
	//    소멸자를 나중에 부르면, 반납 직후 그 블록이 다른 스레드/호출에
	//    재사용된 뒤 소멸자가 남의 메모리를 건드리는 사고로 이어질 수 있어
	//    반드시 이 순서(소멸 → 반납)를 지킵니다.
	obj->~Type();

	// 3. xnew()가 1-1/1-2에서 어느 경로로 할당했는지에 맞춰 짝이 되는
	//    Release 계열 함수로 메모리를 되돌립니다.
	if constexpr( alignof(Type) > alignof(std::max_align_t) )
	{
		// 3-1. 1-1에서 AllocAligned로 받았던 경우 ReleaseAligned로 짝을 맞춥니다.
		PoolAllocator::ReleaseAligned(obj, alignof(Type));
	}
	else
	{
		// 3-2. 1-2에서 Alloc으로 받았던 경우 Release로 짝을 맞춥니다.
		PoolAllocator::Release(obj);
	}
}

//***************************************************************************
// @brief PoolAllocator 기반 단일 컨트롤 블록 메모리 할당을 수행하는 std::shared_ptr을 생성합니다.
//
// @details
// std::allocate_shared와 StlAllocator를 결합하여 SharedPtr Control Block과
// 객체를 단 1회의 메모리 풀 할당으로 조립합니다.
//
// [이런 기능에 사용하면 유리합니다]
// - 세션/커넥션 객체(CRioSession, CIocpSession 등): 접속·해제 빈도가
//   패킷 빈도보다 몇 자릿수 낮아, 그 타입만의 전용 풀을 따로 둘 실익이
//   적은 타입
// - 설정/컨텍스트 객체, 각각은 자주 생성되지 않는 다양한 소형 타입들:
//   타입마다 전용 풀을 만들면 풀 개수만 늘고 관리 비용만 커지는 경우
// - 아직 그 타입이 핫패스인지 확실치 않은 프로토타입/초기 구현 단계:
//   프로파일링으로 실제 핫스팟임이 확인되면 그때 CObjectPool<Type>로
//   승격을 검토
//
// gpMemory의 크기별 공유 풀과 TLS 배치 캐시 이점을 그대로 받는 대신,
// 크기대가 같은 다른 타입들과 풀을 함께 씁니다.
//
// @tparam Type 관리할 객체 타입
// @tparam Args 생성자 전달 가변 인자 타입
// @param args 생성자에 전달할 인자 목록
// @return StlAllocator 기반으로 완전히 통합된 std::shared_ptr<Type>
//***************************************************************************
template<typename Type, typename... Args>
std::shared_ptr<Type> MakeShared(Args&&... args)
{
	return std::allocate_shared<Type>(StlAllocator<Type>(), std::forward<Args>(args)...);
}

#endif // ndef UC_ALLOCATOR_H