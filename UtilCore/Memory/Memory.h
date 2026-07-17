
//***************************************************************************
// Memory.h
//
// 설명 : 사이즈별 CMemoryPool들을 관리하며, 요청 크기에 맞는 풀을 O(1)로
//        찾아 라우팅해주는 전역 메모리 관리자. 프로젝트 전역에서 gpMemory
//        싱글턴 포인터를 통해 접근됩니다(선언/생성/파괴는 BaseGlobal에서).
//
//        32~1024바이트는 32단위, 1024~2048바이트는 128단위, 2048~4096바이트는
//        256단위로 총 POOL_COUNT개의 풀을 구성합니다. 이 초과 크기(> MAX_ALLOC_SIZE)
//        요청은 풀을 거치지 않고 raw 할당으로 직접 처리됩니다.
//
//        [스레드 로컬(TLS) 캐시]
//        CMemoryPool::Push/Pop은 매 호출마다 Interlocked SList 연산(원자적
//        CAS)을 수행하는데, 멀티코어 환경에서 여러 스레드가 동시에 같은
//        풀을 두드리면 원자 연산 자체의 캐시라인 경합이 발생합니다.
//        이를 줄이기 위해 CMemory는 스레드별로 작은 free-list(TlsBucket)를
//        따로 두고, 대부분의 Allocate/Release는 이 로컬 캐시에서 처리합니다.
//        로컬 캐시가 비었을 때만 전역 풀에서 배치 단위로 당겨오고(배치
//        충전), 로컬 캐시가 너무 커지면 절반을 전역 풀에 되돌려(배치
//        반납) 메모리가 특정 스레드에 편중되는 것을 막습니다.
//
//        배치 크기(충전/상한)는 풀마다 균일하지 않고 블록 크기 구간별로
//        차등 적용됩니다. 작고 고빈도인 블록(예: 32~128B)은 배치를 크게
//        잡아 원자 연산 호출을 최대한 줄이고, 크고 드물게 쓰이는 블록
//        (예: 2~4KB)은 배치를 작게 잡아 스레드별 상주 메모리 낭비를
//        억제합니다. (자세한 임계값은 CMemory.cpp의 DetermineTlsBatchSize
//        참고)
//
//        [풀 인덱스 계산]
//        "요청 크기 -> 담당 풀 인덱스"는 테이블 조회가 아니라
//        ComputePoolIndex()의 수식 계산(시프트 연산 + 올림 처리)으로
//        구합니다. 이전에는 MAX_ALLOC_SIZE+1개 항목의 배열을 두고
//        조회했었는데, 이 배열은 다른 코드가 L1/L2 캐시를 심하게
//        오염시킨 상태에서 참조하면 캐시 미스가 날 수 있는 크기였습니다.
//        수식 계산은 테이블 자체가 없으므로 이런 캐시 미스 가능성이
//        구조적으로 없습니다.
//
//        [이중 반납(Double Free) 방지]
//        CMemoryPool::Push가 전역 풀에 반납할 때 allocSize를 0으로
//        표시해 두는 것과 동일하게, CMemory::Release도 TLS 캐시에
//        반납하기 직전 allocSize를 0으로 표시합니다. 같은 포인터에
//        Release가 두 번 호출되면 두 번째 호출에서 Release 맨 앞의
//        ASSERT_CRASH(allocSize > 0)가 즉시 걸립니다. 이 표시가 없으면
//        이중 반납이 TLS 로컬 free-list를 자기 자신을 가리키는 순환
//        구조로 만들어, 이후 서로 다른 두 번의 Allocate가 같은 메모리
//        블록을 동시에 소유하는 조용한 메모리 오염으로 이어질 수 있어
//        반드시 필요한 처리입니다.
//
//        MemoryHeader가 SLIST_ENTRY(= Next 포인터 하나)를 상속하고 있어,
//        이 Next 필드를 그대로 재사용해 TLS 로컬 free-list를 연결합니다.
//        (MemoryHeader/CMemoryPool 구조는 전혀 변경하지 않음)
//
//        [스레드 종료 시 캐시 반납 - 두 가지 경로]
//        1) CThreadManager로 생성된 워커 스레드
//           -> 스레드가 자연 종료되면 thread_local TlsCache의 소멸자가
//              자동 호출되어 로컬 캐시를 전역 풀로 반납합니다.
//              (CThreadManager::JoinThreads()가 이 종료를 대기하므로,
//               ThreadManager가 CMemory보다 먼저 파괴되는 한 안전합니다.
//               ThreadManager::DestroyTLS()에서도 명시적으로 flush를
//               호출해 이중으로 보장합니다.)
//        2) 메인 스레드
//           -> ThreadManager가 관리하지 않으므로 아무도 join해주지
//              않습니다. main() 종료 시점에야 thread_local 소멸자가
//              불리는데, 그 전에 gpMemory가 이미 delete되어 있으면
//              use-after-free가 발생합니다. 따라서 BaseGlobal::Destroy()
//              에서 gpMemory를 delete하기 직전, 반드시
//              CMemory::FlushCurrentThreadCache()를 명시적으로 호출해
//              메인 스레드의 로컬 캐시를 먼저 비워야 합니다.
//        3) CThreadManager를 거치지 않는 그 외의 스레드
//           -> 메인 스레드와 동일한 문제(아무도 join하지 않음)를 그대로
//              가집니다. 서드파티 라이브러리 콜백 스레드 등 CThreadManager
//              밖에서 만들어진 스레드가 PoolAllocator(xnew/StlAllocator
//              등)를 사용한다면, 그 스레드도 종료 전에 반드시
//              CMemory::FlushCurrentThreadCache()를 직접 호출해야 하며,
//              그 시점까지 gpMemory가 살아있어야 합니다. 이는 일반적인
//              규칙(모든 스레드는 gpMemory보다 먼저, 그리고 종료 시
//              스스로 캐시를 비워야 함)의 연장선이며 코드로 자동
//              강제되지 않으므로 스레드를 새로 추가할 때 반드시 지켜야
//              하는 규약입니다. 이 규약을 지킬 수 없는 외부 스레드라면
//              DrainBuckets()가 gpMemory nullptr 여부를 확인하여 최소한
//              크래시 대신 누수로 저하되도록 방어되어 있습니다.
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

