# CAdoConnPool 설계 문서

> 이 문서는 `CAdoConnPool`을 중심으로 작성되었으며, 그 위에서 큐+워커로 동작하는
> 비동기 서비스 계층 `CAdoAsyncSrv`(§11)까지 함께 다룬다.

## 1. 개념

`CAdoConnPool`은 고정 크기의 ADO 커넥션(`CAdoDB`)을 미리 생성해두고, 슬롯 단위로 대여/반납하며,
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
| Lock-free 대여/반납 | `GetAdoConn` / `ReleaseAdoConn`은 `std::atomic`의 fetch_add/fetch_sub만 사용 |
| 비동기 자동 재연결 | 헬스체크 스레드가 끊어진 슬롯을 감지하고, 별도 워커 풀이 실제 재연결(ADO `Connect()`, I/O)을 병렬 수행 |
| 지수 백오프 + 지터 | DB 전체 장애 시 모든 슬롯이 동시에 재시도하는 connection storm을 방지 |
| 이벤트 기반 정밀 재시도 스케줄링 | 백오프 대기는 `CDelayedTaskQueue`(§3의 `_delayedTaskQueue`)가 전담하며, 계산된 지연 시간이 정확히 지난 시점에 콜백이 1회 실행되어 재시도를 트리거한다. 헬스체크 스캔 주기(500ms)와는 독립적으로 동작한다 |
| 동적 워커 수 조정 | `SetReconnectConfig`로 런타임 중 재연결 워커 수/백오프 정책을 조정 가능 |
| 격리(Quarantine) 큐 | 교체된 낡은 커넥션에 참조가 남아있으면 즉시 삭제하지 않고 격리 후 안전할 때 삭제 |
| Safe Leak | 프로세스 종료 시점까지 참조가 남은 커넥션은 삭제를 포기(누수)하여 UAF 크래시를 방지 |
| False sharing 방지 | 슬롯별 원자 배열(`_pAdoConns`, `_pRefCount`, `_pReconnecting`, `_pRetryFailCount`)을 `CachePaddedAtomic<T>[]`로, `_slotLocks`를 캐시라인 정렬된 `SpinLockDefault[]`로 구성해 슬롯 간 캐시라인 공유를 차단 |
| 할당자 분리 | `CAdoConnPool` 자신은 `BaseAllocator` 상속으로 RawAllocator 경로를, 내부 `CAdoDB` 커넥션은 `xnew`/`xdelete`(PoolAllocator)로 별도 관리 (§10 참고) |

## 3. 멤버 변수 설명

### 기본 상태
| 변수 | 설명 |
|---|---|
| `_dbClass` | 사용 중인 DB 종류 (`EDBClass`: MSSQL, MYSQL, ORACLE 등) |
| `_tszConnStr` | ADO 연결 문자열 (`TCHAR[512]`, `CAdoDB::Connect()`에 그대로 전달) |
| `_nTimeOut` | 커넥션 타임아웃 (`Init()`에서 지정, 재연결 시에도 동일하게 재사용) |
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
| `_pAdoConns` | 슬롯별 실제 커넥션 포인터 (`CachePaddedAtomic<CAdoDB*>[]`) |
| `_pRefCount` | 슬롯별 참조 카운트 (`CachePaddedAtomic<int32>[]`, 가장 핫한 배열) |
| `_slotLocks` | 슬롯별 교체(swap) 보호용 스핀락 배열 |
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

## 4. 멤버 함수 설명

