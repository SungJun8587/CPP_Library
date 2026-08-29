//***************************************************************************
// Allocator.cpp
//
// @brief Allocator.h에 정의된 메인 메모리 할당자들(BaseAllocator, StompAllocator,
//        PoolAllocator)의 상세 로직을 구현합니다.
//***************************************************************************

#include "pch.h"
#include "Allocator.h"

#include <limits>

//***************************************************************************
// BaseAllocator 구현부
//***************************************************************************

//***************************************************************************
// @brief Raw 메모리를 지정한 크기만큼 할당받습니다.
// @param size 할당할 바이트 크기
// @return 할당된 메모리의 시작 포인터
//***************************************************************************
void* BaseAllocator::Alloc(size_t size)
{
	ASSERT_CRASH(size > 0);

	return RawAllocator::Alloc(size);

}

//***************************************************************************
// @brief Alloc으로 할당받은 Raw 메모리를 해제합니다.
// @param ptr 해제할 메모리 포인터
//***************************************************************************
void BaseAllocator::Release(void* ptr)
{
	RawAllocator::Free(ptr);
}


//***************************************************************************
// StompAllocator 정적 변수 정의
//***************************************************************************

int8* StompAllocator::s_arenaBase = nullptr;

atomic<int8*> StompAllocator::s_arenaCursor = nullptr;

once_flag StompAllocator::s_arenaInitFlag;

shared_mutex StompAllocator::s_sizeClassMapLock;

unordered_map<size_t, unique_ptr<SLIST_HEADER>> StompAllocator::s_sizeClassFreeLists;

atomic<bool> StompAllocator::s_isCleanedUp = false;

//***************************************************************************
// StompAllocator 구현부
//***************************************************************************

//***************************************************************************
// @brief 가상 메모리 아레나 공간을 최초 1회만 스레드 안전하게 초기화합니다.
//
// @details
// std::call_once를 활용하여 프로세스 수명 동안 256GB 가상 주소 공간(MEM_RESERVE)을
// 단 1회 예약하며, 커서 포인터(s_arenaCursor)를 아레나 시작 주소로 세팅합니다.
//***************************************************************************
void StompAllocator::EnsureArenaInitialized()
{
	call_once(
		s_arenaInitFlag,
		[]
		{
			void* base = ::VirtualAlloc(
				nullptr,
				ARENA_RESERVE_SIZE,
				MEM_RESERVE,
				PAGE_READWRITE);

			ASSERT_CRASH(base != nullptr);

			s_arenaBase = static_cast<int8*>(base);

			s_arenaCursor.store(
				s_arenaBase,
				memory_order_release);
		});
}

