# CMySQLConnPool 설계 문서

> 이 문서는 `CMySQLConnPool`을 중심으로 작성되었으며, 그 위에서 큐+워커로 동작하는
> 비동기 서비스 계층 `CMySQLAsyncSrv`(§11)까지 함께 다룬다. `COdbcConnPool`과 설계
> 철학이 동일한 자매 클래스이므로, 구조 자체는 §1~10 전반에서 대부분 대응된다.

## 1. 개념

`CMySQLConnPool`은 고정 크기의 MySQL 커넥션(`CBaseMySQL`)을 미리 생성해두고, 슬롯 단위로
대여/반납하며, 연결이 끊어진 슬롯을 백그라운드에서 자동으로 재연결하는 락-프리 지향
커넥션 풀이다.

핵심 설계 방향은 다음과 같다.

- **핫패스(커넥션 대여/반납)는 원자 연산 위주로 구성**하여 뮤텍스 경합을 피한다.
- **재연결(블로킹 I/O)은 별도 워커 스레드 풀에 위임**하여 헬스체크 루프나 커넥션 대여 경로를 막지 않는다.
- **낡은 커넥션 삭제는 참조 카운트가 0이 될 때까지 지연**시키고, 그래도 안 되면 격리 큐(quarantine)로
  보내 Use-After-Free를 원천적으로 방지한다.

## 2. 특징

| 특징 | 설명 |
|---|---|
| 슬롯 기반 고정 크기 풀 | `_nMaxPoolSize`로 크기가 고정되며 런타임에 늘어나거나 줄지 않는다 |
| Lock-free 대여/반납 | `GetMySQLConn` / `ReleaseMySQLConn`은 `std::atomic`의 fetch_add/fetch_sub만 사용 |
| 비동기 자동 재연결 | 헬스체크 스레드가 끊어진 슬롯을 감지하고, 별도 워커 풀이 실제 재연결(I/O)을 병렬 수행 |
| 지수 백오프 + 지터 | DB 전체 장애 시 모든 슬롯이 동시에 재시도하는 connection storm을 방지 |
| 백오프 하한 강제 | `RECONNECT_BACKOFF_MIN_MS`(10ms) 미만의 `nBackoffBaseMs`는 재연결 폭주로 이어질 수 있어 `ValidateReconnectConfig`에서 거부 |
| 이벤트 기반 정밀 재시도 스케줄링 | 백오프 대기는 `CDelayedTaskQueue`(§3의 `_delayedTaskQueue`)가 전담하며, 계산된 지연 시간이 정확히 지난 시점에 콜백이 1회 실행되어 재시도를 트리거한다. 헬스체크 스캔 주기(500ms)와는 독립적으로 동작한다 |
| 동적 워커 수 조정 | `SetReconnectConfig`로 런타임 중 재연결 워커 수/백오프 정책을 조정 가능 |
| 격리(Quarantine) 큐 | 교체된 낡은 커넥션에 참조가 남아있으면 즉시 삭제하지 않고 격리 후 안전할 때 삭제 |
| 격리 큐 요약 경보 | 갇힌 항목이 있어도 개별 로그 대신 5분(`LOG_ALERT_INTERVAL_MS`) 주기로 개수와 최장 체류 시간을 한 줄로 요약 로깅해 로그 폭주 방지 |
| Safe Leak | 프로세스 종료 시점까지 참조가 남은 커넥션은 삭제를 포기(누수)하여 UAF 크래시를 방지 |
| False sharing 방지 | 슬롯별 원자 배열(`_pMySQLConns`, `_pRefCount`, `_pReconnecting`, `_pRetryFailCount`)을 `CachePaddedAtomic<T>[]`로, `_slotLocks`를 캐시라인 정렬된 `SpinLockDefault[]`로 구성해 슬롯 간 캐시라인 공유를 차단 |
| char/wchar_t 이중 접속정보 입력 | `Init()`이 `char*`/`wchar_t*` 두 오버로드를 제공하며, wchar_t 버전은 `WideCharToMultiByte` 변환 후 공통 로직(`FinishInit`)에 합류 |
| 할당자 분리 | `CMySQLConnPool` 자신은 `BaseAllocator` 상속으로 RawAllocator 경로를, 내부 `CBaseMySQL` 커넥션은 `xnew`/`xdelete`(PoolAllocator)로 별도 관리 (§10 참고) |

## 3. 멤버 변수 설명

### 기본 상태
| 변수 | 설명 |
|---|---|
| `_szDBHost` / `_szDBUserId` / `_szDBPasswd` / `_szDBName` / `_uiPort` | 접속 정보(호스트/계정/비밀번호/DB명/포트). `Init()` 시 채워지며 재연결(`TryReconnect`)마다 재사용된다 |
| `_nMaxPoolSize` | 풀 최대 크기 (생성 시 `const`로 고정, 변경 불가) |

### 재연결 정책 (필드별 개별 원자 변수)
| 변수 | 설명 |
|---|---|
| `_nBackoffBaseMs` | 최초 재시도 간격 |
| `_nBackoffMaxMs` | 재시도 간격 상한 |
| `_nBackoffMaxShift` | 지수 증가 상한 shift (오버플로 방지 겸 상한 역할) |
| `_nBackoffJitterMs` | 재시도 타이밍 분산을 위한 지터 상한 |

> 구조체 전체를 `std::atomic<TReconnectConfig>`로 감싸면 내부적으로 뮤텍스 폴백이 걸려 lock-free가
> 깨지기 때문에, 서로 독립적으로만 쓰인다는 성질을 이용해 필드별로 쪼개 관리한다. (`OnReconnectFailed`
> 같은 핫패스에 숨은 락이 생기는 것을 막기 위함)

### 슬롯 배열 (생성자에서 단 1회만 할당되는 불변 배열)
| 변수 | 설명 |
|---|---|
| `_pMySQLConns` | 슬롯별 실제 커넥션 포인터 (`CachePaddedAtomic<CBaseMySQL*>[]`) |
| `_pRefCount` | 슬롯별 참조 카운트 (`CachePaddedAtomic<int32>[]`, 가장 핫한 배열) |
| `_slotLocks` | 슬롯별 교체(swap) 보호용 스핀락 배열 (`SpinLockDefault[]`) |
| `_pReconnecting` | 슬롯별 "재연결 워커가 처리 중" 플래그 (`CachePaddedAtomic<bool>[]`, 중복 디스패치 방지) |
| `_pRetryFailCount` | 슬롯별 연속 재연결 실패 횟수 (`CachePaddedAtomic<int32>[]`). 지수 백오프 shift 계산에 쓰이는 동시에, 0보다 크면 "이미 `_delayedTaskQueue`에 재시도가 예약된 상태"임을 나타내는 상태 플래그 역할도 겸함(§5.2) |

