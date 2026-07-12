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
#include "MemoryPool.h"
#include "RawAllocator.h"

// gpMemory : 전역 CMemory 싱글턴 포인터 (실제 정의는 BaseGlobal.cpp)
extern CMemory* gpMemory;

// 스레드마다 독립적으로 존재하는 TLS 캐시 인스턴스 정의
thread_local CMemory::TlsCache CMemory::_tlsCache;

/*-------------
	CMemory
---------------*/

// 설명 : 블록 크기 구간에 따라 TLS 배치 충전 개수를 결정합니다. 작고
//        고빈도인 블록(예: 128B 이하)일수록 배치를 크게 잡아 원자 연산
//        호출 빈도를 최대한 줄이고, 큰 블록(예: 1024B 초과)일수록 배치를
//        작게 잡아 스레드별 상주 메모리 낭비를 억제합니다.
// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
// 반환값   : 이 크기 구간에 적용할 배치 충전 개수
int32 CMemory::DetermineTlsBatchSize(int32 allocSize)
{
	if (allocSize <= 128)
		return 64;   // 소형/고빈도 구간 - 원자 연산 절감을 최우선
	if (allocSize <= 1024)
		return 32;   // 중형 구간 - 절충
	return 4;        // 대형/저빈도 구간 - 상주 메모리 절약을 우선
}

// 설명 : 블록 크기 구간에 따라 TLS 로컬 캐시 상한을 결정합니다.
//        DetermineTlsBatchSize와 같은 구간 기준을 사용하며, 상한은
//        배치 충전 개수의 4배 수준으로 설정해 배치 충전/반납이 너무
//        잦게 반복되지 않도록 여유를 둡니다.
// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
// 반환값   : 이 크기 구간에 적용할 로컬 캐시 상한
int32 CMemory::DetermineTlsMaxCount(int32 allocSize)
{
	return DetermineTlsBatchSize(allocSize) * 4;
}

// 설명 : 32~1024(32단위), 1024~2048(128단위), 2048~4096(256단위) 세 구간에
//        걸쳐 CMemoryPool을 생성하고, 각 풀이 커버하는 크기 범위만큼
//        _poolIndexTable(풀 인덱스)에 매핑을 채워 넣습니다. 같은 구간에
//        대해 풀별 TLS 배치 충전 개수(_tlsBatchSizeTable)와 로컬 캐시
//        상한(_tlsMaxCountTable)도 함께 미리 계산해 둡니다. 이렇게
//        생성자에서 한 번만 계산해 두면, Allocate/Release 핫패스에서는
//        조건 분기 없이 배열 조회만으로 즉시 사용할 수 있습니다.
CMemory::CMemory()
{
	int32 size = 0;
	int32 tableIndex = 0;

	for (size = 32; size <= 1024; size += 32)
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));

		while (tableIndex <= size)
		{
			_poolIndexTable[tableIndex] = static_cast<int16>(poolIndex);
			tableIndex++;
		}
	}

	for (; size <= 2048; size += 128)
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));

		while (tableIndex <= size)
		{
			_poolIndexTable[tableIndex] = static_cast<int16>(poolIndex);
			tableIndex++;
		}
	}

	for (; size <= 4096; size += 256)
	{
		CMemoryPool* pool = new CMemoryPool(size);
		const int32 poolIndex = static_cast<int32>(_pools.size());
		_pools.push_back(pool);

		_tlsBatchSizeTable[poolIndex] = static_cast<int16>(DetermineTlsBatchSize(size));
		_tlsMaxCountTable[poolIndex] = static_cast<int16>(DetermineTlsMaxCount(size));

		while (tableIndex <= size)
		{
			_poolIndexTable[tableIndex] = static_cast<int16>(poolIndex);
			tableIndex++;
		}
	}
}

// 설명 : 생성했던 모든 CMemoryPool을 delete합니다. 각 CMemoryPool의
//        소멸자가 내부에 남은 블록들을 raw 해제하므로 여기서는 풀
//        컨테이너 자체만 정리하면 됩니다.
//
//        [호출 전제조건] 이 소멸자가 호출되는 시점에는 프로세스 내
//        모든 스레드의 TLS 캐시가 이미 전역 풀로 반납되어 있어야 합니다.
//        - 워커 스레드: CThreadManager가 CMemory보다 먼저 파괴되어
//          JoinThreads()로 모든 워커 스레드의 자연 종료(및 TlsCache
//          소멸)를 이미 완료한 상태여야 함
//        - 메인 스레드 및 그 외 스레드: gpMemory를 delete하기 직전
//          CMemory::FlushCurrentThreadCache()가 명시적으로 호출된
//          상태여야 함
//        (BaseGlobal::Destroy()의 파괴 순서 참고)
CMemory::~CMemory()
{
	for (CMemoryPool* pool : _pools)
		delete pool;

	_pools.clear();
}

