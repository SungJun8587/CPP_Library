
//***************************************************************************
// JobQueue.cpp : implementation of the CJobQueue class.
//
//***************************************************************************

#include "pch.h"
#include "JobQueue.h"

void CJobQueue::DoAsync(CallbackType&& callback)
{
	Push(CObjectPool<CJob>::MakeShared(std::move(callback)));
}

void CJobQueue::DoTimer(uint64 tickAfterMs, CallbackType&& callback)
{
	CJobRef job = CObjectPool<CJob>::MakeShared(std::move(callback));
	if( gpJobTimer != nullptr ) gpJobTimer->Reserve(tickAfterMs, shared_from_this(), job);
}

void CJobQueue::Push(CJobRef job, bool pushOnly)
{
	const int32 prevCount = _jobCount.fetch_add(1);
	_jobs.Push(job);

	if( prevCount == 0 )
	{
		if( LCurrentJobQueue == nullptr && pushOnly == false )
			Execute();
		else
		{
			if( gpGlobalQueue != nullptr ) gpGlobalQueue->Push(shared_from_this());
		}
	}
}

void CJobQueue::Execute()
{
	LCurrentJobQueue = this;

	// [개선] 슬롯 시작 시각을 QPC 카운트로 설정
	// LEndTickCount는 InitTLS()에서 QPCTimer::Now()로 초기화됐으므로
	// 여기서 갱신할 때도 QPCTimer를 사용해 단위 일치
	LEndTickCount = QPCTimer::Now()
		+ QPCTimer::MsToCount(EXECUTE_TIME_LIMIT_MS);

	while( true )
	{
		// 명시적 크기 기반 PopAll (암묵적 shared_ptr bool 변환 제거)
		CVector<CJobRef> jobs;
		_jobs.PopAll(OUT jobs);

		const int32 jobCount = static_cast<int32>(jobs.size());
		for( int32 i = 0; i < jobCount; i++ )
			jobs[i]->Execute();

		// 처리한 만큼 차감. 0이 되면 큐가 완전히 비어있음 → 종료
		if( _jobCount.fetch_sub(jobCount) == jobCount )
		{
			LCurrentJobQueue = nullptr;
			return;
		}

		// 슬롯 만료 → GlobalQueue에 재위임
		if( QPCTimer::Now() >= LEndTickCount )
		{
			LCurrentJobQueue = nullptr;
			if( gpGlobalQueue != nullptr ) gpGlobalQueue->Push(shared_from_this());
			break;
		}
	}
}
