# Windows RIO(Registered I/O) 완벽 정리

## 1. 들어가며

IOCP는 이미 Windows에서 대규모 동시 접속을 처리하는 사실상 표준 기술이다. 하지만 초당 수십만~수백만 건의 I/O를 처리해야 하는 초고빈도 환경(HFT, 대규모 UDP 실시간 트래픽 등)에서는 IOCP조차 병목이 될 수 있다. RIO는 이 지점을 정면으로 겨냥해 Windows 8 / Server 2012부터 Winsock 확장(Winsock Kernel extension)으로 추가된 API다. 이 글은 RIO가 어떤 문제를 해결하는지, 핵심 구성 요소가 무엇인지, 그리고 IOCP와 어떻게 연결되는지를 정리한다.

---

## 2. IOCP만으로 부족해지는 지점

IOCP 기반 서버에서 `WSASend`/`WSARecv`를 호출할 때마다 다음 비용이 반복적으로 발생한다.

1. **시스템 콜(유저모드 ↔ 커널모드 전환) 비용**: 호출 1건마다 모드 전환이 발생한다. 전환 자체는 마이크로초 단위지만, 초당 수십만 건이 쌓이면 무시할 수 없는 CPU 비용이 된다.
2. **버퍼 검증/락(lock) 비용**: 커널은 매 I/O마다 유저모드 버퍼가 유효한 메모리인지 검증하고, I/O 도중 페이징되지 않도록 락을 건다. 이 과정이 호출마다 반복된다.
3. **완료 통지의 개별 처리**: `GetQueuedCompletionStatus()`는 한 번에 완료 항목 하나(또는 `GetQueuedCompletionStatusEx`로 여러 개)를 받지만, 여전히 통지 자체의 오버헤드가 호출 단위로 발생한다.

RIO는 이 세 가지를 각각 **배치 제출**, **버퍼 사전 등록**, **배치 완료 수거**로 줄인다.

---

## 3. RIO의 핵심 구성 요소

### 3.1 Registered Buffer — 버퍼를 미리 등록해 검증 비용을 없앤다

```cpp
char* pBuffer = (char*)VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
RIO_BUFFERID bufferId = g_RIORegisterBuffer(pBuffer, (DWORD)bufferSize);
```

한 번 등록해두면, 이후 이 메모리 영역을 사용하는 모든 I/O는 매번 커널에 버퍼 유효성을 검증받을 필요가 없다. 실제 송수신 시에는 등록된 버퍼 안에서 오프셋과 길이만 지정한 `RIO_BUF`를 넘긴다.

```cpp
RIO_BUF rioBuf;
rioBuf.BufferId = bufferId;
rioBuf.Offset   = offsetInBuffer;
rioBuf.Length   = dataLength;
```

큰 메모리 블록 하나를 등록해두고, 그 안을 여러 세션이 슬롯 단위로 나눠 쓰는 방식(슬롯 할당자)이 일반적이다. 슬롯 할당/반납은 애플리케이션이 직접 관리해야 하며, 이 관리 로직의 정확성이 RIO 구현 난이도의 상당 부분을 차지한다.

### 3.2 Request Queue(RQ)와 Completion Queue(CQ)

- **RQ**: 소켓 하나당 하나씩 만든다. `RIOCreateRequestQueue()`로 생성하며, 이 소켓에 대한 송수신 요청이 여기에 쌓인다.
- **CQ**: 완료된 I/O 결과가 모이는 곳. `RIOCreateCompletionQueue()`로 생성하며, **여러 개의 RQ(즉 여러 소켓)가 하나의 CQ를 공유**할 수 있다.

