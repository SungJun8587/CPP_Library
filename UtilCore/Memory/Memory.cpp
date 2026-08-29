
//***************************************************************************
// Memory.cpp
//
// @brief Memory.h에서 선언한 CMemory 클래스의 실제 구현부.
// @details
// 풀 범위를 초과하는 대형 할당은 RawAllocator를 통해 처리하고,
// 32~4096바이트 구간(핫패스)은 스레드 로컬 캐시(TlsCache)를 우선
// 경유하여 전역 CMemoryPool의 원자 연산 호출 빈도를 줄입니다.
//***************************************************************************

#include "pch.h"
#include "Memory.h"

#include <limits>

// gpMemory : 전역 CMemory 싱글턴 포인터 (실제 정의는 BaseGlobal.cpp)

#if defined(USE_GPMEMORY)
extern CMemory* gpMemory;
#endif

// 스레드마다 독립적으로 존재하는 TLS 캐시 인스턴스 정의
thread_local CMemory::TlsCache CMemory::_tlsCache;

// 실제 살아있는 할당 수 - Allocate()/Release()에서 직접 증감 (TLS 캐싱과 무관하게 정확)
atomic<int64> CMemory::_liveAllocationCount = 0;

//***************************************************************************
// @brief 요청 크기(헤더 포함)로부터 담당 풀의 _pools 인덱스를 수식으로 계산합니다.
// @details
// allocSize는 호출부가 이미 MAX_ALLOC_SIZE 이하임을 보장한 채로 넘긴다는 전제이며,
// 이 함수 스스로도 그 전제를 ASSERT_CRASH로 검증합니다.
// 세 구간(32/128/256 단위)마다 클램핑으로 각 구간에 속하는 길이를 구한 뒤
// 각 구간 단위로 올림한 개수를 더해 _pools 인덱스를 산출합니다.
// @param allocSize 헤더를 포함한 전체 블록 크기 (1 ~ MAX_ALLOC_SIZE)
// @return _pools 및 TlsCache::buckets에 대응하는 인덱스
//***************************************************************************
int32 CMemory::ComputePoolIndex(size_t allocSize)
{
	ASSERT_CRASH(allocSize > 0 && allocSize <= MAX_ALLOC_SIZE);
	const int32 size = static_cast<int32>(allocSize);

	const int32 len1 = (std::min)(size, 1024);
	const int32 len2 = (std::min)((std::max)(size - 1024, 0), 1024);
	const int32 len3 = (std::max)(size - 2048, 0);

	const int32 count1 = (len1 + 31) >> 5;    // 32단위 올림
	const int32 count2 = (len2 + 127) >> 7;   // 128단위 올림
	const int32 count3 = (len3 + 255) >> 8;   // 256단위 올림

	return count1 + count2 + count3 - 1;
}

//***************************************************************************
// @brief 블록 크기 구간에 따라 TLS 배치 충전 개수를 결정합니다.
// @details 작고 고빈도인 블록일수록 배치를 크게 잡고, 크고 드물게 쓰이는 블록일수록 배치를 작게 잡습니다.
// @param allocSize 헤더를 포함한 전체 블록 크기
// @return 이 크기 구간에 적용할 배치 충전 개수
//***************************************************************************
int32 CMemory::DetermineTlsBatchSize(int32 allocSize)
{
	if( allocSize <= 128 )
		return 64;   // 소형/고빈도 구간 - 원자 연산 절감을 최우선
	if( allocSize <= 1024 )
		return 32;   // 중형 구간 - 절충
	return 4;        // 대형/저빈도 구간 - 상주 메모리 절약을 우선
}

//***************************************************************************
// @brief 블록 크기 구간에 따라 TLS 로컬 캐시 상한을 결정합니다.
// @details DetermineTlsBatchSize와 같은 구간 기준을 사용하며 상한은 배치 충전 개수의 4배로 설정합니다.
// @param allocSize 헤더를 포함한 전체 블록 크기
// @return 이 크기 구간에 적용할 로컬 캐시 상한
//***************************************************************************
int32 CMemory::DetermineTlsMaxCount(int32 allocSize)
{
	return DetermineTlsBatchSize(allocSize) * 4;
}

