# COdbcConnPool 설계 문서

> 이 문서는 `COdbcConnPool`을 중심으로 작성되었으며, 그 위에서 큐+워커로 동작하는
> 비동기 서비스 계층 `COdbcAsyncSrv`(§11)와, 이를 도메인별로 소유·관리하는
> `CDbServiceManager`(§12), 핸들러 등록 매크로 `DBAsyncHandler.h`(§11.1) 및 공용
> 요청 게시 헬퍼 `DBAsyncPushHelper.h`(§11.6)까지 함께 다룬다.

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
| O(1) 프리 슬롯 획득 | 비어 있고 연결된 슬롯 인덱스만 담아두는 전용 큐(`_freeSlotQueue`)에서 즉시 후보를 꺼낸다 — 풀이 바쁠 때도 풀 전체를 스캔하지 않는다 |
| 비동기 자동 재연결 | 헬스체크 스레드가 끊어진 슬롯을 감지하고, 별도 워커 풀이 실제 재연결(I/O)을 병렬 수행 |
| 지수 백오프 + 지터 | DB 전체 장애 시 모든 슬롯이 동시에 재시도하는 connection storm을 방지 |
| 이벤트 기반 정밀 재시도 스케줄링 | 백오프 대기는 `CDelayedTaskQueue`(§3의 `_delayedTaskQueue`)가 전담하며, 계산된 지연 시간이 정확히 지난 시점에 콜백이 1회 실행되어 재시도를 트리거한다. 헬스체크 스캔 주기(500ms)와는 독립적으로 동작한다 |
| 동적 워커 수 조정 | `SetReconnectConfig`로 런타임 중 재연결 워커 수/백오프 정책을 조정 가능 |
| 격리(Quarantine) 큐 — 이중 시각 관리 | 교체된 낡은 커넥션에 참조가 남아있으면 즉시 삭제하지 않고 격리한다. "최초 격리 시각"(`enqueueTime`, 절대 갱신 안 됨)과 "마지막 경고 로그 시각"(`lastLogTime`, 경고마다 갱신)을 분리해 관리하므로, 5분마다 경고 로그를 남기는 것과 무관하게 10분 강제 정리 타임아웃이 정확히 동작한다 |
| Safe Leak | 프로세스 종료 시점까지 참조가 남은 커넥션은 삭제를 포기(누수)하여 UAF 크래시를 방지 |
| False sharing 방지 | 슬롯별 원자 배열(`_pOdbcConns`, `_pRefCount`, `_pReconnecting`, `_pRetryFailCount`, `_pInFreeSlotQueue`)을 `CachePaddedAtomic<T>[]`로, `_slotLocks`를 슬롯별 단독 락인 `PLock[]`로 구성해 슬롯 간 캐시라인 공유를 차단 |
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
| `_pRetryFailCount` | 슬롯별 연속 재연결 실패 횟수 (`CachePaddedAtomic<int32>[]`). 지수 백오프 shift 계산에 쓰이는 동시에, 0보다 크면 "이미 `_delayedTaskQueue`에 재시도가 예약된 상태"임을 나타내는 상태 플래그 역할도 겸함(§5.2) |
| `_pInFreeSlotQueue` | 슬롯별 "현재 `_freeSlotQueue`에 이미 들어가 있는지" 플래그 (`CachePaddedAtomic<bool>[]`). `EnqueueFreeSlot()`의 중복 삽입 방지용 CAS 게이트(§5.1) |

> `_quarantineQueue`가 `&_pRefCount[i].value` 주소를 그대로 저장하므로, 위 배열들은 런타임 중
> 재할당(make_unique 재호출 등)이 절대 금지된다. 각 슬롯이 `CachePaddedAtomic<T>`로 캐시라인
> 하나씩을 점유해, 서로 다른 슬롯을 동시에 다루는 스레드들이 false sharing으로 서로의
> 캐시라인을 무효화시키는 것을 막는다.

### 헬스체크 / 재연결 워커 / 프리 슬롯 큐
| 변수 | 설명 |
|---|---|
| `_healthCheckThreadMgr` / `_bStopHealthCheck` / `_nHealthCheckIntervalMs` | 헬스체크 스레드 관리, 종료 신호, 주기(기본 500ms) |
| `_delayedTaskQueue` / `_delayedTaskThreadMgr` | 재연결 실패 시 계산된 백오프 지연을 정확한 시각에 1회 실행하기 위한 `CDelayedTaskQueue`와, 이를 처리하는 전담 스레드(단일 컨슈머) |
| `_freeSlotMutex` / `_freeSlotQueue` | 비어 있고 연결된 슬롯 인덱스만 담아두는 O(1) 프리 큐와 이를 보호하는 뮤텍스(§5.1) |
| `_reconnectWorkerMgr` / `_bStopReconnectWorkers` | 재연결 워커 스레드 관리, 전체 종료 신호 |
| `_nCurrentWorkerCount` / `_nDesiredWorkerCount` | 현재 워커 수 / 목표 워커 수 |
| `_reconnectQueueMutex` / `_reconnectQueueCv` / `_reconnectPendingSlots` | 재연결 대기열과 그 동기화 객체 |
| `_globalQuarantineLock` / `_quarantineQueue` | 격리 큐와 이를 보호하는 전역 스핀락 |

## 4. 멤버 함수 설명

### Public API
| 함수 | 설명 |
|---|---|
| `Init(dbClass, dsn, reconnectConfig)` | 풀을 초기화하고 DSN으로 커넥션을 동기적으로 채운다. 잘못된 `reconnectConfig`는 기본값으로 대체된다. 연결에 성공한 슬롯은 즉시 `EnqueueFreeSlot()`으로 프리 큐에 등록된다 |
| `GetOdbcConn(nType)` | 슬롯의 참조 카운트를 증가시키고 커넥션을 반환. 끊어진 슬롯이면 즉시 `nullptr`을 반환하고 카운트를 되돌린다 |
| `ReleaseOdbcConn(nType)` | 참조 카운트를 감소시켜 슬롯을 반납. 감소가 마지막 참조였고(참조 0) 커넥션이 여전히 연결돼 있으면 `EnqueueFreeSlot()`으로 프리 큐에 등록한다. 연결 상태 확인은 반드시 참조 카운트 감소 "전"에 수행해 `ApplyReconnectedConn()`과의 use-after-free를 방지한다 |
| `GetPooledConnUnsafe(nType)` | `PopFreeSlotIndex`로 이미 선점된 슬롯을 카운트 변경 없이 조회 (`OdbcConnGuard` 전용) |
| `PopFreeSlotIndex()` | `_freeSlotQueue`에서 후보를 하나씩 꺼내 참조 카운트 0→1 CAS로 즉시 선점한다. 큐에 있는 동안 연결이 끊겼거나 다른 경로(`GetOdbcConn` 명시적 인덱스)로 이미 선점된 후보는 버리고 다음 후보로 넘어간다. 큐가 비어 있으면 즉시 `-1` 반환 |
| `SetReconnectConfig(cfg)` | 백오프/워커 수 정책을 런타임에 변경. 유효성 실패 시 전체 거부(부분 적용 없음) |
| `GetReconnectConfig()` | 현재 정책 스냅샷 조회 (모니터링용) |