```cpp
RIO_NOTIFICATION_COMPLETION notifyType;
notifyType.Type = RIO_IOCP_COMPLETION;
notifyType.Iocp.IocpHandle = hIOCP;
notifyType.Iocp.CompletionKey = (void*)cqCompletionKey;
notifyType.Iocp.Overlapped = pOverlappedForCq;

RIO_CQ cq = g_RIOCreateCompletionQueue(queueSize, &notifyType);

RIO_RQ rq = g_RIOCreateRequestQueue(
    socket,
    maxOutstandingReceive, maxReceiveDataBuffers,
    maxOutstandingSend, maxSendDataBuffers,
    cq,  // Receive CQ
    cq,  // Send CQ (다르게 분리할 수도 있음)
    (void*)pSessionContext
);
```

N개의 커넥션을 소수의 CQ로 묶어 처리할 수 있다는 점이 확장성의 핵심이다. CQ 하나를 워커 스레드 하나가 전담하는 1:1 구조로 설계하는 경우가 많다.

### 3.3 배치 제출 — RIOSend / RIOReceive와 Notify

```cpp
BOOL RIOSend(
    RIO_RQ SocketQueue,
    PRIO_BUF pData,
    ULONG DataBufferCount,
    DWORD Flags,        // RIO_MSG_DEFER 등
    PVOID RequestContext
);
```

`Flags`에 `RIO_MSG_DEFER`를 주면, 이 요청은 즉시 커널로 제출되지 않고 RQ에 큐잉만 된 상태로 남는다. 여러 건을 이렇게 쌓아둔 뒤,

```cpp
g_RIONotify(cq);
```

를 한 번 호출하면 쌓여있던 요청들이 한꺼번에 커널로 제출된다. 매 패킷마다 시스템 콜을 발생시키는 대신, 여러 패킷을 모아 시스템 콜 1회로 처리하는 것이 핵심 아이디어다.

### 3.4 배치 완료 수거 — RIODequeueCompletion

```cpp
RIORESULT results[256];
ULONG count = g_RIODequeueCompletion(cq, results, 256);

for (ULONG i = 0; i < count; ++i) {
    void* pContext = (void*)results[i].RequestContext;
    ULONG bytesTransferred = results[i].BytesTransferred;
    // RequestContext로 원래 요청(세션, 버퍼 슬롯 등) 식별 후 후처리
}
```

한 번의 호출로 최대 수백 건의 완료 결과를 배열로 받아올 수 있다. IOCP의 `GetQueuedCompletionStatus()`가 기본적으로 한 건씩 처리하는 것과 대비된다(`GetQueuedCompletionStatusEx`도 배치를 지원하지만, RIO는 애초에 이 배치 처리를 중심으로 설계됐다는 차이가 있다).

---

## 4. RIO와 IOCP의 관계

RIO의 CQ는 두 가지 모드로 쓸 수 있다.

- **폴링(Polling) 모드**: `RIONotify()`를 호출하지 않고, 워커 스레드가 주기적으로 `RIODequeueCompletion()`을 직접 호출해 완료 여부를 확인한다. 지연 시간을 최소화할 수 있지만 CPU를 계속 점유(busy-wait)하게 된다.
- **IOCP 연동 모드**: CQ 생성 시 `RIO_NOTIFICATION_COMPLETION`에 IOCP 핸들을 지정해두면, `RIONotify()` 호출 시 해당 CQ에 완료 항목이 쌓였다는 사실이 IOCP 완료 큐에 한 번의 통지로 전달된다. 워커 스레드는 기존 IOCP 워커 루프(`GetQueuedCompletionStatus`)에서 이 신호를 받고, 그 시점에 `RIODequeueCompletion()`으로 실제 완료 목록을 배치로 꺼내온다.

실무에서는 대부분 후자를 쓴다. **RIO는 IOCP의 스케줄링/워커 스레드 모델은 그대로 재사용하면서, 데이터 경로(버퍼 검증, 제출·완료 처리)만 배치화해서 가볍게 만드는 확장**이라고 이해하는 것이 정확하다. RIO가 IOCP를 대체하는 것이 아니다.

전형적인 RIO 워커 루프:

