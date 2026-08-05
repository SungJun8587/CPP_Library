# RWLock

Windows `SRWLOCK` API를 래핑한 C++ 읽기/쓰기 락 클래스.  
읽기는 여러 스레드가 동시에 접근하고, 쓰기는 한 스레드만 독점하는 패턴에 최적화됨.  
데드락 프로파일러(`CDeadLockProfiler`)와 연동 가능한 락 이름 인자를 API 전반에 지원.

---

## 클래스 목록

| 클래스 | 역할 |
|---|---|
| `CSRWLock` | 핵심 락 클래스. 읽기/쓰기 락 획득·해제 API 제공 |
| `ExclusiveLockGuard` | 쓰기 락 RAII 가드. 블로킹 획득 |
| `SharedLockGuard` | 읽기 락 RAII 가드. 블로킹 획득 |
| `TryExclusiveLockGuard` | 쓰기 락 RAII 가드. 비블로킹 획득 시도 |
| `TrySharedLockGuard` | 읽기 락 RAII 가드. 비블로킹 획득 시도 |
| `CSRWCustomLockGuard<LockObj>` | 매크로 연동용 통합 가드 템플릿 (읽기/쓰기 타입을 인자로 받음) |

---

## CSRWLock

### 개요

`SRWLOCK` 핸들을 내부에 보유하며 읽기/쓰기 락 획득·해제 API를 제공하는 핵심 클래스.  
복사·이동 불가. 스택 또는 클래스 멤버로 선언하여 사용.

### 생성자 / 소멸자

#### `CSRWLock()`
`InitializeSRWLock`으로 내부 핸들 초기화.  
동적 할당 없이 스택/멤버 변수로 선언 가능.

#### `~CSRWLock()`
`_DEBUG` 빌드에서만 `TryExclusiveLock`으로 잠금 상태를 검사.  
잠긴 채 소멸되면 `assert` 발동. `SRWLOCK`은 별도 Destroy API 없음.

### 메서드

#### `void ExclusiveLock(const char* name = nullptr)`
쓰기 락(Exclusive) 획득. 블로킹.  
모든 읽기·쓰기 스레드가 락을 해제할 때까지 대기.  
`USE_GPDEADLOCKPROFILER && _DEBUG` 빌드에서 `name`이 지정되면 데드락 프로파일러에 획득 순서를 기록.

#### `[[nodiscard]] bool TryExclusiveLock(const char* name = nullptr)`
쓰기 락 비블로킹 획득 시도.  
즉시 획득 가능하면 `true`, 불가능하면 `false` 반환.  
반환값 무시 시 컴파일러 경고 발생.  
획득 성공 시에만 프로파일러에 기록(실패한 시도는 기록하지 않음).

#### `void ExclusiveUnLock(const char* name = nullptr)`
쓰기 락 해제.  
`ExclusiveLock` 또는 `TryExclusiveLock` 성공 후 반드시 호출.  
프로파일러 연동 빌드에서 `name`으로 해제 기록.

#### `void SharedLock(const char* name = nullptr)`
읽기 락(Shared) 획득. 블로킹.  
다른 읽기 스레드와 동시 획득 가능. 쓰기 스레드 존재 시 대기.

#### `[[nodiscard]] bool TrySharedLock(const char* name = nullptr)`
읽기 락 비블로킹 획득 시도.  
즉시 획득 가능하면 `true`, 불가능하면 `false` 반환.  
반환값 무시 시 컴파일러 경고 발생.

#### `void SharedUnLock(const char* name = nullptr)`
읽기 락 해제.  
`SharedLock` 또는 `TrySharedLock` 성공 후 반드시 호출.

#### `SRWLOCK* NativeHandle()`
내부 `SRWLOCK` 핸들 포인터 반환.  
`CONDITION_VARIABLE`과 연동 시 `SleepConditionVariableSRW`에 직접 전달하는 용도.

### 데드락 프로파일러 연동

