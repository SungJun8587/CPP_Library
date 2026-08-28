
//***************************************************************************
// DBSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef UC_DBSQLQUERY_H
#define UC_DBSQLQUERY_H

#include <DB/DBMSSQLQuery.h> 
#include <DB/DBMYSQLQuery.h> 
#include <DB/DBORACLEQuery.h> 

//***************************************************************************
// @brief 데이터베이스 정보
// @details 데이터베이스 명 정보를 저장합니다.
//***************************************************************************
class DB_INFO
{
public:
	TCHAR tszDBName[DATABASE_NAME_STRLEN] = { 0, }; // 데이터베이스 명
};

//***************************************************************************
// @brief 데이터베이스 오브젝트
// @details 데이터베이스 내의 오브젝트 정보(이름, 타입)를 저장합니다.
//***************************************************************************
class DB_OBJECT
{
	TCHAR tszDBName[DATABASE_NAME_STRLEN] = { 0, }; // 데이터베이스 명
	EDBObjectType ObjectType;                       // 오브젝트 타입
	TCHAR tszObjectName[DATABASE_OBJECT_NAME_STRLEN] = { 0, }; // 오브젝트 명
};

//***************************************************************************
// @brief DB 시스템 정보
// @details DB 종류, 버전, 캐릭터셋, 정렬 규칙 등의 시스템 정보를 저장합니다.
//***************************************************************************
class DB_SYSTEM_INFO
{
public:
	EDBClass DBClass; // DB 종류(1/2/3 : MSSQL/MYSQL/ORACLE)
	TCHAR tszVersion[DATABASE_BASE_STRLEN] = { 0, }; // 버전
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, }; // 캐릭터셋
	TCHAR tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, }; // 데이터 정렬(문자비교규칙)
};

//***************************************************************************
// @brief DB 데이터 타입 정보
// @details 시스템 데이터 타입 속성 정보를 저장합니다. (MSSQL 전용 sys.types)
//***************************************************************************
class DB_SYSTEM_DATATYPE
{
public:
	uint8	SystemTypeId; // 시스템 타입 ID
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, }; // 데이터 타입 명
	int16	MaxLength; // 최대 길이
	uint8	Precision; // 정밀도
	uint8	Scale; // 소수 자릿수
	TCHAR   tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, }; // 문자비교규칙
	bool	IsNullable = false; // NULL 허용 여부
};

//***************************************************************************
// @brief 테이블 정보
// @details 스키마, 테이블 명, 엔진, 캐릭터셋 등 테이블 메타 정보를 저장합니다.
//***************************************************************************
class TABLE_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	int64	AutoIncrementValue; // Identity 생성된 마지막 값
	TCHAR	tszStorageEngine[DATABASE_BASE_STRLEN]; // 스토리지 엔진
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN]; // 캐릭터셋
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN]; // 문자비교규칙
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX]; // 테이블 주석
	TCHAR	tszCreateDate[DATETIME_STRLEN]; // 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN]; // 수정 일시
};

//***************************************************************************
// @brief 테이블 컬럼 정보
// @details 컬럼 명, 데이터 타입, 제약 사항 및 주석 등의 정보를 저장합니다.
//***************************************************************************
class COLUMN_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	int32	Seq; // 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN]; // 컬럼 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN]; // 컬럼 데이터타입
	int64	MaxLength; // 컬럼의 최대 길이(바이트)
	uint8   Precision; // 최대 전체 자릿수
	uint8	Scale; // 최대 소수 자릿수
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN]; // 컬럼 데이터타입 상세
	bool	IsNullable; // NULL 허용 여부
	bool	IsIdentity; // Identity 값 여부
	uint64	SeedValue; // Identity 시드 값
	uint64  IncValue; // Identity 증분 값
	TCHAR   tszDefaultConstraintName[DATABASE_WVARCHAR_MAX]; // 기본제약 조건 명
	TCHAR   tszDefaultDefinition[DATABASE_WVARCHAR_MAX]; // 기본값 정의
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN]; // 캐릭터셋
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN]; // 문자비교규칙
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX]; // 컬럼 주석
};

