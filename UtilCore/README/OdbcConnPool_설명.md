# COdbcConnPool 설계 문서

> 이 문서는 `COdbcConnPool`을 중심으로 작성되었으며, 그 위에서 큐+워커로 동작하는
> 비동기 서비스 계층 `COdbcAsyncSrv`(§11)까지 함께 다룬다.

## 1. 개념

`COdbcConnPool`은 고정 크기의 ODBC 커넥션을 미리 생성해두고, 슬롯 단위로 대여/반납하며,
연결이 끊어진 슬롯을 백그라운드에서 자동으로 재연결하는 락-프리 지향 커넥션 풀이다.

핵심 설계 방향은 다음과 같다.

- **핫패스(커넥션 대여/반납)는 원자 연산 위주로 구성**하여 뮤텍스 경합을 피한다.
- **재연결(블로킹 I/O)은 별도 워커 스레드 풀에 위임**하여 헬스체크 루프나 커넥션 대여 경로를 막지 않는다.
- **낡은 커넥션 삭제는 참조 카운트가 0이 될 때까지 지연**시키고, 그래도 안 되면 격리 큐(quarantine)로
  보내 Use-After-Free를 원천적으로 방지한다.

## 2. 특징

| 특징 | 설명 |
|---|---|
| 슬롯 기반 고정 크기 풀 | `_nMaxPoolSize`로 크기가 고정되며 런타임에 늘어나거나 줄지 않는다 |
| Lock-free 대여/반납 | `GetOdbcConn` / `ReleaseOdbcConn`은 `std::atomic`의 fetch_add/fetch_sub만 사용 |
| 비동기 자동 재연결 | 헬스체크 스레드가 끊어진 슬롯을 감지하고, 별도 워커 풀이 실제 재연결(I/O)을 병렬 수행 |
| 지수 백오프 + 지터 | DB 전체 장애 시 모든 슬롯이 동시에 재시도하는 connection storm을 방지 |
| 동적 워커 수 조정 | `SetReconnectConfig`로 런타임 중 재연결 워커 수/백오프 정책을 조정 가능 |
| 격리(Quarantine) 큐 | 교체된 낡은 커넥션에 참조가 남아있으면 즉시 삭제하지 않고 격리 후 안전할 때 삭제 |
| Safe Leak | 프로세스 종료 시점까지 참조가 남은 커넥션은 삭제를 포기(누수)하여 UAF 크래시를 방지 |
| False sharing 방지 | 슬롯별 원자 배열(`_pOdbcConns`, `_pRefCount`, `_pReconnecting`, `_pNextRetryAllowedMs`, `_pRetryFailCount`)을 `CachePaddedAtomic<T>[]`로, `_slotLocks`를 캐시라인 정렬된 `SpinLockDefault[]`로 구성해 슬롯 간 캐시라인 공유를 차단 |
| 할당자 분리 | `COdbcConnPool` 자신은 `BaseAllocator` 상속으로 RawAllocator 경로를, 내부 `CBaseODBC` 커넥션은 `xnew`/`xdelete`(PoolAllocator)로 별도 관리 (§10 참고) |

## 3. 멤버 변수 설명

### 기본 상태
| 변수 | 설명 |
|---|---|
| `_dbClass` | 사용 중인 DB 종류 (MSSQL, MySQL 등) |
| `_tszDSN` | 접속용 DSN 문자열 |
| `_nMaxPoolSize` | 풀 최대 크기 (생성 시 고정, 변경 불가) |

### 재연결 정책 (필드별 개별 원자 변수)
| 변수 | 설명 |
|---|---|
| `_nBackoffBaseMs` | 최초 재시도 간격 |
| `_nBackoffMaxMs` | 재시도 간격 상한 |
| `_nBackoffMaxShift` | 지수 증가 상한 shift (오버플로 방지 겸 상한 역할) |
| `_nBackoffJitterMs` | 재시도 타이밍 분산을 위한 지터 상한 |

> 구조체 전체를 `std::atomic<TReconnectConfig>`로 감싸면 내부적으로 뮤텍스 폴백이 걸려 lock-free가
> 깨지기 때문에, 서로 독립적으로만 쓰인다는 성질을 이용해 필드별로 쪼개 관리한다.

