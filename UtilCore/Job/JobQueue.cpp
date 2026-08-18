
//***************************************************************************
// JobQueue.cpp : implementation of the CJobQueue class.
//
//***************************************************************************

#include "pch.h"
#include "JobQueue.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief 콜백 함수 기반의 비동기 작업을 큐에 등록합니다.
// @detail 전달받은 콜백 함수를 객체 풀을 통해 작업 객체로 생성한 뒤 큐에 푸시합니다.
// @param callback 실행할 콜백 함수 (오른값 레퍼런스)
//***************************************************************************
void CJobQueue::DoAsync(CallbackType&& callback)
{
	Push(CObjectPool<CJob>::MakeShared(std::move(callback)));
}

//***************************************************************************
// @brief 지정된 시간(밀리초) 이후에 실행될 콜백 함수 기반의 타이머 작업을 예약합니다.
// @detail 전달받은 콜백 함수로 작업 객체를 생성한 후, 전역 타이머 관리자를 통해 지정된 지연 시간 뒤에 실행되도록 예약합니다.
// @param tickAfterMs 작업을 지연시킬 시간 (밀리초 단위)
// @param callback 예약할 콜백 함수 (오른값 레퍼런스)
//***************************************************************************
void CJobQueue::DoTimer(uint64 tickAfterMs, CallbackType&& callback)
{
	CJobRef job = CObjectPool<CJob>::MakeShared(std::move(callback));
	if( gpJobTimer != nullptr ) gpJobTimer->Reserve(tickAfterMs, shared_from_this(), job);
}

//***************************************************************************
// @brief 작업 큐에 새로운 작업을 추가합니다.
// @detail 큐 내부에 작업을 푸시하고, 대기 중인 작업이 처음 추가된 경우 직접 실행하거나 전역 큐에 등록합니다.
// @param job 추가할 작업 객체 레퍼런스
// @param pushOnly 전역 큐 등록 여부 (true: 푸시만 수행, false: 상황에 따라 직접 실행 또는 전역 큐 연동)
//***************************************************************************
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

//***************************************************************************
// @brief 큐에 적재된 작업을 순차적으로 실행합니다.
// @detail 실행 시간 제한(QPC 기준) 내에서 큐에 담긴 작업들을 모두 처리하며, 시간이 초과되면 전역 큐에 재위임합니다.
//***************************************************************************
void CJobQueue::Execute()
{
	// 1. 현재 실행 중인 스레드의 TLS에 이 작업 큐의 컨텍스트를 등록합니다.
	LCurrentJobQueue = this;

	// 2. 현재 스레드의 연속 실행 시간 제한(슬롯)을 설정하기 위해,
	//    QPC(QueryPerformanceCounter)를 기준으로 만료 시각(LEndTickCount)을 계산합니다.
	LEndTickCount = QPCTimer::Now() + QPCTimer::MsToCount(EXECUTE_TIME_LIMIT_MS);

	while( true )
	{
		// 3. 스레드 안전한 큐에서 대기 중인 모든 작업을 한 번에 인출(PopAll)합니다.
		CVector<CJobRef> jobs;
		_jobs.PopAll(OUT jobs);

		// 4. 인출된 작업들의 개수를 확인하고 순회하며 각각의 작업을 실행합니다.
		const int32 jobCount = static_cast<int32>(jobs.size());
		for( int32 i = 0; i < jobCount; i++ )
			jobs[i]->Execute();

		// 5. 전체 작업 개수(_jobCount)에서 방금 처리한 작업 개수만큼 원자적으로 차감합니다.
		//    만약 차감 후 남은 작업 수가 0이라면 큐가 완전히 비어있는 상태이므로 처리를 종료하고 TLS를 해제합니다.
		if( _jobCount.fetch_sub(jobCount) == jobCount )
		{
			LCurrentJobQueue = nullptr;
			return;
		}

		// 6. 큐에 아직 작업이 남아있더라도, 지정된 실행 시간 제한(LEndTickCount)을 초과했다면
		//    다른 작업 큐의 공정한 실행 기회를 위해 현재 큐를 GlobalQueue에 재위임하고 루프를 탈출합니다.
		if( QPCTimer::Now() >= LEndTickCount )
		{
			LCurrentJobQueue = nullptr;
			if( gpGlobalQueue != nullptr ) gpGlobalQueue->Push(shared_from_this());
			break;
		}
	}
}