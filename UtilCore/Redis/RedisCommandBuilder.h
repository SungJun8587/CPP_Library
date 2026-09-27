
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
//          [수정 — 성능 개선] HeaderLen()/WriteHeader()가 예전엔 길이를
//          "일단 실제로 포맷팅해보고 그 결과 길이를 재는" 방식으로
//          _snprintf_s()를 썼다 — 로케일 인식 포맷팅 머신을 순전히 자릿수를
//          세는 용도로 돌리는 셈이라 낭비였다. 게다가 명령 하나를 인코딩할
//          때 CalcEncodedSize()가 인자당 한 번, Encode()가 다시 인자당 한
//          번씩 이 함수들을 불러서, 인자 N개짜리 명령 하나에 _snprintf_s가
//          최대 2N번 호출됐다. 이 프로젝트의 HMGET(방 대화 기록 조회)은
//          한 번에 최대 1000개 인자(메시지 ID)를 보내므로, 그 경우
//          _snprintf_s 호출이 최대 2000번까지 쌓였다. 자릿수를 직접
//          계산/기록하는 정수 연산으로 바꿔서 이 오버헤드를 없앴다.
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
	// @details 로깅/디버깅 용도의 간편 버전이라 std::ostringstream을 그대로
	//          둔다 — 실제 송신 경로(CalcEncodedSize/Encode)는 이 함수를
	//          안 거친다.
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
	// @brief nValue의 10진수 자릿수를 셈 (0은 1자리로 취급)
	//***************************************************************************
	static uint32 CountDigits(size_t nValue)
	{
		uint32 nDigits = 1;
		while( nValue >= 10 )
		{
			nValue /= 10;
			++nDigits;
		}
		return nDigits;
	}

	//***************************************************************************
	// @brief "<cPrefix><nValue>\r\n" 형태 헤더 한 줄이 차지할 바이트 수를 계산함
	// @details [수정 — 성능 개선] _snprintf_s로 실제 포맷팅해서 길이를 재던
	//          방식을 자릿수 직접 계산으로 바꿨다 — WriteHeader()가 실제로
	//          쓰는 바이트 수와 정확히 일치해야 하며(자릿수 계산 방식이
	//          동일하므로 항상 일치한다), 이 값이 작게 나오면 Encode()가
	//          버퍼 밖을 쓰게 되므로 반드시 함께 맞춰 유지해야 한다.
	//***************************************************************************
	static uint32 HeaderLen(char cPrefix, size_t nValue)
	{
		(void)cPrefix; // 접두문자 1바이트는 길이에 고정으로 더해짐(자릿수와 무관)
		return 1 + CountDigits(nValue) + 2; // prefix(1) + 자릿수 + "\r\n"(2)
	}

	//***************************************************************************
	// @brief "<cPrefix><nValue>\r\n" 형태 헤더 한 줄을 pDest에 기록하고 기록한
	//        바이트 수를 반환함
	// @details [수정 — 성능 개선] _snprintf_s 대신 자릿수를 직접 계산해서
	//          역순으로 채운 뒤 정방향으로 복사한다 — HeaderLen()이 계산한
	//          길이와 정확히 같은 바이트 수를 쓴다.
	//***************************************************************************
	static size_t WriteHeader(char* pDest, char cPrefix, size_t nValue)
	{
		char* p = pDest;
		*p++ = cPrefix;

		// size_t 최대값(64비트 기준)도 20자리를 안 넘으므로 24바이트면 충분히 여유 있다.
		char digits[24];
		int nDigitCount = 0;

		if( nValue == 0 )
		{
			digits[nDigitCount++] = '0';
		}
		else
		{
			size_t nTemp = nValue;
			while( nTemp > 0 )
			{
				digits[nDigitCount++] = static_cast<char>('0' + (nTemp % 10));
				nTemp /= 10;
			}
		}

		// digits는 최하위 자릿수부터(역순으로) 채워졌으므로 뒤에서부터 복사한다.
		for( int i = nDigitCount - 1; i >= 0; --i )
			*p++ = digits[i];

		*p++ = '\r';
		*p++ = '\n';

		return static_cast<size_t>(p - pDest);
	}
};

#endif // ndef UC_REDISCOMMANDBUILDER_H