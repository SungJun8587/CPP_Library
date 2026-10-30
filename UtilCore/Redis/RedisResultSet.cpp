
//***************************************************************************
// RedisResultSet.cpp: implementation of the CRedisResultSet class.
//
//***************************************************************************

#include "pch.h"
#include "RedisResultSet.h"
#include <Redis/RedisHelper.h>


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

	// 에러 응답은 어느 분기에도 해당하지 않아 _vecResultSplit이 빈 채로 남는다.
	// "정상적인 빈 결과"와 구분할 수 있도록 IsError()로 플래그를 남긴다.
	else if( value.eType == ERedisType::Error )
	{
		_bError = true;
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

	// 홀수면 (Member-Score) 쌍이 아닌 응답이다. 개발 빌드에서는 즉시 드러나게 단정하고,
	// 운영 빌드에서는 서버 프로세스를 죽이지 않도록 로그만 남기고 완성된 쌍의 개수를 돌려준다.
#ifdef _DEBUG
	ASSERT_CRASH((_vecResultSplit.size() % 2) == 0);
#else
	if( (_vecResultSplit.size() % 2) != 0 )
		LOG_ERROR(_T("CRedisResultSet::GetRankInfoRetCount: 응답 요소 수(%d)가 짝수가 아님"), static_cast<int>(_vecResultSplit.size()));
#endif

	return static_cast<INT32>(_vecResultSplit.size()) / 2;
}

//***************************************************************************
// @brief 순차 커서 위치에서 정수 타입 데이터를 추출함 (INT8~UINT64 공용)
// @details 문자열 전체가 10진 정수이고 Dest 범위 안일 때만 성공하며, 이때만 커서를 옮긴다.
//          실패하면 커서가 그 자리에 남아, 호출부가 같은 필드를 다른 타입으로 다시 시도하거나
//          원인 파악을 위해 값을 들여다볼 수 있다(변환 실패한 필드가 조용히 소비되어 이후
//          필드가 밀리는 일이 없다). 부호 없는 형식에 음수 문자열이 오면 실패한다.
//***************************************************************************
template<typename T>
bool CRedisResultSet::ReadInteger(T& Dest)
{
	Dest = 0;
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	if( !Redis::ParseInteger(_vecResultSplit[_readCursor], Dest) )
	{
		Dest = 0;
		return false;
	}

	++_readCursor;
	return true;
}

bool CRedisResultSet::GetData(INT8& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(UINT8& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(INT16& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(INT32& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(UINT32& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(INT64& Dest) { return ReadInteger(Dest); }
bool CRedisResultSet::GetData(UINT64& Dest) { return ReadInteger(Dest); }

//***************************************************************************
// @brief 순차 커서 위치에서 std::string 타입 데이터를 추출함
//***************************************************************************
bool CRedisResultSet::GetData(std::string& Dest)
{
	if( _vecResultSplit.empty() || _vecResultSplit.size() <= _readCursor ) return false;

	// 한 번 읽은 필드는 커서가 지나가므로 다시 읽히지 않는다 — 복사 대신 이동한다.
	Dest = std::move(_vecResultSplit[_readCursor++]);
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

	const std::string& strSrc = _vecResultSplit[_readCursor];

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
			else
			{
				return false; // 변환 불가 — 다른 GetData()와 같이 커서를 옮기지 않는다
			}
		}
	}
#else
	::strncpy_s(Dest, nSize, strSrc.c_str(), _TRUNCATE);
#endif

	++_readCursor;
	return true;
}