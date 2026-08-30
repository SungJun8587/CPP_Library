# 📘 CThreadManager 설명 문서

## 개요
`CThreadManager`는 워커 스레드의 **생성, 종료(Join), TLS 초기화/정리**를 전담하는 클래스입니다.  
외부 코드가 `std::thread`를 직접 다루지 않고, 항상 이 클래스에 위임하도록 설계되었습니다.

---

## 주요 특징
- **스레드 소유권**: 모든 스레드 핸들은 `CThreadManager`가 관리합니다.
- **락 사용 규칙**: 컨테이너 수정은 락 안에서만, `join()`은 락 밖에서 수행합니다.
- **종료 플래그**: 전체 종료(`JoinThreads`) 중에는 새 스레드 생성이 거부됩니다. 반면 `JoinLastThreads`(부분 정리)는 이후에도 스레드 생성이 계속될 수 있는 정상 흐름의 일부이므로 종료 플래그와 무관하게 동작합니다.
- **부분 종료**: 일부 스레드만 정리할 수 있습니다.

---

## 멤버 변수

| 멤버 | 타입 | 설명 |
|---|---|---|
| `_threads` | `std::vector<std::thread>` | 관리 중인 워커 스레드 핸들 목록 |
| `_lock` | `mutable std::mutex` | `_threads` 컨테이너 동기화용 뮤텍스 |
| `_bShuttingDown` | `std::atomic<bool>` (기본값 `false`) | 전체 종료(`JoinThreads`) 절차 진입 여부 플래그 |

---

## 멤버 함수

### 공개(public)

| 함수 | 반환값 | 설명 |
|---|---|---|
| `CThreadManager()` | - | 기본 생성자. `_threads`/`_lock`을 멤버 초기화 리스트로 기본 생성 |
| `~CThreadManager()` | - | `JoinThreads()` 호출로 남아있는 워커 스레드를 모두 정리 |
| `CreateThread(fncCallback)` | `bool` | 락 안에서 `_bShuttingDown` 확인 후(진행 중이면 즉시 `false`) `_threads`에 새 스레드를 `emplace_back`. 스레드 진입 시 `InitTLS()` → 콜백 실행 → `DestroyTLS()` 순서로 실행하며, 콜백 예외 발생 시 `DestroyTLS()` 호출 후 재던짐(로그 없음, `std::terminate()`로 이어짐) |
| `JoinThreads()` | `void` | `_bShuttingDown`을 `true`로 설정하고 `_threads` 전체를 락 밖으로 옮긴 뒤 모두 `join()`. 호출 후 목록은 빈 상태가 됨 |
| `JoinLastThreads(count)` | `void` | `_threads` 뒤쪽(가장 최근 생성된 순서)에서 `count`개(또는 전체 크기 중 더 작은 값)를 골라 락 밖에서 `join()` |
| `JoinThreadByIndex(index)` | `void` | `index`가 범위를 벗어나거나 이미 join 불가능하면 아무 동작 없이 반환. 유효하면 목록에서 제거 후 락 밖에서 `join()` |
| `JoinThreadById(threadId)` | `void` | `std::find_if`로 ID가 일치하는 스레드를 찾아 제거 후 락 밖에서 `join()`. 못 찾으면 아무 동작 없음 |
| `GetThreadCount() const` | `size_t` | 락을 잡고 `_threads.size()` 반환 |
| `IsShuttingDown() const` | `bool` | (헤더 인라인) `_bShuttingDown.load()` 반환 |
| `RequestShutdown()` | `void` | (헤더 인라인) `_bShuttingDown.store(true)`만 수행하고 Join은 하지 않음(멱등 — 이미 종료 절차 중이어도 재호출 안전) |

### 비공개(private)

