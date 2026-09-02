# 큐 시스템 종합 문서 (설명 + 실전 예제)

## 📌 개요
멀티스레드 환경에서 사용되는 다양한 **큐(queue) 구현체**를 분석합니다. 각 클래스의 **목적, 구현 방식, 장점, 사용 예시, 게임 서버 적용, 실전 예제 코드**를 제공하며, 종료 처리까지 포함된 예제를 명시합니다.

---

## ⏳ CDelayedTaskQueue
- **목적**: 특정 시점 이후 작업 예약  
- **구현 방식**: `priority_queue`(최소 힙) + `condition_variable`  
- **장점**:  
  - 정밀한 시간 기반 실행  
  - 효율적 대기
- **성능**: 대기 효율성이 매우 높음. `condition_variable` 기반으로 CPU 낭비 없이 정확한 시점에 작업 실행.  
- **효율성**: 작업 수가 많아질수록 힙 연산 비용 증가 → 대규모 예약 작업에서는 성능 저하 가능.    
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `_queue` | `std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<DelayedTask>>` | 실행 시각 기준 최소 힙 |
| `_mutex` | `std::mutex` | `_queue`/`_stopped` 보호용 뮤텍스 |
| `_cv` | `std::condition_variable` | 타이머 대기용 조건 변수 |
| `_stopped` | `bool` | `ProcessExpiredTasks()` 루프 정지 플래그 (`_mutex`로 보호) |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| `Reserve` | `bool Reserve(std::chrono::milliseconds delay, F&& task)` | 지정 시간 뒤 실행할 작업 예약. `Stop()` 이후면 `false` |
| `ProcessExpiredTasks` | `void ProcessExpiredTasks()` | 만료된 작업을 순차적으로 꺼내 실행(블로킹 루프) |
| `Stop` | `void Stop()` | 정지 플래그 설정 후 대기 스레드 모두 깨움 |

- **사용 예시**:  
  - 네트워크 패킷 재전송  
  - 게임 서버 버프/디버프 만료  
  - 푸시 알림 예약 발송  
- **게임 서버 적용**:  
  - 세션 idle timeout  
  - 스킬 쿨다운 종료  
  - 예약 이벤트 처리  
- **API 참고**: `Reserve()`는 지연 시간을 `std::chrono::milliseconds`로 받고, Stop() 이후 거부된 예약과 정상 예약을 구분할 수 있도록 `bool`을 반환합니다.  
- **실전 예제**:
```cpp
CDelayedTaskQueue queue;
queue.Reserve(std::chrono::milliseconds(2000), [] { std::cout << "2초 뒤 실행!" << std::endl; });

std::thread worker([&queue]() {
    queue.ProcessExpiredTasks();
});

std::this_thread::sleep_for(std::chrono::seconds(5));
queue.Stop(); // 종료 처리
worker.join();
```

---

## 🔀 CDoubleBufferQueue
- **목적**: 초고속 배치 처리  
- **구현 방식**: 더블 버퍼링, 원자적 스왑, InFlightGuard로 producer 안전성 보장  
- **장점**:  
  - 제로-할당으로 성능 극대화  
  - 락 경합 최소화
- **성능**: 쓰기/읽기 성능 모두 극대화. 프로듀서가 동시에 데이터를 밀어넣어도 충돌 최소화.  
- **효율성**: 더블 버퍼링으로 메모리 재할당 비용 제거 → 초고속 배치 처리에 최적.  
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `m_bufferLock[2]` | `PLock[2]` | 두 버퍼 각각의 플랫폼 통합 락 |
| `m_buffer[2]` | `CVector<T>[2]` | 더블 버퍼(실제 데이터 저장소) |
| `m_writeIdx` | `std::atomic<int>` | 현재 활성(쓰기) 버퍼 인덱스(0 또는 1) |
| `m_isSwapping` | `std::atomic<bool>` | Consumer 스왑 중 여부(다중 Consumer 진입 차단용) |
| `m_inFlight[2]` | `std::atomic<int>[2]` | 각 버퍼에 기록 중인 Producer 수 |
| `m_stopped` | `std::atomic<bool>` | 종료 플래그 |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| `Push` | `void Push(const T&)` / `void Push(T&&)` | 활성 버퍼에 데이터 삽입(복사/이동) |
| `Emplace` | `template<typename... Args> void Emplace(Args&&...)` | 활성 버퍼에 데이터 제자리 생성 |
| `Swap` | `CVector<T> Swap()` | 버퍼 스왑 후 이전 버퍼 데이터를 새 벡터로 반환 |
| `SwapInto` | `void SwapInto(CVector<T>& out)` | 버퍼 스왑 후 이전 버퍼 데이터를 `out`에 옮겨 담음 |
| `ApproxSize` | `size_t ApproxSize() const` | 양쪽 버퍼 크기 합의 근사치(디버깅/단독 스레드용) |
| `Stop` | `void Stop()` | 신규 데이터 유입 차단 |

