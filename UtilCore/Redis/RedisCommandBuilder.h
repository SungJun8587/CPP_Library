
//***************************************************************************
// RedisCommandBuilder.h : Redis Protocol Command Encoding Helper
//
//***************************************************************************

#ifndef UC_REDISCOMMANDBUILDER_H
#define UC_REDISCOMMANDBUILDER_H

#include <string>
#include <vector>
#include <sstream>
#include <cstdio>

//***************************************************************************
// @brief Redis 인자 리스트를 RESP 프로토콜 규격 커맨드로 변환하는 정적 유틸리티 클래스
// @details 입력받은 문자열 벡터를 RESP 프로토콜의 Array 타입 형태(`*N\r\n$L\r\n...`)로 인코딩합니다.
//          Build()는 std::string 결과물이 필요한 곳(로깅, 디버깅 등)에 쓰는
//          간편한 버전이고, CalcEncodedSize()/Encode()는 송신 버퍼에 직접
//          써넣어 중간 std::string 사본을 만들지 않는 경로에 사용합니다.
//
// @code
// // 간편 버전:
// std::string req = CRedisCommandBuilder::Build({"SET", "Player:100", "Data"});
// // 결과: "*3\r\n$3\r\nSET\r\n$10\r\nPlayer:100\r\n$4\r\nData\r\n"
//
// // 버퍼 직접 기록 버전:
// uint32 nSize = CRedisCommandBuilder::CalcEncodedSize(vecArgs);
// CSendBufferRef sendBuffer = CSendBufferManager::Open(nSize);
// CRedisCommandBuilder::Encode(vecArgs, reinterpret_cast<char*>(sendBuffer->Buffer()));
// sendBuffer->Close(nSize);
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

	//***************************************************************************
	// @brief Encode()가 필요로 하는 인코딩 결과의 정확한 바이트 크기를 계산함
	// @param vecArgs Redis 명령어 및 인자 리스트
	// @return 인코딩 결과 바이트 크기
	//***************************************************************************
	static uint32 CalcEncodedSize(const CVector<std::string>& vecArgs)
	{
		uint32 nSize = HeaderLen('*', vecArgs.size());
		for( const auto& strArg : vecArgs )
		{
			nSize += HeaderLen('$', strArg.size());
			nSize += static_cast<uint32>(strArg.size()) + 2; // 데이터 + \r\n
		}
		return nSize;
	}

	//***************************************************************************
	// @brief 명령어 인자 리스트를 RESP 규격으로 직접 pDest 버퍼에 기록함
	// @param vecArgs Redis 명령어 및 인자 리스트
	// @param pDest CalcEncodedSize(vecArgs) 이상의 여유 공간을 가진 대상 버퍼
	//***************************************************************************
	static void Encode(const CVector<std::string>& vecArgs, char* pDest)
	{
		char* p = pDest;
		p += WriteHeader(p, '*', vecArgs.size());

		for( const auto& strArg : vecArgs )
		{
			p += WriteHeader(p, '$', strArg.size());
			::memcpy(p, strArg.data(), strArg.size());
			p += strArg.size();
			*p++ = '\r';
			*p++ = '\n';
		}
	}

private:
	//***************************************************************************
	// @brief "<cPrefix><nValue>\r\n" 형태 헤더 한 줄이 차지할 바이트 수를 계산함
	//***************************************************************************
	static uint32 HeaderLen(char cPrefix, size_t nValue)
	{
		char szTemp[32];
		int nLen = ::_snprintf_s(szTemp, sizeof(szTemp), _TRUNCATE, "%c%zu\r\n", cPrefix, nValue);
		return static_cast<uint32>(nLen);
	}

	//***************************************************************************
	// @brief "<cPrefix><nValue>\r\n" 형태 헤더 한 줄을 pDest에 기록하고 기록한
	//        바이트 수를 반환함
	//***************************************************************************
	static size_t WriteHeader(char* pDest, char cPrefix, size_t nValue)
	{
		char szTemp[32];
		int nLen = ::_snprintf_s(szTemp, sizeof(szTemp), _TRUNCATE, "%c%zu\r\n", cPrefix, nValue);
		::memcpy(pDest, szTemp, nLen);
		return static_cast<size_t>(nLen);
	}
};

#endif // ndef UC_REDISCOMMANDBUILDER_H