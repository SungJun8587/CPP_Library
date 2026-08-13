
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

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#pragma once

#ifndef __RAWALLOCATOR_H__
#include <Memory/RawAllocator.h>
#endif

#include <new>           // std::align_val_t
#include <unordered_map> // StompAllocator의 크기별 free-list 맵
#include <shared_mutex>  // StompAllocator의 크기별 free-list 맵 보호 (SRWLock/shared_mutex)
#include <mutex>         // StompAllocator의 아레나 1회 초기화 (std::once_flag)

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
	static void* Alloc(int32 size);

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
	void* operator new(size_t size, align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	//***************************************************************************
	// @brief 확장 정렬(alignas)을 요구하는 배열 객체의 메모리 할당 연산자
	// @param size 할당할 바이트 크기
	// @param alignment 정렬 바이트 단위 (std::align_val_t)
	// @return 정렬 조건이 보장된 메모리 포인터
	//***************************************************************************
	void* operator new[](size_t size, align_val_t alignment)
	{
		return RawAllocator::AllocAligned(size, static_cast<size_t>(alignment));
	}

	//***************************************************************************
	// @brief 확장 정렬 메모리 해제 연산자 (단일 객체)
	// @param ptr 해제할 메모리 포인터
	//***************************************************************************
	void operator delete(void* ptr, align_val_t /*alignment*/)
	{
		RawAllocator::FreeAligned(ptr);
	}

	//***************************************************************************
	// @brief 확장 정렬 메모리 해제 연산자 (배열 객체)
	// @param ptr 해제할 메모리 포인터
	//***************************************************************************
	void operator delete[](void* ptr, align_val_t /*alignment*/)
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
	enum { PAGE_SIZE = 0x1000 };

	// 64비트 가상 주소 공간 예약 크기 (기본 256GB)
	static constexpr int64 ARENA_RESERVE_SIZE = static_cast<int64>(256) * 1024 * 1024 * 1024;

	//***************************************************************************
	// @struct RegionMeta
	// @brief 각 할당 영역의 메타데이터 및 Lock-Free Free-List 노드 구조체.
	//***************************************************************************
	struct RegionMeta : public SLIST_ENTRY
	{
		int8* dataPagesBase;   // 데이터 페이지 영역 시작 주소
		int64          dataRegionSize;  // 데이터 영역 크기 (PAGE_SIZE 단위)
		atomic<int32>  freed;           // 0: 사용 중, 1: 반납됨 (이중 해제 탐지용)
	};

public:
	//***************************************************************************
	// @brief Page-aligned 기반 디버그 메모리를 할당합니다.
	// @param size 요청할 데이터 크기 (바이트)
	// @return 페이지 끝단에 맞춤 정렬된 메모리 포인터
	//***************************************************************************
	static void* Alloc(int32 size);

	//***************************************************************************
	// @brief StompAllocator로 할당받은 디버그 메모리를 반납합니다.
	// @param ptr 반납할 메모리 포인터
	//***************************************************************************
	static void Release(void* ptr);

private:
	//***************************************************************************
	// @brief 가상 메모리 아레나 공간을 최초 1회만 스레드 안전하게 초기화합니다.
	//***************************************************************************
	static void EnsureArenaInitialized();

	//***************************************************************************
	// @brief 크기 부류(Size Class)에 맞는 Free-List 헤더 포인터를 조회하거나 생성합니다.
	// @param dataRegionSize 데이터 영역 크기
	// @return 해상 크기 부류의 Lock-Free SLIST_HEADER 포인터
	//***************************************************************************
	static SLIST_HEADER* GetOrCreateSizeClassFreeList(int64 dataRegionSize);

private:
	static int8* s_arenaBase;          // 아레나 시작 주소
	static atomic<int8*>                                      s_arenaCursor;        // 현재 커서 오프셋
	static once_flag                                          s_arenaInitFlag;      // 1회 초기화 플래그
	static shared_mutex                                       s_sizeClassMapLock;   // Free-List 맵 동기화 락
	static unordered_map<int64, unique_ptr<SLIST_HEADER>>     s_sizeClassFreeLists; // 크기별 Free-List 맵
};