| 함수 | 반환값 | 설명 |
|---|---|---|
| `InitTLS()` | `void` | 스레드 진입 시 호출. `static std::atomic<uint32> SThreadId`를 `fetch_add`하여 스레드 로컬 `LThreadId`를 발급 |
| `DestroyTLS()` | `void` | 스레드 종료 직전 호출. `UC_MEMORY_H`가 정의된 빌드에서만 `CMemory::FlushCurrentThreadCache()` 실행 |

---

## 제공 메서드
- **CreateThread**: 콜백을 받아 새 워커 스레드 생성  
  ⚠️ 콜백 실행 중 예외가 발생하면 `catch(...)`에서 `DestroyTLS()`만 호출한 뒤 그대로 재던집니다. **로그는 남기지 않습니다** — 재던진 예외가 `std::thread`의 진입 함수를 벗어나므로 표준에 의해 `std::terminate()`가 호출되어 프로세스가 즉시 종료됩니다.
- **JoinThreads**: 모든 스레드 종료
- **JoinLastThreads**: 최근 생성된 스레드 일부만 종료
- **JoinThreadByIndex**: 인덱스로 특정 스레드 종료  
  ⚠️ `erase`로 인해 인덱스가 당겨지므로 조회 직후 즉시 사용하는 **일회성 식별자**로만 사용해야 합니다.
- **JoinThreadById**: `std::thread::id`로 특정 스레드 종료
- **GetThreadCount**: 현재 관리 중인 스레드 개수 반환
- **IsShuttingDown**: `_bShuttingDown` 플래그를 atomic load로 확인. 워커 스레드가 주기적으로 호출해 종료 시점을 감지하고 루프를 탈출하는 용도
- **RequestShutdown**: Join은 하지 않고 `_bShuttingDown` 플래그만 먼저 세팅(멱등). IOCP 등으로 워커를 깨우기 전에 미리 호출해두는 용도이며, 실제 정리는 별도로 `JoinThreads()`를 호출해야 함

---

## 예외 처리
- `CreateThread` 내부에서 콜백 실행 중 예외 발생 시:
  - `catch(...)`에서 예외 타입을 구분하지 않고 `DestroyTLS()`만 호출한 뒤 그대로 재던짐(로그 없음).
  - 재던진 예외가 스레드 진입 함수를 벗어나 `std::terminate()`가 호출되고, 프로세스가 로그 없이 즉시 종료됨. 콜백 내부에서 발생 가능한 예외는 반드시 콜백 쪽에서 자체적으로 처리(try/catch)해야 함.

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
  → 로그로는 알 수 없습니다. `CreateThread`는 콜백 예외를 잡아도 로그를 남기지 않고 `DestroyTLS()`만 호출한 뒤 재던지며, 이 예외가 스레드 진입 함수를 벗어나면 `std::terminate()`가 호출되어 프로세스가 즉시 종료됩니다. 원인을 로그로 확인하고 싶다면 콜백 내부에서 직접 try/catch로 잡아 처리해야 합니다.

- **JoinThreads와 JoinLastThreads의 차이는 무엇인가요?**  
  → `JoinThreads`는 전체 종료 플래그를 세우고 모든 스레드를 정리합니다.  
     `JoinLastThreads`는 일부만 정리하며 이후에도 새 스레드 생성이 가능합니다.

---

## ⚡ 성능 최적화 팁

- **스레드 ID 검색**: `JoinThreadById`는 선형 탐색을 사용합니다. 스레드 수가 많아질 경우 `unordered_map<std::thread::id, std::thread>`를 병행하면 성능이 개선됩니다.
- **예외 안전성**: `std::scope_exit`(C++17 이후)를 사용하면 `DestroyTLS()` 호출을 더 깔끔하게 보장할 수 있습니다.
- **컨테이너 관리**: `erase`로 인덱스가 당겨지는 점을 고려해, 인덱스 기반 접근 대신 ID 기반 접근을 권장합니다.
- **락 범위 최소화**: 컨테이너 수정만 락 안에서 수행하고, join은 락 밖에서 처리 → 데드락 방지 및 성능 향상.