// 설명 : buckets 배열에 남은 모든 블록을 순회하며 원래 속했던 전역
//        CMemoryPool(_pools[i])에 되돌립니다. TlsCache 소멸자와
//        FlushCurrentThreadCache 양쪽에서 공통으로 사용하는 내부 로직.
//
//        gpMemory가 이미 파괴된 뒤 호출되는 경우(예: 정해진 파괴 순서
//        규약을 지키지 않는 외부 스레드가 프로세스 종료 시점 근처에
//        뒤늦게 종료되는 경우)를 대비해, gpMemory가 nullptr이면 즉시
//        반환합니다. 이 시점에 로컬 캐시에 남은 블록은 정상적으로
//        반납되지 못하고 그대로 버려지지만(누수), 프로세스 자체가
//        종료되는 상황이므로 OS가 결국 회수합니다. use-after-free로
//        크래시를 내는 것보다 조용히 넘어가는 편이 안전합니다.
// 매개변수 : buckets - 비워낼 TlsBucket 배열 (크기 POOL_COUNT)
void CMemory::DrainBuckets(TlsBucket* buckets)
{
	if (gpMemory == nullptr)
		return;

	for (int32 i = 0; i < POOL_COUNT; i++)
	{
		MemoryHeader* node = buckets[i].freeList;
		while (node != nullptr)
		{
			MemoryHeader* next = static_cast<MemoryHeader*>(node->Next);
			gpMemory->_pools[i]->Push(node); // 전역 Lock-free SList로 반납
			node = next;
		}

		buckets[i].freeList = nullptr;
		buckets[i].count = 0;
	}
}

// 설명 : 스레드 종료 시 자동 호출되어(thread_local 소멸자), 이 스레드의
//        로컬 캐시에 남아있는 모든 블록을 원래 속했던 전역 CMemoryPool에
//        되돌립니다. 주로 CThreadManager로 생성된 워커 스레드의 자연
//        종료 경로에서 호출됩니다.
CMemory::TlsCache::~TlsCache()
{
	DrainBuckets(buckets);
}

// 설명 : 현재 호출 스레드의 TLS 캐시를 즉시 비웁니다. thread_local
//        소멸자는 스레드가 실제로 종료돼야만 호출되므로, ThreadManager가
//        join해주지 않는 스레드(메인 스레드 및 그 외 스레드)는 이 함수를
//        gpMemory delete 직전에 반드시 명시적으로 호출해야 합니다.
void CMemory::FlushCurrentThreadCache()
{
	DrainBuckets(_tlsCache.buckets);
}

// 설명 : 지정한 크기 구간에 해당하는 풀에 raw 블록을 count개 미리
//        채워 넣습니다. 대상 풀을 찾아 raw 할당으로 블록을 만든 뒤
//        곧바로 그 풀에 반납(Push)하는 방식으로, 이후 실제 Allocate
//        호출 시에는 이미 채워진 블록을 즉시 재사용할 수 있게 됩니다.
void CMemory::WarmUp(int32 allocDataSize, int32 count)
{
	const int32 allocSize = allocDataSize + sizeof(MemoryHeader);

	ASSERT_CRASH(allocDataSize > 0);
	ASSERT_CRASH(allocSize <= MAX_ALLOC_SIZE); // 풀 범위를 넘는 크기는 워밍업 대상이 아님
	ASSERT_CRASH(count > 0);

	const int32 poolIndex = _poolIndexTable[allocSize];
	CMemoryPool* pool = _pools[poolIndex];

	for (int32 i = 0; i < count; i++)
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(
			RawAllocator::AllocAligned(static_cast<size_t>(allocSize), SLIST_ALIGNMENT));

		ASSERT_CRASH(header != nullptr);

		header->allocSize = 0; // CMemoryPool::Push가 기대하는 "미사용" 표시
		pool->Push(header);
	}
}

