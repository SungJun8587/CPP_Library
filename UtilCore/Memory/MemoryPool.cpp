
//***************************************************************************
// MemoryPool.cpp
//
// @brief MemoryPool.h에서 선언한 CMemoryPool 클래스의 실제 구현부.
// @details
// Raw 메모리 확보/해제는 RawAllocator를 통해 수행하여, 실제 사용할
// malloc 라이브러리(mimalloc 등)를 컴파일 타임 매크로로 교체할 수
// 있도록 합니다.
//***************************************************************************

#include "pch.h"
#include "MemoryPool.h"

//***************************************************************************
// @brief CMemoryPool 생성자.
// @details 이 풀이 다룰 고정 블록 크기(_allocSize)를 지정하고, SLIST 헤더를 초기화합니다.
// @param allocSize 이 풀에서 다룰 고정 블록 크기(헤더 포함, 바이트)
//***************************************************************************
CMemoryPool::CMemoryPool(size_t allocSize) : _allocSize(allocSize)
{
	ASSERT_CRASH(allocSize >= sizeof(MemoryHeader));
	ASSERT_CRASH((allocSize % SLIST_ALIGNMENT) == 0);

	::InitializeSListHead(&_header);
}

//***************************************************************************
// @brief CMemoryPool 소멸자.
// @details 소멸 시 SLIST에 남아있는 모든 블록을 꺼내어 RawAllocator로 실제 해제합니다.
//***************************************************************************
CMemoryPool::~CMemoryPool()
{
	while( MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header)) )
		RawAllocator::FreeAligned(memory);
}

//***************************************************************************
// @brief 블록을 사용 종료 표시(allocSize = 0)한 뒤 Lock-free SLIST에 되돌립니다.
// @details 통계용 카운터(_poolOutstandingCount / _poolReserveCount)도 함께 갱신합니다.
// @param ptr 반납할 메모리 블록(헤더) 포인터
//***************************************************************************
void CMemoryPool::Push(MemoryHeader* ptr)
{
	ASSERT_CRASH(ptr != nullptr);

	ptr->allocSize.store(0, std::memory_order_relaxed);

	::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

	_poolCheckedOutCount.fetch_sub(1, std::memory_order_relaxed);
	_poolReserveCount.fetch_add(1, std::memory_order_relaxed);
}

//***************************************************************************
// @brief WarmUp 단계에서 새로 생성한 메모리 블록을 풀에 미리 채워넣습니다.
// @details 일반 Push와 달리 사용 중 카운트(_poolOutstandingCount)를 감소시키지 않고 대기 수량만 증가시킵니다.
// @param ptr 채워넣을 메모리 블록(헤더) 포인터
//***************************************************************************
void CMemoryPool::WarmUpPush(MemoryHeader* ptr)
{
	ASSERT_CRASH(ptr != nullptr);

	ptr->allocSize.store(0, std::memory_order_relaxed);

	::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

	_poolReserveCount.fetch_add(1, std::memory_order_relaxed);
}

//***************************************************************************
// @brief SLIST에서 블록을 하나 꺼내오며, 풀이 비어있으면 RawAllocator에서 새로 할당받아 반환합니다.
// @return 사용 가능한 메모리 블록(헤더) 포인터
//***************************************************************************
MemoryHeader* CMemoryPool::Pop()
{
	MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header));

	// 풀이 비어있으면 새로 raw 할당
	if( memory == nullptr )
	{
		memory = reinterpret_cast<MemoryHeader*>(
			RawAllocator::AllocAligned(_allocSize, SLIST_ALIGNMENT));

		// 할당 실패(OOM) 방어 체크 - nullptr 상태로 이후 로직이 진행되는 것을 차단
		ASSERT_CRASH(memory != nullptr);
	}
	else
	{
		// 풀에서 꺼낸 블록은 반드시 Push 시 allocSize가 0으로 초기화되어 있어야 함
		ASSERT_CRASH(memory->allocSize.load(std::memory_order_relaxed) == 0);
		_poolReserveCount.fetch_sub(1, std::memory_order_relaxed);
	}

	_poolCheckedOutCount.fetch_add(1, std::memory_order_relaxed);

	return memory;
}