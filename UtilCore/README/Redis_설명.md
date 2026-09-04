# Redis 모듈 설명

IOCP 기반 서버 프레임워크 위에서 동작하는 비동기 Redis 클라이언트 모듈이다.
소켓 하나당 커맨드 하나씩 순차 처리하는 `CRedisClient`를 여러 개 묶어 `CRedisConnectionPool`이 관리하고,
그 위에 `CRedisService`가 스레드 안전한 콜백 이관(JobQueue)을 얹은 최상위 파사드로 노출된다.
RESP(REdis Serialization Protocol) 인코딩/디코딩은 `CRedisCommandBuilder`(요청)와 `CRedisParser`(응답)가 전담하고,
파싱된 응답은 `CRedisResultSet`을 통해 타입 안전하게 꺼내 쓴다. `CRedisServerHeartbeat`는 이 스택 위에서
서버 자신의 생존 여부를 Redis에 등록/갱신하는 디스커버리용 보조 클래스다.

```
CRedisService (파사드, 스레드 이관)
    └─ CRedisConnectionPool (커넥션 대여/반납, 격리 큐 + 백그라운드 재연결)
            └─ CRedisClient × N (소켓 1개당 1개, IOCP 비동기 송수신)
                    ├─ CRedisCommandBuilder (요청 인코딩)
                    └─ CRedisParser (응답 디코딩)

CRedisResultSet          — RedisValue → 타입별 순차 추출 유틸
CRedisServerHeartbeat     — CRedisService를 이용한 서버 등록/TTL 갱신
```

> 아래 멤버 함수 표는 `함수 | 함수 설명 | 파라미터명 | 파라미터 설명` 4열이다.
> 파라미터가 여러 개인 함수는 파라미터 하나당 한 행을 차지하며, 이 경우 함수/함수 설명 칸은
> 그 함수의 첫 행에만 표기하고 이어지는 행은 비워 같은 함수임을 나타낸다.

---

## 0. RedisProtocol.h — 공통 타입

RESP 데이터를 표현하는 최상위 타입들. 별도 클래스는 아니지만 이후 모든 클래스가 이 타입을 주고받는다.

### ERedisType (enum class)
RESP 응답의 5가지 기본 타입.

| 값 | RESP 접두 | 의미 |
|---|---|---|
| `Unknown` | - | 초기값 |
| `SimpleString` | `+` | 단순 문자열 (예: "OK") |
| `Error` | `-` | 에러 메시지 |
| `Integer` | `:` | 정수 |
| `BulkString` | `$` | 길이 지정 문자열(바이너리 세이프) |
| `Array` | `*` | 하위 `RedisValue`들의 배열 |

### struct RedisValue
Redis 응답 하나를 표현하는 재귀적 구조체.

**멤버 변수**

| 이름 | 타입 | 설명 |
|---|---|---|
| `eType` | `ERedisType` | RESP 타입 |
| `i64Val` | `int64` | Integer 값 또는 BulkString/Array의 길이(-1이면 Null) |
| `strVal` | `std::string` | SimpleString/Error/BulkString의 문자열 데이터 |
| `arrayVal` | `CVector<RedisValue>` | Array 타입일 때의 하위 요소 목록 |

**멤버 함수**

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `bool IsNull() const` | BulkString 또는 Array 타입이면서 `i64Val == -1`인 경우(Null Bulk String / Null Array)를 판별 | 없음 | - |

### using RedisCallback
`std::function<void(const RedisValue&)>` — 모든 계층(Client/Pool/Service)이 공통으로 사용하는 응답 콜백 시그니처.

---

## 1. CRedisParser

**역할**: 소켓에서 수신된 바이트 스트림을 누적하며, RESP 규격에 따라 완전한 값(`RedisValue`) 단위로 잘라낸다.
미완성 패킷(TCP 스트림이 메시지 중간에서 끊긴 경우) 재조립 책임을 전담하는 유일한 컴포넌트다.

