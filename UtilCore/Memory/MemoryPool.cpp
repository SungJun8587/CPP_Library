//***************************************************************************
// MemoryPool.cpp
//
// 설명 : MemoryPool.h에서 선언한 CMemoryPool의 실제 구현부.
//        raw 메모리 확보/해제는 RawAllocator를 통해 수행하여, 실제 사용할
//        malloc 라이브러리(mimalloc 등)를 컴파일 타임 매크로로 교체할 수
//        있도록 합니다.
//***************************************************************************

#include "pch.h"
#include "MemoryPool.h"
#include "RawAllocator.h"

/*-----------------
	CMemoryPool
------------------*/

// 설명 : 고정 블록 크기를 기록하고 Lock-free SLIST 헤더를 초기화합니다.
CMemoryPool::CMemoryPool(int32 allocSize) : _allocSize(allocSize)
{
	::InitializeSListHead(&_header);
}

// 설명 : 소멸 시 SLIST에 남아있는 모든 블록을 순서대로 꺼내어 실제
//        raw 메모리로 해제합니다(프로세스 종료/풀 재구성 시 누수 방지).
CMemoryPool::~CMemoryPool()
{
	while (MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header)))
		RawAllocator::FreeAligned(memory);
}

// 설명 : 블록을 "미사용" 상태로 표시(allocSize = 0)한 뒤 Lock-free SLIST에
//        되돌립니다. Interlocked 연산이므로 별도의 락 없이 여러 스레드가
//        동시에 호출해도 안전합니다.
void CMemoryPool::Push(MemoryHeader* ptr)
{
	ptr->allocSize = 0;

	::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

	_useCount.fetch_sub(1);
	_reserveCount.fetch_add(1);
}

// 설명 : SLIST에서 블록을 하나 꺼냅니다. 비어있으면 RawAllocator를 통해
//        새 블록을 할당합니다(라이브러리 자동 분기 + 정렬 보장).
//        꺼내온 블록이 정상적으로 반납된 상태였는지(allocSize == 0)를
//        ASSERT_CRASH로 검증하여, 이중 반납(double free) 등으로 인한
//        상태 오염을 조기에 잡아냅니다.
MemoryHeader* CMemoryPool::Pop()
{
	MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header));

	// 풀이 비어있으면 새로 raw 할당
	if (memory == nullptr)
	{
		memory = reinterpret_cast<MemoryHeader*>(
			RawAllocator::AllocAligned(static_cast<size_t>(_allocSize), SLIST_ALIGNMENT));

		// 할당 실패(OOM) 방어 체크 - nullptr 상태로 이후 로직이 진행되는 것을 차단
		ASSERT_CRASH(memory != nullptr);
	}
	else
	{
		// 풀에서 꺼낸 블록은 반드시 Push 시 allocSize가 0으로 초기화되어 있어야 함
		ASSERT_CRASH(memory->allocSize == 0);
		_reserveCount.fetch_sub(1);
	}

	_useCount.fetch_add(1);

	return memory;
}
