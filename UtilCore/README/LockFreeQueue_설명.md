# 큐 시스템 종합 문서 (Lock-Free 큐)

## 📌 개요
이 문서는 **Lock-Free 큐 구현체**들을 분석합니다. 각 클래스의 **목적, 구현 방식, 장점, 종료 처리, 멤버 변수/함수 상세, 사용 예시, 게임 서버 적용, 실전 예제 코드**를 제공합니다.

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
  - blocking `Push()`/`Pop()`은 `bool`을 반환하며, 성공 여부를 호출자가 구분할 수 있습니다.

### 멤버 변수
| 이름 | 타입 | 설명 |
|------|------|------|
| `kIndexMask` | `static constexpr std::size_t` | `Capacity - 1`. 2의 거듭제곱 Capacity에 대해 모듈로 연산을 대체하는 인덱스 마스크 |
| `m_Buffer[Capacity]` | `Cell[]` | 실제 데이터를 저장하는 링 버퍼. 각 `Cell`은 cache line 단위로 정렬되어 false sharing 방지 |
| `m_EnqueuePos` | `std::atomic<std::size_t>` | 다음 삽입 위치. 여러 producer가 CAS로 경쟁 |
| `m_DequeuePos` | `std::atomic<std::size_t>` | 다음 추출 위치. 여러 consumer가 CAS로 경쟁 |
| `m_NotEmptyMutex` / `m_NotFullMutex` | `std::mutex` | blocking 대기 시에만 사용되는 뮤텍스 |
| `m_NotEmptyCv` / `m_NotFullCv` | `std::condition_variable` | 큐 상태 변화를 통보하는 조건 변수 |
| `m_WaitingPoppers` | `std::atomic<int>` | 현재 blocking 대기 중인 consumer 수. notify 스킵 최적화에 사용 |
| `m_WaitingPushers` | `std::atomic<int>` | 현재 blocking 대기 중인 producer 수. notify 스킵 최적화에 사용 |
| `m_Closed` | `std::atomic<bool>` | 큐 종료 상태. `Close()` 호출 시 `true`로 전환 |
| `kSpinBeforeSleepCount` | `static constexpr int` | blocking 대기 진입 전 스핀 반복 횟수(1000) |

### 멤버 함수
| 함수 | 접근 범위 | 반환 타입 | 설명 |
|------|-----------|-----------|------|
| `MPMCLockFreeQueue()` | public | — | 생성자. 모든 슬롯의 Sequence, position, 종료 상태를 초기화 |
| `~MPMCLockFreeQueue()` | public | — | 소멸자. 남아있는 데이터를 직접 소멸(`DrainAndDestroy` 호출) |
| `TryPush(const T&)` / `TryPush(T&&)` | public | `bool` | 논블로킹 삽입. 가득 참/종료 시 `false` |
| `Push(const T&)` / `Push(T&&)` | public | `bool` | 블로킹 삽입. 스핀 후 CV 대기. 종료 시 `false` |
| `TryPop(T&)` | public | `bool` | 논블로킹 추출. 비어있으면 `false` |
| `Pop(T&)` | public | `bool` | 블로킹 추출. 스핀 후 CV 대기. 종료+drain 완료 시 `false` |
| `Close()` | public | `void` | 큐를 종료 상태로 전환하고 대기 중인 모든 스레드를 깨움. idempotent |
| `IsClosed()` | public | `bool` | 종료 상태 조회 |
| `SizeApprox()` | public | `std::size_t` | 대략적인 현재 큐 크기 |
| `GetCapacity()` | public | `constexpr std::size_t` | 큐 최대 용량 |
| `IsEmptyApprox()` | private | `bool` | 대략적인 empty 여부 (`SizeApprox() == 0`) |
| `IsFullApprox()` | private | `bool` | 대략적인 full 여부 (`SizeApprox() >= Capacity`) |
| `NotifyNotEmpty()` | private | `void` | 대기 중인 consumer에게 통보 (lost wakeup 방지) |
| `NotifyNotFull()` | private | `void` | 대기 중인 producer에게 통보 (lost wakeup 방지) |
| `DrainAndDestroy()` | private | `void` | 소멸자 전용. 남은 원소를 직접 소멸 |
| `BlockingEmplacePush(U&&)` | private (template) | `bool` | `Push()`의 실제 구현. 스핀 → CV 대기 순으로 삽입 시도 |
| `EmplacePush(U&&)` | private (template) | `bool` | `TryPush()`/`BlockingEmplacePush()`의 공통 삽입 로직 (CAS 슬롯 예약) |

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