```cpp
void RioWorkerLoop(HANDLE hIOCP, RIO_CQ cq) {
    while (true) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* pOverlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE
        );
        if (!ok) { /* 에러/셧다운 처리 */ continue; }

        RIORESULT results[256];
        ULONG count;
        do {
            count = g_RIODequeueCompletion(cq, results, 256);
            for (ULONG i = 0; i < count; ++i) {
                ProcessRioResult(results[i]);
            }
        } while (count > 0); // 큐가 빌 때까지 배치로 계속 수거

        g_RIONotify(cq); // 다음 완료를 다시 통지받기 위해 재무장
    }
}
```

한 번 통지를 받으면 큐가 완전히 빌 때까지 반복해서 배치 수거를 하고, 마지막에 다시 `RIONotify()`로 "재무장"하는 패턴이 일반적이다. 이 재무장을 빼먹으면 이후 완료가 쌓여도 IOCP로 통지가 오지 않는다.

---

## 5. 성능 이득이 나오는 지점과 나오지 않는 지점

### 5.1 이득이 큰 경우

- 패킷 크기가 작고 초당 처리 건수가 매우 많은 트래픽(UDP 기반 실시간 미디어, 금융 틱 데이터, 대규모 게임의 위치 동기화 패킷 등)
- 이미 IOCP + 최적화된 버퍼 풀로도 시스템 콜 비용이 프로파일링상 유의미한 병목으로 확인된 경우

### 5.2 이득이 크지 않거나 오히려 손해인 경우

- 동접/트래픽 규모가 크지 않은 일반적인 서비스: 오버헤드보다 구현·유지보수 복잡도 비용이 더 크다.
- 패킷 크기가 크고 빈도가 낮은 경우: 시스템 콜 비용 자체가 전체 처리 시간에서 차지하는 비중이 작다.
- TCP 스트림 위주에 순서 보장·재조립 로직이 이미 복잡한 경우: 여기에 버퍼 슬롯 관리까지 얹으면 복잡도가 급격히 늘어난다.

**RIO 도입 전에는 반드시 IOCP만으로 프로파일링해서 병목이 정말 시스템 콜/버퍼 검증에 있는지 확인해야 한다.** 실무에서 병목은 흔히 락 경합, 직렬화 비용, DB 접근 등 애플리케이션 로직에 있는 경우가 더 많다.

---

## 6. 구현 시 특히 주의할 점

RIO는 IOCP보다 직접 관리해야 하는 자원과 생명주기 규칙이 많다. 다음은 실무에서 자주 부딪히는 지점이다.

- **버퍼 슬롯 할당자의 동시성**: 여러 워커 스레드가 동시에 슬롯을 할당/반납하므로, 락프리 자료구조(예: Treiber 스택 기반 free list)나 세밀한 락 설계가 필요하다. ABA 문제, double-free 검출 같은 전형적인 동시성 이슈가 그대로 적용된다.
- **셧다운 순서**: RQ/CQ/등록 버퍼는 서로 의존 관계가 있다. 예를 들어 CQ를 먼저 파괴하고 아직 그 CQ를 참조하는 RQ에 I/O가 남아있으면 문제가 된다. "진행 중인 I/O가 0이 될 때까지 실제 자원 해제를 미룬다"는 원칙(IOCP에서도 동일하게 중요한 원칙)을 버퍼 등록 해제, CQ 파괴, RQ 파괴까지 전 계층에 일관되게 적용해야 한다.
- **소켓을 닫아도 등록된 버퍼/큐는 별도 해제 대상**: `closesocket()`만으로 RIO 관련 리소스가 자동 정리되지 않는다. `RIOCloseCompletionQueue()`, 버퍼 등록 해제(deregister) 등을 명시적으로 순서에 맞게 호출해야 한다.
- **재무장(RIONotify) 누락**: 위 워커 루프 예시처럼, 완료를 다 처리한 뒤 반드시 다시 `RIONotify()`를 호출해야 다음 완료 통지를 받을 수 있다. 이를 빼먹으면 트래픽이 멈춘 것처럼 보이는 디버깅하기 까다로운 버그가 된다.