- **사용 예시**:  
  - 멀티스레드 로깅  
  - 초당 수만 건 통계 수집  
  - DB 비동기 결과 처리  
- **게임 서버 적용**:  
  - 로그 수집  
  - 통계 집계  
  - 비동기 결과 수거  
- **실전 예제**:
```cpp
CDoubleBufferQueue<std::string> logQueue;
logQueue.Push("로그 메시지 1");
logQueue.Push("로그 메시지 2");

CVector<std::string> batch = logQueue.Swap();
for (auto& msg : batch) {
    std::cout << "처리: " << msg << std::endl;
}

logQueue.Stop(); // 종료 처리
```

---

## 🔒 CSpinLockQueue
- **목적**: 단순하고 직관적인 스레드 세이프 큐  
- **구현 방식**: 스핀락 기반 Push/Pop, 아토믹 카운터로 크기 조회  
- **장점**:  
  - 직관적 인터페이스  
  - MPMC 환경 지원
- **성능**: Push/Pop 속도 빠름. 단순 구조로 직관적.  
- **효율성**: 락 경합이 심한 경우 CPU 사용량 증가 가능. 하지만 Empty/Size 조회는 락 없이 가능해 상태 확인은 효율적.    
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `_lock` | `PLock` | 플랫폼 통합 단독 락 객체(Windows는 SRWLock, 그 외는 스핀락) |
| `_items` | `CQueue<T>` | 커스텀 큐 기반 내부 저장소 |
| `_size` | `std::atomic<int64>` | 락 없는 Empty/Size 조회용 카운터 |
| `_stopped` | `std::atomic<bool>` | 종료 플래그 |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| `Push` | `bool Push(T item)` | 데이터 삽입. `Stop()` 이후면 `false` |
| `TryPop` | `[[nodiscard]] bool TryPop(T& outItem)` | 데이터 하나 꺼냄. 비어있으면 `false` |
| `PopAll` | `void PopAll(CVector<T>& items)` | 전체 데이터를 한 번에 꺼내 `items`에 담음 |
| `Clear` | `void Clear()` | 큐를 완전히 비움 |
| `Empty` | `bool Empty() const` | 비어있는지 여부(락 없이 조회) |
| `Size` | `size_t Size() const` | 현재 아이템 개수(락 없이 조회) |
| `Stop` | `void Stop()` | 정지 상태로 전환, 이후 Push 차단 |

- **사용 예시**:  
  - 글로벌 작업 큐  
  - 멀티스레드 JobQueue 관리  
- **게임 서버 적용**:  
  - 멀티스레드 작업 분배  
  - 글로벌 JobQueue  
- **실전 예제**:
```cpp
CSpinLockQueue<int> jobQueue;
jobQueue.Push(42);
jobQueue.Push(100);

int job;
if (jobQueue.TryPop(job)) {
    std::cout << "처리된 작업: " << job << std::endl;
}

CVector<int> jobs;
jobQueue.PopAll(jobs);
for (auto& j : jobs) {
    std::cout << "배치 처리: " << j << std::endl;
}

jobQueue.Stop(); // 종료 처리
```

---

## ⛔ CBlockingTaskQueue
- **목적**: 블로킹 Pop 지원  
- **구현 방식**: `mutex` + `condition_variable`  
- **장점**:  
  - 안전한 블로킹 처리  
  - graceful shutdown 지원  
