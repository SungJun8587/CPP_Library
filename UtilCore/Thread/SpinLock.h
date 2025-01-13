
//***************************************************************************
// SpinLock.h : interface for the CSpinLock class.
//
//***************************************************************************

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#pragma once

#ifndef _ATOMIC_
#include <atomic>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#if _DEBUG
    #ifndef __DEADLOCKPROFILER_H__
    #include <Thread/DeadLockProfiler.h>
    #endif
#endif

//***************************************************************************
//  RW SpinLock
//      [WWWWWWWW][WWWWWWWW][RRRRRRRR][RRRRRRRR]
//      - W : WriteFlag (Exclusive Lock Owner ThreadId)
//      - R : ReadFlag (Shared Lock Count)
class CSpinLock
{
    enum : uint32
    {
        ACQUIRE_TIMEOUT_TICK = 10000,
        MAX_SPIN_COUNT = 5000,
        WRITE_THREAD_MASK = 0xFFFF'0000,
        READ_COUNT_MASK = 0x0000'FFFF,
        EMPTY_FLAG = 0x0000'0000
    };

public:
    void WriteLock(const char* name);
    void WriteUnlock(const char* name);
    void ReadLock(const char* name);
    void ReadUnlock(const char* name);

private:
    std::atomic<uint32> _lockFlag = EMPTY_FLAG;
    uint16 _writeCount = 0;
};

//***************************************************************************
// LockGuards
class ReadLockGuard
{
public:
	ReadLockGuard(CSpinLock& lock, const char* name) : _lock(lock), _name(name) { _lock.ReadLock(name); }
	~ReadLockGuard() { _lock.ReadUnlock(_name); }

private:
    CSpinLock& _lock;
    const char* _name;
};

class WriteLockGuard
{
public:
	WriteLockGuard(CSpinLock& lock, const char* name) : _lock(lock), _name(name) { _lock.WriteLock(name); }
	~WriteLockGuard() { _lock.WriteUnlock(_name); }

private:
    CSpinLock& _lock;
    const char* _name;
};

//***************************************************************************
// Lock
#define USE_MANY_LOCKS(count)	CSpinLock _locks[count];
#define USE_LOCK				USE_MANY_LOCKS(1)
#define	READ_LOCK_IDX(idx)		ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
#define READ_LOCK				READ_LOCK_IDX(0)
#define	WRITE_LOCK_IDX(idx)		WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK				WRITE_LOCK_IDX(0)
//***************************************************************************

#endif // ndef __SPINLOCK_H__
