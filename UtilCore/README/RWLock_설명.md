# RWLock

Windows `SRWLOCK` API를 래핑한 C++ 읽기/쓰기 락 클래스.  
읽기는 여러 스레드가 동시에 접근하고, 쓰기는 한 스레드만 독점하는 패턴에 최적화됨.

---

## 클래스 목록

| 클래스 | 역할 |
|---|---|
| `RWLock` | 핵심 락 클래스. 읽기/쓰기 락 획득·해제 API 제공 |
| `WriteLockGuard` | 쓰기 락 RAII 가드. 블로킹 획득 |
| `ReadLockGuard` | 읽기 락 RAII 가드. 블로킹 획득 |
| `TryWriteLockGuard` | 쓰기 락 RAII 가드. 비블로킹 획득 시도 |
| `TryReadLockGuard` | 읽기 락 RAII 가드. 비블로킹 획득 시도 |

---

## RWLock

### 개요

`SRWLOCK` 핸들을 내부에 보유하며 읽기/쓰기 락 획득·해제 API를 제공하는 핵심 클래스.  
복사·이동 불가. 스택 또는 클래스 멤버로 선언하여 사용.

### 생성자 / 소멸자

#### `RWLock()`
`InitializeSRWLock`으로 내부 핸들 초기화.  
동적 할당 없이 스택/멤버 변수로 선언 가능.

#### `~RWLock()`
`_DEBUG` 빌드에서만 `TryWriteLock`으로 잠금 상태를 검사.  
잠긴 채 소멸되면 `assert` 발동. `SRWLOCK`은 별도 Destroy API 없음.

### 메서드

#### `void WriteLock()`
쓰기 락(Exclusive) 획득. 블로킹.  
모든 읽기·쓰기 스레드가 락을 해제할 때까지 대기.

#### `[[nodiscard]] bool TryWriteLock()`
쓰기 락 비블로킹 획득 시도.  
즉시 획득 가능하면 `true`, 불가능하면 `false` 반환.  
반환값 무시 시 컴파일러 경고 발생.

#### `void WriteUnlock()`
쓰기 락 해제.  
`WriteLock` 또는 `TryWriteLock` 성공 후 반드시 호출.

#### `void ReadLock()`
읽기 락(Shared) 획득. 블로킹.  
다른 읽기 스레드와 동시 획득 가능. 쓰기 스레드 존재 시 대기.

#### `[[nodiscard]] bool TryReadLock()`
읽기 락 비블로킹 획득 시도.  
즉시 획득 가능하면 `true`, 불가능하면 `false` 반환.  
반환값 무시 시 컴파일러 경고 발생.

#### `void ReadUnlock()`
읽기 락 해제.  
`ReadLock` 또는 `TryReadLock` 성공 후 반드시 호출.

#### `SRWLOCK* NativeHandle()`
내부 `SRWLOCK` 핸들 포인터 반환.  
`CONDITION_VARIABLE`과 연동 시 `SleepConditionVariableSRW`에 직접 전달하는 용도.

---

## WriteLockGuard

### 개요

쓰기 락 블로킹 획득용 RAII 가드.  
생성 시 `WriteLock` 획득, 소멸 시 `WriteUnlock` 자동 호출.  
예외 발생 시에도 락 해제 보장.

### 생성자 / 소멸자

#### `explicit WriteLockGuard(RWLock& lock)`
`lock.WriteLock()` 호출. 락 획득까지 블로킹.

#### `~WriteLockGuard()`
`lock.WriteUnlock()` 호출. 스코프 종료 또는 예외 발생 시 자동 실행.

### 사용 예시

```cpp
{
    WriteLockGuard guard(lock);  // WriteLock 획득
    _data[id] = hp;
}   // 스코프 종료 → WriteUnlock 자동 호출
```

---

## ReadLockGuard

### 개요

읽기 락 블로킹 획득용 RAII 가드.  
생성 시 `ReadLock` 획득, 소멸 시 `ReadUnlock` 자동 호출.  
예외 발생 시에도 락 해제 보장.