### Protected 내부 로직
| 함수 | 설명 |
|---|---|
| `Clear()` | 모든 슬롯을 정리. 프리 큐를 먼저 비우고 `_pInFreeSlotQueue` 플래그를 전부 리셋한 뒤, 참조가 남은 슬롯은 격리 큐로 보냄 (Shutdown 전용) |
| `IsValidIndex(nType)` | 슬롯 인덱스 범위 검사 |
| `ValidateReconnectConfig(cfg)` | 재연결 설정값의 상식적 범위 검사 (Init/SetReconnectConfig 공용) |
| `TryReconnect(nType)` | 새 커넥션을 생성하고 실제 연결까지 시도하는 블로킹 I/O 로직 |
| `ApplyReconnectedConn(nType, pNewConn)` | 새 커넥션으로 슬롯을 스왑하고, 낡은 커넥션을 안전하게 삭제 또는 격리한다. 진입 시점에 슬롯 참조 카운트가 이미 0이 아니면(다른 경로로 재선점된 정상적인 레이스) 스왑을 포기하고 `pNewConn`을 폐기한 뒤 `false`를 반환한다 — 호출부(`ReconnectWorkerLoop`)는 이 반환값으로 실제 적용 여부를 정확히 구분해 `OnReconnectSucceeded`/`OnReconnectFailed`를 분기한다. 스왑이 실제로 일어난 경우(격리로 빠지든 즉시 삭제되든)는 항상 `true` |
| `ScheduleRetry(nType)` | 실패 횟수(`_pRetryFailCount`)를 늘리고 지수 백오프+지터로 지연 시간을 계산한 뒤, `_delayedTaskQueue.Reserve()`로 그 시간 뒤 1회 실행될 재시도 콜백을 예약 |
| `OnReconnectFailed(nType)` | `ScheduleRetry(nType)` 호출로 위임. `ApplyReconnectedConn()`이 스왑을 포기한 경우(반환값 `false`)에도 이 경로를 타 재시도가 예약된다 |
| `OnReconnectSucceeded(nType)` | `_pRetryFailCount`를 0으로 초기화 (백오프 상태 리셋). `ApplyReconnectedConn()`이 실제로 스왑을 적용했을 때(반환값 `true`)만 호출된다 |
| `HealthCheckLoop()` | 격리 큐 청소 + 끊어진 슬롯 스캔. 격리 큐 항목의 전체 경과 시간은 `enqueueTime` 기준으로, 경고 쿨다운은 `lastLogTime` 기준으로 각각 독립적으로 판단한다. `_pReconnecting`을 CAS로 선점한 뒤 `_pRetryFailCount == 0`(아직 예약된 재시도가 없는 슬롯)인 경우에만 즉시 재연결 큐에 등록하고, 실패 이력이 있는 슬롯은 `_delayedTaskQueue`의 예약에 맡기고 그냥 넘어감 (블로킹 I/O 없음) |
| `StartHealthCheckThread()` / `StopHealthCheckThread()` | 헬스체크 스레드 기동/안전 종료(join) |
| `DelayedTaskLoop()` | `_delayedTaskQueue.ProcessExpiredTasks()`를 호출해 만료된 재시도 콜백들을 실행하는 루프 |
| `StartDelayedTaskThread()` / `StopDelayedTaskThread()` | 지연 타이머 전담 스레드 기동 / `_delayedTaskQueue.Stop()` 후 안전 종료(join) |
| `ReconnectWorkerLoop()` | 대기열에서 슬롯을 꺼내 실제 `TryReconnect` + `ApplyReconnectedConn`을 수행하는 워커 루프. 반환값에 따라 `OnReconnectSucceeded`/`OnReconnectFailed`를 분기한다 |
| `StartReconnectWorkers(n)` / `StopReconnectWorkers()` | 재연결 워커 풀 기동/종료 |
| `SetWorkerCount(n)` | 목표 워커 수 갱신. 확대는 즉시 스폰, 축소는 워커가 스스로 종료하도록 유도 |
| `TryExitIfExcess()` | 현재 워커가 초과 인원인지 CAS로 판정하고, 맞다면 스스로 종료 |
| `EnqueueReconnect(nType)` | 재연결 대기열에 슬롯을 넣고 워커 하나를 깨움 |
| `EnqueueFreeSlot(nType)` | 슬롯을 프리 큐에 등록한다. `_pInFreeSlotQueue[nType]`을 CAS(false→true)로 먼저 확인해, 이미 큐에 들어가 있으면 중복 삽입하지 않는다. `Init()`/`ReleaseOdbcConn()`/`ApplyReconnectedConn()` 세 곳에서 "이 슬롯이 지금 사용 가능해졌다"는 이벤트가 발생할 때마다 호출된다 |

## 5. 동작 흐름

### 5.1 커넥션 대여/반납 (핫패스)
1. `OdbcConnGuard` 생성 시 `PopFreeSlotIndex()`로 `_freeSlotQueue`에서 후보를 꺼내 참조 카운트
   0→1 CAS로 즉시 선점한다 (실질 O(1) — 후보가 큐에 있는 동안 연결이 끊겼거나 이미 다른 경로로
   선점된 경우에만 다음 후보로 넘어간다).
2. `GetPooledConnUnsafe()`로 커넥션 포인터 조회
3. 소멸 시 `ReleaseOdbcConn()`으로 참조 카운트 반납. 반납으로 참조가 정확히 0이 되고 커넥션이
   여전히 연결돼 있으면, 같은 함수 안에서 `EnqueueFreeSlot()`을 호출해 다음 대여자를 위해 다시
   프리 큐에 등록한다.

> `GetOdbcConn(nType)`으로 특정 슬롯을 직접 지정해 대여하는 경로는 프리 큐를 거치지 않는다.
> 이 경로로 선점된 슬롯이 여전히 프리 큐 안에 남아 있는 채로 `PopFreeSlotIndex()`가 그 인덱스를
> 뽑아가더라도, 참조 카운트 CAS가 실패해 안전하게 다음 후보로 넘어가므로 정확성에는 문제가 없다.

### 5.2 자동 재연결
1. `HealthCheckLoop()`이 500ms마다 순회하며 참조 카운트 0 & 연결 끊김인 슬롯을 찾고, `_pReconnecting`을
   CAS로 선점한다.
2. 선점에 성공한 슬롯 중 **`_pRetryFailCount == 0`(과거 실패 이력이 없어 아직 예약된 재시도가 없는
   슬롯)만** `EnqueueReconnect()`로 즉시 대기열에 등록해 워커를 깨운다. `_pRetryFailCount > 0`인
   슬롯은 이미 `_delayedTaskQueue`에 재시도가 예약돼 있다는 뜻이므로, 이번 헬스체크 순회에서는
   `_pReconnecting`만 다시 반납하고 개입하지 않는다.
3. `ReconnectWorkerLoop()`이 `TryReconnect()`로 블로킹 I/O 수행
4. 연결에 성공하면 `ApplyReconnectedConn()`으로 슬롯 스왑을 시도한다. 이 시점에 슬롯이 아직
   참조 카운트 0인 상태여야 실제로 스왑이 적용되며(스왑된 슬롯은 프리 큐에도 다시 등록된다),
   그 사이 다른 경로로 슬롯이 재선점됐다면(드문 레이스) 스왑을 포기하고 새로 만든 커넥션을
   버린다. 호출부는 `ApplyReconnectedConn()`의 반환값으로 이 둘을 구분해, 실제 적용됐을 때만
   `OnReconnectSucceeded()`로 `_pRetryFailCount`를 리셋하고, 포기된 경우는 `OnReconnectFailed()`로
   재시도를 다시 예약한다.
5. 연결 실패, 또는 스왑이 포기된 경우 모두 `OnReconnectFailed()` → `ScheduleRetry()`가 실패
   횟수를 늘려 지수 백오프+지터 지연을 계산하고, `_delayedTaskQueue.Reserve(지연ms, 콜백)`으로
   **정확히 그 시간 뒤 1회 실행되는** 재시도 콜백을 예약한다. 콜백은 만료 시점에 슬롯이 여전히
   재연결이 필요한 상태인지 재확인한 뒤 `EnqueueReconnect()`하거나, 그 사이 다른 경로로 이미
   해소됐다면 `_pReconnecting`만 반납한다. 이 재시도 시각은 헬스체크 주기(500ms)와 무관하게
   백오프 계산값과 정확히 일치한다.

> 종료 시에는 `StopHealthCheckThread()` → `StopDelayedTaskThread()` → `StopReconnectWorkers()`
> 순으로 정지하고(`~COdbcConnPool()`/재`Init()` 공통), 반대로 시작 시에는 `Init()`이
> `StartDelayedTaskThread()` → `StartHealthCheckThread()` → `StartReconnectWorkers()` 순으로
> 기동한다 — 헬스체크나 재연결 워커가 첫 실패를 만들어 `ScheduleRetry()`를 호출하기 전에
> 지연 타이머 스레드가 먼저 요청을 받을 준비를 갖추도록 하기 위함이다.

### 5.3 격리 큐의 두 시각 필드와 강제 정리
`TQuarantineItem`은 격리된 커넥션마다 `enqueueTime`(최초 격리 시각, 절대 갱신되지 않음)과
`lastLogTime`(마지막 경고 로그 시각, 5분 경고마다 갱신)을 별도로 들고 있다.