//***************************************************************************
// @brief 제약조건 정보
// @details 테이블에 설정된 제약조건 정보를 저장합니다.
//***************************************************************************
class CONSTRAINT_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszConstName[DATABASE_OBJECT_NAME_STRLEN]; // 제약조건 명
	TCHAR   tszConstType[DATABASE_OBJECT_NAME_STRLEN]; // 제약조건 타입
	TCHAR   tszConstTypeDesc[DATABASE_OBJECT_NAME_STRLEN]; // 제약조건 타입 설명
	TCHAR   tszConstValue[DATABASE_WVARCHAR_MAX]; // 제약조건 정의값
	bool	IsSystemNamed; // 시스템이 인덱스명을 할당했는지 여부
	bool	IsStatus; // 제약 조건의 시행 상태 (ORACLE 전용)
	TCHAR   tszSortValue[DATABASE_OBJECT_NAME_STRLEN]; // 정렬 값
};

//***************************************************************************
// @brief 인덱스 정보
// @details 인덱스 구성 및 키 속성 정보를 저장합니다.
//***************************************************************************
class INDEX_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN]; // 인덱스 명
	int32	IndexId; // 인덱스 고유번호
	TCHAR   tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN]; // 인덱스 타입
	bool    IsPrimaryKey; // 기본키 여부
	bool    IsUnique; // 유니크 여부
	int32   ColumnSeq; // 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN]; // 컬럼 명
	int8    ColumnSort; // 정렬(1/2 : ASC/DESC)
	bool	IsSystemNamed; // 시스템이 인덱스명을 할당했는지 여부
};

//***************************************************************************
// @brief 외래키 정보
// @details 외래키 관계 및 참조 동작 정보를 저장합니다.
//***************************************************************************
class FOREIGNKEYS_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszForeignKeyName[DATABASE_OBJECT_NAME_STRLEN]; // 외래키 명
	bool	IsDisabled; // FOREIGN KEY 제약 조건 사용 여부
	bool	IsNotTrusted; // 시스템의 데이터 일관성 확인 여부
	TCHAR	tszForeignKeyTableName[DATABASE_TABLE_NAME_STRLEN]; // 외래키 테이블 명
	TCHAR	tszForeignKeyColumnName[DATABASE_COLUMN_NAME_STRLEN]; // 외래키 컬럼 명
	TCHAR   tszReferenceKeySchemaName[DATABASE_OBJECT_NAME_STRLEN]; // 참조키 스키마 명
	TCHAR	tszReferenceKeyTableName[DATABASE_TABLE_NAME_STRLEN]; // 참조키 테이블 명
	TCHAR	tszReferenceKeyColumnName[DATABASE_COLUMN_NAME_STRLEN]; // 참조키 컬럼 명
	TCHAR   tszUpdateReferentialAction[60]; // 업데이트 발생 시 참조 작업
	TCHAR   tszDeleteReferentialAction[60]; // 삭제 발생 시 참조 작업
	bool	IsSystemNamed; // 시스템이 인덱스명을 할당했는지 여부
};

//***************************************************************************
// @brief 체크 제약조건 정보
// @details 컬럼 또는 테이블의 체크 제약조건 정보를 저장합니다.
//***************************************************************************
class CHECK_CONSTRAINT_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszCheckConstName[DATABASE_OBJECT_NAME_STRLEN]; // 체크 제약조건 명
	TCHAR   tszCheckValue[DATABASE_WVARCHAR_MAX]; // 컬럼 제약조건 정의값
	bool	IsSystemNamed; // 시스템이 인덱스명을 할당했는지 여부
};

//***************************************************************************
// @brief 기본값 제약조건 정보
// @details 컬럼의 기본값 제약조건 정보를 저장합니다.
//***************************************************************************
class DEFAULT_CONSTRAINT_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszDefaultConstName[DATABASE_OBJECT_NAME_STRLEN]; // 기본값 제약조건 명
	TCHAR   tszColumnName[DATABASE_COLUMN_NAME_STRLEN]; // 컬럼 명
	TCHAR   tszDefaultValue[DATABASE_WVARCHAR_MAX]; // 컬럼 제약조건 정의값
	bool	IsSystemNamed; // 시스템이 인덱스명을 할당했는지 여부
};

