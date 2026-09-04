
//***************************************************************************
// RedisParser.h : interface for the CRedisParser class.
//
//***************************************************************************

#ifndef UC_REDISPARSER_H
#define UC_REDISPARSER_H

#include <Redis/RedisProtocol.h>

//***************************************************************************
// @brief RESP 프로토콜 스트림 파싱을 담당하는 클래스
// @details 소켓으로부터 수신된 원시 바이트를 Feed()로 누적하고, TryParse()를
//          반복 호출하여 누적된 버퍼 안에 완성되어 있는 RESP 값들을 순서대로
//          꺼내온다. 데이터가 아직 부족해 값 하나를 완성할 수 없으면 TryParse()는
//          false를 반환하며 버퍼 상태를 그대로 보존하므로, 다음 Feed() 이후
//          다시 호출하면 이어서 파싱을 재개한다. 미완성 데이터의 보관 책임은
//          이 클래스(_pendingBuffer) 하나로 일원화되어 있으며, 호출자는 수신한
//          바이트를 그대로 Feed()에 전달하고 그 바이트에 대한 소유권(수신 버퍼
//          커서 이동 등)을 즉시 넘기면 된다.
//
// @code
// // 사용 예시:
// CRedisParser parser;
// parser.Feed(pRecvBuffer, nRecvBytes);
//
// RedisValue result;
// while (parser.TryParse(result))
// {
//     // 완성된 값 하나 처리
// }
// @endcode
//***************************************************************************
class CRedisParser
{
public:
	CRedisParser();
	~CRedisParser();

	//***************************************************************************
	// @brief 파서 내부 상태 및 미완성 버퍼를 초기화함
	//***************************************************************************
	void        Reset();

	//***************************************************************************
	// @brief 수신된 원시 바이트를 내부 누적 버퍼에 추가함
	// @param pBuffer 수신 데이터 버퍼 포인터
	// @param nBytes 수신된 바이트 크기
	//***************************************************************************
	void        Feed(const char* pBuffer, const int32 nBytes);

	//***************************************************************************
	// @brief 누적 버퍼에 이미 들어있는 데이터만으로 완전한 RESP 값 하나를 꺼냄
	// @param outValue 파싱된 결과 객체 (출력)
	// @return 완성된 값을 꺼냈으면 true, 데이터가 부족하거나 프로토콜 오류면 false
	//         (버퍼 상태는 실패 시에도 보존되어 다음 Feed() 이후 이어서 재시도 가능)
	//***************************************************************************
	bool        TryParse(RedisValue& outValue);

private:
	bool        ParseValue(const char* pBuffer, const int32 nSize, int32& nOffset, RedisValue& outValue);
	bool        ReadLine(const char* pBuffer, const int32 nSize, int32& nOffset, std::string& strLine);

private:
	std::string _pendingBuffer;		// 미완성 패킷 버퍼링 (누적 버퍼의 유일한 소유자)
};

#endif // ndef UC_REDISPARSER_H