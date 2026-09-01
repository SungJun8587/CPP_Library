# 큐 시스템 종합 문서 (Lock-Free 큐)

## 📌 개요
이 문서는 **Lock-Free 큐 구현체**들을 분석합니다. 각 클래스의 **목적, 구현 방식, 장점, 종료 처리, 사용 예시, 게임 서버 적용, 실전 예제 코드**를 제공합니다.

---

## 🔀 MPMCLockFreeQueue
- **목적**: 다중 프로듀서-다중 컨슈머(MPMC) 환경에서 락 없는 범용 큐 제공
- **구현 방식**: Dmitry Vyukov 알고리즘 기반, 슬롯 단위 CAS로 push/pop 소유권 조정
- **장점**:
  - 전역 락 없음 → 높은 동시성
  - 범용적이며 가장 일반적인 lock-free 큐
  - blocking 경로는 짧은 스핀 후 condition_variable로 대기
- **종료 처리**: `Close()`
  - `Close()` 호출 이후 새로운 Push(`TryPush`/`Push` 모두)는 실패합니다.
  - `Close()` 이전에 이미 큐에 들어간 데이터는 계속 Pop할 수 있습니다.
  - 큐가 닫히고 남은 데이터가 없으면 blocking `Pop()`은 `false`를 반환합니다.
  - `Close()`는 대기 중인 모든 producer/consumer를 즉시 깨웁니다.
  - `IsClosed()`로 종료 상태를 조회할 수 있습니다.
  - blocking `Push()`/`Pop()`은 `bool`을 반환하며, `Close()`로 인한 실패와 정상 실패(성공/실패)를 호출자가 구분할 수 있습니다.
- **사용 예시**:
  - 네트워크 패킷 처리 (여러 수신 스레드 → 여러 처리 스레드)
  - 멀티스레드 작업 큐 (스레드 풀)
  - 고성능 로그/메시지 큐
- **게임 서버 적용**:
  - 멀티스레드 패킷 처리
  - 스레드 풀 기반 작업 분배
- **실전 예제**:
```cpp
MPMCLockFreeQueue<int, 1024> queue;

queue.Push(10);
queue.Push(20);

int value;
if (queue.TryPop(value)) {
    std::cout << "처리된 값: " << value << std::endl;
}

// 종료 처리
queue.Close();               // 이후 Push는 모두 실패, 대기 중인 스레드는 즉시 깨어남
while (queue.TryPop(value))  // 이미 들어가 있던 데이터는 마저 drain
    std::cout << "잔여 값: " << value << std::endl;

// 소멸자에서도 남은 데이터를 자동으로 비웁니다.
```

---

## 🔀 MPSCLockFreeQueue
- **목적**: 다중 프로듀서-단일 컨슈머(MPSC) 환경에서 락 없는 큐 제공
- **구현 방식**: producer는 CAS 경쟁, consumer는 단순 인덱스 증가
- **장점**:
  - 단일 소비자 환경에서 효율적
  - producer는 경쟁하지만 consumer는 빠름
  - condition_variable 대신 C++20 `std::atomic::wait`/`notify`로 블로킹 대기
- **종료 처리**: `Close()`
  - `Close()` 호출 이후 새로운 Push는 실패합니다.
  - 이미 큐에 들어간 데이터는 정상적으로 Pop할 수 있습니다.
  - `Close()`는 idempotent이며, 대기 중인 producer/consumer를 즉시 깨웁니다.
  - `IsClosed()`, `IsClosedAndEmpty()`로 상태를 조회할 수 있습니다.
  - blocking `Push()`/`Pop()`은 `bool`을 반환합니다.
- **사용 예시**:
  - 로깅 시스템 (여러 쓰레드 로그 기록 → 단일 쓰레드 출력)
  - 이벤트 큐
  - 네트워크 수신 큐
- **게임 서버 적용**:
  - 로그 수집
  - 이벤트 큐 처리
- **실전 예제**:
```cpp
MPSCLockFreeQueue<int, 1024> queue;

queue.Push(1);
queue.Push(2);

int value;
if (queue.TryPop(value)) {
    std::cout << "처리된 값: " << value << std::endl;
}

// 종료 처리
queue.Close();
while (queue.TryPop(value))
    std::cout << "잔여 값: " << value << std::endl;

// 소멸자에서도 남은 데이터를 자동으로 비웁니다.
```

---

## 🔀 SPMCLockFreeQueue
- **목적**: 단일 프로듀서-다중 컨슈머(SPMC) 환경에서 락 없는 큐 제공
- **구현 방식**: producer는 단순 인덱스 증가, consumer는 CAS 경쟁
- **장점**:
  - producer 경로 단순화 → 성능 향상
  - consumer만 경쟁
  - producer 생성자/이동 대입 예외 발생 시에도 큐 구조를 보존
- **종료 처리**: `Stop()` / `Reset()`
  - `Stop()` 호출 이후 새로운 Push는 거부됩니다.
  - 이미 큐에 들어간 데이터는 Pop으로 계속 drain할 수 있습니다.
  - `Stop()`은 여러 번 호출해도 안전하며, 대기 중인 producer/consumer를 즉시 깨웁니다.
  - blocking `Push()`/`Pop()`은 `void`이므로, `Stop()`으로 인한 삽입 실패 여부를 확인하려면 `TryPush()`를 사용해야 합니다.
  - `Reset()`으로 `Stop()` 상태를 해제하고 큐를 재사용할 수 있습니다(단, 다른 producer/consumer가 없는 상태에서만 호출).
  - `IsStopped()`로 상태를 조회할 수 있습니다.
