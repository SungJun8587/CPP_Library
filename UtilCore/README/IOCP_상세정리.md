# Windows IOCP(I/O Completion Port) 완벽 정리

## 1. 들어가며

대규모 동시 접속을 처리하는 서버(게임 서버, 실시간 서비스, 대용량 API 게이트웨이 등)를 Windows에서 구현할 때 사실상 표준으로 쓰이는 것이 IOCP다. 이 글에서는 IOCP가 왜 필요한지부터 내부 동작 원리, 실전 구현 패턴, 흔히 겪는 함정까지 순서대로 정리한다.

---

## 2. 동기 I/O 모델의 한계

### 2.1 Thread-per-connection

가장 직관적인 서버 모델은 커넥션마다 스레드를 하나씩 붙이는 방식이다.

```cpp
while (true) {
    SOCKET client = accept(listenSocket, ...);
    std::thread(HandleClient, client).detach();
}
```

구현은 단순하지만 두 가지 문제가 있다.

- **메모리**: 스레드마다 기본 1MB의 스택이 할당된다. 커넥션이 1만 개면 그것만으로 10GB.
- **컨텍스트 스위칭 비용**: OS 스케줄러가 수천~수만 개의 스레드를 오가며 스위칭하는 비용이 실제 작업량보다 커진다.

### 2.2 select / WSAPoll 기반 이벤트 루프 (Reactor 패턴)

소켓 집합을 감시하다가 "읽기/쓰기가 가능한 상태"가 된 소켓만 골라 처리하는 방식이다. 스레드 수는 줄일 수 있지만, 매 호출마다 감시 대상 전체를 스캔하기 때문에 소켓 수에 비례해 오버헤드가 커진다(`select`는 특히 최대 소켓 수 제한도 있다). 그리고 "가능하다"는 신호일 뿐 실제 읽기/쓰기는 애플리케이션이 수행해야 한다.

### 2.3 Proactor 패턴으로의 전환

IOCP는 이 둘과 근본적으로 다른 접근을 취한다. "지금 읽어도 된다"가 아니라 **"읽기가 이미 끝났다"**는 사실을 커널이 알려준다. 애플리케이션은 미리 비동기 I/O 요청(overlapped I/O)을 커널에 던져두고, 실제 데이터 전송은 커널이 백그라운드에서 수행한 뒤 완료만 통지받는다. 이것이 Proactor 패턴이며, IOCP는 Windows 커널이 이 패턴을 위해 제공하는 오브젝트다.

---

## 3. IOCP의 핵심 구조

### 3.1 완료 포트라는 커널 오브젝트

```cpp
HANDLE hIOCP = CreateIoCompletionPort(
    INVALID_HANDLE_VALUE, // 새 포트를 생성
    NULL,                  // 기존 포트 없음
    0,                     // CompletionKey (신규 생성 시 미사용)
    0                      // 동시성 값 0 → 코어 수만큼 자동 설정
);
```

이렇게 만든 포트에 소켓 핸들을 연결한다.

```cpp
CreateIoCompletionPort(
    (HANDLE)clientSocket,
    hIOCP,
    (ULONG_PTR)pSession,  // CompletionKey: 세션 포인터를 그대로 사용
    0
);
```

한 번 연결된 소켓은 이후 그 소켓에 대한 모든 비동기 I/O의 완료가 이 포트의 **내부 완료 큐(FIFO 큐)**에 자동으로 쌓인다. CompletionKey는 임의의 값을 넣을 수 있는데, 보통 세션 객체를 식별할 수 있는 포인터나 ID를 넣어 완료 시 어떤 세션의 I/O인지 바로 찾을 수 있게 한다.

### 3.2 OVERLAPPED 구조체 — 요청과 완료를 잇는 열쇠

비동기 I/O를 걸 때마다 `OVERLAPPED` 구조체(혹은 이를 포함하는 확장 구조체)를 함께 넘긴다.

```cpp
struct IoContext : OVERLAPPED {
    WSABUF wsaBuf;
    IoType type;  // Recv, Send 등 사용자 정의
    // ...
};
```

I/O가 완료되면 커널은 이 포인터를 그대로 완료 큐에 실어 보낸다. 즉 `OVERLAPPED`를 상속/포함한 구조체를 만들어두면, 완료 시 `reinterpret_cast`로 되돌려서 "이게 무슨 요청이었는지" 알아낼 수 있다. 이는 하나의 소켓에 여러 개의 I/O(예: Recv와 Send를 동시에)를 걸어둘 수 있게 해주는 핵심 메커니즘이다.

