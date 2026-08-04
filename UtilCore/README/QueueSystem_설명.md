# 큐 시스템 종합 문서 (설명 + 실전 예제)

## 📌 개요
이 문서는 멀티스레드 환경에서 사용되는 다양한 **큐(queue) 구현체**를 분석하고, 각 클래스의 **목적, 구현 방식, 장점, 사용 예시, 실전 예제 코드**를 제공합니다. 각 큐는 특정한 **프로듀서-컨슈머 패턴**과 성능 요구사항에 맞게 최적화되어 있습니다.

---

## ⏳ CDelayedTaskQueue

- **목적**: 특정 시점 이후에 작업을 실행하도록 예약.
- **구현 방식**:
  - `std::priority_queue`(최소 힙)으로 실행 시각 정렬.
  - `std::condition_variable`로 효율적 대기.
  - `Stop()` 호출 시 즉시 종료.
- **장점**:
  - 정밀한 시간 기반 실행.
  - 네트워크, 게임 서버, 스케줄러에 적합.
- **사용 예시**:
  - 네트워크 패킷 재전송.
  - 게임 서버 버프/디버프 만료.
  - 푸시 알림 예약 발송.
- **실전 예제**:

```cpp
CDelayedTaskQueue queue;

// 2초 뒤에 실행될 작업 예약
queue.Reserve(2000, [] {
    std::cout << "2초 뒤 실행!" << std::endl;
});

// 별도 스레드에서 실행 루프 시작
std::thread worker([&queue]() {
    queue.ProcessExpiredTasks();
});

// 5초 뒤 큐 정지
std::this_thread::sleep_for(std::chrono::seconds(5));
queue.Stop();
worker.join();
```

---

## 🔀 CDoubleBufferQueue

- **목적**: 초고속 배치 처리용 큐.
- **구현 방식**:
  - 두 개의 버퍼를 원자적으로 스왑.
  - 프로듀서는 한쪽 버퍼에 기록, 컨슈머는 다른 버퍼를 한 번에 처리.
  - 힙 메모리 재할당 최소화.
- **장점**:
  - **제로-할당**으로 성능 극대화.
  - 락 경합 최소화.
- **사용 예시**:
  - 멀티스레드 로깅 시스템.
  - 초당 수만 건의 통계 수집.
  - DB 비동기 결과 처리.
- **실전 예제**:

```cpp
CDoubleBufferQueue<std::string> logQueue;

// 여러 스레드에서 로그 푸시
logQueue.Push("로그 메시지 1");
logQueue.Push("로그 메시지 2");

// 컨슈머 스레드에서 배치 처리
std::vector<std::string> batch = logQueue.Swap();
for (auto& msg : batch) {
    std::cout << "처리: " << msg << std::endl;
}
```

---

## 🔒 CSpinLockQueue

- **목적**: 단순하고 직관적인 스레드 세이프 큐.
- **구현 방식**:
  - 스핀락으로 Push/Pop 보호.
  - 아토믹 카운터로 크기 조회 시 락 불필요.
- **장점**:
  - 직관적인 인터페이스.
  - **MPMC** 환경 지원.
- **사용 예시**:
  - 글로벌 작업 큐.
  - 멀티스레드 JobQueue 관리.
- **실전 예제**:

```cpp
CSpinLockQueue<int> jobQueue;

// 프로듀서 스레드
jobQueue.Push(42);
jobQueue.Push(100);

// 컨슈머 스레드
int job = jobQueue.Pop();
std::cout << "처리된 작업: " << job << std::endl;

// 모든 작업 한 번에 꺼내기
CVector<int> jobs;
jobQueue.PopAll(jobs);
for (auto& j : jobs) {
    std::cout << "배치 처리: " << j << std::endl;
}
```

---

## ⛔ CBlockingTaskQueue

- **목적**: 블로킹 Pop을 지원하는 큐.
- **구현 방식**:
  - `std::mutex` + `std::condition_variable` 사용.
  - `Pop()`은 데이터가 없으면 대기.
  - `SetProducerDone()`으로 종료 신호 전달.
- **장점**:
  - 안전한 블로킹 처리.
  - graceful shutdown 지원.
