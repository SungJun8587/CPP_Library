
//***************************************************************************
// RedisRetry.cpp : implementation of SendCommandWithRetry.
//
//***************************************************************************

#include "pch.h"
#include <Redis/RedisHelper.h>
#include "RedisRetry.h"

namespace
{
	// 재시도 간에 공유되는 불변 상태. 콜백이 shared_ptr로 붙들 뿐 자기 자신을 가리키지 않아
	// 순환 참조가 없고, 마지막 콜백이 끝나면 해제된다.
	struct SRetryState
	{
		CRedisService* pRedisService = nullptr;
		CJobQueueRef			pJobQueue;
		RedisScheduleDelayedFn	fnScheduleDelayed;
		CVector<std::string>	vecArgs;
		int32					nRetryDelayMs = 0;
		RedisCallback			fnOnResult;
	};

	void AttemptSend(const std::shared_ptr<SRetryState>& pState, const int32 nRetryLeft)
	{
		const bool bQueued = pState->pRedisService->SendCommand(pState->vecArgs, [pState, nRetryLeft](const RedisValue& res)
			{
				if( nRetryLeft > 0 && IsRedisConnectionError(res) )
				{
					LOG_ERROR(_T("SendCommandWithRetry: 일시적 연결 오류로 재시도 예약 (남은재시도=%d, 지연=%dms, err=%hs)"),
						nRetryLeft, pState->nRetryDelayMs, res.strVal.c_str());

					if( pState->pJobQueue && pState->fnScheduleDelayed && pState->nRetryDelayMs > 0 )
					{
						pState->fnScheduleDelayed(pState->nRetryDelayMs, [pState, nRetryLeft]()
							{
								pState->pJobQueue->DoAsync([pState, nRetryLeft]()
									{
										AttemptSend(pState, nRetryLeft - 1);
									});
							});
					}
					else
					{
						AttemptSend(pState, nRetryLeft - 1);
					}
					return;
				}

				if( pState->fnOnResult )
					pState->fnOnResult(res);
			});

		// 큐 적재 실패(풀 없음/대기 큐 포화) 시 콜백이 오지 않으므로 에러 응답으로 마무리한다.
		if( !bQueued && pState->fnOnResult )
		{
			pState->fnOnResult(Redis::MakeError(Redis::kErrSendRejected));
		}
	}
}

void SendCommandWithRetry(CRedisService* pRedisService, CJobQueueRef pJobQueue, RedisScheduleDelayedFn fnScheduleDelayed, const CVector<std::string>& vecArgs, const int32 nMaxRetry, const int32 nRetryDelayMs, RedisCallback fnOnResult)
{
	auto pState = std::make_shared<SRetryState>();
	pState->pRedisService = pRedisService;
	pState->pJobQueue = std::move(pJobQueue);
	pState->fnScheduleDelayed = std::move(fnScheduleDelayed);
	pState->vecArgs = vecArgs;
	pState->nRetryDelayMs = nRetryDelayMs;
	pState->fnOnResult = std::move(fnOnResult);
	AttemptSend(pState, nMaxRetry);
}