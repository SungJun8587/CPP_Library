
//***************************************************************************
// Allocator.cpp
//
// 설명 : Allocator.h에서 선언한 BaseAllocator / StompAllocator / PoolAllocator의 실제 구현부.
//***************************************************************************

#include "pch.h"
#include "Allocator.h"


//***************************************************************************
// BaseAllocator
//***************************************************************************

//***************************************************************************
// 설명 : RawAllocator에 그대로 위임합니다. 컴파일 타임 매크로에 따라
//        mimalloc/jemalloc/tcmalloc/malloc 중 하나로 자동 분기되므로
//        이 함수 자체는 라이브러리 선택에 관여하지 않습니다.
void* BaseAllocator::Alloc(int32 size)
{
	return RawAllocator::Alloc(static_cast<size_t>(size));
}

//***************************************************************************
// 설명 : Alloc()으로 할당된 메모리를 RawAllocator를 통해 해제합니다.
void BaseAllocator::Release(void* ptr)
{
	RawAllocator::Free(ptr);
}


//***************************************************************************
// StompAllocator
//***************************************************************************

int8* StompAllocator::s_arenaBase = nullptr;
atomic<int8*>			StompAllocator::s_arenaCursor = nullptr;
once_flag				StompAllocator::s_arenaInitFlag;
shared_mutex			StompAllocator::s_sizeClassMapLock;
unordered_map<int64, unique_ptr<SLIST_HEADER>> StompAllocator::s_sizeClassFreeLists;

//***************************************************************************
// 설명 : 프로세스 생애 동안 정확히 한 번만 큰 가상 주소 공간을
//        VirtualAlloc(MEM_RESERVE)으로 예약합니다. 이 시점에는 아직
//        아무 페이지도 커밋되지 않으므로 물리 메모리는 소모되지 않습니다.
void StompAllocator::EnsureArenaInitialized()
{
	call_once(s_arenaInitFlag, []()
		{
			void* base = ::VirtualAlloc(NULL, static_cast<size_t>(ARENA_RESERVE_SIZE), MEM_RESERVE, PAGE_READWRITE);
			ASSERT_CRASH(base != nullptr);

			s_arenaBase = static_cast<int8*>(base);
			s_arenaCursor.store(s_arenaBase);
		});
}

//***************************************************************************
// 설명 : dataRegionSize에 대응하는 SLIST_HEADER를 맵에서 찾아 반환합니다.
//        먼저 공유 락(읽기)만으로 조회를 시도하고, 처음 등장하는 크기라면
//        배타 락으로 승격해 새로 만들어 등록합니다(이중 확인 잠금).
SLIST_HEADER* StompAllocator::GetOrCreateSizeClassFreeList(int64 dataRegionSize)
{
	{
		shared_lock<shared_mutex> readLock(s_sizeClassMapLock);
		auto it = s_sizeClassFreeLists.find(dataRegionSize);
		if( it != s_sizeClassFreeLists.end() )
			return it->second.get();
	}

	unique_lock<shared_mutex> writeLock(s_sizeClassMapLock);

	// 배타 락을 잡는 사이 다른 스레드가 먼저 등록했을 수 있으므로 재확인
	auto it = s_sizeClassFreeLists.find(dataRegionSize);
	if( it != s_sizeClassFreeLists.end() )
		return it->second.get();

	auto header = make_unique<SLIST_HEADER>();
	::InitializeSListHead(header.get());

	SLIST_HEADER* raw = header.get();
	s_sizeClassFreeLists.emplace(dataRegionSize, move(header));
	return raw;
}