---

## 7. API 레퍼런스 — 주요 함수 인터페이스

RIO 함수들은 일반 Winsock 함수와 달리 정적으로 링크되지 않는다. `WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER)`로 함수 포인터 테이블을 직접 얻어와야 한다.

### 7.1 RIO 함수 포인터 테이블 로딩

```cpp
RIO_EXTENSION_FUNCTION_TABLE g_RIO = { 0 };

GUID functionTableId = WSAID_MULTIPLE_RIO;
DWORD bytesReturned = 0;

WSAIoctl(
    dummySocket,                       // RIO 대상이 될 소켓(임시로 하나 생성해서 사용)
    SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
    &functionTableId, sizeof(functionTableId),
    &g_RIO, sizeof(g_RIO),
    &bytesReturned,
    NULL, NULL
);
```

이렇게 채워진 `g_RIO` 구조체의 멤버(`g_RIO.RIORegisterBuffer`, `g_RIO.RIOSend` 등)를 이후 계속 호출하는 방식이다. 아래 각 함수는 이 테이블의 멤버로 존재한다.

### 7.2 RIORegisterBuffer / RIODeregisterBuffer

```cpp
RIO_BUFFERID RIORegisterBuffer(
    PCHAR DataBuffer,
    DWORD DataLength
);

VOID RIODeregisterBuffer(
    RIO_BUFFERID BufferId
);
```

| 파라미터 | 설명 |
|---|---|
| `DataBuffer` | 등록할 메모리 시작 주소. `VirtualAlloc` 등으로 미리 확보해둔 연속된 메모리 |
| `DataLength` | 등록할 길이(바이트). 최대 등록 가능 크기는 시스템 제한(보통 페이지드 풀 관련 제약)이 있으므로 지나치게 큰 단일 등록은 지양 |
| `BufferId` | 등록 성공 시 반환되는 식별자. 이후 `RIO_BUF.BufferId`에 사용. 실패 시 `RIO_INVALID_BUFFERID` |

`RIODeregisterBuffer`는 반환값이 없는(`VOID`) 함수로, 해당 버�퍼를 더 이상 참조하는 진행 중인 I/O가 없을 때 호출해야 안전하다.

### 7.3 RIOCreateCompletionQueue / RIOCloseCompletionQueue / RIOResizeCompletionQueue

```cpp
RIO_CQ RIOCreateCompletionQueue(
    DWORD                         QueueSize,
    PRIO_NOTIFICATION_COMPLETION  NotificationCompletion
);

VOID RIOCloseCompletionQueue(RIO_CQ CQ);

BOOL RIOResizeCompletionQueue(
    RIO_CQ CQ,
    DWORD  QueueSize
);
```

| 파라미터 | 설명 |
|---|---|
| `QueueSize` | 이 CQ가 담을 수 있는 최대 완료 항목 수. 여러 RQ가 공유하므로 예상 동시 처리량을 감안해 넉넉히 설정 |
| `NotificationCompletion` | `RIO_NOTIFICATION_COMPLETION` 구조체. `Type`을 `RIO_EVENT_COMPLETION`(이벤트 기반) 또는 `RIO_IOCP_COMPLETION`(IOCP 연동)으로 설정. IOCP 연동 시 `Iocp.IocpHandle`, `Iocp.CompletionKey`, `Iocp.Overlapped`를 채움 |
| `CQ`(Resize) | 크기를 조정할 기존 CQ |

큐가 가득 차면 이후 제출이 실패하므로(`RIO_CORRUPT_CQ` 등 에러 상태), 트래픽 패턴에 맞춰 `QueueSize`를 산정하거나 필요 시 `RIOResizeCompletionQueue`로 동적으로 늘려야 한다.