### 3.3 비동기 요청 걸기 — WSARecv / WSASend

```cpp
DWORD flags = 0;
int ret = WSARecv(
    socket,
    &pContext->wsaBuf, 1,
    NULL,                 // 즉시 완료 시 바이트 수(overlapped라 보통 NULL)
    &flags,
    static_cast<OVERLAPPED*>(pContext),
    NULL
);

if (ret == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING) {
        // 진짜 에러 — 연결 종료 처리
    }
    // WSA_IO_PENDING은 정상: 비동기로 진행 중이라는 뜻
}
```

`WSA_IO_PENDING`은 에러가 아니라 "커널이 백그라운드에서 처리 중"이라는 정상 신호다. 이 구분을 놓치면 정상적으로 동작 중인 I/O를 에러로 오판하는 버그가 흔히 발생한다.

### 3.4 완료 수신 — GetQueuedCompletionStatus

워커 스레드들은 이 함수 하나로 대기하며 완료된 I/O를 받아간다.

```cpp
BOOL GetQueuedCompletionStatus(
    HANDLE hCompletionPort,
    LPDWORD lpNumberOfBytesTransferred,
    PULONG_PTR lpCompletionKey,
    LPOVERLAPPED* lpOverlapped,
    DWORD dwMilliseconds
);
```

전형적인 워커 루프:

```cpp
void WorkerLoop(HANDLE hIOCP) {
    while (true) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* pOverlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE
        );

        auto* pSession = reinterpret_cast<Session*>(completionKey);
        auto* pContext = static_cast<IoContext*>(pOverlapped);

        if (!ok || (ok && bytesTransferred == 0 && pContext->type != IoType::Zero)) {
            // 연결 종료(정상 종료 또는 에러) 처리
            HandleDisconnect(pSession);
            continue;
        }

        switch (pContext->type) {
            case IoType::Recv: HandleRecvCompleted(pSession, pContext, bytesTransferred); break;
            case IoType::Send: HandleSendCompleted(pSession, pContext, bytesTransferred); break;
        }
    }
}
```

여기서 반드시 구분해야 할 세 가지 케이스가 있다.

- `ok == FALSE`이고 `pOverlapped == NULL`: 큐 자체 대기 실패(포트 핸들 문제 등) — 드묾
- `ok == FALSE`이고 `pOverlapped != NULL`: I/O 자체는 완료됐지만 에러 상태(연결 리셋 등) — `GetLastError()`로 원인 확인
- `ok == TRUE`이고 `bytesTransferred == 0`: 상대가 정상적으로 연결을 종료(graceful close)했다는 뜻(Recv 기준)

이 세 케이스를 뭉뚱그려 처리하면 정상 종료와 비정상 종료를 구분하지 못하는 버그로 이어진다.

### 3.5 동시성 값(NumberOfConcurrentThreads)의 의미

`CreateIoCompletionPort`의 네 번째 인자는 흔히 오해하는 부분이다. 이는 "워커 스레드를 몇 개 만들어야 하는가"가 아니라, **"이 포트에서 동시에 실행(러너블) 가능한 스레드 수의 상한"**이다.

- 워커 스레드는 보통 이 값보다 많이(예: 코어 수의 1.5~2배) 만들어 대기시켜 둔다. 일부가 블로킹 시스템 콜이나 페이지 폴트로 잠시 멈춰도 다른 스레드가 즉시 CPU를 활용할 수 있게 하기 위해서다.
- 반면 커널은 이 값을 넘는 수의 스레드를 동시에 러너블 상태로 만들지 않는다. 즉 8코어 머신에서 동시성 값을 8로 주면, 워커 스레드를 16개 만들어놔도 실제로 동시에 CPU를 쓰는 스레드는 8개로 제한되어 캐시 스레싱과 불필요한 스위칭을 줄여준다.
- 값이 0이면 시스템의 프로세서 수를 기본값으로 사용한다.

### 3.6 Zero-byte Recv, 지연 Accept 등 실전 기법

