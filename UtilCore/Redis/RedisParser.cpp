
//***************************************************************************
// RedisParser.cpp: implementation of the CRedisParser class.
//
//***************************************************************************

#include "pch.h"
#include "RedisParser.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisParser 생성자
//***************************************************************************
CRedisParser::CRedisParser()
{
	Reset();
}

//***************************************************************************
// @brief CRedisParser 소멸자
//***************************************************************************
CRedisParser::~CRedisParser()
{
}

//***************************************************************************
// @brief 파서 내부 상태 및 미완성 버퍼를 초기화함
//***************************************************************************
void CRedisParser::Reset()
{
	_pendingBuffer.clear();
}

//***************************************************************************
// @brief 수신된 원시 바이트를 내부 누적 버퍼에 추가함
//***************************************************************************
void CRedisParser::Feed(const char* pBuffer, const int32 nBytes)
{
	if( pBuffer == nullptr || nBytes <= 0 ) return;
	_pendingBuffer.append(pBuffer, nBytes);
}

//***************************************************************************
// @brief 누적 버퍼에 이미 들어있는 데이터만으로 완전한 RESP 값 하나를 꺼냄
//***************************************************************************
bool CRedisParser::TryParse(RedisValue& outValue)
{
	if( _pendingBuffer.empty() ) return false;

	int32 nOffset = 0;
	if( !ParseValue(_pendingBuffer.data(), static_cast<int32>(_pendingBuffer.size()), nOffset, outValue) )
		return false;

	_pendingBuffer.erase(0, nOffset);
	return true;
}

//***************************************************************************
// @brief RESP 접두 타입 문자열에 따라 재귀적으로 값 구조를 파싱함
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nSize 버퍼의 전체 바이트 크기
// @param nOffset 읽기 오프셋 위치 (입출력)
// @param outValue 파싱 결과 객체 (출력)
// @return 성공 여부 (true: 성공, false: 실패 — 데이터 부족/프로토콜 오류 모두 포함)
//***************************************************************************
bool CRedisParser::ParseValue(const char* pBuffer, const int32 nSize, int32& nOffset, RedisValue& outValue)
{
	if( nOffset >= nSize ) return false;

	int32 nSavedOffset = nOffset;
	char cPrefix = pBuffer[nOffset++];

	std::string strLine;
	if( !ReadLine(pBuffer, nSize, nOffset, strLine) )
	{
		nOffset = nSavedOffset;
		return false;
	}

	try
	{
		switch( cPrefix )
		{
		case '+': // Simple String
		{
			outValue.eType = ERedisType::SimpleString;
			outValue.strVal = strLine;
			return true;
		}
		case '-': // Error
		{
			outValue.eType = ERedisType::Error;
			outValue.strVal = strLine;
			return true;
		}
		case ':': // Integer
		{
			outValue.eType = ERedisType::Integer;
			outValue.i64Val = std::stoll(strLine);
			return true;
		}
		case '$': // Bulk String
		{
			outValue.eType = ERedisType::BulkString;
			int64 i64Len = std::stoll(strLine);

			// 상한을 nSize로 먼저 못박는다 — 버퍼보다 큰 데이터는 애초에 이
			// 시점에 도착해있을 수 없으므로 이 자체가 안전한 검증이며,
			// 이렇게 i64Len을 int32 범위(nSize) 안으로 미리 묶어두면 바로
			// 아래의 "i64Len + 2" 덧셈이 int64 오버플로를 일으킬 수 없다.
			// (검증 없이 곧장 더했다면, 공격자가 INT64_MAX에 가까운 길이를
			// 선언했을 때 오버플로로 값이 음수로 뒤집혀 아래 크기 검사를
			// 무력화하고, 실제 버퍼보다 훨씬 큰 범위를 읽어버리는 힙
			// 오버리드로 이어질 수 있었다.)
			if( i64Len < -1 || i64Len > static_cast<int64>(nSize) )
			{
				nOffset = nSavedOffset;
				return false;
			}

			outValue.i64Val = i64Len;
			if( i64Len == -1 ) return true; // Null Bulk String

			if( nSize - nOffset < i64Len + 2 )
			{
				nOffset = nSavedOffset;
				return false;
			}

			outValue.strVal.assign(pBuffer + nOffset, static_cast<size_t>(i64Len));
			nOffset += static_cast<int32>(i64Len) + 2; // Data + \r\n
			return true;
		}
		case '*': // Array
		{
			outValue.eType = ERedisType::Array;
			int64 i64Count = std::stoll(strLine);

			// BulkString과 동일한 이유로 nSize를 상한으로 미리 못박는다.
			// 배열 요소 하나가 최소 4바이트(예: ":0\r\n")는 차지하므로 nSize
			// 자체가 이미 충분히 보수적인 상한이며, 그 덕에 바로 아래
			// reserve() 호출이 터무니없이 큰 값으로 시도되는 것도 막는다
			// (오버플로는 나지 않더라도 불필요하게 거대한 할당 시도로 인한
			// 지연/예외 처리 낭비를 예방).
			if( i64Count < -1 || i64Count > static_cast<int64>(nSize) )
			{
				nOffset = nSavedOffset;
				return false;
			}

			outValue.i64Val = i64Count;
			if( i64Count == -1 ) return true; // Null Array

			outValue.arrayVal.reserve(static_cast<size_t>(i64Count));
			for( int64 i = 0; i < i64Count; ++i )
			{
				RedisValue elem;
				if( !ParseValue(pBuffer, nSize, nOffset, elem) )
				{
					nOffset = nSavedOffset;
					return false;
				}
				outValue.arrayVal.push_back(elem);
			}
			return true;
		}
		default:
			break;
		}
	}
	catch( const std::exception& )
	{
		// strLine이 정수로 변환 불가능한 경우(프로토콜 손상) — 예외를 상위로
		// 전파하지 않고 파싱 실패로 처리한다.
		nOffset = nSavedOffset;
		return false;
	}

	nOffset = nSavedOffset;
	return false;
}

//***************************************************************************
// @brief \r\n 개행문자 기준으로 한 줄의 문자열을 읽음
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nSize 버퍼 전체 크기
// @param nOffset 읽기 오프셋 위치 (입출력)
// @param strLine 읽어온 라인 문자열 (출력)
// @return 읽기 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisParser::ReadLine(const char* pBuffer, const int32 nSize, int32& nOffset, std::string& strLine)
{
	for( int32 i = nOffset; i < nSize - 1; ++i )
	{
		if( pBuffer[i] == '\r' && pBuffer[i + 1] == '\n' )
		{
			strLine.assign(pBuffer + nOffset, i - nOffset);
			nOffset = i + 2;
			return true;
		}
	}
	return false;
}