- `HealthCheckLoop()`은 매 순회마다 격리 큐 전체를 훑으며, 참조 카운트가 0이 됐으면 즉시 삭제
  대상으로 옮긴다.
- 아직 참조가 남아있는 항목은 `now - enqueueTime`이 `FORCE_CLEANUP_TIMEOUT_MS`(10분)를 넘었는지로
  강제 정리 여부를 판단하고, `now - lastLogTime`이 `LOG_ALERT_INTERVAL_MS`(5분)를 넘었을 때만
  경고 로그를 남기며 그 시점의 `lastLogTime`을 갱신한다.
- 두 필드가 분리돼 있어, 5분마다 반복되는 경고 로그가 10분 강제 정리 판정 시점을 뒤로 미루는
  일이 없다 — `enqueueTime`은 격리된 순간 이후 어떤 경로에서도 다시 쓰이지 않는다.

### 5.4 워커 수 동적 조정
- 확대: `_nDesiredWorkerCount`를 CAS로 목표까지 끌어올리고 부족분만큼 즉시 스폰
- 축소: 스레드를 직접 종료시키지 않고 조건 변수만 깨움 → 각 워커가 다음 순회에서
  `TryExitIfExcess()`로 스스로 초과 여부 판단 후 종료. 반복/역전 호출에도 최종 목표치로 정확히 수렴

## 6. 장단점

### 장점
- 대여/반납 핫패스가 원자 연산만 사용해 뮤텍스 경합이 없다.
- 프리 슬롯 탐색이 전용 큐 기반 O(1)이라, 풀이 바쁠수록 느려지는 스캔 비용이 없다.
- 재연결 I/O가 별도 워커 풀에서 병렬 처리되어 헬스체크나 대여 경로를 막지 않는다.
- 지수 백오프 + 지터로 DB 장애 시 재연결 폭주(connection storm)를 방지한다.
- 격리 큐와 Safe Leak 정책으로 UAF 크래시 위험을 구조적으로 차단하고, 강제 정리 타임아웃이
  경고 로그 빈도와 무관하게 정확히 동작한다.
- 워커 수/백오프 정책을 서비스 운영 중 무중단으로 조정할 수 있다.
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
- `GetOdbcConn(nType)`(명시적 인덱스 대여)은 프리 큐와 동기화되지 않는다 — 정확성에는 영향이
  없지만, 이 경로 사용 빈도가 높으면 프리 큐에 무효 후보가 남아 `PopFreeSlotIndex()`가 그 후보를
  버리고 다음으로 넘어가는 낭비가 소폭 생길 수 있다.

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
| DB 비동기 워커 | `COdbcAsyncSrv`의 요청 큐에서 쿼리 요청을 꺼내 `COdbcConnPool`에서 커넥션을 빌려 실제 쿼리(블로킹) 실행 | 예상 동시 DB 요청 수 기준. `COdbcConnPool`의 `_nMaxPoolSize`를 넘지 않는 선에서 결정 |
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
- DB 비동기 워커 스레드 수(`COdbcAsyncSrv::StartService`의 `nMaxThreadCnt`)와 각
  `COdbcConnPool`의 크기는 같은 값으로 맞춰진다(§11.1 — `InitOdbc`가 자동으로
  `_nMaxThreadCnt`를 각 풀의 `nMaxPoolSize`로 사용) — 워커가 풀보다 많으면
  `PopFreeSlotIndex()` 실패(-1)가 늘어난다.
- 위 수치는 시작점일 뿐이며, 최종적으로는 실제 부하 테스트(동접자 수, DB 쿼리
  QPS, 패킷 처리량)로 튜닝해야 한다.

## 9. 사용법 — `COdbcConnPool` 단독 사용

`COdbcConnPool`은 그 자체로도 독립적인 컴포넌트라, `COdbcAsyncSrv`/`CDbServiceManager`
없이 직접 생성해 쓸 수 있다.

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
- 풀 소멸 시 `~COdbcConnPool()`이 헬스체크 → 지연 타이머 → 재연결 워커 스레드를 순서대로 먼저
  종료한 뒤 `Clear()`로 자원을 정리한다(§5.2 참고).
- 여러 DB(계정/게임/로그 등)를 다루는 실제 서비스에서는 이렇게 `COdbcConnPool`을 직접 여러 개
  만들어 관리하기보다, §11의 `COdbcAsyncSrv`와 §12의 `CDbServiceManager`를 통해 DB 노드
  목록으로부터 자동으로 풀 집합을 구성하는 쪽을 권장한다.

## 10. 할당자(Allocator) 설계

`COdbcConnPool`은 `class COdbcConnPool : public BaseAllocator`로 선언되어 있다. 즉 이
클래스를 직접 `new`/`delete`하면 전역 `::operator new`/`delete`가 아니라 `BaseAllocator`가
오버라이드한 `operator new`/`delete`가 호출되어, 프로젝트의 `RawAllocator`(mimalloc/jemalloc/
tcmalloc/malloc 중 컴파일 타임 선택) 경로를 탄다.

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

### 10.3 `COdbcAsyncSrv`/`CDbServiceManager`는 `BaseAllocator`를 상속하지 않는다

