
//***************************************************************************
// RedisProtocol.h : RESP(REdis Serialization Protocol) 데이터 타입 정의
//
//***************************************************************************

#ifndef UC_REDISPROTOCOL_H
#define UC_REDISPROTOCOL_H

#include <BaseRedefineDataType.h>
#include <Redis/RedisDefine.h>

#include <cstring>
#include <functional>
#include <string>
#include <vector>

//***************************************************************************
// @brief Redis 응답 데이터를 표현하는 재귀적 데이터 구조체
// @details SimpleString, Error, Integer, BulkString, Array 표현을 모두 수용합니다.
//          Array 타입일 경우 arrayVal 멤버에 하위 RedisValue 개체들이 재귀적으로 저장됩니다.
//
// @code
// // 사용 예시:
// if (res.eType == ERedisType::SimpleString) {
//     std::cout << "OK: " << res.strVal << std::endl;
// } else if (res.eType == ERedisType::Array) {
//     for (const auto& elem : res.arrayVal) {
//         std::cout << "Elem: " << elem.strVal << std::endl;
//     }
// }
// @endcode
//***************************************************************************
struct RedisValue
{
	ERedisType              eType = ERedisType::Unknown; // RESP 타입
	int64                   i64Val = 0;                  // 정수 값 또는 데이터/배열 길이
	std::string             strVal;                      // 문자열 데이터
	CVector<RedisValue>		arrayVal;                    // 배열 타입일 때의 하위 요소 목록

	//***************************************************************************
	// @brief 응답 데이터가 Null(Null Bulk String 또는 Null Array)인지 확인함
	// @return Null 여부 (true: Null, false: Valid)
	//***************************************************************************
	bool IsNull() const
	{
		return (eType == ERedisType::BulkString && i64Val == -1) ||
			(eType == ERedisType::Array && i64Val == -1);
	}
};

using RedisCallback = std::function<void(const RedisValue&)>;

// 클라이언트/풀 내부 경로용 응답 핸들러. 응답을 rvalue로 넘겨, 마지막 소비자(서비스가 JobQueue로 넘기는 지점)가
// 복사 없이 move할 수 있게 한다. const RedisValue&를 받는 람다도 그대로 대입된다(rvalue는 const&에 묶임).
using RedisResponseHandler = std::function<void(RedisValue&&)>;

//***************************************************************************
// @brief 응답이 "연결 문제"로 만들어진 에러인지 판별함
// @details CRedisClient는 연결 끊김/전송 실패/프로토콜 오류를 알리려고 직접 만든
//          ERedisType::Error 값을 콜백에 넘긴다("ERR connection closed ..." 등).
//          Redis 서버가 돌려준 에러(WRONGTYPE, NOSCRIPT, OOM 등)와 구분해, 재시도처럼
//          연결 문제에만 의미 있는 처리에 쓴다.
//***************************************************************************
inline bool IsRedisConnectionError(const RedisValue& value)
{
	if( value.eType != ERedisType::Error )
		return false;

	static const char* const kPrefixes[] = {
		Redis::kErrConnectionClosed,
		Redis::kErrSendFailed,
		Redis::kErrRecvRegistrationFailed,
		Redis::kErrProtocolError,
	};

	for( const char* pszPrefix : kPrefixes )
	{
		if( value.strVal.compare(0, std::strlen(pszPrefix), pszPrefix) == 0 )
			return true;
	}

	return false;
}

//***************************************************************************
// @brief 구독(Pub/Sub) 모드 연결이 서버로부터 받은 push 메시지를 전달받는 콜백
// @details value는 ["message", channel, payload] 또는
//          ["pmessage", pattern, channel, payload] 형태의 Array다. rvalue로 넘겨, 콜백이
//          채널/본문 문자열을 복사 없이 move할 수 있다.
//***************************************************************************
using RedisPushCallback = std::function<void(RedisValue&&)>;

//***************************************************************************
// @brief 구독한 채널(또는 패턴)에 메시지가 도착했을 때 호출되는 콜백
// @param strChannel 메시지가 발행된 실제 채널 이름(패턴 구독이어도 실제 채널)
// @param strPayload 발행된 메시지 본문
//***************************************************************************
using RedisMessageCallback = std::function<void(const std::string& strChannel, const std::string& strPayload)>;

#endif // ndef UC_REDISPROTOCOL_H