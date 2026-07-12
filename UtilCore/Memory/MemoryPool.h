//***************************************************************************
// MemoryPool.h
//
// 설명 : 고정 크기 메모리 블록을 재사용하기 위한 Lock-free 메모리 풀.
//        Windows SLIST(Interlocked Singly Linked List)를 이용해 뮤텍스 없이
//        멀티스레드 환경에서 블록을 안전하게 Push/Pop 할 수 있습니다.
//***************************************************************************

#pragma once

enum
{
	SLIST_ALIGNMENT = 16
};

/*-----------------
	MemoryHeader

	설명 : 사용자에게 반환되는 모든 메모리 블록 앞에 붙는 헤더.
	       [MemoryHeader][실제 데이터] 형태로 배치되며, SLIST_ENTRY를
	       상속하여 Push/Pop 시 헤더 자체가 링크드리스트의 노드로 사용됩니다
	       (별도의 next 포인터를 추가하지 않고 SLIST_ENTRY의 내부 필드를
	       재사용).
------------------*/

DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
	// 설명 : 헤더를 생성하며 allocSize(헤더 포함 전체 크기)를 기록합니다.
	// 매개변수 : size - 헤더를 포함한 전체 할당 크기
	MemoryHeader(int32 size) : allocSize(size) { }

	// 설명 : 원시 메모리 블록에 헤더를 placement new로 얹고, 사용자에게
	//        반환할 데이터 시작 포인터를 계산합니다. [Header][Data] 배치이므로
	//        header를 하나 증가시키면 곧바로 Data 영역의 시작 주소가 됩니다.
	// 매개변수 : header - 헤더를 얹을 원시 메모리 시작 주소
	//           size   - 헤더를 포함한 전체 할당 크기
	// 반환값   : 사용자에게 반환할 데이터 영역 포인터
	static void* AttachHeader(MemoryHeader* header, int32 size)
	{
		new(header)MemoryHeader(size); // placement new
		return reinterpret_cast<void*>(++header);
	}

	// 설명 : 사용자 데이터 포인터로부터 그 앞에 붙어있는 헤더의 주소를
	//        역산합니다(포인터를 하나 감소).
	// 매개변수 : ptr - 사용자에게 반환됐던 데이터 포인터
	// 반환값   : 해당 데이터에 대응하는 헤더 포인터
	static MemoryHeader* DetachHeader(void* ptr)
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
		return header;
	}

	// 헤더 포함 전체 할당 크기. 0이면 "풀에 반납된 상태"를 의미(Push에서 설정).
	int32 allocSize;
	// TODO : 필요한 추가 필드
};

/*-----------------
	CMemoryPool

	설명 : 고정 크기(_allocSize) 블록만을 다루는 Lock-free 프리리스트.
	       Pop() 시 풀에 여유 블록이 없으면 즉시 신규 할당(raw)하여 반환하고,
	       Push() 시에는 실제 OS 반환 없이 내부 SLIST에 되돌려 다음 Pop()에서
	       재사용되도록 합니다. 이를 통해 malloc/free를 반복 호출하는 대신
	       메모리를 재사용해 할당 비용을 크게 절감합니다.

	       BaseAllocator는 상속받지 않습니다. CMemoryPool 인스턴스는
	       CMemory 생성자 안에서 개수가 정해진 채(POOL_COUNT개) 시작 시
	       한 번만 생성되므로, 이 할당을 raw 경로로 격리하는 것보다 전역
	       new/delete를 그대로 쓰는 단순함을 우선했습니다.

	       클래스 자체를 64바이트(일반적인 CPU 캐시 라인 크기) 경계에
	       정렬해, 서로 다른 CMemoryPool 인스턴스가 같은 캐시 라인을
	       공유해 false sharing이 발생하는 것을 막습니다. (64는 SLIST가
	       요구하는 최소 정렬인 SLIST_ALIGNMENT(16)의 배수이므로 SLIST
	       요구사항도 함께 만족합니다.) 또한 SLIST 헤더(_header, Pop/Push
	       때마다 원자적으로 갱신)와 순수 통계용 카운터(_useCount/
	       _reserveCount)를 서로 다른 캐시 라인에 배치해, 통계 조회가
	       SLIST 원자 연산과 캐시 라인을 두고 경합하지 않도록 합니다.
------------------*/

DECLSPEC_ALIGN(64)
class CMemoryPool
{
public:
	// 설명 : 이 풀이 다룰 고정 블록 크기(allocSize)를 지정하고, SLIST 헤더를
	//        초기화합니다.
	// 매개변수 : allocSize - 이 풀에서 다룰 고정 블록 크기(헤더 포함, 바이트)
	CMemoryPool(int32 allocSize);

	// 설명 : 소멸 시 SLIST에 남아있는 모든 블록을 raw 메모리로 실제 해제합니다.
	~CMemoryPool();

	// 설명 : 블록을 사용 종료 표시(allocSize = 0)한 뒤 Lock-free SLIST에
	//        되돌립니다. 통계용 카운터(useCount/reserveCount)도 함께 갱신합니다.
	// 매개변수 : ptr - 반납할 메모리 블록(헤더) 포인터
	void			Push(MemoryHeader* ptr);

	// 설명 : SLIST에서 블록을 하나 꺼냅니다. 풀이 비어있으면 raw 메모리를
	//        새로 할당하여 반환합니다(풀은 "최대 크기 제한 없는 동적 확장형"
	//        프리리스트로 동작).
	// 반환값 : 사용 가능한 메모리 블록(헤더) 포인터
	MemoryHeader*	Pop();

private:
	// Windows Lock-free Singly Linked List 헤더 (Interlocked API로 조작)
	SLIST_HEADER	_header;
	// 이 풀이 다루는 고정 블록 크기
	int32			_allocSize = 0;

	// 통계용 카운터를 별도 캐시 라인으로 분리 - Pop/Push가 SLIST를 원자적으로
	// 조작하는 것과, 통계를 읽는 스레드가 이 카운터에 접근하는 것이 같은
	// 캐시 라인을 두고 경합하지 않도록 함
	alignas(64) atomic<int32>	_useCount = 0;      // 현재 사용 중(꺼내어진 채 반납되지 않은) 블록 수 - 통계/모니터링용
	atomic<int32>				_reserveCount = 0;  // 현재 풀 내에 대기 중인(재사용 가능한) 블록 수 - 통계/모니터링용
	                                                // (_useCount와 같은 캐시 라인 - 두 카운터는 항상 Push/Pop에서
	                                                //  함께 갱신되므로 같은 라인에 있어도 추가 경합이 없음)
};