### 생성자 / 소멸자

#### `explicit ReadLockGuard(RWLock& lock)`
`lock.ReadLock()` 호출. 락 획득까지 블로킹.

#### `~ReadLockGuard()`
`lock.ReadUnlock()` 호출. 스코프 종료 또는 예외 발생 시 자동 실행.

### 사용 예시

```cpp
{
    ReadLockGuard guard(lock);  // ReadLock 획득
    return _data.find(id)->second;
}   // 스코프 종료 → ReadUnlock 자동 호출
```

---

## TryWriteLockGuard

### 개요

쓰기 락 비블로킹 획득용 RAII 가드.  
생성 시 `TryWriteLock` 시도, 획득 성공 시 소멸자에서 `WriteUnlock` 자동 호출.  
`IsAcquired()`로 획득 여부 확인 후 임계 구역 진입.

### 생성자 / 소멸자

#### `explicit TryWriteLockGuard(RWLock& lock)`
`lock.TryWriteLock()` 호출. 결과를 `_acquired`에 저장.

#### `~TryWriteLockGuard()`
`_acquired`가 `true`일 때만 `lock.WriteUnlock()` 호출.

### 메서드

#### `[[nodiscard]] bool IsAcquired() const`
락 획득 성공 여부 반환.  
`true`이면 임계 구역 진입 가능. `false`이면 락 미보유 상태.

### 사용 예시

```cpp
TryWriteLockGuard guard(lock);
if (guard.IsAcquired())
{
    _data[id] = hp;   // 예외 발생 시에도 소멸자가 WriteUnlock 보장
}
```

---

## TryReadLockGuard

### 개요

읽기 락 비블로킹 획득용 RAII 가드.  
생성 시 `TryReadLock` 시도, 획득 성공 시 소멸자에서 `ReadUnlock` 자동 호출.  
`IsAcquired()`로 획득 여부 확인 후 임계 구역 진입.

### 생성자 / 소멸자

#### `explicit TryReadLockGuard(RWLock& lock)`
`lock.TryReadLock()` 호출. 결과를 `_acquired`에 저장.

#### `~TryReadLockGuard()`
`_acquired`가 `true`일 때만 `lock.ReadUnlock()` 호출.

### 메서드

#### `[[nodiscard]] bool IsAcquired() const`
락 획득 성공 여부 반환.  
`true`이면 읽기 임계 구역 진입 가능. `false`이면 락 미보유 상태.

### 사용 예시

```cpp
TryReadLockGuard guard(lock);
if (guard.IsAcquired())
{
    return _data.find(id)->second;
}
return -1;  // 락 획득 실패 시 fallback
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
lock.WriteLock();
SleepConditionVariableSRW(&cv, lock.NativeHandle(), INFINITE, 0 /* Exclusive */);

// 읽기 락 잡은 상태에서 조건 대기
lock.ReadLock();
SleepConditionVariableSRW(&cv, lock.NativeHandle(), INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
```

---

## 타 락과 비교

| 항목 | `RWLock` (SRWLock) | `CRITICAL_SECTION` | `std::shared_mutex` |
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
lock.WriteLock();
lock.WriteLock();  // ❌ 데드락
```

### 락 승격 불가 (No Upgrade)
`ReadLock` 상태에서 `WriteLock`으로 직접 승격 불가.  
반드시 읽기 락 해제 후 쓰기 락 획득.

```cpp
lock.ReadLock();
lock.WriteLock();   // ❌ 데드락
lock.ReadUnlock();
```

### TryLock 실패 시 Unlock 금지
`TryWriteLockGuard` / `TryReadLockGuard` 없이 Try 계열을 직접 사용할 경우,  
`false` 반환 시 락을 획득하지 못한 상태이므로 Unlock 호출 금지.

```cpp
if (lock.TryWriteLock())
{
    // ...
    lock.WriteUnlock();  // ✅ 획득 성공 시에만 해제
}
// lock.WriteUnlock();   // ❌ 획득 실패 후 해제 금지
```

---

## 사용 예제

### 예제 1 — 플레이어 캐시 (읽기 다수 / 쓰기 소수)

읽기가 압도적으로 많고 쓰기가 드문 구조. `ReadLockGuard`로 다수 스레드 동시 조회 허용,  
`WriteLockGuard`로 삽입·삭제 독점 보호.

```cpp
#include "RWLock.h"
#include <unordered_map>
#include <string>