> `_quarantineQueue`가 `&_pRefCount[i].value` 주소를 그대로 저장하므로, 위 배열들은 런타임 중
> 재할당(make_unique 재호출 등)이 절대 금지된다. 각 슬롯이 `CachePaddedAtomic<T>`로 캐시라인
> 하나씩을 점유해, 서로 다른 슬롯을 동시에 다루는 스레드들이 false sharing으로 서로의
> 캐시라인을 무효화시키는 것을 막는다.

### 헬스체크 / 재연결 워커
| 변수 | 설명 |
|---|---|
| `_healthCheckThreadMgr` / `_bStopHealthCheck` / `_nHealthCheckIntervalMs` | 헬스체크 스레드 관리, 종료 신호, 주기(기본 500ms) |
| `_delayedTaskQueue` / `_delayedTaskThreadMgr` | 재연결 실패 시 계산된 백오프 지연을 정확한 시각에 1회 실행하기 위한 `CDelayedTaskQueue`와, 이를 처리하는 전담 스레드(단일 컨슈머) |
| `_nNextSlotHint` | `PopFreeSlotIndex` 탐색 시작 위치 힌트 (경합 분산용) |
| `_reconnectWorkerMgr` / `_bStopReconnectWorkers` | 재연결 워커 스레드 관리, 전체 종료 신호 |
| `_nCurrentWorkerCount` / `_nDesiredWorkerCount` | 현재 워커 수 / 목표 워커 수 |
| `_reconnectQueueMutex` / `_reconnectQueueCv` / `_reconnectPendingSlots` | 재연결 대기열과 그 동기화 객체 |
| `_globalQuarantineLock` / `_quarantineQueue` | 격리 큐와 이를 보호하는 전역 스핀락 |
| `_quarantineLastSummaryLogTime` | 격리 큐 요약 경보를 마지막으로 남긴 시각. HealthCheckLoop 스레드에서만 읽고 쓰므로 원자적일 필요가 없다 |

## 4. 멤버 함수 설명

### Public API
| 함수 | 설명 |
|---|---|
| `Init(char* 접속정보..., reconnectConfig)` | 풀을 초기화하고 접속 정보로 커넥션을 동기적으로 채운다. 잘못된 `reconnectConfig`는 기본값으로 대체된다 |
| `Init(wchar_t* 접속정보..., reconnectConfig)` | 와이드 문자열 접속 정보를 `WideCharToMultiByte`로 변환한 뒤 동일한 `FinishInit`으로 합류 |
| `GetMySQLConn(nType)` | 슬롯의 참조 카운트를 증가시키고 커넥션을 반환. 끊어진 슬롯이면 즉시 `nullptr`을 반환하고 카운트를 되돌린다 |
| `ReleaseMySQLConn(nType)` | 참조 카운트를 감소시켜 슬롯을 반납 |
| `GetPooledConnUnsafe(nType)` | `PopFreeSlotIndex`로 이미 선점된 슬롯을 카운트 변경 없이 조회 (`MySQLConnGuard` 전용) |
| `GetMaxPoolSize()` | 풀의 최대 크기 조회 |
| `PopFreeSlotIndex()` | 빈 슬롯(참조 카운트 0)을 찾아 즉시 원자적으로 선점하고 인덱스 반환 |
| `SetReconnectConfig(cfg)` | 백오프/워커 수 정책을 런타임에 변경. 유효성 실패 시 전체 거부(부분 적용 없음) |
| `GetReconnectConfig()` | 현재 정책 스냅샷 조회 (모니터링용) |

### Protected 내부 로직
| 함수 | 설명 |
|---|---|
| `Clear()` | 모든 슬롯을 정리. 참조가 남은 슬롯은 격리 큐로 보냄 (Shutdown/재초기화 공용) |
| `IsValidIndex(nType)` | 슬롯 인덱스 범위 검사 |
| `ValidateReconnectConfig(cfg)` | 재연결 설정값의 상식적 범위 검사 (Init/SetReconnectConfig 공용). `nBackoffBaseMs < RECONNECT_BACKOFF_MIN_MS`, `nBackoffMaxShift`가 0~30 범위를 벗어나는 경우 등을 거부 |
| `FinishInit(reconnectConfig)` | 접속 정보가 이미 채워진 뒤 공통으로 실행되는 마무리 로직 — 정책 검증/적용, 커넥션 사전 생성, 헬스체크/재연결 스레드 기동 |
| `TryReconnect(nType)` | 새 커넥션을 생성하고 실제 연결까지 시도하는 블로킹 I/O 로직 |
| `ApplyReconnectedConn(nType, pNewConn)` | 새 커넥션으로 슬롯을 스왑하고, 낡은 커넥션을 안전하게 삭제 또는 격리 |
| `ScheduleRetry(nType)` | 실패 횟수(`_pRetryFailCount`)를 늘리고 지수 백오프+지터로 지연 시간을 계산한 뒤, `_delayedTaskQueue.Reserve()`로 그 시간 뒤 1회 실행될 재시도 콜백을 예약 |
| `OnReconnectFailed(nType)` | `ScheduleRetry(nType)` 호출로 위임 |
| `OnReconnectSucceeded(nType)` | `_pRetryFailCount`를 0으로 초기화 (백오프 상태 리셋) |
| `HealthCheckLoop()` | 격리 큐 청소(PART A) + 끊어진 슬롯 스캔(PART B). `_pReconnecting`을 CAS로 선점한 뒤 `_pRetryFailCount == 0`(아직 예약된 재시도가 없는 슬롯)인 경우에만 즉시 재연결 큐에 등록하고, 실패 이력이 있는 슬롯은 `_delayedTaskQueue`의 예약에 맡기고 그냥 넘어감 (블로킹 I/O 없음) |
| `StartHealthCheckThread()` / `StopHealthCheckThread()` | 헬스체크 스레드 기동/종료(Join까지 대기) |
| `DelayedTaskLoop()` | `_delayedTaskQueue.ProcessExpiredTasks()`를 호출해 만료된 재시도 콜백들을 실행하는 루프 |
| `StartDelayedTaskThread()` / `StopDelayedTaskThread()` | 지연 타이머 전담 스레드 기동 / `_delayedTaskQueue.Stop()` 후 안전 종료(join) |
| `ReconnectWorkerLoop()` | 대기열에서 슬롯을 꺼내 실제 `TryReconnect` + 스왑을 수행하는 워커 루프 |
| `StartReconnectWorkers(n)` / `StopReconnectWorkers()` | 재연결 워커 풀 기동/종료 |
| `SetWorkerCount(n)` | 목표 워커 수 갱신. 확대는 즉시 스폰, 축소는 워커가 스스로 종료하도록 유도 |
| `TryExitIfExcess()` | 현재 워커가 초과 인원인지 CAS로 판정하고, 맞다면 스스로 종료 |
| `EnqueueReconnect(nType)` | 재연결 대기열에 슬롯을 넣고 워커 하나를 깨움 |

