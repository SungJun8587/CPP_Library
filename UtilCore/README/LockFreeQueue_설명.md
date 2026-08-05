# 큐 시스템 종합 문서 (Lock-Free 큐)

## 📌 개요
이 문서는 **Lock-Free 큐 구현체**들을 분석합니다. 각 클래스의 **목적, 구현 방식, 장점, 사용 예시, 게임 서버 적용, 실전 예제 코드**를 제공합니다. 모든 예제는 종료 처리까지 포함합니다.

---

## 🔀 MPMCLockFreeQueue
- **목적**: 다중 프로듀서-다중 컨슈머(MPMC) 환경에서 락 없는 범용 큐 제공  
- **구현 방식**: Dmitry Vyukov 알고리즘 기반, 슬롯 단위 CAS로 push/pop 소유권 조정  
- **장점**:  
  - 전역 락 없음 → 높은 동시성  
  - 범용적이며 가장 일반적인 lock-free 큐  
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

// 종료 처리: 소멸자에서 자동 비움
```

---

## 🔀 MPSCLockFreeQueue
- **목적**: 다중 프로듀서-단일 컨슈머(MPSC) 환경에서 락 없는 큐 제공  
- **구현 방식**: producer는 CAS 경쟁, consumer는 단순 인덱스 증가  
- **장점**:  
  - 단일 소비자 환경에서 효율적  
  - producer는 경쟁하지만 consumer는 빠름  
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

// 종료 처리: 소멸자에서 자동 비움
```

---

## 🔀 SPMCLockFreeQueue
- **목적**: 단일 프로듀서-다중 컨슈머(SPMC) 환경에서 락 없는 큐 제공  
- **구현 방식**: producer는 단순 인덱스 증가, consumer는 CAS 경쟁  
- **장점**:  
  - producer 경로 단순화 → 성능 향상  
  - consumer만 경쟁  
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

// 종료 처리: 소멸자에서 자동 비움
```

---

## 🔀 SPSCLockFreeQueue
- **목적**: 단일 프로듀서-단일 컨슈머(SPSC) 환경에서 락 없는 큐 제공  
- **구현 방식**: CAS 불필요, 단순 인덱스 증가만으로 동작  
- **장점**:  
  - 가장 빠른 lock-free 큐  
  - 구조 단순, 구현 간결  
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

// 종료 처리: 소멸자에서 자동 비움
```

---

# ⚖️ 큐 클래스 통합 비교 표

| 특징 | **MPMCLockFreeQueue** | **MPSCLockFreeQueue** | **SPMCLockFreeQueue** | **SPSCLockFreeQueue** |
|------|--------------------------|--------------------------|--------------------------|--------------------------|
| **패턴** | MPMC | MPSC | SPMC | SPSC |
| **목적** | 범용 다중 producer/consumer | 다중 producer, 단일 consumer | 단일 producer, 다중 consumer | 단일 producer, 단일 consumer |
| **구현 방식** | Vyukov 알고리즘, CAS 경쟁 | producer CAS, consumer 단순 인덱스 | producer 단순 인덱스, consumer CAS | CAS 불필요, 인덱스 증가만 |
| **중점** | 범용성, 높은 동시성 | 단일 consumer 효율성 | producer 성능 최적화 | 최고 성능, 저지연 |
| **락 방식** | Lock-Free + CAS | Lock-Free + CAS | Lock-Free + CAS | Lock-Free (CAS 없음) |
| **종료 처리** | 소멸자에서 자동 비움 | 소멸자에서 자동 비움 | 소멸자에서 자동 비움 | 소멸자에서 자동 비움 |
| **장점** | 범용, 높은 동시성 | 단일 consumer 효율적 | producer 성능 향상 | 가장 빠름, 단순 |
| **사용 예시** | 네트워크 패킷 처리, 스레드 풀 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 파이프라인, 스트리밍 |
| **게임 서버 적용** | 멀티스레드 패킷 처리 | 로그 수집, 이벤트 큐 | 이벤트 브로드캐스트 | 네트워크 수신 파이프라인 |

---

## 🚀 설계 인사이트
- **MPMCLockFreeQueue** → 가장 범용적이며, 다중 producer/consumer 환경에서 안정적인 성능을 제공.  
- **MPSCLockFreeQueue** → 단일 consumer 환경에서 효율적, 로그 수집이나 이벤트 큐에 적합.  
- **SPMCLockFreeQueue** → 단일 producer 환경에서 성능 최적화, 이벤트 브로드캐스트에 적합.  
- **SPSCLockFreeQueue** → 가장 단순하고 빠른 큐, 저지연 파이프라인에 최적.  