//***************************************************************************
// @brief 트리거 정보
// @details 테이블 트리거 메타 정보를 저장합니다.
//***************************************************************************
class TRIGGER_INFO
{
public:
	int32	ObjectId; // MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN]; // 테이블 명
	TCHAR   tszTriggerName[DATABASE_OBJECT_NAME_STRLEN]; // 트리거 명
};

//***************************************************************************
// @brief 저장프로시저 정보
// @details 저장프로시저의 메타 정보 및 소스 코드를 저장합니다.
//***************************************************************************
class PROCEDURE_INFO
{
public:
	int32	ObjectId; // MSSQL 저장프로시저 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN]; // 저장프로시저 명
	TCHAR	tszProcComment[DATABASE_WVARCHAR_MAX]; // 저장프로시저 주석
	TCHAR   tszProcText[DATABASE_OBJECT_CONTENTTEXT_STRLEN]; // 저장프로시저 내용
	TCHAR	tszCreateDate[DATETIME_STRLEN]; // 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN]; // 수정 일시
};

//***************************************************************************
// @brief 저장프로시저 파라미터 정보
// @details 저장프로시저 매개변수의 속성 정보를 저장합니다.
//***************************************************************************
class PROCEDURE_PARAM_INFO
{
public:
	int32	ObjectId; // MSSQL 저장프로시저 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN]; // 저장프로시저 명
	int32	Seq; // 파라미터 순서
	int8	ParamMode; // 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN]; // 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN]; // 파라미터 데이터타입
	uint64	MaxLength; // 파라미터의 최대 길이(바이트)
	uint8   Precision; // 최대 전체 자릿수
	uint8	Scale; // 최대 소수 자릿수
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN]; // 파라미터 데이터타입 상세
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX]; // 파라미터 주석
};

//***************************************************************************
// @brief 함수 정보
// @details 사용자 정의 함수의 메타 정보 및 소스 코드를 저장합니다.
//***************************************************************************
class FUNCTION_INFO
{
public:
	int32	ObjectId; // MSSQL 함수 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN]; // 함수 명
	TCHAR	tszFuncComment[DATABASE_WVARCHAR_MAX]; // 함수 주석
	TCHAR   tszFuncText[DATABASE_OBJECT_CONTENTTEXT_STRLEN]; // 함수 내용
	TCHAR	tszCreateDate[DATETIME_STRLEN]; // 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN]; // 수정 일시
};

//***************************************************************************
// @brief 함수 파라미터 정보
// @details 사용자 정의 함수 매개변수의 속성 정보를 저장합니다.
//***************************************************************************
class FUNCTION_PARAM_INFO
{
public:
	int32	ObjectId; // MSSQL 함수 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN]; // MSSQL 스키마 명
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN]; // 함수 명
	int32	Seq; // 파라미터 순서
	int8	ParamMode; // 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN]; // 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN]; // 파라미터 데이터타입
	uint64	MaxLength; // 파라미터의 최대 길이(바이트)
	uint8   Precision; // 최대 전체 자릿수
	uint8	Scale; // 최대 소수 자릿수
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN]; // 파라미터 데이터타입 상세
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX]; // 파라미터 주석
};

//***************************************************************************
// @brief DB 시스템 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetDBSystemQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetDBSystemQuery();
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetDBSystemQuery();
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetDBSystemQuery();
		break;
	}
	return query;
}

//***************************************************************************
// @brief DB 시스템 데이터타입 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetDBSystemDataTypeQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetDBSystemDataTypeQuery();
		break;
	case EDBClass::MYSQL:
		// MYSQL은 시스템 데이터타입 목록에 대한 시스템 테이블이 존재하지 않음
		break;
	case EDBClass::ORACLE:
		// ORACLE은 시스템 데이터타입 목록에 대한 시스템 테이블이 존재하지 않음
		break;
	}
	return query;
}

