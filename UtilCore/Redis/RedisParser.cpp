
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
// @brief 수신된 바이트 스트림에서 완전한 RESP 응답 단위를 파싱함
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nBytes 수신된 바이트 크기
// @param nConsumedBytes 파싱에 사용된 바이트 수 (출력)
// @param outValue 파싱된 결과 객체 (출력)
// @return 파싱 성공 여부 (true: 성공, false: 데이터 부족 또는 파싱 실패)
//***************************************************************************
bool CRedisParser::Parse(const char* pBuffer, const int32 nBytes, int32& nConsumedBytes, RedisValue& outValue)
{
	nConsumedBytes = 0;
	if( pBuffer == nullptr || nBytes <= 0 ) return false;

	_pendingBuffer.append(pBuffer, nBytes);

	int32 nOffset = 0;
	bool bSuccess = ParseValue(_pendingBuffer.data(), static_cast<int32>(_pendingBuffer.size()), nOffset, outValue);

	if( bSuccess )
	{
		nConsumedBytes = nOffset;
		_pendingBuffer.erase(0, nConsumedBytes);
		return true;
	}

	return false;
}

//***************************************************************************
// @brief RESP 접두 타입 문자열에 따라 재귀적으로 값 구조를 파싱함
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nSize 버퍼의 전체 바이트 크기
// @param nOffset 읽기 오프셋 위치 (입출력)
// @param outValue 파싱 결과 객체 (출력)
// @return 성공 여부 (true: 성공, false: 실패)
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