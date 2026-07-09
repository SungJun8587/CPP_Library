
//***************************************************************************
// JobTimer.cpp : implementation of the CJobTimer class.
//
//***************************************************************************

#include "pch.h"
#include "JobTimer.h"

void CJobTimer::Reserve(uint64 tickAfterMs, weak_ptr<CJobQueue> owner, CJobRef job)
{
	// executeTick = 현재 QPC 카운트 + (tickAfterMs를 QPC 카운트로 변환)
	const uint64 executeTick = QPCTimer::Now() + QPCTimer::MsToCount(tickAfterMs);

	JobData* jobData = CObjectPool<JobData>::Pop(owner, job);

	WRITE_LOCK;
	_items.push(TimerItem{ executeTick, jobData });
}

void CJobTimer::Distribute(uint64 now)
{
	// 중복 실행 방지 (다른 스레드가 이미 Distribute 중이면 스킵)
	if( _distributing.exchange(true) == true )
		return;

	CVector<TimerItem> items;
	{
		WRITE_LOCK;
		while( _items.empty() == false )
		{
			const TimerItem& timerItem = _items.top();
			// [개선] now와 executeTick 모두 QPC 카운트 단위 → 올바른 비교
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

void CJobTimer::Clear()
{
	WRITE_LOCK;
	while( _items.empty() == false )
	{
		const TimerItem& timerItem = _items.top();
		CObjectPool<JobData>::Push(timerItem.jobData);
		_items.pop();
	}
}