// 설명 : 사용자 요청 크기(size)에 헤더 크기를 더해 실제 필요한 전체
//        크기(allocSize)를 계산한 뒤:
//          - _STOMP 빌드   : StompAllocator로 디버그 가드 할당
//          - MAX_ALLOC_SIZE 초과 : RawAllocator로 raw 정렬 할당
//          - 그 외         : 스레드 로컬 캐시(TlsBucket)에서 우선 꺼냄.
//                            로컬이 비었으면 전역 풀에서 이 크기 구간에
//                            맞는 배치 개수(_tlsBatchSizeTable)만큼
//                            당겨와 로컬을 채운 뒤 하나를 꺼냄.
//        마지막으로 헤더를 얹고 사용자에게 반환할 데이터 포인터를 만들어
//        돌려줍니다.
//
//        size가 0 이하이면 즉시 ASSERT_CRASH로 걸러냅니다. 또한
//        size + sizeof(MemoryHeader) 계산이 int32 범위를 넘어설 만큼
//        비정상적으로 큰 size가 들어오면(오버플로로 인해 allocSize가
//        음수가 되어 이후 _poolIndexTable을 잘못된 인덱스로 참조할 수
//        있으므로), int64 산술로 먼저 검증한 뒤 int32로 좁힙니다.
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
	if (allocSize > MAX_ALLOC_SIZE)
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
		const int32 poolIndex = _poolIndexTable[allocSize];
		TlsBucket& bucket = _tlsCache.buckets[poolIndex];

		// 로컬 캐시가 비어있으면 전역 풀에서 이 크기 구간에 맞는 배치
		// 개수만큼 채워온다 (원자 연산 1회가 아니라 배치 개수만큼의
		// Pop이 발생하지만, 이후 그만큼의 Allocate는 로컬에서 처리되므로
		// 스레드당 원자 연산 호출 빈도는 평균적으로 크게 줄어듦)
		if (bucket.freeList == nullptr)
		{
			CMemoryPool* pool = _pools[poolIndex];
			const int32 batchSize = _tlsBatchSizeTable[poolIndex];

			for (int32 i = 0; i < batchSize; i++)
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

// 설명 : 사용자 데이터 포인터로부터 헤더를 역산하여 allocSize를 확인하고:
//          - _STOMP 빌드   : StompAllocator로 페이지 단위 해제
//          - MAX_ALLOC_SIZE 초과 : RawAllocator로 raw 해제
//          - 그 외         : 스레드 로컬 캐시에 우선 반납. 로컬 캐시가
//                            이 크기 구간의 상한(_tlsMaxCountTable)을
//                            넘으면 절반을 전역 풀에 배치로 되돌려
//                            특정 스레드로의 메모리 편중을 방지.
// 매개변수 : ptr - Allocate()가 반환했던 데이터 포인터
void CMemory::Release(void* ptr)
{
	MemoryHeader* header = MemoryHeader::DetachHeader(ptr);

	const int32 allocSize = header->allocSize;
	ASSERT_CRASH(allocSize > 0);

#ifdef _STOMP
	StompAllocator::Release(header);
#else
	if (allocSize > MAX_ALLOC_SIZE)
	{
		// 메모리 풀의 최대 크기를 초과한 raw 할당 블록 해제
		RawAllocator::FreeAligned(header);
	}
	else
	{
		const int32 poolIndex = _poolIndexTable[allocSize];
		TlsBucket& bucket = _tlsCache.buckets[poolIndex];

		header->Next = static_cast<PSLIST_ENTRY>(bucket.freeList);
		bucket.freeList = header;
		bucket.count++;

		// 로컬 캐시가 이 크기 구간의 상한을 넘으면 절반을 전역 풀에
		// 배치로 반납 (한 스레드가 계속 Release만 반복해 메모리를
		// 독점하는 상황 방지)
		const int32 maxCount = _tlsMaxCountTable[poolIndex];
		if (bucket.count > maxCount)
		{
			CMemoryPool* pool = _pools[poolIndex];
			const int32 releaseCount = maxCount / 2;

			for (int32 i = 0; i < releaseCount; i++)
			{
				MemoryHeader* node = bucket.freeList;
				bucket.freeList = static_cast<MemoryHeader*>(node->Next);
				pool->Push(node);
				bucket.count--;
			}
		}
	}
#endif	
}