class PlayerCache
{
public:
    // 플레이어 정보 등록 — 쓰기 독점
    void Add(int id, const std::string& name)
    {
        WriteLockGuard guard(_lock);
        _players[id] = name;
    }

    // 플레이어 정보 삭제 — 쓰기 독점
    void Remove(int id)
    {
        WriteLockGuard guard(_lock);
        _players.erase(id);
    }

    // 플레이어 이름 조회 — 읽기 공유 (다수 스레드 동시 접근 가능)
    std::string GetName(int id) const
    {
        ReadLockGuard guard(_lock);
        auto it = _players.find(id);
        return it != _players.end() ? it->second : "";
    }

    // 플레이어 존재 여부 확인 — 읽기 공유
    bool Contains(int id) const
    {
        ReadLockGuard guard(_lock);
        return _players.count(id) > 0;
    }

private:
    mutable RWLock                      _lock;
    std::unordered_map<int, std::string> _players;
};
```

---

### 예제 2 — 논블로킹 스탯 업데이트 (TryWriteLockGuard)

락 경합 시 대기하지 않고 즉시 반환. 게임 루프처럼 프레임마다 호출되는 경로에서  
블로킹을 피하고 싶을 때 사용.

```cpp
#include "RWLock.h"
#include <unordered_map>

class StatBoard
{
public:
    // 논블로킹 업데이트 — 락 획득 실패 시 false 반환, 대기 없음
    bool TryUpdateStat(int playerId, int value)
    {
        TryWriteLockGuard guard(_lock);
        if (!guard.IsAcquired())
            return false;   // 다른 스레드가 쓰는 중 → 이번 프레임 스킵

        _stats[playerId] = value;
        return true;
    }

    // 논블로킹 조회 — 락 획득 실패 시 캐시된 기본값 반환
    int TryGetStat(int playerId, int fallback = 0) const
    {
        TryReadLockGuard guard(_lock);
        if (!guard.IsAcquired())
            return fallback;

        auto it = _stats.find(playerId);
        return it != _stats.end() ? it->second : fallback;
    }

private:
    mutable RWLock                  _lock;
    std::unordered_map<int, int>    _stats;
};
```

---

### 예제 3 — 조건 변수 연동 (작업 큐)

`NativeHandle()`을 `SleepConditionVariableSRW`에 전달해 생산자-소비자 패턴 구현.  
소비자는 쓰기 락을 잡고 대기, 생산자가 항목 추가 후 `WakeConditionVariable` 호출.

```cpp
#include "RWLock.h"
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
        WriteLockGuard guard(_lock);
        _queue.push(job);
        WakeConditionVariable(&_cv);        // 대기 중인 소비자 1개 깨우기
    }

    // 소비자 — 작업이 생길 때까지 대기 (블로킹)
    int Pop()
    {
        _lock.WriteLock();

        // 큐가 빌 때마다 조건 변수로 대기
        while (_queue.empty())
        {
            // WriteLock을 원자적으로 해제하고 대기, 깨어나면 재획득
            SleepConditionVariableSRW(&_cv, _lock.NativeHandle(), INFINITE, 0);
        }

        int job = _queue.front();
        _queue.pop();

        _lock.WriteUnlock();
        return job;
    }

private:
    RWLock              _lock;
    CONDITION_VARIABLE  _cv;
    std::queue<int>     _queue;
};
```
