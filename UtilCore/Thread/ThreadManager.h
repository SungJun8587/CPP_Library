
//***************************************************************************
// ThreadManager.h : interface for the CThreadManager class.
//
// 설명 : 워커 스레드의 생성/보관/join을 관리하는 클래스.
//        스레드 시작/종료 시점에 스레드 로컬 자원(스레드 ID, CMemory의
//        TLS 캐시 등)을 초기화/정리하는 훅(InitTLS/DestroyTLS)을 제공.
//***************************************************************************

#ifndef __THREADMANAGER_H__
#define __THREADMANAGER_H__

#pragma once

#ifndef _THREAD_
#include <thread>
#endif

#ifndef _FUNCTIONAL_
#include <functional>
#endif

#ifndef _ATOMIC_
#include <atomic>
#endif

#ifndef _MUTEX_
#include <mutex>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

class CThreadManager
{
public:
	CThreadManager();
	CThreadManager(const CThreadManager& other) = delete;
	CThreadManager(CThreadManager&& other) = delete;
	CThreadManager& operator=(const CThreadManager& other) = delete;
	CThreadManager& operator=(CThreadManager&& other) = delete;

	// 설명 : 소멸 시 JoinThreads()를 호출해 보유 중인 모든 워커 스레드가
	//        완전히 종료될 때까지 대기합니다.
	//
	//        [중요] 각 워커 스레드가 종료되는 순간 그 스레드의 thread_local
	//        CMemory::TlsCache 소멸자가 자동 호출되어 gpMemory의 전역 풀을
	//        참조합니다. 따라서 이 소멸자(및 JoinThreads)가 완료되기 전에
	//        gpMemory가 먼저 파괴되면 use-after-free가 발생합니다.
	//        -> BaseGlobal::Destroy()에서 반드시 gpThreadManager를
	//           gpMemory보다 먼저 delete해야 합니다.
	~CThreadManager();

	// 설명 : 새 워커 스레드를 생성하고 목록에 등록합니다. 스레드 시작/종료
	//        시점에 InitTLS()/DestroyTLS()를 자동으로 감싸 호출합니다.
	// 매개변수 : function - 스레드에서 실행할 콜백
	void CreateThread(function<void(void)> function);

	// 설명 : 보유 중인 모든 워커 스레드가 종료될 때까지 대기(join)한 뒤
	//        목록을 비웁니다. 이 함수가 반환된 시점에는 모든 워커 스레드의
	//        thread_local 자원(TlsCache 포함)이 이미 정리 완료된 상태입니다.
	void JoinThreads();

	// 설명 : 스레드 시작 시 호출되어 스레드 로컬 ID(LThreadId)를 부여합니다.
	static void InitTLS();

	// 설명 : 스레드 종료 직전(콜백 실행 완료 후) 호출되어 스레드 로컬
	//        자원을 명시적으로 정리합니다. 현재는 CMemory의 TLS 캐시를
	//        명시적으로 flush합니다(자연적인 thread_local 소멸 순서에만
	//        의존하지 않고, 정리 시점을 코드로 명확히 고정하기 위함).
	static void DestroyTLS();

	// 스레드 ID 반환 (TLS 활용)
	static int getThreadID() {
		return LThreadId;
	}

private:
	CVector<thread> _threads;
	mutex _lock;
};

#endif // ndef __THREADMANAGER_H__