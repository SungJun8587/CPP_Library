# 📘 CThreadManager 설명 문서

## 개요
`CThreadManager`는 워커 스레드의 **생성, 종료(Join), TLS 초기화/정리**를 전담하는 클래스입니다.  
외부 코드가 `std::thread`를 직접 다루지 않고, 항상 이 클래스에 위임하도록 설계되었습니다.

---

## 주요 특징
- **스레드 소유권**: 모든 스레드 핸들은 `CThreadManager`가 관리합니다.
- **락 사용 규칙**: 컨테이너 수정은 락 안에서만, `join()`은 락 밖에서 수행합니다.
- **종료 플래그**: 전체 종료 중에는 새 스레드 생성이 거부됩니다.
- **부분 종료**: 일부 스레드만 정리할 수 있습니다.

---

## 제공 메서드
- **CreateThread**: 콜백을 받아 새 워커 스레드 생성  
  ⚠️ 예외 발생 시 `LOG_ERROR`로 메시지를 남기고 재던집니다.
- **JoinThreads**: 모든 스레드 종료
- **JoinLastThreads**: 최근 생성된 스레드 일부만 종료
- **JoinThreadByIndex**: 인덱스로 특정 스레드 종료  
  ⚠️ `erase`로 인해 인덱스가 당겨지므로 조회 직후 즉시 사용하는 **일회성 식별자**로만 사용해야 합니다.
- **JoinThreadById**: `std::thread::id`로 특정 스레드 종료
- **GetThreadCount**: 현재 관리 중인 스레드 개수 반환

---

## 예외 처리
- `CreateThread` 내부에서 콜백 실행 중 예외 발생 시:
  - `LOG_ERROR`로 메시지를 남김 (`std::exception`이면 `e.what()`, 아니면 "Unhandled unknown exception").
  - 이후 `DestroyTLS()` 호출 후 예외 재던짐 → 프로세스 종료 시 로그로 원인 확인 가능.

---

## 테스트 코드 예시
```cpp
#include "ThreadManager.h"
#include <iostream>
#include <chrono>

int main()
{
    CThreadManager mgr;

    // 워커 스레드 생성
    mgr.CreateThread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Worker finished" << std::endl;
    });

    // 현재 스레드 개수 확인
    size_t count = mgr.GetThreadCount();
    std::cout << "Thread count: " << count << std::endl;

    // 특정 인덱스 스레드 종료
    mgr.JoinThreadByIndex(0);

    // 전체 종료
    mgr.JoinThreads();

    return 0;
}
```

---

## ❓ FAQ

- **인덱스 여러 개를 모아 순차적으로 Join해도 되나요?**  
  → 아니요. `erase`로 인해 인덱스가 당겨지므로 조회 직후 즉시 사용하는 경우에만 안전합니다.

- **스레드가 예외로 죽으면 어떻게 알 수 있나요?**  
  → `CreateThread` 내부에서 `LOG_ERROR`로 예외 메시지를 남기므로 로그를 확인해야 합니다.

- **JoinThreads와 JoinLastThreads의 차이는 무엇인가요?**  
  → `JoinThreads`는 전체 종료 플래그를 세우고 모든 스레드를 정리합니다.  
     `JoinLastThreads`는 일부만 정리하며 이후에도 새 스레드 생성이 가능합니다.

---

## ⚡ 성능 최적화 팁

- **스레드 ID 검색**: `JoinThreadById`는 선형 탐색을 사용합니다. 스레드 수가 많아질 경우 `unordered_map<std::thread::id, std::thread>`를 병행하면 성능이 개선됩니다.
- **예외 안전성**: `std::scope_exit`(C++17 이후)를 사용하면 `DestroyTLS()` 호출을 더 깔끔하게 보장할 수 있습니다.
- **컨테이너 관리**: `erase`로 인덱스가 당겨지는 점을 고려해, 인덱스 기반 접근 대신 ID 기반 접근을 권장합니다.
- **락 범위 최소화**: 컨테이너 수정만 락 안에서 수행하고, join은 락 밖에서 처리 → 데드락 방지 및 성능 향상.