//***************************************************************************
// @brief 크기 부류(Size Class)에 맞는 Free-List 헤더 포인터를 조회하거나 생성합니다.
//
// @details
// 이중 검수 락킹(Double-Checked Locking)과 SRWLock(shared_mutex)을 사용하여
// 읽기 작업 시에는 공유 락, 신규 Size Class 생성 시에는 독점 락을 획득합니다.
//
// @param dataRegionSize 데이터 영역 크기
// @return 해당 크기 부류의 Lock-Free SLIST_HEADER 포인터
//***************************************************************************
SLIST_HEADER* StompAllocator::GetOrCreateSizeClassFreeList(size_t dataRegionSize)
{
	{
		shared_lock<shared_mutex> readLock(s_sizeClassMapLock);

		auto it = s_sizeClassFreeLists.find(dataRegionSize);
		if( it != s_sizeClassFreeLists.end() )
			return it->second.get();
	}

	unique_lock<shared_mutex> writeLock(s_sizeClassMapLock);

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
// @brief Arena에서 연속된 가상 주소 영역을 원자적(CAS)으로 예약합니다.
//
// @details
// 원자적 Compare-And-Swap 루프를 통해 멀티스레드 환경에서 아레나 커서 오프셋을
// 스레드 안전하게 전진시키고 오버플로우 발생 여부를 검증합니다.
// 만약 256GB 공간이 다 차면 nullptr를 반납하여 Direct Fallback을 유도합니다.
//
// @param size 예약받고자 하는 총 영역 크기
// @return 예약된 아레나 오프셋의 시작 포인터 (고갈 시 nullptr)
//***************************************************************************
int8* StompAllocator::ReserveArenaRegion(size_t size)
{
	ASSERT_CRASH(size > 0);

	if( size > ARENA_RESERVE_SIZE )
		return nullptr;

	const uintptr_t arenaBegin = reinterpret_cast<uintptr_t>(s_arenaBase);
	const uintptr_t arenaEnd = arenaBegin + ARENA_RESERVE_SIZE;

	int8* current = s_arenaCursor.load(memory_order_acquire);

	for( ;;)
	{
		const uintptr_t currentAddress = reinterpret_cast<uintptr_t>(current);

		if( currentAddress < arenaBegin || currentAddress > arenaEnd )
			return nullptr;

		const size_t remaining = static_cast<size_t>(arenaEnd - currentAddress);
		if( size > remaining )
		{
			// 256GB Arena 주소 공간 초과  
			return nullptr;
		}

		const uintptr_t nextAddress = currentAddress + size;

		int8* next = reinterpret_cast<int8*>(nextAddress);

		if( s_arenaCursor.compare_exchange_weak(
			current,
			next,
			memory_order_acq_rel,
			memory_order_acquire) )
		{
			return current;
		}
	}
}

//***************************************************************************
// @brief Page-aligned 기반 디버그 메모리를 할당합니다.
//
// @details
// 1. 요청 크기의 산술 오버플로우를 정교하게 방어합니다.
// 2. 동일한 크기 부류의 Free-List(SLIST)에 반납된 블록이 존재하면 재사용합니다.
// 3. 그렇지 않을 경우 아레나 커서를 이동시켜 가상 메모리를 Commit합니다.
//    (아레나 주소 공간 고갈 시 Direct VirtualAlloc Fallback 경로를 탑니다.)
// 4. 할당된 데이터의 끝단을 페이지 경계(Boundary) 바로 앞에 배치하고
//    다음 페이지 접근 시 크래시가 유발되도록 배치합니다.
//
// @param size 요청할 데이터 크기 (바이트)
// @return 페이지 끝단에 맞춤 정렬된 메모리 포인터
//***************************************************************************
void* StompAllocator::Alloc(size_t size)
{
	ASSERT_CRASH(size > 0);
	// Cleanup() 이후의 재사용은 계약 위반이므로, 재현 어려운 형태로 나중에
	// 크래시하는 대신 여기서 즉시 명확하게 잡습니다.
	ASSERT_CRASH(!s_isCleanedUp.load(memory_order_relaxed));

	// 1. 기본 정렬 overflow 검사  
	ASSERT_CRASH(size <= (std::numeric_limits<size_t>::max)() - (ALIGNMENT - 1));

	const size_t alignedSize = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

	// 2. AllocationHeader 추가에 대한 overflow 검사  
	ASSERT_CRASH(alignedSize <= (std::numeric_limits<size_t>::max)() - sizeof(AllocationHeader));

	const size_t sizeWithHeader = alignedSize + sizeof(AllocationHeader);

	// 3. 페이지 단위 올림 overflow 검사  
	ASSERT_CRASH(sizeWithHeader <= (std::numeric_limits<size_t>::max)() - (PAGE_SIZE - 1));

	const size_t dataRegionSize = ((sizeWithHeader + (PAGE_SIZE - 1)) / PAGE_SIZE) * PAGE_SIZE;

	ASSERT_CRASH(dataRegionSize <= (std::numeric_limits<size_t>::max)() - PAGE_SIZE);

	// RegionMeta를 더 이상 아레나 내부("메타 페이지")에 두지 않으므로,
	// 실제로 아레나에서 커밋되는 영역은 데이터 페이지뿐입니다.
	const size_t committedRegionSize = dataRegionSize;

	ASSERT_CRASH(committedRegionSize <= (std::numeric_limits<size_t>::max)() - PAGE_SIZE);

	// 예약(주소 공간)만 하고 절대 커밋하지 않는 가드 페이지(PAGE_SIZE)를
	// committedRegionSize 뒤에 덧붙여 함께 예약합니다. 이 가드 페이지는
	// 아레나 범프 커서가 건너뛰어 다음 할당에 절대 재사용되지 않으므로,
	// "다음 할당이 아직 이 자리를 커밋하지 않았는가"라는 타이밍에 기대지
	// 않고도 데이터 영역 끝을 넘는 접근이 항상 접근 위반으로 크래시합니다.
	const size_t reserveRegionSize = committedRegionSize + PAGE_SIZE;

	// 4. Arena 초기화  
	EnsureArenaInitialized();

	// 5. 동일 Size Class free-list에서 재사용  
	SLIST_HEADER* freeList = GetOrCreateSizeClassFreeList(dataRegionSize);
	RegionMeta* meta = static_cast<RegionMeta*>(::InterlockedPopEntrySList(freeList));

	if( meta != nullptr )
	{
		// 가드 페이지는 최초 생성 시 이미 예약된 채 한 번도 커밋되지
		// 않았으므로, 재사용 시에는 데이터 페이지만 다시 커밋하면 됩니다.
		// (RegionMeta 자체는 힙에 있으므로 항상 유효합니다.)
		void* committed = ::VirtualAlloc(
			meta->dataPagesBase,
			meta->dataRegionSize,
			MEM_COMMIT,
			PAGE_READWRITE);

		ASSERT_CRASH(committed != nullptr);
		ASSERT_CRASH(committed == meta->dataPagesBase);

		meta->isFallback = false;
	}
	else
	{
		// 6. 신규 Arena 영역 예약 (데이터 + 가드 페이지 크기로 예약)  
		int8* regionBase = ReserveArenaRegion(reserveRegionSize);
		bool isFallback = false;

		// 256GB 아레나 한계 초과 시 Direct System Allocation Fallback  
		if( regionBase == nullptr )
		{
			// 가드 페이지를 포함해 통째로 주소 공간만 예약한 뒤,
			// 커밋은 가드 페이지를 제외한 committedRegionSize만 수행합니다.
			regionBase = static_cast<int8*>(::VirtualAlloc(
				nullptr,
				reserveRegionSize,
				MEM_RESERVE,
				PAGE_NOACCESS));

			ASSERT_CRASH(regionBase != nullptr);

			void* committed = ::VirtualAlloc(
				regionBase,
				committedRegionSize,
				MEM_COMMIT,
				PAGE_READWRITE);

			ASSERT_CRASH(committed != nullptr);
			ASSERT_CRASH(committed == regionBase);

			isFallback = true;
		}
		else
		{
			// 가드 페이지를 제외한 앞쪽 committedRegionSize만 커밋합니다.
			void* committed = ::VirtualAlloc(
				regionBase,
				committedRegionSize,
				MEM_COMMIT,
				PAGE_READWRITE);

			ASSERT_CRASH(committed != nullptr);
			ASSERT_CRASH(committed == regionBase);
		}

		// RegionMeta는 아레나가 아니라 일반 힙에서 할당합니다 - Release() 시
		// 데이터 페이지 전체를 완전히 반납할 수 있도록 하기 위함입니다.
		// SLIST 노드로 직접 쓰이므로 반드시 AllocAligned(ALIGNMENT)로 받아와야
		// InterlockedPush/PopEntrySList가 요구하는 정렬이 보장됩니다.
		meta = static_cast<RegionMeta*>(RawAllocator::AllocAligned(sizeof(RegionMeta), ALIGNMENT));
		ASSERT_CRASH(meta != nullptr);
		::new(meta) RegionMeta();

		meta->dataPagesBase = regionBase;
		meta->dataRegionSize = dataRegionSize;
		meta->isFallback = isFallback;
	}

	// 7. Data 영역 끝에 AllocationHeader + User Memory를 배치  
	ASSERT_CRASH(dataRegionSize >= sizeWithHeader);

	const size_t dataOffset = dataRegionSize - sizeWithHeader;
	int8* headerAddress = meta->dataPagesBase + dataOffset;

	AllocationHeader* header = reinterpret_cast<AllocationHeader*>(headerAddress);
	int8* userAddress = headerAddress + sizeof(AllocationHeader);

	// Alignment 검증  
	ASSERT_CRASH((reinterpret_cast<uintptr_t>(headerAddress) % ALIGNMENT) == 0);
	ASSERT_CRASH((reinterpret_cast<uintptr_t>(userAddress) % ALIGNMENT) == 0);

	// Header 및 Meta 초기화  
	header->meta = meta;
	header->magic = ALLOCATION_MAGIC;

	meta->freed.store(0, memory_order_release);

	return static_cast<void*>(userAddress);
}

//***************************************************************************
// @brief StompAllocator로 할당받은 디버그 메모리를 반납합니다.
//
// @details
// 1. 매직 넘버(ALLOCATION_MAGIC)를 검증하여 유효하지 않은 메모리 해제를 차단합니다.
// 2. Atomic exchange를 통해 이중 해제(Double Free) 발생 시 원자적으로 탐지하여 크래시합니다.
// 3. 실제 데이터가 존재하던 페이지를 MEM_DECOMMIT 처리하여 해제 직후
//    해당 영역에 대한 읽기/쓰기 접근(Use-After-Free)을 즉시 크래시시킵니다.
// 4. Fallback 할당인 경우 완전히 해제하고, 일반 Arena 할당은 Lock-Free Free-List(SLIST)에 반납합니다.
//
// @param ptr 반납할 메모리 포인터
//***************************************************************************
void StompAllocator::Release(void* ptr)
{
	if( ptr == nullptr )
		return;

	// Cleanup() 이후의 재사용은 계약 위반이므로 즉시 명확하게 잡습니다.
	ASSERT_CRASH(!s_isCleanedUp.load(memory_order_relaxed));

	// 1. User pointer에서 Header 복원  
	int8* userAddress = static_cast<int8*>(ptr);
	AllocationHeader* header = reinterpret_cast<AllocationHeader*>(
		userAddress - sizeof(AllocationHeader));

	// 2. Magic 검증  
	ASSERT_CRASH(header->magic == ALLOCATION_MAGIC);

	// 3. RegionMeta 복원  
	RegionMeta* meta = header->meta;
	ASSERT_CRASH(meta != nullptr);

	// 4. Double Free 검사  
	const int32 wasFreed = meta->freed.exchange(1, memory_order_acq_rel);
	ASSERT_CRASH(wasFreed == 0);

	// 5. Header invalidate  
	header->magic = 0;

	// 6. Fallback 할당인 경우 전체 Release  
	if( meta->isFallback )
	{
		// RegionMeta가 더 이상 아레나의 첫 페이지가 아니라 dataPagesBase가
		// 곧 예약 영역의 시작 주소이므로 별도 오프셋 보정이 필요 없습니다.
		int8* regionBase = meta->dataPagesBase;
		::VirtualFree(regionBase, 0, MEM_RELEASE);

		// Fallback 메타는 free-list에 재사용 등록되지 않으므로(아레나 밖 1회성
		// 할당) 힙에서 직접 해제합니다. AllocAligned로 받았으므로 FreeAligned로 짝을 맞춥니다.
		meta->~RegionMeta();
		RawAllocator::FreeAligned(meta);
		return;
	}

	// 7. Data Page 전체 DECOMMIT (RegionMeta는 힙에 있어 항상 유효)  
	::VirtualFree(
		meta->dataPagesBase,
		meta->dataRegionSize,
		MEM_DECOMMIT);

	// 8. Size Class free-list에 RegionMeta 등록  
	SLIST_HEADER* freeList = GetOrCreateSizeClassFreeList(meta->dataRegionSize);
	::InterlockedPushEntrySList(freeList, static_cast<PSLIST_ENTRY>(meta));
}

//***************************************************************************
// @brief StompAllocator의 모든 내부 리소스를 해제합니다.
// @details
// Cleanup()은 StompAllocator의 terminal cleanup입니다.
// 호출 이후에는 StompAllocator를 다시 사용할 수 없습니다.
// Cleanup() 호출 시점에는 모든 Alloc/Release 작업이 종료되어 있어야 하며,
// 일반적으로 프로세스 또는 메모리 시스템 종료 직전에 단 한 번 호출합니다.
//***************************************************************************
void StompAllocator::Cleanup()
{
	unique_lock<shared_mutex> writeLock(s_sizeClassMapLock);

	// 중복 Cleanup() 호출도 동일한 계약 위반이므로 즉시 잡습니다.
	ASSERT_CRASH(!s_isCleanedUp.load(memory_order_relaxed));

	for( auto& [sizeClass, freeListHeader] : s_sizeClassFreeLists )
	{
		if( freeListHeader == nullptr )
			continue;

		// RegionMeta는 이제 아레나가 아니라 힙에서 할당되므로, 아레나를
		// 통째로 VirtualFree하는 것만으로는 회수되지 않습니다. free-list에
		// 남은 노드를 모두 꺼내 명시적으로 해제해야 합니다(AllocAligned로
		// 받았으므로 FreeAligned로 짝을 맞춥니다).
		while( RegionMeta* meta = static_cast<RegionMeta*>(::InterlockedPopEntrySList(freeListHeader.get())) )
		{
			meta->~RegionMeta();
			RawAllocator::FreeAligned(meta);
		}
	}

	s_sizeClassFreeLists.clear();

	if( s_arenaBase != nullptr )
	{
		::VirtualFree(s_arenaBase, 0, MEM_RELEASE);
		s_arenaBase = nullptr;
		s_arenaCursor.store(nullptr, memory_order_relaxed);
	}

	s_isCleanedUp.store(true, memory_order_relaxed);
}


//***************************************************************************
// PoolAllocator 구현부
//***************************************************************************

//***************************************************************************
// @brief CMemoryPool에서 적절한 크기의 메모리 블록을 할당받습니다.
// @param size 할당 요청 크기 (바이트)
// @return 메모리 블록 시작 포인터
//***************************************************************************
void* PoolAllocator::Alloc(size_t size)
{
	ASSERT_CRASH(size > 0);

#if defined(USE_GPMEMORY)
	return gpMemory->Allocate(size);
#else
	return ::operator new(size);
#endif
}

//***************************************************************************
// @brief 지정된 정렬 조건을 만족하는 메모리 블록을 할당받습니다.
//
// @details
// gpMemory(CMemory)는 내부적으로 SLIST_ALIGNMENT(16바이트) 단위로만 블록을
// 관리하므로, 그보다 큰 정렬을 요구하는 타입을 raw 정렬 할당으로 완전히
// 우회시키면 그런 타입은 풀링의 이점을 전혀 받지 못합니다. 대신 여기서는
// "정렬 여유분 + 원본 포인터 저장 공간"을 포함해 gpMemory에서 통상 크기로
// 할당받은 뒤, 반환 직전 주소를 요청 정렬에 맞춰 올림 보정합니다. 원본
// 포인터는 보정된 반환 주소 바로 앞 8바이트에 저장해 ReleaseAligned()에서
// 역산할 수 있게 합니다. gpMemory 크기별 풀을 그대로 경유하므로 반복
// 할당/해제 패턴에서 여전히 풀링 이점을 얻습니다.
//
// @param size 할당 요청 크기 (바이트)
// @param alignment 요구 정렬 바이트 단위
// @return 정렬 조건이 보장된 메모리 블록 시작 포인터
//***************************************************************************
void* PoolAllocator::AllocAligned(size_t size, size_t alignment)
{
	ASSERT_CRASH(size > 0);
	ASSERT_CRASH(alignment >= alignof(std::max_align_t));
	ASSERT_CRASH((alignment & (alignment - 1)) == 0);

#if defined(USE_GPMEMORY)
	// 정렬 올림으로 최대 (alignment - 1)바이트가 더 필요할 수 있고,
	// 반환 주소 바로 앞에 원본 포인터(void*)를 저장할 공간도 필요합니다.
	ASSERT_CRASH(size <= (std::numeric_limits<size_t>::max)() - alignment - sizeof(void*));

	const size_t totalSize = size + alignment + sizeof(void*);

	void* raw = gpMemory->Allocate(totalSize);
	ASSERT_CRASH(raw != nullptr);

	const uintptr_t searchStart = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
	const uintptr_t alignedAddr = (searchStart + alignment - 1) & ~(alignment - 1);

	// 원본 포인터를 보정된 주소 바로 앞 8바이트에 저장 (ReleaseAligned에서 역산용)
	*(reinterpret_cast<void**>(alignedAddr) - 1) = raw;

	return reinterpret_cast<void*>(alignedAddr);
#else
	return ::operator new(size, std::align_val_t(alignment));
#endif
}

//***************************************************************************
// @brief 할당받은 메모리 블록을 CMemoryPool에 반납합니다.
// @param ptr 반납할 메모리 포인터
//***************************************************************************
void PoolAllocator::Release(void* ptr)
{
	if( ptr == nullptr )
		return;

#if defined(USE_GPMEMORY)
	gpMemory->Release(ptr);
#else
	::operator delete(ptr);
#endif
}

//***************************************************************************
// @brief 확장 정렬로 할당받은 메모리 블록을 반납합니다.
// @param ptr 할당받은 메모리 포인터
// @param alignment 할당 시 사용한 정렬 바이트 단위 (미사용 - 원본 포인터로 역산하므로 검증 용도)
//***************************************************************************
void PoolAllocator::ReleaseAligned(void* ptr, size_t alignment)
{
	if( ptr == nullptr )
		return;

	ASSERT_CRASH(alignment >= alignof(std::max_align_t));
	ASSERT_CRASH((alignment & (alignment - 1)) == 0);

#if defined(USE_GPMEMORY)
	// AllocAligned()이 보정된 주소 바로 앞 8바이트에 저장해 둔 원본
	// gpMemory 할당 포인터를 역산해 그대로 반납합니다.
	void* raw = *(reinterpret_cast<void**>(ptr) - 1);
	gpMemory->Release(raw);
#else
	::operator delete(ptr, std::align_val_t(alignment));
#endif
}