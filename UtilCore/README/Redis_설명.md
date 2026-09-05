# Redis 모듈 설명

IOCP 기반 서버 프레임워크 위에서 동작하는 비동기 Redis 클라이언트 모듈이다.
소켓 하나당 커맨드 하나씩 순차 처리하는 `CRedisClient`를 여러 개 묶어 `CRedisConnectionPool`이 관리하고,
그 위에 `CRedisService`가 노드별 풀 라우팅과 스레드 안전한 콜백 이관(JobQueue)을 얹은 최상위 파사드로
노출된다. RESP(REdis Serialization Protocol) 인코딩/디코딩은 `CRedisCommandBuilder`(요청)와
`CRedisParser`(응답)가 전담하고, 파싱된 응답은 `CRedisResultSet`을 통해 타입 안전하게 꺼내 쓴다.
`CRedisServerHeartbeat`는 이 스택 위에서 서버 자신의 생존 여부를 Redis에 등록/갱신하는 디스커버리용
보조 클래스다.

접속 직후 논리 DB를 고정하는 `SELECT`(nDbIndex), 끊긴 커넥션에 대한 지수 백오프 재연결, 여러 Redis
노드에 대한 라우팅(`CRedisService`가 노드 ID별로 풀을 따로 관리)까지 지원한다.