### 7.4 RIOCreateRequestQueue

```cpp
RIO_RQ RIOCreateRequestQueue(
    SOCKET      Socket,
    ULONG       MaxOutstandingReceive,
    ULONG       MaxReceiveDataBuffers,
    ULONG       MaxOutstandingSend,
    ULONG       MaxSendDataBuffers,
    RIO_CQ      ReceiveCQ,
    RIO_CQ      SendCQ,
    PVOID       SocketContext
);
```

| 파라미터 | 설명 |
|---|---|
| `Socket` | `WSASocket()`에 `WSA_FLAG_REGISTERED_IO` 플래그를 주고 생성한 소켓이어야 함 |
| `MaxOutstandingReceive` | 이 RQ에 동시에 걸어둘 수 있는 최대 수신 요청 수 |
| `MaxReceiveDataBuffers` | 수신 요청 1건당 사용할 수 있는 최대 버퍼(scatter) 개수 — 단일 버퍼만 쓰면 `1` |
| `MaxOutstandingSend` / `MaxSendDataBuffers` | 송신 쪽 동일 개념 |
| `ReceiveCQ` / `SendCQ` | 이 RQ의 수신/송신 완료가 각각 어느 CQ로 갈지 지정. 같은 CQ를 공유해도 되고 분리해도 됨 |
| `SocketContext` | 이 RQ(소켓)를 식별할 사용자 정의 컨텍스트. `RIORESULT.RequestContext`가 아니라 별도로 세션 매핑에 활용(구현에 따라 요청 단위 `RequestContext`와 별개로 관리) |

RQ에는 별도의 명시적 Close 함수가 없다. `closesocket()`을 호출하면 연결된 RQ도 함께 정리된다(단, 그 시점에 outstanding I/O가 없어야 안전하다는 원칙은 동일하게 적용됨).

### 7.5 RIOSend / RIOSendEx / RIOReceive / RIOReceiveEx

```cpp
BOOL RIOSend(
    RIO_RQ SocketQueue,
    PRIO_BUF pData,
    ULONG DataBufferCount,
    DWORD Flags,
    PVOID RequestContext
);

BOOL RIOReceive(
    RIO_RQ SocketQueue,
    PRIO_BUF pData,
    ULONG DataBufferCount,
    DWORD Flags,
    PVOID RequestContext
);
```

| 파라미터 | 설명 |
|---|---|
| `SocketQueue` | 요청을 넣을 RQ |
| `pData` | `RIO_BUF` 배열(등록된 버퍼 내 오프셋+길이) |
| `DataBufferCount` | `pData` 배열의 원소 수 (`RQ` 생성 시 지정한 `MaxSendDataBuffers`/`MaxReceiveDataBuffers` 이내여야 함) |
| `Flags` | `RIO_MSG_DEFER`(즉시 커널 제출하지 않고 큐잉만 함), `RIO_MSG_DONT_NOTIFY`(이 요청은 완료돼도 알림을 생략), `RIO_MSG_COMMIT_ONLY`(제출 없이 지연된 요청을 커밋만 함) 등을 비트 OR로 조합 |
| `RequestContext` | 완료 시 `RIORESULT.RequestContext`로 그대로 돌아오는 값. 어떤 요청이었는지 식별하는 열쇠(IOCP의 `OVERLAPPED` 포인터와 같은 역할) |

`RIOSendEx`/`RIOReceiveEx`는 여기에 UDP 목적지 주소(`pRemoteAddress`), 제어 정보(`pControlContext`, `pFlags` 등 확장 필드)를 추가로 받는 확장판으로, 주로 UDP 송수신에 사용한다.

```cpp
BOOL RIOReceiveEx(
    RIO_RQ SocketQueue,
    PRIO_BUF pData,
    ULONG DataBufferCount,
    PRIO_BUF pLocalAddress,
    PRIO_BUF pRemoteAddress,
    PRIO_BUF pControlContext,
    PRIO_BUF pFlags,
    DWORD Flags,
    PVOID RequestContext
);
```