### 슬롯 배열 (생성자에서 단 1회만 할당되는 불변 배열)
| 변수 | 설명 |
|---|---|
| `_pOdbcConns` | 슬롯별 실제 커넥션 포인터 (`CachePaddedAtomic<CBaseODBC*>[]`) |
| `_pRefCount` | 슬롯별 참조 카운트 (`CachePaddedAtomic<int32>[]`, 가장 핫한 배열) |
| `_slotLocks` | 슬롯별 교체(swap) 보호용 스핀락 배열 |
| `_pReconnecting` | 슬롯별 "재연결 워커가 처리 중" 플래그 (`CachePaddedAtomic<bool>[]`, 중복 디스패치 방지) |
| `_pNextRetryAllowedMs` | 슬롯별 다음 재시도 허용 시각 (`CachePaddedAtomic<int64>[]`, 백오프) |
| `_pRetryFailCount` | 슬롯별 연속 재연결 실패 횟수 (`CachePaddedAtomic<int32>[]`) |

> `_quarantineQueue`가 `&_pRefCount[i].value` 주소를 그대로 저장하므로, 위 배열들은 런타임 중
> 재할당(make_unique 재호출 등)이 절대 금지된다. 각 슬롯이 `CachePaddedAtomic<T>`로 캐시라인
> 하나씩을 점유해, 서로 다른 슬롯을 동시에 다루는 스레드들이 false sharing으로 서로의
> 캐시라인을 무효화시키는 것을 막는다.

### 헬스체크 / 재연결 워커
| 변수 | 설명 |
|---|---|
| `_healthCheckThreadMgr` / `_bStopHealthCheck` / `_nHealthCheckIntervalMs` | 헬스체크 스레드 관리, 종료 신호, 주기(기본 500ms) |
| `_nNextSlotHint` | `PopFreeSlotIndex` 탐색 시작 위치 힌트 (경합 분산용) |
| `_reconnectWorkerMgr` / `_bStopReconnectWorkers` | 재연결 워커 스레드 관리, 전체 종료 신호 |
| `_nCurrentWorkerCount` / `_nDesiredWorkerCount` | 현재 워커 수 / 목표 워커 수 |
| `_reconnectQueueMutex` / `_reconnectQueueCv` / `_reconnectPendingSlots` | 재연결 대기열과 그 동기화 객체 |
| `_globalQuarantineLock` / `_quarantineQueue` | 격리 큐와 이를 보호하는 전역 스핀락 |

## 4. 멤버 함수 설명

### Public API
| 함수 | 설명 |
|---|---|
| `Init(dbClass, dsn, reconnectConfig)` | 풀을 초기화하고 DSN으로 커넥션을 동기적으로 채운다. 잘못된 `reconnectConfig`는 기본값으로 대체된다 |
| `GetOdbcConn(nType)` | 슬롯의 참조 카운트를 증가시키고 커넥션을 반환. 끊어진 슬롯이면 즉시 `nullptr`을 반환하고 카운트를 되돌린다 |
| `ReleaseOdbcConn(nType)` | 참조 카운트를 감소시켜 슬롯을 반납 |
| `GetPooledConnUnsafe(nType)` | `PopFreeSlotIndex`로 이미 선점된 슬롯을 카운트 변경 없이 조회 (`OdbcConnGuard` 전용) |
| `PopFreeSlotIndex()` | 빈 슬롯(참조 카운트 0)을 찾아 즉시 원자적으로 선점하고 인덱스 반환 |
| `SetReconnectConfig(cfg)` | 백오프/워커 수 정책을 런타임에 변경. 유효성 실패 시 전체 거부(부분 적용 없음) |
| `GetReconnectConfig()` | 현재 정책 스냅샷 조회 (모니터링용) |