## 5. 동작 흐름

### 5.1 커넥션 대여/반납 (핫패스)
1. `MySQLConnGuard` 생성 시 `PopFreeSlotIndex()`로 빈 슬롯을 원자적으로 선점 (참조 카운트 1)
2. `GetPooledConnUnsafe()`로 커넥션 포인터 조회. 스왑 타이밍 등으로 슬롯이 이미 무효화됐다면
   `ReleaseMySQLConn()`으로 선점했던 카운트를 되돌리고 대여 실패로 처리
3. 소멸 시 `ReleaseMySQLConn()`으로 참조 카운트 반납

### 5.2 자동 재연결
1. `HealthCheckLoop()`이 500ms마다 순회하며 참조 카운트 0 & 연결 끊김인 슬롯을 찾고, `_pReconnecting`을
   CAS로 선점한다.
2. 선점에 성공한 슬롯 중 `_pRetryFailCount == 0`(아직 예약된 재시도가 없는 슬롯)인 경우만
   `EnqueueReconnect()`로 즉시 대기열에 등록해 워커를 깨운다. 실패 이력이 있어 이미
   `_delayedTaskQueue`에 재시도가 예약된 슬롯은 이번 순회에서 `_pReconnecting`만 반납하고 넘어간다.
3. `ReconnectWorkerLoop()`이 `TryReconnect()`로 블로킹 I/O 수행 (다른 워커들은 각자 슬롯을 병렬 처리)
4. 성공 시 `ApplyReconnectedConn()`으로 슬롯 스왑, 낡은 커넥션은 참조가 빠질 때까지 최대
   `WAIT_TIMEOUT_MS`(100ms) 대기 후 삭제(또는 타임아웃 시 격리), `OnReconnectSucceeded()`로
   `_pRetryFailCount`를 0으로 리셋
5. 실패 시 `OnReconnectFailed()` → `ScheduleRetry()`가 실패 횟수를 늘려 지수 백오프+지터 지연을
   계산하고, `_delayedTaskQueue.Reserve(지연ms, 콜백)`으로 정확히 그 시간 뒤 1회 실행되는 재시도
   콜백을 예약한다. 콜백은 만료 시점에 슬롯이 여전히 재연결이 필요한 상태인지 재확인한 뒤
   `EnqueueReconnect()`하거나, 그 사이 다른 경로로 이미 해소됐다면 `_pReconnecting`만 반납한다.

> 풀 시작 시에는 `FinishInit()`이 `StartDelayedTaskThread()` → `StartHealthCheckThread()` →
> `StartReconnectWorkers()` 순으로 기동해, 헬스체크나 재연결 워커가 첫 실패로 `ScheduleRetry()`를
> 호출하기 전에 지연 타이머 스레드가 먼저 요청을 받을 준비를 갖춘다. 종료 시에는 반대로
> `StopHealthCheckThread()` → `StopDelayedTaskThread()` → `StopReconnectWorkers()` 순으로 정지한다
> (`~CMySQLConnPool()`/재`Init()` 공통).

### 5.3 격리 큐 처리
1. `HealthCheckLoop()` PART A에서 매 사이클 `_globalQuarantineLock` 안에서 큐 전체를 순회하며
   각 항목의 참조 카운트가 0이 됐는지 재검사
2. 0이 된 항목은 삭제 대상 리스트(`vDeletes`)에 적재만 하고, 락을 벗어난 뒤(PART A-1) 실제 `xdelete` 수행
3. 여전히 참조가 남은 항목은 큐에 재삽입하고, 그 중 가장 오래 갇힌 시각을 집계
4. 갇힌 항목이 있으면 5분(`LOG_ALERT_INTERVAL_MS`)마다 한 번만 "개수 + 최장 체류 시간" 요약 로그를 남김

### 5.4 워커 수 동적 조정
- 확대: `_nDesiredWorkerCount`를 CAS로 목표까지 끌어올리고 부족분만큼 즉시 스폰
- 축소: 스레드를 직접 종료시키지 않고 조건 변수만 깨움 → 각 워커가 다음 순회에서
  `TryExitIfExcess()`로 스스로 초과 여부 판단 후 종료. 반복/역전 호출에도 최종 목표치로 정확히 수렴

## 6. 장단점

### 장점
- 대여/반납 핫패스가 원자 연산만 사용해 뮤텍스 경합이 없다.
- 재연결 I/O가 별도 워커 풀에서 병렬 처리되어 헬스체크나 대여 경로를 막지 않는다.
- 지수 백오프 + 지터로 DB 장애 시 재연결 폭주(connection storm)를 방지한다.
- 격리 큐와 Safe Leak 정책으로 UAF 크래시 위험을 구조적으로 차단한다.
- 격리 큐가 개별 로그 대신 요약 로그만 남겨, 대량 격리 상황에서도 로그가 폭주하지 않는다.
- 워커 수/백오프 정책을 서비스 운영 중 무중단으로 조정할 수 있다.
- char/wchar_t 양쪽 접속 정보를 모두 지원해 호출부 인코딩 제약이 적다.
- 재시도 대기는 `CDelayedTaskQueue` 기반 이벤트 방식으로 처리되어, 헬스체크 스캔 주기(500ms)와
  무관하게 계산된 백오프 시간이 지난 즉시 정확히 재시도가 트리거된다.

### 단점 / 트레이드오프
- 슬롯 배열이 생성자에서 고정 할당되므로, 풀 크기 자체는 런타임에 늘릴 수 없다.
- 모든 슬롯의 백오프 재시도 콜백이 `_delayedTaskQueue`라는 단일 전담 스레드를 통해 순차
  처리된다. 콜백 자체는 가벼운 상태 확인 + 큐 등록뿐이라 실질적 지연은 미미하지만, 구조적으로는
  단일 컨슈머 직렬화 지점이라는 점을 인지하고 있어야 한다.
- `GetReconnectConfig()`의 스냅샷은 필드별 개별 로드이므로 완전한 원자적 일관성은 보장하지 않는다
  (모니터링 용도로는 문제 없으나 정합성이 중요한 로직에는 부적합).