`COdbcAsyncSrv`와 `CDbServiceManager`는 둘 다 `BaseAllocator`를 상속하지 않는 평범한 클래스다.
`CDbServiceManager::Instance()`가 함수-지역 `static`(Meyer's singleton, §12)으로 생성되고,
그 안의 `COdbcAsyncSrv` 인스턴스는 `std::make_unique<COdbcAsyncSrv>()`로 생성된다.
`std::make_unique`/`std::make_shared`는 컨트롤 블록과 객체를 하나로 묶어 자체 할당 경로로
확보하고 `operator new`를 거치지 않으므로, 설령 이 타입들이 `BaseAllocator`를 상속하더라도
효과가 없었을 것이다 — 두 클래스 다 프로세스 수명 동안 도메인당 1개만 생성되는 경량 객체라,
할당자를 별도로 신경 쓸 실익 자체가 크지 않다.

## 11. 비동기 서비스 계층 — `COdbcAsyncSrv`

풀(`COdbcConnPool`) 위에, DB 노드별로 풀을 관리하며 공용 요청 큐 + 워커 스레드 풀로 쿼리를
비동기 처리하는 서비스 계층이다. `COdbcAsyncSrv` 자신은 싱글턴이 아니라 "DB 서비스 하나를
나타내는 재사용 가능한 부품"이며, 여러 인스턴스(멤버/게임/로그 등 도메인별)를 소유·관리하는
책임은 §12의 `CDbServiceManager`가 진다.

### 11.1 구조 요약

- 요청 베이스 구조체(`st_DBAsyncRq`/`st_DBAsyncRp`)와 핸들러 인터페이스(`CDBAsyncSrvHandler`)는
  `DBAsyncSrv.h`에 별도로 분리되어 있다. 둘 다 `BaseAllocator`를 상속하고 `virtual` 소멸자를
  가진다. 이 헤더는 자연 정렬(패딩 없음)을 쓴다 — `st_DBAsyncRp`의 `pthis` 포인터가 정렬된
  오프셋에 위치해야 하고, `st_DBAsyncRq`를 상속하는 파생 구조체가 `std::vector` 등 정렬에
  민감한 멤버를 안전하게 추가할 수 있어야 하기 때문이다.
- `Regist(callIdent, handler)`로 명령어별 핸들러를 등록해두면, `Push()`로 큐에 들어온
  `st_DBAsyncRq` 요청을 워커 스레드들이 `Pop()` → `callIdent`로 핸들러 조회 → 실행한다.
  핸들러 조회는 `std::unordered_map`을 사용해 매 쿼리마다의 조회 비용을 O(1) 평균으로 유지한다.
  `Regist()`는 `emplace()`의 결과로 실제 등록된(또는 이미 있던) 항목을 반환하며, 중복 등록
  시도는 로그로 알린다.
- 핸들러 클래스 작성과 `Regist()` 호출을 매번 손으로 반복하지 않도록, `DBAsyncHandler.h`가
  `DECLARE_DBASYNC_HANDLER_EX(srvClass, command)` 매크로를 제공한다. 이 매크로 한 줄로
  `command##_handler` 클래스 정의(`CDBAsyncSrvHandler` 상속, 생성자/가상 소멸자, `ProcessAsyncCall`
  선언)와, 정적 멤버 `asyncHandler`의 초기화식 안에서 `(srvClass).Regist(command,
  std::make_shared<command##_handler>())`를 호출해 프로그램 시작(정적 초기화) 시점에 자동으로
  핸들러가 등록되게 하는 것까지 한 번에 끝낸다. 호출부는 매크로 뒤에 `ProcessAsyncCall`의 본문만
  `{ ... }`로 이어 붙이면 된다. `srvClass`에는 보통 §12의 `MEMBER_DB_ASYNC` 같은 매크로(즉
  `CDbServiceManager::Instance().MemberDB()` 표현식)를 그대로 넘긴다 — `CDbServiceManager::
  Instance()`가 함수-지역 `static`이라 이 정적 초기화 시점의 호출 순서(SIOF)에서 안전하다(§12).
- 요청 큐 `_queueDBAsyncRq`는 `CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>`로,
  요청은 원시 포인터가 아니라 `std::unique_ptr`로 소유된다. 즉 큐를 떠난 요청 객체는
  수동 해제 없이, `unique_ptr`가 스코프를 벗어나는 시점에 RAII로 자동 해제된다 — 처리
  성공/실패/핸들러 미등록 등 어떤 경로로 빠져나가도 누수나 이중 해제 위험이 없다.
- 큐 동기화는 두 겹으로 이루어진다 — `CChunkedSwapQueue` 자신의 내부 `PLock`이 큐 데이터를
  보호하고, `COdbcAsyncSrv`의 `std::mutex`(`_mutex`) + `std::condition_variable`(`_cva`)는
  그 위에서 블로킹 대기/기상을 담당한다(자세한 구분은 §11.1b). `Push()`는 `_mutex`를 잡은
  채로 `_queueDBAsyncRq.PushAndGetSize()`(큐 내부 락 안에서 푸시와 결과 큐 크기 조회를 원자적으로
  처리)만 수행하고, **락을 해제한 뒤에** `_cva.notify_one()`을 호출한다 — 깨어난 소비자가 곧바로
  같은 락을 다시 잡으려다 막히는 불필요한 컨텍스트 스위치를 피하기 위함이다. `Pop()`이
  `_cva.wait()`의 predicate(`!IsEmpty()`)를 검사하는 구간과 `Push()`의 push 구간은 여전히
  동일한 `_mutex`로 직렬화되므로, "predicate 검사 직후·wait 진입 직전"의 틈에 notify가 끼어들어
  유실되는 lost-wakeup 문제는 notify 위치와 무관하게 발생하지 않는다 — `_bStopThread` 체크/세팅이
  같은 `_mutex`로 직렬화되는 데서 이 보장이 나온다. `Pop()`이 `SwapChunk()`를 호출할 때도 이미
  같은 `_mutex`를 먼저 잡은 뒤이므로, 잠금 순서는 항상 (`_mutex` → 큐 내부 `PLock`)으로 일관되어
  데드락 위험은 없다.
- `st_DBAsyncRq`는 `callIdent`별로 실제 쿼리 데이터를 담은 파생 구조체(예:
  `PRODUCER_DATA_BATCH_REQ`)의 베이스이며 `virtual` 소멸자를 가진다 — 베이스 포인터로
  담긴 `unique_ptr`가 소멸해도 파생 소멸자가 정확히 호출된다.
- 쿼리가 타임아웃되어 처음 재시도될 때는 원본 요청 객체(`unique_ptr`)를 그대로 `std::move`로
  재사용해 `bReTry` 플래그만 세팅한 뒤 재큐잉한다 — 파생 구조체를 통째로 다시 할당하지 않는다.
  `callIdent`는 `move` 전에 로컬 변수로 미리 복사해 둬서 로그에 사용한다. 재큐잉이 실패하면
  (서비스 종료 시점과 겹쳐 `Push()`가 0을 반환한 경우) 해당 `unique_ptr`는 `Push()` 내부에서
  버려지는 즉시 소멸자에 의해 자동 해제된다 — 별도의 명시적 delete 호출이 없다.
- `InitOdbc`는 호출 시작 시 `_bOpen`/`_bStopThread`를 재설정하고, `dbNodeVec`이 비어 있으면
  즉시 실패 처리한다(DB 노드가 하나도 없는 서비스를 성공으로 취급하면, 뒤이어 스폰되는 워커
  스레드가 `_bOpen == false`라 아무 일도 하지 않고 바로 끝나는 무의미한 스레드 생성/소멸
  비용만 생긴다). 각 DB 노드마다 `COdbcConnPool`을 만들 때 풀 크기를 `nMaxThreadCnt`(=DB
  비동기 워커 스레드 수)로, 재연결 워커 수(`TReconnectConfig.nWorkerCount`)는
  `max(4, nMaxThreadCnt / 4)`로 산정해 전달함으로써, 재연결 워커 수가 풀 크기에 비례하도록
  한다.
- `Action()`의 지연 쿼리 경고는 빌드 구성에 따라 임계값이 다르다 — 디버그 빌드는 300ms,
  릴리즈 빌드는 1000ms 이상 걸린 쿼리에 대해 경고 로그를 남긴다. 누적 호출 수를 세는
  `_cumulateCallCnt`는 인스턴스 멤버(`std::atomic<uint64>`)로, `CDbServiceManager`가 멤버/게임/
  로그 등 여러 도메인 인스턴스를 동시에 소유하고 각자 자기 `Action()`을 자기 워커 스레드에서
  돌리기 때문에 도메인별로 독립적으로 집계된다(함수 지역 `static`이었다면 모든 도메인 인스턴스가
  이 카운터를 공유해, "이 도메인에서 몇 번째 지연 쿼리인지"라는 진단 정보의 의미가 깨졌을 것이다).
- `Clear()`는 DB 요청 큐를 비우는 역할만 담당한다. `GetSize()`로 크기를 먼저 읽어
  `SwapChunk(tempQueue, size)`로 그만큼만 옮기는 대신, 전용 `_queueDBAsyncRq.Swap(tempQueue)`로
  그 시점의 큐 전체를 한 번의 락 구간 안에서 통째로 이관한다 — 대상 큐가 비어 있으므로 내부적으로
  컨테이너 자체를 O(1)로 스왑하며, "크기 조회 이후 들어온 새 항목이 이번 드레인에서 누락되는"
  TOCTOU 여지도 함께 없앤다. 옮겨진 `tempQueue`를 `pop()`으로 비우면 각 `unique_ptr`가 소멸하며
  자동으로 메모리를 해제한다. 등록된 핸들러(`_mapCommand`)는 `Clear()`의 영향을 받지 않으므로,
  초기화가 중간에 실패해 `Clear()`가 호출되어도 `Regist()`로 등록해둔 핸들러는 그대로 유지된다.

### 11.1a 소비자별 배치 인출 — `Pop()`의 로컬 큐 스왑

`Pop()`은 매번 락을 잡고 항목 하나씩 꺼내는 대신, 워커(소비자)마다 로컬 큐(`localQueue`,
`Action()`의 스택 변수)를 두고 다음과 같이 동작한다.

1. `localQueue`에 항목이 남아있으면 락 없이 바로 하나 꺼내 반환한다.
2. `localQueue`가 비어 있을 때만 `_mutex`를 잡고, 조건 변수 `_cva`로 전체 큐가 비지 않았거나
   종료 신호가 올 때까지 대기한다.
3. 깨어나면 `_queueDBAsyncRq.SwapChunk(localQueue, 64)`로 **한 번에 최대 64개** 항목을
   전체 큐에서 로컬 큐로 옮겨온 뒤 락을 반환하고, `_cvProducer.notify_all()`로 대기 중인
   생산자를 전부 깨운다(§11.4). 한 번에 여러 개의 여유가 생겼을 수 있으므로, 대기 중인
   프로듀서가 여럿이면 전부 깨워 각자 자기 predicate로 재검증하게 한다 — `notify_one()`이었다면
   한 스레드만 깨어나 나머지는 다음 `Pop()` 호출까지 불필요하게 계속 대기해야 했다.
4. 이후 63개는 다시 락을 잡지 않고 1단계에서 소비된다.

즉 워커 하나가 락을 잡는 빈도가 항목 1개당 1회에서 최대 64개당 1회로 줄어, 다중 워커
환경에서 `_mutex` 경합이 크게 감소한다. `_bStopThread`가 켜졌고 전체 큐와 `localQueue`가
모두 비어 있을 때만 `nullptr`을 반환해 워커 루프가 종료된다.

### 11.1b `CChunkedSwapQueue` 자체의 스레드 안전성과 두 겹의 동기화 구조

`_queueDBAsyncRq`(`CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>`)는 그 자체로 이미
스레드 세이프한 컨테이너다. `Push`/`PushAndGetSize`/`PushBatch`/`Swap`/`SwapChunk` 모두
내부의 단독 락 `PLock _lock` 하나로 `_inQueue`와 `_size`를 함께 보호하며, 락 범위 밖에서
크기를 조회할 때만 별도의 `std::atomic<int64> _size`를 relaxed 로드한다.

이는 `COdbcAsyncSrv`가 자체로 갖고 있는 `_mutex`/`_cva`/`_cvProducer`와는 **역할이 다른
별개의 락**이다.

| 락/조건변수 | 소속 | 역할 |
|---|---|---|
| `CChunkedSwapQueue::_lock`(`PLock`) | 큐 내부 | `_inQueue`/`_size`에 대한 데이터 자체의 스레드 세이프 보장 |
| `COdbcAsyncSrv::_mutex` + `_cva`/`_cvProducer` | 서비스 계층 | 큐가 비었을 때 워커를 재우고(`Pop`), 큐가 가득 찼을 때 생산자를 재우는(`WaitPushCapacity`) **블로킹 대기/기상 신호**만 담당 — 큐 데이터 보호 목적이 아니다 |

즉 `Pop()`이 `_mutex`를 잡는 것은 `_cva.wait()`로 조건을 검사·대기하기 위함이고, 실제
`_inQueue`/`_size` 변경은 그 안에서 호출하는 `SwapChunk()`가 자신의 내부 `_lock`으로
다시 한번 보호한다 — 결과적으로 하나의 `Pop()` 호출 동안 서로 다른 두 개의 락이 순차적으로
관여한다.

추가로 눈여겨볼 점:
- `CChunkedSwapQueue`는 `Stop()`으로 켜지는 자체 `_stopped` 플래그를 갖고 있어, 켜지면 이후
  `Push`/`PushAndGetSize`/`PushBatch`가 삽입을 거부한다. `Stop()`과 각 Push 계열 함수 모두
  같은 내부 `_lock` 안에서 플래그를 확인·세팅하므로, `Stop()`이 반환한 **이후** 시작되는
  호출은 반드시 드롭됨이 보장된다(happens-before). 각 함수는 드롭 여부를 반환값으로 명시한다
  — `Push()`/`PushBatch()`는 `bool`, `PushAndGetSize()`는 드롭 시 `-1`(정상 성공 시 항상
  1 이상이라 값이 겹치지 않음). `Start()`로 정지를 해제해 재사용할 수도 있다. 다만
  `COdbcAsyncSrv`는 이 큐 레벨 `Stop()`을 호출하는 곳이 없다 — 대신 자신의 `_bStopThread`
  아토믹을 `Push()` 진입 시점에 먼저 검사해 같은 효과를 낸다. 즉 `CChunkedSwapQueue`가
  제공하는 자체 정지 기능은 이 호출부에서는 쓰이지 않는 여분의 기능이며, 다른 코드가 실수로
  `_queueDBAsyncRq.Stop()`을 직접 호출하면 `COdbcAsyncSrv`의 정지 상태(`_bStopThread`)와
  어긋나므로 주의해야 한다.
- 큐 전체를 한 번에 비우는 전용 메서드 `Swap()`(대상 큐가 비어 있으면 컨테이너째로 O(1)
  스왑)을 `Clear()`/`FlushRemainingTasks()`가 사용한다 — `GetSize()`로 크기를 먼저 읽어
  `SwapChunk(tempQueue, size)`로 그만큼만 옮기던 방식은 read-then-act 사이에 새 항목이
  들어오면 이번 드레인에서 누락될 수 있는 TOCTOU 여지가 있는데, `Swap()`은 그 읽기 단계
  자체가 없어 한 번의 락 구간 안에서 "그 시점의 큐 전체"를 정확히 이관한다.
- `CChunkedSwapQueue`는 복사뿐 아니라 이동도 금지되어 있다(`= delete` 4종) — 멤버로
  고정 배치되는 용도에 맞춰 설계되었다.

### 11.2 서비스 시작 — `StartService()`

`COdbcAsyncSrv`는 생성자에서 아무 것도 시작하지 않는다 — ODBC 커넥션 풀 초기화와 워커
스레드 기동을 한 번에 처리하는 `StartService(dbNodeVec, nMaxThreadCnt = 0)`을 명시적으로
호출해야 실제로 동작을 시작한다.

- `_bStarted` 플래그로 한 인스턴스당 정확히 한 번만 호출 가능하도록 막는다. 이미 시작된
  인스턴스에 다시 호출하면, 내부적으로 `InitOdbc()`가 `_odbcPools.clear()`를 하는 과정에서
  기존 워커 스레드가 아직 그 풀들을 참조하며 실행 중일 수 있어 위험하기 때문이다 — 재시작이
  필요하면 인스턴스를 새로 만들어야 한다.
- 내부적으로 `InitOdbc()`(풀 집합 생성 및 초기 연결) → `StartIoThreads()`(`_nMaxThreadCnt`개의
  `std::thread`를 `[this]() { RunningThread(); }` 람다로 생성) 순서로 진행한다. `StartIoThreads()`
  도중 `std::thread` 생성이 예외(`std::system_error`, 시스템 자원 부족 등)를 던지면, 이미
  스폰된 스레드들을 `Stop()`+`Join()`으로 안전하게 정리한 뒤 예외를 다시 던진다. `StartService()`
  자신도 이 예외를 잡아 `_bOpen = false` 및 `ClearOdbcPools()`로 마저 정리한다.
- 각 워커는 `RunningThread()` → (`_bOpen`이면) `Action()`으로 이어지는 루프를 돈다.
- `dbNodeVec`이 이 서비스(도메인) 하나가 관리할 DB 노드 목록이다. 같은 논리 도메인의
  샤드/복제본 여러 대를 나열하는 용도이며, 서로 다른 도메인(멤버/게임/로그)을 한 인스턴스에
  섞지 않는다 — 도메인마다 별도의 `COdbcAsyncSrv` 인스턴스를 두는 것이 §12 `CDbServiceManager`의
  전제다.

### 11.3 종료 — `Stop()` / `Join()` / 소멸자

- `Stop()`은 `_mutex` 임계구역 안에서 `_bStopThread`를 `true`로 세팅한 뒤 `_cva.notify_all()`
  / `_cvProducer.notify_all()`을 호출한다. `Push()`도 같은 `_mutex`로 `_bStopThread`를
  검사하므로, `Stop()`이 반환한 시점 이후로는 어떤 `Push()`도 성공하지 않음이 보장된다.
  이 함수 자체는 신호만 보낼 뿐 대기하지 않는다.
- `Join()`은 `_workerThreads`의 모든 스레드를 순회하며 `join()`한다 — `Stop()`으로 신호를
  보낸 워커들이 실제로 전부 종료할 때까지 대기한다. 반드시 `Stop()` 다음에 호출해야 하며,
  `Stop()` 없이 `Join()`만 호출하면 워커가 절대 끝나지 않아 영원히 블로킹된다.
- 소멸자 `~COdbcAsyncSrv()`는 `Stop()` → `Join()` → `FlushRemainingTasks()` →
  `ClearOdbcPools()` 순으로 정리한다. 워커 스레드가 실제로 전부 종료했음을 `Join()`으로
  먼저 확인한 뒤에야 커넥션 풀을 정리하므로, "워커가 아직 도는데 자원이 먼저 사라지는"
  경쟁이 성립하지 않는다.

### 11.3a 종료 시 잔여 작업 처리 — `FlushRemainingTasks()`

`Join()`으로 워커 스레드가 하나도 남아있지 않음을 이미 확인한 뒤에 호출된다는 전제 하에,
큐에 남은 요청들을 호출부 스레드에서 직접 동기적으로 마저 처리하는 함수다. 워커가 이미
없으므로 여기서 다시 `_bStopThread`를 세팅하거나 조건 변수를 notify할 필요가 없다.

1. `_queueDBAsyncRq.Swap()`으로 남은 전체 요청을 지역 임시 큐(`tempQueue`)로 한 번에 옮긴다.
2. 옮겨 담은 요청들을 하나씩 `std::move`로 꺼내 `_mapCommand`에서 핸들러를 찾아
   `ProcessAsyncCall()`을 **호출부 스레드에서 직접** 실행한다. 핸들러를 찾지 못하거나
   처리 결과가 `EDBReturnType::OK`가 아니면 에러를 로그로 남긴다. 각 요청은 `unique_ptr`이므로
   별도의 해제 호출 없이 자동으로 메모리가 해제된다.
3. 큐가 빌 때까지 반복한 뒤 완료 로그를 남기고 반환한다.

- `Action()`의 워커 루프에 있던 타임아웃 재시도 로직(§11.1의 `bReTry` 재큐잉)은 여기에는
  없다 — 종료 처리 경로이므로 실패한 요청을 다시 큐에 넣지 않고 에러 로그만 남기고 넘어간다.

### 11.4 Back-pressure — `WaitPushCapacity()`

DB 처리 속도보다 요청 생산 속도가 빠른 상황에서 큐가 무한정 커지는 것을 막기 위한
선택적 안전장치다. `Push()` 자체는 큐 크기를 검사하지 않으므로, 이 기능을 쓰려면
생산자 쪽 호출부가 `Push()` 전에 `WaitPushCapacity(maxCapacity)`를 명시적으로 호출해야
한다 — 강제되는 하드 리밋이 아니라 협조적(cooperative) 방식이다.

```cpp
pAsyncSrv->WaitPushCapacity(10000); // 큐가 10000개 미만으로 줄어들 때까지 대기
pAsyncSrv->Push(std::move(pRequest));
```

- 내부적으로 워커 대기용(`_cva`)과 분리된 별도 조건 변수 `_cvProducer`를 사용한다.
  `Pop()`이 큐에서 항목을 최대 64개까지 꺼낼 때마다(락 해제 후) `_cvProducer.notify_all()`을
  호출해, `WaitPushCapacity()`로 대기 중이던 생산자들을 모두 깨운다(§11.1a). 각 생산자는
  깨어난 뒤 자기 predicate(`GetSize() < maxCapacity`)로 재검증하므로 불필요하게 깨어난
  스레드는 곧바로 다시 대기 상태로 돌아간다.
- `Stop()`과 `FlushRemainingTasks()`가 트리거되는 소멸 경로 양쪽 모두 `_cva`뿐 아니라
  `_cvProducer`도 함께 `notify_all()`하므로, 종료 시점에 큐 공간을 기다리며 블로킹 중이던
  생산자 스레드도 함께 깨어나 빠져나올 수 있다 (대기 조건에 `_bStopThread.load()`가 포함되어 있음).
- 생산자가 여러 스레드라면, `WaitPushCapacity()`가 반환된 직후와 실제 `Push()` 사이에
  다른 생산자도 동시에 같은 판단을 내려 함께 `Push()`할 수 있는 TOCTOU 여지가 있다.
  즉 `maxCapacity`는 동시 생산자 수만큼 일시적으로 초과될 수 있는 **연성(soft) 상한**이며,
  정확한 하드 리밋이 필요하면 별도의 원자 카운터로 자리를 예약하는 절차가 추가로 필요하다.
  생산자가 단일 스레드(예: 게임 로직 스레드 하나)라면 이 여지 자체가 없다.
- 성능 측면에서는 `Pop()`마다 추가되는 `notify_all()` 호출 하나뿐이다. 대기 중인
  생산자가 없으면(가장 흔한 경우) 조건 변수 `notify`는 사실상 비용이 거의 없는 연산이라
  핫패스인 `Pop()`에 유의미한 오버헤드를 주지 않는다.

### 11.5 미완료 요청 카운터 — Outstanding Requests

큐에 들어갔지만 아직 끝까지 처리되지 않은 요청이 몇 개인지 외부에서 조회할 수 있도록,
`_nOutstandingRequests`(원자 정수)와 이를 캡슐화한 세 개의 인터페이스를 제공한다.

| 함수 | 설명 |
|---|---|
| `AddOutstandingRequest()` | 카운터를 1 증가 (`Push()` 이전에 호출부가 직접 호출) |
| `SubOutstandingRequest()` | 카운터를 1 감소 |
| `GetOutstandingRequests()` | 현재 카운터 값 조회 |

`SubOutstandingRequest()`는 요청이 최종적으로 끝나는 모든 경로에서 짝을 맞춰 호출된다 —
`Action()`에서 요청이 성공하거나, 재시도 없이 실패하거나, 재시도 후 다시 실패해
끝나는 시점과, `FlushRemainingTasks()`가 종료 시점에 남은 요청을 동기 처리한 직후 양쪽
모두에서 호출된다. 타임아웃으로 재시도되어 `continue`로 다시 큐에 들어가는 경로는 아직
요청이 끝난 게 아니므로 호출되지 않고, 같은 요청이 나중에 다시 루프를 돌 때 최종적으로
한 번만 호출된다. `_mapCommand`에서 핸들러를 찾지 못하는 경로에서도 카운터 정합성을
위해 `SubOutstandingRequest()`가 호출된다.

- `AddOutstandingRequest()`를 호출하는 지점은 이 클래스 안에는 없다 — `Push()`
  이전에 호출부(요청을 생성하는 쪽, 보통 §11.6의 헬퍼)가 직접 호출하는 것을 전제로 한 설계다.

### 11.6 공용 요청 생성/게시 헬퍼 — `DBAsyncPushHelper.h`

§11.4의 `WaitPushCapacity()` + `Push()` 조합이나, 요청 생성 → `callIdent` 세팅 →
`AddOutstandingRequest()` → `Push()` → 실패 시 `SubOutstandingRequest()`로 되돌리는
패턴은 `COdbcAsyncSrv`뿐 아니라 `CMySQLAsyncSrv`/`CAdoAsyncSrv`에서도 호출부마다
거의 동일하게 반복된다. `DBAsyncPushHelper.h`는 이 반복을 함수 템플릿 두 개로
공용화한다.

| 함수 | 스레드 제약 | 큐 포화 시 동작 |
|---|---|---|
| `PushDBAsyncRequest<TSrv, TReq>(srv, cmd, initializer, maxQueueCapacity)` | 논블로킹 — IOCP 워커/네트워크 콜백처럼 **절대 재우면 안 되는 스레드** 전용 | `srv.GetQueryQueueSize() >= maxQueueCapacity`면 대기 없이 즉시 `false` 반환 |
| `PushDBAsyncRequestBlocking<TSrv, TReq>(srv, cmd, initializer, maxQueueCapacity)` | 블로킹 — `srv.WaitPushCapacity()`로 호출 스레드를 재움. 배치 Producer/Consumer 같은 **전용 피더/워커 스레드**에서만 사용 | 큐에 여유가 생길 때까지 대기한 뒤 진행 |

```cpp
// 논블로킹 — IOCP 워커/네트워크 콜백 스레드에서 호출
bool bOk = PushDBAsyncRequest<COdbcAsyncSrv, PRODUCER_DATA_BATCH_REQ>(
    MEMBER_DB_ASYNC, CMD_PRODUCER_DATA_BATCH,
    [&](PRODUCER_DATA_BATCH_REQ* pReq) { pReq->nUserID = nUserID; /* ... */ },
    10000);

// 블로킹 — 배치 Producer 전용 피더 스레드에서 호출
bool bOk2 = PushDBAsyncRequestBlocking<COdbcAsyncSrv, PRODUCER_DATA_BATCH_REQ>(
    MEMBER_DB_ASYNC, CMD_PRODUCER_DATA_BATCH,
    [&](PRODUCER_DATA_BATCH_REQ* pReq) { pReq->nUserID = nUserID; /* ... */ },
    10000);
```

공통 동작 순서(둘 다 동일, 용량 확인 방식만 다름):
1. (블로킹 버전만) `srv.WaitPushCapacity(maxQueueCapacity)`로 대기, 또는 (논블로킹
   버전만) `srv.GetQueryQueueSize()`가 상한 이상이면 즉시 실패 반환
2. `std::make_unique<TReq>()`로 요청 객체를 예외 안전하게 생성하고 `callIdent`를 세팅
3. 호출자가 넘긴 `initializer`로 나머지 필드를 채움 — `Fn&&`로 perfect-forwarding되는
   템플릿 매개변수라, 캡처가 큰 람다를 넘겨도 `std::function`을 거칠 때 생기는 힙 할당이
   없다. `initializer`가 `TReq*`를 받아 호출할 수 없는 타입(예: `nullptr`)이면 `if constexpr
   (std::is_invocable_v<Fn, TReq*>)`로 컴파일 타임에 호출 자체가 스킵된다
4. `srv.AddOutstandingRequest()` 호출 후 `srv.Push()`. `Push()`가 `0`을 반환하면(서비스
   종료 등) `srv.SubOutstandingRequest()`로 카운터를 대칭적으로 되돌리고 `false` 반환

- `TSrv`/`TReq`는 함수 템플릿의 앞쪽 두 매개변수라 호출부가 명시적으로 지정해야 하고
  (예: `PushDBAsyncRequest<COdbcAsyncSrv, PRODUCER_DATA_BATCH_REQ>(...)`), `Fn`은 뒤쪽
  매개변수라 `initializer` 인자로부터 자동 추론된다. `TSrv`는 `GetQueryQueueSize()`/
  `WaitPushCapacity()`/`AddOutstandingRequest()`/`SubOutstandingRequest()`/`Push()`를 갖는
  타입이면 되고, 공식 추상 베이스 클래스는 없다 — `COdbcAsyncSrv`/`CMySQLAsyncSrv`/
  `CAdoAsyncSrv` 세 클래스가 이 메서드들의 시그니처를 동일하게 맞춰 이 "덕 타이핑" 계약을
  지킨다. `TReq`는 `st_DBAsyncRq`를 상속하고 기본 생성자를 갖는 타입이어야 한다.
- 반환값이 `false`라는 것은 요청 객체가 애초에 만들어지지도, 큐에 들어가지도 않았다는
  뜻이다. 호출부가 이 실패를 알리는 응답(예: 클라이언트에 실패 패킷 전송)을 직접
  처리해야 한다 — 헬퍼가 실패를 재시도하거나 대신 알려주지 않는다.
- 두 함수 다 어떤 구체적인 `AsyncSrv` 헤더도 강제로 include하지 않는다 — 템플릿이라
  실제 인스턴스화(호출부) 시점에만 `TSrv`/`TReq`의 완전한 타입 정의가 필요하기 때문이다.

### 11.7 멤버 변수 설명

#### DB 풀 / 서비스 상태
| 변수 | 설명 |
|---|---|
| `_bOpen` | 서비스 오픈(초기화 완료) 여부 |
| `_bStarted` | `StartService()`가 이미 호출됐는지(중복 호출 방지) |
| `_nMaxThreadCnt` | DB 비동기 워커 스레드 수(= 각 `COdbcConnPool`의 풀 크기이기도 함). `StartService`에서 인자로 받은 값이 0이면 `SYSTEM::CoreCount()`로 대체 |
| `_odbcPools` | `std::vector<std::unique_ptr<COdbcConnPool>>` — DB 노드 수만큼의 풀 집합. 예외 발생 시(초기화 도중 실패 등) 이미 만든 풀들이 자동 정리된다 |
| `_workerThreads` | `std::vector<std::thread>` — 이 인스턴스가 직접 소유하는 워커 스레드. `Join()`이 정확히 이 인스턴스가 만든 스레드만 기다리기 위해 공용 스레드 매니저에 맡기지 않고 직접 보유한다 |

#### 요청 큐 / 핸들러
| 변수 | 설명 |
|---|---|
| `_queueDBAsyncRq` | `CChunkedSwapQueue<std::unique_ptr<st_DBAsyncRq>>` — 비동기 요청 큐 본체 (§11.1b) |
| `_mapCommand` | `std::unordered_map<uint16, std::shared_ptr<CDBAsyncSrvHandler>>` — `callIdent` → 핸들러 매핑 (`COMMAND_MAP` 타입 별칭) |

#### 동기화
| 변수 | 설명 |
|---|---|
| `_mutex` | `_cva`/`_cvProducer`의 조건 검사·대기를 보호하는 뮤텍스. 큐 내부 락(`PLock`)과는 역할이 다른 별개의 락(§11.1b) |
| `_cva` | 컨슈머(워커) 대기 조건 변수 — `Pop()`이 큐가 비었을 때 대기 |
| `_cvProducer` | 생산자 대기 조건 변수 — `WaitPushCapacity()`가 큐가 가득 찼을 때 대기(§11.4) |
| `_bStopThread` | `std::atomic<bool>` — 스레드 중단 플래그. `Push()` 진입 시 이 값을 검사해 종료 후 신규 삽입을 차단 |

#### 카운터 / 모니터링
| 변수 | 설명 |
|---|---|
| `_nOutstandingRequests` | `std::atomic<int32>`(기본값 0) — 아직 끝나지 않은 요청 수 (§11.5) |
| `_cumulateCallCnt` | `std::atomic<uint64>`(기본값 0) — `Action()`의 지연 쿼리 경고 로그에 찍히는 누적 카운트. 인스턴스 멤버라 도메인(멤버/게임/로그)별로 독립적으로 집계된다 |

> `COdbcAsyncSrv`에는 `CAdoAsyncSrv`/`CMySQLAsyncSrv`에 있는 큐 적체 경고 시스템
> (`_nLastWarnedQueueSize`/`_bMaxWarningActive` 기반)이 없다 — 지연 쿼리 경고(위
> `_cumulateCallCnt` 관련 로그)만으로 진단하는 구조다.

### 11.8 멤버 함수 설명

#### Public — 서비스 생명주기
| 함수 | 설명 |
|---|---|
| `StartService(dbNodeVec, nMaxThreadCnt=0)` | `InitOdbc()` → `StartIoThreads()` 순으로 서비스를 시작하는 단일 엔트리포인트(§11.2) |
| `Stop()` | 워커 스레드에게 종료 신호만 보낸다 — 대기하지 않음(§11.3) |
| `Join()` | `Stop()`으로 신호를 보낸 워커 스레드들이 실제로 전부 종료할 때까지 대기(§11.3) |
| `InitOdbc(dbNodeVec, nMaxThreadCnt)` | DB 노드 수만큼 `COdbcConnPool`을 생성해 각각 `Init()`. 도중 실패 시 `_odbcPools`를 정리하고 `false` 반환 |
| `StartIoThreads()` | `_nMaxThreadCnt`개의 워커 스레드 생성, 각각 `RunningThread()` 실행 |
| `RunningThread()` | `_bOpen`이면 `Action()` 호출 |
| `Action()` | 워커의 메인 루프: `Pop()` → 핸들러 조회(미등록 시 `SubOutstandingRequest()` 후 스킵) → `ProcessAsyncCall()` 실행 → 실패 시 로그 → `TIMEOUT`이고 미재시도면 재큐잉(재큐잉 성공/실패 모두 로그), 그 외에는 지연 쿼리 경고 로그 후 `SubOutstandingRequest()` |

#### Public — 요청 등록 / 큐 조작
| 함수 | 설명 |
|---|---|
| `Regist(command, handler)` | `callIdent`별 핸들러 등록. 보통 직접 호출하지 않고 `DECLARE_DBASYNC_HANDLER_EX` 매크로가 정적 초기화 시점에 대신 호출(§11.1) |
| `Push(pAsyncRq)` | 큐에 요청 추가. `_mutex` 보호 구간 안에서 `PushAndGetSize()`만 수행하고, 락을 해제한 뒤 `_cva.notify_one()`을 호출한다(§11.1). 반환값은 삽입 후 큐 크기, `_bStopThread`가 켜져 있으면 `0` |
| `Pop(localQueue)` | 로컬 큐 우선 소비, 비어 있으면 `_mutex`+`_cva`로 대기 후 `SwapChunk(64)`로 일괄 인출, 락 해제 후 `_cvProducer.notify_all()`(§11.1a) |
| `GetQueryQueueSize()` | 큐에 쌓인 요청 수 조회 |
| `IsEmpty()` | 큐가 비어 있는지 여부 |
| `WaitPushCapacity(maxCapacity)` | 큐 크기가 `maxCapacity` 미만이 되거나 종료 신호가 올 때까지 `_cvProducer`로 대기(§11.4 back-pressure) |

#### Public — Outstanding 카운터
| 함수 | 설명 |
|---|---|
| `AddOutstandingRequest()` | `_nOutstandingRequests` 1 증가. `Push()` 이전에 호출부가 직접 호출 |
| `SubOutstandingRequest()` | `_nOutstandingRequests` 1 감소. 요청이 최종적으로 끝나는 모든 경로에서 호출(§11.5) |
| `GetOutstandingRequests()` | 현재 카운터 값 조회 |

#### Public — DB 풀 접근자
| 함수 | 설명 |
|---|---|
| `GetOdbcConnPool()` | `_odbcPools[0]` 반환. 시작 전(또는 실패 후) 호출되면 디버그 빌드에서 `assert`로 잡히고, 릴리즈 빌드에서도 `_odbcPools.empty()`면 `nullptr`을 명시적으로 반환한다 |
| `GetOdbcConnPool(id)` | `id % _odbcPools.size()`로 균등 모듈로 샤딩해 담당 풀을 반환. 풀이 1개뿐이면 그 하나만 반환 |

#### Private — 내부 정리 로직
| 함수 | 설명 |
|---|---|
| `FlushRemainingTasks()` | 종료 시(`Join()` 이후) 남은 요청을 호출부 스레드에서 동기적으로 직접 처리(§11.3a) |
| `ClearOdbcPools()` | `_odbcPools.clear()` — 각 풀은 `unique_ptr`이라 자동 해제됨 |

### 11.9 사용법

```cpp
// 1. 핸들러 등록 — 정적 초기화 시점에 자동으로 Regist()가 호출된다(§11.1, DBAsyncHandler.h)
//    실제 쿼리 실행은 이 ProcessAsyncCall 본문 안에서, GetOdbcConnPool()로 얻은 풀을
//    OdbcConnGuard로 감싸 수행한다 — Action()의 워커 스레드가 이 코드를 실행하는
//    바로 그 지점이므로, 블로킹 쿼리를 호출해도 IOCP 워커나 게임 로직 스레드를 막지 않는다.
DECLARE_DBASYNC_HANDLER_EX(MEMBER_DB_ASYNC, CMD_PRODUCER_DATA_BATCH)
{
    PRODUCER_DATA_BATCH_REQ* pReq = static_cast<PRODUCER_DATA_BATCH_REQ*>(pStAsync);

    OdbcConnGuard guard(MEMBER_DB_ASYNC.GetOdbcConnPool(pReq->nUserID));
    if( guard == nullptr )
        return EDBReturnType::TIMEOUT; // 재연결 대기 중 등 — bReTry 경로로 재시도(§11)

    if( !guard->ExecuteQuery(_T("SELECT ...")) )
        return EDBReturnType::TIMEOUT;

    return EDBReturnType::OK;
}

// 2. 서비스 시작 (보통 CDbServiceManager 생성 이후 서버 초기화 루틴에서 한 번 호출, §12)
CVector<CDBNode> dbNodeVec = { /* 이 도메인의 DB 노드(샤드/복제본) 목록 */ };
MEMBER_DB_ASYNC.StartService(dbNodeVec, /*nMaxThreadCnt=*/8);

// 3. 요청 게시 — 직접 Push()를 조립하기보다 §11.6의 헬퍼 사용을 권장.
//    이 호출은 IOCP 워커나 게임 로직 스레드 등 "쿼리를 요청하고 싶은 쪽"에서 하며,
//    실제 쿼리(위 1번 핸들러 본문)는 별도의 DB 비동기 워커 스레드에서 나중에 실행된다.
bool bOk = PushDBAsyncRequest<COdbcAsyncSrv, PRODUCER_DATA_BATCH_REQ>(
    MEMBER_DB_ASYNC, CMD_PRODUCER_DATA_BATCH,
    [&](PRODUCER_DATA_BATCH_REQ* pReq) { pReq->nUserID = nUserID; },
    10000);
if( !bOk )
{
    // 큐 포화 또는 서비스 종료 중 — 호출부가 실패 응답을 직접 처리
}

// 4. 서비스 종료 (보통 CDbServiceManager::ShutdownAll()을 통해 일괄 수행, §12)
CDbServiceManager::Instance().ShutdownAll();
```

- 즉 이 서비스는 "요청 게시(3번, 호출 스레드)"와 "실제 쿼리 실행(1번 핸들러 본문, DB
  비동기 워커 스레드)"이 서로 다른 스레드에서 시차를 두고 일어나는 구조다.
  `GetOdbcConnPool()`/`OdbcConnGuard`로 커넥션을 빌려 블로킹 쿼리를 실행하는 코드는
  항상 후자, 즉 핸들러 본문 안에 있어야 한다 — 요청을 게시하는 쪽(3번)에서 직접
  풀에 접근해 쿼리를 실행하면, `COdbcAsyncSrv`를 거치는 의미(호출 스레드를 블로킹
  I/O로부터 보호)가 사라진다.
