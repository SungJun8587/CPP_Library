
//***************************************************************************
// RedisParser.cpp: implementation of the CRedisParser class.
//
//***************************************************************************

#include "pch.h"
#include "RedisParser.h"

namespace
{
	//***************************************************************************
	// @brief [추가 — 버그 수정] 문자열 전체가 정확히 정수인지 확인하며 변환함.
	// @details std::stoll()은 문자열 앞부분만 숫자면 나머지를 조용히 무시하고
	//          성공 처리한다(예: "123abc" -> 123, 남은 "abc"는 무시됨) — RESP
	//          헤더 라인이 이런 식으로 손상된 경우에도 잘못된 값을 정상
	//          파싱된 것처럼 받아들이는 문제가 있었다(외부 리뷰로 발견).
	//          std::stoll의 pos 출력 인자로 실제 소비한 문자 수를 확인해서,
	//          문자열 전체를 다 안 썼으면 실패로 처리한다.
	//***************************************************************************
	bool ParseInt64Strict(const std::string& str, int64& outValue)
	{
		if( str.empty() )
			return false;

		try
		{
			size_t nPos = 0;
			long long value = std::stoll(str, &nPos, 10);
			if( nPos != str.size() )
				return false;

			outValue = static_cast<int64>(value);
			return true;
		}
		catch( const std::exception& )
		{
			return false;
		}
	}
}

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
// @details [추가 — 방어 강화] 누적 크기가 kMaxPendingBufferSize를 넘기면
//          append하지 않고 false를 반환한다 — 정상적인 Redis 응답이라면
//          절대 이 크기를 넘지 않아야 하므로, 넘었다는 것 자체가 스트림이
//          이미 신뢰할 수 없다는 신호다.
//***************************************************************************
bool CRedisParser::Feed(const char* pBuffer, const int32 nBytes)
{
	if( pBuffer == nullptr || nBytes <= 0 ) return true;

	const size_t nBytesSize = static_cast<size_t>(nBytes);
	if( nBytesSize > kMaxPendingBufferSize - _pendingBuffer.size() )
		return false;

	_pendingBuffer.append(pBuffer, nBytesSize);
	return true;
}

//***************************************************************************
// @brief 누적 버퍼에 이미 들어있는 데이터만으로 완전한 RESP 값 하나를 꺼냄
// @details [수정 — 방어 강화] ParseValue()가 배열을 파싱하다가 중간에
//          실패할 수 있는데, 예전엔 호출부가 넘긴 outValue에 곧바로 쓰고
//          있어서 그 실패한 시도의 일부 결과(예: 5개 중 2개만 채워진
//          arrayVal)가 outValue에 남을 수 있었다. 지금 이 클래스를 쓰는
//          CRedisClient::ProcessRecv()는 실패 시 그 outValue를 그냥
//          버리는 방식이라 실제 사고로 이어지진 않지만, 이 함수 자체의
//          계약을 명확히 하기 위해 임시 객체에 완전히 파싱한 뒤 성공했을
//          때만 outValue로 옮긴다 — 이러면 호출 패턴이 달라져도 안전하다.
//***************************************************************************
ERedisParseResult CRedisParser::TryParse(RedisValue& outValue)
{
	if( _pendingBuffer.empty() ) return ERedisParseResult::NeedMoreData;

	RedisValue parsedValue;
	int32 nOffset = 0;

	const ERedisParseResult result = ParseValue(_pendingBuffer.data(), static_cast<int32>(_pendingBuffer.size()), nOffset, parsedValue, 0);
	if( result != ERedisParseResult::Success )
		return result;

	_pendingBuffer.erase(0, static_cast<size_t>(nOffset));
	outValue = std::move(parsedValue);
	return ERedisParseResult::Success;
}

