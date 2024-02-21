
//***************************************************************************
// DBModel.h : interface for the Database Model.
//
//***************************************************************************

#ifndef __DBMODEL_H__
#define __DBMODEL_H__

#include <regex>

NAMESPACE_BEGIN(DBModel)

USING_SHARED_PTR(Column);
USING_SHARED_PTR(IndexColumn);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(IndexOption);
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

enum class EDataType
{
	NONE = 0,
	TEXT = 35,
	TINYINT = 48,
	SMALLINT = 52,
	INT = 56,
	REAL = 59,
	DATETIME = 61,
	FLOAT = 62,
	NTEXT = 99,
	BIT = 104,
	DECIMAL = 106,
	NUMERIC = 108,
	BIGINT = 127,
	VARBINARY = 165,
	VARCHAR = 167,
	BINARY = 173,
	CHAR = 175,
	NVARCHAR = 231,
	NCHAR = 239
};

//***************************************************************************
//
class DB_SYSTEMINFO
{
public:
	TCHAR tszVersion[DATABASE_BASE_STRLEN] = { 0, };				// 버전
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, };	// 캐릭터셋
	TCHAR tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };		// 데이터 정렬(문자비교규칙)
};

//***************************************************************************
//
class DB_SYSTEM_DATATYPE
{
public:
	uint8	SystemTypeId;
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	int16	MaxLength;
	uint8	Precision;
	uint8	Scale;
	TCHAR   tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };
	bool	IsNullable = false;
};

//***************************************************************************
//
class TABLE_INFO
{
public:
	int32	ObjectId;										// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	int64	AutoIncrementValue;								// Identity 생성된 마지막 값
	TCHAR	tszStorageEngine[DATABASE_BASE_STRLEN];			// 스토리지 엔진
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN];	// 캐릭터셋
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN];		// 문자비교규칙
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX];			// 테이블 주석
	TCHAR	tszCreateDate[DATETIME_STRLEN];					// 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN];					// 수정 일시
};

//***************************************************************************
//
class COLUMN_INFO
{
public:
	int32	ObjectId;											// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];			// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];			// 테이블 명
	int32	Seq;												// 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];			// 컬럼 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];			// 컬럼 데이터타입
	uint64	MaxLength;											// 컬럼의 최대 길이(바이트)
	uint8   Precision;											// 최대 전체 자릿수(숫자 기반 형식인 경우에는 형식의 최대 전체 자릿수, 그렇지 않으면 0)
	uint8	Scale;												// 최대 소수 자릿수(숫자 기반인 경우 형식의 최대 소수 자릿수, 그렇지 않으면 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];		// 컬럼 데이터타입 상세(Ex. VARCHAR[100])
	bool	IsNullable;											// NULL 허용 여부(true/false : 허용/비허용)
	bool	IsIdentity;											// Identity 값 여부(true/false : 유/무)
	uint64	SeedValue;											// Identity 시드 값
	uint64  IncValue;											// Identity 증분 값
	TCHAR   tszDefaultConstraintName[DATABASE_WVARCHAR_MAX];	// 기본제약 조건 명
	TCHAR   tszDefaultDefinition[DATABASE_WVARCHAR_MAX];		// 기본값 정의
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN];							// 캐릭터셋
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN];								// 문자비교규칙
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX];			// 컬럼 주석
};

//***************************************************************************
//
class INDEX_INFO
{
public:
	int32	ObjectId;										// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];		// 인덱스 명
	int32	IndexId;										// 인덱스 고유번호
	int8    IndexKind;										// 인덱스 종류(0/1/2 : NONE/CLUSTERED/NONCLUSTERED)
	int8    IndexType;										// 인덱스 타입(0/1/2/3/4/5 : NONE/PRIMARYKEY/UNIQUE/INDEX/FULLTEXT/SPATIAL)
	bool    IsPrimaryKey;									// 기본키 여부(true/false : 유/무)
	bool    IsUnique;										// 유니크 여부(true/false : 유/무)
	int32   ColumnSeq;										// 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// 컬럼 명
	int8    ColumnSort;										// 정렬(1/2 : ASC/DESC)
};