### Protected 내부 로직
| 함수 | 설명 |
|---|---|
| `Clear()` | 모든 슬롯을 정리. 참조가 남은 슬롯은 격리 큐로 보냄 (Shutdown 전용) |
| `IsValidIndex(nType)` | 슬롯 인덱스 범위 검사 |
| `ValidateReconnectConfig(cfg)` | 재연결 설정값의 상식적 범위 검사 (Init/SetReconnectConfig 공용) |
| `TryReconnect(nType)` | 새 커넥션을 생성하고 실제 연결까지 시도하는 블로킹 I/O 로직 |
| `ApplyReconnectedConn(nType, pNewConn)` | 새 커넥션으로 슬롯을 스왑하고, 낡은 커넥션을 안전하게 삭제 또는 격리 |
| `IsRetryAllowed(nType)` | 백오프 대기 시각이 지났는지 검사 |
| `OnReconnectFailed(nType)` | 실패 횟수를 늘리고 다음 허용 시각을 지수적으로(+지터) 연기 |
| `OnReconnectSucceeded(nType)` | 백오프 상태 초기화 |
| `HealthCheckLoop()` | 격리 큐 청소 + 끊어진 슬롯을 스캔해 재연결 큐에 등록 (블로킹 I/O 없음) |
| `ReconnectWorkerLoop()` | 대기열에서 슬롯을 꺼내 실제 `TryReconnect` + 스왑을 수행하는 워커 루프 |
| `StartReconnectWorkers(n)` / `StopReconnectWorkers()` | 재연결 워커 풀 기동/종료 |
| `SetWorkerCount(n)` | 목표 워커 수 갱신. 확대는 즉시 스폰, 축소는 워커가 스스로 종료하도록 유도 |
| `TryExitIfExcess()` | 현재 워커가 초과 인원인지 CAS로 판정하고, 맞다면 스스로 종료 |
| `EnqueueReconnect(nType)` | 재연결 대기열에 슬롯을 넣고 워커 하나를 깨움 |

## 5. 동작 흐름

### 5.1 커넥션 대여/반납 (핫패스)
1. `OdbcConnGuard` 생성 시 `PopFreeSlotIndex()`로 빈 슬롯을 원자적으로 선점 (참조 카운트 1)
2. `GetPooledConnUnsafe()`로 커넥션 포인터 조회
3. 소멸 시 `ReleaseOdbcConn()`으로 참조 카운트 반납

### 5.2 자동 재연결
1. `HealthCheckLoop()`이 500ms마다 순회하며 참조 카운트 0 & 연결 끊김 & 백오프 통과한 슬롯을 찾음
2. `EnqueueReconnect()`로 대기열에 등록, 워커 하나를 깨움
3. `ReconnectWorkerLoop()`이 `TryReconnect()`로 블로킹 I/O 수행
4. 성공 시 `ApplyReconnectedConn()`으로 슬롯 스왑, 낡은 커넥션은 참조가 빠질 때까지 대기 후 삭제(또는 격리)
5. 실패 시 `OnReconnectFailed()`로 백오프 적용 후 다음 헬스체크 주기에 재시도

### 5.3 워커 수 동적 조정
- 확대: `_nDesiredWorkerCount`를 CAS로 목표까지 끌어올리고 부족분만큼 즉시 스폰
- 축소: 스레드를 직접 종료시키지 않고 조건 변수만 깨움 → 각 워커가 다음 순회에서
  `TryExitIfExcess()`로 스스로 초과 여부 판단 후 종료. 반복/역전 호출에도 최종 목표치로 정확히 수렴

## 6. 장단점

### 장점
- 대여/반납 핫패스가 원자 연산만 사용해 뮤텍스 경합이 없다.
- 재연결 I/O가 별도 워커 풀에서 병렬 처리되어 헬스체크나 대여 경로를 막지 않는다.
- 지수 백오프 + 지터로 DB 장애 시 재연결 폭주(connection storm)를 방지한다.
- 격리 큐와 Safe Leak 정책으로 UAF 크래시 위험을 구조적으로 차단한다.
- 워커 수/백오프 정책을 서비스 운영 중 무중단으로 조정할 수 있다.

### 단점 / 트레이드오프
- 슬롯 배열이 생성자에서 고정 할당되므로, 풀 크기 자체는 런타임에 늘릴 수 없다.
- `GetReconnectConfig()`의 스냅샷은 필드별 개별 로드이므로 완전한 원자적 일관성은 보장하지 않는다
  (모니터링 용도로는 문제 없으나 정합성이 중요한 로직에는 부적합).
- `ApplyReconnectedConn`/`Clear`의 100ms 대기 후 격리 전환 로직은 반환 지연이 긴 호출자가 있을 경우
  일시적으로 메모리를 계속 점유(격리)하게 된다.