- **AcceptEx / ConnectEx**: 이들도 overlapped I/O로 걸 수 있어, accept·connect 자체도 IOCP 완료 큐를 통해 비동기로 처리할 수 있다. `accept()`를 블로킹으로 별도 스레드에서 도는 구조보다 일관성이 높다.
- **Zero-byte Recv**: 버퍼 크기 0으로 `WSARecv`를 걸어 "데이터가 도착했다"는 신호만 받고, 실제 읽기는 완료 후 별도로 수행하는 기법. 큰 수신 버퍼를 세션마다 상시 점유하지 않아도 되는 장점이 있지만, 실제 읽기를 한 번 더 호출해야 하므로 지연이 늘어나는 트레이드오프가 있다.
- **Disconnect(TransmitFile로 재사용)**: `DisconnectEx`와 `AcceptEx`를 조합하면 소켓 핸들을 재사용해 accept 비용을 줄일 수 있다.

---

## 4. 세션(커넥션) 수명 관리 — 가장 많이 틀리는 부분

IOCP 자체는 매우 안정적인 API지만, 실무에서 버그의 8할은 "세션이 언제 안전하게 파괴되어도 되는가"에서 나온다.

### 4.1 문제의 본질

소켓을 `closesocket()`으로 닫아도, 이미 커널에 제출되어 진행 중이던 I/O의 완료 통지는 큐에 남아 나중에 도착할 수 있다. 세션 객체를 즉시 `delete`하면, 뒤늦게 도착한 완료 통지가 이미 해제된 메모리(`OVERLAPPED`를 포함한 컨텍스트, 세션 자체)를 참조하는 use-after-free가 발생한다.

### 4.2 일반적인 해법 — 참조 카운팅

- 세션에 진행 중인 I/O 개수를 원자적 카운터로 추적한다(Recv를 걸 때 +1, 완료 시 -1 등).
- `close` 요청이 오면 소켓은 즉시 닫되(더 이상 새 I/O를 받지 않도록), 실제 세션 객체 파괴는 "카운터가 0이 될 때"까지 미룬다.
- `shared_ptr`/`enable_shared_from_this` 조합으로 I/O 컨텍스트가 세션의 소유권을 들고 있게 하면, 완료 통지가 늦게 와도 세션이 먼저 파괴되지 않는다.

```cpp
class Session : public std::enable_shared_from_this<Session> {
    std::atomic<int> _outstandingIo{0};
public:
    void PostRecv() {
        _outstandingIo.fetch_add(1, std::memory_order_relaxed);
        auto self = shared_from_this(); // 컨텍스트가 세션을 붙잡음
        // WSARecv 호출...
    }
    void OnIoCompleted() {
        if (_outstandingIo.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // 마지막 I/O였다면 최종 정리(FinalizeClose 등) 수행
        }
    }
};
```

이 원칙("진행 중인 I/O가 0이 될 때까지 실제 자원 해제를 미룬다")은 IOCP 기반 서버 설계에서 가장 중요한 불변식(invariant) 중 하나다.

---

## 5. 성능 관점에서 본 IOCP

- **스레드 수가 커넥션 수에 비례하지 않는다**: 워커 스레드는 코어 수 근처로 고정하고, 커넥션은 수만~수십만 개까지 확장 가능하다.
- **락 경합 최소화**: IOCP 자체 큐잉/스케줄링은 커널이 담당하므로 애플리케이션 레벨에서 별도의 이벤트 루프 락이 필요 없다.
- **버퍼 관리가 성능에 큰 영향**: overlapped I/O 도중에는 버퍼가 유효해야 하므로, 세션마다 버퍼를 고정 할당하거나 오브젝트 풀에서 재사용하는 방식이 일반적이다. 매 I/O마다 `new`/`delete`로 버퍼를 할당하면 IOCP의 이점을 스레드 관리에서만 얻고 메모리 할당에서 잃게 된다.

---

## 6. API 레퍼런스 — 주요 함수 인터페이스

### 6.1 CreateIoCompletionPort

완료 포트를 새로 만들거나, 기존 포트에 파일/소켓 핸들을 연결한다. 두 역할을 하나의 함수가 겸한다.

```cpp
HANDLE CreateIoCompletionPort(
    HANDLE    FileHandle,
    HANDLE    ExistingCompletionPort,
    ULONG_PTR CompletionKey,
    DWORD     NumberOfConcurrentThreads
);
```