### 멤버 변수
| 이름 | 타입 | 설명 |
|------|------|------|
| `kIndexMask` | `static constexpr std::size_t` | `Capacity - 1`. 인덱스 마스크 |
| `m_Buffer[Capacity]` | `Cell[]` | 실제 데이터를 저장하는 링 버퍼 |
| `m_EnqueuePos` | `std::atomic<std::size_t>` | 다음 삽입 위치. 여러 producer가 CAS로 경쟁. 동시에 consumer notification의 wait/notify 대상 |
| `m_DequeuePos` | `std::atomic<std::size_t>` | 다음 추출 위치. 단일 consumer 전용이라 CAS 불필요. 동시에 producer notification의 wait/notify 대상 |
| `m_Closed` | `std::atomic<bool>` | 큐 종료 상태 |

### 멤버 함수
| 함수 | 접근 범위 | 반환 타입 | 설명 |
|------|-----------|-----------|------|
| `MPSCLockFreeQueue()` | public | — | 생성자. position/종료 상태 초기화 |
| `~MPSCLockFreeQueue()` | public | — | 소멸자. 남은 데이터를 직접 소멸 |
| `TryPush(const T&)` / `TryPush(T&&)` | public | `bool` | 논블로킹 삽입 |
| `Push(const T&)` / `Push(T&&)` | public | `bool` | 블로킹 삽입. `m_DequeuePos`의 atomic wait로 대기 |
| `TryPop(T&)` | public | `bool` | 논블로킹 추출. CAS 없이 relaxed load/store만 사용 |
| `Pop(T&)` | public | `bool` | 블로킹 추출. `m_EnqueuePos`의 atomic wait로 대기 |
| `Close()` | public | `void` | 큐를 종료 상태로 전환. idempotent |
| `IsClosed()` | public | `bool` | 종료 상태 조회 |
| `IsClosedAndEmpty()` | public | `bool` | 종료 상태이며 큐가 완전히 비었는지 조회 |
| `SizeApprox()` | public | `std::size_t` | 대략적인 현재 큐 크기 |
| `GetCapacity()` | public | `constexpr std::size_t` | 큐 최대 용량 |
| `BlockingEmplacePush(U&&)` | private (template) | `bool` | `Push()`의 실제 구현. 스핀 → atomic wait 순으로 삽입 시도 |
| `EmplacePush(U&&)` | private (template) | `bool` | 공통 삽입 로직 (CAS 슬롯 예약, Close 여부 재확인) |

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
- **종료 처리**: `Close()` / `Reset()`
  - `Close()` 호출 이후 새로운 Push는 거부됩니다.
  - 이미 큐에 들어간 데이터는 Pop으로 계속 drain할 수 있습니다.
  - `Close()`는 여러 번 호출해도 안전하며, 대기 중인 producer/consumer를 즉시 깨웁니다.
  - blocking `Push()`/`Pop()`은 `bool`을 반환하며, `Close()`로 인한 실패와 정상 실패를 호출자가 구분할 수 있습니다.
  - `Reset()`으로 `Close()` 상태를 해제하고 큐를 재사용할 수 있습니다(단, 다른 producer/consumer가 없는 상태에서만 호출).
  - `IsClosed()`로 상태를 조회할 수 있습니다.

### 멤버 변수
| 이름 | 타입 | 설명 |
|------|------|------|
| `kIndexMask` | `static constexpr std::size_t` | `Capacity - 1`. 인덱스 마스크 |
| `m_Buffer[Capacity]` | `Cell[]` | 실제 데이터를 저장하는 링 버퍼. cache line 단위 정렬 |
| `m_EnqueuePos` | `std::atomic<std::size_t>` | 다음 삽입 위치. 단일 producer 전용이라 CAS 불필요 |
| `m_DequeuePos` | `std::atomic<std::size_t>` | 다음 추출 위치. 여러 consumer가 CAS로 경쟁 |
| `m_Closed` | `std::atomic<bool>` | 큐 종료 상태. `Close()`/`Reset()`으로 전환 |
| `m_NotEmptyMutex` / `m_NotFullMutex` | `std::mutex` | blocking 대기 시에만 사용되는 뮤텍스 |
| `m_NotEmptyCv` / `m_NotFullCv` | `std::condition_variable` | 큐 상태 변화를 통보하는 조건 변수 |
| `m_WaitingPoppers` | `std::atomic<int>` | 현재 blocking 대기 중인 consumer 수 |
| `m_WaitingPushers` | `std::atomic<int>` | 현재 blocking 대기 중인 producer 수 |
| `kSpinBeforeSleepCount` | `static constexpr int` | blocking 대기 진입 전 스핀 반복 횟수(1000) |