//***************************************************************************
//
class INDEX_OPTION_INFO
{
public:
	int32	ObjectId;											// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];			// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];			// 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];			// 인덱스 명
	int32	IndexId;											// 인덱스 고유번호
	bool    IsPrimaryKey;										// 기본키 여부(true/false : 유/무)
	bool    IsUnique;											// 유니크 여부(true/false : 유/무)
	bool	IsDisabled;											// 인덱스 비활성화 여부(0/1 : 활성화/비활성화)
	bool	IsPadded;											// PAD_INDEX = { ON | OFF } : 인덱스 패딩을 지정. 기본값은 OFF
	int8	FillFactor;											// FILLFACTOR = <fillfactor 값> : 각 인덱스 페이지의 리프 수준을 어느 정도 채울지 나타내는 비율을 지정
	bool	IgnoreDupKey;										// IGNORE_DUP_KEY = { ON | OFF } : 삽입 작업에서 고유 인덱스에 중복된 키 값을 삽입하려고 할 때 응답 유형을 지정
	bool	AllowRowLocks;										// ALLOW_ROW_LOCKS = { ON | OFF } : 인덱스에서 행 잠금 허용 여부(0/1 : 비허용/허용)
	bool	AllowPageLocks;										// ALLOW_PAGE_LOCKS = { ON | OFF } : 인덱스에서 페이지 잠금 허용 여부(0/1 : 비허용/허용)
	bool	HasFilter;											// 인덱스 필터 존재 여부(0/1 : 무/유)
	TCHAR   tszFilterDefinition[DATABASE_BASE_STRLEN];			// 필터링된 인덱스에 포함된 행의 하위 집합에 대한 식
	TCHAR   tszOptimizeForSequentialKey[DATABASE_BASE_STRLEN];	// OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF } : 마지막 페이지 삽입 경합에 최적화할지 여부를 지정. 기본값은 OFF
	bool	StatisticsNoRecompute;								// STATISTICS_NORECOMPUTE = { ON | OFF } : 통계를 다시 계산할지 여부를 지정. 기본값은 OFF
	bool	Resumable;											// RESUMABLE = { ON | OFF } : ALTER TABLE ADD CONSTRAINT 작업이 다시 시작될 수 있는지 여부를 지정. 기본값은 OFF
	int16	MaxDop;												// MAXDOP = <max_degree_of_parallelism> : 인덱스 작업 동안 max degree of parallelism 구성 옵션을 재정의 
	int8	DataCompression;									// 각 파티션에 대한 압축 상태(0/1/2/3/4 : NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	TCHAR   tszDataCompressionDesc[DATABASE_BASE_STRLEN];		// 각 파티션에 대한 압축 상태 설명 문자열(NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	bool	XmlCompression;										// 각 파티션에 대한 XML 압축 상태(0/1 : OFF/ON)
	TCHAR   tszXmlCompressionDesc[DATABASE_BASE_STRLEN];		// 각 파티션에 대한 XML 압축 상태 설명 문자열(OFF/ON)
	TCHAR   tszDataSpaceName[DATABASE_BASE_STRLEN];				// 데이터베이스 내에서 고유한 데이터 공간의 이름
	TCHAR   tszDataSpaceTypeDesc[DATABASE_BASE_STRLEN];			// 데이터 공간 유형에 대한 설명
};

//***************************************************************************
//
class FOREIGNKEYS_INFO
{
public:
	int32	ObjectId;												// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// 테이블 명
	TCHAR   tszForeignKeyName[DATABASE_OBJECT_NAME_STRLEN];			// 외래키 명
	TCHAR	tszForeignKeyTableName[DATABASE_TABLE_NAME_STRLEN];		// 외래키 테이블 명
	TCHAR	tszForeignKeyColumnName[DATABASE_COLUMN_NAME_STRLEN];	// 외래키 컬럼 명
	TCHAR	tszReferenceKeyTableName[DATABASE_TABLE_NAME_STRLEN];	// 참조키 테이블 명
	TCHAR	tszReferenceKeyColumnName[DATABASE_COLUMN_NAME_STRLEN];	// 참조키 컬럼 명
	TCHAR   tszUpdateReferentialAction[60];							// 업데이트 발생할 때 외래키에 대해 선언된 참조 작업(NO_ACTION/CASCADE/RESTRICT/SET NULL/SET DEFAULT)
	TCHAR   tszDeleteReferentialAction[60];							// 삭제 발생할 때 외래키에 대해 선언된 참조 작업(NO_ACTION/CASCADE/RESTRICT/SET NULL/SET DEFAULT)
};

