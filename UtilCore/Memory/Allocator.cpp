
//***************************************************************************
// Allocator.cpp
//
// 설명 : Allocator.h에서 선언한 BaseAllocator / StompAllocator / PoolAllocator의 실제 구현부.
//***************************************************************************

#include "pch.h"
#include "Allocator.h"

/*-------------------
	BaseAllocator
-------------------*/

// 설명 : RawAllocator에 그대로 위임합니다. 컴파일 타임 매크로에 따라
//        mimalloc/jemalloc/tcmalloc/malloc 중 하나로 자동 분기되므로
//        이 함수 자체는 라이브러리 선택에 관여하지 않습니다.
void* BaseAllocator::Alloc(int32 size)
{
	return RawAllocator::Alloc(static_cast<size_t>(size));
}

// 설명 : Alloc()으로 할당된 메모리를 RawAllocator를 통해 해제합니다.
void BaseAllocator::Release(void* ptr)
{
	RawAllocator::Free(ptr);
}

/*-------------------
	StompAllocator
-------------------*/

// 설명 : size 바이트를 담기 위해 필요한 페이지 수를 계산하고, 그 페이지들을
//        VirtualAlloc으로 예약/커밋합니다. 데이터 시작 위치를 페이지의
//        "끝"에 맞춰 오프셋을 계산함으로써, 오버런이 발생하면 바로 다음
//        미매핑 페이지에 접근해 즉시 크래시가 나도록 유도합니다.
//
//        페이지 단위 가드가 목적이므로 RawAllocator(mimalloc 등 힙 할당기)로
//        대체하지 않고 VirtualAlloc을 직접 사용합니다. 이는 성능이 아니라
//        "메모리 손상의 즉각적 진단"이 목적이기 때문입니다.
void* StompAllocator::Alloc(int32 size)
{
	const int64 pageCount = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	const int64 dataOffset = pageCount * PAGE_SIZE - size;

	void* baseAddress = ::VirtualAlloc(NULL, pageCount * PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	// VirtualAlloc은 실패 시 nullptr을 반환할 수 있음 - 이후 포인터 연산 전에
	// 반드시 확인하여 잘못된 주소 접근을 방지합니다.
	ASSERT_CRASH(baseAddress != nullptr);

	return static_cast<void*>(static_cast<int8*>(baseAddress) + dataOffset);
}

// 설명 : Alloc()이 반환한 데이터 포인터로부터 원래의 페이지 시작 주소를
//        역산하여(주소를 PAGE_SIZE로 내림 정렬) VirtualFree로 페이지 전체를
//        예약 해제합니다.
void StompAllocator::Release(void* ptr)
{
	const int64 address = reinterpret_cast<int64>(ptr);
	const int64 baseAddress = address - (address % PAGE_SIZE);
	::VirtualFree(reinterpret_cast<void*>(baseAddress), 0, MEM_RELEASE);
}

/*-------------------
	PoolAllocator
-------------------*/

// 설명 : 전역 싱글턴 gpMemory에게 할당을 위임합니다. gpMemory가 사이즈에
//        맞는 메모리 풀을 찾아 블록을 반환합니다.
void* PoolAllocator::Alloc(int32 size)
{
	return gpMemory->Allocate(size);
}

// 설명 : 전역 싱글턴 gpMemory에게 반납을 위임합니다. gpMemory가 헤더에
//        기록된 allocSize를 보고 원래 속했던 풀(혹은 raw 할당 여부)을
//        판별합니다.
void PoolAllocator::Release(void* ptr)
{
	gpMemory->Release(ptr);
}