**사용 패턴**: 수신 바이트를 `Feed()`로 넣고, `TryParse()`를 반복 호출해 그 시점까지 완성된 값을 모두 꺼낸다.
데이터가 부족하면 `TryParse()`는 `false`를 반환하며 내부 상태를 그대로 보존하므로, 다음 `Feed()` 이후 다시 호출하면
이어서 파싱을 재개한다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_pendingBuffer` | `std::string` | 아직 소비되지 않은(=완성되지 않은) 누적 바이트. 미완성 패킷 보관의 유일한 소유자 |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisParser()` | 생성자. `Reset()` 호출 | 없음 | - |
| `~CRedisParser()` | 소멸자 | 없음 | - |
| `void Reset()` | `_pendingBuffer`를 비워 파서 상태를 초기화(연결 종료/재연결 시 사용) | 없음 | - |
| `void Feed(const char* pBuffer, const int32 nBytes)` | 수신된 원시 바이트를 `_pendingBuffer`에 append | `pBuffer` | 수신 데이터 버퍼 포인터. `nullptr`이면 아무 동작도 하지 않음 |
| | | `nBytes` | 수신된 바이트 크기. `0` 이하면 아무 동작도 하지 않음 |
| `bool TryParse(RedisValue& outValue)` | `_pendingBuffer` 맨 앞에서 완전한 RESP 값 하나를 파싱해 꺼낸다. 성공 시 소비한 만큼 버퍼를 앞으로 당기고(`erase`) `true`; 데이터 부족/프로토콜 오류면 `false`(버퍼는 보존) | `outValue` | (출력) 파싱 성공 시 결과를 담을 참조 |
| `bool ParseValue(...)` (private) | 접두 문자(`+ - : $ *`)를 보고 재귀적으로 값을 해석. 실패 시 `nOffset`을 호출 전 위치로 되돌림. `$`/`*`의 길이·개수 값이 `-2` 이하(Null 표시 `-1` 제외 음수)면 프로토콜 손상으로 간주해 실패 처리. `std::stoll` 예외는 내부에서 캐치해 파싱 실패로 변환 | `pBuffer` | 파싱 대상 버퍼 포인터(`_pendingBuffer.data()`) |
| | | `nSize` | `pBuffer`의 전체 바이트 크기 |
| | | `nOffset` | (입출력) 읽기 시작 오프셋. 성공 시 소비한 만큼 전진, 실패 시 호출 전 값으로 복원 |
| | | `outValue` | (출력) 파싱 결과. Array 타입이면 재귀 호출로 하위 요소가 채워짐 |
| `bool ReadLine(...)` (private) | `\r\n` 기준으로 한 줄을 읽는다. 개행을 찾지 못하면 `false` | `pBuffer` | 읽을 버퍼 포인터 |
| | | `nSize` | `pBuffer`의 전체 바이트 크기 |
| | | `nOffset` | (입출력) 읽기 시작 오프셋. 성공 시 개행(`\r\n`) 다음 위치로 이동 |
| | | `strLine` | (출력) 개행 전까지 읽은 한 줄 문자열 |

---

## 2. CRedisCommandBuilder

**역할**: `CVector<std::string>` 형태의 명령어 인자 목록을 RESP Array 포맷(`*N\r\n$L\r\n...`)으로 인코딩하는 정적 유틸리티.
모든 메서드가 `static`이며 인스턴스를 만들 필요가 없다.

**두 가지 경로**
- `Build()`: `std::string` 결과가 필요한 곳(로깅/디버깅 등)에 쓰는 간편 버전.
- `CalcEncodedSize()` + `Encode()`: 송신 버퍼에 결과를 직접 써넣어 중간 `std::string` 사본을 만들지 않는 경로. 실제 전송 경로(`CRedisClient::SendCommand`)는 이 버전을 사용한다.