### Public API
| 함수 | 설명 |
|---|---|
| `Init(dbClass, ptszConnStr, nTimeOut, reconnectConfig)` | 풀을 초기화하고 연결 문자열로 커넥션을 동기적으로 채운다. 슬롯마다 `xnew<CAdoDB>()`로 객체를 만든 뒤 `Connect()`를 호출하며, 하나라도 실패하면(`xnew` 실패 또는 `Connect()` 반환값이 음수) 즉시 `Clear()`로 되돌리고 `false`를 반환한다. 잘못된 `reconnectConfig`는 기본값으로 대체된다 |
| `GetAdoConn(nType)` | 슬롯의 참조 카운트를 증가시키고 커넥션을 반환. 커넥션이 `nullptr`이거나 `GetDBCon()`이 거짓이면 즉시 `nullptr`을 반환하고 카운트를 되돌린다 |
| `ReleaseAdoConn(nType)` | 참조 카운트를 감소시켜 슬롯을 반납 |
| `GetPooledConnUnsafe(nType)` | `PopFreeSlotIndex`로 이미 선점된 슬롯을 카운트 변경 없이 조회 (`AdoConnGuard` 전용) |
| `PopFreeSlotIndex()` | 빈 슬롯(참조 카운트 0)을 찾아 즉시 원자적으로 선점하고, 커넥션이 살아있는지(`GetDBCon()`)까지 확인한 뒤 인덱스 반환 |
| `GetMaxPoolSize()` | `_nMaxPoolSize` 반환 (헤더 인라인) |
| `SetReconnectConfig(cfg)` | 백오프/워커 수 정책을 런타임에 변경. 유효성 실패 시 전체 거부(부분 적용 없음) |
| `GetReconnectConfig()` | 현재 정책 스냅샷 조회 (모니터링용) |

### Protected 내부 로직
| 함수 | 설명 |
|---|---|
| `Clear()` | 모든 슬롯을 정리. 참조가 남은 슬롯은 격리 큐로 보냄 (Shutdown 전용) |
| `IsValidIndex(nType)` | 슬롯 인덱스 범위 검사 |
| `ValidateReconnectConfig(cfg)` | 재연결 설정값의 상식적 범위 검사 (Init/SetReconnectConfig 공용) |
| `TryReconnect(nType)` | 새 `CAdoDB`를 `xnew`로 생성하고 `Connect(_dbClass, _tszConnStr, _nTimeOut)`으로 실제 연결까지 시도하는 블로킹 I/O 로직. 반환값이 음수면 실패로 간주하고 `xdelete` 후 `nullptr` 반환 |
| `ApplyReconnectedConn(nType, pNewConn)` | 새 커넥션으로 슬롯을 스왑하고, 낡은 커넥션을 안전하게 삭제 또는 격리 |
| `ScheduleRetry(nType)` | 실패 횟수(`_pRetryFailCount`)를 늘리고 지수 백오프+지터로 지연 시간을 계산한 뒤, `_delayedTaskQueue.Reserve()`로 그 시간 뒤 1회 실행될 재시도 콜백을 예약 |
| `OnReconnectFailed(nType)` | `ScheduleRetry(nType)` 호출로 위임 |
| `OnReconnectSucceeded(nType)` | `_pRetryFailCount`를 0으로 초기화 (백오프 상태 리셋) |
| `HealthCheckLoop()` | 격리 큐 청소 + 끊어진 슬롯(`GetDBCon()`이 거짓인 슬롯) 스캔. `_pReconnecting`을 CAS로 선점한 뒤 `_pRetryFailCount == 0`(아직 예약된 재시도가 없는 슬롯)인 경우에만 즉시 재연결 큐에 등록하고, 실패 이력이 있는 슬롯은 `_delayedTaskQueue`의 예약에 맡기고 그냥 넘어감 (블로킹 I/O 없음) |
| `StartHealthCheckThread()` / `StopHealthCheckThread()` | 헬스체크 스레드 기동/안전 종료(join) |
| `DelayedTaskLoop()` | `_delayedTaskQueue.ProcessExpiredTasks()`를 호출해 만료된 재시도 콜백들을 실행하는 루프 |
| `StartDelayedTaskThread()` / `StopDelayedTaskThread()` | 지연 타이머 전담 스레드 기동 / `_delayedTaskQueue.Stop()` 후 안전 종료(join) |
| `ReconnectWorkerLoop()` | 대기열에서 슬롯을 꺼내 실제 `TryReconnect` + 스왑을 수행하는 워커 루프 |
| `StartReconnectWorkers(n)` / `StopReconnectWorkers()` | 재연결 워커 풀 기동/종료 |
| `SetWorkerCount(n)` | 목표 워커 수 갱신. 확대는 즉시 스폰, 축소는 워커가 스스로 종료하도록 유도 |
| `TryExitIfExcess()` | 현재 워커가 초과 인원인지 CAS로 판정하고, 맞다면 스스로 종료 |
| `EnqueueReconnect(nType)` | 재연결 대기열에 슬롯을 넣고 워커 하나를 깨움 |