- **사용 예시**:
  - 이벤트 루프 (단일 쓰레드 생산, 여러 쓰레드 소비)
  - 게임 서버 이벤트 브로드캐스트
  - 로그 처리
- **게임 서버 적용**:
  - 이벤트 브로드캐스트
  - 단일 producer 기반 패킷 분배
- **실전 예제**:
```cpp
SPMCLockFreeQueue<int, 1024> queue;

queue.Push(100);

int value;
if (queue.TryPop(value)) {
    std::cout << "처리된 값: " << value << std::endl;
}

// 종료 처리
queue.Stop();
while (queue.TryPop(value))
    std::cout << "잔여 값: " << value << std::endl;

// 소멸자에서도 남은 데이터를 자동으로 비웁니다.
```

---

## 🔀 SPSCLockFreeQueue
- **목적**: 단일 프로듀서-단일 컨슈머(SPSC) 환경에서 락 없는 큐 제공
- **구현 방식**: CAS 불필요, 단순 인덱스 증가만으로 동작
- **장점**:
  - 가장 빠른 lock-free 큐
  - 구조 단순, 구현 간결
  - producer 생성자/이동 대입 예외 발생 시에도 큐 구조를 보존
- **종료 처리**: 별도의 종료 API 없음
  - `Close()`/`Stop()` 같은 명시적 종료 메서드를 제공하지 않습니다.
  - producer/consumer가 각각 하나뿐인 파이프라인 구조를 전제로, 상위 로직에서 두 스레드를 정상 종료시킨 뒤 큐를 소멸시키는 방식으로 사용합니다.
  - 소멸자가 남은 데이터를 자동으로 비웁니다(소멸 시점에는 다른 스레드가 큐에 접근하지 않아야 함).
- **사용 예시**:
  - 두 쓰레드 간 파이프라인 (예: 네트워크 수신 → 처리)
  - 오디오/비디오 스트리밍 버퍼
  - 저지연 데이터 전달
- **게임 서버 적용**:
  - 네트워크 수신 → 처리 파이프라인
  - 실시간 스트리밍 처리
- **실전 예제**:
```cpp
SPSCLockFreeQueue<int, 1024> queue;

queue.Push(5);

int value;
if (queue.TryPop(value)) {
    std::cout << "처리된 값: " << value << std::endl;
}

// 종료 처리: 별도 API 없이, producer/consumer 스레드를 상위 로직에서
// 정상 종료시킨 뒤 큐를 소멸시킵니다. 소멸자가 남은 데이터를 자동으로 비웁니다.
```

---

# ⚖️ 큐 클래스 통합 비교 표

| 특징 | **MPMCLockFreeQueue** | **MPSCLockFreeQueue** | **SPMCLockFreeQueue** | **SPSCLockFreeQueue** |
|------|--------------------------|--------------------------|--------------------------|--------------------------|
| **패턴** | MPMC | MPSC | SPMC | SPSC |
| **목적** | 범용 다중 producer/consumer | 다중 producer, 단일 consumer | 단일 producer, 다중 consumer | 단일 producer, 단일 consumer |
| **구현 방식** | Vyukov 알고리즘, CAS 경쟁 | producer CAS, consumer 단순 인덱스 | producer 단순 인덱스, consumer CAS | CAS 불필요, 인덱스 증가만 |
| **중점** | 범용성, 높은 동시성 | 단일 consumer 효율성 | producer 성능 최적화 | 최고 성능, 저지연 |
| **락 방식** | Lock-Free + CAS (blocking 대기만 mutex/CV) | Lock-Free + CAS (blocking 대기는 atomic wait/notify) | Lock-Free + CAS (blocking 대기만 mutex/CV) | Lock-Free, CAS 없음 (blocking 대기만 mutex/CV) |
| **종료 처리** | `Close()` / `IsClosed()`, blocking Push/Pop이 `bool` 반환 | `Close()` / `IsClosed()` / `IsClosedAndEmpty()`, blocking Push/Pop이 `bool` 반환 | `Stop()` / `IsStopped()` / `Reset()`(재사용 가능), blocking Push/Pop은 `void` | 없음 — 소멸자에서만 자동 비움 |
| **생성자 예외 허용** | 불허 (nothrow 생성 강제) | 불허 (nothrow 생성 강제) | 허용 (producer 측, position 갱신을 생성 성공 이후로 지연) | 허용 (producer 측, position 갱신을 생성 성공 이후로 지연) |
| **장점** | 범용, 높은 동시성 | 단일 consumer 효율적 | producer 성능 향상 | 가장 빠름, 단순 |
| **사용 예시** | 네트워크 패킷 처리, 스레드 풀 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 파이프라인, 스트리밍 |
| **게임 서버 적용** | 멀티스레드 패킷 처리 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 네트워크 수신 파이프라인 |

---

## 🚀 설계 인사이트
- **MPMCLockFreeQueue** → 가장 범용적이며, 다중 producer/consumer 환경에서 안정적인 성능을 제공. `Close()`로 명시적인 graceful shutdown과 drain을 지원.
- **MPSCLockFreeQueue** → 단일 consumer 환경에서 효율적, 로그 수집이나 이벤트 큐에 적합. `Close()` 기반 종료 처리는 MPMC와 동일한 계약을 따르되, C++20 atomic wait/notify로 블로킹 대기를 구현.
- **SPMCLockFreeQueue** → 단일 producer 환경에서 성능 최적화, 이벤트 브로드캐스트에 적합. `Stop()`/`Reset()`으로 큐를 재사용 가능한 형태로 종료·재개할 수 있음.
- **SPSCLockFreeQueue** → 가장 단순하고 빠른 큐, 저지연 파이프라인에 최적. 명시적 종료 API 없이 두 스레드의 정상 종료를 전제로 사용.
