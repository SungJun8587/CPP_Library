
//***************************************************************************
// RedisParser.h : interface for the CRedisParser class.
//
//***************************************************************************

#ifndef __REDISPARSER_H__
#define __REDISPARSER_H__

#ifndef __REDISPROTOCOL_H__
#include <Redis/RedisProtocol.h>
#endif

//***************************************************************************
// @brief RESP 프로토콜 스트림 파싱을 담당하는 클래스
// @details 네트워크로부터 수신된 단편화(Chunked) 데이터를 내부 버퍼에 누적하며,
//          완전한 RESP 패킷이 조립될 때까지 상태를 유지하며 재귀적으로 파싱합니다.
//
// @code
// // 사용 예시:
// CRedisParser parser;
// int32 nConsumedBytes = 0;
// RedisValue result;
//
// if (parser.Parse(pBuffer, nRecvBytes, nConsumedBytes, result)) {
//     // 파싱 성공 처리
// }
// @endcode
//***************************************************************************
class CRedisParser
{
public:
	CRedisParser();
	~CRedisParser();

	void        Reset();
	bool        Parse(const char* pBuffer, const int32 nBytes, int32& nConsumedBytes, RedisValue& outValue);

private:
	bool        ParseValue(const char* pBuffer, const int32 nSize, int32& nOffset, RedisValue& outValue);
	bool        ReadLine(const char* pBuffer, const int32 nSize, int32& nOffset, std::string& strLine);

private:
	std::string _pendingBuffer;		// 미완성 패킷 버퍼링
};

#endif // ndef __REDISPARSER_H__