## 5. 동작 흐름

### 5.1 커넥션 대여/반납 (핫패스)
1. `AdoConnGuard` 생성 시 `PopFreeSlotIndex()`로 빈 슬롯을 원자적으로 선점 (참조 카운트 1)
2. `GetPooledConnUnsafe()`로 커넥션 포인터 조회
3. 소멸 시 `ReleaseAdoConn()`으로 참조 카운트 반납

### 5.2 자동 재연결
1. `HealthCheckLoop()`이 500ms마다 순회하며 참조 카운트 0 & 연결 끊김(`GetDBCon()`이 거짓)인
   슬롯을 찾고, `_pReconnecting`을 CAS로 선점한다.
2. 선점에 성공한 슬롯 중 `_pRetryFailCount == 0`(아직 예약된 재시도가 없는 슬롯)인 경우만
   `EnqueueReconnect()`로 즉시 대기열에 등록해 워커를 깨운다. 실패 이력이 있어 이미
   `_delayedTaskQueue`에 재시도가 예약된 슬롯은 이번 순회에서 `_pReconnecting`만 반납하고 넘어간다.
3. `ReconnectWorkerLoop()`이 `TryReconnect()`로 `CAdoDB::Connect()` 블로킹 I/O 수행
4. 성공 시 `ApplyReconnectedConn()`으로 슬롯 스왑, 낡은 커넥션은 참조가 빠질 때까지 대기 후
   삭제(또는 격리), `OnReconnectSucceeded()`로 `_pRetryFailCount`를 0으로 리셋
5. 실패 시 `OnReconnectFailed()` → `ScheduleRetry()`가 실패 횟수를 늘려 지수 백오프+지터 지연을
   계산하고, `_delayedTaskQueue.Reserve(지연ms, 콜백)`으로 정확히 그 시간 뒤 1회 실행되는 재시도
   콜백을 예약한다. 콜백은 만료 시점에 슬롯이 여전히 재연결이 필요한 상태인지 재확인한 뒤
   `EnqueueReconnect()`하거나, 그 사이 다른 경로로 이미 해소됐다면 `_pReconnecting`만 반납한다.

> 풀 시작 시에는 `Init()`이 `StartDelayedTaskThread()` → `StartHealthCheckThread()` →
> `StartReconnectWorkers()` 순으로 기동해, 헬스체크나 재연결 워커가 첫 실패로 `ScheduleRetry()`를
> 호출하기 전에 지연 타이머 스레드가 먼저 요청을 받을 준비를 갖춘다. 종료 시에는 반대로
> `StopHealthCheckThread()` → `StopDelayedTaskThread()` → `StopReconnectWorkers()` 순으로 정지한다
> (`~CAdoConnPool()`/재`Init()` 공통).

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
- `_nTimeOut`은 `Init()` 시점의 값이 고정되어 이후 재연결(`TryReconnect`)에도 그대로 재사용된다 — 운영 중 타임아웃만 별도로 조정할 방법은 없다.

## 7. 재연결 워커 스레드 개수(`nWorkerCount`) 설정 가이드

`nWorkerCount`는 `TReconnectConfig`에서 기본값이 4로 되어 있지만, 실제 서비스 환경에서는
아래 요소들을 고려해 조정하는 것이 좋다.

### 7.1 워커가 하는 일과 비용 특성
- 워커는 대기열이 비어있는 동안은 조건 변수에서 블로킹 대기하므로(`_reconnectQueueCv.wait`),
  유휴 상태에서는 CPU를 소모하지 않는다.
- 실제 비용은 `TryReconnect()`가 수행하는 **ADO 연결 수립(네트워크 I/O) 시간** 뿐이다. 따라서
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

`CAdoConnPool`을 IOCP 게임 서버의 DB 처리에 사용할 경우, 서버 전체 스레드를
기능별로 분리하는 것이 좋다. 아래는 역할 구분과, 요즘 많이 쓰이는 코어 구성
기준의 개수 예시다.

### 8.1 기능별 스레드 그룹