//***************************************************************************
// @brief RESP 접두 타입 문자열에 따라 재귀적으로 값 구조를 파싱함
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nSize 버퍼의 전체 바이트 크기
// @param nOffset 읽기 오프셋 위치 (입출력)
// @param outValue 파싱 결과 객체 (출력) — TryParse()가 넘기는 임시 객체이므로
//        이 함수 안에서는 실패해도 그대로 둬도 안전하다(호출부가 폐기함).
// @param nDepth 지금까지의 중첩 깊이 — kMaxParseDepth 초과 시 ProtocolError.
// @return Success/NeedMoreData/ProtocolError
//***************************************************************************
ERedisParseResult CRedisParser::ParseValue(const char* pBuffer, const int32 nSize, int32& nOffset, RedisValue& outValue, const int32 nDepth)
{
	if( nOffset >= nSize ) return ERedisParseResult::NeedMoreData;

	// [추가 — 방어 강화] 중첩 배열의 재귀 깊이 제한 — 스택 오버플로 방지.
	if( nDepth > kMaxParseDepth )
		return ERedisParseResult::ProtocolError;

	int32 nSavedOffset = nOffset;
	char cPrefix = pBuffer[nOffset++];

	std::string strLine;
	const ERedisParseResult lineResult = ReadLine(pBuffer, nSize, nOffset, strLine);
	if( lineResult != ERedisParseResult::Success )
	{
		nOffset = nSavedOffset;
		return lineResult;
	}

	try
	{
		switch( cPrefix )
		{
		case '+': // Simple String
		{
			outValue.eType = ERedisType::SimpleString;
			outValue.strVal = strLine;
			return ERedisParseResult::Success;
		}
		case '-': // Error
		{
			outValue.eType = ERedisType::Error;
			outValue.strVal = strLine;
			return ERedisParseResult::Success;
		}
		case ':': // Integer
		{
			int64 i64Value = 0;
			if( !ParseInt64Strict(strLine, i64Value) )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			outValue.eType = ERedisType::Integer;
			outValue.i64Val = i64Value;
			return ERedisParseResult::Success;
		}
		case '$': // Bulk String
		{
			int64 i64Len = 0;
			if( !ParseInt64Strict(strLine, i64Len) )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			// [수정 — 방어 강화] 예전엔 nSize(현재 버퍼 크기)를 상한으로
			// 썼는데, 그 nSize 자체가 kMaxPendingBufferSize까지 커질 수
			// 있어서 사실상 상한 역할을 못 했다. kMaxPendingBufferSize를
			// 직접 상한으로 쓴다 — 정상적인 Redis 응답이라면 이보다 큰
			// 단일 Bulk String을 보낼 일이 없다.
			if( i64Len < -1 || i64Len > static_cast<int64>(kMaxPendingBufferSize) )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			outValue.eType = ERedisType::BulkString;
			outValue.i64Val = i64Len;
			if( i64Len == -1 ) return ERedisParseResult::Success; // Null Bulk String

			if( nSize - nOffset < i64Len + 2 )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::NeedMoreData;
			}

			outValue.strVal.assign(pBuffer + nOffset, static_cast<size_t>(i64Len));
			nOffset += static_cast<int32>(i64Len);

			// [추가 — 방어 강화] Bulk String 데이터 뒤에 실제로 CRLF가
			// 오는지 확인한다 — 예전엔 길이만 믿고 그냥 +2를 했는데,
			// 그 자리가 진짜 "\r\n"이 아니면 이후 스트림 전체가 밀려서
			// 깨지는데도 아무 에러 없이 계속 진행했다.
			if( pBuffer[nOffset] != '\r' || pBuffer[nOffset + 1] != '\n' )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}
			nOffset += 2;
			return ERedisParseResult::Success;
		}
		case '*': // Array
		{
			int64 i64Count = 0;
			if( !ParseInt64Strict(strLine, i64Count) )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			if( i64Count < -1 )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			outValue.eType = ERedisType::Array;
			outValue.i64Val = i64Count;
			if( i64Count == -1 ) return ERedisParseResult::Success; // Null Array

			// [추가 — 방어 강화] 배열 원소 개수 자체를 고정 상한으로
			// 제한한다 — 예전엔 nSize(가변적이고 최대 16MB까지 커질 수
			// 있는 값)로만 제한해서, 그 안에서는 매우 큰 개수를 선언할 수
			// 있었다(예: 4바이트짜리 최소 원소로 채운다 해도 수백만 개).
			if( i64Count > kMaxArrayElements )
			{
				nOffset = nSavedOffset;
				return ERedisParseResult::ProtocolError;
			}

			outValue.arrayVal.reserve(static_cast<size_t>(i64Count));
			for( int64 i = 0; i < i64Count; ++i )
			{
				RedisValue elem;
				const ERedisParseResult elemResult = ParseValue(pBuffer, nSize, nOffset, elem, nDepth + 1);
				if( elemResult != ERedisParseResult::Success )
				{
					nOffset = nSavedOffset;
					return elemResult;
				}
				outValue.arrayVal.push_back(std::move(elem));
			}
			return ERedisParseResult::Success;
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
		return ERedisParseResult::ProtocolError;
	}

	nOffset = nSavedOffset;
	return ERedisParseResult::ProtocolError;
}

//***************************************************************************
// @brief \r\n 개행문자 기준으로 한 줄의 문자열을 읽음
// @param pBuffer 수신 데이터 버퍼 포인터
// @param nSize 버퍼 전체 크기
// @param nOffset 읽기 오프셋 위치 (입출력)
// @param strLine 읽어온 라인 문자열 (출력)
// @return Success/NeedMoreData (형식 자체는 항상 유효하므로 ProtocolError는
//         반환하지 않는다 — 개행을 못 찾으면 데이터 부족으로 본다)
//***************************************************************************
ERedisParseResult CRedisParser::ReadLine(const char* pBuffer, const int32 nSize, int32& nOffset, std::string& strLine)
{
	for( int32 i = nOffset; i < nSize - 1; ++i )
	{
		if( pBuffer[i] == '\r' && pBuffer[i + 1] == '\n' )
		{
			strLine.assign(pBuffer + nOffset, i - nOffset);
			nOffset = i + 2;
			return ERedisParseResult::Success;
		}
	}
	return ERedisParseResult::NeedMoreData;
}