
//***************************************************************************
// JobQueue.h : interface for the CJobQueue class.
//
//***************************************************************************

#ifndef __JOBQUEUE_H__
#define __JOBQUEUE_H__

#pragma once

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

#ifndef __JOB_H__
#include <Job/Job.h>
#endif

#ifndef __LOCKQUEUE_H__
#include <Job/LockQueue.h>
#endif

#ifndef __JOBTIMER_H__
#include <Job/JobTimer.h>
#endif

#ifndef __GLOBALQUEUE_H__
#include <Job/GlobalQueue.h>
#endif

class CJobTimer;
extern CJobTimer* gpJobTimer;

class CGlobalQueue;
extern CGlobalQueue* gpGlobalQueue;

class CJobQueue;
extern thread_local CJobQueue* LCurrentJobQueue;

class CJobQueue : public enable_shared_from_this<CJobQueue>
{
	enum : uint64
	{
		// 스레드당 최대 연속 실행 시간 (ms 단위, 내부에서 QPC 카운트로 변환)
		EXECUTE_TIME_LIMIT_MS = 10ULL	// 10ms
	};

public:
	// DoAsync 일반 함수 및 템플릿 선언
	void DoAsync(CallbackType&& callback);

	template<typename T, typename Ret, typename... Args>
	void DoAsync(Ret(T::* memFunc)(Args...), Args... args);

	// DoTimer 일반 함수 및 템플릿 선언
	void DoTimer(uint64 tickAfterMs, CallbackType&& callback);

	template<typename T, typename Ret, typename... Args>
	void DoTimer(uint64 tickAfterMs, Ret(T::* memFunc)(Args...), Args... args);

	void ClearJobs() { _jobs.Clear(); }

public:
	void Push(CJobRef job, bool pushOnly = false);
	void Execute();

protected:
	CLockQueue<CJobRef>	_jobs;
	Atomic<int32>		_jobCount = 0;
};

#include <Job/JobQueue.inl>

#endif // ndef __JOBQUEUE_H__