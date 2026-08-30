
//***************************************************************************
// DBBind.h : interface for the Database Bind.
//
//***************************************************************************

#ifndef UC_DBBIND_H
#define UC_DBBIND_H

//***************************************************************************
// @brief 지정된 비트 수(C)만큼 비트 플래그(1)를 채우기 위한 템플릿 구조체
// @detail 컴파일 타임 메타프로그래밍을 통해 0번부터 C-1번 비트까지 모두 1로 설정된 mask 값을 생성합니다.
//***************************************************************************
template<int32 C>
struct FullBits { enum { value = (1 << (C - 1)) | FullBits<C - 1>::value }; };

//***************************************************************************
// @brief FullBits 1비트 특수화 구조체
//***************************************************************************
template<>
struct FullBits<1> { enum { value = 1 }; };

//***************************************************************************
// @brief FullBits 0비트 특수화 구조체
//***************************************************************************
template<>
struct FullBits<0> { enum { value = 0 }; };

//***************************************************************************
// @brief ODBC 파라미터 및 컬럼 바인딩 자동화 래퍼 클래스
// @detail 쿼리 파라미터 개수(ParamCount)와 바인딩 컬럼 개수(ColumnCount)를 템플릿 인자로 받아
//         바인딩 상태 플래그 검증 및 CBaseODBC 래핑 기능을 제공합니다.
//***************************************************************************
template<int32 ParamCount, int32 ColumnCount>
class CDBBind
{
public:
	//***************************************************************************
	// @brief CDBBind 생성자입니다.
	// @param dbConn 바인딩을 수행할 CBaseODBC 객체 참조
	// @param query 실행할 SQL 쿼리 문자열
	//***************************************************************************
	CDBBind(CBaseODBC& dbConn, _tstring query)
		: _dbConn(dbConn), _query(query)
	{
		::memset(_paramIndex, 0, sizeof(_paramIndex));
		::memset(_columnIndex, 0, sizeof(_columnIndex));
		_paramFlag = 0;
		_columnFlag = 0;
	}

	// 복사 생성자 및 대입 연산자 금지 (소멸자에서 ClearStmt 중복 호출 방지)
	CDBBind(const CDBBind&) = delete;
	CDBBind& operator=(const CDBBind&) = delete;

	//***************************************************************************
	// @brief CDBBind 소멸자입니다.
	// @detail 객체가 스코프를 벗어나 소멸될 때 자동으로 바인딩 및 구문(Stmt)을 정리합니다.
	//***************************************************************************
	~CDBBind()
	{
		_paramFlag = 0;
		_columnFlag = 0;
		_dbConn.ClearStmt();
	}

	//***************************************************************************
	// @brief 설정된 모든 파라미터와 컬럼이 빠짐없이 바인딩되었는지 비트 플래그로 검증합니다.
	// @return bool 모든 파라미터 및 컬럼 바인딩이 완료되었으면 true, 아니면 false
	//***************************************************************************
	bool Validate()
	{
		return _paramFlag == FullBits<ParamCount>::value && _columnFlag == FullBits<ColumnCount>::value;
	}

	//***************************************************************************
	// @brief 바인딩 검증 후 SQL 쿼리를 직접 실행(ExecDirect)합니다.
	// @return bool 쿼리 실행 성공 여부
	//***************************************************************************
	bool ExecDirect()
	{
		ASSERT_CRASH(Validate());
		return _dbConn.ExecDirect(_query.c_str());
	}

	//***************************************************************************
	// @brief SQL 쿼리를 준비(Prepare)합니다.
	// @return bool Prepare 성공 여부
	//***************************************************************************
	bool Prepare()
	{
		return _dbConn.PrepareQuery(_query.c_str());
	}

	//***************************************************************************
	// @brief 준비된 SQL 쿼리를 실행(Execute)합니다.
	// @return bool 쿼리 실행 성공 여부
	//***************************************************************************
	bool Execute()
	{
		ASSERT_CRASH(Validate());
		return _dbConn.Execute();
	}

	//***************************************************************************
	// @brief 결과 세트에서 다음 레코드를 가져옵니다.
	// @return bool 인출(Fetch) 성공 여부
	//***************************************************************************
	bool Fetch()
	{
		return _dbConn.Fetch();
	}

	//***************************************************************************
	// @brief 영향받은 행(Row)의 수를 반환합니다.
	// @return int64 행의 개수
	//***************************************************************************
	int64 RowCount()
	{
		return _dbConn.RowCount();
	}