- Safe Leak 정책은 크래시를 막는 대신 셧다운 시점에 의도적인 메모리 누수를 허용한다.
- 재연결 워커 축소가 즉시 반영되지 않고 다음 워커 순회 시점에 반영된다 (지연 수렴).

## 7. 재연결 워커 스레드 개수(`nWorkerCount`) 설정 가이드

`nWorkerCount`는 `TReconnectConfig`에서 기본값이 4로 되어 있지만, 실제 서비스 환경에서는
아래 요소들을 고려해 조정하는 것이 좋다.

### 7.1 워커가 하는 일과 비용 특성
- 워커는 대기열이 비어있는 동안은 조건 변수에서 블로킹 대기하므로(`_reconnectQueueCv.wait`),
  유휴 상태에서는 CPU를 소모하지 않는다.
- 실제 비용은 `TryReconnect()`가 수행하는 **네트워크 I/O(연결 수립) 시간** 뿐이다. 따라서
  워커 스레드는 CPU 코어 수보다는 "동시에 재연결이 필요할 수 있는 슬롯 수"와
  "커넥션 1개 수립에 걸리는 시간(RTT + 인증)"을 기준으로 산정해야 한다.

### 7.2 상한 (Upper Bound)
- 워커 수가 `_nMaxPoolSize`를 넘어도 이득이 없다. 동시에 재연결이 필요한 슬롯은 최대
  풀 크기만큼이므로, 그 이상의 워커는 항상 유휴 상태로 대기열만 바라보게 된다.
- DB 서버 자체가 짧은 시간에 대량의 신규 연결/인증 요청을 받으면 오히려 커넥션 수립
  지연이나 인증 스로틀링을 유발할 수 있다. DB 서버의 최대 동시 연결/인증 처리량도
  상한을 정하는 데 함께 고려해야 한다.

### 7.3 하한 (Lower Bound)
- 워커가 너무 적으면, DB 서버 재시작처럼 **풀의 슬롯 대부분이 한꺼번에 끊어지는 상황**에서
  회복이 직렬화되어 느려진다. 예를 들어 풀 크기가 64인데 워커가 4개뿐이라면, 한 번에
  4개 슬롯만 병렬로 재연결되고 나머지는 대기열에서 순서를 기다리게 되어 전체 풀이
  정상화되기까지 시간이 오래 걸린다.
- 이런 "동시 대량 장애" 시나리오를 얼마나 빨리 회복해야 하는지가 최소 워커 수를
  정하는 핵심 기준이다.

### 7.4 산정 가이드라인
| 상황 | 권장 방향 |
|---|---|
| 풀 크기가 작고(수 개~십여 개) 장애가 드묾 | 기본값(4) 정도로 충분 |
| 풀 크기가 크고(수십~수백 개) DB 재시작 등 대량 동시 장애 복구 속도가 중요 | 풀 크기의 10~25% 수준으로 상향 검토 |
| DB 서버의 동시 연결/인증 처리 능력이 제한적 | 워커 수를 낮게 유지하고 백오프(`nBackoffBaseMs`, `nBackoffJitterMs`)로 폭주를 흡수 |
| 네트워크 RTT가 크거나 TLS 핸드셰이크 비용이 큰 환경 | 워커당 재연결 소요 시간이 길어지므로 워커 수를 다소 늘려 병렬성 확보 |

이 값들은 고정된 정답이 없으므로, 운영 환경의 DB 재시작/네트워크 장애 시나리오를
기준으로 실측 후 `SetReconnectConfig()`로 튜닝하는 것을 권장한다.

### 7.5 런타임 조정 시 동작
- **확대**: `_nDesiredWorkerCount`를 CAS로 목표치까지 즉시 갱신하고, 부족한 만큼의
  스레드를 그 자리에서 추가로 스폰한다. 반영이 즉시 이루어진다.
- **축소**: 스레드를 강제 종료하지 않는다. 목표치만 낮추고 조건 변수를 깨우면, 각
  워커가 자신의 다음 순회 시작 시점(`TryExitIfExcess()`)에 스스로 초과 인원인지
  판단해 종료한다. 따라서 축소는 즉시가 아니라 **워커가 다음 순회에 진입하는 시점까지
  지연**될 수 있다 (대기 중이던 워커라면 조건 변수가 깨어나는 즉시 확인하므로 사실상
  빠르게 반영되지만, 재연결 I/O를 수행 중인 워커는 해당 작업을 끝낸 뒤에야 확인한다).