- **성능**: 소비자가 데이터 없을 때 CPU 낭비 없이 대기 가능. 안정적이나 초고속 환경에는 부적합.  
- **효율성**: 종료 제어(graceful shutdown) 지원으로 안정성이 높음. 
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `_queue` | `CQueue<T>` | 내부 큐 컨테이너 |
| `_mutex` | `std::mutex` | 동기화용 뮤텍스 |
| `_cv` | `std::condition_variable` | 소비자 대기 제어용 조건 변수 |
| `_producerDone` | `bool` | 프로듀서 종료 플래그(`_mutex`로 보호) |
| `_stopped` | `bool` | 강제 종료 플래그(`_mutex`로 보호) |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| `Push` | `bool Push(T item)` | 데이터 하나 삽입. `Stop()`/`SetProducerDone()` 이후면 `false` |
| `PushBatch` | `bool PushBatch(CVector<T>& items)` | 여러 데이터 일괄 삽입(성공 시 `items` 비워짐) |
| `Pop` | `bool Pop(T& out)` | 데이터 없으면 블로킹 대기 후 하나 꺼냄 |
| `SetProducerDone` | `void SetProducerDone()` | 프로듀서 종료 신호(남은 데이터만 마저 처리) |
| `Stop` | `void Stop()` | 강제 종료, 대기 중인 모든 스레드 깨움 |

- **사용 예시**:  
  - 백그라운드 워커 스레드  
  - 생산자-소비자 파이프라인  
- **게임 서버 적용**:  
  - 워커 스레드 파이프라인  
  - 종료 제어가 필요한 작업 처리  
- **API 참고**: `T`는 nothrow move constructible이어야 합니다 — PushBatch()가 Stop()/SetProducerDone() 이후 거부될 때 옮겨둔 원소를 items로 되돌리는 롤백 경로가 이를 전제로 하며, 컴파일 타임 static_assert로 강제됩니다.  
- **실전 예제**:
```cpp
CBlockingTaskQueue<std::string> taskQueue;

std::thread producer([&taskQueue]() {
    taskQueue.Push("작업 A");
    taskQueue.Push("작업 B");
    taskQueue.SetProducerDone();
});

std::thread consumer([&taskQueue]() {
    std::string task;
    while (taskQueue.Pop(task)) {
        std::cout << "처리: " << task << std::endl;
    }
    std::cout << "생산자 종료, 소비자도 종료" << std::endl;
});

producer.join();
taskQueue.Stop(); // 종료 처리
consumer.join();
```

---

## 📦 CChunkedSwapQueue
- **목적**: 청킹 단위 스왑으로 부하 제어  
- **구현 방식**: `SwapChunk()`로 지정된 개수만큼 이동, 아토믹 카운터로 크기 추적  
- **장점**:  
  - 폭주 트래픽 시 소비자 과부하 방지  
  - 부하 분산 및 프레임 드랍 방지
- **성능**: 소비자가 한 번에 가져가는 양을 제한 → 프레임 드랍 방지.  
- **효율성**: 단일 producer 환경에서 최적. 폭주 트래픽 제어에 강점.      
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `_lock` | `PLock` | 플랫폼 통합 단독 락 객체 |
| `_inQueue` | `CQueue<T>` | 내부 입력을 받는 큐 버퍼 |
| `_size` | `std::atomic<int64>` | 락 경합 없는 빠른 크기 조회용 카운터 |
| `_stopped` | `std::atomic<bool>` | 정지 플래그(Stop/Start는 `_lock`으로 직렬화, 조회는 lock-free) |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| `Push` | `bool Push(T item)` | 단일 아이템 삽입. 정지 상태면 `false` |
| `PushAndGetSize` | `int64 PushAndGetSize(T item)` | 삽입 후 갱신된 전체 크기 반환(정지 상태면 -1) |
| `PushBatch` | `bool PushBatch(CVector<T>& items)` | 여러 아이템 일괄 삽입(성공 시에만 `items` 비워짐) |
| `Swap` | `void Swap(CQueue<T>& outQueue)` | 입력 큐 전체를 `outQueue`로 스왑(이동) |
| `SwapChunk` | `void SwapChunk(CQueue<T>& outQueue, size_t maxCount)` | 최대 `maxCount`개만 `outQueue`로 이동(청킹) |
| `IsEmpty` | `bool IsEmpty() const` | 비어있는지 여부 |
| `GetSize` | `int64 GetSize() const` | 현재 큐 크기 |
| `Stop` | `void Stop()` | 정지 상태로 전환, 이후 Push 차단 |
| `Start` | `void Start()` | 정지 상태 해제(재사용) |
| `IsStopped` | `bool IsStopped() const` | 현재 정지 상태 여부 |