`USE_GPDEADLOCKPROFILER`와 `_DEBUG`가 모두 정의된 빌드에서, 각 Lock/Unlock 계열 메서드에 전달한 `name` 인자가 전역 `gpDeadLockProfiler`(`CDeadLockProfiler`)로 전달되어 락 획득 순서를 추적한다. `name`을 생략하면(`nullptr`) 해당 호출은 프로파일링 대상에서 제외된다. 릴리즈 빌드나 프로파일러 미사용 빌드에서는 `name` 인자가 완전히 무시되며 순수 SRWLock 호출만 수행되어 오버헤드가 없다.

---

## ExclusiveLockGuard

### 개요

쓰기 락 블로킹 획득용 RAII 가드.  
생성 시 `ExclusiveLock` 획득, 소멸 시 `ExclusiveUnLock` 자동 호출.  
예외 발생 시에도 락 해제 보장.

### 생성자 / 소멸자

#### `explicit ExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr)`
`lock.ExclusiveLock(name)` 호출. 락 획득까지 블로킹.

#### `~ExclusiveLockGuard()`
`lock.ExclusiveUnLock(name)` 호출. 스코프 종료 또는 예외 발생 시 자동 실행.

### 사용 예시

```cpp
{
    ExclusiveLockGuard guard(lock);  // ExclusiveLock 획득
    _data[id] = hp;
}   // 스코프 종료 → ExclusiveUnLock 자동 호출
```

---

## SharedLockGuard

### 개요

읽기 락 블로킹 획득용 RAII 가드.  
생성 시 `SharedLock` 획득, 소멸 시 `SharedUnLock` 자동 호출.  
예외 발생 시에도 락 해제 보장.

### 생성자 / 소멸자

#### `explicit SharedLockGuard(CSRWLock& lock, const char* name = nullptr)`
`lock.SharedLock(name)` 호출. 락 획득까지 블로킹.

#### `~SharedLockGuard()`
`lock.SharedUnLock(name)` 호출. 스코프 종료 또는 예외 발생 시 자동 실행.

### 사용 예시

```cpp
{
    SharedLockGuard guard(lock);  // SharedLock 획득
    return _data.find(id)->second;
}   // 스코프 종료 → SharedUnLock 자동 호출
```

---

## TryExclusiveLockGuard

### 개요

쓰기 락 비블로킹 획득용 RAII 가드.  
생성 시 `TryExclusiveLock` 시도, 획득 성공 시 소멸자에서 `ExclusiveUnLock` 자동 호출.  
`IsAcquired()`로 획득 여부 확인 후 임계 구역 진입.

### 생성자 / 소멸자

#### `explicit TryExclusiveLockGuard(CSRWLock& lock, const char* name = nullptr)`
`lock.TryExclusiveLock(name)` 호출. 결과를 `_acquired`에 저장.

#### `~TryExclusiveLockGuard()`
`_acquired`가 `true`일 때만 `lock.ExclusiveUnLock(name)` 호출.

### 메서드

#### `[[nodiscard]] bool IsAcquired() const`
락 획득 성공 여부 반환.  
`true`이면 임계 구역 진입 가능. `false`이면 락 미보유 상태.

### 사용 예시

```cpp
TryExclusiveLockGuard guard(lock);
if (guard.IsAcquired())
{
    _data[id] = hp;   // 예외 발생 시에도 소멸자가 ExclusiveUnLock 보장
}
```

---

## TrySharedLockGuard

### 개요

읽기 락 비블로킹 획득용 RAII 가드.  
생성 시 `TrySharedLock` 시도, 획득 성공 시 소멸자에서 `SharedUnLock` 자동 호출.  
`IsAcquired()`로 획득 여부 확인 후 임계 구역 진입.

### 생성자 / 소멸자

#### `explicit TrySharedLockGuard(CSRWLock& lock, const char* name = nullptr)`
`lock.TrySharedLock(name)` 호출. 결과를 `_acquired`에 저장.

#### `~TrySharedLockGuard()`
`_acquired`가 `true`일 때만 `lock.SharedUnLock(name)` 호출.

### 메서드

#### `[[nodiscard]] bool IsAcquired() const`
락 획득 성공 여부 반환.  
`true`이면 읽기 임계 구역 진입 가능. `false`이면 락 미보유 상태.

### 사용 예시