| 스레드 그룹 | 역할 | 개수 결정 기준 |
|---|---|---|
| IOCP 워커 | `GetQueuedCompletionStatusEx`로 완료된 Recv/Send I/O를 꺼내 세션에 전달. 순수 네트워크 I/O 처리만 담당 | 물리 코어 수 기준 (I/O 대기 비중에 따라 조정) |
| 게임 로직(콘텐츠) 워커 | JobQueue에서 패킷 처리/게임 로직 Job을 꺼내 실행. IOCP 워커와 분리해 로직 처리 지연이 네트워크 I/O를 막지 않게 함 | 콘텐츠 샤딩 여부에 따라 1개(단일 월드) ~ 샤드 수 |
| DB 비동기 워커 | DB JobQueue에서 쿼리 요청을 꺼내 `CAdoConnPool`에서 커넥션을 빌려 실제 쿼리(블로킹) 실행 후 결과를 완료 큐로 반환 | 예상 동시 DB 요청 수 기준. `_nMaxPoolSize`를 넘지 않는 선에서 결정 |
| `CAdoConnPool` 헬스체크 스레드 | 끊어진 슬롯을 감지해 재연결 대기열에 등록 (논블로킹) | 1개 고정 (클래스 내부에서 자동 생성) |
| `CAdoConnPool` 재연결 워커 스레드 | 실제 재연결 I/O 수행 (`TryReconnect`) | §7 기준 (풀 크기 대비 10~25%, 소규모면 기본값 4) |
| 타이머/틱 스레드 | 게임 틱, 스케줄된 이벤트(리스폰, 버프 만료 등) 처리 | 1개 (로직 워커의 주기 Job으로 흡수 가능) |
| 비동기 로깅 스레드 | 로그를 큐에 쌓고 파일/네트워크로 flush (로직 스레드가 디스크 I/O로 막히지 않게) | 1개 |
| Listener/Accept | 신규 접속 수락 | 별도 생성 불필요, IOCP 워커 중 하나가 AcceptEx 완료도 함께 처리 |

### 8.2 예시 1: 8코어 16스레드 (중소 규모 서버)

| 스레드 그룹 | 개수 |
|---|---|
| IOCP 워커 | 8 |
| 게임 로직 워커 | 1~4 |
| DB 비동기 워커 | 4~8 |
| `CAdoConnPool` 헬스체크 | 1 |
| `CAdoConnPool` 재연결 워커 | 2~4 |
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
| `CAdoConnPool` 헬스체크 | 1 |
| `CAdoConnPool` 재연결 워커 | 4~8 |
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
- DB 비동기 워커 스레드 수와 `CAdoConnPool` 풀 크기(`_nMaxPoolSize`)는 함께
  정해야 한다 — 워커가 풀 크기보다 많으면 대여 실패(`PopFreeSlotIndex` → -1)만
  늘어난다.
- 위 수치는 시작점일 뿐이며, 최종적으로는 실제 부하 테스트(동접자 수, DB 쿼리
  QPS, 패킷 처리량)로 튜닝해야 한다.

## 9. 사용법

```cpp
// 1. 풀 생성 및 초기화
CAdoConnPool pool(/*nMaxPoolSize=*/16);

CAdoConnPool::TReconnectConfig cfg;
cfg.nWorkerCount     = 4;
cfg.nBackoffBaseMs   = 500;
cfg.nBackoffMaxMs    = 30000;
cfg.nBackoffMaxShift = 6;
cfg.nBackoffJitterMs = 250;

if( !pool.Init(EDBClass::MSSQL, _T("Provider=MSOLEDBSQL;Server=127.0.0.1;Database=game_db;Uid=sa;Pwd=***;"), /*nTimeOut=*/5, cfg) )
{
    // 초기 커넥션 생성 실패 처리
}

// 2. 커넥션 대여 (RAII 가드 사용 권장)
{
    AdoConnGuard guard(&pool);
    if( guard != nullptr )
    {
        guard->Execute(_T("SELECT ..."));
    }
    // 스코프 종료 시 자동으로 ReleaseAdoConn 호출됨
}

// 3. 운영 중 재연결 정책 변경 (예: 워커 수를 8개로 확장)
CAdoConnPool::TReconnectConfig newCfg = pool.GetReconnectConfig();
newCfg.nWorkerCount = 8;
pool.SetReconnectConfig(newCfg);
```

- `AdoConnGuard`를 사용하지 않고 `GetAdoConn`/`ReleaseAdoConn`을 직접 짝지어 호출할 수도 있으나,
  예외 발생 시 반납 누락 위험이 있으므로 가드 사용을 권장한다.