- `ApplyReconnectedConn`/`Clear`의 100ms 대기 후 격리 전환 로직은 반환 지연이 긴 호출자가 있을 경우
  일시적으로 메모리를 계속 점유(격리)하게 된다.
- Safe Leak 정책은 크래시를 막는 대신 셧다운 시점에 의도적인 메모리 누수를 허용한다.
- 재연결 워커 축소가 즉시 반영되지 않고 다음 워커 순회 시점에 반영된다 (지연 수렴).
- 격리 큐 요약 로그는 5분 주기이므로, 그 사이 격리 상황이 심각해져도 로그만으로는 실시간
  파악이 어렵다 (모니터링 지표로 별도 노출하는 것을 권장).

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
- MySQL 서버 자체가 짧은 시간에 대량의 신규 연결/인증 요청을 받으면 오히려 커넥션 수립
  지연이나 `max_connections` 임계 근접, 인증 스로틀링을 유발할 수 있다. DB 서버의 최대
  동시 연결/인증 처리량도 상한을 정하는 데 함께 고려해야 한다.

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
| MySQL 서버의 동시 연결/인증 처리 능력이 제한적 | 워커 수를 낮게 유지하고 백오프(`nBackoffBaseMs`, `nBackoffJitterMs`)로 폭주를 흡수 |
| 네트워크 RTT가 크거나 TLS 핸드셰이크 비용이 큰 환경 | 워커당 재연결 소요 시간이 길어지므로 워커 수를 다소 늘려 병렬성 확보 |

이 값들은 고정된 정답이 없으므로, 운영 환경의 DB 재시작/네트워크 장애 시나리오를
기준으로 실측 후 `SetReconnectConfig()`로 튜닝하는 것을 권장한다. `CMySQLAsyncSrv::InitMySQL`은
기본값으로 `std::max(4, nMaxThreadCnt / 4)`를 적용한다(§11.1 참고).

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

`CMySQLConnPool`을 IOCP 게임 서버의 DB 처리에 사용할 경우, 서버 전체 스레드를
기능별로 분리하는 것이 좋다. 아래는 역할 구분과, 요즘 많이 쓰이는 코어 구성
기준의 개수 예시다.

### 8.1 기능별 스레드 그룹

| 스레드 그룹 | 역할 | 개수 결정 기준 |
|---|---|---|
| IOCP 워커 | `GetQueuedCompletionStatusEx`로 완료된 Recv/Send I/O를 꺼내 세션에 전달. 순수 네트워크 I/O 처리만 담당 | 물리 코어 수 기준 (I/O 대기 비중에 따라 조정) |
| 게임 로직(콘텐츠) 워커 | JobQueue에서 패킷 처리/게임 로직 Job을 꺼내 실행. IOCP 워커와 분리해 로직 처리 지연이 네트워크 I/O를 막지 않게 함 | 콘텐츠 샤딩 여부에 따라 1개(단일 월드) ~ 샤드 수 |
| DB 비동기 워커 (`CMySQLAsyncSrv`) | DB 요청 큐(`_queueDBAsyncRq`)에서 쿼리 요청을 꺼내 `CMySQLConnPool`에서 커넥션을 빌려 실제 쿼리(블로킹) 실행 후 결과를 완료 큐로 반환 | 예상 동시 DB 요청 수 기준. `_nMaxPoolSize`(=`_nMaxThreadCnt`)를 넘지 않는 선에서 결정 |
| `CMySQLConnPool` 헬스체크 스레드 | 끊어진 슬롯을 감지해 재연결 대기열에 등록 (논블로킹) | DB 노드당 1개 고정 (클래스 내부에서 자동 생성) |
| `CMySQLConnPool` 재연결 워커 스레드 | 실제 재연결 I/O 수행 (`TryReconnect`) | §7 기준 (풀 크기 대비 10~25%, 소규모면 기본값 4) |
| 타이머/틱 스레드 | 게임 틱, 스케줄된 이벤트(리스폰, 버프 만료 등) 처리 | 1개 (로직 워커의 주기 Job으로 흡수 가능) |
| 비동기 로깅 스레드 | 로그를 큐에 쌓고 파일/네트워크로 flush (로직 스레드가 디스크 I/O로 막히지 않게) | 1개 |
| Listener/Accept | 신규 접속 수락 | 별도 생성 불필요, IOCP 워커 중 하나가 AcceptEx 완료도 함께 처리 |

### 8.2 예시 1: 8코어 16스레드 (중소 규모 서버)

| 스레드 그룹 | 개수 |
|---|---|
| IOCP 워커 | 8 |
| 게임 로직 워커 | 1~4 |
| DB 비동기 워커 | 4~8 |
| `CMySQLConnPool` 헬스체크 (DB 노드 수만큼) | 1~3 |
| `CMySQLConnPool` 재연결 워커 (노드당) | 2~4 |
| 타이머/틱 | 1 |
| 비동기 로깅 | 1 |
| **총합** | **약 18~29개** |

IOCP 워커 + 게임 로직 워커 = 9~12로 코어 수(8) 근처~살짝 초과하지만, 나머지는
대부분 I/O 대기형 스레드라 실질적인 CPU 경합 부담은 크지 않다.

### 8.3 예시 2: 16코어 32스레드 (대규모/실서비스 서버)

| 스레드 그룹 | 개수 |
|---|---|
| IOCP 워커 | 16 |
| 게임 로직 워커 | 4~8 |
| DB 비동기 워커 | 8~16 |
| `CMySQLConnPool` 헬스체크 (DB 노드 수만큼) | 1~3 |
| `CMySQLConnPool` 재연결 워커 (노드당) | 4~8 |
| 타이머/틱 | 1 |
| 비동기 로깅 | 1 |
| **총합** | **약 35~53개** |

IOCP 워커 + 게임 로직 워커 = 20~24로 코어 수(16)보다 다소 많지만, DB/재연결
워커처럼 블로킹 I/O 대기가 대부분인 스레드가 큰 비중을 차지해 컨텍스트 스위칭
부담은 제한적이다.

### 8.4 적용 팁

- IOCP 워커 + 게임 로직 워커의 합은 코어 수의 1.5배를 크게 넘기지 않는 선에서
  시작하고, 실측 CPU 사용률/지연시간을 보며 조정한다.
- DB 워커·재연결 워커는 대부분 블로킹 I/O 대기 상태이므로 코어 수보다 많아도
  실질적인 CPU 경합은 적다. `SYSTEM::CoreCount()`(§11.1)로 코어 수를 런타임에
  조회해 초기값의 기준점으로 삼는 것을 권장한다.
