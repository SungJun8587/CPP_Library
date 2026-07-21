
//***************************************************************************
// Memory.cpp
//
// 설명 : Memory.h에서 선언한 CMemory 클래스의 실제 구현부.
//        풀 범위를 초과하는 대형 할당은 RawAllocator를 통해 처리하고,
//        32~4096바이트 구간(핫패스)은 스레드 로컬 캐시(TlsCache)를 우선
//        경유하여 전역 CMemoryPool의 원자 연산 호출 빈도를 줄입니다.
//***************************************************************************

#include "pch.h"
#include "Memory.h"

// gpMemory : 전역 CMemory 싱글턴 포인터 (실제 정의는 BaseGlobal.cpp)
extern CMemory* gpMemory;

// 스레드마다 독립적으로 존재하는 TLS 캐시 인스턴스 정의
thread_local CMemory::TlsCache CMemory::_tlsCache;


//***************************************************************************
// 설명 : a와 b 중 작은/큰 값을 반환하는 분기 없는(branchless) 헬퍼.
//        std::min/std::max 대신 이 이름을 쓰는 이유는, Windows.h가
//        NOMINMAX 미정의 시 min/max를 매크로로 정의해 이름이 충돌할
//        수 있기 때문입니다.
static inline int32 ClampMin(int32 a, int32 b) { return (a < b) ? a : b; }
static inline int32 ClampMax(int32 a, int32 b) { return (a > b) ? a : b; }


//***************************************************************************
// 설명 : 요청 크기(헤더 포함, allocSize)로부터 담당 풀의 _pools 인덱스를
//        분기 없이(branchless) 수식으로 계산합니다. 세 구간(32/128/256
//        단위, 경계 1024/2048)마다 클램핑으로 "이 구간에 속하는 길이"를
//        구한 뒤 각 구간 단위로 올림한 개수를 모두 더합니다. 이 수식은
//        생성자의 구간 생성 로직과 반드시 일치해야 하므로, 생성자가
//        풀을 만들 때마다 결과가 실제 _pools 인덱스와 같은지
//        ASSERT_CRASH로 교차 검증합니다.
// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기 (1 ~ MAX_ALLOC_SIZE)
// 반환값   : _pools/TlsCache::buckets에 대응하는 인덱스
int32 CMemory::ComputePoolIndex(int32 allocSize)
{
	const int32 len1 = ClampMin(allocSize, 1024);
	const int32 len2 = ClampMin(ClampMax(allocSize - 1024, 0), 1024);
	const int32 len3 = ClampMax(allocSize - 2048, 0);

	const int32 count1 = (len1 + 31) >> 5;    // 32단위 올림
	const int32 count2 = (len2 + 127) >> 7;   // 128단위 올림
	const int32 count3 = (len3 + 255) >> 8;   // 256단위 올림

	return count1 + count2 + count3 - 1;
}

//***************************************************************************
// 설명 : 블록 크기 구간에 따라 TLS 배치 충전 개수를 결정합니다. 작고
//        고빈도인 블록일수록 배치를 크게 잡아 원자 연산 호출을 줄이고,
//        크고 드물게 쓰이는 블록일수록 배치를 작게 잡아 스레드별 상주
//        메모리 낭비를 억제합니다.
// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
// 반환값   : 이 크기 구간에 적용할 배치 충전 개수
int32 CMemory::DetermineTlsBatchSize(int32 allocSize)
{
	if( allocSize <= 128 )
		return 64;   // 소형/고빈도 구간 - 원자 연산 절감을 최우선
	if( allocSize <= 1024 )
		return 32;   // 중형 구간 - 절충
	return 4;        // 대형/저빈도 구간 - 상주 메모리 절약을 우선
}