- 풀 소멸 시 `~CAdoConnPool()`이 헬스체크 → 지연 타이머 → 재연결 워커 스레드를 순서대로
  먼저 종료한 뒤 `Clear()`로 자원을 정리한다(§5.2 참고).

### 9.1 여러 DB를 다루는 실제 서비스 통합 패턴

계정 DB, 게임 DB, 로그 DB처럼 DB가 여러 개인 서비스에서는 `CAdoConnPool`을 DB 노드 수만큼
배열로 만들어 두고, DB 비동기 워커 스레드들이 공용 요청 큐에서 작업을 꺼내 필요한 풀을
선택해 쓰는 구조가 일반적이다. `CAdoAsyncSrv::InitAdo`가 실제로 이 패턴을 구현한다 (§11.1).

```cpp
// DB 노드 개수만큼 풀을 생성 (예: 계정 DB, 게임 DB, 로그 DB)
CAdoConnPool** pAdoConnPools = new CAdoConnPool*[nDBCount](); // 값 초기화로 모든 슬롯을 nullptr로 둔다

// 재연결 워커 수는 각 풀 크기(= DB 비동기 워커 스레드 수) 대비 비례 산정 (§7.4)
CAdoConnPool::TReconnectConfig reconnectCfg;
reconnectCfg.nWorkerCount = std::max(4, nMaxThreadCnt / 4);

for( int32 i = 0; i < nDBCount; ++i )
{
    // CAdoConnPool이 BaseAllocator를 상속하므로 평범한 new로도 RawAllocator 경로를 타고,
    // 실패 시 예외 대신 nullptr을 반환한다 (§10 참고)
    pAdoConnPools[i] = new CAdoConnPool(nMaxThreadCnt);
    if( pAdoConnPools[i] == nullptr || !pAdoConnPools[i]->Init(dbClass[i], connStr[i], /*nTimeOut=*/5, reconnectCfg) )
    {
        // 이미 만든 풀들까지 함께 정리(ClearAdoPools)한 뒤 실패 처리
        break;
    }
}
```

- 배열을 `new CAdoConnPool*[nDBCount]()`처럼 값 초기화해 두면, 아직 만들어지지 않은
  슬롯도 항상 `nullptr` 상태로 유지되어 정리 루틴이 모든 인덱스를 안전하게 순회할 수 있다.
- 각 풀은 독립된 `CAdoConnPool` 인스턴스이므로 DB별로 서로 다른 연결 문자열/재연결 정책을 줄 수 있다.
- DB 비동기 워커 스레드 수(`nMaxThreadCnt`)와 풀 크기를 동일하게 맞추면, 워커 스레드 각각이
  항상 자기 몫의 슬롯을 확보할 수 있어 `PopFreeSlotIndex()` 실패(풀 고갈)를 구조적으로 방지한다.

## 10. 할당자(Allocator) 설계

`CAdoConnPool`은 `class CAdoConnPool : public BaseAllocator`로 선언되어 있다. 즉 이
클래스를 직접 `new`/`delete`하면(예: 위 §9.1의 `new CAdoConnPool(nMaxThreadCnt)`) 전역
`::operator new`/`delete`가 아니라 `BaseAllocator`가 오버라이드한 `operator new`/`delete`가
호출되어, 프로젝트의 `RawAllocator`(mimalloc/jemalloc/tcmalloc/malloc 중 컴파일 타임 선택) 경로를
탄다.

### 10.1 왜 PoolAllocator(xnew/xdelete)가 아니라 BaseAllocator인가

프로젝트의 할당자 계층은 용도가 명확히 나뉜다.

| 할당자 | 설계 목적 | `CAdoConnPool`과의 적합성 |
|---|---|---|
| `PoolAllocator` (→ `xnew`/`xdelete`) | 실서비스 핫패스(패킷, 세션 등 고빈도 할당/해제) | 부적합 — 풀 자체는 DB 노드당 1개, 서버 기동 시 한 번만 생성됨 |
| `BaseAllocator` | 크기가 크거나 드물게 생성되는 객체를 풀과 분리 | 적합 — 위 프로필과 정확히 일치 |

