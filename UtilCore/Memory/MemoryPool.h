
//***************************************************************************
// MemoryPool.h
//
// 설명 : 고정 크기 메모리 블록을 재사용하기 위한 Lock-free 메모리 풀.
//        Windows SLIST(Interlocked Singly Linked List)를 이용해 뮤텍스 없이
//        멀티스레드 환경에서 블록을 안전하게 Push/Pop 할 수 있습니다.
//***************************************************************************

#ifndef __MEMORYPOOL_H__
#define __MEMORYPOOL_H__

#pragma once

enum
{
	SLIST_ALIGNMENT = 16
};

/*-----------------
	MemoryHeader

	설명 : 사용자에게 반환되는 모든 메모리 블록 앞에 붙는 헤더.
		   [MemoryHeader][실제 데이터] 형태로 배치되며, SLIST_ENTRY를
		   상속하여 별도의 next 포인터 없이 헤더 자체가 SLIST 노드로
		   재사용됩니다.

		   allocSize(atomic<int32>)는 단순 통계가 아니라 "블록이
		   살아있는지(> 0)/반납되었는지(0)"를 나타내는 이중 반납 탐지
		   플래그이기도 합니다. atomic으로 선언한 이유와 exchange() 기반
		   TOCTOU 경쟁 방지 방식은 MemoryModule_설명.md 3.5절 참고.
------------------*/

DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
	// 설명 : 헤더를 생성하며 allocSize(헤더 포함 전체 크기)를 기록합니다.
	//        placement new 직후라 아직 이 헤더를 가리키는 다른 포인터가
	//        없어 경쟁 상태 없이 안전합니다.
	// 매개변수 : size - 헤더를 포함한 전체 할당 크기
	MemoryHeader(int32 size) : allocSize(size) {}

	// 설명 : 원시 메모리 블록에 헤더를 placement new로 얹고, 데이터 시작
	//        포인터를 반환합니다([Header][Data] 배치이므로 header를 하나
	//        증가시키면 Data 영역의 시작 주소가 됨).
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
	atomic<int32> allocSize;
	// TODO : 필요한 추가 필드
};

/*-----------------
	CMemoryPool

	설명 : 고정 크기(_allocSize) 블록만을 다루는 Lock-free 프리리스트.
		   Pop()은 풀이 비어있으면 즉시 raw 신규 할당하고, Push()는 실제
		   OS 반환 없이 SLIST에 되돌려 다음 Pop()에서 재사용합니다.

		   BaseAllocator를 상속하지 않는 이유, 클래스 64바이트 정렬과
		   캐시 라인 분리(_header vs _useCount/_reserveCount) 설계는
		   MemoryModule_설명.md 3.5절 참고.
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
	MemoryHeader* Pop();

private:
	// Windows Lock-free Singly Linked List 헤더 (Interlocked API로 조작)
	SLIST_HEADER	_header;
	// 이 풀이 다루는 고정 블록 크기
	int32			_allocSize = 0;

	// 통계용 카운터 - SLIST(_header)와 별도 캐시 라인으로 분리(3.5절 참고)
	alignas(64) atomic<int32>	_useCount = 0;      // 현재 사용 중인 블록 수
	atomic<int32>				_reserveCount = 0;  // 현재 풀에 대기 중인 블록 수
};

#endif // ndef __MEMORYPOOL_H__