| 파라미터 | 설명 |
|---|---|
| `FileHandle` | 포트에 연결할 핸들. 소켓, 파일, 파이프 등 overlapped I/O를 지원하는 핸들이면 된다. **새 포트만 생성**할 때는 `INVALID_HANDLE_VALUE`를 넘긴다. |
| `ExistingCompletionPort` | 기존 포트 핸들. **새 포트 생성** 시에는 `NULL`. 기존 소켓을 특정 포트에 **연결**할 때는 그 포트의 핸들을 넘긴다. |
| `CompletionKey` | 이 핸들에 대한 완료 통지마다 함께 전달될 사용자 정의 값. 보통 세션/커넥션을 가리키는 포인터를 캐스팅해서 넣는다. 새 포트 생성 시에는 무시된다. |
| `NumberOfConcurrentThreads` | 동시에 실행 가능한 스레드 수 상한 힌트. `0`이면 시스템의 프로세서 개수를 기본값으로 사용. **새 포트 생성** 시에만 의미가 있고, 기존 소켓 연결 호출에서는 무시된다. |

**반환값**: 성공 시 완료 포트 핸들(신규 생성이든 연결이든 동일 핸들 반환), 실패 시 `NULL`(`GetLastError()`로 원인 확인).

사용 패턴은 항상 두 단계다.

```cpp
// ① 포트 생성
HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
// ② 소켓을 그 포트에 연결
CreateIoCompletionPort((HANDLE)clientSocket, hIOCP, (ULONG_PTR)pSession, 0);
```

### 6.2 GetQueuedCompletionStatus

워커 스레드가 완료된 I/O를 하나 받아올 때 호출한다. 이 스레드에 완료 항목이 없으면 지정한 시간만큼 블로킹한다.

```cpp
BOOL GetQueuedCompletionStatus(
    HANDLE       CompletionPort,
    LPDWORD      lpNumberOfBytesTransferred,
    PULONG_PTR   lpCompletionKey,
    LPOVERLAPPED *lpOverlapped,
    DWORD        dwMilliseconds
);
```

| 파라미터 | 설명 |
|---|---|
| `CompletionPort` | 대기할 완료 포트 핸들 |
| `lpNumberOfBytesTransferred` | [출력] 완료된 I/O에서 실제로 전송된 바이트 수 |
| `lpCompletionKey` | [출력] 해당 핸들을 포트에 연결할 때 지정했던 `CompletionKey` — 보통 세션 식별용 |
| `lpOverlapped` | [출력] 완료된 I/O에 사용됐던 `OVERLAPPED` 구조체(또는 이를 포함한 확장 구조체)의 포인터 — 어떤 요청이었는지 식별하는 열쇠 |
| `dwMilliseconds` | 대기 시간(ms). `INFINITE`면 무한 대기, `0`이면 즉시 반환(폴링) |

**반환값과 판별 방법**:

| 반환값 | `lpOverlapped` | 의미 |
|---|---|---|
| `TRUE` | 유효 | I/O 정상 완료. `bytesTransferred == 0`이면(Recv 기준) 상대가 정상 종료(graceful close)한 것 |
| `FALSE` | `NULL` | 큐 자체에서 대기 실패 (타임아웃 포함) — `GetLastError()`로 원인 확인 |
| `FALSE` | 유효 | I/O는 완료됐으나 에러 상태로 완료됨(연결 리셋 등) — `GetLastError()`로 원인 확인 |

이 세 가지 조합을 정확히 구분해서 처리하는 것이 IOCP 서버 안정성의 기본이다.

### 6.3 GetQueuedCompletionStatusEx

여러 완료 항목을 한 번에 배치로 받아올 수 있는 확장 버전(Vista/Server 2008+).

```cpp
BOOL GetQueuedCompletionStatusEx(
    HANDLE             CompletionPort,
    LPOVERLAPPED_ENTRY lpCompletionPortEntries,
    ULONG              ulCount,
    PULONG             ulNumEntriesRemoved,
    DWORD              dwMilliseconds,
    BOOL               fAlertable
);
```