//***************************************************************************
// 설명 : 요청 크기를 16단위로 올림해(반환 포인터가 MemoryHeader의 16바이트
//        정렬을 항상 만족하도록) 데이터 영역 크기를 구한 뒤, 같은 크기의
//        free-list에 재사용 대기 중인 영역이 있으면 데이터 페이지만
//        재커밋하고, 없으면 아레나에서 [메타데이터 1페이지 + 데이터
//        페이지들]을 새로 커밋합니다.
void* StompAllocator::Alloc(int32 size)
{
	EnsureArenaInitialized();

	const int64 alignedSize = (static_cast<int64>(size) + 15) & ~static_cast<int64>(15);
	const int64 dataRegionSize = ((alignedSize + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
	const int64 dataOffset = dataRegionSize - alignedSize;

	SLIST_HEADER* freeList = GetOrCreateSizeClassFreeList(dataRegionSize);
	RegionMeta* meta = static_cast<RegionMeta*>(::InterlockedPopEntrySList(freeList));

	if( meta != nullptr )
	{
		// 재사용 : 디커밋되어 있던 데이터 페이지만 다시 커밋
		void* committed = ::VirtualAlloc(meta->dataPagesBase, static_cast<size_t>(dataRegionSize), MEM_COMMIT, PAGE_READWRITE);
		ASSERT_CRASH(committed != nullptr);
	}
	else
	{
		// 새 영역 : 아레나에서 [메타데이터 1페이지 + 데이터 페이지들]만큼 커밋
		const int64 totalSize = PAGE_SIZE + dataRegionSize;
		int8* regionBase = s_arenaCursor.fetch_add(totalSize);

		// 아레나(ARENA_RESERVE_SIZE)를 넘어서면 더 이상 커밋할 예약 공간이
		// 없다는 뜻 - 아레나 크기를 늘리거나 _STOMP 사용 패턴을 점검해야 함
		ASSERT_CRASH(regionBase + totalSize <= s_arenaBase + ARENA_RESERVE_SIZE);

		void* committed = ::VirtualAlloc(regionBase, static_cast<size_t>(totalSize), MEM_COMMIT, PAGE_READWRITE);
		ASSERT_CRASH(committed != nullptr);

		meta = new(regionBase) RegionMeta(); // placement new
		meta->dataPagesBase = regionBase + PAGE_SIZE;
		meta->dataRegionSize = dataRegionSize;
	}

	meta->freed.store(0);

	return static_cast<void*>(meta->dataPagesBase + dataOffset);
}

//***************************************************************************
// 설명 : 데이터 포인터로부터 첫 데이터 페이지 시작 주소를 역산해(그
//        바로 앞 페이지가 메타데이터) 포인터 산술만으로 RegionMeta를
//        찾습니다. freed 플래그를 원자적으로 검사/설정해 이중 반납을
//        감지한 뒤, 데이터 페이지만 디커밋하고 free-list에 등록합니다.
void StompAllocator::Release(void* ptr)
{
	const int64 address = reinterpret_cast<int64>(ptr);
	int8* dataPagesBase = reinterpret_cast<int8*>(address - (address % PAGE_SIZE));
	RegionMeta* meta = reinterpret_cast<RegionMeta*>(dataPagesBase - PAGE_SIZE);

	// exchange(1)로 "읽고 반납 표시하기"를 단일 원자 연산으로 처리 -
	// 같은 영역에 대해 Release가 두 번(혹은 동시에) 호출되면 정확히 한
	// 번만 0을 받고 나머지는 이미 1이 된 값을 받아 즉시 걸립니다.
	const int32 wasFreed = meta->freed.exchange(1);
	ASSERT_CRASH(wasFreed == 0);

	::VirtualFree(meta->dataPagesBase, static_cast<size_t>(meta->dataRegionSize), MEM_DECOMMIT);

	SLIST_HEADER* freeList = GetOrCreateSizeClassFreeList(meta->dataRegionSize);
	::InterlockedPushEntrySList(freeList, static_cast<PSLIST_ENTRY>(meta));
}


//***************************************************************************
//	PoolAllocator
//***************************************************************************

//***************************************************************************
// 설명 : 전역 싱글턴 gpMemory에게 할당을 위임합니다. gpMemory가 사이즈에
//        맞는 메모리 풀을 찾아 블록을 반환합니다.
void* PoolAllocator::Alloc(int32 size)
{
	return gpMemory->Allocate(size);
}

//***************************************************************************
// 설명 : 전역 싱글턴 gpMemory에게 반납을 위임합니다. gpMemory가 헤더에
//        기록된 allocSize를 보고 원래 속했던 풀(혹은 raw 할당 여부)을
//        판별합니다.
void PoolAllocator::Release(void* ptr)
{
	gpMemory->Release(ptr);
}