- DB 비동기 워커 스레드 수(`_nMaxThreadCnt`)와 `CMySQLConnPool` 풀 크기(`_nMaxPoolSize`)는
  `CMySQLAsyncSrv::InitMySQL`에서 항상 동일하게(`new CMySQLConnPool(_nMaxThreadCnt)`)
  맞춰진다 — 워커가 풀 크기보다 많으면 대여 실패(`PopFreeSlotIndex` → -1)만 늘어나므로
  구조적으로 이를 방지한 설계다.
- 위 수치는 시작점일 뿐이며, 최종적으로는 실제 부하 테스트(동접자 수, DB 쿼리
  QPS, 패킷 처리량)로 튜닝해야 한다.

## 9. 사용법

```cpp
// 1. 풀 생성 및 초기화
CMySQLConnPool pool(/*nMaxPoolSize=*/16);

CMySQLConnPool::TReconnectConfig cfg;
cfg.nWorkerCount     = 4;
cfg.nBackoffBaseMs   = 500;
cfg.nBackoffMaxMs    = 30000;
cfg.nBackoffMaxShift = 6;
cfg.nBackoffJitterMs = 250;

if( !pool.Init("127.0.0.1", "user", "passwd", "dbname", 3306, cfg) )
{
    // 초기 커넥션 생성 실패 처리
}

// 2. 커넥션 대여 (RAII 가드 사용 권장)
{
    MySQLConnGuard guard(&pool);
    if( guard != nullptr )
    {
        guard->Query("SELECT ...");
    }
    // 스코프 종료 시 자동으로 ReleaseMySQLConn 호출됨
}

// 3. 운영 중 재연결 정책 변경 (예: 워커 수를 8개로 확장)
CMySQLConnPool::TReconnectConfig newCfg = pool.GetReconnectConfig();
newCfg.nWorkerCount = 8;
pool.SetReconnectConfig(newCfg);
```

- `MySQLConnGuard`를 사용하지 않고 `GetMySQLConn`/`ReleaseMySQLConn`을 직접 짝지어 호출할 수도 있으나,
  예외 발생 시 반납 누락 위험이 있으므로 가드 사용을 권장한다.
- 풀 소멸 시 `~CMySQLConnPool()`이 헬스체크 → 지연 타이머 → 재연결 워커 스레드를 순서대로
  먼저 종료한 뒤 `Clear()`로 자원을 정리한다(§5.2 참고).

### 9.1 여러 DB를 다루는 실제 서비스 통합 패턴

계정 DB, 게임 DB, 로그 DB처럼 DB가 여러 개인 서비스에서는 `CMySQLConnPool`을 DB 노드 수만큼
배열로 만들어 두고, DB 비동기 워커 스레드들이 공용 요청 큐에서 작업을 꺼내 필요한 풀을
선택해 쓰는 구조가 일반적이다. `CMySQLAsyncSrv::InitMySQL`이 실제로 이 패턴을 구현한다.

```cpp
// DB 노드 개수만큼 풀을 생성 (예: 계정 DB, 게임 DB, 로그 DB)
// 값 초기화(...) 로 모든 슬롯을 nullptr로 두어, 초기화 도중 실패해도
// 아직 생성되지 않은 슬롯을 쓰레기 포인터로 delete하는 일이 없게 한다.
CMySQLConnPool** pMySQLConnPools = new CMySQLConnPool*[nDBCount]();

// 재연결 워커 수는 각 풀 크기(= DB 비동기 워커 스레드 수) 대비 비례 산정 (§7.4)
CMySQLConnPool::TReconnectConfig reconnectCfg;
reconnectCfg.nWorkerCount = std::max(4, nMaxThreadCnt / 4);

for( int32 i = 0; i < nDBCount; ++i )
{
    // CMySQLConnPool이 BaseAllocator를 상속하므로 평범한 new로도 RawAllocator 경로를 타고,
    // 실패 시 예외 대신 nullptr을 반환한다 (§10 참고)
    pMySQLConnPools[i] = new CMySQLConnPool(nMaxThreadCnt);
    if( pMySQLConnPools[i] == nullptr ||
        !pMySQLConnPools[i]->Init(dbNode[i]._tszDBHost, dbNode[i]._tszDBUserId,
                                   dbNode[i]._tszDBPasswd, dbNode[i]._tszDBName,
                                   dbNode[i]._nPort, reconnectCfg) )
    {
        // 이미 만든 풀들까지 함께 정리(ClearMySQLConnPools)한 뒤 실패 처리
        break;
    }
}
```

- 배열을 `new CMySQLConnPool*[nDBCount]()`처럼 값 초기화해 두면, 아직 만들어지지 않은
  슬롯도 항상 `nullptr` 상태로 유지되어 정리 루틴이 모든 인덱스를 안전하게 순회할 수 있다.
- 각 풀은 독립된 `CMySQLConnPool` 인스턴스이므로 DB별로 서로 다른 접속 정보/재연결 정책을 줄 수 있다.
- DB 비동기 워커 스레드 수(`_nMaxThreadCnt`)와 풀 크기를 동일하게 맞추면, 워커 스레드 각각이
  항상 자기 몫의 슬롯을 확보할 수 있어 `PopFreeSlotIndex()` 실패(풀 고갈)를 구조적으로 방지한다.

## 10. 할당자(Allocator) 설계

`CMySQLConnPool`은 `class CMySQLConnPool : public BaseAllocator`로 선언되어 있다. 즉 이
클래스를 직접 `new`/`delete`하면(예: 위 §9.1의 `new CMySQLConnPool(nMaxThreadCnt)`) 전역
`::operator new`/`delete`가 아니라 `BaseAllocator`가 오버라이드한 `operator new`/`delete`가
호출되어, 프로젝트의 `RawAllocator` 경로를 탄다.

### 10.1 왜 PoolAllocator(xnew/xdelete)가 아니라 BaseAllocator인가

프로젝트의 할당자 계층은 용도가 명확히 나뉜다.

| 할당자 | 설계 목적 | `CMySQLConnPool`과의 적합성 |
|---|---|---|
| `PoolAllocator` (→ `xnew`/`xdelete`) | 실서비스 핫패스(패킷, 세션 등 고빈도 할당/해제) | 부적합 — 풀 자체는 DB 노드당 1개, 서버 기동 시 한 번만 생성됨 |
| `BaseAllocator` | 크기가 크거나 드물게 생성되는 객체를 풀과 분리 | 적합 — 위 프로필과 정확히 일치 (`COdbcConnPool`과 동일한 근거) |