- **사용 예시**:  
  - IOCP 서버 패킷 전달  
  - 대량 요청 처리  
- **게임 서버 적용**:  
  - 네트워크 패킷 전달(IOCP)  
  - 대량 요청 처리  
- **API 참고**: `PushBatch()`의 무롤백 루프는 할당 실패가 예외를 던지지 않는다는 전제(USE_GPMEMORY 빌드)에서만 안전합니다. 이 전제를 런타임에 검증할 수 없으므로, USE_GPMEMORY가 정의되지 않은 빌드에서 `PushBatch()`를 실제로 호출하면 컴파일 에러로 막히도록 가드되어 있습니다. 단건 `Push()`/`PushAndGetSize()`/`Swap()`/`SwapChunk()`는 이 전제와 무관하여 영향받지 않습니다.  
- **실전 예제**:
```cpp
CChunkedSwapQueue<int> packetQueue;
for (int i = 0; i < 100; ++i) {
    packetQueue.Push(i);
}

CQueue<int> outQueue;
packetQueue.SwapChunk(outQueue, 10);

while (!outQueue.empty()) {
    std::cout << "처리된 패킷: " << outQueue.front() << std::endl;
    outQueue.pop();
}

packetQueue.Stop(); // 종료 처리
```

---

## 🛑 CChunkedBlockingQueue
- **목적**: 청킹 단위 + 블로킹 대기 지원  
- **구현 방식**: `mutex` + `condition_variable` + 청킹 스왑  
- **장점**:  
  - 소비자가 지정된 청크 단위로 안전하게 가져감  
  - 데이터 없을 때 블로킹 대기 → CPU 낭비 없음  
  - 종료 제어(graceful shutdown) 지원  
- **성능**: 소비자가 한 번에 가져가는 양을 제한하면서도 블로킹 대기를 지원 → 안정성과 성능 균형 확보.  
- **효율성**: 폭주 트래픽 제어 + 안정적 종료 제어가 동시에 가능.  
- **멤버 변수**:

| 변수명 | 타입 | 설명 |
|--------|------|------|
| `_inQueue` | `CQueue<T>` | 내부 데이터를 보관하는 큐 버퍼 |
| `_mutex` | `std::mutex` | 동기화용 뮤텍스 |
| `_cv` | `std::condition_variable` | 소비자 대기/통보용 조건 변수 |
| `_notFullCv` | `std::condition_variable` | 생산자 백프레셔 대기용 조건 변수(단일 프로듀서 전제) |
| `_producerDone` | `bool` | 프로듀서 완료 플래그(`_mutex`로 보호) |
| `_stopped` | `bool` | 강제 정지 플래그(`_mutex`로 보호) |
| `_maxQueueSize` | `size_t` | 큐 최대 크기(0 = 무제한) |

- **멤버 함수**:

| 함수명 | 시그니처 | 설명 |
|--------|----------|------|
| (생성자) | `explicit CChunkedBlockingQueue(size_t maxQueueSize = 0)` | 최대 큐 크기 지정(백프레셔용) |
| `Push` | `bool Push(T item)` | 단일 아이템 삽입. `maxQueueSize` 초과 시 공간이 생길 때까지 블로킹 |
| `PushBatch` | `bool PushBatch(CVector<T>& items)` | 배치 단위 삽입(원자성 보장, 단일 프로듀서 전제) |
| `PopChunk` | `bool PopChunk(CQueue<T>& outQueue, size_t maxCount)` | 최대 `maxCount`개를 꺼내 `outQueue`로 이동, 없으면 블로킹 대기 |
| `SetProducerDone` | `void SetProducerDone()` | 프로듀서 완료 신호, 모든 소비자 스레드 깨움 |
| `Stop` | `void Stop()` | 강제 정지, 모든 소비자/생산자 스레드 해제 |

- **사용 예시**:  
  - 대량 요청 처리 시 안정적 워커 스레드 운영  
  - IOCP 서버 패킷 전달 시 청킹 + 블로킹 대기  