//***************************************************************************
//
class TRIGGER_INFO
{
public:
	int32	ObjectId;												// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// 테이블 명
	TCHAR   tszTriggerName[DATABASE_OBJECT_NAME_STRLEN];			// 트리거 명
};

//***************************************************************************
//
class PROCEDURE_INFO
{
public:
	int32	ObjectId;												// MSSQL 저장프로시저 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN];				// 저장프로시저 명
	TCHAR	tszProcComment[DATABASE_WVARCHAR_MAX];					// 저장프로시저 주석
	TCHAR   tszProcText[DATABASE_OBJECT_CONTENTTEXT_STRLEN];		// 저장프로시저 내용
	TCHAR	tszCreateDate[DATETIME_STRLEN];							// 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN];							// 수정 일시
};

//***************************************************************************
//
class PROCEDURE_PARAM_INFO
{
public:
	int32	ObjectId;												// MSSQL 저장프로시저 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN];				// 저장프로시저 명
	int32	Seq;													// 파라미터 순서
	int8	ParamMode;												// 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// 파라미터 데이터타입
	uint64	MaxLength;												// 파라미터의 최대 길이(바이트)
	uint8   Precision;												// 최대 전체 자릿수(숫자 기반 형식인 경우에는 형식의 최대 전체 자릿수, 그렇지 않으면 0)
	uint8	Scale;													// 최대 소수 자릿수(숫자 기반인 경우 형식의 최대 소수 자릿수, 그렇지 않으면 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// 파라미터 데이터타입 상세(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// 파라미터 주석
};

//***************************************************************************
//
class FUNCTION_INFO
{
public:
	int32	ObjectId;												// MSSQL 함수 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN];				// 함수 명
	TCHAR	tszFuncComment[DATABASE_WVARCHAR_MAX];					// 함수 주석
	TCHAR   tszFuncText[DATABASE_OBJECT_CONTENTTEXT_STRLEN];		// 함수 내용
	TCHAR	tszCreateDate[DATETIME_STRLEN];							// 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN];							// 수정 일시
};

//***************************************************************************
//
class FUNCTION_PARAM_INFO
{
public:
	int32	ObjectId;												// MSSQL 함수 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN];				// 함수 명
	int32	Seq;													// 파라미터 순서
	int8	ParamMode;												// 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// 파라미터 데이터타입
	uint64	MaxLength;												// 파라미터의 최대 길이(바이트)
	uint8   Precision;												// 최대 전체 자릿수(숫자 기반 형식인 경우에는 형식의 최대 전체 자릿수, 그렇지 않으면 0)
	uint8	Scale;													// 최대 소수 자릿수(숫자 기반인 경우 형식의 최대 소수 자릿수, 그렇지 않으면 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// 파라미터 데이터타입 상세(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// 파라미터 주석
};

class Column
{
public:
	_tstring			CreateColumn(EDBClass dbClass);
	_tstring			ModifyColumn(EDBClass dbClass);
	_tstring			DropColumn(EDBClass dbClass);
	_tstring			CreateText(EDBClass dbClass);
	_tstring			CreateDefaultConstraint(EDBClass dbClass);
	_tstring			DropDefaultConstraint(EDBClass dbClass);

public:
	_tstring			_schemaName;
	_tstring			_tableName;
	_tstring			_seq;
	_tstring			_name;
	_tstring			_desc;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	bool				_nullable = false;
	bool				_identity = false;
	_tstring			_identitydesc;
	int64				_seedValue = 0;
	int64				_incrementValue = 0;
	_tstring			_characterset;
	_tstring			_collation;
	_tstring			_defaultConstraintName;
	_tstring			_defaultDefinition;
};

class IndexColumn
{
public:
	_tstring	GetSortText();

public:
	_tstring	_seq;
	_tstring	_name;
	EIndexSort	_sort = EIndexSort::ASC;
};

