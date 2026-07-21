
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

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// 설명 : 이 풀이 다룰 고정 블록 크기(allocSize)를 지정하고, SLIST 헤더를
//        초기화합니다.
// 매개변수 : allocSize - 이 풀에서 다룰 고정 블록 크기(헤더 포함, 바이트)
CMemoryPool::CMemoryPool(int32 allocSize) : _allocSize(allocSize)
{
	::InitializeSListHead(&_header);
}

//***************************************************************************
// 설명 : 소멸 시 SLIST에 남아있는 모든 블록을 꺼내어 raw 메모리로 실제
//        해제합니다(프로세스 종료/풀 재구성 시 누수 방지).
CMemoryPool::~CMemoryPool()
{
	while( MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header)) )
		RawAllocator::FreeAligned(memory);
}

//***************************************************************************
// 설명 : 블록을 사용 종료 표시(allocSize = 0)한 뒤 Lock-free SLIST에
//        되돌립니다. 통계용 카운터(useCount/reserveCount)도 함께 갱신합니다.
// 매개변수 : ptr - 반납할 메모리 블록(헤더) 포인터
void CMemoryPool::Push(MemoryHeader* ptr)
{
	ptr->allocSize = 0;

	::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

	_useCount.fetch_sub(1);
	_reserveCount.fetch_add(1);
}

//***************************************************************************
// 설명 : SLIST에서 블록을 하나 꺼냅니다. 풀이 비어있으면 raw 메모리를
//        새로 할당하여 반환합니다(풀은 "최대 크기 제한 없는 동적 확장형"
//        프리리스트로 동작).
// 반환값 : 사용 가능한 메모리 블록(헤더) 포인터
MemoryHeader* CMemoryPool::Pop()
{
	MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header));

	// 풀이 비어있으면 새로 raw 할당
	if( memory == nullptr )
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