
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

#ifndef __PLATFORMLOCK_H__
#include <Thread/PlatformLock.h>
#endif

//***************************************************************************
//	QPC 유틸리티 (JobTimer 전용)
//***************************************************************************
namespace QPCTimer
{
	//***************************************************************************
	// @brief 고해상도 성능 카운터의 주파수(초당 틱 수)를 반환합니다.
	// @detail 프로세스 시작 시 단 1회만 QueryPerformanceFrequency를 호출하여 캐시된 주파수 값을 반환합니다.
	// @return 초당 QPC 카운트 횟수
	//***************************************************************************
	inline int64 Frequency()
	{
		static const int64 freq = []() -> int64 {
			LARGE_INTEGER f;
			::QueryPerformanceFrequency(&f);
			return f.QuadPart;
			}();
		return freq;
	}

	//***************************************************************************
	// @brief 현재 고해상도 성능 카운터의 카운트 값을 반환합니다.
	// @detail QueryPerformanceCounter를 호출하여 현재 시점의 정밀한 QPC 카운트를 가져옵니다.
	// @return 현재 QPC 카운트 값
	//***************************************************************************
	inline uint64 Now()
	{
		LARGE_INTEGER counter;
		::QueryPerformanceCounter(&counter);
		return static_cast<uint64>(counter.QuadPart);
	}

	//***************************************************************************
	// @brief 밀리초 단위를 QPC 카운트 단위로 변환합니다.
	// @detail 주어진 밀리초 값에 주파수를 곱하고 1000으로 나누어 해당하는 QPC 카운트를 계산합니다.
	// @param ms 변환할 밀리초 시간
	// @return 환산된 QPC 카운트 값
	//***************************************************************************
	inline uint64 MsToCount(uint64 ms)
	{
		return (ms * static_cast<uint64>(Frequency())) / 1000ULL;
	}

	//***************************************************************************
	// @brief QPC 카운트 단위를 밀리초 단위로 변환합니다.
	// @detail 디버그 및 로그 출력을 위해 QPC 카운트 값을 밀리초 단위 시간으로 환산합니다.
	// @param count 변환할 QPC 카운트 값
	// @return 환산된 밀리초 시간
	//***************************************************************************
	inline uint64 CountToMs(uint64 count)
	{
		return (count * 1000ULL) / static_cast<uint64>(Frequency());
	}
}

//***************************************************************************
// @brief 예약된 타이머 작업에 필요한 소유자 정보와 작업 객체를 관리하는 구조체입니다.
// @detail 타이머가 만료되었을 때 어떤 작업 큐(CJobQueue)로 어떤 작업(CJob)을 전달해야 하는지 저장합니다.
//***************************************************************************
struct JobData
{
	//***************************************************************************
	// @brief JobData 구조체의 생성자입니다.
	// @detail 전달받은 작업 큐 소유자(약한 참조)와 작업 객체 레퍼런스로 초기화합니다.
	// @param owner 작업 큐의 약한 참조 (weak_ptr)
	// @param job 실행할 작업 객체 레퍼런스
	//***************************************************************************
	JobData(weak_ptr<CJobQueue> owner, CJobRef job) : owner(owner), job(job) {}

	weak_ptr<CJobQueue>	owner; // 작업을 처리할 대상 작업 큐의 약한 참조 (weak_ptr)
	CJobRef				job;   // 예약된 작업 객체 레퍼런스
};

//***************************************************************************
// @brief 우선순위 큐에서 관리되는 개별 타이머 항목 구조체입니다.
// @detail 실행 시각(Tick)을 기준으로 우선순위를 비교할 수 있는 연산자를 제공합니다.
//***************************************************************************
struct TimerItem
{
	//***************************************************************************
	// @brief 우선순위 큐 정렬을 위한 비교 연산자입니다.
	// @detail min-heap(최소 힙) 구조를 구현하기 위해 실행 시각(executeTick)이 더 작은 항목이 먼저 나오도록 역비교를 수행합니다.
	// @param other 비교할 다른 TimerItem 객체
	// @return 현재 객체의 실행 시각이 다른 객체보다 늦은 경우 true, 아니면 false
	//***************************************************************************
	bool operator<(const TimerItem& other) const
	{
		return executeTick > other.executeTick;
	}

	uint64		executeTick = 0;	// 타이머가 만료되어 실행되어야 할 목표 QPC 시각
	JobData* jobData = nullptr;     // 해당 타이머 항목이 품고 있는 작업 데이터 포인터
};

//***************************************************************************
// @brief 비동기 작업을 특정 시간 이후에 실행되도록 예약하고 관리하는 타이머 클래스입니다.
// @detail 고해상도 타이머(QPC)를 기반으로 작업들의 실행 시각을 관리하며, 우선순위 큐를 통해 만료된 작업을 배포합니다.
//***************************************************************************
class CJobTimer
{
public:
	void		Reserve(uint64 tickAfterMs, weak_ptr<CJobQueue> owner, CJobRef job);
	void		Distribute(uint64 now);
	void		Clear();

private:
	mutable PRWLock				_lock;                  // 타이머 항목 동기화를 위한 플랫폼 독립적 읽기/쓰기 락
	CPriorityQueue<TimerItem>	_items;					// 예약된 타이머 항목들을 시간 순서대로 관리하는 우선순위 큐 컨테이너
	Atomic<bool>				_distributing = false;	// 현재 타이머 분배 작업이 진행 중인지 나타내는 원자적 플래그
};

#endif // ndef __JOBTIMER_H__