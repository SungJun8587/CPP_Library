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
- **사용 예시**:  
  - 네트워크 패킷 재전송  
  - 게임 서버 버프/디버프 만료  
  - 푸시 알림 예약 발송  
- **게임 서버 적용**:  
  - 세션 idle timeout  
  - 스킬 쿨다운 종료  
  - 예약 이벤트 처리  
- **실전 예제**:
```cpp
CDelayedTaskQueue queue;
queue.Reserve(2000, [] { std::cout << "2초 뒤 실행!" << std::endl; });

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

int job = jobQueue.Pop();
std::cout << "처리된 작업: " << job << std::endl;

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
- **사용 예시**:  
  - 백그라운드 워커 스레드  
  - 생산자-소비자 파이프라인  
- **게임 서버 적용**:  
  - 워커 스레드 파이프라인  
  - 종료 제어가 필요한 작업 처리  
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
- **사용 예시**:  
  - IOCP 서버 패킷 전달  
  - 대량 요청 처리  
- **게임 서버 적용**:  
  - 네트워크 패킷 전달(IOCP)  
  - 대량 요청 처리  
- **실전 예제**:
```cpp
CChunkedSwapQueue<int> packetQueue;
for (int i = 0; i < 100; ++i) {
    packetQueue.Push(i);
}

std::queue<int> outQueue;
packetQueue.SwapChunk(outQueue, 10);

while (!outQueue.empty()) {
    std::cout << "처리된 패킷: " << outQueue.front() << std::endl;
    outQueue.pop();
}

packetQueue.Stop(); // 종료 처리
```

---

# ⚖️ 큐 클래스 통합 비교 표

| 특징 | **CDelayedTaskQueue** | **CDoubleBufferQueue** | **CSpinLockQueue** | **CBlockingTaskQueue** | **CChunkedSwapQueue** |
|------|--------------------------|--------------------------|--------------------------|--------------------------|--------------------------|
| **패턴** | SPMC | MPSC | MPMC | MPMC | SPMC |
| **목적** | 특정 시점 이후 작업 실행 | 초고속 배치 처리 | 범용 작업 큐 | 블로킹 대기 지원 | 부하 제어 및 청킹 처리 |
| **구현 방식** | priority_queue + condition_variable | 더블 버퍼링, 원자적 스왑 | 스핀락 + 아토믹 카운터 | mutex + condition_variable | 청킹 스왑 + 아토믹 카운터 |
| **중점** | 시간 기반 실행 | 배치 처리 성능 | 직관적 범용 큐 | 안전한 블로킹 대기 | 부하 제어 및 분산 |
| **락 방식** | Mutex + Condition Variable | Spinlock + Atomic | Spinlock | Mutex + Condition Variable | Spinlock |
| **종료 처리** | Stop 플래그, 대기 스레드 깨움 | Stop 플래그, 데이터 유입 차단 | Stop 플래그, Push 차단 | Stop 플래그, 모든 스레드 깨움 | Stop 플래그, Push 차단 |
| **장점** | 정밀 타이머, 효율적 대기 | 제로-할당, 락 경합 최소화 | 직관적, 빠른 상태 확인 | 안전한 종료, graceful shutdown | 폭주 트래픽 제어 |
| **사용 예시** | 네트워크 재전송, 게임 이벤트 | 로그/통계 수집, DB 결과 | 글로벌 JobQueue | 워커 파이프라인 | IOCP 패킷 전달 |
| **게임 서버 적용** | 타이머/스케줄링 | 로그/통계 수집 | 작업 분배 | 종료 제어 | 실시간 부하 제어 |

---

## 🚀 설계 인사이트
- **CDelayedTaskQueue** → 시간 기반 이벤트 처리에 최적  
- **CDoubleBufferQueue** → 초고속 로그/통계 수집에 적합  
- **CSpinLockQueue** → 단순하고 직관적인 범용 작업 큐  
- **CBlockingTaskQueue** → 블로킹 대기와 종료 제어가 필요한 파이프라인에 적합  
- **CChunkedSwapQueue** → 폭주 트래픽 제어 및 실시간 안정성 확보에 최적  
