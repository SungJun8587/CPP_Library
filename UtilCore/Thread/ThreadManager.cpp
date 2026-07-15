
//***************************************************************************
// ThreadManager.cpp: implementation of the CThreadManager class.
//
//***************************************************************************

#include "pch.h"
#include "ThreadManager.h"

// 프로젝트에 CMemory 모듈이 포함된 경우에만 TLS 캐시 flush 호출을 활성화.
// __MEMORY_H__는 Memory.h에 정의된 인클루드 가드를 그대로 재사용하여,
// ThreadManager가 CMemory 모듈 존재 여부와 무관하게 독립적으로 컴파일될 수
// 있도록 합니다(BaseGlobal.cpp와 동일한 패턴).
#ifdef __MEMORY_H__
#include "Memory.h"
#endif

//***************************************************************************
// Construction/Destruction
//***************************************************************************
CThreadManager::CThreadManager()
	: _threads()
	, _lock()
{
}

// 설명 : JoinThreads()를 호출해 모든 워커 스레드의 자연 종료를 대기합니다.
//        각 워커 스레드는 종료 직전 DestroyTLS()에서 자신의 CMemory TLS
//        캐시를 명시적으로 비우므로, 이 소멸자가 반환되는 시점에는 모든
//        워커 스레드의 캐시가 이미 전역 풀로 반납된 상태입니다.
CThreadManager::~CThreadManager()
{
	JoinThreads();
}

//***************************************************************************
// 설명 : 새 워커 스레드를 생성합니다. 스레드 진입 시 InitTLS()로 스레드
//        ID를 부여하고, 콜백 실행이 끝나면 DestroyTLS()로 스레드 로컬
//        자원을 정리한 뒤 스레드가 종료됩니다.
void CThreadManager::CreateThread(function<void(void)> fncCallback)
{
	lock_guard<mutex> lock(_lock);

	// create new thread
	_threads.push_back(thread([=]()
		{
			// set thread id
			InitTLS();
			fncCallback();
			DestroyTLS();
		}));
}

//***************************************************************************
// 설명 : 보유한 모든 워커 스레드가 종료될 때까지 대기(join)한 뒤 목록을
//        비웁니다. join이 반환된다는 것은 해당 스레드의 모든 thread_local
//        객체(소멸자 포함)가 이미 정리 완료되었음을 의미합니다.
void CThreadManager::JoinThreads()
{
	lock_guard<mutex> lock(_lock);

	for( thread& thread : _threads )
	{
		if( thread.joinable() )
			thread.join();
	}

	_threads.clear();
}

//***************************************************************************
// 설명 : 스레드 로컬 ID(LThreadId)를 발급합니다. SThreadId는 모든 스레드가
//        공유하는 원자적 카운터로, 스레드마다 겹치지 않는 ID를 순차 부여.
void CThreadManager::InitTLS()
{
	static std::atomic<uint32> SThreadId = 1;
	LThreadId = SThreadId.fetch_add(1);
}

//***************************************************************************
// 설명 : 스레드 종료 직전 호출되어 스레드 로컬 자원을 정리합니다.
//
//        CMemory 모듈이 포함된 빌드에서는 이 스레드의 CMemory::TlsCache에
//        남아있는 블록을 명시적으로 전역 풀에 반납합니다. 이 반납은
//        thread_local 소멸자(자동 호출)로도 결국 일어나지만, 여기서
//        명시적으로 한 번 더 호출해 "콜백 종료 -> 이 시점에는 이미
//        스레드가 쓰던 메모리 캐시가 비어있다"는 것을 코드로 명확히
//        보장합니다. (여러 thread_local 객체 간 소멸 순서에 대한 암묵적
//        가정을 줄이는 방어적 설계)
void CThreadManager::DestroyTLS()
{
#ifdef __MEMORY_H__
	CMemory::FlushCurrentThreadCache();
#endif
}