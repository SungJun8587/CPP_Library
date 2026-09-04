
//***************************************************************************
// RedisResultSet.cpp: implementation of the CRedisResultSet class.
//
//***************************************************************************

#include "pch.h"
#include "RedisResultSet.h"

//***************************************************************************
// @brief RedisValue 응답 객체를 수신받아 내부 순차 보관 컨테이너를 구성함
// @param value 파싱 완료된 Redis 응답 구조체
//***************************************************************************
CRedisResultSet::CRedisResultSet(const RedisValue& value)
	: _readCursor(0)
{
	if( value.IsNull() )
	{
		return;
	}

	if( value.eType == ERedisType::SimpleString || value.eType == ERedisType::BulkString )
	{
		_vecResultSplit.push_back(value.strVal);
	}
	else if( value.eType == ERedisType::Integer )
	{
		_vecResultSplit.push_back(std::to_string(value.i64Val));
	}
	else if( value.eType == ERedisType::Array )
	{
		const size_t size = value.arrayVal.size();
		_vecResultSplit.reserve(size);

		for( size_t i = 0; i < size; ++i )
		{
			const RedisValue& item = value.arrayVal[i];

			if( item.IsNull() )
			{
				_vecResultSplit.push_back("");
			}
			else if( item.eType == ERedisType::Integer )
			{
				_vecResultSplit.push_back(std::to_string(item.i64Val));
			}
			else
			{
				_vecResultSplit.push_back(item.strVal);
			}
		}
	}
}

//***************************************************************************
// @brief 결과 데이터 존재 여부를 반환함
// @return true: 비어있음, false: 데이터 존재함
//***************************************************************************
bool CRedisResultSet::IsEmpty() const
{
	return _vecResultSplit.empty();
}

//***************************************************************************
// @brief (Member-Score) 쌍으로 구성된 랭킹 결과의 항목 개수를 반환함
// @details 정상적인 랭킹 응답(예: ZRANGE ... WITHSCORES)은 항상 짝수 개의
//          요소로 구성되므로, 홀수인 경우는 서버 응답이 기대한 커맨드 형태와
//          다르다는 신호로 보고 개발 빌드에서 즉시 드러나도록 assert한다.
//***************************************************************************
INT32 CRedisResultSet::GetRankInfoRetCount() const
{
	if( _vecResultSplit.empty() )
		return 0;

	ASSERT_CRASH((_vecResultSplit.size() % 2) == 0);

	return static_cast<INT32>(_vecResultSplit.size()) / 2;
}

//***************************************************************************
// @brief 순차 커서 위치에서 INT8 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(INT8& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = static_cast<INT8>(std::stoi(_vecResultSplit[_readCursor++]));
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 UINT8 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(UINT8& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = static_cast<UINT8>(std::stoul(_vecResultSplit[_readCursor++]));
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 INT16 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(INT16& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = static_cast<INT16>(std::stoi(_vecResultSplit[_readCursor++]));
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 INT32 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(INT32& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = std::stoi(_vecResultSplit[_readCursor++]);
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 UINT32 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(UINT32& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = static_cast<UINT32>(std::stoul(_vecResultSplit[_readCursor++]));
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 INT64 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(INT64& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = std::stoll(_vecResultSplit[_readCursor++]);
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 UINT64 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(UINT64& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		Dest = std::stoull(_vecResultSplit[_readCursor++]);
		return true;
	}
	catch( const std::exception& )
	{
		return false;
	}
}

//***************************************************************************
// @brief 순차 커서 위치에서 std::string 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(std::string& Dest)
{
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	Dest = _vecResultSplit[_readCursor++];
	return true;
}

//***************************************************************************
// @brief 순차 커서 위치에서 TCHAR 문자열 버퍼로 데이터를 문자 인코딩 변환하여 복사함
// @param Dest 문자열을 저장할 TCHAR 버퍼 포인터
// @param nSize 버퍼의 TCHAR 요소 개수
//***************************************************************************
bool CRedisResultSet::GetData(TCHAR* Dest, int nSize)
{
	if( Dest == nullptr || nSize <= 0 ) return false;
	::memset(Dest, 0, nSize * sizeof(TCHAR));

	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

#if defined(UNICODE) || defined(_UNICODE)
	::MultiByteToWideChar(CP_ACP, 0, _vecResultSplit[_readCursor++].c_str(), -1, Dest, nSize);
#else
	::strncpy_s(Dest, nSize, _vecResultSplit[_readCursor++].c_str(), _TRUNCATE);
#endif

	return true;
}