```
CRedisService (파사드, 노드별 풀 라우팅, 스레드 이관)
    └─ CRedisConnectionPool × N (노드 하나당 하나, 커넥션 대여/반납, 격리 큐 + 지수 백오프 재연결)
            └─ CRedisClient × N (소켓 1개당 1개, IOCP 비동기 송수신, 접속 시 SELECT로 DB 고정)
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

**안전성**: BulkString(`$`)/Array(`*`)의 길이·개수 필드는 파싱 전에 버퍼 크기(`nSize`) 이하인지 먼저 검증한다.
이 상한 검사 없이 곧장 `길이 + 2` 같은 산술을 했다면, 공격자가 `INT64_MAX`에 가까운 길이를 선언했을 때
정수 오버플로로 값이 음수로 뒤집혀 크기 검사를 무력화하고 실제 버퍼보다 훨씬 큰 범위를 읽어버리는
힙 오버리드로 이어질 수 있었다 — 상한을 먼저 못박아 이 오버플로 자체가 발생할 수 없게 막는다.

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
| `bool ParseValue(...)` (private) | 접두 문자(`+ - : $ *`)를 보고 재귀적으로 값을 해석. 실패 시 `nOffset`을 호출 전 위치로 되돌림. `$`/`*`의 길이·개수 값은 먼저 `-1`(Null) 여부와 `nSize` 이하 여부를 검증해(정수 오버플로 방지) 규격 위반/버퍼 초과 시 실패 처리. `std::stoll` 예외는 내부에서 캐치해 파싱 실패로 변환 | `pBuffer` | 파싱 대상 버퍼 포인터(`_pendingBuffer.data()`) |
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
접속 직후 `SELECT`로 논리 DB를 고정할 수 있다.

**스레드 계약**: 한 시점에 이 객체에 대해 미완료 `SendCommand()`가 동시에 하나만 존재한다고 가정한다(`_sendEvent`와
전송 오프셋을 객체당 하나만 두고 재사용하기 때문). `CRedisConnectionPool`은 커넥션을 대여한 뒤 응답을 받을 때까지
다시 대여되지 않도록 보장하는 방식으로 이 계약을 지킨다. 풀을 거치지 않고 이 클래스를 직접 여러 스레드에서
동시에 호출하지 않도록 한다.

다만 `Disconnect()`만은 예외적으로 여러 스레드에서 동시에 호출될 수 있다고 가정한다 — `_recvEvent`와
`_sendEvent`가 서로 다른 IOCP 워커 스레드에서 완료 통지를 받을 수 있고, 연결이 끊기는 상황에서는 둘 다 거의
동시에 `numOfBytes==0`으로 완료되어 `Dispatch()`가 두 스레드에서 동시에 `Disconnect()`를 호출할 수 있기
때문이다. `_socket`을 `std::atomic<SOCKET>`으로 두고 `Disconnect()`가 `exchange()`로 원자적으로 회수해,
이 경우에도 소켓이 정확히 한 번만 close되고 콜백 드레인·파서 리셋도 단 한 번만 수행되도록 보장한다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_socket` | `std::atomic<SOCKET>` | 소켓 핸들. 미연결 시 `INVALID_SOCKET`. 동시 `Disconnect()` 호출 시 정확히 한 번만 close되도록 atomic exchange로 관리 |
| `_iocpCore` | `CIocpCoreRef` | IOCP 코어 참조 |
| `_recvBuffer` | `CRingBuffer` | 수신 링 버퍼. 소켓에서 갓 도착한 바이트를 IOCP 완료 시점까지만 보관 |
| `_recvEvent` | `RecvEvent` | Recv용 Overlapped 이벤트 |
| `_sendEvent` | `SendEvent` | Send용 Overlapped 이벤트 |
| `_pendingSendBuffer` | `CSendBufferRef` | 현재 전송 중인 명령의 송신 버퍼. 부분 전송 시 이어 보내기 위해 유지 |
| `_sendOffset` | `uint32` | `_pendingSendBuffer` 중 이미 전송 완료된 바이트 수 |
| `_sendTotalSize` | `uint32` | `_pendingSendBuffer`의 전체 바이트 수 |
| `_parser` | `CRedisParser` | RESP 파서 (미완성 패킷 재조립의 유일한 소유자) |
| `_queueLock` | `std::mutex` | `_pendingCallbacks` 동기화 락 |
| `_pendingCallbacks` | `CQueue<RedisCallback>` | 전송했지만 아직 응답을 받지 못한 명령들의 콜백 큐(FIFO, Redis 파이프라이닝의 응답 순서 보장을 전제) |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisClient(CIocpCoreRef iocpCore)` | 생성자. 소켓을 `INVALID_SOCKET`으로, 수신 버퍼를 `DATABASE_BUFFER_SIZE`로 초기화 | `iocpCore` | IOCP 코어 참조 객체 |
| `~CRedisClient()` | 소멸자. `Disconnect()` 호출 | 없음 | - |
| `HANDLE GetHandle() override` | IOCP 바인딩용 소켓 핸들 반환(`_socket`을 원자적으로 읽음) | 없음 | - |
| `CIocpObjectRef GetIocpObjectPtr() override` | Aliasing Constructor로 `CIocpObjectRef`를 생성해 반환(제어 블록은 `CRedisClient`와 공유) | 없음 | - |
| `void Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes = 0) override` | IOCP 완료 통지 처리. `_recvEvent`면 `numOfBytes==0`일 때 `Disconnect()`, 아니면 `ProcessRecv()`. `_sendEvent`면 `numOfBytes==0`일 때도 `Disconnect()`; 그 외엔 `_sendOffset`을 누적해 부분 전송이면 `DoSend()`로 이어 보내고(재등록 실패 시 `Disconnect()`), 다 보냈으면 송신 상태 초기화 | `iocpEvent` | 완료된 IOCP 이벤트 포인터(`&_recvEvent`/`&_sendEvent`와 비교해 판별) |
| | | `numOfBytes` | 이번 완료로 전송/수신된 바이트 수(`0`이면 연결 끊김으로 간주) |
| `bool Connect(const std::string& strIP, const uint16 nPort, const int32 nDbIndex = 0, const int32 nConnectTimeoutMs = 3000)` | TCP 소켓 생성 후 연결. `connect()`가 동기 호출이므로 소켓을 일시 논블로킹 전환 후 `select()`(writefds+exceptfds)로 타임아웃을 걸고 다시 블로킹으로 복원. 연결 성공 시에만 `_socket`에 값을 채워 넣는다. IOCP 등록과 최초 수신 등록까지 성공해야 하며, `nDbIndex > 0`이면 이어서 `SelectDb()`로 논리 DB까지 고정해야 최종 `true`. 중간 어느 단계든 실패 시 `Disconnect()` 후 `false` | `strIP` | 접속할 Redis 서버 IP 주소 |
| | | `nPort` | 접속할 Redis 서버 포트 번호 |
| | | `nDbIndex` | 접속 직후 `SELECT`로 고정할 논리 DB 인덱스. `0`(기본 DB)이면 `SELECT`를 생략해 접속 지연을 줄임. 재연결 시에도 동일 인자로 다시 호출되므로 재연결된 커넥션도 항상 같은 DB를 바라봄 |
| | | `nConnectTimeoutMs` | connect 완료 및 (nDbIndex>0인 경우) `SELECT` 응답 대기 최대 시간(ms). 기본값 3000 |
| `bool Disconnect()` | `_socket`을 atomic exchange로 원자적으로 회수한다. 실제로 유효한 핸들을 받아온 단 하나의 호출만 소켓 close·콜백 드레인·파서 리셋을 수행하고, 동시에 호출된 나머지는 이미 `INVALID_SOCKET`을 보고 즉시 반환(소켓 이중 close 및 `_parser`/큐 동시 수정을 원천 차단). 대기 중이던 콜백들은 락 밖으로 꺼내 에러 값으로 즉시 완료 처리 — 이 덕분에 풀의 래핑 콜백이 정상 동작해 커넥션이 반납됨 | 없음 | - |
| `bool IsConnected() const` | `_socket`(원자적 읽기)이 `INVALID_SOCKET`이 아닌지 반환 | 없음 | - |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)` | `CommandBuilder`로 송신 버퍼에 RESP를 직접 인코딩(중간 복사 없음). 콜백을 큐에 등록 후 `DoSend()`로 전송 시작. 등록 실패 시 콜백을 되돌리고 `false` | `vecArgs` | Redis 명령어 및 인자 목록 |
| | | `fnCallback` | 응답(또는 연결 종료로 인한 에러) 완료 시 호출될 콜백 |
| `bool RegisterRecv()` (private) | `_recvBuffer.GetWSARecvBuffers()`로 얻은 최대 2개 청크에 대해 `WSARecv`를 IOCP에 등록 | 없음 | - |
| `void ProcessRecv(DWORD dwBytesTransferred)` (private) | 수신 커서 이동 후 새 바이트를 `_parser.Feed()`로 넘기고 즉시 읽기 커서 이동(중복 투입 방지). `_parser.TryParse()`를 반복해 완성된 값을 모두 `OnReceiptResponse()`로 전달. 마지막에 `RegisterRecv()` 재호출, 실패 시 `Disconnect()` | `dwBytesTransferred` | 이번 IOCP 완료로 수신된 바이트 수 |
| `void OnReceiptResponse(const RedisValue& value)` (private) | `_pendingCallbacks`에서 가장 오래된 콜백 하나를 꺼내 `value`와 함께 호출 | `value` | 파싱 완료된 Redis 응답 데이터 |
| `bool DoSend()` (private) | `_pendingSendBuffer`의 `_sendOffset` 위치부터 나머지를 `WSASend`로 등록. 완료 시 요청 바이트를 전부 보냈다는 보장이 없으므로(부분 전송), `Dispatch()`가 필요 시 반복 호출해 이어 보냄 | 없음 | 멤버 `_pendingSendBuffer`/`_sendOffset`/`_sendTotalSize`를 사용 |
| `bool SelectDb(const int32 nDbIndex, const int32 nTimeoutMs)` (private) | 접속 직후 이 커넥션이 사용할 논리 DB를 `SELECT` 명령으로 고정. `SendCommand()`는 비동기이므로, `Connect()`가 "연결 및 DB 선택까지 끝난 뒤"의 결과를 그대로 돌려줄 수 있도록 내부적으로 `condition_variable`로 응답을 기다려 동기 호출처럼 동작시킨다(IOCP 완료 통지는 별도 워커 스레드에서 오므로 이 대기가 스스로를 막는 데드락은 생기지 않음) | `nDbIndex` | 선택할 Redis 논리 DB 인덱스 |
| | | `nTimeoutMs` | 응답을 기다릴 최대 시간(ms) |