`pRemoteAddress`에 송신지 주소 정보를 함께 받아올 수 있어, `recvfrom()`과 동등한 기능을 RIO 경로로 수행할 수 있다.

**반환값**: 성공 시 `TRUE`. `FALSE`면 `WSAGetLastError()`로 원인 확인(버퍼 부족, RQ의 outstanding 한도 초과 등).

### 7.6 RIONotify

큐잉된(`RIO_MSG_DEFER`로 지연된) 요청들을 실제로 커널에 제출하고, CQ에 완료가 쌓이면 통지받을 수 있도록 "재무장"한다.

```cpp
INT RIONotify(RIO_CQ CQ);
```

| 파라미터 | 설명 |
|---|---|
| `CQ` | 제출/재무장할 완료 큐 |

**반환값**: 성공 시 `ERROR_SUCCESS`(0). 이미 통지가 무장된 상태에서 다시 호출하면 `WSAEALREADY`가 반환되는데, 이는 에러가 아니라 "이미 알림이 걸려 있다"는 정상적인 상태 신호이므로 별도 처리 없이 무시해도 된다.

### 7.7 RIODequeueCompletion

CQ에 쌓인 완료 결과를 배치로 꺼내온다.

```cpp
ULONG RIODequeueCompletion(
    RIO_CQ     CQ,
    PRIORESULT Array,
    ULONG      ArraySize
);
```

| 파라미터 | 설명 |
|---|---|
| `CQ` | 대상 완료 큐 |
| `Array` | 결과를 받을 `RIORESULT` 배열. `RIORESULT`는 `RequestContext`, `Status`, `BytesTransferred`, `SocketContext` 필드를 가짐 |
| `ArraySize` | `Array`의 크기(한 번에 받을 최대 개수) |

**반환값**: 실제로 꺼내온 항목 수. `0`이면 큐가 비어있다는 뜻이고, `RIO_CORRUPT_CQ`(특수 값)가 반환되면 해당 CQ가 손상된 상태이므로 그 이후로는 재시도하지 말고 CQ를 폐기/재생성해야 한다.

호출 패턴은 보통 "큐가 빌 때까지 반복 호출 → 마지막에 `RIONotify()`로 재무장"이다(6절 예시 코드 참고).

### 7.8 정리 순서 요약

| 생성 순서 | 해제 순서(역순 권장) |
|---|---|
| ① `RIORegisterBuffer` | ④ `RIODeregisterBuffer` |
| ② `RIOCreateCompletionQueue` | ③ `RIOCloseCompletionQueue` |
| ③ `RIOCreateRequestQueue` (소켓당) | ② `closesocket()` (RQ도 함께 정리됨) |
| ④ `RIOSend`/`RIOReceive` 반복 | ① outstanding I/O가 0이 될 때까지 대기 후 위 역순 진행 |

이 표는 원칙을 보여주기 위한 단순화이며, 실제로는 "해당 자원을 참조하는 진행 중인 I/O가 없음을 확인한 뒤에만 해제한다"는 규칙이 각 단계마다 개별적으로 성립해야 한다.

---

## 8. 마무리

RIO는 IOCP의 완료 통지 모델을 그대로 유지하면서, 버퍼 사전 등록과 배치 제출·수거로 시스템 콜·버퍼 검증 오버헤드를 극한까지 줄이기 위한 특수 목적 확장이다. 대부분의 서버는 IOCP만으로 충분하며, RIO는 실측으로 병목이 확인된 초고빈도·초고처리량 트래픽 상황에서 선택적으로 고려하는 것이 합리적이다. 도입한다면 버퍼 슬롯 관리와 셧다운 순서를 처음부터 원칙 있게 설계하는 것이 가장 중요하다.
