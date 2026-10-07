
//***************************************************************************
// JobTimer.cpp : implementation of the CJobTimer class.
//
//***************************************************************************

#include "pch.h"
#include "JobTimer.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief 지정된 시간 이후에 실행될 비동기 작업을 예약합니다.
// @detail 현재 QPC 시각에 지연 시간을 QPC 카운트로 환산한 값을 더해 목표 실행 시각을 구하고, 객체 풀에서 JobData를 할당받아 타이머 큐에 등록합니다.
// @param tickAfterMs 지금으로부터 몇 밀리초 후에 실행할지 지정하는 지연 시간
// @param owner 작업을 처리할 대상 작업 큐의 약한 참조 (weak_ptr)
// @param job 예약할 작업 객체 레퍼런스
// @details _items.push()가 예외(bad_alloc 등)를 던지면 객체 풀에서 꺼낸 jobData를 풀에 반환한 뒤
//          예외를 그대로 다시 던집니다.
//***************************************************************************
void CJobTimer::Reserve(uint64 tickAfterMs, weak_ptr<CJobQueue> owner, CJobRef job)
{
	// executeTick = 현재 QPC 카운트 + (tickAfterMs를 QPC 카운트로 변환)
	const uint64 executeTick = QPCTimer::Now() + QPCTimer::MsToCount(tickAfterMs);

	JobData* jobData = CObjectPool<JobData>::Pop(owner, job);

	try
	{
		PRWriteLockGuard writeLock(_lock);
		_items.push(TimerItem{ executeTick, jobData });
	}
	catch( ... )
	{
		CObjectPool<JobData>::Push(jobData);
		throw;
	}
}

//***************************************************************************
// @brief 시간이 만료된 타이머 작업을 검사하여 해당 작업 큐로 분배합니다.
// @detail 원자적 플래그로 중복 실행을 방지하며, 현재 QPC 시각을 기준으로 만료된 항목들을 우선순위 큐에서 추출하여 각 작업 큐에 전달합니다.
// @param now 현재 QPC 카운트 값 (QPCTimer::Now() 반환값)
//***************************************************************************
void CJobTimer::Distribute(uint64 now)
{
	// 중복 실행 방지 (다른 스레드가 이미 Distribute 중이면 스킵)
	if( _distributing.exchange(true) == true )
		return;

	CVector<TimerItem> items;
	{
		PRWriteLockGuard writeLock(_lock);
		while( _items.empty() == false )
		{
			const TimerItem& timerItem = _items.top();
			// now와 executeTick 모두 QPC 카운트 단위입니다.
			if( now < timerItem.executeTick )
				break;

			items.push_back(timerItem);
			_items.pop();
		}
	}

	for( TimerItem& item : items )
	{
		if( CJobQueueRef owner = item.jobData->owner.lock() )
			owner->Push(item.jobData->job, true);

		CObjectPool<JobData>::Push(item.jobData);
	}

	_distributing.store(false);
}

//***************************************************************************
// @brief 타이머에 등록된 모든 예약 작업을 초기화합니다.
// @detail 쓰기 락 안에서는 항목만 인출하고, JobData 반환(보관 중이던 작업 객체의 해제)은 락 밖에서 수행합니다.
//         작업 객체의 소멸자가 다시 Reserve()를 호출하더라도 락 재진입 교착이 발생하지 않습니다.
//***************************************************************************
void CJobTimer::Clear()
{
	CVector<JobData*> jobDatas;
	{
		PRWriteLockGuard writeLock(_lock);
		jobDatas.reserve(_items.size());
		while( _items.empty() == false )
		{
			jobDatas.push_back(_items.top().jobData);
			_items.pop();
		}
	}

	for( JobData* jobData : jobDatas )
		CObjectPool<JobData>::Push(jobData);
}