`BaseAllocator`는 데이터 멤버가 없고 상속되는 함수도 모두 non-virtual이라, 상속해도
`CAdoConnPool` 인스턴스에 vptr 등 추가 메모리 오버헤드가 붙지 않는다.

### 10.2 내부 `CAdoDB` 커넥션은 별도로 `xnew`/`xdelete` 유지

풀 "껍데기"(`CAdoConnPool` 자신)와 달리, 그 안에서 관리하는 실제 ADO 커넥션(`CAdoDB`)은
`Init()`의 초기 채움과 `TryReconnect()`의 재연결 시마다(네트워크 장애가 잦으면 상대적으로
자주) 반복적으로 생성/삭제된다. 이쪽은 여전히 `xnew<CAdoDB>()` / `xdelete(...)`
(`PoolAllocator` 경로)를 그대로 사용한다 — 같은 클래스 계층 안에서도 "이 객체를 만드는 빈도"에
따라 할당자를 다르게 선택한 것이다.

### 10.3 `make_shared`로 생성하는 타입에는 적용 무의미

`BaseAllocator` 상속이 효과를 가지려면 해당 타입이 **직접 `new 타입(...)`** 형태로 생성돼야
한다. `std::make_shared<T>()`는 컨트롤 블록과 객체를 하나로 묶어 자체 할당 경로로 확보하고
`T`의 `operator new`를 거치지 않으므로, 그런 방식으로 생성되는 타입에 `BaseAllocator`를
상속해도 효과가 없다 (`CAdoConnPool`은 위 예시처럼 직접 `new`되므로 해당 사항 없음. 반면
§11의 `CAdoAsyncSrv`는 `make_shared`로 생성되는 진짜 싱글턴이라 해당 사항이다).

## 11. 비동기 서비스 계층 — `CAdoAsyncSrv`

풀(`CAdoConnPool`) 위에, DB 노드별로 풀을 배열로 들고 공용 요청 큐 +
워커 스레드 풀로 쿼리를 비동기 처리하는 서비스 계층이다.

### 11.1 구조 요약

- `Regist(callIdent, handler)`로 명령어별 핸들러를 등록해두면, `Push()`로 큐에 들어온
  `st_DBAsyncRq` 요청을 워커 스레드들이 `Pop()` → `callIdent`로 핸들러 조회 → 실행한다.
  핸들러 조회는 `std::unordered_map`을 사용해 매 쿼리마다의 조회 비용을 O(1) 평균으로 유지한다.
- DB 노드 수만큼 `CAdoConnPool*` 배열(`_pAdoConnPools`)을 두고, `InitAdo()`가 각 노드에
  대해 `CAdoConnPool`을 생성한 뒤 `iter._dbClass`/`iter._tszDSN`으로 `Init()`을 호출해
  채운다. 이때 커넥션 타임아웃은 노드별 값이 아니라 `5`(초)로 고정 전달된다.
- 배열 내 위치에 따라 용도별 풀을 구분해서 가져다 쓴다:
  - `GetAccountAdoConnPool()` — 항상 인덱스 `0`번 풀(계정 DB) 반환
  - `GetAdoConnPool(m_nID)` — `_nDBCount`가 2 초과일 때만 `m_nID`로 분산 선택
    (`m_nID > 0`이면 `(m_nID % (_nDBCount - 1)) + 1`번째 풀, 그 외에는 마지막 인덱스인
    `_nDBCount - 1`번 풀). 게임/월드 DB처럼 ID 기반으로 여러 풀에 부하를 분산할 때 쓰는
    용도로 보인다.
  - `GetLogAdoConnPool()` — `_nDBCount > 2`를 전제로 항상 인덱스 `2`번 풀(로그 DB) 반환
- 배열은 값 초기화되어 있고, 정리 전용 함수 `ClearAdoPools()`가 소멸자와 초기화 실패
  경로 양쪽에서 공용으로 각 풀을 안전하게 해제한다.
- `Instance()`는 `std::make_shared`로 생성되는 진짜 싱글턴이다 — `T::operator new`를
  거치지 않으므로 `BaseAllocator` 상속은 이 클래스에는 적용하지 않는다(§10.3).
- 큐 동기화는 `std::mutex` + `std::condition_variable`로 이루어진다. `Push()`는 큐 조작을
  마치고 락을 해제한 뒤 `notify_one()`을 호출해, 깨어난 워커가 곧바로 락을 잡을 수 있게 한다.