`BaseAllocator`는 데이터 멤버가 없고 상속되는 함수도 모두 non-virtual이라, 상속해도
`CMySQLConnPool` 인스턴스에 vptr 등 추가 메모리 오버헤드가 붙지 않는다.

### 10.2 내부 `CBaseMySQL` 커넥션은 별도로 `xnew`/`xdelete` 유지

풀 "껍데기"(`CMySQLConnPool` 자신)와 달리, 그 안에서 관리하는 실제 MySQL 커넥션(`CBaseMySQL`)은
`FinishInit()`의 초기 채움과 `TryReconnect()`의 재연결 시마다(네트워크 장애가 잦으면 상대적으로
자주) 반복적으로 생성/삭제된다. 이쪽은 여전히 `xnew<CBaseMySQL>(...)` / `xdelete(...)`
(`PoolAllocator` 경로)를 그대로 사용한다 — 같은 클래스 계층 안에서도 "이 객체를 만드는 빈도"에
따라 할당자를 다르게 선택한 것이다.

### 10.3 `make_shared`로 생성하는 타입에는 적용 무의미

`BaseAllocator` 상속이 효과를 가지려면 해당 타입이 **직접 `new 타입(...)`** 형태로 생성돼야
한다. `std::make_shared<T>()`는 컨트롤 블록과 객체를 하나로 묶어 자체 할당 경로로 확보하고
`T`의 `operator new`를 거치지 않으므로, 그런 방식으로 생성되는 타입에 `BaseAllocator`를
상속해도 효과가 없다 (`CMySQLConnPool`은 위 예시처럼 직접 `new`되므로 해당 사항 없음. 반면
§11의 `CMySQLAsyncSrv`는 `make_shared`로 생성되는 진짜 싱글턴이라 해당 사항이다).

## 11. 비동기 서비스 계층 — `CMySQLAsyncSrv`

풀(`CMySQLConnPool`) 위에, DB 노드별로 풀을 배열로 들고 공용 요청 큐 +
워커 스레드 풀로 쿼리를 비동기 처리하는 서비스 계층이다.

### 11.1 구조 요약

- `Regist(command, handler)`로 명령어별 핸들러를 등록해두면, `Push()`로 큐에 들어온
  `st_DBAsyncRq` 요청을 워커 스레드들이 `Pop()` → `callIdent`로 핸들러 조회 → 실행한다.
  핸들러 조회는 매 DB 비동기 호출마다(핫패스) 이루어지므로 O(log n) 탐색인 `std::map`
  대신 O(1) 평균 탐색인 `std::unordered_map`(`COMMAND_MAP`)을 사용한다.
- DB 노드 수만큼 `CMySQLConnPool*` 배열(`_pMySQLConnPools`)을 두고,
  `GetAccountConnPool()`(인덱스 0)/`GetMySQLConnPool(m_nID)`(3개 초과 시 `(m_nID % (DBCount-1)) + 1`로
  해시 분산, 그 외엔 마지막 인덱스)/`GetLogConnPool()`(인덱스 2, DB 3개 초과 전제)로
  용도별 풀을 가져다 쓴다 (§9.1 참고). 배열은 값 초기화되어 있고, 정리 전용 함수
  `ClearMySQLConnPools()`가 소멸자와 초기화 실패 경로 양쪽에서 공용으로 각 풀을 안전하게 해제한다.
- `Instance()`는 C++11 Meyers' Singleton(함수 내 static 지역 변수, magic statics)으로
  스레드 안전하게 생성되는 `std::make_shared` 기반 진짜 싱글턴이다 — `T::operator new`를
  거치지 않으므로 `BaseAllocator` 상속은 이 클래스에는 적용하지 않는다(§10.3).
- 큐 동기화는 `std::mutex` + `std::condition_variable`로 이루어진다. Push/Pop이 매 DB
  요청마다 항상 배타적으로(unique_lock) 잠그는 핫패스이고, 공유 잠금이 필요한 경로
  (`GetQueryQueueSize`/`IsEmpty`)는 모니터링용으로 드물게만 쓰이므로 `shared_mutex` 대신
  표준 `mutex`/`condition_variable`을 사용한다(`shared_mutex`의 배타 잠금 자체가 더 무겁고
  `condition_variable_any`도 락 타입 소거 오버헤드가 있기 때문). `Push()`는 큐 조작을
  마치고 락을 해제한 뒤 `notify_one()`을 호출해, 깨어난 워커가 곧바로 락을 잡을 수 있게 한다.
  `Pop()`은 소비자이므로 `notify_all()`을 호출하지 않는다(다른 소비자를 깨울 필요가 없음).
- `st_DBAsyncRq`는 `callIdent`별로 실제 쿼리 데이터를 담은 파생 구조체의 베이스 타입이다.
  타임아웃 재시도 시, 베이스 타입으로 복사(`new st_DBAsyncRq{ *pAsyncRq }`)하면 파생
  클래스의 실제 쿼리 파라미터가 잘려나가는 오브젝트 슬라이싱 버그가 발생하므로, 복사본을
  새로 만들지 않고 **원본 객체를 그대로 재사용**해 `bReTry` 플래그만 세팅한 뒤 재큐잉한다.
  슬라이싱 버그가 사라지는 것은 물론, 이미 DB가 지연되고 있는 상황에서 불필요한 heap
  할당/해제 한 쌍도 없어진다. 재큐잉이 실패하면(`Push()`가 0을 반환 — 서비스 종료 시점과
  겹친 경우) 해당 객체는 직접 해제된다(누수 방지).
- `InitMySQL`은 호출 시작 시 `_bStopThread`를 `false`로 재설정해, `StopThread()`
  이후 서비스를 다시 시작하는 재시작 시나리오에서도 워커 스레드들이 정상적으로 큐를
  처리한다(기존에는 이 초기화가 없어 재시작 시 워커가 즉시 종료 조건으로 빠지는 문제가 있었음).
  또한 `_pMySQLConnPools` 배열을 `new CMySQLConnPool*[_nDBCount]()`로 값 초기화해,
  초기화 도중 일부만 생성된 상태에서 실패해도 `ClearMySQLConnPools()`가 미생성 슬롯을
  쓰레기 포인터로 delete하지 않게 한다. 풀 할당(`new CMySQLConnPool(...)`) 실패나
  `Init()` 실패 시에도 이미 만들어진 앞쪽 풀들까지 `ClearMySQLConnPools()`로 함께
  정리한 뒤 반환한다(기존에는 이 정리가 없어 누수가 있었음). 각 DB 노드의 풀을 생성할
  때는 `TReconnectConfig.nWorkerCount`를 `max(4, _nMaxThreadCnt / 4)`로 산정해 전달함으로써,
  재연결 워커 수가 풀 크기(= DB 비동기 워커 스레드 수)에 비례하도록 한다.