- DB 비동기 워커 스레드 수(`nMaxThreadCnt`)와 각 `COdbcConnPool`의 크기는 `InitOdbc()`가
  자동으로 동일하게 맞춘다(§11.1) — 별도로 신경 쓸 필요는 없지만, 이 값을 너무 작게 잡으면
  `PopFreeSlotIndex()` 실패(-1)가 늘어난다는 점은 §9와 동일하다.
- `Stop()`을 명시적으로 호출하지 않아도, `~COdbcAsyncSrv()`가 실행되면 그 안에서
  `Stop()` → `Join()` → `FlushRemainingTasks()`가 순서대로 실행되어 요청 유실 없이
  종료된다(§11.3).

## 12. 도메인별 서비스 관리 — `CDbServiceManager`

`DbServiceManager.h`는 멤버/게임/로그 등 도메인별 `COdbcAsyncSrv`(및 `CMySQLAsyncSrv`/
`CAdoAsyncSrv`) 인스턴스를 소유하고 이름 있는 접근자로 노출하는 프로세스 전역 매니저다.

- `CDbServiceManager::Instance()`는 함수-지역 `static CDbServiceManager instance;`로
  구현된 Meyer's singleton이다 — 최초 호출 시점에 생성되므로, `DECLARE_DBASYNC_HANDLER_EX`
  매크로의 정적 멤버 초기화식처럼 다른 전역 객체의 정적 초기화 도중 이 함수가 호출돼도
  정적 초기화 순서 문제(SIOF)에서 자유롭다.