### 멤버 변수
없음 (상태를 갖지 않는 순수 정적 유틸리티 클래스).

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `static std::string Build(const CVector<std::string>& vecArgs)` | `ostringstream`으로 RESP 문자열을 조립해 반환 | `vecArgs` | Redis 명령어 및 인자 목록(예: `{"SET","Key","Value"}`) |
| `static uint32 CalcEncodedSize(const CVector<std::string>& vecArgs)` | `Encode()`가 필요로 하는 정확한 바이트 크기를 미리 계산 | `vecArgs` | Redis 명령어 및 인자 목록 |
| `static void Encode(const CVector<std::string>& vecArgs, char* pDest)` | `pDest`에 RESP 포맷을 직접 기록. 헤더는 `WriteHeader()`로, 데이터는 `memcpy`로 기록 | `vecArgs` | Redis 명령어 및 인자 목록 |
| | | `pDest` | (출력) 기록 대상 버퍼. `CalcEncodedSize(vecArgs)` 이상의 여유 공간 필요 |
| `static uint32 HeaderLen(char cPrefix, size_t nValue)` (private) | `"<cPrefix><nValue>\r\n"` 한 줄이 차지할 바이트 수 계산(`_snprintf_s` 이용) | `cPrefix` | RESP 접두 문자(`*` 또는 `$`) |
| | | `nValue` | 헤더에 들어갈 개수/길이 값 |
| `static size_t WriteHeader(char* pDest, char cPrefix, size_t nValue)` (private) | 위 헤더 문자열을 `pDest`에 실제로 기록하고 기록한 바이트 수 반환 | `pDest` | (출력) 기록 대상 위치 |
| | | `cPrefix` | RESP 접두 문자(`*` 또는 `$`) |
| | | `nValue` | 헤더에 들어갈 개수/길이 값 |

---

## 3. CRedisClient

**역할**: `CIocpObject`를 상속받는, 소켓 1개에 대응하는 비동기 Redis 연결 단위. 연결/해제, 명령 전송, 응답 수신 및
콜백 디스패치를 담당한다. `std::enable_shared_from_this`를 상속해 IOCP 콜백에서 자기 자신의 생존을 보장한다.