//***************************************************************************
// @class PoolAllocator
// @brief 고속 메모리 풀(CMemoryPool)을 활용하는 프로젝트 기본 핫패스 할당자.
//
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
	static void* Alloc(int32 size);

	//***************************************************************************
	// @brief 할당받은 메모리 블록을 CMemoryPool에 반납합니다.
	// @param ptr 반납할 메모리 포인터
	//***************************************************************************
	static void Release(void* ptr);
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
	StlAllocator() {}

	//***************************************************************************
	// @brief 서로 다른 타입의 StlAllocator 간 리바인딩(Rebind) 변환 생성자
	//***************************************************************************
	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) {}

	//***************************************************************************
	// @brief 서로 다른 타입의 StlAllocator 대입 연산자
	//***************************************************************************
	template<typename Other>
	StlAllocator<T>& operator=(const StlAllocator<Other>&)
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
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	//***************************************************************************
	// @brief allocate()로 할당받은 메모리를 PoolAllocator에 반납합니다.
	// @param ptr 해제할 메모리 포인터
	// @param count (미사용) 원소 개수
	//***************************************************************************
	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}

	//***************************************************************************
	// @brief 할당자 동등성 비교 연산자 (C++ STL 컨테이너 호환성용)
	// @details
	// std::vector 등의 STL 컨테이너가 swap, move 대입 또는 할당자 호환성 검사를 수행할 때
	// 서로 다른 컨테이너의 할당자가 동일한 메모리 풀/메커니즘을 사용하는지 확인하는 연산자입니다.
	//
	// 상태가 없는(Stateless) 전역 메모리 할당자 기반이므로,
	// 모든 StlAllocator 인스턴스끼리는 서로 교환/호환이 가능하여 항상 true/false를 반환합니다.
	//***************************************************************************
	template <typename U>
	bool operator==(const StlAllocator<U>&) const noexcept { return true; }

	template <typename U>
	bool operator!=(const StlAllocator<U>&) const noexcept { return false; }
};

//***************************************************************************
// @brief PoolAllocator 기반 배치 생성(Placement New) 객체 할당 유틸리티.
//
// @details
// 메모리 풀에서 Type의 크기만큼 메모리를 가져온 후 Placement New를 수행합니다.
// std::forward 호출 오버헤드를 피하기 위해 static_cast<Args&&>(args)... 형태로 직접 인자를 전달합니다.
//
// @tparam Type 생성할 객체 타입
// @tparam Args 생성자 전달 가변 인자 타입
// @param args 객체 생성자에 전달할 인자 목록
// @return 생성된 Type 객체의 포인터
//***************************************************************************
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
	Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
	new(memory)Type(static_cast<Args&&>(args)...);
	return memory;
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
	if( obj == nullptr )
		return;

	obj->~Type();
	PoolAllocator::Release(obj);
}

//***************************************************************************
// @brief PoolAllocator 기반 커스텀 삭제자(xdelete)가 연결된 std::shared_ptr을 생성합니다.
//
// @details
// C++ 표준 std::make_shared 대신 이 함수를 사용하면 shared_ptr을 이용하면서도
// 메모리 풀 경로를 그대로 활용할 수 있습니다.
//
// @tparam Type 관리할 객체 타입
// @tparam Args 생성자 전달 가변 인자 타입
// @param args 객체 생성자에 전달할 인자 목록
// @return 커스텀 삭제자가 지정된 std::shared_ptr<Type>
//***************************************************************************
template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
	return shared_ptr<Type>{ xnew<Type>(static_cast<Args&&>(args)...), xdelete<Type> };
}

#endif // ndef __ALLOCATOR_H__