- 생성자에서 `_memberDB = std::make_unique<COdbcAsyncSrv>()`로 인스턴스만 만들 뿐,
  `StartService()`는 자동으로 호출하지 않는다 — 실제 DB 접속/워커 스레드 기동은 호출부가
  `MemberDB().StartService(...)`(§11.9의 2번)를 명시적으로 호출해야 한다.
- `MemberDB()`는 `*_memberDB`를 반환한다. 매크로 `MEMBER_DB_ASYNC`는
  `CDbServiceManager::Instance().MemberDB()`의 축약형이며, 이 헤더가 include된 모든
  번역 단위에 이름이 그대로 노출된다 — 다른 곳에 같은 이름의 매크로/심볼이 있으면 충돌할
  수 있으니 주의한다.
- `ShutdownAll()`은 등록된 모든 도메인 서비스를 한 번에 종료한다. `_memberDB->Stop()` →
  `_memberDB->Join()` → `_memberDB.reset()` 순으로 처리하며, `_memberDB`가 이미
  `nullptr`이면(두 번째 호출 등) 아무 일도 하지 않고 조용히 반환해 멱등하게 동작한다.
- 도메인이 늘어나면(예: 게임 DB, 로그 DB) `_gameDB`/`_logDB` 같은 멤버와 `GameDB()`/
  `LogDB()` 접근자, `GAME_DB_ASYNC`/`LOG_DB_ASYNC` 매크로를 같은 패턴으로 추가하면 된다.