//***************************************************************************
// 설명 : 블록 크기 구간에 따라 TLS 로컬 캐시 상한을 결정합니다.
//        DetermineTlsBatchSize와 같은 구간 기준을 사용하며, 상한은
//        배치 충전 개수의 4배로 설정해 배치 충전/반납이 너무 잦게
//        반복되지 않도록 여유를 둡니다.
// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
// 반환값   : 이 크기 구간에 적용할 로컬 캐시 상한
int32 CMemory::DetermineTlsMaxCount(int32 allocSize)
{
	return DetermineTlsBatchSize(allocSize) * 4;
}

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// 설명 : 32~1024(32단위), 1024~2048(128단위), 2048~4096(256단위) 세 구간에
//        걸쳐 CMemoryPool을 생성합니다. 각 구간은 항상 그 구간의 경계
//        (1024+128, 2048+256)에서 명시적으로 시작하며, 매 풀 생성 시
//        ComputePoolIndex()와의 정합성을 ASSERT_CRASH로 검증합니다.
//        같은 구간에 대해 풀별 TLS 배치 충전 개수(_tlsBatchSizeTable)와
//        로컬 캐시 상한(_tlsMaxCountTable)도 함께 미리 계산해 둡니다.
CMemory::CMemory()
{
	int32 size = 0;

	for( size = 32; size <= 1024; size += 32 )
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		// 수식 계산 결과가 실제 생성 순서(인덱스)와 어긋나지 않는지 검증
		ASSERT_CRASH(ComputePoolIndex(size) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}

	// 두 번째 구간은 항상 1024+128에서 시작 (이전 구간이 끝난 값을 이어받지 않음)
	for( size = 1024 + 128; size <= 2048; size += 128 )
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		ASSERT_CRASH(ComputePoolIndex(size) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}

	// 세 번째 구간은 항상 2048+256에서 시작 (이전 구간이 끝난 값을 이어받지 않음)
	for( size = 2048 + 256; size <= 4096; size += 256 )
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		ASSERT_CRASH(ComputePoolIndex(size) == poolIndex);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));
	}
}

//***************************************************************************
// 설명 : 생성했던 모든 CMemoryPool을 delete합니다. 각 CMemoryPool의
//        소멸자가 내부에 남은 블록들을 raw 해제하므로 여기서는 풀
//        컨테이너 자체만 정리하면 됩니다. 호출 시점에는 모든 스레드의
//        TLS 캐시가 이미 전역 풀로 반납되어 있어야 합니다.
CMemory::~CMemory()
{
	for( CMemoryPool* pool : _pools )
		delete pool;

	_pools.clear();
}

//***************************************************************************
// 설명 : buckets 배열에 남은 모든 블록을 순회하며 원래 속했던 전역
//        CMemoryPool(_pools[i])에 되돌립니다. TlsCache 소멸자와
//        FlushCurrentThreadCache 양쪽에서 공통으로 사용하는 내부 로직.
//        gpMemory가 이미 파괴된 뒤 호출되는 경우를 대비해 nullptr이면
//        즉시 반환합니다(남은 블록은 누수되지만 프로세스 종료 시 OS가
//        회수하므로, use-after-free보다 안전).
// 매개변수 : buckets - 비워낼 TlsBucket 배열 (크기 POOL_COUNT)
void CMemory::DrainBuckets(TlsBucket* buckets)
{
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
}

//***************************************************************************
// 설명 : 스레드 종료 시 자동 호출되어(thread_local 소멸자), 이 스레드의
//        로컬 캐시에 남아있는 모든 블록을 원래 속했던 전역 CMemoryPool에
//        되돌립니다. 주로 CThreadManager로 생성된 워커 스레드의 자연
//        종료 경로에서 호출됩니다.
CMemory::TlsCache::~TlsCache()
{
	DrainBuckets(buckets);
}

//***************************************************************************
// 설명 : 현재 호출 스레드의 TLS 캐시를 즉시 비웁니다. ThreadManager가
//        join해주지 않는 스레드(메인 스레드 및 그 외 스레드)는 이 함수를
//        gpMemory delete 직전에 반드시 명시적으로 호출해야 합니다.
void CMemory::FlushCurrentThreadCache()
{
	DrainBuckets(_tlsCache.buckets);
}