| 파라미터 | 설명 |
|---|---|
| `CompletionPort` | 대기할 완료 포트 핸들 |
| `lpCompletionPortEntries` | [출력] `OVERLAPPED_ENTRY` 배열. 각 원소가 `lpOverlapped`/`lpCompletionKey`/`dwNumberOfBytesTransferred`를 담고 있다 |
| `ulCount` | 배열의 크기(한 번에 받을 수 있는 최대 항목 수) |
| `ulNumEntriesRemoved` | [출력] 실제로 받아온 항목 수 |
| `dwMilliseconds` | 대기 시간(ms) |
| `fAlertable` | `TRUE`면 alertable wait 상태로 대기(APC 큐 처리 가능). 보통 `FALSE` |

한 번의 호출로 여러 완료를 받을 수 있어 `GetQueuedCompletionStatus()`를 반복 호출하는 것보다 호출 횟수를 줄일 수 있다. 다만 IOCP 자체의 기본 모델(단건 통지)과 워커 로직을 다시 짜야 하므로, 처리량이 매우 중요한 경우가 아니라면 단건 버전으로도 충분한 경우가 많다.

### 6.4 PostQueuedCompletionStatus

실제 I/O 완료가 아니라, 애플리케이션이 임의로 완료 큐에 이벤트를 넣고 싶을 때 사용한다. 워커 스레드에 "종료해라" 같은 커스텀 신호를 보낼 때 흔히 쓴다.

```cpp
BOOL PostQueuedCompletionStatus(
    HANDLE       CompletionPort,
    DWORD        dwNumberOfBytesTransferred,
    ULONG_PTR    dwCompletionKey,
    LPOVERLAPPED lpOverlapped
);
```

| 파라미터 | 설명 |
|---|---|
| `CompletionPort` | 이벤트를 넣을 완료 포트 |
| `dwNumberOfBytesTransferred` | `GetQueuedCompletionStatus()`가 그대로 돌려줄 값 — 의미는 애플리케이션이 정의 |
| `dwCompletionKey` | 마찬가지로 애플리케이션이 정의하는 값. 예: 특수 값으로 "종료 신호"를 표시 |
| `lpOverlapped` | 마찬가지. `NULL`을 넣고 `CompletionKey`만으로 종료 신호를 구분하는 방식이 흔함 |

워커 스레드 개수만큼 이 함수를 호출해 각 워커가 "종료 신호"를 하나씩 받고 루프를 빠져나가게 하는 것이 일반적인 종료 패턴이다.

### 6.5 WSARecv / WSASend

overlapped 모드로 비동기 수신/송신을 요청한다. 시그니처가 거의 동일하다.

```cpp
int WSARecv(
    SOCKET                             s,
    LPWSABUF                           lpBuffers,
    DWORD                              dwBufferCount,
    LPDWORD                            lpNumberOfBytesRecvd,
    LPDWORD                            lpFlags,
    LPWSAOVERLAPPED                    lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

int WSASend(
    SOCKET                             s,
    LPWSABUF                           lpBuffers,
    DWORD                              dwBufferCount,
    LPDWORD                            lpNumberOfBytesSent,
    DWORD                              dwFlags,
    LPWSAOVERLAPPED                    lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
```

| 파라미터 | 설명 |
|---|---|
| `s` | 대상 소켓 |
| `lpBuffers` | `WSABUF{ ULONG len; CHAR* buf; }` 배열. 여러 버퍼를 한 번에 넘길 수 있는 scatter/gather I/O 지원 |
| `dwBufferCount` | `lpBuffers` 배열의 원소 수 |
| `lpNumberOfBytesRecvd` / `lpNumberOfBytesSent` | 즉시 완료된 경우 전송된 바이트 수가 채워짐. overlapped로 걸리면(`WSA_IO_PENDING`) 이 값은 무시하고, 나중에 `GetQueuedCompletionStatus()`의 출력값을 사용해야 함 |
| `lpFlags`(Recv) | [입출력] `MSG_PEEK` 등 수신 플래그 |
| `dwFlags`(Send) | 송신 플래그(보통 `0`) |
| `lpOverlapped` | IOCP에 연결된 소켓이라면 반드시 유효한 `OVERLAPPED` 포인터를 넘겨야 비동기로 동작함 |
| `lpCompletionRoutine` | APC 기반 콜백. **IOCP를 쓸 때는 반드시 `NULL`** — IOCP와 완료 루틴 콜백은 같은 소켓에서 함께 쓸 수 없음 |