- **게임 서버 적용**:  
  - 네트워크 패킷 전달(IOCP)  
  - 대량 요청 처리 시 안정적 워커 스레드 운영  
  - 종료 제어가 필요한 네트워크 파이프라인  
- **주의**: `PopChunk()`의 `_notFullCv.notify_one()`은 단일 프로듀서(SPMC) 가정 하에서만 안전합니다. 멀티 프로듀서로 확장할 계획이 있다면 `notify_all`로 전환을 검토해야 합니다.  
- **실전 예제**:
```cpp
CChunkedBlockingQueue<int> packetQueue;

std::thread producer([&packetQueue]() {
    for (int i = 0; i < 50; ++i) {
        packetQueue.Push(i);
    }
    packetQueue.SetProducerDone();
});

std::thread consumer([&packetQueue]() {
    CQueue<int> outQueue;
    while (packetQueue.PopChunk(outQueue, 10)) {
        while (!outQueue.empty()) {
            std::cout << "처리된 패킷: " << outQueue.front() << std::endl;
            outQueue.pop();
        }
    }
    std::cout << "생산자 종료, 소비자도 종료" << std::endl;
});

producer.join();
packetQueue.Stop(); // 종료 처리
consumer.join();
```

---

# ⚖️ 큐 클래스 통합 비교 표 (업데이트)

| 특징 | **CDelayedTaskQueue** | **CDoubleBufferQueue** | **CSpinLockQueue** | **CBlockingTaskQueue** | **CChunkedSwapQueue** | **CChunkedBlockingQueue** |
|------|--------------------------|--------------------------|--------------------------|--------------------------|--------------------------|--------------------------|
| **패턴** | MPMC | MPSC | MPMC | MPMC | MPMC | SPMC |
| **목적** | 특정 시점 이후 작업 실행 | 초고속 배치 처리 | 범용 작업 큐 | 블로킹 대기 지원 | 부하 제어 및 청킹 처리 | 청킹 + 블로킹 대기 |
| **구현 방식** | priority_queue + condition_variable | 더블 버퍼링, 원자적 스왑 | 스핀락 + 아토믹 카운터 | mutex + condition_variable | 청킹 스왑 + 아토믹 카운터 | mutex + condition_variable + 청킹 |
| **중점** | 시간 기반 실행 | 배치 처리 성능 | 직관적 범용 큐 | 안전한 블로킹 대기 | 부하 제어 및 분산 | 안정적 청킹 + 블로킹 |
| **락 방식** | Mutex + Condition Variable | Spinlock + Atomic | Spinlock | Mutex + Condition Variable | Spinlock | Mutex + Condition Variable |
| **종료 처리** | Stop 플래그, 대기 스레드 깨움 | Stop 플래그, 데이터 유입 차단 | Stop 플래그, Push 차단 | Stop 플래그, 모든 스레드 깨움 | Stop 플래그, Push 차단 | Stop 플래그, 모든 스레드 깨움 |
| **장점** | 정밀 타이머, 효율적 대기 | 제로-할당, 락 경합 최소화 | 직관적, 빠른 상태 확인 | 안전한 종료, graceful shutdown | 폭주 트래픽 제어 | 폭주 트래픽 제어 + 종료 안정성 |
| **사용 예시** | 네트워크 재전송, 게임 이벤트 | 로그/통계 수집, DB 결과 | 글로벌 JobQueue | 워커 파이프라인 | IOCP 패킷 전달 | IOCP 패킷 전달 + 안정적 종료 |
| **게임 서버 적용** | 타이머/스케줄링 | 로그/통계 수집 | 작업 분배 | 종료 제어 | 실시간 부하 제어 | 실시간 부하 제어 + 종료 제어 |

---

## 🚀 설계 인사이트 (업데이트)
- **CDelayedTaskQueue** → 시간 기반 이벤트 처리에 최적  
- **CDoubleBufferQueue** → 초고속 로그/통계 수집에 적합  
- **CSpinLockQueue** → 단순하고 직관적인 범용 작업 큐  
- **CBlockingTaskQueue** → 블로킹 대기와 종료 제어가 필요한 파이프라인에 적합  
- **CChunkedSwapQueue** → 폭주 트래픽 제어 및 실시간 안정성 확보에 최적  
- **CChunkedBlockingQueue** → 폭주 트래픽 제어 + 안정적 종료 제어까지 필요한 환경에 최적  