//***************************************************************************
// 설명 : 지정한 크기 구간에 해당하는 풀에 raw 블록을 count개 미리
//        채워 넣습니다. 대상 풀을 찾아 raw 할당으로 블록을 만든 뒤
//        곧바로 그 풀에 반납(Push)하는 방식입니다.
void CMemory::WarmUp(int32 allocDataSize, int32 count)
{
	const int32 allocSize = allocDataSize + sizeof(MemoryHeader);

	ASSERT_CRASH(allocDataSize > 0);
	ASSERT_CRASH(allocSize <= MAX_ALLOC_SIZE); // 풀 범위를 넘는 크기는 워밍업 대상이 아님
	ASSERT_CRASH(count > 0);

	const int32 poolIndex = ComputePoolIndex(allocSize);
	CMemoryPool* pool = _pools[poolIndex];

	for( int32 i = 0; i < count; i++ )
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(
			RawAllocator::AllocAligned(static_cast<size_t>(allocSize), SLIST_ALIGNMENT));

		ASSERT_CRASH(header != nullptr);

		header->allocSize = 0; // CMemoryPool::Push가 기대하는 "미사용" 표시
		pool->Push(header);
	}
}

//***************************************************************************
// 설명 : 사용자 요청 크기(size)에 헤더 크기를 더해 실제 필요한 전체
//        크기(allocSize)를 계산한 뒤:
//          - _STOMP 빌드   : StompAllocator로 디버그 가드 할당
//          - MAX_ALLOC_SIZE 초과 : RawAllocator로 raw 정렬 할당
//          - 그 외         : 스레드 로컬 캐시(TlsBucket)에서 우선 꺼냄.
//                            로컬이 비었으면 전역 풀에서 배치 개수
//                            (_tlsBatchSizeTable)만큼 당겨와 채운 뒤 꺼냄.
//        마지막으로 헤더를 얹고 데이터 포인터를 반환합니다. size가
//        0 이하이거나 오버플로가 발생할 정도로 큰 경우 int64 산술로
//        먼저 검증한 뒤 ASSERT_CRASH로 걸러냅니다.
// 매개변수 : size - 사용자가 요청한 순수 데이터 크기(헤더 제외)
// 반환값   : 사용자가 사용할 데이터 영역 포인터
void* CMemory::Allocate(int32 size)
{
	ASSERT_CRASH(size > 0);

	// int64 산술로 오버플로 여부를 먼저 검증한 뒤 int32로 좁힌다.
	const int64 allocSize64 = static_cast<int64>(size) + static_cast<int64>(sizeof(MemoryHeader));
	ASSERT_CRASH(allocSize64 <= static_cast<int64>(INT32_MAX));
	const int32 allocSize = static_cast<int32>(allocSize64);

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
			RawAllocator::AllocAligned(static_cast<size_t>(allocSize), SLIST_ALIGNMENT));

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

	return MemoryHeader::AttachHeader(header, allocSize);
}

//***************************************************************************
// 설명 : 사용자 데이터 포인터로부터 헤더를 역산하여 allocSize를 확인하고:
//          - _STOMP 빌드   : StompAllocator로 페이지 단위 해제
//          - MAX_ALLOC_SIZE 초과 : RawAllocator로 raw 해제
//          - 그 외         : 스레드 로컬 캐시에 우선 반납. 로컬 캐시가
//                            이 크기 구간의 상한(_tlsMaxCountTable)을
//                            넘으면 절반을 전역 풀에 배치로 되돌려
//                            특정 스레드로의 메모리 편중을 방지.
//        header->allocSize.exchange(0)로 "읽고 0으로 바꾸기"를 원자적
//        으로 처리해 이중 반납을 경쟁 상태 없이 탐지합니다.
// 매개변수 : ptr - Allocate()가 반환했던 데이터 포인터
void CMemory::Release(void* ptr)
{
	MemoryHeader* header = MemoryHeader::DetachHeader(ptr);

	// "읽고 0으로 바꾸기"를 단일 원자 연산으로 - 이중 반납/경쟁 상태 탐지
	const int32 allocSize = header->allocSize.exchange(0);
	ASSERT_CRASH(allocSize > 0);

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