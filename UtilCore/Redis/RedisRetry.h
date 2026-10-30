
//***************************************************************************
// RedisRetry.h : SendCommandWithRetry — 연결 오류에 한해 재시도하는 공용 헬퍼
//
//***************************************************************************

#ifndef UC_REDISRETRY_H
#define UC_REDISRETRY_H

#include <Redis/RedisService.h>
#include <Job/JobQueue.h>

#include <functional>

//***************************************************************************
// @brief 지연 실행 예약 함수 형식(지연 밀리초, 실행할 작업)
//***************************************************************************
using RedisScheduleDelayedFn = std::function<void(int32, std::function<void()>)>;

//***************************************************************************
// @brief Redis 명령을 보내고, 응답이 "연결 오류"(IsRedisConnectionError)이면 지정한
//        횟수만큼 지연을 두고 재시도한다.
// @details 커넥션 풀은 끊어진 커넥션을 백그라운드에서 재연결하지만, 끊기는 순간 그
//          커넥션에 이미 올라타 있던 요청은 재연결을 기다리지 않고 에러로 끝난다. 이런
//          일시적 연결 오류를 호출부가 빈 결과로 오인하지 않게 이 헬퍼가 흡수한다.
//          WRONGTYPE/NOSCRIPT/OOM 같은 서버가 돌려준 에러와 정상적인 빈 결과는 재시도하지
//          않는다(재시도해도 같은 결과이거나, 이미 반영된 명령이 중복될 수 있다).
//
// @details 지연은 jobQueue->DoTimer()가 아니라 호출자가 넘기는 scheduleDelayed(별도의
//          지연 큐/유지보수 스레드)에 예약하고, 기한이 되면 jobQueue->DoAsync()로 되돌아와
//          재시도한다. DoTimer()는 CJobTimer::Distribute()를 주기적으로 돌리는 루프가
//          있어야만 동작하기 때문이다. scheduleDelayed나 jobQueue가 없거나 nRetryDelayMs가
//          0 이하이면 지연 없이 즉시 재시도한다.
//
// @details 서비스 수명: 재시도 상태가 pRedisService를 원시 포인터로 쥐고 있고, 지연 재시도는
//          scheduleDelayed(지연 큐)에 남아 있을 수 있다. 서비스를 해제하기 전에 지연 큐를 비우거나
//          멈춰, 해제된 서비스로 재시도가 나가지 않게 해야 한다.
//
// @details 멱등하지 않은 명령(INCR, LPUSH 등)을 재시도하면 서버가 이미 처리한 뒤 응답만
//          유실된 경우 중복 반영될 수 있다. 멱등한 명령이나 스크립트에만 쓴다.
//
// @param nMaxRetry 최대 재시도 횟수(0이면 한 번만 시도)
// @param onResult 최종 결과 콜백 — 재시도를 다 써도 연결 오류면 마지막 에러 값이 전달된다.
//                 큐 적재 실패(풀 없음/대기 큐 포화)도 에러 값("SendCommand rejected")으로 전달된다.
//***************************************************************************
void SendCommandWithRetry(CRedisService* pRedisService, CJobQueueRef pJobQueue, RedisScheduleDelayedFn fnScheduleDelayed, const CVector<std::string>& vecArgs, const int32 nMaxRetry, const int32 nRetryDelayMs, RedisCallback fnOnResult);

#endif // ndef UC_REDISRETRY_H