//***************************************************************************
// @brief CMemory 생성자.
// @details
// 32~1024(32단위), 1024~2048(128단위), 2048~4096(256단위) 구간에 걸쳐 CMemoryPool을 생성하며,
// 풀별 배치 충전 개수 및 로컬 캐시 상한 테이블을 초기화합니다.
//***************************************************************************
CMemory::CMemory()
{
	int32 size = 0;

	for( size = 32; size <= 1024; size += 32 )
	{
		CMemoryPool* pool = new CMemoryPool(static_cast<size_t>(size));
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		// 수식 계산 결과가 실제 생성 순서(인덱스)와 어긋나지 않는지 검증
		ASSERT_CRASH(ComputePoolIndex(static_cast<size_t>(size)) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}

	// 두 번째 구간은 항상 1024+128에서 시작 (이전 구간이 끝난 값을 이어받지 않음)
	for( size = 1024 + 128; size <= 2048; size += 128 )
	{
		CMemoryPool* pool = new CMemoryPool(static_cast<size_t>(size));
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		ASSERT_CRASH(ComputePoolIndex(static_cast<size_t>(size)) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}

	// 세 번째 구간은 항상 2048+256에서 시작 (이전 구간이 끝난 값을 이어받지 않음)
	for( size = 2048 + 256; size <= 4096; size += 256 )
	{
		CMemoryPool* pool = new CMemoryPool(static_cast<size_t>(size));
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		ASSERT_CRASH(ComputePoolIndex(static_cast<size_t>(size)) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}
}

//***************************************************************************
// @brief CMemory 소멸자.
// @details 생성했던 모든 CMemoryPool 인스턴스를 해제하고 컨테이너를 정리합니다.
//***************************************************************************
CMemory::~CMemory()
{
	for( CMemoryPool* pool : _pools )
		delete pool;

	_pools.clear();
}

//***************************************************************************
// @brief buckets 배열에 남은 모든 블록을 원래 속했던 전역 CMemoryPool에 되돌립니다.
// @param buckets 비워낼 TlsBucket 배열
//***************************************************************************
void CMemory::DrainBuckets(TlsBucket* buckets)
{
#if defined(USE_GPMEMORY)
	if( gpMemory == nullptr )
		return;

	for( int32 i = 0; i < POOL_COUNT; i++ )
	{
		MemoryHeader* node = buckets[i].freeList;
		while( node != nullptr )
		{
			MemoryHeader* next = static_cast<MemoryHeader*>(node->Next);
			gpMemory->_pools[i]->Push(node); // 전역 Lock-free SList로 반납
			node = next;
		}

		buckets[i].freeList = nullptr;
		buckets[i].count = 0;
	}
#endif
}

//***************************************************************************
// @brief TlsCache 소멸자.
// @details 스레드 종료 시 자동 호출되어 남은 모든 로컬 블록을 전역 CMemoryPool로 되돌립니다.
//***************************************************************************
CMemory::TlsCache::~TlsCache()
{
	DrainBuckets(buckets);
}

//***************************************************************************
// @brief 현재 호출 스레드의 TLS 캐시를 명시적으로 즉시 비웁니다.
//***************************************************************************
void CMemory::FlushCurrentThreadCache()
{
	DrainBuckets(_tlsCache.buckets);
}

//***************************************************************************
// @brief 지정한 크기 구간의 풀에 블록을 미리 생성하여 채워 넣습니다.
// @param allocDataSize 순수 데이터 크기
// @param count 미리 할당하여 채워넣을 블록 개수
//***************************************************************************
void CMemory::WarmUp(size_t allocDataSize, size_t count)
{
	ASSERT_CRASH(allocDataSize > 0);
	ASSERT_CRASH(count > 0);

	ASSERT_CRASH(allocDataSize <= (std::numeric_limits<size_t>::max)() - sizeof(MemoryHeader));

	const size_t allocSize = allocDataSize + sizeof(MemoryHeader);

	ASSERT_CRASH(allocSize <= MAX_ALLOC_SIZE);

	const int32 poolIndex = ComputePoolIndex(allocSize);

	CMemoryPool* pool = _pools[poolIndex];

	const size_t poolBlockSize = pool->GetBlockSize();

	for( size_t i = 0; i < count; ++i )
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(RawAllocator::AllocAligned(poolBlockSize, SLIST_ALIGNMENT));

		ASSERT_CRASH(header != nullptr);

		pool->WarmUpPush(header);
	}
}

//***************************************************************************
// @brief 요청 크기에 알맞은 메모리 블록을 할당받아 데이터 영역 포인터를 반환합니다.
// @param size 사용자가 요청한 순수 데이터 크기(헤더 제외)
// @return 할당된 사용자 데이터 영역 포인터
//***************************************************************************
void* CMemory::Allocate(size_t size)
{
	ASSERT_CRASH(size > 0);
	// size + sizeof(MemoryHeader) 덧셈 자체가 오버플로하지 않는지 검증
	ASSERT_CRASH(size <= (std::numeric_limits<size_t>::max)() - sizeof(MemoryHeader));

	const size_t allocSize = size + sizeof(MemoryHeader);

	MemoryHeader* header = nullptr;

#ifdef _STOMP
	// 디버그 전용: 페이지 가드 방식으로 오버런을 즉시 크래시로 탐지
	header = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(allocSize));
#else
	if( allocSize > MAX_ALLOC_SIZE )
	{
		// 메모리 풀의 최대 크기를 초과하면 raw 할당
		// (mimalloc/jemalloc/tcmalloc/malloc 중 컴파일 타임에 선택된 라이브러리 사용)
		header = reinterpret_cast<MemoryHeader*>(
			RawAllocator::AllocAligned(allocSize, SLIST_ALIGNMENT));

		// 대형 할당 실패(OOM) 방어 체크 - nullptr로 AttachHeader가 진행되는 것을 차단
		ASSERT_CRASH(header != nullptr);
	}
	else
	{
		const int32 poolIndex = ComputePoolIndex(allocSize);
		TlsBucket& bucket = _tlsCache.buckets[poolIndex];

		// 로컬 캐시가 비어있으면 전역 풀에서 이 크기 구간에 맞는 배치
		// 개수만큼 채워온다 (원자 연산 1회가 아니라 배치 개수만큼의
		// Pop이 발생하지만, 이후 그만큼의 Allocate는 로컬에서 처리되므로
		// 스레드당 원자 연산 호출 빈도는 평균적으로 크게 줄어듦)
		if( bucket.freeList == nullptr )
		{
			CMemoryPool* pool = _pools[poolIndex];
			const int32 batchSize = _tlsBatchSizeTable[poolIndex];

			for( int32 i = 0; i < batchSize; i++ )
			{
				MemoryHeader* node = pool->Pop();
				node->Next = static_cast<PSLIST_ENTRY>(bucket.freeList);
				bucket.freeList = node;
				bucket.count++;
			}
		}

		header = bucket.freeList;
		bucket.freeList = static_cast<MemoryHeader*>(header->Next);
		bucket.count--;
	}
#endif	

	_liveAllocationCount.fetch_add(1, std::memory_order_relaxed);

	return MemoryHeader::AttachHeader(header, allocSize);
}

//***************************************************************************
// @brief Allocate()로 할당했던 메모리 블록을 해제하여 풀 또는 시스템에 반납합니다.
// @param ptr 반납할 사용자 데이터 포인터
//***************************************************************************
void CMemory::Release(void* ptr)
{
	if( ptr == nullptr )
		return;

	MemoryHeader* header = MemoryHeader::DetachHeader(ptr);

	// "읽고 0으로 바꾸기"를 단일 원자 연산으로 - 이중 반납/경쟁 상태 탐지
	const size_t allocSize = header->allocSize.exchange(0, std::memory_order_relaxed);
	ASSERT_CRASH(allocSize > 0);

	_liveAllocationCount.fetch_sub(1, std::memory_order_relaxed);

#ifdef _STOMP
	StompAllocator::Release(header);
#else
	if( allocSize > MAX_ALLOC_SIZE )
	{
		// 메모리 풀의 최대 크기를 초과한 raw 할당 블록 해제
		RawAllocator::FreeAligned(header);
	}
	else
	{
		const int32 poolIndex = ComputePoolIndex(allocSize);
		TlsBucket& bucket = _tlsCache.buckets[poolIndex];

		// header->allocSize는 위 exchange(0)에서 이미 "미사용" 표시로
		// 바뀌었으므로 여기서 다시 대입할 필요가 없음
		header->Next = static_cast<PSLIST_ENTRY>(bucket.freeList);
		bucket.freeList = header;
		bucket.count++;

		// 로컬 캐시가 이 크기 구간의 상한을 넘으면 절반을 전역 풀에
		// 배치로 반납 (한 스레드가 계속 Release만 반복해 메모리를
		// 독점하는 상황 방지)
		const int32 maxCount = _tlsMaxCountTable[poolIndex];
		if( bucket.count > maxCount )
		{
			CMemoryPool* pool = _pools[poolIndex];
			const int32 releaseCount = maxCount / 2;

			for( int32 i = 0; i < releaseCount; i++ )
			{
				MemoryHeader* node = bucket.freeList;
				bucket.freeList = static_cast<MemoryHeader*>(node->Next);
				pool->Push(node); // CMemoryPool::Push가 allocSize=0을 다시 한번 보장
				bucket.count--;
			}
		}
	}
#endif	
}