//***************************************************************************
// @brief 데이터베이스 목록 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetDatabaseListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetDatabaseListQuery();
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetDatabaseListQuery();
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetDatabaseListQuery();
		break;
	}
	return query;
}

//***************************************************************************
// @brief 테이블 목록 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetTableListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetTableListQuery();
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetTableListQuery();
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetTableListQuery();
		break;
	}
	return query;
}

//***************************************************************************
// @brief 테이블 상세 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetTableInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetTableInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetTableInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetTableInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 테이블 Identity 컬럼 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetTableIdentityColumnInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		break;
	case EDBClass::MYSQL:
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetTableIdentityColumnInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 테이블 컬럼 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetTableColumnInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetTableColumnInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetTableColumnInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetTableColumnInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 제약조건 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetConstraintsInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetConstraintsInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetConstraintsInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetConstraintsInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 인덱스 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetIndexInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetIndexInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetIndexInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetIndexInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 인덱스 옵션 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetIndexOptionInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetIndexOptionInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		break;
	case EDBClass::ORACLE:
		break;
	}
	return query;
}

//***************************************************************************
// @brief 파티션 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetPartitionInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetPartitionInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetPartitionInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetPartitionInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 외래키 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetForeignKeyInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetForeignKeyInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetForeignKeyInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetForeignKeyInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 기본값 제약조건 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetDefaultConstInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetDefaultConstInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		break;
	case EDBClass::ORACLE:
		break;
	}
	return query;
}

//***************************************************************************
// @brief 체크 제약조건 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetCheckConstInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetCheckConstInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetCheckConstInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetCheckConstInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 트리거 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param tableName 조회할 테이블 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetTriggerInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetTriggerInfoQuery(tableName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetTriggerInfoQuery(tableName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetTriggerInfoQuery(tableName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 저장프로시저 목록 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetProcedureListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetProcedureListQuery();
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetProcedureListQuery();
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetProcedureListQuery();
		break;
	}
	return query;
}

//***************************************************************************
// @brief 저장프로시저 상세 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param procName 조회할 저장프로시저 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetProcedureInfoQuery(EDBClass dbClass, _tstring procName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetProcedureInfoQuery(procName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetProcedureInfoQuery(procName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetProcedureInfoQuery(procName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 저장프로시저 파라미터 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param procName 조회할 저장프로시저 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetProcedureParamInfoQuery(EDBClass dbClass, _tstring procName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetProcedureParamInfoQuery(procName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetProcedureParamInfoQuery(procName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetProcedureParamInfoQuery(procName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 함수 목록 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetFunctionListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetFunctionListQuery();
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetFunctionListQuery();
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetFunctionListQuery();
		break;
	}
	return query;
}

//***************************************************************************
// @brief 함수 상세 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param funcName 조회할 함수 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetFunctionInfoQuery(EDBClass dbClass, _tstring funcName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetFunctionInfoQuery(funcName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetFunctionInfoQuery(funcName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetFunctionInfoQuery(funcName);
		break;
	}
	return query;
}

//***************************************************************************
// @brief 함수 파라미터 정보 조회 쿼리 반환
// @param dbClass DB 종류 (MSSQL / MYSQL / ORACLE)
// @param funcName 조회할 함수 명 (기본값: 전체)
// @return _tstring 생성된 SQL 쿼리문
//***************************************************************************
inline _tstring GetFunctionParamInfoQuery(EDBClass dbClass, _tstring funcName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
	case EDBClass::MSSQL:
		query = MSSQLGetFunctionParamInfoQuery(funcName);
		break;
	case EDBClass::MYSQL:
		query = MYSQLGetFunctionParamInfoQuery(funcName);
		break;
	case EDBClass::ORACLE:
		query = ORACLEGetFunctionParamInfoQuery(funcName);
		break;
	}
	return query;
}

#endif // ndef UC_DBSQLQUERY_H