### 멤버 함수
| 함수 | 접근 범위 | 반환 타입 | 설명 |
|------|-----------|-----------|------|
| `SPMCLockFreeQueue()` | public | — | 생성자. Sequence/position/종료 상태 초기화 |
| `~SPMCLockFreeQueue()` | public | — | 소멸자. 정상 publish된 원소만 직접 소멸 |
| `TryPush(const T&)` / `TryPush(T&&)` | public | `bool` | 논블로킹 삽입. 단일 producer 전용, CAS 없이 relaxed load/store 사용 |
| `Push(const T&)` / `Push(T&&)` | public | `bool` | 블로킹 삽입. 종료 시 `false` |
| `TryPop(T&)` | public | `bool` | 논블로킹 추출. move assignment 예외 발생 시에도 슬롯을 반드시 release |
| `Pop(T&)` | public | `bool` | 블로킹 추출. 종료+drain 완료 시 `false` |
| `Close()` | public | `void` | 큐를 종료 상태로 전환하고 대기 중인 모든 스레드를 깨움. idempotent |
| `IsClosed()` | public | `bool` | 종료 상태 조회 |
| `Reset()` | public | `void` | 종료 상태를 해제하여 큐를 재사용 가능하게 함. 다른 스레드가 없을 때만 호출 |
| `SizeApprox()` | public | `std::size_t` | 대략적인 현재 큐 크기 |
| `IsEmptyApprox()` | public | `bool` | 대략적인 empty 여부 |
| `IsFullApprox()` | public | `bool` | 대략적인 full 여부 |
| `GetCapacity()` | public | `constexpr std::size_t` | 큐 최대 용량 |
| `NotifyNotEmpty()` | private | `void` | 대기 중인 consumer에게 통보 (lost wakeup 방지) |
| `NotifyNotFull()` | private | `void` | 대기 중인 producer에게 통보 (lost wakeup 방지) |
| `BlockingEmplacePush(U&&)` | private (template) | `bool` | `Push()`의 실제 구현. 스핀 → CV 대기 순으로 삽입 시도 |
| `EmplacePush(U&&)` | private (template) | `bool` | 공통 삽입 로직. T 생성 성공 이후에만 position 갱신(예외 안전성 보장) |

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
queue.Close();
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
- **종료 처리**: `Close()`
  - `Close()` 호출 이후 새로운 Push는 실패합니다.
  - `Close()` 이전에 삽입된 데이터는 계속 Pop할 수 있습니다.
  - `Close()` 이후 큐가 비어 있으면 blocking `Pop()`은 `false`를 반환합니다.
  - `Close()`는 대기 중인 producer/consumer를 모두 깨우며, 여러 번 호출해도 안전합니다.
  - `IsClosed()`로 상태를 조회할 수 있습니다.
  - blocking `Push()`/`Pop()`은 `bool`을 반환합니다.

### 멤버 변수
| 이름 | 타입 | 설명 |
|------|------|------|
| `kIndexMask` | `static constexpr std::size_t` | `Capacity - 1`. 인덱스 마스크 |
| `m_Buffer[Capacity]` | `Cell[]` | 실제 데이터를 저장하는 링 버퍼. Sequence가 없는 단순화된 Cell 사용 |
| `m_EnqueuePos` | `AlignedAtomicPosition` | 다음 삽입 위치. producer 전용, cache line 정렬 |
| `m_DequeuePos` | `AlignedAtomicPosition` | 다음 추출 위치. consumer 전용, cache line 정렬 |
| `m_NotEmptyMutex` / `m_NotFullMutex` | `std::mutex` | blocking 대기 시에만 사용되는 뮤텍스 |
| `m_NotEmptyCv` / `m_NotFullCv` | `std::condition_variable` | 큐 상태 변화를 통보하는 조건 변수 |
| `m_WaitingPoppers` | `std::atomic<int>` | 현재 blocking 대기 중인 consumer 수 |
| `m_WaitingPushers` | `std::atomic<int>` | 현재 blocking 대기 중인 producer 수 |
| `m_Closed` | `std::atomic<bool>` | 큐 종료 상태 |
| `kSpinBeforeSleepCount` | `static constexpr int` | blocking 대기 진입 전 스핀 반복 횟수(1000) |