```cpp
TrySharedLockGuard guard(lock);
if (guard.IsAcquired())
{
    return _data.find(id)->second;
}
return -1;  // 락 획득 실패 시 fallback
```

---

## CSRWCustomLockGuard / 매크로 연동

### 개요

`enum class SRWLockType { Read, Write }`으로 락 타입을 지정받아 생성자/소멸자에서 `ExclusiveLock`/`SharedLock`과 대응하는 Unlock을 자동 호출하는 템플릿 가드. `LockObj`를 템플릿 인자로 받아 `CSRWLock` 외의 호환 락 타입에도 재사용 가능. 프로젝트 전역 매크로 3종으로 감싸서 사용한다.

| 매크로 | 역할 |
|---|---|
| `SRW_USE_LOCK` | `mutable CSRWLock _lock;` 멤버 선언 |
| `SRW_WRITE_LOCK` | 함수 스코프에 쓰기 락 가드 생성 (`__func__`을 프로파일러 이름으로 자동 전달) |
| `SRW_READ_LOCK` | 함수 스코프에 읽기 락 가드 생성 (`__func__`을 프로파일러 이름으로 자동 전달) |

### 사용 예시

```cpp
class PlayerCache
{
    SRW_USE_LOCK;
    std::unordered_map<int, std::string> _players;

public:
    void Add(int id, const std::string& name)
    {
        SRW_WRITE_LOCK;
        _players[id] = name;
    }

    std::string GetName(int id) const
    {
        SRW_READ_LOCK;
        auto it = _players.find(id);
        return it != _players.end() ? it->second : "";
    }
};
```

---

## 동작 원리

```
스레드 A (읽기) ──┐
스레드 B (읽기) ──┼── 동시 접근 가능 ✅
스레드 C (읽기) ──┘

스레드 D (쓰기) ── A, B, C 모두 해제 후 단독 진입 ✅
                   쓰기 중에는 읽기 스레드도 블록   ✅
```

---

## 조건 변수 연동

`CONDITION_VARIABLE`과 함께 사용할 경우 `NativeHandle()`로 내부 핸들을 전달.

```cpp
CONDITION_VARIABLE cv;
InitializeConditionVariable(&cv);

// 쓰기 락 잡은 상태에서 조건 대기
lock.ExclusiveLock();
SleepConditionVariableSRW(&cv, lock.NativeHandle(), INFINITE, 0 /* Exclusive */);

// 읽기 락 잡은 상태에서 조건 대기
lock.SharedLock();
SleepConditionVariableSRW(&cv, lock.NativeHandle(), INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
```

---

## 타 락과 비교

| 항목 | `CSRWLock` (SRWLock) | `CRITICAL_SECTION` | `std::shared_mutex` |
|---|---|---|---|
| 크기 | 8 bytes | 40 bytes | 구현체마다 상이 |
| 읽기 동시 접근 | ✅ | ❌ | ✅ |
| 재진입 | ❌ | ✅ | ❌ |
| 조건 변수 | ✅ (`CONDITION_VARIABLE`) | ✅ | ✅ |
| 플랫폼 | Windows 전용 | Windows 전용 | 크로스 플랫폼 |

---

## 주의사항

### 재진입 불가 (Non-recursive)
동일 스레드에서 같은 락을 중첩 획득하면 데드락 발생.

```cpp
lock.ExclusiveLock();
lock.ExclusiveLock();  // ❌ 데드락
```

### 락 승격 불가 (No Upgrade)
`SharedLock` 상태에서 `ExclusiveLock`으로 직접 승격 불가.  
반드시 읽기 락 해제 후 쓰기 락 획득.

```cpp
lock.SharedLock();
lock.ExclusiveLock();   // ❌ 데드락
lock.SharedUnLock();
```

### TryLock 실패 시 Unlock 금지
`TryExclusiveLockGuard` / `TrySharedLockGuard` 없이 Try 계열을 직접 사용할 경우,  
`false` 반환 시 락을 획득하지 못한 상태이므로 Unlock 호출 금지.

```cpp
if (lock.TryExclusiveLock())
{
    // ...
    lock.ExclusiveUnLock();  // ✅ 획득 성공 시에만 해제
}
// lock.ExclusiveUnLock();   // ❌ 획득 실패 후 해제 금지
```