- `StartIoThreads()`는 워커 스레드 생성에 `std::bind` 대신 `this`만 캡처하는 람다
  (`[this]() { RunningThread(); }`)를 사용한다. `std::bind`는 내부적으로 타입 소거된 호출
  객체를 만들어 컴파일러 인라인 최적화가 잘 들어가지 않는 경우가 많아, 더 가볍고
  `CMySQLConnPool`의 워커 스레드들과도 스타일이 일치하는 람다를 택했다. 다만 두 방식
  모두 캡처/바인딩되는 것은 동일한 raw `this` 포인터라서, 댕글링 포인터에 대한 안전성
  자체는 동일하다(`std::bind`가 더 안전한 것은 아님).
- `Action()`의 지연 쿼리 경고는 빌드 구성에 따라 임계값이 다르다 — 디버그 빌드는 300ms,
  릴리즈 빌드는 1000ms 이상 걸린 쿼리에 대해 경고 로그를 남긴다.
- `Clear()`는 DB 요청 큐(`_queueDBAsyncRq`)를 비우는 역할만 담당한다. 등록된 핸들러
  (`_mapCommand`)는 `Clear()`의 영향을 받지 않으므로, 초기화가 중간에 실패해 `Clear()`가
  호출되어도 `Regist()`로 등록해둔 핸들러는 그대로 유지된다.

### 11.2 스레드 생성

`StartIoThreads()`는 `_nMaxThreadCnt`개의 워커 스레드를 람다(`[this]() { RunningThread(); }`)로
생성한다(`gpThreadManager`를 통해). 각 워커는 `RunningThread()` → `Action()`으로 이어지는
루프를 돌며 큐에서 요청을 꺼내 처리한다. `_nMaxThreadCnt`는 `nMaxThreadCnt`가 0으로
주어지면 `SYSTEM::CoreCount()`(물리 코어 수)로 자동 산정된다.

### 11.3 종료 시 잔여 작업 처리 — `FlushRemainingTasks()`

프로세스 종료 등으로 워커 스레드들을 더 기다릴 수 없는 상황에서, 큐에 남은 요청들을
비동기 워커 대신 동기적으로 마저 처리하기 위한 함수다. `~CMySQLAsyncSrv()` 소멸자
맨 앞에서, `StopThread()`(워커 종료)·`Clear()`(요청 큐 정리)·`ClearMySQLConnPools()`(커넥션
풀 해제)보다 먼저 호출한다 — 워커를 세우거나 커넥션 풀을 해제하기 전에 남은 요청을
먼저 다 처리해 둠으로써, 아직 처리되지 않은 요청이 워커 종료·풀 해제와 타이밍이 겹쳐
유실되거나(요청이 처리되지 못한 채 `Clear()`로 그냥 비워짐) 이미 해제된 풀을 참조하는
일이 없게 한다.

1. `_bStopThread`를 `true`로 설정하고 `_cva.notify_all()`을 호출해, 이후 새 `Push()`를 막고
   대기 중이던 워커들이 종료 조건을 확인하도록 깨운다.
2. `_mutex`를 짧게 잡은 상태에서 `_queueDBAsyncRq` 전체를 지역 임시 큐(`tempQueue`)로
   `std::move`한다 — 잠금 구간을 큐 이관 한 번으로 최소화해, 그 사이 워커 스레드가 오래
   블로킹되지 않게 한다.
3. 잠금을 푼 뒤, 임시 큐에 옮겨 담은 요청들을 하나씩 꺼내 `_mapCommand`에서 핸들러를 찾아
   `ProcessAsyncCall()`을 호출부 스레드에서 직접 실행한다. `Action()`의 워커 루프와 동일하게
   핸들러를 찾지 못하거나 처리 결과가 `EDBReturnType::OK`가 아니면 에러를 로그로 남기고,
   각 요청은 처리 후 `SAFE_DELETE`로 해제한다.
4. 큐가 빌 때까지 반복한 뒤 완료 로그를 남기고 반환한다.

* `Action()`의 워커 루프에 있던 타임아웃 재시도 로직(§11.1의 `bReTry` 재큐잉)은 여기에는
  없다 — 종료 처리 경로이므로 실패한 요청을 다시 큐에 넣지 않고 에러 로그만 남기고 넘어간다.
* 큐를 옮겨받은 뒤(2단계) 처리하는 동안(3단계)은 `_mutex`를 잡지 않으므로,
  `FlushRemainingTasks()` 실행 중에도 다른 스레드가 `GetQueryQueueSize()`/`IsEmpty()` 같은
  조회 함수를 호출하는 것 자체는 안전하다 (다만 이미 `_bStopThread`가 켜진 뒤라 `Push()`는
  더 이상 큐에 쌓이지 않는다).

### 11.4 요청 큐 백프레셔 — `WaitPushCapacity()`

`_queueDBAsyncRq`는 크기 상한이 없는 큐다. 지금까지는 `MAX_WARNING_QUERY_QUEUE_SIZE`(10만 건)를
넘기면 경고 로그만 남길 뿐, 실제로 큐가 계속 커지는 것을 막지는 못했다 — 워커의 처리 속도보다
`Push()`가 더 빠른 상황이 지속되면 큐가 무한정 쌓여 메모리를 소모할 수 있다. `WaitPushCapacity()`는
이 문제를 해결하기 위한 백프레셔(back-pressure) 장치로, "생산자(요청을 넣는 쪽)를 큐 크기 기준으로
직접 감속시키는" 방식이다.

**추가된 요소**

- `_cvProducer` (`std::condition_variable`) — 큐 공간 부족으로 대기 중인 생산자 전용 조건 변수.
  워커 대기용 `_cva`와 분리되어 있어, 소비자(워커)를 깨우는 알림과 생산자를 깨우는 알림이
  서로 불필요하게 섞이지 않는다.
- `WaitPushCapacity(size_t maxCapacity)` (public, 인라인) — 큐 크기가 `maxCapacity` 미만이 될
  때까지, 또는 `_bStopThread`가 켜질 때까지 `_cvProducer`에서 대기한다. 호출부는 `Push()` 앞에서
  이 함수를 호출해 큐가 일정 크기 이하로 유지되도록 스스로 속도를 늦출 수 있다.

**깨우는 지점**