- 짧은 시간 내에 확대/축소가 반복 호출되어도 CAS 기반 조율 덕분에 최종적으로는 가장
  마지막에 설정한 목표치로 정확히 수렴하며, 스레드가 중복 스폰되거나 스테일 종료
  신호로 인해 잘못 죽는 일이 없다.

## 8. IOCP 게임서버에서의 스레드 구성 가이드

`COdbcConnPool`을 IOCP 게임 서버의 DB 처리에 사용할 경우, 서버 전체 스레드를
기능별로 분리하는 것이 좋다. 아래는 역할 구분과, 요즘 많이 쓰이는 코어 구성
기준의 개수 예시다.

### 8.1 기능별 스레드 그룹

| 스레드 그룹 | 역할 | 개수 결정 기준 |
|---|---|---|
| IOCP 워커 | `GetQueuedCompletionStatusEx`로 완료된 Recv/Send I/O를 꺼내 세션에 전달. 순수 네트워크 I/O 처리만 담당 | 물리 코어 수 기준 (I/O 대기 비중에 따라 조정) |
| 게임 로직(콘텐츠) 워커 | JobQueue에서 패킷 처리/게임 로직 Job을 꺼내 실행. IOCP 워커와 분리해 로직 처리 지연이 네트워크 I/O를 막지 않게 함 | 콘텐츠 샤딩 여부에 따라 1개(단일 월드) ~ 샤드 수 |
| DB 비동기 워커 | DB JobQueue에서 쿼리 요청을 꺼내 `COdbcConnPool`에서 커넥션을 빌려 실제 쿼리(블로킹) 실행 후 결과를 완료 큐로 반환 | 예상 동시 DB 요청 수 기준. `_nMaxPoolSize`를 넘지 않는 선에서 결정 |
| `COdbcConnPool` 헬스체크 스레드 | 끊어진 슬롯을 감지해 재연결 대기열에 등록 (논블로킹) | 1개 고정 (클래스 내부에서 자동 생성) |
| `COdbcConnPool` 재연결 워커 스레드 | 실제 재연결 I/O 수행 (`TryReconnect`) | §7 기준 (풀 크기 대비 10~25%, 소규모면 기본값 4) |
| 타이머/틱 스레드 | 게임 틱, 스케줄된 이벤트(리스폰, 버프 만료 등) 처리 | 1개 (로직 워커의 주기 Job으로 흡수 가능) |
| 비동기 로깅 스레드 | 로그를 큐에 쌓고 파일/네트워크로 flush (로직 스레드가 디스크 I/O로 막히지 않게) | 1개 |
| Listener/Accept | 신규 접속 수락 | 별도 생성 불필요, IOCP 워커 중 하나가 AcceptEx 완료도 함께 처리 |

### 8.2 예시 1: 8코어 16스레드 (중소 규모 서버)

| 스레드 그룹 | 개수 |
|---|---|
| IOCP 워커 | 8 |
| 게임 로직 워커 | 1~4 |
| DB 비동기 워커 | 4~8 |
| `COdbcConnPool` 헬스체크 | 1 |
| `COdbcConnPool` 재연결 워커 | 2~4 |
| 타이머/틱 | 1 |
| 비동기 로깅 | 1 |
| **총합** | **약 18~26개** |

IOCP 워커 + 게임 로직 워커 = 9~12로 코어 수(8) 근처~살짝 초과하지만, 나머지는
대부분 I/O 대기형 스레드라 실질적인 CPU 경합 부담은 크지 않다.

### 8.3 예시 2: 16코어 32스레드 (대규모/실서비스 서버)

| 스레드 그룹 | 개수 |
|---|---|
| IOCP 워커 | 16 |
| 게임 로직 워커 | 4~8 |
| DB 비동기 워커 | 8~16 |
| `COdbcConnPool` 헬스체크 | 1 |
| `COdbcConnPool` 재연결 워커 | 4~8 |
| 타이머/틱 | 1 |
| 비동기 로깅 | 1 |
| **총합** | **약 35~51개** |

