
//***************************************************************************
// RedisResultSet.h : interface for the CRedisResultSet class.
//
//***************************************************************************

#ifndef __REDISRESULTSET_H__
#define __REDISRESULTSET_H__

#pragma once

#ifndef __REDISPROTOCOL_H__
#include <Redis/RedisProtocol.h>
#endif

#include <vector>
#include <string>

//***************************************************************************
// @class RedisResultSet
// @brief RedisValue 응답 객체로부터 타입 안전하게 데이터를 순차 추출하는 Util 클래스.
//
// @details
// 역할:
//      CRedisService / CRedisClient를 통해 전달받은 파싱 결과(RedisValue)를
//      내부 문자열 컨테이너로 변환하고, 순차 커서(Cursor) 방식으로 다양한
//      기본 데이터 타입(INT32, INT64, TCHAR 등)으로 변환 및 수거합니다.
//
// 특징:
//      - RedisValue의 ERedisType 응답 형태(SimpleString, BulkString, Integer, Array 등)를 완벽 대응
//      - Boost 라이브러리 없이 C++ 표준 라이브러리(std::stoll 등)만 활용하여 변환
//      - TCHAR/MultiByte 변환 지원으로 유니코드/아스키 환경 모두 대응
//
// 사용 예시 (Usage Example):
//      @code
//      pRedisService->SendCommand({"HMGET", "User:1001", "Level", "Exp", "Name"}, 
//          [](const RedisValue& res) {
//              // 1. CRedisResultSet 생성
//              CRedisResultSet resultSet(res);
//
//              if (resultSet.IsEmpty())
//                  return;
//
//              // 2. 변수 준비
//              INT32 nLevel = 0;
//              INT64 nExp = 0;
//              TCHAR szName[32] = { 0, };
//
//              // 3. GetData()로 순차적 데이터 추출 (성공 시 커서 자동 증가)
//              if (resultSet.GetData(nLevel) && 
//                  resultSet.GetData(nExp) && 
//                  resultSet.GetData(szName, 32))
//              {
//                  // 유저 데이터 처리 로직 수행
//              }
//          });
//      @endcode
//***************************************************************************
class CRedisResultSet
{
public:
	//***************************************************************************
	// @brief CRedisResultSet 생성자
	// @param value 파싱이 완료된 Redis 응답 객체
	//***************************************************************************
	explicit CRedisResultSet(const RedisValue& value);

	//***************************************************************************
	// @brief CRedisResultSet 소멸자
	//***************************************************************************
	~CRedisResultSet() = default;

public:
	//***************************************************************************
	// @brief 결과 집합에서 지정된 타입으로 데이터를 순차 추출합니다.
	// @param Dest 데이터를 전달받을 변수 참조
	// @return 데이터 추출 성공 여부 (true: 성공, false: 범위를 벗어났거나 변환 실패)
	//***************************************************************************
	bool GetData(INT8& Dest);
	bool GetData(UINT8& Dest);
	bool GetData(INT16& Dest);
	bool GetData(INT32& Dest);
	bool GetData(UINT32& Dest);
	bool GetData(INT64& Dest);
	bool GetData(UINT64& Dest);
	bool GetData(std::string& Dest);
	bool GetData(TCHAR* Dest, int nSize);

	//***************************************************************************
	// @brief 랭킹 등의 Multi-Bulk 응답에서 튜플(Key-Value 쌍) 개수를 반환합니다.
	// @return INT32 전체 요소 수의 절반 (요소 수 / 2)
	//***************************************************************************
	INT32 GetRankInfoRetCount() const;

	//***************************************************************************
	// @brief 결과 집합이 비어있는지 여부를 확인합니다.
	// @return bool true: 비어있음, false: 데이터 존재함
	//***************************************************************************
	bool IsEmpty() const;

	//***************************************************************************
	// @brief 전체 파싱 결과 요소의 개수를 반환합니다.
	// @return size_t 요소 개수
	//***************************************************************************
	size_t GetSize() const { return _vecResultSplit.size(); }

private:
	UINT                    _readCursor;      // 순차 데이터 읽기 커서 위치
	CVector<std::string>	_vecResultSplit;  // 문자열로 분할 및 보관된 결과 목록
};

#endif // ndef __REDISRESULTSET_H__