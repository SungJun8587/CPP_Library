
//***************************************************************************
// MemoryPool.h
//
// @brief 고정 크기 메모리 블록을 재사용하기 위한 Lock-free 메모리 풀.
// @details Windows SLIST(Interlocked Singly Linked List)를 이용해 뮤텍스 없이
//          멀티스레드 환경에서 블록을 안전하게 Push/Pop 할 수 있습니다.
//
// [수정 내용]
// MemoryHeader::allocSize / CMemoryPool::_allocSize 및 관련 함수
// 시그니처를 int32 -> size_t로 변경해, PoolAllocator부터 이어지는 size_t
// 파라미터가 이 계층에서 더 이상 좁혀지지 않도록 했습니다. MemoryHeader는
// DECLSPEC_ALIGN(SLIST_ALIGNMENT)(16바이트) 제약상 atomic<int32>였을 때도
// 이미 16바이트로 패딩되고 있었으므로, atomic<size_t>(8바이트, x64 기준)로
// 바뀌어도 실제 구조체 크기나 메모리 사용량에는 변화가 없습니다
// (SLIST_ENTRY 8바이트 + allocSize 8바이트 = 16바이트, 패딩 없음).
//***************************************************************************

#ifndef UC_MEMORYPOOL_H
#define UC_MEMORYPOOL_H

//***************************************************************************
// @brief 메모리 정렬 및 캐시 라인 관련 기준값.
//***************************************************************************
enum
{
	// Windows SLIST에 사용하는 메모리 블록의 최소 정렬 단위
	SLIST_ALIGNMENT = 16,

	// CPU 캐시 라인 단위 정렬 기준. false sharing 완화에 사용
	CACHE_LINE_ALIGNMENT = 64
};

//***************************************************************************
// @brief 사용자에게 반환되는 모든 메모리 블록 앞에 붙는 헤더.
// @details [MemoryHeader][실제 데이터] 형태로 배치되며, SLIST_ENTRY를
//          상속하여 별도의 next 포인터 없이 헤더 자체가 SLIST 노드로
//          재사용됩니다.
//          allocSize(atomic<size_t>)는 단순 통계가 아니라 "블록이
//          살아있는지(> 0)/반납되었는지(0)"를 나타내는 이중 반납 탐지
//          플래그이기도 합니다. exchange()로 "읽고 0으로 바꾸기"를 단일
//          원자 연산으로 묶어 TOCTOU 경쟁 없이 반납 여부를 판별합니다.
//***************************************************************************
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
	//***************************************************************************
	// @brief 헤더를 생성하며 allocSize(헤더 포함 전체 크기)를 기록합니다.
	// @details placement new 직후라 아직 이 헤더를 가리키는 다른 포인터가 없어 경쟁 상태 없이 안전합니다.
	// @param size 헤더를 포함한 전체 할당 크기
	//***************************************************************************
	MemoryHeader(size_t size) : SLIST_ENTRY{}, allocSize(size) {}

	//***************************************************************************
	// @brief 원시 메모리 블록에 헤더를 placement new로 얹고, 데이터 시작 포인터를 반환합니다.
	// @details [Header][Data] 배치이므로 header를 하나 증가시키면 Data 영역의 시작 주소가 됩니다.
	//          전역 placement new(::new)를 명시해, header 타입 자체가 향후 별도의 operator new(size_t, void*)를
	//          갖게 되더라도 항상 표준 placement new가 선택되도록 합니다.
	// @param header 헤더를 얹을 원시 메모리 시작 주소
	// @param size 헤더를 포함한 전체 할당 크기
	// @return 사용자에게 반환할 데이터 영역 포인터
	//***************************************************************************
	static void* AttachHeader(MemoryHeader* header, size_t size)
	{
		::new(header) MemoryHeader(size); // placement new
		return reinterpret_cast<void*>(++header);
	}

	//***************************************************************************
	// @brief 사용자 데이터 포인터로부터 그 앞에 붙어있는 헤더의 주소를 역산합니다.
	// @details 포인터를 하나 감소시켜 헤더 위치를 찾습니다.
	// @param ptr 사용자에게 반환됐던 데이터 포인터
	// @return 해당 데이터에 대응하는 헤더 포인터
	//***************************************************************************
	static MemoryHeader* DetachHeader(void* ptr)
	{
		MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
		return header;
	}

	// 헤더 포함 전체 할당 크기. 0이면 "풀에 반납된 상태"를 의미(Push에서 설정).
	atomic<size_t> allocSize;
};