IOCP 워커 + 게임 로직 워커 = 20~24로 코어 수(16)보다 다소 많지만, DB/재연결
워커처럼 블로킹 I/O 대기가 대부분인 스레드가 큰 비중을 차지해 컨텍스트 스위칭
부담은 제한적이다.

### 8.4 적용 팁

- IOCP 워커 + 게임 로직 워커의 합은 코어 수의 1.5배를 크게 넘기지 않는 선에서
  시작하고, 실측 CPU 사용률/지연시간을 보며 조정한다.
- DB 워커·재연결 워커는 대부분 블로킹 I/O 대기 상태이므로 코어 수보다 많아도
  실질적인 CPU 경합은 적다. `std::thread::hardware_concurrency()`로 코어 수를
  런타임에 조회해 초기값의 기준점으로 삼는 것을 권장한다.
- DB 비동기 워커 스레드 수와 `COdbcConnPool` 풀 크기(`_nMaxPoolSize`)는 함께
  정해야 한다 — 워커가 풀 크기보다 많으면 대여 실패(`PopFreeSlotIndex` → -1)만
  늘어난다.
- 위 수치는 시작점일 뿐이며, 최종적으로는 실제 부하 테스트(동접자 수, DB 쿼리
  QPS, 패킷 처리량)로 튜닝해야 한다.

## 9. 사용법

```cpp
// 1. 풀 생성 및 초기화
COdbcConnPool pool(/*nMaxPoolSize=*/16);

COdbcConnPool::TReconnectConfig cfg;
cfg.nWorkerCount     = 4;
cfg.nBackoffBaseMs   = 500;
cfg.nBackoffMaxMs    = 30000;
cfg.nBackoffMaxShift = 6;
cfg.nBackoffJitterMs = 250;

if( !pool.Init(EDBClass::MSSQL, _T("MyDSN"), cfg) )
{
    // 초기 커넥션 생성 실패 처리
}

// 2. 커넥션 대여 (RAII 가드 사용 권장)
{
    OdbcConnGuard guard(&pool);
    if( guard != nullptr )
    {
        guard->ExecuteQuery(_T("SELECT ..."));
    }
    // 스코프 종료 시 자동으로 ReleaseOdbcConn 호출됨
}

// 3. 운영 중 재연결 정책 변경 (예: 워커 수를 8개로 확장)
COdbcConnPool::TReconnectConfig newCfg = pool.GetReconnectConfig();
newCfg.nWorkerCount = 8;
pool.SetReconnectConfig(newCfg);
```

- `OdbcConnGuard`를 사용하지 않고 `GetOdbcConn`/`ReleaseOdbcConn`을 직접 짝지어 호출할 수도 있으나,
  예외 발생 시 반납 누락 위험이 있으므로 가드 사용을 권장한다.
- 풀 소멸 시 `~COdbcConnPool()`이 헬스체크/재연결 스레드를 먼저 종료한 뒤 `Clear()`로 자원을 정리한다.

### 9.1 여러 DB를 다루는 실제 서비스 통합 패턴

계정 DB, 게임 DB, 로그 DB처럼 DB가 여러 개인 서비스에서는 `COdbcConnPool`을 DB 노드 수만큼
배열로 만들어 두고, DB 비동기 워커 스레드들이 공용 요청 큐에서 작업을 꺼내 필요한 풀을
선택해 쓰는 구조가 일반적이다.

```cpp
// DB 노드 개수만큼 풀을 생성 (예: 계정 DB, 게임 DB, 로그 DB)
COdbcConnPool** pOdbcConnPools = new COdbcConnPool*[nDBCount](); // 값 초기화로 모든 슬롯을 nullptr로 둔다

// 재연결 워커 수는 각 풀 크기(= DB 비동기 워커 스레드 수) 대비 비례 산정 (§7.4)
COdbcConnPool::TReconnectConfig reconnectCfg;
reconnectCfg.nWorkerCount = std::max(4, nMaxThreadCnt / 4);

for( int32 i = 0; i < nDBCount; ++i )
{
    // COdbcConnPool이 BaseAllocator를 상속하므로 평범한 new로도 RawAllocator 경로를 타고,
    // 실패 시 예외 대신 nullptr을 반환한다 (§10 참고)
    pOdbcConnPools[i] = new COdbcConnPool(nMaxThreadCnt);
    if( pOdbcConnPools[i] == nullptr || !pOdbcConnPools[i]->Init(dbClass[i], dsn[i], reconnectCfg) )
    {
        // 이미 만든 풀들까지 함께 정리(ClearOdbcPools)한 뒤 실패 처리
        break;
    }
}
```