**스레드 계약**: 한 시점에 이 객체에 대해 미완료 `SendCommand()`가 동시에 하나만 존재한다고 가정한다(`_sendEvent`와
전송 오프셋을 객체당 하나만 두고 재사용하기 때문). `CRedisConnectionPool`은 커넥션을 대여한 뒤 응답을 받을 때까지
다시 대여되지 않도록 보장하는 방식으로 이 계약을 지킨다. 풀을 거치지 않고 이 클래스를 여러 스레드에서 직접
동시 호출해서는 안 된다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_socket` | `SOCKET` | 소켓 핸들. 미연결 시 `INVALID_SOCKET` |
| `_iocpCore` | `CIocpCoreRef` | IOCP 코어 참조 |
| `_recvBuffer` | `CRingBuffer` | 수신 링 버퍼. 소켓에서 갓 도착한 바이트를 IOCP 완료 시점까지만 보관 |
| `_recvEvent` | `RecvEvent` | Recv용 Overlapped 이벤트 |
| `_sendEvent` | `SendEvent` | Send용 Overlapped 이벤트 |
| `_pendingSendBuffer` | `CSendBufferRef` | 현재 전송 중인 명령의 송신 버퍼. 부분 전송 시 이어 보내기 위해 유지 |
| `_sendOffset` | `uint32` | `_pendingSendBuffer` 중 이미 전송 완료된 바이트 수 |
| `_sendTotalSize` | `uint32` | `_pendingSendBuffer`의 전체 바이트 수 |
| `_parser` | `CRedisParser` | RESP 파서. 미완성 패킷 재조립의 유일한 소유자 |
| `_queueLock` | `std::mutex` | `_pendingCallbacks` 동기화 락 |
| `_pendingCallbacks` | `CQueue<RedisCallback>` | 전송했지만 아직 응답을 받지 못한 명령들의 콜백 큐(FIFO, Redis 파이프라이닝의 응답 순서 보장을 전제) |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisClient(CIocpCoreRef iocpCore)` | 생성자. 소켓을 `INVALID_SOCKET`으로, 수신 버퍼를 `DATABASE_BUFFER_SIZE`로 초기화 | `iocpCore` | IOCP 코어 참조 객체 |
| `~CRedisClient()` | 소멸자. `Disconnect()` 호출 | 없음 | - |
| `HANDLE GetHandle() override` | IOCP 바인딩용 소켓 핸들 반환 | 없음 | - |
| `CIocpObjectRef GetIocpObjectPtr() override` | Aliasing Constructor로 `CIocpObjectRef`를 생성해 반환(제어 블록은 `CRedisClient`와 공유) | 없음 | - |
| `void Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes = 0) override` | IOCP 완료 통지 처리. `_recvEvent`면 `numOfBytes==0`일 때 `Disconnect()`, 아니면 `ProcessRecv()`. `_sendEvent`면 `numOfBytes==0`일 때 `Disconnect()`; 아니면 `_sendOffset`을 누적해 부분 전송이면 `DoSend()`로 이어 보내고, 다 보냈으면 송신 상태 초기화 | `iocpEvent` | 완료된 IOCP 이벤트 포인터(`&_recvEvent`/`&_sendEvent`와 비교해 판별) |
| | | `numOfBytes` | 이번 완료로 전송/수신된 바이트 수(`0`이면 연결 끊김으로 간주) |
| `bool Connect(const std::string& strIP, const uint16 nPort, const int32 nConnectTimeoutMs = 3000)` | TCP 소켓 생성 후 연결. `connect()`가 동기 호출이므로 소켓을 일시 논블로킹 전환 후 `select()`(writefds+exceptfds)로 타임아웃을 걸고 다시 블로킹으로 복원. IOCP 등록과 최초 수신 등록까지 성공해야 `true`, 중간 실패 시 `Disconnect()` 후 `false` | `strIP` | 접속할 Redis 서버 IP 주소 |
| | | `nPort` | 접속할 Redis 서버 포트 번호 |
| | | `nConnectTimeoutMs` | connect 완료 대기 최대 시간(ms). 기본값 3000 |
| `bool Disconnect()` | 소켓을 닫고, 대기 중이던 콜백들을 락 밖으로 꺼내 에러 값으로 즉시 완료 처리(데드락 방지를 위해 큐 비우기와 콜백 실행을 분리). 이 완료 처리 덕분에 풀의 래핑 콜백이 정상 동작해 커넥션이 반납됨. `_parser.Reset()`으로 마무리 | 없음 | - |
| `bool IsConnected() const` | `_socket != INVALID_SOCKET` 여부 반환 | 없음 | - |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)` | `CommandBuilder`로 송신 버퍼에 RESP를 직접 인코딩(중간 복사 없음). 콜백을 큐에 등록 후 `DoSend()`로 전송 시작. 등록 실패 시 콜백을 되돌리고 `false` | `vecArgs` | Redis 명령어 및 인자 목록 |
| | | `fnCallback` | 응답(또는 연결 종료로 인한 에러) 완료 시 호출될 콜백 |
| `bool RegisterRecv()` (private) | `_recvBuffer.GetWSARecvBuffers()`로 얻은 최대 2개 청크에 대해 `WSARecv`를 IOCP에 등록 | 없음 | - |
| `void ProcessRecv(DWORD dwBytesTransferred)` (private) | 수신 커서 이동 후 새 바이트를 `_parser.Feed()`로 넘기고 즉시 읽기 커서 이동(중복 투입 방지). `_parser.TryParse()`를 반복해 완성된 값을 모두 `OnReceiptResponse()`로 전달. 마지막에 `RegisterRecv()` 재호출, 실패 시 `Disconnect()` | `dwBytesTransferred` | 이번 IOCP 완료로 수신된 바이트 수 |
| `void OnReceiptResponse(const RedisValue& value)` (private) | `_pendingCallbacks`에서 가장 오래된 콜백 하나를 꺼내 `value`와 함께 호출 | `value` | 파싱 완료된 Redis 응답 데이터 |
| `bool DoSend()` (private) | `_pendingSendBuffer`의 `_sendOffset` 위치부터 나머지를 `WSASend`로 등록. 완료 시 요청 바이트를 전부 보냈다는 보장이 없으므로(부분 전송), `Dispatch()`가 필요 시 반복 호출해 이어 보냄 | 없음 | 멤버 `_pendingSendBuffer`/`_sendOffset`/`_sendTotalSize`를 사용 |

---

## 4. CRedisConnectionPool

**역할**: 여러 개의 `CRedisClient`를 생성/분배/회수하는 커넥션 풀. `std::enable_shared_from_this`를 상속해
콜백 래핑 시 자기 자신을 약한 참조로 캡처한다.

**격리 큐 + 백그라운드 재연결**: 커넥션 반납 시 연결 상태를 확인해, 살아있으면 Free 큐로, 끊어져 있으면 격리 큐
(`_queueBroken`)로 보낸다. 전용 재연결 스레드가 일정 주기로 격리 큐를 훑어 재연결을 시도하고, 성공한 커넥션만
다시 Free 큐로 돌려보낸다 — 죽은 커넥션이 Free 큐에 섞여 매 요청을 실패시키는 것을 막기 위함이다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_iocpCore` | `CIocpCoreRef` | IOCP 코어 참조 |
| `_strIP` | `std::string` | 연결 대상 IP |
| `_nPort` | `uint16` | 연결 대상 포트 |
| `_lock` | `std::mutex` | `_vecAllClients`/`_queueFree`/`_queueBroken` 공용 동기화 락 |
| `_vecAllClients` | `CVector<CRedisClientRef>` | 풀이 생성한 전체 클라이언트 목록(생존 관리 목적) |
| `_queueFree` | `CQueue<CRedisClientRef>` | 대여 가능한(연결된) 클라이언트 큐 |
| `_queueBroken` | `CQueue<CRedisClientRef>` | 연결이 끊겨 재연결을 기다리는 격리 큐 |
| `_bInitialized` | `std::atomic<bool>` | 풀 초기화 여부 |
| `_nReconnectIntervalSec` | `int32` | 격리 큐를 훑어 재연결을 시도하는 주기(초). 기본 3 |
| `_reconnectThread` | `std::thread` | 재연결 전용 백그라운드 스레드 |
| `_reconnectLock` | `std::mutex` | `_reconnectCv` 대기/신호 전달 전용 뮤텍스(재연결 시도 자체는 이 락 밖에서 수행) |
| `_reconnectCv` | `std::condition_variable` | 재연결 주기 대기 및 즉시 종료 신호 수신용 |
| `_stopping` | `std::atomic<bool>` | 재연결 스레드 정지 요청 플래그 |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisConnectionPool(CIocpCoreRef iocpCore)` | 생성자 | `iocpCore` | IOCP 코어 참조 객체 |
| `~CRedisConnectionPool()` | 소멸자. `Clear()` 호출 | 없음 | - |
| `bool Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize, const int32 nReconnectIntervalSec = 3)` | `nPoolSize`개의 클라이언트를 생성·연결해 `_vecAllClients`/`_queueFree`에 채움. 중간 실패 시 `Clear()` 후 `false`. 전부 성공하면 초기화 완료 표시 후 재연결 스레드 시작 | `strIP` | 연결 대상 서버 IP |
| | | `nPort` | 연결 대상 서버 포트 |
| | | `nPoolSize` | 생성할 커넥션 개수 |
| | | `nReconnectIntervalSec` | 재연결 시도 주기(초). `0` 이하면 기본값 3으로 보정 |
| `void Clear()` | 재연결 스레드를 먼저 정지·조인(락을 쥔 채 조인을 기다리는 데드락 방지를 위해 락 밖에서 호출)한 뒤, 두 큐를 비우고 모든 클라이언트를 `Disconnect()`, 전체 목록/초기화 플래그 정리 | 없음 | - |
| `CRedisClientRef PopConnection()` (private) | `_queueFree`에서 하나를 꺼내 반환, 비어있으면 `nullptr` | 없음 | - |
| `void PushConnection(CRedisClientRef pClient)` (private) | 반납된 커넥션의 `IsConnected()`를 확인해 Free 큐 또는 격리 큐로 분배 | `pClient` | 반납할 클라이언트. `nullptr`이면 아무 동작도 하지 않음 |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)` | `PopConnection()`으로 대여해 명령 전송. 콜백을 원본 실행 후 `weak_ptr`로 캡처한 풀 자신을 잠가 `PushConnection()`으로 반납하도록 래핑. 전송 등록 실패 시 즉시 반납 후 `false` | `vecArgs` | Redis 명령어 및 인자 목록 |
| | | `fnCallback` | 응답 처리 콜백 함수 |
| `void StartReconnectLoop()` (private) | `_stopping`을 `false`로 리셋하고 `ReconnectLoop()` 스레드 시작 | 없음 | - |
| `void StopReconnectLoop()` (private) | `_stopping`을 `true`로 CAS(중복 호출 안전) 후 `notify_all()`로 깨우고 스레드 조인 | 없음 | - |
| `void ReconnectLoop()` (private) | `_reconnectLock`으로 감싼 `wait_for`로 주기 대기(정지 신호 시 즉시 깨어남). 깨어나면 락을 놓은 채로 격리 큐를 스냅샷해 순서대로 `Connect()` 시도(대량 재연결 중에도 즉시 정지 가능하도록 락 밖에서 수행). 성공하면 Free 큐로, 실패/정지 시 격리 큐로 되돌림 | 없음 | - |