### 멤버 함수
| 함수 | 접근 범위 | 반환 타입 | 설명 |
|------|-----------|-----------|------|
| `SPSCLockFreeQueue()` | public | — | 생성자. position/종료 상태 초기화 |
| `~SPSCLockFreeQueue()` | public | — | 소멸자. 남은 데이터를 직접 소멸 |
| `TryPush(const T&)` / `TryPush(T&&)` | public | `bool` | 논블로킹 삽입. CAS 불필요, relaxed/acquire load만 사용 |
| `Push(const T&)` / `Push(T&&)` | public | `bool` | 블로킹 삽입. 종료 시 `false` |
| `TryPop(T&)` | public | `bool` | 논블로킹 추출 |
| `Pop(T&)` | public | `bool` | 블로킹 추출. 종료+drain 완료 시 `false` |
| `SizeApprox()` | public | `std::size_t` | 대략적인 현재 큐 크기 (언더플로 방지 클램프 포함) |
| `GetCapacity()` | public | `constexpr std::size_t` | 큐 최대 용량 |
| `Close()` | public | `void` | 큐를 종료 상태로 전환하고 대기 중인 모든 스레드를 깨움. idempotent |
| `IsClosed()` | public | `bool` | 종료 상태 조회 |
| `HasData()` | private | `bool` | 큐에 데이터가 존재하는지 확인 (CV predicate용) |
| `HasFreeSlot()` | private | `bool` | 큐에 빈 슬롯이 있는지 확인 (CV predicate용) |
| `NotifyNotEmpty()` | private | `void` | 대기 중인 consumer에게 통보 |
| `NotifyNotFull()` | private | `void` | 대기 중인 producer에게 통보 |
| `BlockingEmplacePush(U&&)` | private (template) | `bool` | `Push()`의 실제 구현. 스핀 → CV 대기 순으로 삽입 시도 |
| `EmplacePush(U&&)` | private (template) | `bool` | 공통 삽입 로직. T 생성 성공 이후에만 position 갱신 |

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

// 종료 처리
queue.Close();
while (queue.TryPop(value))
    std::cout << "잔여 값: " << value << std::endl;

// 소멸자에서도 남은 데이터를 자동으로 비웁니다.
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
| **종료 처리** | `Close()` / `IsClosed()`, blocking Push/Pop이 `bool` 반환 | `Close()` / `IsClosed()` / `IsClosedAndEmpty()`, blocking Push/Pop이 `bool` 반환 | `Close()` / `IsClosed()` / `Reset()`(재사용 가능), blocking Push/Pop이 `bool` 반환 | `Close()` / `IsClosed()`, blocking Push/Pop이 `bool` 반환 |
| **생성자 예외 허용** | 불허 (nothrow 생성 강제) | 불허 (nothrow 생성 강제) | 허용 (producer 측, position 갱신을 생성 성공 이후로 지연) | 허용 (producer 측, position 갱신을 생성 성공 이후로 지연) |
| **장점** | 범용, 높은 동시성 | 단일 consumer 효율적 | producer 성능 향상 | 가장 빠름, 단순 |
| **사용 예시** | 네트워크 패킷 처리, 스레드 풀 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 파이프라인, 스트리밍 |
| **게임 서버 적용** | 멀티스레드 패킷 처리 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 네트워크 수신 파이프라인 |

---

## 🚀 설계 인사이트
- **MPMCLockFreeQueue** → 가장 범용적이며, 다중 producer/consumer 환경에서 안정적인 성능을 제공. `Close()`로 명시적인 graceful shutdown과 drain을 지원.
- **MPSCLockFreeQueue** → 단일 consumer 환경에서 효율적, 로그 수집이나 이벤트 큐에 적합. `Close()` 기반 종료 처리는 MPMC와 동일한 계약을 따르되, C++20 atomic wait/notify로 블로킹 대기를 구현.
- **SPMCLockFreeQueue** → 단일 producer 환경에서 성능 최적화, 이벤트 브로드캐스트에 적합. `Close()`/`Reset()`으로 큐를 재사용 가능한 형태로 종료·재개할 수 있으며, 종료 처리와 blocking Push/Pop의 반환 계약이 나머지 세 클래스와 동일하게 통일되어 있음.
- **SPSCLockFreeQueue** → 가장 단순하고 빠른 큐, 저지연 파이프라인에 최적. `Close()`를 통한 명시적 종료를 지원하여, producer가 먼저 종료되는 상황에서도 consumer의 blocking `Pop()`이 무한 대기에 빠지지 않도록 함.

---

## 🧩 클래스 간 공통 API 요약

네 클래스 모두 다음 계약을 공유합니다:

| 항목 | 공통 계약 |
|------|-----------|
| `TryPush` / `TryPop` | 논블로킹, `bool` 반환 |
| `Push` / `Pop` | 블로킹(짧은 스핀 후 대기), `bool` 반환. 종료 상태로 인한 실패를 호출자가 구분 가능 |
| 종료 메서드 | `Close()` (`void`) / `IsClosed()` (`bool`) |
| `SizeApprox()` | 대략적인 현재 크기, 언더플로 방지 클램프 포함 |
| `GetCapacity()` | `constexpr` 최대 용량 |
| 복사/대입 | 모두 `delete` |
| Capacity 제약 | 2 이상, 2의 거듭제곱 (`static_assert`) |

클래스별로 갈라지는 지점은 각 패턴(MPMC/MPSC/SPMC/SPSC)이 활용할 수 있는 동시성 완화(단일 producer 또는 단일 consumer 여부)에 따른 내부 구현(CAS 필요 여부, wait/notify 메커니즘)과, `SPMCLockFreeQueue`만 제공하는 `Reset()`(큐 재사용) 정도입니다.
