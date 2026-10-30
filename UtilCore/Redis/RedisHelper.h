
//***************************************************************************
// RedisHelper.h: Redis 모듈 안팎에서 반복되던 작은 공용 함수 모음.
//
//***************************************************************************

#ifndef UC_REDISHELPER_H
#define UC_REDISHELPER_H

#include <BaseRedefineDataType.h>
#include <Redis/RedisDefine.h>
#include <Redis/RedisProtocol.h>

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Redis
{
	//***************************************************************************
	// @brief 클라이언트/풀이 직접 만드는 에러 응답(ERedisType::Error)을 생성함
	// @param pszMessage 에러 문자열(RedisDefine.h의 kErr... 상수 등)
	// @details 서버가 돌려준 에러가 아니라 연결/전송 문제를 콜백에 알리는 값이다.
	//          IsRedisConnectionError()가 접두어로 이 값을 구분한다.
	//***************************************************************************
	inline RedisValue MakeError(const char* pszMessage)
	{
		RedisValue err;
		err.eType = ERedisType::Error;
		err.strVal = pszMessage;
		return err;
	}

	//***************************************************************************
	// @brief Redis 명령 인자 목록을 한 줄로 만듦
	// @code
	// service.SendCommand(Redis::MakeArgs({ "SADD", strKey, strMember }), cb);
	// @endcode
	//***************************************************************************
	inline CVector<std::string> MakeArgs(std::initializer_list<std::string> args)
	{
		return CVector<std::string>(args.begin(), args.end());
	}

	//***************************************************************************
	// @brief MakeArgs의 가변 인자 버전 — 인자를 완벽 전달해 문자열을 한 번만 만든다.
	// @details initializer_list 버전은 원소가 const라 인자마다 문자열 복사가 한 번 더 생긴다.
	//          본문이 큰 인자(채팅 메시지 등)를 넘기는 경로에서는 이 버전에 std::move로 넘긴다.
	// @code
	// service.SendCommand(Redis::MakeArgs("RPUSH", strKey, std::move(strBody)), cb);
	// @endcode
	//***************************************************************************
	template<typename... TArgs>
		requires (sizeof...(TArgs) > 0 && (std::is_constructible_v<std::string, TArgs&&> && ...))
	inline CVector<std::string> MakeArgs(TArgs&&... args)
	{
		CVector<std::string> vecArgs;
		vecArgs.reserve(sizeof...(TArgs));
		(vecArgs.emplace_back(std::forward<TArgs>(args)), ...);
		return vecArgs;
	}

	//***************************************************************************
	// @brief 문자열 전체가 10진 정수일 때만 outValue에 변환해 돌려줌 (예외 없음)
	// @param sv 변환할 문자열. 공백·부호 접두(+)·뒤따르는 문자가 있으면 실패
	// @param outValue T 범위를 벗어나거나 변환에 실패하면 건드리지 않는다
	// @return 성공 여부. 부호 없는 형식에 음수 문자열이 오면 실패한다
	// @details std::stoi/stoul은 "12abc"를 12로 받고 실패를 예외로 알린다. 파서의 길이 헤더와
	//          결과 변환(RedisResultSet)이 이 함수 하나를 공유한다.
	//***************************************************************************
	template<typename T>
	inline bool ParseInteger(const std::string_view sv, T& outValue)
	{
		static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "ParseInteger: 정수형만 지원");

		using TWide = std::conditional_t<std::is_signed_v<T>, int64, uint64>;
		TWide wide = 0;

		const char* const pBegin = sv.data();
		const char* const pEnd = pBegin + sv.size();
		const auto result = std::from_chars(pBegin, pEnd, wide, 10);
		if( result.ec != std::errc() || result.ptr != pEnd )
			return false;

		if( !std::in_range<T>(wide) )
			return false;

		outValue = static_cast<T>(wide);
		return true;
	}

	//***************************************************************************
	// @brief 재연결 실패 횟수에 따른 지수 백오프 대기 시간(ms)을 계산함
	// @param nFailCount 연속 실패 횟수(1부터). 1차 실패=kReconnectBackoffBaseMs, 이후 두 배씩 늘며 상한까지
	// @param nJitterMs 0이면 지터 없음. 양수면 0~nJitterMs를 무작위로 더함(동시 재시도 충돌 방지)
	// @return 지터를 더한 뒤에도 kReconnectBackoffMaxMs를 넘지 않는 대기 시간
	//***************************************************************************
	inline int64 ComputeReconnectBackoffMs(const int32 nFailCount, const int32 nJitterMs = 0)
	{
		const int32 nShift = (std::max)(0, (std::min)(nFailCount - 1, kReconnectBackoffMaxShift));
		int64 nDelayMs = (std::min<int64>)(kReconnectBackoffBaseMs << nShift, kReconnectBackoffMaxMs);

		if( nJitterMs > 0 )
		{
			thread_local std::mt19937 rng(std::random_device{}());
			std::uniform_int_distribution<int32> jitterDist(0, nJitterMs);
			nDelayMs = (std::min<int64>)(nDelayMs + jitterDist(rng), kReconnectBackoffMaxMs);
		}

		return nDelayMs;
	}

	//***************************************************************************
	// @brief 재연결 실패 횟수와 다음 재시도 시각을 묶은 지수 백오프 상태 (풀과 구독 연결 공용)
	// @details 시각은 ptime::MonotonicNowMillis() 기준(ms)이다. 스레드 안전하지 않다 —
	//          재연결 스레드 하나만 만진다.
	//***************************************************************************
	struct SReconnectBackoff
	{
		int32	nFailCount = 0;		// 연속 재연결 실패 횟수
		int64	nNextRetryMs = 0;	// 이 시각 이전에는 재시도를 건너뜀

		bool ShouldSkip(const int64 nNowMs) const { return nFailCount > 0 && nNowMs < nNextRetryMs; }

		void OnFail(const int64 nNowMs, const int32 nJitterMs = 0)
		{
			++nFailCount;
			nNextRetryMs = nNowMs + ComputeReconnectBackoffMs(nFailCount, nJitterMs);
		}

		void Reset()
		{
			nFailCount = 0;
			nNextRetryMs = 0;
		}
	};
}

#endif // ndef UC_REDISHELPER_H