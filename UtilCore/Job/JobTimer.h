
//***************************************************************************
// JobTimer.h : interface for the CJobTimer class.
//
//***************************************************************************

#ifndef __JOBTIMER_H__
#define __JOBTIMER_H__

#pragma once

#include <windows.h>

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h> 
#endif

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h> 
#endif

//***************************************************************************
//	QPC 유틸리티 (JobTimer 전용)
//***************************************************************************
namespace QPCTimer
{
	// 프로세스 시작 시 1회 캐시 (QueryPerformanceFrequency는 변하지 않음)
	inline int64 Frequency()
	{
		static const int64 freq = []() -> int64 {
			LARGE_INTEGER f;
			::QueryPerformanceFrequency(&f);
			return f.QuadPart;
			}();
		return freq;
	}

	// 현재 QPC 카운트
	inline uint64 Now()
	{
		LARGE_INTEGER counter;
		::QueryPerformanceCounter(&counter);
		return static_cast<uint64>(counter.QuadPart);
	}

	// 밀리초 → QPC 카운트 변환
	inline uint64 MsToCount(uint64 ms)
	{
		return (ms * static_cast<uint64>(Frequency())) / 1000ULL;
	}

	// QPC 카운트 → 밀리초 변환 (디버그/로그용)
	inline uint64 CountToMs(uint64 count)
	{
		return (count * 1000ULL) / static_cast<uint64>(Frequency());
	}
}

//***************************************************************************
//	JobData
//***************************************************************************
struct JobData
{
	JobData(weak_ptr<CJobQueue> owner, CJobRef job) : owner(owner), job(job) {}
	weak_ptr<CJobQueue>	owner;
	CJobRef				job;
};

//***************************************************************************
//	TimerItem
//***************************************************************************
struct TimerItem
{
	bool operator<(const TimerItem& other) const
	{
		// priority_queue는 최대 힙 → executeTick이 작을수록 먼저 나오도록 역비교
		return executeTick > other.executeTick;
	}

	uint64		executeTick = 0;	// [개선] QPC 카운트 단위 (기존: ms 단위)
	JobData* jobData = nullptr;
};

//***************************************************************************
//	JobTimer
//***************************************************************************
class CJobTimer
{
public:
	// tickAfterMs: 지금으로부터 몇 밀리초 후에 실행할지 (외부 인터페이스는 ms 유지)
	// 내부적으로는 QPC 카운트로 변환하여 저장
	void		Reserve(uint64 tickAfterMs, weak_ptr<CJobQueue> owner, CJobRef job);

	// now: QPC 카운트 (QPCTimer::Now() 반환값)
	// [개선] 기존 GetTickCount64() 밀리초 → QPC 카운트로 변경
	void		Distribute(uint64 now);

	void		Clear();

private:
	USE_LOCK;
	CPriorityQueue<TimerItem>	_items;
	Atomic<bool>				_distributing = false;
};

#endif // ndef __JOBTIMER_H__