- `st_DBAsyncRq`는 `callIdent`별로 실제 쿼리 데이터를 담은 파생 구조체의 베이스이며,
  `Action()`에서 베이스 포인터(`st_DBAsyncRq*`)로 `SAFE_DELETE`되는 것으로 보아 가상
  소멸자를 갖는 구조로 설계되어 있다.
- 쿼리가 타임아웃되어 처음 재시도될 때는 원본 요청 객체를 그대로 재사용해 `bReTry` 플래그만
  세팅한 뒤 `Push()`로 재큐잉한다 — 파생 구조체를 통째로 다시 할당하지 않는다. 재큐잉이
  실패하면(반환값 0, 서비스 종료 시점과 겹친 경우) 해당 객체는 직접 해제된다.
- `InitAdo`는 호출 시작 시 `_bStopThread`를 `false`로 재설정해, `StopThread()`
  이후 서비스를 다시 시작하는 시나리오에서도 워커 스레드들이 정상적으로 큐를 처리한다.
  `nMaxThreadCnt`가 `0`이면 `SYSTEM::CoreCount()`로 코어 수를 자동 산정한다. 또한 각
  DB 노드의 풀을 생성할 때 `TReconnectConfig.nWorkerCount`를 `max(4, nMaxThreadCnt / 4)`로
  산정해 전달함으로써, 재연결 워커 수가 풀 크기(= DB 비동기 워커 스레드 수)에 비례하도록 한다.
- `Action()`은 처리 결과가 `EDBReturnType::TIMEOUT`이고 아직 재시도한 적이 없을 때만
  재큐잉하며, 그 외 실패(`OK`가 아닌 다른 코드, 또는 이미 재시도한 `TIMEOUT`)는 별도
  경고 없이 요청을 해제하고 다음 요청으로 넘어간다.
- `Clear()`는 DB 요청 큐를 비우는 역할만 담당한다. 등록된 핸들러(`_mapCommand`)는
  `Clear()`의 영향을 받지 않으므로, 초기화가 중간에 실패해 `Clear()`가 호출되어도
  `Regist()`로 등록해둔 핸들러는 그대로 유지된다.
- `_nOutstandingRequests`(진행 중 요청 수)는 `Action()`과 `FlushRemainingTasks()` 양쪽에서
  요청을 최종 처리한 직후 `SubOutstandingRequest()`로 감소시킨다. 증가시키는
  `AddOutstandingRequest()` 호출은 `Push()` 이전, 즉 요청 생성 시점의 호출부 쪽 책임으로
  보인다(본 구현 범위 안에서는 감소 호출만 확인된다).

### 11.2 스레드 생성

`StartIoThreads()`는 `_nMaxThreadCnt`개의 워커 스레드를 람다(`[this]() { RunningThread(); }`)로
생성한다. 각 워커는 `RunningThread()` → `Action()`으로 이어지는 루프를 돌며 큐에서 요청을
꺼내 처리한다.

### 11.3 종료 시 잔여 작업 처리 — `FlushRemainingTasks()`

프로세스 종료 등으로 워커 스레드들을 더 기다릴 수 없는 상황에서, 큐에 남은 요청들을
비동기 워커 대신 동기적으로 마저 처리하기 위한 함수다. `~CAdoAsyncSrv()` 소멸자
맨 앞에서, `StopThread()`(워커 종료)·`Clear()`(요청 큐 정리)·`ClearAdoPools()`(커넥션
풀 해제)보다 먼저 호출한다 — 워커를 세우거나 커넥션 풀을 해제하기 전에 남은 요청을
먼저 다 처리해 둠으로써, 아직 처리되지 않은 요청이 워커 종료·풀 해제와 타이밍이
겹쳐 유실되거나(요청이 처리되지 못한 채 `Clear()`로 그냥 비워짐) 이미 해제된 풀을
참조하는 일이 없게 한다.

1. `_bStopThread`를 `true`로 설정하고 `_cva.notify_all()` / `_cvProducer.notify_all()`을
   호출해, 이후 새 `Push()`를 막고 대기 중이던 워커/생산자들이 종료 조건을 확인하도록 깨운다.