- 배열을 `new COdbcConnPool*[nDBCount]()`처럼 값 초기화해 두면, 아직 만들어지지 않은
  슬롯도 항상 `nullptr` 상태로 유지되어 정리 루틴이 모든 인덱스를 안전하게 순회할 수 있다.
- 각 풀은 독립된 `COdbcConnPool` 인스턴스이므로 DB별로 서로 다른 DSN/재연결 정책을 줄 수 있다.
- DB 비동기 워커 스레드 수(`nMaxThreadCnt`)와 풀 크기를 동일하게 맞추면, 워커 스레드 각각이
  항상 자기 몫의 슬롯을 확보할 수 있어 `PopFreeSlotIndex()` 실패(풀 고갈)를 구조적으로 방지한다.

## 10. 할당자(Allocator) 설계

`COdbcConnPool`은 `class COdbcConnPool : public BaseAllocator`로 선언되어 있다. 즉 이
클래스를 직접 `new`/`delete`하면(예: 위 §9.1의 `new COdbcConnPool(nMaxThreadCnt)`) 전역
`::operator new`/`delete`가 아니라 `BaseAllocator`가 오버라이드한 `operator new`/`delete`가
호출되어, 프로젝트의 `RawAllocator`(mimalloc/jemalloc/tcmalloc/malloc 중 컴파일 타임 선택) 경로를
탄다.

### 10.1 왜 PoolAllocator(xnew/xdelete)가 아니라 BaseAllocator인가

프로젝트의 할당자 계층은 용도가 명확히 나뉜다.

| 할당자 | 설계 목적 | `COdbcConnPool`과의 적합성 |
|---|---|---|
| `PoolAllocator` (→ `xnew`/`xdelete`) | 실서비스 핫패스(패킷, 세션 등 고빈도 할당/해제) | 부적합 — 풀 자체는 DB 노드당 1개, 서버 기동 시 한 번만 생성됨 |
| `BaseAllocator` | 크기가 크거나 드물게 생성되는 객체를 풀과 분리 | 적합 — 위 프로필과 정확히 일치 |

`BaseAllocator`는 데이터 멤버가 없고 상속되는 함수도 모두 non-virtual이라, 상속해도
`COdbcConnPool` 인스턴스에 vptr 등 추가 메모리 오버헤드가 붙지 않는다.

### 10.2 내부 `CBaseODBC` 커넥션은 별도로 `xnew`/`xdelete` 유지

풀 "껍데기"(`COdbcConnPool` 자신)와 달리, 그 안에서 관리하는 실제 ODBC 커넥션(`CBaseODBC`)은
`Init()`의 초기 채움과 `TryReconnect()`의 재연결 시마다(네트워크 장애가 잦으면 상대적으로
자주) 반복적으로 생성/삭제된다. 이쪽은 여전히 `xnew<CBaseODBC>(...)` / `xdelete(...)`
(`PoolAllocator` 경로)를 그대로 사용한다 — 같은 클래스 계층 안에서도 "이 객체를 만드는 빈도"에
따라 할당자를 다르게 선택한 것이다.

### 10.3 `make_shared`로 생성하는 타입에는 적용 무의미

`BaseAllocator` 상속이 효과를 가지려면 해당 타입이 **직접 `new 타입(...)`** 형태로 생성돼야
한다. `std::make_shared<T>()`는 컨트롤 블록과 객체를 하나로 묶어 자체 할당 경로로 확보하고
`T`의 `operator new`를 거치지 않으므로, 그런 방식으로 생성되는 타입에 `BaseAllocator`를
상속해도 효과가 없다 (`COdbcConnPool`은 위 예시처럼 직접 `new`되므로 해당 사항 없음. 반면
§11의 `COdbcAsyncSrv`는 `make_shared`로 생성되는 진짜 싱글턴이라 해당
사항이다).