---

## 사용 예제

### 예제 1 — 플레이어 캐시 (읽기 다수 / 쓰기 소수)

읽기가 압도적으로 많고 쓰기가 드문 구조. `SharedLockGuard`로 다수 스레드 동시 조회 허용,  
`ExclusiveLockGuard`로 삽입·삭제 독점 보호.

```cpp
#include "SRWLock.h"
#include <unordered_map>
#include <string>

class PlayerCache
{
public:
    // 플레이어 정보 등록 — 쓰기 독점
    void Add(int id, const std::string& name)
    {
        ExclusiveLockGuard guard(_lock);
        _players[id] = name;
    }

    // 플레이어 정보 삭제 — 쓰기 독점
    void Remove(int id)
    {
        ExclusiveLockGuard guard(_lock);
        _players.erase(id);
    }

    // 플레이어 이름 조회 — 읽기 공유 (다수 스레드 동시 접근 가능)
    std::string GetName(int id) const
    {
        SharedLockGuard guard(_lock);
        auto it = _players.find(id);
        return it != _players.end() ? it->second : "";
    }

    // 플레이어 존재 여부 확인 — 읽기 공유
    bool Contains(int id) const
    {
        SharedLockGuard guard(_lock);
        return _players.count(id) > 0;
    }

private:
    mutable CSRWLock                     _lock;
    std::unordered_map<int, std::string> _players;
};
```

---

### 예제 2 — 논블로킹 스탯 업데이트 (TryExclusiveLockGuard)

락 경합 시 대기하지 않고 즉시 반환. 게임 루프처럼 프레임마다 호출되는 경로에서  
블로킹을 피하고 싶을 때 사용.

```cpp
#include "SRWLock.h"
#include <unordered_map>

class StatBoard
{
public:
    // 논블로킹 업데이트 — 락 획득 실패 시 false 반환, 대기 없음
    bool TryUpdateStat(int playerId, int value)
    {
        TryExclusiveLockGuard guard(_lock);
        if (!guard.IsAcquired())
            return false;   // 다른 스레드가 쓰는 중 → 이번 프레임 스킵

        _stats[playerId] = value;
        return true;
    }

    // 논블로킹 조회 — 락 획득 실패 시 캐시된 기본값 반환
    int TryGetStat(int playerId, int fallback = 0) const
    {
        TrySharedLockGuard guard(_lock);
        if (!guard.IsAcquired())
            return fallback;

        auto it = _stats.find(playerId);
        return it != _stats.end() ? it->second : fallback;
    }

private:
    mutable CSRWLock              _lock;
    std::unordered_map<int, int>  _stats;
};
```

---

### 예제 3 — 조건 변수 연동 (작업 큐)

`NativeHandle()`을 `SleepConditionVariableSRW`에 전달해 생산자-소비자 패턴 구현.  
소비자는 쓰기 락을 잡고 대기, 생산자가 항목 추가 후 `WakeConditionVariable` 호출.

```cpp
#include "SRWLock.h"
#include <queue>
#include <windows.h>

class JobQueue
{
public:
    JobQueue()
    {
        InitializeConditionVariable(&_cv);
    }

    // 생산자 — 작업 추가 후 소비자 깨우기
    void Push(int job)
    {
        ExclusiveLockGuard guard(_lock);
        _queue.push(job);
        WakeConditionVariable(&_cv);        // 대기 중인 소비자 1개 깨우기
    }

    // 소비자 — 작업이 생길 때까지 대기 (블로킹)
    int Pop()
    {
        _lock.ExclusiveLock();

        // 큐가 빌 때마다 조건 변수로 대기
        while (_queue.empty())
        {
            // ExclusiveLock을 원자적으로 해제하고 대기, 깨어나면 재획득
            SleepConditionVariableSRW(&_cv, _lock.NativeHandle(), INFINITE, 0);
        }

        int job = _queue.front();
        _queue.pop();

        _lock.ExclusiveUnLock();
        return job;
    }

private:
    CSRWLock             _lock;
    CONDITION_VARIABLE   _cv;
    std::queue<int>      _queue;
};
```