---

## 5. CRedisResultSet

**역할**: `RedisValue` 응답 객체를 문자열 목록으로 평탄화한 뒤, 순차 커서(cursor) 방식으로 원하는 기본 타입으로
하나씩 꺼내주는 유틸리티. Boost 없이 표준 라이브러리(`std::stoll` 등)만으로 변환한다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_readCursor` | `UINT` | 순차 데이터 읽기 커서 위치 |
| `_vecResultSplit` | `CVector<std::string>` | 문자열로 분할·보관된 결과 목록 |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `explicit CRedisResultSet(const RedisValue& value)` | `value`가 Null이면 빈 상태로 둠. SimpleString/BulkString은 그대로, Integer는 `to_string`, Array는 각 원소를 (Null→빈 문자열, Integer→`to_string`, 그 외→원문) 규칙으로 적재 | `value` | 파싱 완료된 Redis 응답 객체(`CRedisParser`의 결과) |
| `~CRedisResultSet() = default` | 소멸자 | 없음 | - |
| `bool GetData(INT8& Dest)` | 커서 위치 문자열을 `INT8`로 변환(`std::stoi`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(UINT8& Dest)` | 커서 위치 문자열을 `UINT8`로 변환(`std::stoul`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(INT16& Dest)` | 커서 위치 문자열을 `INT16`으로 변환(`std::stoi`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(INT32& Dest)` | 커서 위치 문자열을 `INT32`로 변환(`std::stoi`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(UINT32& Dest)` | 커서 위치 문자열을 `UINT32`로 변환(`std::stoul`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(INT64& Dest)` | 커서 위치 문자열을 `INT64`로 변환(`std::stoll`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(UINT64& Dest)` | 커서 위치 문자열을 `UINT64`로 변환(`std::stoull`)해 꺼내고 커서 1 전진. 범위 초과/변환 실패 시 `false`, `Dest`는 0 | `Dest` | (출력) 추출한 값을 받을 변수 참조 |
| `bool GetData(std::string& Dest)` | 커서 위치 문자열을 그대로 꺼내고 커서 전진 | `Dest` | (출력) 추출한 문자열을 받을 변수 참조 |
| `bool GetData(TCHAR* Dest, int nSize)` | `Dest`를 0으로 채운 뒤 커서 위치 문자열을 (UNICODE 빌드면 `MultiByteToWideChar`, 아니면 `strncpy_s`로) 복사. 범위 초과 시 버퍼는 비운 채 `false` | `Dest` | (출력) 문자열을 저장할 TCHAR 버퍼 포인터. `nullptr`이면 즉시 `false` |
| | | `nSize` | `Dest` 버퍼의 TCHAR 요소 개수. `0` 이하면 즉시 `false` |
| `INT32 GetRankInfoRetCount() const` | (Member, Score) 쌍 랭킹류 응답의 튜플 개수(`size/2`) 반환. 정상 응답은 항상 짝수 요소이므로 홀수면 `ASSERT_CRASH`로 개발 빌드에서 즉시 노출 | 없음 | - |
| `bool IsEmpty() const` | `_vecResultSplit`이 비었는지 여부 | 없음 | - |
| `size_t GetSize() const` | `_vecResultSplit.size()` 반환 | 없음 | - |

---

## 6. CRedisService

**역할**: 외부 모듈에 노출되는 최상위 파사드. `CRedisConnectionPool`을 내부에 두고, IOCP 수신 스레드에서 받은
응답 콜백을 지정된 `CJobQueue`로 이관해 **로직/메인 스레드에서 별도 동기화 없이 안전하게** 결과를 받을 수 있게 한다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_pool` | `CRedisConnectionPoolRef` | 내부 커넥션 풀 |
| `_jobQueue` | `CJobQueueRef` | 콜백을 이관할 대상 스레드의 작업 큐 |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisService(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue)` | 생성자. `_pool`을 `make_shared`로 생성 | `iocpCore` | IOCP 코어 참조 객체 |
| | | `pJobQueue` | 결과를 전달받을 메인/대상 스레드의 작업 큐 |
| `~CRedisService()` | 소멸자(별도 처리 없음, `_pool`의 소멸자가 정리) | 없음 | - |
| `bool Init(const std::string& strIP, const uint16 nPort, const int32 nPoolSize)` | `_pool->Init()`에 위임 | `strIP` | 연결 대상 서버 IP |
| | | `nPort` | 연결 대상 서버 포트 |
| | | `nPoolSize` | 커넥션 풀 개수 |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)` | IOCP 스레드에서 실행될 내부 콜백을 만들어 `_pool->SendCommand()`에 전달. 내부 콜백은 `_jobQueue`를 `weak_ptr`로 잠가 `DoAsync()`로 대상 스레드 큐에 콜백 실행을 예약 | `vecArgs` | Redis 명령어 인자 목록 |
| | | `fnMainThreadCallback` | 대상 스레드에서 안전하게 실행될 콜백 함수 |

---

## 7. CRedisServerHeartbeat

**역할**: 서버 프로세스가 시작될 때 자신의 실행 정보를 Redis Hash로 등록(`HSET`)하고, 주기적으로 TTL을
`EXPIRE`로 갱신해 "살아있음"을 알리는 디스커버리용 클래스. TTL이 heartbeat 주기보다 충분히 길게 설정되어,
프로세스가 비정상 종료되면 heartbeat가 끊기고 TTL 만료로 Redis에서 해당 키가 자동 소멸한다(별도 정리 배치 불필요).
정상 종료 시에는 `Stop()`이 `DEL`로 키를 즉시 지워 TTL 만료를 기다리지 않는다.

등록 키 형식: `"Server:{serverType}:{serverId}"` (Redis Hash, 필드: `serverType`, `serverId`, `port`, `pid`,
`startedAt`, `updatedAt`).

**스레드 모델**: 전용 스레드 하나가 `condition_variable::wait_for`로 heartbeat 주기만큼 대기 후 깨어나
`EXPIRE`를 게시한다. `CRedisService::SendCommand()`의 콜백은 IOCP 워커 스레드에서 `CJobQueue`로 이관되어
실행되므로, 이 클래스 내부 상태는 heartbeat 스레드/콜백 양쪽에서 손대지 않는다(콜백은 결과를 읽기만 함) —
별도 락 없이 안전.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_redisService` | `CRedisService*` | 명령 전송에 사용할 Redis 서비스 포인터(`nullptr`면 `Start()`가 실패) |
| `_serverType` | `std::string` | 서버 종류 식별자(예: "GameServer") |
| `_serverId` | `std::string` | 서버 인스턴스 식별자 |
| `_port` | `uint16` | 클라이언트/내부 연동이 접속할 포트(등록 정보용) |
| `_ttlSec` | `int32` | 살아있음으로 간주할 TTL(초). 기본 15 |
| `_heartbeatIntervalSec` | `int32` | heartbeat(`EXPIRE` 갱신) 주기(초). 기본 5, `_ttlSec`보다 반드시 작아야 함 |
| `_thread` | `std::thread` | heartbeat 전용 스레드 |
| `_lock` | `std::mutex` | `_cv` 대기/신호 전달 전용 뮤텍스 |
| `_cv` | `std::condition_variable` | heartbeat 주기 대기 및 즉시 종료 신호 수신용 |
| `_stopping` | `std::atomic<bool>` | 스레드 정지 요청 플래그 |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisServerHeartbeat(CRedisService* redisService, std::string serverType, std::string serverId, uint16 port)` | 생성자. 각 필드를 이동 저장 | `redisService` | 명령 전송에 사용할 Redis 서비스. `nullptr`면 `Start()`가 실패 |
| | | `serverType` | 서버 종류 식별자(예: `"GameServer"`, `"LoginServer"`) |
| | | `serverId` | 서버 인스턴스 식별자(예: 서버 번호, 호스트명 등) |
| | | `port` | 클라이언트/내부 연동이 접속할 포트(등록 정보용) |
| `~CRedisServerHeartbeat()` | 소멸자. 아직 실행 중이면 `Stop()`으로 정리 | 없음 | - |
| `bool Start(int32 ttlSec = 15, int32 heartbeatIntervalSec = 5)` | `_redisService`가 `nullptr`이거나 `heartbeatIntervalSec<=0` 또는 `ttlSec<=heartbeatIntervalSec`이면 `ASSERT_CRASH` 후 `false`. 유효하면 저장 후 `RegisterInitial()` 호출, `HeartbeatLoop()` 스레드 시작 | `ttlSec` | 살아있음으로 간주할 TTL(초) |
| | | `heartbeatIntervalSec` | EXPIRE 갱신 주기(초). `ttlSec`보다 충분히 작아야 함(같거나 크면 갱신 전 TTL 만료 창이 생김) |
| `void Stop()` | `_stopping`을 `true`로 CAS(중복 호출 안전) 후 `notify_all()`로 깨우고 스레드 조인. 이후 등록 키를 `DEL`(fire-and-forget) | 없음 | - |
| `void RegisterInitial()` (private) | `HSET`으로 `serverType/serverId/port/pid/startedAt/updatedAt` 등록. 완료 콜백 안에서 `EXPIRE` 게시("내용 없는" 키 방지, 콜백 도착 자체를 완료로 간주) | 없음 | - |
| `void SendHeartbeat()` (private) | `updatedAt` `HSET` + TTL `EXPIRE` 갱신을 각각 게시. 두 커맨드는 별도 RTT라 완전한 원자성은 없지만 짧은 주기 반복으로 다음 틱에 자연 복구 | 없음 | - |
| `void HeartbeatLoop()` (private) | `_cv.wait_for`로 heartbeat 주기만큼 대기(`Stop()` notify 시 즉시 깨어남). 타임아웃으로 깨어난 경우에만 `SendHeartbeat()` 게시 | 없음 | - |
| `std::string BuildKey() const` (private) | `"Server:" + _serverType + ":" + _serverId"` 반환 | 없음 | - |