## 11. 비동기 서비스 계층 — `COdbcAsyncSrv`

풀(`COdbcConnPool`) 위에, DB 노드별로 풀을 배열로 들고 공용 요청 큐 +
워커 스레드 풀로 쿼리를 비동기 처리하는 서비스 계층이다.

### 11.1 구조 요약

- `Regist(callIdent, handler)`로 명령어별 핸들러를 등록해두면, `Push()`로 큐에 들어온
  `st_DBAsyncRq` 요청을 워커 스레드들이 `Pop()` → `callIdent`로 핸들러 조회 → 실행한다.
  핸들러 조회는 `std::unordered_map`을 사용해 매 쿼리마다의 조회 비용을 O(1) 평균으로 유지한다.
- DB 노드 수만큼 `COdbcConnPool*` 배열(`_pOdbcConnPools`)을 두고,
  `GetAccountOdbcConnPool()`/`GetOdbcConnPool(id)`/`GetLogOdbcConnPool()`로
  용도별 풀을 가져다 쓴다 (§9.1 참고). 배열은 값 초기화되어 있고, 정리 전용 함수
  `ClearOdbcPools()`가 소멸자와 초기화 실패 경로 양쪽에서 공용으로
  각 풀을 안전하게 해제한다.
- `Instance()`는 `std::make_shared`로 생성되는 진짜 싱글턴이다 — `T::operator new`를
  거치지 않으므로 `BaseAllocator` 상속은 이 클래스에는 적용하지 않는다(§10.3).
- 큐 동기화는 `std::mutex` + `std::condition_variable`로 이루어진다. `Push()`는 큐 조작을
  마치고 락을 해제한 뒤 `notify_one()`을 호출해, 깨어난 워커가 곧바로 락을 잡을 수 있게 한다.
- `st_DBAsyncRq`는 `callIdent`별로 실제 쿼리 데이터를 담은 파생 구조체(예:
  `CONSUMER_DATA_BATCH_REQ`)의 베이스이며 `virtual` 소멸자를 가진다 — 베이스 포인터로
  삭제해도 파생 소멸자가 정확히 호출된다. 또한 `st_DBAsyncRq`는 `BaseAllocator`를 상속해,
  이 계열 요청 구조체를 만드는 `new`/삭제하는 `SAFE_DELETE`가 모두 자동으로 RawAllocator
  경로를 탄다.
- 쿼리가 타임아웃되어 처음 재시도될 때는 원본 요청 객체를 그대로 재사용해 `bReTry` 플래그만
  세팅한 뒤 재큐잉한다 — 파생 구조체를 통째로 다시 할당하지 않는다. 재큐잉이 실패하면(서비스
  종료 시점과 겹친 경우) 해당 객체는 직접 해제된다.
- `InitOdbc`는 호출 시작 시 `_bStopThread`를 `false`로 재설정해, `StopThread()`
  이후 서비스를 다시 시작하는 시나리오에서도 워커 스레드들이 정상적으로 큐를 처리한다.
  또한 각 DB 노드의 풀을 생성할 때 `TReconnectConfig.nWorkerCount`를
  `max(4, nMaxThreadCnt / 4)`로 산정해 전달함으로써, 재연결 워커 수가 풀 크기(= DB 비동기
  워커 스레드 수)에 비례하도록 한다.
- `Action()`의 지연 쿼리 경고는 빌드 구성에 따라 임계값이 다르다 — 디버그 빌드는 300ms,
  릴리즈 빌드는 1000ms 이상 걸린 쿼리에 대해 경고 로그를 남긴다.
- `Clear()`는 DB 요청 큐를 비우는 역할만 담당한다. 등록된 핸들러(`_mapCommand`)는
  `Clear()`의 영향을 받지 않으므로, 초기화가 중간에 실패해 `Clear()`가 호출되어도
  `Regist()`로 등록해둔 핸들러는 그대로 유지된다.

### 11.2 스레드 생성

`StartIoThreads()`는 `_nMaxThreadCnt`개의 워커 스레드를 람다(`[this]() { RunningThread(); }`)로
생성한다. 각 워커는 `RunningThread()` → `Action()`으로 이어지는 루프를 돌며 큐에서 요청을
꺼내 처리한다.