// MemoryHeader는 x64 기준 SLIST_ENTRY(8) + atomic<size_t>(8) = 16 bytes.
// 이 크기는 [MemoryHeader][UserData] 레이아웃의 ABI 계약으로 유지한다.
static_assert(sizeof(MemoryHeader) == 16, "MemoryHeader size must remain 16 bytes");

static_assert(alignof(MemoryHeader) >= SLIST_ALIGNMENT, "MemoryHeader alignment must satisfy SLIST_ALIGNMENT");

//***************************************************************************
// @brief 고정 크기(_allocSize) 블록만을 다루는 Lock-free 프리리스트.
// @details Pop()은 풀이 비어있으면 즉시 raw 신규 할당하고, Push()는 실제
//          OS 반환 없이 SLIST에 되돌려 다음 Pop()에서 재사용합니다.
//          BaseAllocator는 상속하지 않습니다(단 한 번만 생성되는 부트스트랩
//          객체라 전역 new/delete의 단순함을 우선). 
//          CACHE_LINE_ALIGNMENT 기준으로 정렬해 인스턴스 간
//          false sharing 가능성을 줄이고, 통계 카운터를 별도의
//          캐시 라인에 배치해 동시 접근으로 인한 간섭을 줄입니다.
//***************************************************************************
DECLSPEC_ALIGN(CACHE_LINE_ALIGNMENT)
class CMemoryPool
{
public:
	CMemoryPool(size_t allocSize);
	~CMemoryPool();

	void			Push(MemoryHeader* ptr);
	void			WarmUpPush(MemoryHeader* ptr);
	MemoryHeader* Pop();

	//***************************************************************************
	// @brief 이 풀이 관리하는 메모리 블록의 크기를 반환합니다.
	// @return 풀의 고정 블록 크기
	//***************************************************************************
	size_t			GetBlockSize() const noexcept { return _allocSize; }

private:
	// Windows Lock-free Singly Linked List 헤더 (Interlocked API로 조작)
	SLIST_HEADER	_header;
	// 이 풀이 다루는 고정 블록 크기
	size_t			_allocSize = 0;

	// 통계용 카운터 - SLIST(_header)와 별도 캐시 라인으로 분리
	// 주의: 이 두 카운터는 "이 풀의 SLIST를 드나든 블록 수"를 셀 뿐,
	// 실제로 사용자에게 살아있는 채로 쥐어진 블록 수와는 다릅니다.
	// CMemory가 TLS 배치 단위로 이 풀에서 미리 꺼내와 캐시에 쌓아두므로,
	// _checkedOutCount에는 아직 사용자에게 전달되지 않고 TLS 캐시에
	// 대기 중인 블록도 포함됩니다. 실제 "살아있는 할당" 수치가 필요하면
	// CMemory::GetLiveAllocationCount()를 사용하십시오.
	alignas(CACHE_LINE_ALIGNMENT) atomic<int32>	_poolCheckedOutCount = 0; // 이 풀의 SLIST 밖으로 나간(반납되지 않은) 블록 수
	atomic<int32>								_poolReserveCount = 0;     // 현재 이 풀의 SLIST에 대기 중인 블록 수
};

static_assert(alignof(CMemoryPool) >= CACHE_LINE_ALIGNMENT, "CMemoryPool alignment must satisfy CACHE_LINE_ALIGNMENT");

#endif // ndef UC_MEMORYPOOL_H