	//***************************************************************************
	// @brief 바인딩에 사용 중인 SQL 쿼리 문자열을 반환합니다.
	// @return _tstring SQL 쿼리 문자열
	//***************************************************************************
	_tstring GetSQLString()
	{
		return _query;
	}

public:
	//***************************************************************************
	// @brief 입력 파라미터를 일반 데이터 타입 참조로 바인딩합니다.
	// @param idx 바인딩할 파라미터 인덱스 (0부터 시작)
	// @param value 바인딩할 변수 참조
	//***************************************************************************
	template<typename T>
	void BindParam(int32 idx, T& value)
	{
		_dbConn.BindParamInput(idx + 1, value, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 입력 파라미터를 널 종료 문자열로 바인딩합니다.
	// @param idx 바인딩할 파라미터 인덱스 (0부터 시작)
	// @param value 바인딩할 널 종료 문자열 포인터
	//***************************************************************************
	void BindParam(int32 idx, const TCHAR* value)
	{
		_dbConn.BindParamInput(idx + 1, value, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 입력 파라미터를 고정 크기 배열(바이트 버퍼)로 바인딩합니다.
	// @param idx 바인딩할 파라미터 인덱스 (0부터 시작)
	// @param value 바인딩할 고정 크기 배열 참조
	//***************************************************************************
	template<typename T, int32 N>
	void BindParam(int32 idx, T(&value)[N])
	{
		_dbConn.BindParamInput(idx + 1, (const BYTE*)value, size32(T) * N, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 입력 파라미터를 포인터와 크기를 지정하여 바인딩합니다.
	// @param idx 바인딩할 파라미터 인덱스 (0부터 시작)
	// @param value 바인딩할 버퍼 포인터
	// @param N 버퍼 요소의 개수
	//***************************************************************************
	template<typename T>
	void BindParam(int32 idx, T* value, int32 N)
	{
		_dbConn.BindParamInput(idx + 1, (const BYTE*)value, size32(T) * N, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 일반 데이터 타입 변수에 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param value 결과를 받을 변수 참조
	//***************************************************************************
	template<typename T>
	void BindCol(int32 idx, T& value)
	{
		_dbConn.BindCol(idx + 1, value, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 지정된 TargetType 형태의 int64 변수에 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param targetType ODBC C 데이터 타입 (SQLSMALLINT)
	// @param value 결과를 받을 int64 변수 참조
	//***************************************************************************
	void BindCol(int32 idx, SQLSMALLINT targetType, int64& value)
	{
		_dbConn.BindCol(idx + 1, targetType, value, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 지정된 TargetType 형태의 uint64 변수에 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param targetType ODBC C 데이터 타입 (SQLSMALLINT)
	// @param value 결과를 받을 uint64 변수 참조
	//***************************************************************************
	void BindCol(int32 idx, SQLSMALLINT targetType, uint64& value)
	{
		_dbConn.BindCol(idx + 1, targetType, value, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 고정 크기 TCHAR 문자열 배열에 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param value 결과를 받을 TCHAR 배열 참조
	//***************************************************************************
	template<int32 N>
	void BindCol(int32 idx, TCHAR(&value)[N])
	{
		int32 iLength = N - 1;

		_dbConn.BindCol(idx + 1, value, iLength, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 TCHAR 포인터와 버퍼 길이를 지정하여 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param value 결과를 받을 TCHAR 버퍼 포인터
	// @param len 버퍼 전체 길이
	//***************************************************************************
	void BindCol(int32 idx, TCHAR* value, int32 len)
	{
		int32 iLength = len - 1;

		_dbConn.BindCol(idx + 1, value, iLength, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	//***************************************************************************
	// @brief 컬럼을 일반 고정 크기 배열(바이트 버퍼)에 바인딩합니다.
	// @param idx 바인딩할 컬럼 인덱스 (0부터 시작)
	// @param value 결과를 받을 배열 참조
	//***************************************************************************
	template<typename T, int32 N>
	void BindCol(int32 idx, T(&value)[N])
	{
		_dbConn.BindCol(idx + 1, value, size32(T) * N, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

protected:
	CBaseODBC&		_dbConn;											// ODBC 연결 제어 객체 참조
	_tstring		_query;												// 바인딩 대상 SQL 쿼리 문자열
	SQLLEN			_paramIndex[ParamCount > 0 ? ParamCount : 1];		// 파라미터 길이/상태 지시자 배열
	SQLLEN			_columnIndex[ColumnCount > 0 ? ColumnCount : 1];	// 컬럼 길이/상태 지시자 배열
	uint64			_paramFlag;											// 파라미터 바인딩 완료 여부 비트 플래그
	uint64			_columnFlag;										// 컬럼 바인딩 완료 여부 비트 플래그
};

#endif // ndef UC_DBBIND_H