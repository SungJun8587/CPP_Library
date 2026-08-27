
//***************************************************************************
// RedisCommandBuilder.h : Redis Protocol Command Encoding Helper
//
//***************************************************************************

#ifndef __REDISCOMMANDBUILDER_H__
#define __REDISCOMMANDBUILDER_H__

#include <string>
#include <vector>
#include <sstream>

//***************************************************************************
// @brief Redis 인자 리스트를 RESP 프로토콜 규격 커맨드로 변환하는 정적 유틸리티 클래스
// @details 입력받은 문자열 벡터를 RESP 프로토콜의 Array 타입 형태(`*N\r\n$L\r\n...`)로 인코딩합니다.
//
// @code
// // 사용 예시:
// std::string req = CRedisCommandBuilder::Build({"SET", "Player:100", "Data"});
// // 결과: "*3\r\n$3\r\nSET\r\n$10\r\nPlayer:100\r\n$4\r\nData\r\n"
// @endcode
//***************************************************************************
class CRedisCommandBuilder
{
public:
	//***************************************************************************
	// @brief 명령어 인자 리스트를 RESP Format 문자열로 변환함
	// @param vecArgs Redis 명령어 및 인자 리스트
	// @return RESP 규격으로 직렬화된 문자열
	//***************************************************************************
	static std::string Build(const CVector<std::string>& vecArgs)
	{
		std::ostringstream oss;
		oss << "*" << vecArgs.size() << "\r\n";
		for( const auto& strArg : vecArgs )
		{
			oss << "$" << strArg.size() << "\r\n" << strArg << "\r\n";
		}
		return oss.str();
	}
};

#endif // ndef __REDISCOMMANDBUILDER_H__