| 지점 | 동작 |
|---|---|
| `Pop()` | 워커가 항목을 하나 꺼내 공간이 하나 생길 때마다 `_cvProducer.notify_one()`으로 대기 중인 생산자 하나를 깨움. `Push()`와 동일하게 `_mutex` 잠금을 해제한 뒤(블록 스코프 종료 후) notify하여 lock-and-wake-under-lock을 피함 |
| `StopThread()` | `_cva.notify_all()`과 함께 `_cvProducer.notify_all()`도 호출해, 대기 중이던 모든 생산자가 종료 조건(`_bStopThread`)을 확인하고 빠져나가게 함 |
| `FlushRemainingTasks()` | 종료 플래그를 세우는 1단계에서 `_cva.notify_all()`과 함께 `_cvProducer.notify_all()`도 함께 호출해, 대기 중인 생산자가 flush 과정에 걸려 무한 대기하지 않도록 함 |

**동작 원리**

1. 생산자 스레드가 `WaitPushCapacity(maxCapacity)`를 호출하면, 큐 크기가 `maxCapacity` 미만이
   될 때까지 블로킹 대기한다 (조건이 이미 만족되면 즉시 반환).
2. 워커가 `Pop()`으로 큐를 소비할 때마다 대기 중인 생산자 하나가 깨어나 조건을 재검사한다.
3. 조건을 만족하면 대기를 빠져나와 `Push()`를 호출해 요청을 큐에 넣는다.
4. 서비스 종료(`StopThread()`/`FlushRemainingTasks()`) 시에는 큐 크기와 무관하게 즉시 깨어나
   대기 상태에서 빠져나온다 — `_bStopThread`가 조건식에 포함되어 있기 때문이다.

**설계 포인트**

- `Pop()`은 이제 `unique_lock`을 전체 함수 스코프가 아니라 블록으로 감싸, 큐 조작이 끝나는
  즉시 락을 해제한 뒤 `_cvProducer.notify_one()`을 호출한다. 락을 쥔 채로 notify하면 깨어난
  생산자가 곧바로 락을 잡지 못하고 다시 잠드는 컨텍스트 스위칭 낭비가 생기므로, `Push()`에서
  이미 쓰던 것과 동일한 패턴을 `Pop()`에도 맞춘 것이다.
- `WaitPushCapacity()` 자체는 큐에 넣는 동작(`Push()`)을 수행하지 않는다 — 순수하게 "지금
  넣어도 되는 상태인지"를 기다리는 게이트 역할만 하며, 실제 `Push()` 호출은 별도로 해야 한다.
- `maxCapacity`는 호출부에서 자유롭게 정할 수 있는 값이라, 상황(DB 노드별 처리량, 요청
  긴급도 등)에 따라 다른 상한을 적용할 수 있다.

### 11.5 미완료 요청 수 카운터 — `Outstanding Requests`

큐에 들어간 뒤 아직 최종적으로 끝나지 않은 요청이 몇 개인지를 별도의 원자 카운터로
추적하는 기능이다. 큐 크기(`GetQueryQueueSize()`)는 "아직 워커가 꺼내가지 않은 개수"만
보여주는 반면, 이 카운터는 "꺼내갔지만 아직 완전히 끝나지 않은 것"까지 포함해 실제로
시스템에 떠 있는 요청 수를 나타낸다.

**추가된 요소**

| 멤버 | 설명 |
|---|---|
| `_nOutstandingRequests` (`std::atomic<int32>`) | 미완료 요청 수 카운터. 기본값 0으로 초기화 |
| `GetOutstandingRequests() const` | 현재 값을 조회 (`memory_order_relaxed`) |
| `AddOutstandingRequest()` | 카운터를 1 증가 (`memory_order_relaxed`) — 요청을 생성해 `Push()`하는 외부 호출부가 사용 |
| `SubOutstandingRequest()` | 카운터를 1 감소 (`memory_order_relaxed`) — 요청이 최종적으로 끝나는 지점에서 클래스 내부(`Action()`, `FlushRemainingTasks()`)가 호출 |

세 함수 모두 헤더에 인라인으로 정의되어 있고, 단순 증감 카운터라서 `memory_order_relaxed`로
충분하다 — 다른 메모리 접근과의 순서를 보장할 필요 없이 카운트 값 자체만 정확하면 되기
때문이다.

**감소 시점 — 두 경로 모두에서 "최종적으로 끝날 때"만 호출**

- `Action()`: 워커 루프에서 요청을 완전히 처리(성공이든, 타임아웃 재시도가 아닌 최종 실패든)한
  뒤, `SAFE_DELETE(pAsyncRq)` 직전에 `SubOutstandingRequest()`를 호출한다. 싱글턴 인스턴스
  자신의 메서드 안에서 호출되는 것이므로 `CMySQLAsyncSrv::Instance()->`를 다시 거치지 않고
  암묵적 `this`로 바로 호출한다.
- `FlushRemainingTasks()`: 종료 시점에 메인 스레드가 남은 요청을 동기 처리하는 루프에서도,
  핸들러를 찾아 `ProcessAsyncCall()`을 실행한 경우(성공/실패 불문) `SubOutstandingRequest()`를
  호출한 뒤 `SAFE_DELETE`한다. `Action()`과 동일한 기준으로 카운터를 관리해, 정상 종료 경로든
  강제 flush 경로든 요청이 "처리를 시도해 끝난" 시점에 항상 카운터가 감소한다.

**카운터를 건드리지 않는 유일한 경로**

- `Action()`의 `_mapCommand.end() == it`(핸들러를 찾지 못한 경우)와, `FlushRemainingTasks()`의
  동일한 분기(`else` — 핸들러 없음) 양쪽 모두 `SubOutstandingRequest()`를 호출하지 않는다.
  이 경로는 `callIdent`에 대응하는 핸들러가 애초에 등록되어 있지 않은, 설정 누락에 가까운
  예외적 상황이다. 카운터가 안 맞는 상태로 남을 수 있음을 인지한 채로, 정상적인 운영에서는
  거의 발생하지 않는 경로이므로 의도적으로 그대로 두었다.
- 타임아웃으로 재큐잉되는 경로(`Ret == TIMEOUT && bReTry == false` → `Push()`로 재투입)는
  "최종적으로 끝난" 것이 아니라 다시 큐로 돌아가는 것이므로 이 시점에는 감소시키지 않는다 —
  재큐잉된 요청이 이후 다시 `Action()`을 통과할 때(성공하든 최종 실패하든) 그때 비로소
  `SubOutstandingRequest()`가 호출된다.
