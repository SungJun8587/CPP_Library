
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

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

//***************************************************************************
// @brief 워커 스레드의 생성/종료(Join)/TLS 초기화-정리를 전담하는 클래스.
//
// @details
//  - 스레드 소유권과 Join 책임은 항상 이 클래스가 갖는다. 외부 코드는
//    std::thread 핸들을 직접 들고 다루지 않고, 개수(count) 또는 인덱스/ID
//    같은 식별 정보만으로 이 클래스에 Join을 위임한다.
//  - join()처럼 블로킹되는 호출은 항상 락 밖에서 수행한다. 락 안에서는
//    _threads 컨테이너의 구조 변경(추가/이동/삭제)만 수행하며, 실제 대기는
//    락이 풀린 뒤 로컬로 옮겨진 스레드 핸들에 대해서만 이루어진다.
//  - JoinThreads()(전체 종료)가 진행 중이면 새로운 스레드 생성을 거부한다.
//    반면 JoinLastThreads()(부분 그룹 정리)는 이후에도 다른 스레드 생성이
//    계속될 수 있는 정상 흐름의 일부이므로 종료 플래그와 무관하게 동작한다.
//
// @note 사용 시 주의사항 및 호출 방법:
//  1. 전체 서비스 종료 시 소멸자 또는 JoinThreads()를 1회 호출하여 모든
//     워커 스레드의 자연 종료를 보장해야 합니다.
//  2. JoinThreadByIndex()를 사용할 때 인덱스는 erase 연산으로 인해
//     뒤쪽 인덱스들이 당겨지므로, 조회 직후 바로 사용하는 **일회성 식별자**
//     로만 사용해야 합니다. 여러 인덱스를 미리 모아 순차적으로 호출하는
//     방식은 안전하지 않습니다.
//  3. CreateThread() 내부에서 콜백 실행 중 예외가 발생하면 DestroyTLS()로
//     TLS 자원을 정리한 뒤 예외를 재던집니다. std::thread의 진입 함수를
//     벗어나는 예외는 표준에 의해 무조건 std::terminate()를 호출하므로,
//     이 경우 별도의 에러 로그 없이 프로세스 전체가 즉시 종료됩니다.
//     콜백 내부에서 발생 가능한 예외는 반드시 콜백 쪽에서 자체적으로
//     처리(try/catch)해야 하며, 이 클래스는 예외를 복구하지 않습니다.
//
// @example 간단한 테스트 코드 예시:
//  ```cpp
//  CThreadManager mgr;
//
//  // 워커 스레드 생성
//  mgr.CreateThread([]() {
//      std::this_thread::sleep_for(std::chrono::seconds(1));
//      printf("Worker finished\n");
//  });
//
//  // 현재 스레드 개수 확인
//  size_t count = mgr.GetThreadCount();
//  printf("Thread count: %zu\n", count);
//
//  // 특정 인덱스 스레드 종료
//  mgr.JoinThreadByIndex(0);
//
//  // 전체 종료
//  mgr.JoinThreads();
//  ```
//***************************************************************************
class CThreadManager
{
public:
    CThreadManager();
    ~CThreadManager();

    bool CreateThread(std::function<void(void)> fncCallback);
    void JoinThreads();
    void JoinLastThreads(size_t count);
    void JoinThreadByIndex(size_t index);
    void JoinThreadById(std::thread::id threadId);
    size_t GetThreadCount() const;

    //***************************************************************************
    // @brief 현재 스레드 매니저가 전체 종료(Shutting Down) 절차에 진입했는지 여부를 반환합니다.
    // 
    // @details
    //  - JoinThreads()가 호출되어 전체 종료 플래그(_bShuttingDown)가 true로 
    //    설정되었는지 스레드 안전하게(atomic load) 확인합니다.
    //  - 워커 스레드 등에서 이 함수를 주기적으로 호출하여 종료 시점을 감지하고
    //    안전하게 루프를 탈출하는 용도로 활용할 수 있습니다.
    // 
    // @return 종료 절차 진입 시 true, 정상 구동 중인 경우 false
    //***************************************************************************
    bool IsShuttingDown() const { return _bShuttingDown.load(); }

    //***************************************************************************
    // @brief 스레드를 Join하지 않고 종료 플래그(_bShuttingDown)만 먼저 설정합니다.
    //
    // @details
    //  - 외부에서 워커 스레드를 깨우기 위한 신호(예: IOCP PostQueuedCompletionStatus)를
    //    보내기 전에 이 함수로 종료 플래그를 먼저 세팅해두면, 워커 스레드가 깨어난 직후
    //    루프 조건(IsShuttingDown())에서 곧바로 종료를 인지할 수 있다.
    //  - 이후 실제 Join은 JoinThreads()를 별도로 호출해야 한다. 이 함수만 호출하고
    //    JoinThreads()를 호출하지 않으면 스레드는 종료되지 않고 목록에도 남아있는다.
    //  - 이미 종료 절차가 진행 중이어도 안전하게 재호출 가능하다(멱등).
    //***************************************************************************
    void RequestShutdown() { _bShuttingDown.store(true); }

private:
    void InitTLS();
    void DestroyTLS();

private:
    CVector<std::thread>    _threads;                   // 관리 중인 워커 스레드 핸들 목록
    mutable std::mutex      _lock;                      // 스레드 목록 동기화를 위한 뮤텍스
    std::atomic<bool>       _bShuttingDown{ false };    // 전체 종료 절차 진입 여부 플래그
};

#endif // ndef __THREADMANAGER_H__