/*-------------
	CMemory

	설명 : PoolAllocator가 실제로 위임하는 대상. 사이즈별 풀 목록(_pools)을
		   관리하며, "요청 크기 -> 담당 풀 인덱스"는 테이블 조회가 아니라
		   ComputePoolIndex()의 수식 계산으로 O(1)에 구합니다. 그 위에
		   스레드 로컬 캐시(TlsCache)를 얹어 핫패스에서의 원자 연산
		   빈도를 줄입니다.

		   BaseAllocator는 상속받지 않습니다. CMemory는 프로그램 시작 시
		   BaseGlobal::Init()에서 단 한 번만 생성되는 싱글턴이므로, 이
		   한 번의 할당을 raw 경로로 격리하는 것보다 전역 new/delete를
		   그대로 쓰는 단순함을 우선했습니다. BaseAllocator는 메모리
		   모듈과 무관한 다른 기능 클래스들이 필요할 때 상속해 쓰는
		   범용 유틸리티로 남겨둡니다.
---------------*/

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
	// 설명 : 32~4096 구간을 세 단계(32/128/256 단위)로 나누어 각 크기에 맞는
	//        CMemoryPool을 생성합니다. 풀별 TLS 배치 충전/상한 값도 블록
	//        크기 구간에 따라 함께 결정해 둡니다.
	//        (반드시 멀티스레드 환경 진입 전, 단일 스레드 초기화 단계에서
	//        호출되어야 합니다 - BaseGlobal::Init()에서 가장 먼저 생성)
	CMemory();

	// 설명 : 관리 중인 모든 CMemoryPool을 delete하여 정리합니다.
	//        호출 시점에는 모든 스레드(워커 + 메인 + 그 외 모든 스레드)의
	//        TLS 캐시가 이미 전역 풀로 반납되어 있어야 합니다. 호출 전
	//        반드시
	//        - 모든 워커 스레드가 join 완료된 상태여야 하고(ThreadManager가
	//          CMemory보다 먼저 파괴되어야 함)
	//        - 메인 스레드 및 그 외 CThreadManager 밖에서 생성된 스레드는
	//          FlushCurrentThreadCache()를 직접 호출해야 합니다.
	~CMemory();

	// 설명 : 요청 크기에 헤더 크기를 더한 뒤, MAX_ALLOC_SIZE 이하면 스레드
	//        로컬 캐시(TlsBucket)에서 우선 꺼내고, 로컬이 비었으면 전역
	//        풀에서 배치로 채운 뒤 반환합니다. MAX_ALLOC_SIZE 초과 시
	//        raw 할당으로 직접 처리합니다.
	//        (_STOMP 매크로 정의 시에는 StompAllocator 경로로 대체됩니다.)
	//
	//        size가 0 이하이거나, 헤더를 더한 계산값이 int32 범위를
	//        넘어설 정도로 비정상적으로 큰 경우 ASSERT_CRASH로 즉시
	//        걸러냅니다(오버플로로 인한 음수 allocSize가 풀 인덱스 계산을
	//        잘못된 값으로 만드는 것을 막기 위함).
	// 매개변수 : size - 사용자가 요청한 순수 데이터 크기(헤더 제외)
	// 반환값   : 사용자가 사용할 데이터 영역 포인터
	void* Allocate(int32 size);

	// 설명 : 데이터 포인터로부터 헤더를 역산해 allocSize를 확인한 뒤,
	//        MAX_ALLOC_SIZE 이하면 스레드 로컬 캐시에 반납합니다. 로컬
	//        캐시가 해당 풀의 상한을 넘으면 절반을 전역 풀에 배치로
	//        되돌립니다. MAX_ALLOC_SIZE 초과 시 raw 메모리로 직접
	//        해제합니다.
	//
	//        TLS 캐시로 반납하기 직전 header->allocSize를 0으로 표시해
	//        둡니다. CMemoryPool::Push가 전역 풀에 반납할 때 하는 것과
	//        동일한 컨벤션으로, 같은 포인터에 대해 Release가 두 번
	//        호출되면(이중 반납) 두 번째 호출에서 이 함수 맨 앞의
	//        ASSERT_CRASH(allocSize > 0)가 즉시 걸립니다. 이 표시가
	//        없으면 이중 반납이 TLS 로컬 free-list를 자기 자신을
	//        가리키는 순환 구조로 만들어, 이후 서로 다른 두 번의
	//        Allocate가 같은 메모리 블록을 동시에 소유하게 되는 조용한
	//        메모리 오염으로 이어질 수 있습니다.
	// 매개변수 : ptr - Allocate()가 반환했던 데이터 포인터
	void	Release(void* ptr);

	// 설명 : 현재 호출한 스레드의 로컬 TLS 캐시에 남아있는 모든 블록을
	//        전역 CMemoryPool로 즉시 반납합니다.
	//
	//        thread_local 객체의 소멸자는 "그 스레드가 실제로 종료될 때"
	//        만 호출됩니다. CThreadManager로 생성된 워커 스레드는 자연
	//        종료 시 이 소멸자가 자동으로 불리지만, 메인 스레드나 그 외
	//        CThreadManager 밖에서 만들어진 스레드는 아무도 join하지
	//        않으므로 언제 종료될지(그리고 그 시점에 gpMemory가 아직
	//        살아있을지) 보장할 수 없습니다.
	//
	//        따라서 gpMemory를 delete하기 직전, 반드시 그런 스레드에서
	//        이 함수를 명시적으로 호출해 로컬 캐시를 먼저 비워야 합니다.
	//        (BaseGlobal::Destroy() 참고)
	static void FlushCurrentThreadCache();

	// 설명 : 지정한 크기 구간에 해당하는 풀에 raw 블록을 count개
	//        미리 채워 넣습니다(warm-up). 서버 시작 직후 동시 접속이
	//        몰리는 시점에 자주 쓰이는 크기의 블록을 그때그때 새로
	//        raw 할당하며 생기는 지연 스파이크를 줄이기 위한 선택적
	//        호출입니다. 모든 풀을 일괄적으로 채우면 시작 시점에 불필요한
	//        메모리를 크게 낭비할 수 있으므로, 실제로 자주 쓰이는 크기만
	//        골라 호출하는 것을 권장합니다.
	// 매개변수 : allocDataSize - 워밍업할 데이터 크기(헤더 제외, Allocate에
	//                           전달하는 것과 동일한 의미의 크기)
	//           count         - 미리 채워 넣을 블록 개수
	void WarmUp(int32 allocDataSize, int32 count);