---

## 4. CRedisConnectionPool

**역할**: 여러 개의 `CRedisClient`를 생성/분배/회수하는 커넥션 풀. `std::enable_shared_from_this`를 상속해
콜백 래핑 시 자기 자신을 약한 참조로 캡처한다. 하나의 풀은 서버 1대(`strIP`/`nPort`)에 대응하며,
여러 노드에 대한 라우팅은 상위의 `CRedisService`가 풀을 노드마다 하나씩 두는 방식으로 처리한다.

**격리 큐 + 백그라운드 재연결(지수 백오프)**: 커넥션 반납 시 연결 상태를 확인해, 살아있으면 Free 큐로,
끊어져 있으면 격리 큐(`_queueBroken`)로 보낸다. 전용 재연결 스레드가 일정 주기(`_nReconnectIntervalSec`)로
격리 큐를 훑어 재연결을 시도하고, 성공한 커넥션만 다시 Free 큐로 돌려보낸다 — 죽은 커넥션이 Free 큐에
섞여 매 요청을 실패시키는 것을 막기 위함이다. 재연결 시도는 클라이언트별로 지수 백오프(+지터)를 적용한다
(500ms에서 시작해 최대 30초까지) — 연속으로 실패할수록 다음 시도까지의 대기 시간이 늘어나, 서버가 오래
죽어있는 동안 스윕 주기마다 무의미하게 계속 두드리는 것을 막는다. 재연결 시도 자체(블로킹 `Connect()`)는
항상 `_lock` 밖에서 수행해, 대량 재연결이 진행 중이거나 풀을 채우는 도중에도 다른 스레드의
`PopConnection()`/`PushConnection()`/`SendCommand()`가 막히지 않게 한다.

