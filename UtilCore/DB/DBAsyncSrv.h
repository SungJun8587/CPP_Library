
//***************************************************************************
// DBAsyncSrv.h : interface for the CDBAsyncSrv class.
//
//***************************************************************************

#ifndef __DBASYNCSRV__H__
#define __DBASYNCSRV__H__

#pragma pack(push, 1)

#ifndef __ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

//***************************************************************************
// 비동기 요청 베이스 구조체
//	- BaseAllocator를 상속받아 오버라이드된 operator new/delete를 사용합니다.
//	- 이 구조체를 상속받는 모든 파생 요청 구조체(예: CONSUMER_DATA_BATCH_REQ 등)가 별도의 할당 코드 작성 없이 자동으로 RawAllocator 경로를 타도록 설계되었습니다.
//	- BaseAllocator는 데이터 멤버가 없어 #pragma pack(push, 1)에 의한 메모리 패킹(1바이트 정렬)과 구조체 크기에 영향을 주지 않습니다.
struct st_DBAsyncRq : public BaseAllocator
{
	st_DBAsyncRq()
	{
		callIdent = 0;
		bReTry = false;
	}

	// 베이스 포인터(st_DBAsyncRq*)를 통해 파생 객체를 delete(또는 SAFE_DELETE)할 때, 
	// 소멸자가 가상 함수가 아니라면 파생 클래스의 소멸자가 호출되지 않아 메모리 누수나 
	// 정의되지 않은 행동(UB, Undefined Behavior)이 발생할 수 있습니다.
	// 특히 파생 구조체인 CONSUMER_DATA_BATCH_REQ는 크기가 수백 KB에 달하므로, sized delete 시 
	// 잘못된 베이스 크기(2바이트)가 전달되어 힙(Heap)이 손상될 위험을 가상 소멸자를 통해 완벽히 
	// 방지했습니다.
	virtual ~st_DBAsyncRq() {
		//LOG_ERROR(_T("Delete st_DBAsyncRq")); 
	}

	BYTE	callIdent;						// 핸들러 식별자
	bool	bReTry;							// 재시도 여부
};

//***************************************************************************
// 비동기 응답 베이스 구조체
//	- BaseAllocator를 상속받아 오버라이드된 operator new/delete를 사용합니다.
//	- 이 구조체를 상속받는 모든 파생 응답 구조체(예: CONSUMER_DATA_BATCH_RES 등)가 별도의 할당 코드 작성 없이 자동으로 RawAllocator 경로를 타도록 설계되었습니다.
//	- BaseAllocator는 데이터 멤버가 없어 #pragma pack(push, 1)에 의한 메모리 패킹(1바이트 정렬)과 구조체 크기에 영향을 주지 않습니다.
struct st_DBAsyncRp : public BaseAllocator
{
	st_DBAsyncRp()
	{
		callIdent = 0;
		pthis = nullptr;
	}

	virtual ~st_DBAsyncRp() {
		LOG_ERROR(_T("Delete st_DBAsyncRp"));
	}

	BYTE			callIdent;						// 핸들러 식별자
	st_DBAsyncRp*	pthis;
};

#pragma pack(pop)

//***************************************************************************
// 비동기 핸들러 인터페이스
class CDBAsyncSrvHandler
{
public:
	CDBAsyncSrvHandler() {}
	virtual ~CDBAsyncSrvHandler() {}

	virtual EDBReturnType ProcessAsyncCall(st_DBAsyncRq* pStAsync) = 0;
};

#endif // ndef __DBASYNCSRV__H__