private:
	// 설명 : 요청 크기(헤더 포함, allocSize)로부터 담당 풀의 _pools
	//        인덱스를 테이블 조회 없이 수식으로 계산합니다. 32/128/256
	//        단위 구간의 경계값(1024, 2048)에 대해 시프트 연산(나눗셈이
	//        2의 거듭제곱이므로 >>5/>>7/>>8로 대체)과 올림 처리만으로
	//        구해지므로, 캐시 미스가 발생할 수 있는 큰 테이블(이전에는
	//        MAX_ALLOC_SIZE+1개 항목의 배열) 조회를 완전히 없앱니다.
	//
	//        이 수식은 생성자의 세 단계 구간 생성 로직(32/128/256 단위,
	//        1024/2048/4096 경계)과 반드시 일치해야 하므로, 두 로직이
	//        어긋나지 않도록 생성자에서 매 풀 생성 시 이 함수의 결과와
	//        실제 _pools 인덱스가 같은지 ASSERT_CRASH로 교차 검증합니다.
	// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기 (1 ~ MAX_ALLOC_SIZE)
	// 반환값   : _pools/TlsCache::buckets에 대응하는 인덱스
	static int32 ComputePoolIndex(int32 allocSize);

	// 설명 : 블록 크기 구간에 따라 TLS 배치 충전 개수를 결정합니다.
	//        작고 고빈도인 블록일수록 배치를 크게 잡아 원자 연산 호출
	//        빈도를 더 많이 줄이고, 크고 드물게 쓰이는 블록일수록 배치를
	//        작게 잡아 스레드별 상주 메모리 낭비를 억제합니다.
	// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
	// 반환값   : 이 크기 구간에 적용할 배치 충전 개수
	static int32 DetermineTlsBatchSize(int32 allocSize);

	// 설명 : 블록 크기 구간에 따라 TLS 로컬 캐시 상한을 결정합니다.
	//        DetermineTlsBatchSize와 같은 구간 기준을 사용하며, 상한은
	//        보통 배치 충전 개수의 몇 배 수준으로 설정합니다.
	// 매개변수 : allocSize - 헤더를 포함한 전체 블록 크기
	// 반환값   : 이 크기 구간에 적용할 로컬 캐시 상한
	static int32 DetermineTlsMaxCount(int32 allocSize);

	/*-------------
		TlsBucket

		설명 : 스레드별로 하나의 풀 크기에 대응하는 로컬 free-list.
			   MemoryHeader::Next(SLIST_ENTRY 상속분)를 연결 고리로
			   재사용하여 단일 연결 리스트를 구성합니다.

			   alignas(16)을 붙인 이유: sizeof(TlsBucket)은 16바이트로,
			   캐시 라인(64바이트)의 정확한 약수입니다. 하지만 포인터
			   멤버 때문에 이 구조체의 자연 정렬은 8바이트뿐이라, buckets
			   배열의 시작 주소가 8바이트로만 정렬되고 16바이트로는
			   정렬되지 않으면 4번째 원소마다 캐시 라인 경계를 걸치게
			   됩니다(예: 시작 오프셋이 8일 때 4번째 원소는 [56,72)
			   구간을 차지해 64 지점에서 캐시 라인이 갈립니다). 정렬
			   요구사항을 16으로 올리면 크기가 이미 16의 배수이므로
			   패딩을 추가하지 않고도(크기 낭비 없이) 모든 원소가 항상
			   하나의 캐시 라인 안에 완전히 들어가도록 보장됩니다. 이는
			   스레드 간 false sharing 방지가 아니라(thread_local이라
			   원천적으로 스레드 간 공유가 없음), 한 스레드가 DrainBuckets
			   등에서 여러 버킷을 순회할 때의 단일 스레드 캐시 지역성을
			   위한 것입니다.
	---------------*/
	struct alignas(16) TlsBucket
	{
		MemoryHeader* freeList = nullptr; // 로컬 free-list 시작 노드
		int32 count = 0;                  // 현재 로컬에 쌓인 블록 수
	};

	/*-------------
		TlsCache

		설명 : 스레드마다 하나씩 존재하는(thread_local) 캐시 컨테이너.
			   POOL_COUNT개의 TlsBucket을 배열로 가지고 있으며, 스레드가
			   종료될 때 소멸자가 자동 호출되어 로컬에 남은 모든 블록을
			   전역 CMemoryPool로 반납합니다(워커 스레드 경로). 메인
			   스레드처럼 자연 종료 시점을 신뢰할 수 없는 경우를 위해
			   동일한 반납 로직을 FlushCurrentThreadCache()로도 노출합니다.
	---------------*/
	struct TlsCache
	{
		TlsBucket buckets[POOL_COUNT];

		// 설명 : 스레드 종료 시 자동 호출되어, 남아있는 모든 로컬 블록을
		//        해당 크기의 전역 CMemoryPool에 되돌립니다.
		~TlsCache();
	};

	// 설명 : buckets 배열에 남은 모든 블록을 전역 풀로 반납하는 공통 로직.
	//        TlsCache 소멸자(스레드 자연 종료 시)와 FlushCurrentThreadCache
	//        (수동 호출 시) 양쪽에서 재사용됩니다.
	//
	//        gpMemory가 이미 파괴된 뒤(예: 정해진 파괴 순서 규약을 지키지
	//        않는 외부 스레드가 프로세스 종료 시점 근처에 뒤늦게 종료되는
	//        경우)에는 gpMemory->_pools에 접근하는 대신 아무 동작도 하지
	//        않고 반환합니다. 이 시점에 남은 블록은 어차피 프로세스
	//        종료와 함께 OS가 회수하므로, use-after-free로 크래시를
	//        내는 것보다 조용히 넘어가는 편이 안전합니다.
	// 매개변수 : buckets - 비워낼 TlsBucket 배열 (크기 POOL_COUNT)
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