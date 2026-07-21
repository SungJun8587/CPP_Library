
//***************************************************************************
// Memory.h
//
// 설명 : 사이즈별 CMemoryPool들을 관리하며, 요청 크기에 맞는 풀을 O(1)로
//        찾아 라우팅해주는 전역 메모리 관리자. 프로젝트 전역에서 gpMemory
//        싱글턴 포인터를 통해 접근됩니다(선언/생성/파괴는 BaseGlobal에서).
//
//        32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는
//        256단위로 총 POOL_COUNT개의 풀을 구성하며, 이 초과 크기
//        (> MAX_ALLOC_SIZE)는 풀을 거치지 않고 raw 할당으로 직접 처리됩니다.
//
//        핫패스에서는 스레드 로컬 캐시(TlsCache)를 우선 경유해 전역 풀의
//        원자 연산 호출 빈도를 줄입니다. 로컬 캐시가 비면 배치 단위로
//        전역 풀에서 채워오고, 상한을 넘으면 절반을 배치로 되돌립니다.
//        스레드 종료 시 로컬 캐시는 워커 스레드는 자동으로, 메인/그 외
//        스레드는 FlushCurrentThreadCache()를 명시적으로 호출해 반납합니다.
//***************************************************************************

#ifndef __MEMORY_H__
#define __MEMORY_H__

#pragma once

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

#ifndef	__MEMORYPOOL_H__
#include <Memory/MemoryPool.h>
#endif

class CMemoryPool;

/***************************************************************************
	CMemory

	설명 : PoolAllocator가 실제로 위임하는 대상. 사이즈별 풀 목록(_pools)을
		   관리하며, 요청 크기에 맞는 풀을 ComputePoolIndex()의 수식
		   계산으로 O(1)에 찾습니다. 그 위에 스레드 로컬 캐시(TlsCache)를
		   얹어 핫패스에서의 원자 연산 빈도를 줄입니다.
***************************************************************************/
class CMemory
{
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

	void*	Allocate(int32 size);
	void	Release(void* ptr);
	void	WarmUp(int32 allocDataSize, int32 count);

	static void FlushCurrentThreadCache();

private:
	static int32 ComputePoolIndex(int32 allocSize);
	static int32 DetermineTlsBatchSize(int32 allocSize);
	static int32 DetermineTlsMaxCount(int32 allocSize);

	/***************************************************************************
		TlsBucket

		설명 : 스레드별로 하나의 풀 크기에 대응하는 로컬 free-list.
			   MemoryHeader::Next(SLIST_ENTRY 상속분)를 연결 고리로
			   재사용합니다. alignas(16)은 buckets 배열을 순회할 때
			   원소가 캐시 라인 경계를 걸치지 않도록 정렬을 보장합니다.
	***************************************************************************/
	struct alignas(16) TlsBucket
	{
		MemoryHeader* freeList = nullptr; // 로컬 free-list 시작 노드
		int32 count = 0;                  // 현재 로컬에 쌓인 블록 수
	};

	/***************************************************************************
		TlsCache

		설명 : 스레드마다 하나씩 존재하는(thread_local) 캐시 컨테이너.
			   POOL_COUNT개의 TlsBucket을 배열로 가지고 있으며, 스레드가
			   종료될 때 소멸자가 자동 호출되어 로컬에 남은 모든 블록을
			   전역 CMemoryPool로 반납합니다(워커 스레드 경로). 메인
			   스레드처럼 자연 종료 시점을 신뢰할 수 없는 경우를 위해
			   동일한 반납 로직을 FlushCurrentThreadCache()로도 노출합니다.
	***************************************************************************/
	struct TlsCache
	{
		TlsBucket buckets[POOL_COUNT];

		// 설명 : 스레드 종료 시 자동 호출되어, 남아있는 모든 로컬 블록을
		//        해당 크기의 전역 CMemoryPool에 되돌립니다.
		~TlsCache();
	};

	static void DrainBuckets(TlsBucket* buckets);

	// 생성된 모든 CMemoryPool 인스턴스 목록 (소멸자에서 일괄 delete)
	vector<CMemoryPool*> _pools;

	// 풀 인덱스별 TLS 배치 충전 개수 / 로컬 캐시 상한.
	// DetermineTlsBatchSize/DetermineTlsMaxCount로 생성자에서 미리 계산해
	// 두어, Allocate/Release 핫패스에서는 조건 분기 없이 배열 조회만으로
	// 즉시 사용합니다. (POOL_COUNT는 수십 개 수준이라 이 정도 크기의
	// 테이블은 거의 항상 L1 캐시에 상주함)
	int16 _tlsBatchSizeTable[POOL_COUNT];
	int16 _tlsMaxCountTable[POOL_COUNT];

	// 스레드마다 독립적으로 존재하는 캐시 인스턴스
	static thread_local TlsCache _tlsCache;
};

#endif // ndef __MEMORY_H__