**재진입 안전성**: `Init()`은 이미 초기화된 풀에 다시 호출해도 안전하다 — 맨 앞에서 항상 `Clear()`를 먼저
호출해 기존 재연결 스레드/커넥션을 완전히 정리한 뒤 처음부터 다시 채운다. 이 선행 정리가 없으면, 이미
실행 중인 `_reconnectThread` 위에 새 `std::thread`를 그대로 대입하려다 `std::terminate()`로 죽는다
(joinable한 `std::thread`에 대한 이동 대입은 표준상 종료를 유발하기 때문).

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_iocpCore` | `CIocpCoreRef` | IOCP 코어 참조 |
| `_strIP` | `std::string` | 연결 대상 IP |
| `_nPort` | `uint16` | 연결 대상 포트 |
| `_nDbIndex` | `int32` | 각 커넥션이 접속 직후 `SELECT`로 고정할 논리 DB 인덱스(0이면 생략). 재연결된 커넥션에도 동일하게 적용 |
| `_lock` | `std::mutex` | `_vecAllClients`/`_queueFree`/`_queueBroken` 공용 동기화 락 |
| `_vecAllClients` | `CVector<CRedisClientRef>` | 풀이 생성한 전체 클라이언트 목록(생존 관리 목적) |
| `_queueFree` | `CQueue<CRedisClientRef>` | 대여 가능한(연결된) 클라이언트 큐 |
| `_queueBroken` | `CQueue<CRedisClientRef>` | 연결이 끊겨 재연결을 기다리는 격리 큐 |
| `_bInitialized` | `std::atomic<bool>` | 풀 초기화 여부 |
| `_nReconnectIntervalSec` | `int32` | 재연결 스레드가 격리 큐를 훑는 주기(초). 기본 3 |
| `_reconnectThread` | `std::thread` | 재연결 전용 백그라운드 스레드 |
| `_reconnectLock` | `std::mutex` | `_reconnectCv` 대기/신호 전달 전용 뮤텍스(재연결 시도 자체는 이 락 밖에서 수행) |
| `_reconnectCv` | `std::condition_variable` | 재연결 주기 대기 및 즉시 종료 신호 수신용 |
| `_stopping` | `std::atomic<bool>` | 재연결 스레드 정지 요청 플래그 |
| `_reconnectBackoffMap` | `std::unordered_map<CRedisClient*, TReconnectBackoffState>` | 클라이언트별 재연결 지수 백오프 상태(실패 횟수/다음 재시도 시각). 재연결 스레드만 접근하므로 별도 락 불필요 |

**중첩 구조체 `TReconnectBackoffState`**: `nFailCount`(연속 재연결 실패 횟수), `nextRetryTime`(이 시각 이전에는 재시도를 건너뜀).

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisConnectionPool(CIocpCoreRef iocpCore)` | 생성자 | `iocpCore` | IOCP 코어 참조 객체 |
| `~CRedisConnectionPool()` | 소멸자. `Clear()` 호출 | 없음 | - |
| `bool Init(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nPoolSize, const int32 nReconnectIntervalSec = 3)` | 맨 앞에서 항상 `Clear()`를 먼저 호출해 재진입에 안전하게 만든 뒤, `strIP`/`_nPort`/`_nDbIndex`/`_nReconnectIntervalSec`를 짧게 락으로 감싸 반영하고, `nPoolSize`개의 클라이언트를 락 밖에서 `Connect()`(블로킹)로 연결해 `_vecAllClients`/`_queueFree`에 채운다. 중간 실패 시 `Clear()` 후 `false`. 전부 성공하면 초기화 완료 표시 후 재연결 스레드 시작 | `strIP` | 연결 대상 서버 IP |
| | | `nPort` | 연결 대상 서버 포트 |
| | | `nDbIndex` | 각 커넥션이 접속 직후 `SELECT`로 고정할 논리 DB 인덱스 |
| | | `nPoolSize` | 생성할 커넥션 개수 |
| | | `nReconnectIntervalSec` | 재연결 시도 주기(초). `0` 이하면 기본값 3으로 보정 |
| `void Clear()` | 재연결 스레드를 먼저 정지·조인(락을 쥔 채 조인을 기다리는 데드락 방지를 위해 락 밖에서 호출)한 뒤, 두 큐와 `_reconnectBackoffMap`을 비우고 모든 클라이언트를 `Disconnect()`, 전체 목록/초기화 플래그 정리 | 없음 | - |
| `CRedisClientRef PopConnection()` (private) | `_queueFree`에서 하나를 꺼내 반환, 비어있으면 `nullptr` | 없음 | - |
| `void PushConnection(CRedisClientRef pClient)` (private) | 반납된 커넥션의 `IsConnected()`를 확인해 Free 큐 또는 격리 큐로 분배 | `pClient` | 반납할 클라이언트. `nullptr`이면 아무 동작도 하지 않음 |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)` | `PopConnection()`으로 대여해 명령 전송. 콜백을 원본 실행 후 `weak_ptr`로 캡처한 풀 자신을 잠가 `PushConnection()`으로 반납하도록 래핑. 전송 등록 실패 시 즉시 반납 후 `false` | `vecArgs` | Redis 명령어 및 인자 목록 |
| | | `fnCallback` | 응답 처리 콜백 함수 |
| `void StartReconnectLoop()` (private) | `_stopping`을 `false`로 리셋하고 `ReconnectLoop()` 스레드 시작 | 없음 | - |
| `void StopReconnectLoop()` (private) | `_stopping`을 `true`로 CAS(중복 호출 안전) 후 `notify_all()`로 깨우고 스레드 조인 | 없음 | - |
| `void ReconnectLoop()` (private) | `_reconnectLock`으로 감싼 `wait_for`로 주기 대기(정지 신호 시 즉시 깨어남). 깨어나면 락을 놓은 채로 격리 큐를 스냅샷해 순서대로 확인 — `_reconnectBackoffMap`상 아직 백오프 대기 중인 클라이언트는 건너뛰고 그대로 되돌리고, 그 외엔 `Connect()` 시도. 성공하면 백오프 상태를 지우고 Free 큐로, 실패하면 실패 횟수를 늘리고 지수 백오프(+지터)로 다음 재시도 시각을 정한 뒤 격리 큐로 되돌림. 정지 신호가 들어오면 남은 클라이언트도 격리 큐로 되돌리고 이번 순회를 즉시 접음 | 없음 | - |

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

**역할**: 외부 모듈에 노출되는 최상위 파사드. Redis 노드 하나당 `CRedisConnectionPool`을 하나씩 두고,
`CRedisNode::_nID`를 키로 `CClusterSpinUnorderedMap`(`_poolMap`)에서 관리한다. IOCP 수신 스레드에서 받은
응답 콜백을 지정된 `CJobQueue`로 이관해 **로직/메인 스레드에서 별도 동기화 없이 안전하게** 결과를 받을 수
있게 한다.

`_poolMap`은 클러스터별 `PRWLock`으로 조회/삽입/전체삭제를 내부적으로 자동 락 처리하므로(`bInnerLock=true`),
`CRedisService`가 별도의 뮤텍스를 두지 않는다. 노드가 하나뿐인 목록으로 초기화한 경우에도 동일하게 그
하나의 풀만 맵에 들어가며, `SendCommand()`는 노드를 지정하지 않아도 항상 첫 번째로 등록된 노드(`_nDefaultNodeId`)로
동작한다.

**재초기화 시 원자성 관련 주의**: `Init()`은 각 노드에 대한 풀을 먼저 전부 만들어본 뒤(하나라도 연결 실패하면
로컬 벡터가 스코프를 벗어나며 그때까지 만든 풀들이 자동 정리되고 즉시 실패 반환) 전부 성공한 경우에만
`_poolMap`에 반영한다. 다만 `_poolMap.ClearObjectMap()`과 각 노드의 `InsertObject()`는 클러스터 단위로 순차
수행되는 별개의 락 구간이라, 이 반영 과정 자체는 원자적이지 않다(트래픽이 흐르는 도중 재초기화를 호출하면
아주 짧은 순간 맵이 비어있거나 일부만 채워진 상태로 보일 수 있음). 보통 트래픽이 시작되기 전 한 번만
호출하는 용도이므로 문제되지 않는다.

### 멤버 변수

| 이름 | 타입 | 설명 |
|---|---|---|
| `_iocpCore` | `CIocpCoreRef` | 노드별 풀을 만들 때 넘겨줄 IOCP 코어 참조 |
| `_jobQueue` | `CJobQueueRef` | 콜백을 이관할 대상 스레드의 작업 큐 |
| `_poolMap` | `CClusterSpinUnorderedMap<int16, CRedisConnectionPoolRef, 4>` | 노드 ID → 커넥션 풀. 클러스터 4개로 락 경합을 분산하며, 조회/삽입/전체삭제가 내부 자동 락(`bInnerLock=true`)으로 스레드 세이프 |
| `_nDefaultNodeId` | `std::atomic<int16>` | `SendCommand()`를 노드 지정 없이 호출했을 때 쓰이는 기본 노드 ID |

### 멤버 함수

| 함수 | 함수 설명 | 파라미터명 | 파라미터 설명 |
|---|---|---|---|
| `CRedisService(CIocpCoreRef iocpCore, CJobQueueRef pJobQueue)` | 생성자. 풀은 아직 만들지 않고 `_iocpCore`만 보관해뒀다가 `Init()` 시점에 필요한 만큼 생성 | `iocpCore` | IOCP 코어 참조 객체 |
| | | `pJobQueue` | 결과를 전달받을 메인/대상 스레드의 작업 큐 |
| `~CRedisService()` | 소멸자(별도 처리 없음, `_poolMap`이 소멸되며 각 풀의 소멸자가 순서대로 정리) | 없음 | - |
| `bool Init(CVector<CRedisNode> redisNodeVec, const int32 nPoolSize)` | `redisNodeVec`의 각 노드마다 별도의 `CRedisConnectionPool`을 만들어 연결까지 마친 뒤(전부 성공해야 함), `_poolMap`을 `ClearObjectMap()`으로 비우고 `CRedisNode::_nID`를 키로 `InsertObject()`. 같은 `_nID`가 중복되면 `InsertObject()`가 실패해 로그만 남기고 건너뜀. 첫 번째 노드(`redisNodeVec[0]`)는 기본 노드로 등록됨 | `redisNodeVec` | 서버 설정에서 읽어온 Redis 노드 목록(비어있으면 실패). 서버 1대만 쓰려면 노드 하나짜리 목록을 넘기면 됨 |
| | | `nPoolSize` | 노드별로 생성할 커넥션 풀 개수(모든 노드에 동일하게 적용) |
| `bool SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)` | 기본 노드(`_nDefaultNodeId`)로 3-파라미터 `SendCommand()`를 위임 호출 | `vecArgs` | Redis 명령어 인자 목록 |
| | | `fnMainThreadCallback` | 대상 스레드에서 안전하게 실행될 콜백 함수 |
| `bool SendCommand(const int16 nNodeId, const CVector<std::string>& vecArgs, RedisCallback fnMainThreadCallback)` | `FindPool(nNodeId)`로 해당 노드의 풀을 찾아 명령을 전송. 못 찾으면 `false`. IOCP 스레드에서 실행될 내부 콜백을 만들어 풀의 `SendCommand()`에 전달하며, 내부 콜백은 `_jobQueue`를 `weak_ptr`로 잠가 `DoAsync()`로 대상 스레드 큐에 콜백 실행을 예약 | `nNodeId` | `Init()`으로 등록된 `CRedisNode::_nID` |
| | | `vecArgs` | Redis 명령어 인자 목록 |
| | | `fnMainThreadCallback` | 대상 스레드에서 안전하게 실행될 콜백 함수 |
| `CRedisConnectionPoolRef FindPool(const int16 nNodeId)` (private) | `_poolMap.FindObject(nNodeId, pPool)`로 노드 ID에 해당하는 풀을 찾아 반환. `[[nodiscard]]` 반환값을 확인해 못 찾으면 `nullptr` | `nNodeId` | 찾을 노드 ID |

**참고(구현 세부, `.cpp` 익명 네임스페이스)**: `TCharToString(const TCHAR* ptsz)` — `CRedisNode::_tszDBHost`(TCHAR 문자열)를 `Init()`에서 `CRedisConnectionPool::Init()`에 넘길 `std::string`으로 변환하는 내부 헬퍼. UNICODE 빌드면 `WideCharToMultiByte`, 아니면 그대로 복사.

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
