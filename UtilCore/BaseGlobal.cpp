
//***************************************************************************
// This file contains the implementation of processing for external variables.
// 
//***************************************************************************

#include "pch.h"
#include "BaseGlobal.h"

#ifdef __MEMORY_H__
CMemory* gpMemory = nullptr;
#endif

#ifdef __GLOBALQUEUE_H__
CGlobalQueue* gpGlobalQueue = nullptr;
#endif

#ifdef __JOBTIMER_H__
CJobTimer* gpJobTimer = nullptr;
#endif

#ifdef __THREADMANAGER_H__
CThreadManager* gpThreadManager = nullptr;
#endif

#ifdef __DEADLOCKPROFILER_H__
CDeadLockProfiler* gpDeadLockProfiler = nullptr;
#endif

//***************************************************************************
// 프로그램 시작 시(main 함수)에서 호출
//	int main() 
//  {
//		BaseGlobal::Init();
//
//		// 로직 수행
//
//		BaseGlobal::Destroy();
//		return 0;
//	}
//
// [파괴 순서에 대한 중요 규칙]
// CMemory(gpMemory)는 xnew/xdelete를 통해 다른 모든 전역 시스템이 내부적으로
// 사용할 수 있으므로, 반드시
//   - 생성(Init)  : 가장 먼저
//   - 파괴(Destroy): 가장 마지막
// 이 되어야 합니다.
//
// 특히 CThreadManager(gpThreadManager)의 소멸자는 워커 스레드들을 join하는데,
// 각 워커 스레드가 종료되는 순간 그 스레드의 thread_local CMemory::TlsCache
// 소멸자가 자동 호출되어 gpMemory의 전역 풀을 참조합니다. 따라서
// gpThreadManager는 반드시 gpMemory보다 먼저 파괴되어야 합니다.
//
// 메인 스레드는 ThreadManager가 관리하지 않으므로 아무도 join해주지
// 않습니다. main() 종료 시점에야 메인 스레드 자신의 thread_local 소멸자가
// 불리는데, 그 전에 gpMemory가 이미 delete되어 있으면 use-after-free가
// 발생합니다. 따라서 gpMemory를 delete하기 직전, 반드시
// CMemory::FlushCurrentThreadCache()를 명시적으로 호출해 메인 스레드의
// 로컬 캐시를 먼저 비웁니다.
//***************************************************************************
namespace BaseGlobal
{
	// 설명 : 모든 전역 시스템을 생성합니다. CMemory를 가장 먼저 생성해야
	//        이후 생성되는 다른 시스템들이 내부적으로 xnew/xdelete(=
	//        CMemory 기반 풀 할당)를 안전하게 사용할 수 있습니다.
	void Init()
	{
#ifdef __MEMORY_H__
		gpMemory = new CMemory();
#endif

#ifdef __GLOBALQUEUE_H__
		gpGlobalQueue = new CGlobalQueue();
#endif

#ifdef __JOBTIMER_H__
		gpJobTimer = new CJobTimer();
#endif

#ifdef __THREADMANAGER_H__
		gpThreadManager = new CThreadManager();
#endif	

#ifdef __DEADLOCKPROFILER_H__
		gpDeadLockProfiler = new CDeadLockProfiler();
#endif	
	}

	// 설명 : 모든 전역 시스템을 생성의 역순으로 파괴합니다.
	//        CThreadManager를 먼저 파괴해 모든 워커 스레드의 TLS 캐시
	//        반납을 완료시키고, 메인 스레드의 TLS 캐시도 수동으로 비운
	//        뒤, 마지막으로 CMemory를 파괴합니다.
	void Destroy()
	{
		// 1) 워커 스레드부터 정리한다.
		//    JoinThreads()가 각 워커 스레드의 자연 종료를 대기하며,
		//    그 과정에서 워커 스레드들의 CMemory TLS 캐시가 이미
		//    gpMemory의 전역 풀로 반납 완료된다.
#ifdef __THREADMANAGER_H__
		if( gpThreadManager != nullptr ) delete gpThreadManager;
#endif	

#ifdef __DEADLOCKPROFILER_H__
		if( gpDeadLockProfiler != nullptr ) delete gpDeadLockProfiler;
#endif	

#ifdef __JOBTIMER_H__
		if( gpJobTimer != nullptr ) delete gpJobTimer;
#endif

#ifdef __GLOBALQUEUE_H__
		if( gpGlobalQueue != nullptr ) delete gpGlobalQueue;
#endif

		// 2) 메인 스레드 자신의 TLS 캐시를 수동으로 비운다.
		//    메인 스레드는 ThreadManager가 join해주지 않으므로,
		//    gpMemory를 delete하기 직전 반드시 이 호출이 필요하다.
		// 3) CMemory는 다른 시스템들이 내부적으로 참조할 수 있으므로
		//    반드시 가장 마지막에 파괴한다.
#ifdef __MEMORY_H__
		CMemory::FlushCurrentThreadCache();
		if( gpMemory != nullptr ) delete gpMemory;
#endif	
	}
}