**반환값**: 즉시 완료되면 `0`, 그 외에는 `SOCKET_ERROR`를 반환하며 `WSAGetLastError()`가 `WSA_IO_PENDING`이면 정상적으로 비동기 진행 중이라는 뜻이다. 그 외 에러 코드는 실제 실패다.

### 6.6 AcceptEx

`accept()`의 비동기 버전. 클라이언트 연결 수립 자체를 overlapped I/O로 처리해 IOCP 완료 큐로 통지받을 수 있게 한다.

```cpp
BOOL AcceptEx(
    SOCKET       sListenSocket,
    SOCKET       sAcceptSocket,
    PVOID        lpOutputBuffer,
    DWORD        dwReceiveDataLength,
    DWORD        dwLocalAddressLength,
    DWORD        dwRemoteAddressLength,
    LPDWORD      lpdwBytesReceived,
    LPOVERLAPPED lpOverlapped
);
```

| 파라미터 | 설명 |
|---|---|
| `sListenSocket` | 리슨 중인 소켓 |
| `sAcceptSocket` | 미리 `socket()`으로 생성만 해둔(아직 연결되지 않은) 소켓. 연결이 완료되면 이 소켓이 클라이언트와 통신하는 소켓이 됨 |
| `lpOutputBuffer` | 로컬/원격 주소 정보와(옵션으로) 최초 수신 데이터를 함께 받을 버퍼 |
| `dwReceiveDataLength` | `lpOutputBuffer` 중 최초 수신 데이터에 할당할 크기. `0`이면 연결 수립 즉시 완료 통지(데이터 도착까지 기다리지 않음) |
| `dwLocalAddressLength` / `dwRemoteAddressLength` | 주소 구조체 저장용 크기. 각각 `sizeof(SOCKADDR) + 16` 이상이어야 함(패딩 필요) |
| `lpdwBytesReceived` | [출력, 동기 완료 시] 수신된 바이트 수 |
| `lpOverlapped` | overlapped 구조체 — IOCP 완료 통지에 사용됨 |

연결이 재사용 가능한 소켓 핸들을 미리 만들어두고 재사용할 수 있어, 매 연결마다 `socket()`을 새로 호출하는 것보다 효율적이다. 완료 후에는 `GetAcceptExSockaddrs()`로 로컬/원격 주소를 파싱하고, `setsockopt(SO_UPDATE_ACCEPT_CONTEXT)`로 새 소켓에 리슨 소켓의 속성을 상속시켜야 `getsockname`/`getpeername` 등이 정상 동작한다.

### 6.7 ConnectEx

`connect()`의 비동기 버전. 클라이언트 역할을 하는 서버(다른 서버로 아웃바운드 연결을 대량으로 맺는 경우)에서 유용하다.

```cpp
BOOL ConnectEx(
    SOCKET         s,
    const sockaddr *name,
    int            namelen,
    PVOID          lpSendBuffer,
    DWORD          dwSendDataLength,
    LPDWORD        lpdwBytesSent,
    LPOVERLAPPED   lpOverlapped
);
```

| 파라미터 | 설명 |
|---|---|
| `s` | **미리 `bind()`가 호출되어 있어야 하는** 소켓 (`ConnectEx`의 특이한 제약) |
| `name` / `namelen` | 접속할 대상 주소 |
| `lpSendBuffer` / `dwSendDataLength` | 연결 수립과 동시에 보낼 초기 데이터(옵션, 없으면 `NULL`/`0`) |
| `lpdwBytesSent` | [출력, 동기 완료 시] 전송된 바이트 수 |
| `lpOverlapped` | overlapped 구조체 |

`AcceptEx`/`ConnectEx`는 둘 다 함수 포인터를 `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)`로 직접 얻어와야 하는 Winsock 확장 함수라는 공통점이 있다(아래 8절 RIO의 확장 함수 로딩과 동일한 방식).

---

## 7. 마무리

IOCP는 "완료를 알려주는" Proactor 모델로, 스레드 수를 커넥션 수와 분리시켜 대규모 동시 접속을 다루는 Windows의 사실상 표준 기술이다. API 자체는 크지 않지만, 실전 안정성은 OVERLAPPED 컨텍스트 설계와 세션 수명 관리(참조 카운팅, close 이후 지연 파괴)에 달려 있다. 이 부분을 제대로 설계해두면 이후 RIO 같은 더 공격적인 최적화도 같은 원칙 위에서 안전하게 얹을 수 있다.