class IndexOption
{
public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_name;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	bool					_isDisabled = false;
	bool					_isPadded = false;
	int8					_fillFactor;
	bool					_ignoreDupKey = false;
	bool					_allowRowLocks = false;
	bool					_allowPageLocks = false;
	bool					_hasFilter = false;
	_tstring				_filterDefinition;
	_tstring				_optimizeForSequentialKey;
	bool					_statisticsNoRecompute = false;
	bool					_resumable = false;
	int16					_maxDop;
	int8					_dataCompression;
	_tstring				_dataCompressionDesc;
	bool					_xmlCompression = false;
	_tstring				_xmlCompressionDesc;
	_tstring				_dataSpaceName;
	_tstring				_dataSpaceTypeDesc;
};

class Index
{
public:
	_tstring			CreateIndex(EDBClass dbClass);
	_tstring			DropIndex(EDBClass dbClass);

	_tstring			GetIndexName(EDBClass dbClass);
	_tstring			GetKindText();
	_tstring			GetTypeText();
	_tstring			GetKeyText();
	_tstring			CreateColumnsText(EDBClass dbClass);
	bool				DependsOn(const _tstring& columnName);

public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_name;			
	int32					_indexId = 0;		
	EIndexKind				_kind = EIndexKind::NONCLUSTERED;
	EIndexType				_type = EIndexType::NONE;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	CVector<IndexColumnRef>	_columns;
};

class ForeignKey
{
public:
	_tstring			CreateForeignKey(EDBClass dbClass);
	_tstring			DropForeignKey(EDBClass dbClass);

	_tstring			GetForeignKeyName(EDBClass dbClass);
	_tstring			CreateColumnsText(EDBClass dbClass, CVector<IndexColumnRef> columns);

public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_foreignKeyName;
	_tstring				_updateRule;
	_tstring				_deleteRule;

	_tstring				_foreignKeyTableName;
	CVector<IndexColumnRef>	_foreignKeyColumns;
	_tstring				_referenceKeyTableName;
	CVector<IndexColumnRef>	_referenceKeyColumns;
};

class Trigger
{
public:
	int32			_objectId = 0;
	_tstring		_schemaName;
	_tstring		_tableName;
	_tstring		_triggerName;
	_tstring		_fullBody;
};

class Table
{
public:
	_tstring			CreateTable(EDBClass dbClass);
	_tstring			DropTable(EDBClass dbClass);
	ColumnRef			FindColumn(const _tstring& columnName);

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_auto_increment_value;
	_tstring					_storageEngine;
	_tstring					_characterset;
	_tstring					_collation;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<ColumnRef>			_columns;
	CVector<IndexRef>			_indexes;
	CVector<IndexOptionRef>		_indexOptions;
	CVector<ForeignKeyRef>		_foreignKeys;
};

class ProcParam
{
public:
	_tstring			GetParameterModeText();

public:
	_tstring			_paramId;
	EParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_desc;
};

class Procedure
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_fullBody;
	_tstring					_body;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<ProcParamRef>		_parameters;
};

class FuncParam
{
public:
	_tstring			GetParameterModeText();

public:
	_tstring			_paramId;
	EParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_desc;
};

class Function
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_fullBody;
	_tstring					_body;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<FuncParamRef>		_parameters;
};

class Helpers
{
public:
	static EIndexKind		StringToIndexKind(const TCHAR* ptszIndexKind);
	static EIndexType		StringToIndexType(const TCHAR* ptszIndexType);
	static EIndexSort		StringToIndexSort(const TCHAR* ptszIndexSort);
	static EParameterMode	StringToParamMode(const _tstring str);
	static EDataType		StringToDataType(const TCHAR* str, OUT uint64& maxLen);
	static _tstring			DataTypeToString(EDataType type);
	static _tstring			DataTypeLengthToString(EDataType type, uint64 iLength);

	static _tstring			Format(const TCHAR* format, ...);
	static _tstring			RemoveWhiteSpace(const _tstring& str);
	static void				LogFileWrite(EDBClass dbClass, _tstring title, _tstring sql, bool newline = false);
};

NAMESPACE_END

#endif // ndef __DBMODEL_H__