2. `_mutex`를 짧게 잡은 상태에서 `_queueDBAsyncRq` 전체를 지역 임시 큐(`tempQueue`)로
   `std::move`한다 — 잠금 구간을 큐 이관 한 번으로 최소화해, 그 사이 워커 스레드가
   오래 블로킹되지 않게 한다.
3. 잠금을 푼 뒤, 임시 큐에 옮겨 담은 요청들을 하나씩 꺼내 `_mapCommand`에서 핸들러를
   찾아 `ProcessAsyncCall()`을 **호출부 스레드에서 직접** 실행한다. 핸들러를 찾지 못한
   요청은 조용히 건너뛴다. 각 요청은 처리 후 `SubOutstandingRequest()` 호출과 함께
   `SAFE_DELETE`로 해제한다.
4. 큐가 빌 때까지 반복한 뒤 완료 로그(`LOG_INFO`)를 남기고 반환한다.

- `Action()`의 워커 루프에 있던 타임아웃 재시도 로직(§11.1의 `bReTry` 재큐잉)은 여기에는
  없다 — 종료 처리 경로이므로 실패/미등록 요청을 다시 큐에 넣지 않고 그대로 넘어간다.
- 큐를 옮겨받은 뒤(2단계) 처리하는 동안(3단계)은 `_mutex`를 잡지 않으므로, `FlushRemainingTasks()`
  실행 중에도 다른 스레드가 `GetQueryQueueSize()`/`IsEmpty()` 같은 조회 함수를 호출하는 것
  자체는 안전하다 (다만 이미 `_bStopThread`가 켜진 뒤라 `Push()`는 더 이상 큐에 쌓이지 않는다).

### 11.4 Back-pressure — `WaitPushCapacity()`

DB 처리 속도보다 요청 생산 속도가 빠른 상황에서 큐가 무한정 커지는 것을 막기 위한
선택적 안전장치다. `Push()` 자체는 큐 크기를 검사하지 않으므로, 이 기능을 쓰려면
생산자 쪽 호출부가 `Push()` 전에 `WaitPushCapacity(maxCapacity)`를 명시적으로 호출해야
한다 — 강제되는 하드 리밋이 아니라 협조적(cooperative) 방식이다.

```cpp
pAsyncSrv->WaitPushCapacity(10000); // 큐가 10000개 미만으로 줄어들 때까지 대기
pAsyncSrv->Push(pRequest);
```

- 내부적으로 워커 대기용(`_cva`)과 분리된 별도 조건 변수 `_cvProducer`를 사용한다.
  `Pop()`이 큐에서 항목을 하나 꺼낼 때마다(락 해제 후) `_cvProducer.notify_one()`을
  호출해, `WaitPushCapacity()`로 대기 중이던 생산자 하나를 깨운다. `Push()`가 소비자
  (`_cva.notify_one()`)를 깨우는 것과 대칭되는 구조다.
- `StopThread()`와 `FlushRemainingTasks()` 양쪽 모두 `_cva`뿐 아니라 `_cvProducer`도
  함께 `notify_all()`하므로, 종료 시점에 큐 공간을 기다리며 블로킹 중이던 생산자
  스레드도 함께 깨어나 빠져나올 수 있다 (대기 조건에 `_bStopThread.load()`가 포함되어 있음).
- 생산자가 여러 스레드라면, `WaitPushCapacity()`가 반환된 직후와 실제 `Push()` 사이에
  다른 생산자도 동시에 같은 판단을 내려 함께 `Push()`할 수 있는 TOCTOU 여지가 있다.
  즉 `maxCapacity`는 동시 생산자 수만큼 일시적으로 초과될 수 있는 **연성(soft) 상한**이며,
  정확한 하드 리밋이 필요하면 별도의 원자 카운터로 자리를 예약하는 절차가 추가로 필요하다.
  생산자가 단일 스레드(예: 게임 로직 스레드 하나)라면 이 여지 자체가 없다.
- 성능 측면에서는 `Pop()`마다 추가되는 `notify_one()` 호출 하나뿐이다. 대기 중인
  생산자가 없으면(가장 흔한 경우) 조건 변수 `notify`는 사실상 비용이 거의 없는 연산이라
  핫패스인 `Pop()`에 유의미한 오버헤드를 주지 않는다.
