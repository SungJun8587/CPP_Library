
//***************************************************************************
// Memory.h
//
// @brief 사이즈별 CMemoryPool들을 관리하며, 요청 크기에 맞는 풀을 O(1)로
//        찾아 라우팅해주는 전역 메모리 관리자.
//
// @details
// 프로젝트 전역에서 gpMemory 싱글턴 포인터를 통해 접근됩니다(선언/생성/파괴는 BaseGlobal에서).
// 32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는 256단위로
// 총 POOL_COUNT개의 풀을 구성하며, 이 초과 크기(> MAX_ALLOC_SIZE)는 풀을 거치지 않고
// raw 할당으로 직접 처리됩니다.
// 핫패스에서는 스레드 로컬 캐시(TlsCache)를 우선 경유해 전역 풀의 원자 연산 호출 빈도를 줄입니다.
// 로컬 캐시가 비면 배치 단위로 전역 풀에서 채워오고, 상한을 넘으면 절반을 배치로 되돌립니다.
// 스레드 종료 시 로컬 캐시는 워커 스레드는 자동으로, 메인/그 외 스레드는
// FlushCurrentThreadCache()를 명시적으로 호출해 반납합니다.
//***************************************************************************

#ifndef UC_MEMORY_H
#define UC_MEMORY_H

#include <Memory/Allocator.h>
#include <Memory/MemoryPool.h>

class CMemoryPool;

//***************************************************************************
// @brief PoolAllocator가 실제로 위임하는 전역 메모리 관리자 클래스.
// @details
// 사이즈별 풀 목록(_pools)을 관리하며, 요청 크기에 맞는 풀을 ComputePoolIndex()의
// 수식 계산으로 O(1)에 찾습니다. 그 위에 스레드 로컬 캐시(TlsCache)를 얹어
// 핫패스에서의 원자 연산 빈도를 줄입니다.
//***************************************************************************
class CMemory
{
	//***************************************************************************
	// @brief CMemory 내부에서 사용되는 풀 구성 관련 상수 정의.
	//***************************************************************************
	enum
	{
		// ~1024까지 32단위, ~2048까지 128단위, ~4096까지 256단위로 구간을 나눠
		// 각 구간마다 필요한 풀 개수를 계산 (총 몇 개의 CMemoryPool이 생성되는지)
		POOL_COUNT = (1024 / 32) + (1024 / 128) + (2048 / 256),
		// 이 크기를 초과하는 요청은 풀을 사용하지 않고 raw 할당으로 직접 처리
		MAX_ALLOC_SIZE = 4096
	};

public:
	CMemory();
	~CMemory();

	void*	Allocate(size_t size);
	void	Release(void* ptr);
	void	WarmUp(size_t allocDataSize, size_t count);

	static void FlushCurrentThreadCache();

	//***************************************************************************
	// @brief 현재 살아있는(Allocate 후 아직 Release되지 않은) 블록 수를 반환합니다.
	// @details
	// CMemoryPool의 _poolCheckedOutCount/_poolReserveCount는 TLS 배치 충전
	// 시점에 갱신되어 "SLIST를 드나든 수"만 반영하므로, TLS 캐시에 대기
	// 중인(아직 사용자에게 전달되지 않은) 블록까지 포함되어 부정확합니다.
	// 이 카운터는 Allocate()/Release() 호출 시점에 직접 증감해 TLS 캐싱
	// 여부와 무관하게 실제 살아있는 할당 수를 정확히 반영합니다.
	//***************************************************************************
	static int64 GetLiveAllocationCount() { return _liveAllocationCount.load(std::memory_order_relaxed); }

private:
	static int32 ComputePoolIndex(size_t allocSize);
	static int32 DetermineTlsBatchSize(int32 allocSize);
	static int32 DetermineTlsMaxCount(int32 allocSize);

	//***************************************************************************
	// @brief 스레드별로 하나의 풀 크기에 대응하는 로컬 free-list.
	// @details
	// MemoryHeader::Next(SLIST_ENTRY 상속분)를 연결 고리로 재사용합니다.
	// TLS bucket의 포인터/카운터 정렬 및 배열 원소 정렬을 보장
	//***************************************************************************
	struct alignas(16) TlsBucket
	{
		MemoryHeader* freeList = nullptr; // 로컬 free-list 시작 노드
		int32 count = 0;                  // 현재 로컬에 쌓인 블록 수
	};

	//***************************************************************************
	// @brief 스레드마다 하나씩 존재하는(thread_local) 캐시 컨테이너.
	// @details
	// POOL_COUNT개의 TlsBucket을 배열로 가지고 있으며, 스레드가 종료될 때 소멸자가
	// 자동 호출되어 로컬에 남은 모든 블록을 전역 CMemoryPool로 반납합니다(워커 스레드 경로).
	// 메인 스레드처럼 자연 종료 시점을 신뢰할 수 없는 경우를 위해 동일한 반납 로직을
	// FlushCurrentThreadCache()로도 노출합니다.
	//***************************************************************************
	struct TlsCache
	{
		TlsBucket buckets[POOL_COUNT]; // 풀 별 로컬 버킷 배열

		//***************************************************************************
		// @brief TlsCache 소멸자.
		// @details 스레드 종료 시 자동 호출되어 남은 모든 로컬 블록을 전역 풀에 반납합니다.
		//***************************************************************************
		~TlsCache();
	};

	static void DrainBuckets(TlsBucket* buckets);

	// Allocate()/Release() 호출 시점에 직접 증감되는 실제 살아있는 할당 수.
	// WarmUp()으로 미리 채워넣은(아직 아무도 Allocate()하지 않은) 블록은
	// 포함하지 않습니다.
	static atomic<int64> _liveAllocationCount;

	std::vector<CMemoryPool*> _pools; // 생성된 모든 CMemoryPool 인스턴스 목록 (소멸자에서 일괄 delete)

	// 풀 인덱스별 TLS 배치 충전 개수 / 로컬 캐시 상한 테이블
	int16 _tlsBatchSizeTable[POOL_COUNT];
	int16 _tlsMaxCountTable[POOL_COUNT];

	static thread_local TlsCache _tlsCache; // 스레드마다 독립적으로 존재하는 캐시 인스턴스
};

#endif // ndef UC_MEMORY_H