- **사용 예시**:
  - 백그라운드 워커 스레드.
  - 생산자-소비자 파이프라인.
  - 종료 시 안전한 자원 정리.
- **실전 예제**:

```cpp
CBlockingTaskQueue<std::string> taskQueue;

// 프로듀서 스레드
std::thread producer([&taskQueue]() {
    taskQueue.Push("작업 A");
    taskQueue.Push("작업 B");
    taskQueue.SetProducerDone();
});

// 컨슈머 스레드
std::thread consumer([&taskQueue]() {
    std::string task;
    while (taskQueue.Pop(task)) {
        std::cout << "처리: " << task << std::endl;
    }
    std::cout << "생산자 종료, 소비자도 종료" << std::endl;
});

producer.join();
consumer.join();
```

---

## 📦 CChunkedSwapQueue

- **목적**: 청킹 단위로 데이터를 스왑하여 부하 제어.
- **구현 방식**:
  - `SwapChunk()`로 지정된 개수만큼만 이동.
  - 아토믹 카운터로 실시간 크기 모니터링.
- **장점**:
  - 폭주 트래픽 시 소비자 과부하 방지.
  - 부하 분산 및 프레임 드랍 방지.
- **사용 예시**:
  - IOCP 서버 패킷 전달.
  - 대량 요청 처리 시 부하 제어.
  - 실시간 시스템의 안정성 확보.
- **실전 예제**:

```cpp
CChunkedSwapQueue<int> packetQueue;

// 프로듀서: 대량 패킷 삽입
for (int i = 0; i < 100; ++i) {
    packetQueue.Push(i);
}

// 컨슈머: 한 번에 10개씩만 처리
std::queue<int> outQueue;
packetQueue.SwapChunk(outQueue, 10);

while (!outQueue.empty()) {
    std::cout << "처리된 패킷: " << outQueue.front() << std::endl;
    outQueue.pop();
}
```

---

## ⚖️ 비교 요약

| 클래스 | 패턴 | 목적 | 구현 방식 | 장점 | 사용 예시 |
|--------|------|------|-----------|------|-----------|
| **DelayedTaskQueue** | SPMC | 시간 기반 실행 | priority_queue + CV | 정밀한 타이머 | 타임아웃, 쿨다운 |
| **DoubleBufferQueue** | MPSC | 배치 처리 성능 | 더블 버퍼링 | 제로-할당, 고속 | 로깅, 통계 |
| **SpinLockQueue** | MPMC | 범용 큐 | 스핀락 + 아토믹 | 직관적, 빠른 조회 | 작업 분배 |
| **BlockingTaskQueue** | MPMC | 블로킹 대기 | Mutex + CV | 안전한 종료 | 워커 파이프라인 |
| **ChunkedSwapQueue** | SPMC | 부하 제어 | 청킹 스왑 | 부하 분산 | IOCP, 폭주 트래픽 |

---

## ⚖️ 특징별 비교 표

| 특징 | **DelayedTaskQueue** | **DoubleBufferQueue** | **SpinLockQueue** | **BlockingTaskQueue** | **ChunkedSwapQueue** |
|------|----------------------|-----------------------|-------------------|-----------------------|----------------------|
| **패턴** | SPMC | MPSC | MPMC | MPMC | SPMC |
| **중점** | 시간 기반 실행 | 배치 처리 성능 | 범용 큐 | 블로킹 대기 | 부하 제어 |
| **락 방식** | Mutex + CV | Spinlock + Atomic | Spinlock | Mutex + CV | Spinlock |
| **종료 처리** | Stop 플래그 | 없음 | Clear | ProducerDone | 없음 |
| **적합한 환경** | 타이머, 스케줄링 | 로깅, 통계 | 작업 큐 | 워커 파이프라인 | IOCP, 폭주 트래픽 |

---

## 🚀 설계 인사이트

- **CDelayedTaskQueue** → 시간 정밀성이 중요한 경우.
- **CDoubleBufferQueue** → 초고속 배치 처리에 최적.
- **CSpinLockQueue** → 단순하고 직관적인 범용 큐.
- **CBlockingTaskQueue** → 블로킹 동작과 종료 제어가 필요한 경우.
- **CChunkedSwapQueue** → 폭주 트래픽 제어 및 부하 분산에 적합.
