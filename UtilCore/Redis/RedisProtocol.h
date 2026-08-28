
//***************************************************************************
// RedisProtocol.h : RESP(REdis Serialization Protocol) 데이터 타입 정의
//
//***************************************************************************

#ifndef UC_REDISPROTOCOL_H
#define UC_REDISPROTOCOL_H

#include <BaseRedefineDataType.h>

#include <string>
#include <vector>

//***************************************************************************
// @brief RESP 프로토콜의 데이터 타입을 나타내는 열거형
// @details Redis 서버와 통신할 때 사용하는 5가지 기본 RESP 데이터 타입을 정의합니다.
//***************************************************************************
enum class ERedisType
{
	Unknown,
	SimpleString,  // +
	Error,         // -
	Integer,       // :
	BulkString,    // $
	Array          // *
};

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

#endif // ndef UC_REDISPROTOCOL_H