
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
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

enum class DataType
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

enum class ParameterMode : uint8
{
	PARAM_RETURN = 0,
	PARAM_IN = 1,
	PARAM_OUT = 2
};

//***************************************************************************
//
class TABLE_INFO
{
public:
	int32	ObjectId;										// MSSQL 테이블 고유번호
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX];			// 테이블 주석
	int64	AutoIncrementValue;								// Identity 생성된 마지막 값
	TCHAR	tszCreateDate[DATETIME_STRLEN];					// 생성 일시
	TCHAR	tszModifyDate[DATETIME_STRLEN];					// 수정 일시
};

//***************************************************************************
//
class COLUMN_INFO
{
public:
	int32	ObjectId;										// MSSQL 테이블 고유번호
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	int32	Seq;											// 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// 컬럼 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];		// 컬럼 데이터타입
	int16	MaxLength;										// 컬럼의 최대 길이(바이트)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];	// 컬럼 데이터타입 상세(Ex. VARCHAR[100])
	bool	IsNullable;										// NULL 허용 여부(true/false : 허용/비허용)
	bool	IsIdentity;										// Identity 값 여부(true/false : 유/무)
	uint64	SeedValue;										// Identity 시드 값
	uint64  IncValue;										// Identity 증분 값
	TCHAR   tszDefaultDefinition[DATABASE_WVARCHAR_MAX];	// 기본값 정의
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX];		// 컬럼 주석
};

//***************************************************************************
//
class INDEX_INFO
{
public:
	int32	ObjectId;										// MSSQL 테이블 고유번호
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
class FOREIGNKEYS_INFO
{
public:
	int32	ObjectId;												// MSSQL 테이블 고유번호
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
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// 테이블 명
	TCHAR   tszTriggerName[DATABASE_OBJECT_NAME_STRLEN];			// 트리거 명
};

//***************************************************************************
//
class PROCEDURE_INFO
{
public:
	int32	ObjectId;												// MSSQL 저장프로시저 고유번호
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
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN];				// 저장프로시저 명
	int32	Seq;													// 파라미터 순서
	int8	ParamMode;												// 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// 파라미터 데이터타입
	int16	MaxLength;												// 파라미터의 최대 길이(바이트)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// 파라미터 데이터타입 상세(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// 파라미터 주석
};

//***************************************************************************
//
class FUNCTION_INFO
{
public:
	int32	ObjectId;												// MSSQL 함수 고유번호
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
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN];				// 함수 명
	int32	Seq;													// 파라미터 순서
	int8	ParamMode;												// 파라미터 입출력 구분(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// 파라미터 명
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// 파라미터 데이터타입
	int16	MaxLength;												// 파라미터의 최대 길이(바이트)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// 파라미터 데이터타입 상세(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// 파라미터 주석
};

class Column
{
public:
	_tstring			CreateColumn(DB_CLASS dbClass);
	_tstring			ModifyColumnType(DB_CLASS dbClass);
	_tstring			DropColumn(DB_CLASS dbClass);
	_tstring			CreateText(DB_CLASS dbClass);
	_tstring			CreateDefaultConstraint(DB_CLASS dbClass);
	_tstring			DropDefaultConstraint(DB_CLASS dbClass);

public:
	_tstring			_tableName;
	_tstring			_seq;
	_tstring			_name;
	_tstring			_desc;
	_tstring			_datatype;
	int32				_maxLength = 0;
	_tstring			_datatypedesc;
	bool				_nullable = false;
	bool				_identity = false;
	_tstring			_identitydesc;
	int64				_seedValue = 0;
	int64				_incrementValue = 0;
	_tstring			_default;
	_tstring			_defaultConstraintName;
};

enum class IndexType
{
	None = 0,
	PrimaryKey = 1,
	Unique,
	Index,
	Fulltext,
	Spatial
};

enum class IndexKind
{
	None = 0,
	Clustered = 1,
	NonClustered = 2
};

class IndexColumn
{
public:
	_tstring	_seq;
	_tstring	_name;
	int8		_sort = 0;
};

class Index
{
public:
	_tstring			CreateIndex(DB_CLASS dbClass);
	_tstring			DropIndex(DB_CLASS dbClass);

	_tstring			GetIndexName(DB_CLASS dbClass);
	_tstring			GetKindText();
	_tstring			GetTypeText();
	_tstring			GetKeyText();
	_tstring			CreateColumnsText(DB_CLASS dbClass);
	bool				DependsOn(const _tstring& columnName);

public:
	_tstring				_tableName;
	_tstring				_name;			
	int32					_indexId = 0;		
	IndexKind				_kind = IndexKind::NonClustered;
	IndexType				_type = IndexType::None;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	CVector<IndexColumnRef>	_columns;
};

class ForeignKey
{
public:
	_tstring			CreateForeignKey(DB_CLASS dbClass);
	_tstring			DropForeignKey(DB_CLASS dbClass);

	_tstring			GetForeignKeyName(DB_CLASS dbClass);
	_tstring			CreateColumnsText(DB_CLASS dbClass, CVector<IndexColumnRef> columns);

public:
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
	_tstring		_tableName;
	_tstring		_triggerName;
	_tstring		_fullBody;
};

class Table
{
public:
	_tstring			CreateTableDesc(DB_CLASS dbClass);
	_tstring			CreateTable(DB_CLASS dbClass);
	_tstring			DropTable(DB_CLASS dbClass);
	ColumnRef			FindColumn(const _tstring& columnName);

public:
	int32						_objectId = 0;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_auto_increment_value;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<ColumnRef>			_columns;
	CVector<IndexRef>			_indexes;
	CVector<ForeignKeyRef>		_foreignKeys;
};

class ProcParam
{
public:
	_tstring			_paramId;
	ParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	int32				_maxLength = 0;
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
	_tstring			_paramId;
	ParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	int32				_maxLength = 0;
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
	static IndexKind		StringToIndexKind(const TCHAR* ptszIndexKind);
	static IndexType		StringToIndexType(const TCHAR* ptszIndexType);
	static _tstring			Format(const TCHAR* format, ...);
	static _tstring			DataType2String(DataType type);
	static _tstring			RemoveWhiteSpace(const _tstring& str);
	static ParameterMode	StringToParamMode(const _tstring str);
	static DataType			StringToDataType(const TCHAR* str, OUT int32& maxLen);
	static _tstring			StringToDataTypeLength(DataType type, int32 iLength);
	static void				LogFileWrite(DB_CLASS dbClass, _tstring title, _tstring sql, bool newline = false);
};

NAMESPACE_END

#endif // ndef __DBMODEL_H__
