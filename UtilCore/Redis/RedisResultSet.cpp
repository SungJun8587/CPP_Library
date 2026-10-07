
//***************************************************************************
// RedisResultSet.cpp: implementation of the CRedisResultSet class.
//
//***************************************************************************

#include "pch.h"
#include "RedisResultSet.h"

namespace
{
	// stoul()/stoull()은 "-1" 같은 음수 문자열을 예외 없이 래핑된 큰 값으로 돌려준다.
	bool IsNegativeText(const std::string& str)
	{
		const size_t nPos = str.find_first_not_of(" \t\r\n");
		return nPos != std::string::npos && str[nPos] == '-';
	}
}

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

	// [참고] value.eType == ERedisType::Error인 경우 위 어느 분기에도
	// 해당하지 않아 _vecResultSplit가 빈 채로 남는다 — 즉 이 클래스는
	// "에러 응답"과 "진짜 빈 결과"를 구분하지 못하고 둘 다 IsEmpty()==true로
	// 본다. 에러와 빈 결과를 구분해야 하는 호출부(예: 일시적 연결 오류를
	// 재시도해야 하는 경우)는 이 클래스를 통하지 말고 RedisValue::eType을
	// 직접 확인해야 한다 — ChatServerMainChat.cpp::SendCommandWithRetry()가
	// 이 방식으로 이미 구분하고 있다.
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
// @details [수정 — 버그 수정: 실패 시 커서 이동] 예전엔
//          "_vecResultSplit[_readCursor++]"를 std::stoi()의 인자 자리에서
//          직접 평가했다 — 인자 평가는 함수 호출 전에 끝나므로, stoi()가
//          예외를 던져도(변환 실패) _readCursor는 이미 증가된 뒤였다.
//          그러면 실패한 필드가 조용히 "소비"되어버려서, 이 함수를 &&로
//          체이닝하지 않고 개별 호출하는 코드에서는 그 다음 GetData()
//          호출이 원래 읽어야 할 필드가 아니라 그 다음 필드를 읽게 되는
//          정렬 밀림이 생길 수 있었다. 이제 먼저 문자열만 복사해두고,
//          변환에 성공했을 때만 커서를 증가시킨다 — 실패하면 커서가 그
//          자리에 그대로 남아 있어, 호출부가 같은 필드를 다른 타입으로
//          다시 시도하거나 원인 파악을 위해 그 값을 들여다볼 수 있다.
//***************************************************************************
bool CRedisResultSet::GetData(INT8& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	try
	{
		const int nValue = std::stoi(_vecResultSplit[_readCursor]);
		if( nValue < (std::numeric_limits<INT8>::min)() || nValue >(std::numeric_limits<INT8>::max)() )
			return false;

		Dest = static_cast<INT8>(nValue);
		++_readCursor;
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
		if( IsNegativeText(_vecResultSplit[_readCursor]) )
			return false;

		const unsigned long nValue = std::stoul(_vecResultSplit[_readCursor]);
		if( nValue > (std::numeric_limits<UINT8>::max)() )
			return false;

		Dest = static_cast<UINT8>(nValue);
		++_readCursor;
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
		const int nValue = std::stoi(_vecResultSplit[_readCursor]);
		if( nValue < (std::numeric_limits<INT16>::min)() || nValue >(std::numeric_limits<INT16>::max)() )
			return false;

		Dest = static_cast<INT16>(nValue);
		++_readCursor;
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
		Dest = std::stoi(_vecResultSplit[_readCursor]);
		++_readCursor;
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
		if( IsNegativeText(_vecResultSplit[_readCursor]) )
			return false;

		Dest = static_cast<UINT32>(std::stoul(_vecResultSplit[_readCursor]));
		++_readCursor;
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
		Dest = std::stoll(_vecResultSplit[_readCursor]);
		++_readCursor;
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
		if( IsNegativeText(_vecResultSplit[_readCursor]) )
			return false;

		Dest = std::stoull(_vecResultSplit[_readCursor]);
		++_readCursor;
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
// @brief 순차 커서 위치에서 std::wstring 타입 데이터를 추출함
// @details Redis에 저장된 문자열은 UTF-8이므로 CP_UTF8로 UTF-16 변환한다
//          (GetData(TCHAR*, int)와 같은 코드페이지). 변환에 실패하면 커서를
//          옮기지 않고 false를 반환한다.
//***************************************************************************
bool CRedisResultSet::GetData(std::wstring& Dest)
{
	Dest.clear();
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	const std::string& strSrc = _vecResultSplit[_readCursor];
	if( !strSrc.empty() )
	{
		const int nSrcLen = static_cast<int>(strSrc.size());
		const int nNeeded = ::MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), nSrcLen, nullptr, 0);
		if( nNeeded <= 0 )
			return false;

		Dest.resize(static_cast<size_t>(nNeeded));
		if( ::MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), nSrcLen, &Dest[0], nNeeded) <= 0 )
		{
			Dest.clear();
			return false;
		}
	}

	++_readCursor;
	return true;
}

//***************************************************************************
// @brief 순차 커서 위치에서 TCHAR 문자열 버퍼로 데이터를 문자 인코딩 변환하여 복사함
// @param Dest 문자열을 저장할 TCHAR 버퍼 포인터
// @param nSize 버퍼의 TCHAR 요소 개수
// @details [수정 — 버그 수정: 잘못된 코드페이지] 예전엔 MultiByteToWideChar()에
//          CP_ACP(시스템 기본 ANSI 코드페이지 — 한글 Windows에선 CP949)를
//          썼다. 그런데 이 프로젝트의 다른 곳(RedisService.cpp의
//          TCharToString()/TStringToString())은 UTF-8 변환 유틸을 쓰고
//          있어서, Redis에 저장/조회되는 문자열은 UTF-8이 관례로 보인다.
//          CP949와 UTF-8은 한글을 표현하는 바이트 시퀀스 자체가 다르므로,
//          한글 로케일에서 실행해도 CP_ACP로 UTF-8 바이트를 해석하면 깨진
//          문자열이 나온다. CP_UTF8로 바꿨다.
//***************************************************************************
bool CRedisResultSet::GetData(TCHAR* Dest, int nSize)
{
	if( Dest == nullptr || nSize <= 0 ) return false;
	::memset(Dest, 0, nSize * sizeof(TCHAR));

	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	const std::string& strSrc = _vecResultSplit[_readCursor++];

#if defined(UNICODE) || defined(_UNICODE)
	if( !strSrc.empty() )
	{
		const int nSrcLen = static_cast<int>(strSrc.size());

		// 버퍼에 다 들어가면 그대로 변환한다. 모자라면 MultiByteToWideChar()는 아무것도
		// 쓰지 않고 실패하므로(결과가 빈 문자열이 됨), 임시 버퍼로 변환한 뒤 잘라 넣는다 —
		// ANSI 빌드의 _TRUNCATE 동작과 같다.
		if( ::MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), nSrcLen, Dest, nSize - 1) == 0 )
		{
			const int nNeeded = ::MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), nSrcLen, nullptr, 0);
			if( nNeeded > 0 )
			{
				std::wstring strWide(static_cast<size_t>(nNeeded), L'\0');
				::MultiByteToWideChar(CP_UTF8, 0, strSrc.c_str(), nSrcLen, &strWide[0], nNeeded);
				::wcsncpy_s(Dest, nSize, strWide.c_str(), _TRUNCATE);
			}
		}
	}
#else
	::strncpy_s(Dest, nSize, strSrc.c_str(), _TRUNCATE);
#endif

	return true;
}