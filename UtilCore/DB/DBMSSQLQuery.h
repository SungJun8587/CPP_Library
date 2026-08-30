
//***************************************************************************
// DBMSSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef UC_DBMSSQLQUERY_H
#define UC_DBMSSQLQUERY_H

//***************************************************************************
// @brief MSSQL 인덱스 조각화 관리 옵션 열거형
//***************************************************************************
enum class EMSSQLIndexFragmentation
{
	REORGANIZE,
	REBUILD,
	DISABLE
};

//***************************************************************************
// @brief MSSQL 인덱스 조각화 옵션을 문자열로 변환하는 함수
// @param v MSSQL 인덱스 조각화 옵션
// @return 해당 옵션의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLIndexFragmentation v)
{
	switch( v )
	{
	case EMSSQLIndexFragmentation::REORGANIZE:	return _T("REORGANIZE");
	case EMSSQLIndexFragmentation::REBUILD:		return _T("REBUILD");
	case EMSSQLIndexFragmentation::DISABLE:		return _T("DISABLE");
	default:									return _T("");
	}
}


//***************************************************************************
// @brief MSSQL 확장 속성 Level 0 개체 유형 열거형
//***************************************************************************
enum class EMSSQLExtendedPropertyLevel0Type
{
	ASSEMBLY,
	CONTRACT,
	EVENT__NOTIFICATION,
	FILEGROUP,
	MESSAGE__TYPE,
	PARTITION__FUNCTION,
	PARTITION__SCHEME,
	REMOTE__SERVICE__BINDING,
	ROUTE,
	SCHEMA,
	SERVICE,
	USER,
	TRIGGER,
	TYPE,
	PLAN__GUIDE,
	NONE
};

//***************************************************************************
// @brief MSSQL 확장 속성 Level 0 개체 유형을 문자열로 변환하는 함수
// @param v Level 0 개체 유형
// @return 해당 개체 유형의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLExtendedPropertyLevel0Type v)
{
	switch( v )
	{
	case EMSSQLExtendedPropertyLevel0Type::ASSEMBLY:					return _T("ASSEMBLY");
	case EMSSQLExtendedPropertyLevel0Type::CONTRACT:					return _T("CONTRACT");
	case EMSSQLExtendedPropertyLevel0Type::EVENT__NOTIFICATION:			return _T("EVENT NOTIFICATION");
	case EMSSQLExtendedPropertyLevel0Type::FILEGROUP:					return _T("FILEGROUP");
	case EMSSQLExtendedPropertyLevel0Type::MESSAGE__TYPE:				return _T("MESSAGE TYPE");
	case EMSSQLExtendedPropertyLevel0Type::PARTITION__FUNCTION:			return _T("PARTITION FUNCTION");
	case EMSSQLExtendedPropertyLevel0Type::PARTITION__SCHEME:			return _T("PARTITION SCHEME");
	case EMSSQLExtendedPropertyLevel0Type::REMOTE__SERVICE__BINDING:	return _T("REMOTE SERVICE BINDING");
	case EMSSQLExtendedPropertyLevel0Type::ROUTE:						return _T("ROUTE");
	case EMSSQLExtendedPropertyLevel0Type::SCHEMA:						return _T("SCHEMA");
	case EMSSQLExtendedPropertyLevel0Type::SERVICE:						return _T("SERVICE");
	case EMSSQLExtendedPropertyLevel0Type::USER:						return _T("USER");
	case EMSSQLExtendedPropertyLevel0Type::TRIGGER:						return _T("TRIGGER");
	case EMSSQLExtendedPropertyLevel0Type::TYPE:						return _T("TYPE");
	case EMSSQLExtendedPropertyLevel0Type::PLAN__GUIDE:					return _T("PLAN GUIDE");
	default:															return _T("NONE");
	}
}


//***************************************************************************
// @brief MSSQL 확장 속성 Level 1 개체 유형 열거형
//***************************************************************************
enum class EMSSQLExtendedPropertyLevel1Type
{
	AGGREGATE,
	DEFAULT,
	FUNCTION,
	LOGICAL__FILE__NAME,
	PROCEDURE,
	QUEUE,
	RULE,
	SEQUENCE,
	SYNONYM,
	TABLE,
	TABLE_TYPE,
	TYPE,
	VIEW,
	XML__SCHEMA__COLLECTION,
	NONE
};

//***************************************************************************
// @brief MSSQL 확장 속성 Level 1 개체 유형을 문자열로 변환하는 함수
// @param v Level 1 개체 유형
// @return 해당 개체 유형의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLExtendedPropertyLevel1Type v)
{
	switch( v )
	{
	case EMSSQLExtendedPropertyLevel1Type::AGGREGATE:					return _T("AGGREGATE");
	case EMSSQLExtendedPropertyLevel1Type::DEFAULT:						return _T("DEFAULT");
	case EMSSQLExtendedPropertyLevel1Type::FUNCTION:					return _T("EVENT FUNCTION");
	case EMSSQLExtendedPropertyLevel1Type::LOGICAL__FILE__NAME:			return _T("LOGICAL FILE NAME");
	case EMSSQLExtendedPropertyLevel1Type::PROCEDURE:					return _T("PROCEDURE");
	case EMSSQLExtendedPropertyLevel1Type::QUEUE:						return _T("QUEUE");
	case EMSSQLExtendedPropertyLevel1Type::RULE:						return _T("RULE");
	case EMSSQLExtendedPropertyLevel1Type::SEQUENCE:					return _T("SEQUENCE");
	case EMSSQLExtendedPropertyLevel1Type::SYNONYM:						return _T("SYNONYM");
	case EMSSQLExtendedPropertyLevel1Type::TABLE:						return _T("TABLE");
	case EMSSQLExtendedPropertyLevel1Type::TABLE_TYPE:					return _T("TABLE_TYPE");
	case EMSSQLExtendedPropertyLevel1Type::TYPE:						return _T("TYPE");
	case EMSSQLExtendedPropertyLevel1Type::VIEW:						return _T("VIEW");
	case EMSSQLExtendedPropertyLevel1Type::XML__SCHEMA__COLLECTION:		return _T("XML SCHEMA COLLECTION");
	default:															return _T("NONE");
	}
}


//***************************************************************************
// @brief MSSQL 확장 속성 Level 2 개체 유형 열거형
//***************************************************************************
enum class EMSSQLExtendedPropertyLevel2Type
{
	COLUMN,
	CONSTRAINT,
	EVENT__NOTIFICATION,
	INDEX,
	PARAMETER,
	TRIGGER,
	NONE
};

//***************************************************************************
// @brief MSSQL 확장 속성 Level 2 개체 유형을 문자열로 변환하는 함수
// @param v Level 2 개체 유형
// @return 해당 개체 유형의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLExtendedPropertyLevel2Type v)
{
	switch( v )
	{
	case EMSSQLExtendedPropertyLevel2Type::COLUMN:					return _T("COLUMN");
	case EMSSQLExtendedPropertyLevel2Type::CONSTRAINT:				return _T("CONSTRAINT");
	case EMSSQLExtendedPropertyLevel2Type::EVENT__NOTIFICATION:		return _T("EVENT NOTIFICATION");
	case EMSSQLExtendedPropertyLevel2Type::INDEX:					return _T("INDEX");
	case EMSSQLExtendedPropertyLevel2Type::PARAMETER:				return _T("PARAMETER");
	case EMSSQLExtendedPropertyLevel2Type::TRIGGER:					return _T("TRIGGER");
	default:														return _T("NONE");
	}
}


//***************************************************************************
// @brief MSSQL 이름 변경 개체 유형 열거형
//***************************************************************************
enum class EMSSQLRenameObjectType : unsigned char
{
	NONE = 0,
	COLUMN,
	DATABASE,
	INDEX,
	OBJECT,
	STATISTICS,
	USERDATATYPE
};

//***************************************************************************
// @brief MSSQL 이름 변경 개체 유형을 문자열로 변환하는 함수
// @param v 이름 변경 개체 유형
// @return 해당 개체 유형의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLRenameObjectType v)
{
	switch( v )
	{
	case EMSSQLRenameObjectType::COLUMN:		return _T("COLUMN");
	case EMSSQLRenameObjectType::DATABASE:		return _T("DATABASE");
	case EMSSQLRenameObjectType::INDEX:			return _T("INDEX");
	case EMSSQLRenameObjectType::OBJECT:		return _T("OBJECT");
	case EMSSQLRenameObjectType::STATISTICS:	return _T("STATISTICS");
	case EMSSQLRenameObjectType::USERDATATYPE:	return _T("USERDATATYPE");
	default:									return _T("NONE");
	}
}


//***************************************************************************
// @brief MSSQL 인덱스 타입 열거형 (sys.indexes 테이블 type_desc 컬럼 참조)
//***************************************************************************
enum EMSSQLIndexType
{
	HEAP = 0,
	CLUSTERED = 1,
	NONCLUSTERED = 2,
	XML = 3,
	SPATIAL = 4,
	CLUSTERED__COLUMNSTORE = 5,
	NONCLUSTERED__COLUMNSTORE = 6,
	NONCLUSTERED__HASH = 7
};

//***************************************************************************
// @brief MSSQL 인덱스 타입을 문자열로 변환하는 함수
// @param v 인덱스 타입
// @return 해당 인덱스 타입의 문자열 표현
//***************************************************************************
inline const TCHAR* ToString(EMSSQLIndexType v)
{
	switch( v )
	{
	case EMSSQLIndexType::HEAP:							return _T("HEAP");
	case EMSSQLIndexType::CLUSTERED:					return _T("CLUSTERED");
	case EMSSQLIndexType::NONCLUSTERED:					return _T("NONCLUSTERED");
	case EMSSQLIndexType::XML:							return _T("XML");
	case EMSSQLIndexType::SPATIAL:						return _T("SPATIAL");
	case EMSSQLIndexType::CLUSTERED__COLUMNSTORE:		return _T("CLUSTERED COLUMNSTORE");
	case EMSSQLIndexType::NONCLUSTERED__COLUMNSTORE:	return _T("NONCLUSTERED COLUMNSTORE");
	case EMSSQLIndexType::NONCLUSTERED__HASH:			return _T("NONCLUSTERED HASH");
	default:											return _T("");
	}
}

//***************************************************************************
// @brief 문자열을 MSSQL 인덱스 타입 열거형으로 변환하는 함수
// @param ptszIndexType 인덱스 타입 문자열
// @return 변환된 EMSSQLIndexType 열거형 값
//***************************************************************************
inline const EMSSQLIndexType StringToMSSQLIndexType(const TCHAR* ptszIndexType)
{
	if( ::_tcsicmp(ptszIndexType, _T("HEAP")) == 0 )
		return EMSSQLIndexType::HEAP;
	else if( ::_tcsicmp(ptszIndexType, _T("CLUSTERED")) == 0 )
		return EMSSQLIndexType::CLUSTERED;
	else if( ::_tcsicmp(ptszIndexType, _T("NONCLUSTERED")) == 0 )
		return EMSSQLIndexType::NONCLUSTERED;
	else if( ::_tcsicmp(ptszIndexType, _T("XML")) == 0 )
		return EMSSQLIndexType::XML;
	else if( ::_tcsicmp(ptszIndexType, _T("SPATIAL")) == 0 )
		return EMSSQLIndexType::SPATIAL;
	else if( ::_tcsicmp(ptszIndexType, _T("CLUSTERED COLUMNSTORE")) == 0 )
		return EMSSQLIndexType::CLUSTERED__COLUMNSTORE;
	else if( ::_tcsicmp(ptszIndexType, _T("NONCLUSTERED COLUMNSTORE")) == 0 )
		return EMSSQLIndexType::NONCLUSTERED__COLUMNSTORE;
	else if( ::_tcsicmp(ptszIndexType, _T("NONCLUSTERED HASH")) == 0 )
		return EMSSQLIndexType::NONCLUSTERED__HASH;

	return EMSSQLIndexType::HEAP;
}

//***************************************************************************
// @brief MSSQL 확장 속성 처리 파라미터 클래스
//***************************************************************************
class MSSQL_ExtendedProperty
{
public:
	_tstring _propertyName;			// 속성의 이름
	_tstring _propertyValue;		// 속성과 연결할 값
	_tstring _level0_object_type;	// 수준 0 개체의 유형
	_tstring _level0_object_name;	// 수준 0 개체 형식의 이름
	_tstring _level1_object_type;	// 수준 1 개체의 유형
	_tstring _level1_object_name;	// 수준 1 개체 형식의 이름
	_tstring _level2_object_type;	// 수준 2 개체의 유형
	_tstring _level2_object_name;	// 수준 2 개체 형식의 이름
};

//***************************************************************************
// @brief MSSQL 인덱스 조각화 정보 클래스
//***************************************************************************
class MSSQL_INDEX_FRAGMENTATION
{
public:
	int32	ObjectId;														// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };			// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };				// 테이블 명
	int32	IndexId;														// 인덱스 고유번호
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// 인덱스 명
	TCHAR   tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };		// 인덱스 타입
	int32   PartitionNum;													// 개체의 파티션 번호
	float	AvgFragmentationInPercent;										// 논리적 조각화(인덱스에서 순서가 잘못된 페이지) 수치
	float	AvgPageSpaceUsedInPercent;										// 평균 페이지 밀도
	int32	PageCount;														// 총 인덱스 또는 데이터 페이지 수
	TCHAR	tszAllocUnitTypeDesc[DATABASE_WVARCHAR_MAX] = { 0, };			// 할당 단위 유형에 대한 설명
};

//***************************************************************************
// @brief MSSQL 인덱스 옵션 정보 클래스
//***************************************************************************
class MSSQL_INDEX_OPTION_INFO
{
public:
	int32	ObjectId;															// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];							// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];							// 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];							// 인덱스 명
	int32	IndexId;															// 인덱스 고유번호
	bool    IsPrimaryKey;														// 기본키 여부(true/false)
	bool    IsUnique;															// 유니크 여부(true/false)
	bool	IsDisabled;															// 인덱스 비활성화 여부(0/1)
	bool	IsPadded;															// 인덱스 패딩 지정 여부 (PAD_INDEX)
	int8	FillFactor;															// 각 인덱스 페이지 리프 수준 채우기 비율 (FILLFACTOR)
	bool	IgnoreDupKey;														// 중복 키 삽입 경고/오류 응답 유형 (IGNORE_DUP_KEY)
	bool	AllowRowLocks;														// 행 잠금 허용 여부 (ALLOW_ROW_LOCKS)
	bool	AllowPageLocks;														// 페이지 잠금 허용 여부 (ALLOW_PAGE_LOCKS)
	bool	HasFilter;															// 인덱스 필터 존재 여부(0/1)
	TCHAR   tszFilterDefinition[DATABASE_WVARCHAR_MAX];							// 필터링된 인덱스 정의 식
	int32	CompressionDelay;													// Columnstore 인덱스 압축 지연 시간(분)
	bool	OptimizeForSequentialKey;											// 마지막 페이지 삽입 경합 최적화 여부
	bool	StatisticsNoRecompute;												// 통계 재계산 여부 (STATISTICS_NORECOMPUTE)
	bool	StatisticsIncremental;												// 증분 통계 생성 여부 (STATISTICS_INCREMENTAL)
	int8	DataCompression;													// 파티션 압축 상태 코드 (0:NONE, 1:ROW, 2:PAGE 등)
	TCHAR   tszDataCompressionDesc[DATABASE_BASE_STRLEN];						// 파티션 압축 상태 설명 문자열
	bool	XmlCompression;														// XML 압축 상태 여부
	TCHAR   tszXmlCompressionDesc[DATABASE_BASE_STRLEN];						// XML 압축 상태 설명 문자열
	TCHAR   tszFileGroupOrPartitionScheme[DATABASE_OBJECT_TYPE_DESC_STRLEN];	// 데이터 공간의 유형 명칭
	TCHAR   tszFileGroupOrPartitionSchemeName[DATABASE_OBJECT_NAME_STRLEN];		// 데이터 공간의 이름
	bool    SortInTempDB;														// tempdb 정렬 결과 저장 여부 (SORT_IN_TEMPDB)
	bool    Online;																// 온라인 인덱스 작업 진행 여부 (ONLINE)
	uint8   MaxDop;																// 최대 병렬 처리 정도 (MAXDOP)
	bool    Resumable;															// 다시 시작 가능한 인덱스 작업 여부 (RESUMABLE)
	int32   MaxDuration;														// 인덱스 재구성 작업 최대 지속 시간(분)
	int8    AbortAfterWait;														// 잠금 대기 차단 시 중단 옵션 (ABORT_AFTER_WAIT)
	bool    LobCompaction;														// LOB 데이터 압축 여부 (LOB_COMPACTION)
	bool    CompressAllRowGroups;												// 모든 rowgroup 압축 강제 여부
};

//***************************************************************************
// @brief MSSQL 기본값 제약조건 정보 클래스
//***************************************************************************
class MSSQL_DEFAULT_CONSTRAINT_INFO
{
public:
	int32	ObjectId;												// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// 테이블 명
	TCHAR   tszDefaultConstName[DATABASE_OBJECT_NAME_STRLEN];		// 기본값 제약조건 명
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];				// 컬럼 명
	TCHAR   tszDefaultValue[DATABASE_WVARCHAR_MAX];					// 컬럼 제약조건 정의값
	bool	IsSystemNamed;											// 시스템이 인덱스명을 할당했는지 여부(true/false)
};

//***************************************************************************
// @brief 테이블 컬럼 옵션 절 구문을 생성하는 함수
// @param dataTypeDesc 데이터 타입 설명
// @param isNullable NULL 허용 여부
// @param isIdentity IDENTITY 속성 여부
// @param seedValue IDENTITY 시드값
// @param incrementValue IDENTITY 증분값
// @param collation 데이터 정렬 방식
// @return 생성된 테이블 컬럼 옵션 문자열
//***************************************************************************
inline _tstring MSSQLGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, bool isIdentity, ulong seedValue, ulong incrementValue, _tstring collation = _T(""))
{
	_tstring columnOption = _T("");

	columnOption = dataTypeDesc;
	if( collation != "" )
		columnOption = columnOption + " COLLATE " + collation;

	columnOption = columnOption + (isNullable ? " NULL" : " NOT NULL") + (isIdentity ? tstring_tcformat(_T(" IDENTITY(%ld,%ld)"), seedValue, incrementValue) : _T(""));

	return columnOption;
}

//***************************************************************************
// @brief 제약조건 삭제 쿼리를 생성하는 함수
// @param schemaName 스키마 명
// @param tableName 테이블 명
// @param constType 제약조건 타입
// @param constName 제약조건 명
// @return 생성된 제약조건 삭제 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDropConstraintQuery(_tstring schemaName, _tstring tableName, _tstring constType, _tstring constName)
{
	_tstring query = _T("");

	if( constType == "PK" || constType == "UQ" || constType == "F" || constType == "D" || constType == "C" )
	{
		query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] DROP CONSTRAINT [%s]"), schemaName.c_str(), tableName.c_str(), constName.c_str());
	}
	return query;
}

//***************************************************************************
// @brief 시스템 DB 정보 조회 쿼리를 반환하는 함수
// @return DB 시스템 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDBSystemQuery()
{
	_tstring query = _T("");

	query = query + "SELECT CONCAT('MSSQL ', CAST(SERVERPROPERTY('PRODUCTVERSION') AS VARCHAR(50))) AS [version], @@LANGUAGE AS [characterset], collation_name AS [collation]";
	query = query + "\n" + "FROM sys.databases";
	query = query + "\n" + "WHERE name = DB_NAME()";
	return query;
}

//***************************************************************************
// @brief 시스템 데이터 타입 정보 조회 쿼리를 반환하는 함수
// @return 데이터 타입 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDBSystemDataTypeQuery()
{
	_tstring query = _T("");

	query = query + "SELECT system_type_id, UPPER([name]) AS datatype, max_length, [precision], scale, collation_name, is_nullable";
	query = query + "\n" + "FROM sys.types";
	query = query + "\n" + "WHERE system_type_id = user_type_id";
	query = query + "\n" + "ORDER BY [name] ASC";
	return query;
}

//***************************************************************************
// @brief 사용자 목록 조회 쿼리를 반환하는 함수
// @return 사용자 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetUserListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT [name] FROM sys.server_principals WHERE is_disabled = 0 AND type IN ('S')";
	return query;
}

//***************************************************************************
// @brief 데이터베이스 목록 조회 쿼리를 반환하는 함수
// @return 데이터베이스 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDatabaseListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT [name] FROM sys.databases WHERE [name] NOT IN('model', 'msdb', 'pubs', 'Northwind', 'tempdb') ORDER BY [name] ASC";
	return query;
}

//***************************************************************************
// @brief 데이터베이스 백업 쿼리를 생성하는 함수
// @param databaseName 백업 대상 데이터베이스 명
// @param backupFilePath 백업 파일 경로
// @return 데이터베이스 백업 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDatabaseBackupQuery(_tstring databaseName, _tstring backupFilePath)
{
	_tstring query = _T("");

	query = query + "BACKUP DATABASE " + databaseName + " TO DISK = '" + backupFilePath + "'";
	return query;
}

//***************************************************************************
// @brief 데이터베이스 복원 (파일 목록 확인) 쿼리를 생성하는 함수
// @param databaseName 복원 대상 데이터베이스 명
// @param restoreFilePath 백업 파일 경로
// @return 데이터베이스 복원 확인 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath)
{
	_tstring query = _T("");

	query = query + "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	return query;
}

//***************************************************************************
// @brief 데이터베이스 복원 쿼리를 생성하는 함수 (경로 이동 포함)
// @param databaseName 복원 대상 데이터베이스 명
// @param restoreFilePath 백업 파일 경로
// @param dataFilePath 복원할 데이터 파일 경로
// @param logFilePath 복원할 로그 파일 경로
// @return 데이터베이스 복원 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath, _tstring dataFilePath, _tstring logFilePath)
{
	_tstring query = _T("");

	query = query + "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "'";
	query = query + " WITH MOVE '" + databaseName + " TO '" + dataFilePath + "'";
	query = query + ", MOVE '" + databaseName + "_log' TO '" + logFilePath + "'";
	return query;
}

//***************************************************************************
// @brief RowStore 인덱스 조각화 상태 확인 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 조각화 상태 확인 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetRowStoreIndexFragmentationCheckQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT ips.object_id AS object_id, OBJECT_SCHEMA_NAME(ips.object_id) AS schema_name, OBJECT_NAME(ips.object_id) AS table_name, ips.index_id AS index_id, ISNULL(i.name, '') AS index_name, i.type_desc AS index_type, ips.partition_number AS partitionnum, ips.avg_fragmentation_in_percent, ips.avg_page_space_used_in_percent, ips.page_count, ips.alloc_unit_type_desc";
	query = query + "\n" + "FROM sys.dm_db_index_physical_stats(DB_ID(), NULL, NULL, NULL, 'SAMPLED') AS ips";
	query = query + "\n" + "INNER JOIN sys.indexes AS i";
	query = query + "\n" + "ON ips.object_id = i.object_id AND ips.index_id = i.index_id";
	if( tableName != "" )
		query = query + "\n" + "WHERE OBJECT_NAME(ips.object_id) = '" + tableName + "'";
	query = query + "\n" + "ORDER BY ips.avg_fragmentation_in_percent DESC";

	return query;
}

//***************************************************************************
// @brief ColumnStore 인덱스 조각화 상태 확인 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 조각화 상태 확인 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetColumnStoreIndexFragmentationCheckQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, OBJECT_NAME(i.object_id) AS table_name, i.index_id AS index_id, ISNULL(i.name, '') AS index_name, i.type_desc AS index_type, 100.0 * (ISNULL(SUM(rgs.deleted_rows), 0)) / NULLIF(SUM(rgs.total_rows), 0) AS avg_fragmentation_in_percent";
	query = query + "\n" + "FROM sys.indexes AS i";
	query = query + "\n" + "INNER JOIN sys.dm_db_column_store_row_group_physical_stats AS rgs";
	query = query + "\n" + "ON i.object_id = rgs.object_id AND i.index_id = rgs.index_id";
	if( tableName != "" )
	{
		query = query + "\n" + "WHERE OBJECT_NAME(i.object_id) = '" + tableName + "' AND rgs.state_desc = 'COMPRESSED'";
		query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
		query = query + "\n" + "ORDER BY index_name, index_type, avg_fragmentation_in_percent DESC";
	}
	else
	{
		query = query + "\n" + "WHERE rgs.state_desc = 'COMPRESSED'";
		query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
		query = query + "\n" + "ORDER BY table_name, index_name, index_type, avg_fragmentation_in_percent DESC";
	}
	return query;
}

//***************************************************************************
// @brief 인덱스 옵션 설정(SET) 쿼리를 생성하는 함수
// @param schemaName 스키마 명
// @param tableName 테이블 명
// @param indexName 인덱스 명 (빈 값일 경우 ALL 적용)
// @param indexOptions 설정할 인덱스 옵션 맵
// @return 인덱스 옵션 설정 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLIndexOptionSetQuery(_tstring schemaName, _tstring tableName, _tstring indexName, unordered_map<_tstring, _tstring> indexOptions)
{
	int32 i = 0;
	_tstring query = _T("");

	if( indexName != "" )
		query = tstring_tcformat(_T("ALTER INDEX [%s] ON [%s].[%s]"), indexName.c_str(), schemaName.c_str(), tableName.c_str());
	else query = tstring_tcformat(_T("ALTER INDEX %s ON [%s].[%s]"), _T("ALL"), schemaName.c_str(), tableName.c_str());

	if( !indexOptions.empty() )
	{
		query = query + "\r\n" + "SET (";
		for( auto indexOption = indexOptions.begin(); indexOption != indexOptions.end(); indexOption++ )
		{
			if( i > 0 ) query = query + ", ";

			query = query + "\r\n\t" + indexOption->first + " = " + indexOption->second;
			i++;
		}
		query = query + "\r\n" + ");";
	}
	return query;
}

//***************************************************************************
// @brief 옵션 없이 인덱스 상태 변경(REORGANIZE, REBUILD, DISABLE) 쿼리를 생성하는 함수
// @param schemaName 스키마 명
// @param tableName 테이블 명
// @param indexName 인덱스 명 (빈 값일 경우 ALL 적용)
// @param eMSSQLIndexFragmentation 인덱스 변경 동작 타입
// @return 인덱스 변경 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLAlterIndexFragmentationNonOptionQuery(_tstring schemaName, _tstring tableName, _tstring indexName, EMSSQLIndexFragmentation eMSSQLIndexFragmentation)
{
	_tstring query = _T("");

	if( indexName != "" )
		query = tstring_tcformat(_T("ALTER INDEX [%s] ON [%s].[%s] %s"), indexName.c_str(), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
	else query = tstring_tcformat(_T("ALTER INDEX %s ON [%s].[%s] %s"), _T("ALL"), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
	return query;
}

//***************************************************************************
// @brief 옵션을 포함한 인덱스 상태 변경 쿼리를 생성하는 함수
// @param schemaName 스키마 명
// @param tableName 테이블 명
// @param indexName 인덱스 명 (빈 값일 경우 ALL 적용)
// @param eMSSQLIndexFragmentation 인덱스 변경 동작 타입
// @param indexOptions 추가 적용할 인덱스 옵션 맵
// @return 옵션이 포함된 인덱스 변경 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLAlterIndexFragmentationOptionQuery(_tstring schemaName, _tstring tableName, _tstring indexName, EMSSQLIndexFragmentation eMSSQLIndexFragmentation, unordered_map<_tstring, _tstring> indexOptions)
{
	int i = 0;
	_tstring query = _T("");

	if( indexName != "" )
		query = tstring_tcformat(_T("ALTER INDEX [%s] ON [%s].[%s]"), indexName.c_str(), schemaName.c_str(), tableName.c_str());
	else query = tstring_tcformat(_T("ALTER INDEX %s ON [%s].[%s]"), _T("ALL"), schemaName.c_str(), tableName.c_str());

	if( !indexOptions.empty() )
	{
		query = query + "\r\n" + _tstring(ToString(eMSSQLIndexFragmentation)) + " WITH (";
		for( auto indexOption = indexOptions.begin(); indexOption != indexOptions.end(); indexOption++ )
		{
			if( i > 0 ) query = query + ", ";

			query = query + "\r\n\t" + indexOption->first + " = " + indexOption->second;
			i++;
		}
		query = query + "\r\n" + ");";
	}
	return query;
}

//***************************************************************************
// @brief 정렬 규칙 도움말 조회 쿼리를 반환하는 함수
// @return 정렬 규칙 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetHelpCollationsQuery()
{
	_tstring query = _T("");

	query = query + "SELECT name FROM sys.fn_helpcollations()";
	return query;
}

//***************************************************************************
// @brief 개체 텍스트 도움말(sp_helptext) 조회 쿼리를 생성하는 함수
// @param dbObject DB 개체 유형
// @return 도움말 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetHelpTextQuery(EDBObjectType dbObject)
{
	_tstring query = _T("");

	switch( dbObject )
	{
	case EDBObjectType::PROCEDURE:
	case EDBObjectType::FUNCTION:
	case EDBObjectType::TRIGGERS:
	case EDBObjectType::EVENTS:
		query = query + "EXEC sp_helptext ?";
		break;
	}
	return query;
}

//***************************************************************************
// @brief 개체 이름 변경(sp_rename) 쿼리를 생성하는 함수
// @param objectName 기존 개체 이름
// @param chgObjectName 변경할 개체 이름
// @param renameObjectType 변경 대상 개체 유형
// @return 개체 이름 변경 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetRenameObjectQuery(_tstring objectName, _tstring chgObjectName, EMSSQLRenameObjectType renameObjectType = EMSSQLRenameObjectType::NONE)
{
	_tstring query = _T("");

	switch( renameObjectType )
	{
	case EMSSQLRenameObjectType::NONE:
		query = tstring_tcformat(_T("EXEC sp_rename '%s', '%s'"), objectName.c_str(), chgObjectName.c_str());
		break;
	default:
		query = tstring_tcformat(_T("EXEC sp_rename '%s', '%s', '%s'"), objectName.c_str(), chgObjectName.c_str(), ToString(renameObjectType));
		break;
	}
	return query;
}

//***************************************************************************
// @brief 사용자 테이블 목록 조회 쿼리를 반환하는 함수
// @return 테이블 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetTableListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, 1 AS object_type, name AS object_name";
	query = query + "\n" + "FROM sys.tables";
	query = query + "\n" + "WHERE type = 'U'";
	query = query + "\n" + "ORDER BY name ASC;";
	return query;
}

//***************************************************************************
// @brief 테이블 상세 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 테이블 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetTableInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, t.object_id AS object_id, SCHEMA_NAME(t.schema_id) AS schema_name, t.name AS table_name, ";
	query = query + "ISNULL((SELECT CAST(ISNULL(last_value, 0) AS BIGINT) FROM sys.identity_columns WHERE object_id = t.object_id AND last_value > 0), 0) AS auto_increment, ";
	query = query + "'' AS engine, '' AS characterset, '' AS collation, ";
	query = query + "CAST(ep.[value] AS NVARCHAR(4000)) AS table_comment, CONVERT(VARCHAR(23), create_date, 121) AS create_date, CONVERT(VARCHAR(23), modify_date, 121) AS modify_date";
	query = query + "\n" + "FROM sys.tables AS t";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON t.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE t.type = 'U' AND t.name = '" + tableName + "';";
	}
	else
	{
		query = query + "\n" + "WHERE t.type = 'U'";
		query = query + "\n" + "ORDER BY t.name ASC;";
	}

	return query;
}

//***************************************************************************
// @brief 테이블 컬럼 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 컬럼 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetTableColumnInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, col.object_id AS object_id, OBJECT_SCHEMA_NAME(col.object_id) AS schema_name, col.table_name AS table_name, col.column_id AS seq, col.name AS column_name, UPPER(TYPE_NAME(col.user_type_id)) AS datatype, ";
	query = query + "col.max_length, col.[precision], col.scale, ";
	query = query + "(UPPER(TYPE_NAME(col.user_type_id)) + (CASE WHEN TYPE_NAME(col.user_type_id) = 'varchar' OR TYPE_NAME(col.user_type_id) = 'char' THEN '(' ";
	query = query + "+ (CASE WHEN col.max_length = -1 THEN 'MAX' ELSE CAST(col.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(col.user_type_id) = 'nvarchar' ";
	query = query + "OR TYPE_NAME(col.user_type_id) = 'nchar' THEN '(' + (CASE WHEN col.max_length = -1 THEN 'MAX' ELSE CAST(col.max_length / 2 AS VARCHAR) END) + ')' ";
	query = query + "WHEN TYPE_NAME(col.user_type_id) = 'decimal' THEN '(' + CAST(col.precision AS VARCHAR) + ',' + CAST(col.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ";
	query = query + "col.is_nullable, col.is_identity, CAST(ISNULL(ic.seed_value, 0) AS BIGINT) AS seed_value, CAST(ISNULL(ic.increment_value, 0) AS BIGINT) AS inc_value, ";
	query = query + "dc.name AS default_constraintname, dc.definition AS default_definition, ";
	query = query + "'' AS characterset, col.collation_name AS collation, ";
	query = query + "CAST(ep.[value] AS NVARCHAR(4000)) AS column_comment";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.columns AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS col";
	query = query + "\n" + "LEFT OUTER JOIN sys.identity_columns AS ic";
	query = query + "\n" + "ON col.object_id = ic.object_id AND col.column_id = ic.column_id";
	query = query + "\n" + "LEFT OUTER JOIN sys.default_constraints AS dc";
	query = query + "\n" + "ON col.default_object_id = dc.object_id";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON col.object_id = ep.major_id AND col.column_id = ep.minor_id AND ep.class = 1 AND ep.name = 'MS_Description'";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY col.column_id ASC;";
	else query = query + "\n" + "ORDER BY col.table_name ASC, col.column_id ASC;";

	return query;
}

//***************************************************************************
// @brief 제약조건 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 제약조건 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetConstraintsInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	if( tableName != "" )
	{
		query = query + "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, OBJECT_NAME(const.parent_object_id) AS table_name, const.name AS const_name, const.type AS const_type, const.type_desc AS const_type_desc, const.const_value AS const_value, const.is_system_named AS is_system_named, 0 AS is_status, ";
		query = query + "(CASE const.type WHEN 'PK' THEN 1 WHEN 'UQ' THEN 2 WHEN 'F' THEN 3 WHEN 'D' THEN 4 WHEN 'C' THEN 5 END) AS sort_value";
		query = query + "\n" + "FROM";
		query = query + "\n" + "(";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, '' AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.key_constraints";
		query = query + "\n\t" + "WHERE OBJECT_NAME(parent_object_id) = '" + tableName + "'";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, '' AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.foreign_keys";
		query = query + "\n\t" + "WHERE OBJECT_NAME(parent_object_id) = '" + tableName + "'";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, definition AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.default_constraints";
		query = query + "\n\t" + "WHERE OBJECT_NAME(parent_object_id) = '" + tableName + "'";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, definition AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.check_constraints";
		query = query + "\n\t" + "WHERE OBJECT_NAME(parent_object_id) = '" + tableName + "'";
		query = query + "\n" + ") AS const";
		query = query + "\n" + "ORDER BY sort_value ASC;";
	}
	else
	{
		query = query + "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, OBJECT_NAME(const.parent_object_id) AS table_name, const.name AS const_name, const.type AS const_type, const.type_desc AS const_type_desc, const.const_value AS const_value, const.is_system_named AS is_system_named, 0 AS is_status, ";
		query = query + "(CASE const.type WHEN 'PK' THEN 1 WHEN 'UQ' THEN 2 WHEN 'F' THEN 3 WHEN 'D' THEN 4 WHEN 'C' THEN 5 END) AS sort_value";
		query = query + "\n" + "FROM";
		query = query + "\n" + "(";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, '' AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.key_constraints";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, '' AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.foreign_keys";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, definition AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.default_constraints";
		query = query + "\n\t" + "UNION";
		query = query + "\n\t" + "SELECT object_id, parent_object_id, name, type, type_desc, definition AS const_value, is_system_named";
		query = query + "\n\t" + "FROM sys.check_constraints";
		query = query + "\n" + ") AS const";
		query = query + "\n" + "ORDER BY table_name ASC, sort_value ASC;";
	}

	return query;
}

//***************************************************************************
// @brief 인덱스 구성 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 인덱스 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetIndexInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.index_id, i.type_desc AS index_type, i.is_primary_key, ";
	query = query + "i.is_unique, ic.index_column_id AS column_seq, COL_NAME(ic.object_id, ic.column_id) AS column_name, ";
	query = query + "(CASE ic.is_descending_key WHEN 1 THEN 2 ELSE 1 END) AS column_sort, ISNULL(kc.is_system_named, 0) AS is_system_named";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.indexes AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS i";
	query = query + "\n" + "INNER JOIN sys.index_columns AS ic";
	query = query + "\n" + "ON i.object_id = ic.object_id AND i.index_id = ic.index_id";
	query = query + "\n" + "LEFT OUTER JOIN sys.key_constraints AS kc";
	query = query + "\n" + "ON i.object_id = kc.parent_object_id AND i.name = kc.name";
	query = query + "\n" + "WHERE i.type > 0";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY i.index_id ASC, ic.index_column_id ASC;";
	else query = query + "\n" + "ORDER BY i.table_name ASC, i.index_id ASC, ic.index_column_id ASC;";

	return query;
}

//***************************************************************************
// @brief 인덱스 옵션 상세 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 인덱스 옵션 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetIndexOptionInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.type_desc, i.is_primary_key, i.is_unique, ";
	query = query + "i.ignore_dup_key, i.fill_factor, i.is_padded, i.is_disabled, i.allow_row_locks, i.allow_page_locks, i.has_filter, i.filter_definition, i.compression_delay, i.optimize_for_sequential_key, ";
	query = query + "s.no_recompute AS statistics_norecompute, s.is_incremental AS statistics_incremental, ";
	query = query + "p.data_compression, p.data_compression_desc, p.xml_compression, p.xml_compression_desc, ";
	query = query + "ds.type_desc AS filegroup_or_partition_scheme, ds.name AS filegroup_or_partition_scheme_name";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.indexes AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS i";
	query = query + "\n" + "INNER JOIN sys.stats AS s";
	query = query + "\n" + "ON i.object_id = s.object_id AND i.index_id = s.stats_id";
	query = query + "\n" + "INNER JOIN sys.data_spaces AS ds";
	query = query + "\n" + "ON i.data_space_id = ds.data_space_id";
	query = query + "\n" + "INNER JOIN";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT object_id, index_id, data_compression, data_compression_desc, xml_compression, xml_compression_desc, ROW_NUMBER() OVER(PARTITION BY object_id, index_id ORDER BY COUNT(*) DESC) AS main_compression";
	query = query + "\n\t" + "FROM sys.partitions";
	query = query + "\n\t" + "GROUP BY object_id, index_id, data_compression, data_compression_desc, xml_compression, xml_compression_desc";
	query = query + "\n" + ") AS p";
	query = query + "\n" + "ON i.object_id = p.object_id AND i.index_id = p.index_id AND p.main_compression = 1";
	query = query + "\n" + "WHERE i.is_hypothetical = 0 AND i.index_id <> 0";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY i.index_id ASC;";
	else query = query + "\n" + "ORDER BY i.table_name ASC, i.index_id ASC;";

	return query;
}

//***************************************************************************
// @brief 테이블 파티션 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 파티션 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetPartitionInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.name AS table_name, ic.column_id AS partition_column_id, c.name AS partition_column_name, i.name AS index_name, ";
	query = query + "(UPPER(TYPE_NAME(c.user_type_id)) + (CASE WHEN TYPE_NAME(c.user_type_id) = 'varchar' OR TYPE_NAME(c.user_type_id) = 'char' THEN '(' ";
	query = query + "+ (CASE WHEN c.max_length = -1 THEN 'MAX' ELSE CAST(c.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(c.user_type_id) = 'nvarchar' ";
	query = query + "OR TYPE_NAME(c.user_type_id) = 'nchar' THEN '(' + (CASE WHEN c.max_length = -1 THEN 'MAX' ELSE CAST(c.max_length / 2 AS VARCHAR) END) + ')' ";
	query = query + "WHEN TYPE_NAME(c.user_type_id) = 'decimal' THEN '(' + CAST(c.precision AS VARCHAR) + ',' + CAST(c.scale AS VARCHAR) + ')' ELSE '' END)) AS partition_column_datatype, ";
	query = query + "s.[name] AS partition_schema_name, p.partition_number AS partition_number, f.name AS partition_function_name, f.type_desc AS partition_function_type, ";
	query = query + "f.boundary_value_on_right AS partition_range, rv.value AS boundary_value1, rv2.value AS boundary_value2, ";
	query = query + "fg.name AS filegroups_name, df.name AS file_name, df.physical_name	AS file_physical_name, df.size AS file_size, df.max_size AS file_maxsize, df.growth AS file_growth, ";
	query = query + "p.rows AS table_rows";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.indexes AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE a.type <= 1 AND b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE a.type <= 1 AND b.type = 'U'";

	query = query + "\n" + ") AS i";
	query = query + "\n" + "INNER JOIN sys.partitions AS p ON i.object_id = p.object_id AND i.index_id = p.index_id";
	query = query + "\n" + "INNER JOIN sys.partition_schemes AS s ON i.data_space_id = s.data_space_id";
	query = query + "\n" + "INNER JOIN sys.index_columns AS ic ON ic.object_id = i.object_id AND ic.index_id = i.index_id AND ic.partition_ordinal >= 1";
	query = query + "\n" + "INNER JOIN sys.columns AS c ON i.object_id = c.object_id AND ic.column_id = c.column_id";
	query = query + "\n" + "INNER JOIN sys.partition_functions AS f ON s.function_id = f.function_id";
	query = query + "\n" + "INNER JOIN sys.destination_data_spaces AS ds ON s.data_space_id = ds.partition_scheme_id AND p.partition_number = ds.destination_id";
	query = query + "\n" + "INNER JOIN sys.filegroups AS fg ON ds.data_space_id = fg.data_space_id";
	query = query + "\n" + "INNER JOIN sys.database_files AS df ON df.data_space_id  = fg.data_space_id";
	query = query + "\n" + "LEFT OUTER JOIN sys.partition_range_values AS r ON f.function_id = r.function_id and r.boundary_id = p.partition_number";
	query = query + "\n" + "LEFT OUTER JOIN sys.partition_range_values AS rv ON f.function_id = rv.function_id AND p.partition_number = rv.boundary_id";
	query = query + "\n" + "LEFT OUTER JOIN sys.partition_range_values AS rv2 ON f.function_id = rv2.function_id AND p.partition_number - 1= rv2.boundary_id";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY p.partition_number ASC;";
	else query = query + "\n" + "ORDER BY i.table_name ASC, p.partition_number ASC;";

	return query;
}

//***************************************************************************
// @brief 외래키(FK) 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 외래키 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetForeignKeyInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, fk.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(fk.parent_object_id) AS schema_name, fk.table_name AS table_name, fk.name AS foreignkey_name, fk.is_disabled, fk.is_not_trusted, ";
	query = query + "OBJECT_NAME(fkcol.parent_object_id) AS foreignkey_table_name, COL_NAME(fkcol.parent_object_id, fkcol.parent_column_id) AS foreignkey_column_name, ";
	query = query + "OBJECT_SCHEMA_NAME(fkcol.referenced_object_id) AS referencekey_schema_name, OBJECT_NAME(fkcol.referenced_object_id) AS referencekey_table_name, COL_NAME(fkcol.referenced_object_id, fkcol.referenced_column_id) AS referencekey_column_name, ";
	query = query + "REPLACE(fk.update_referential_action_desc, '_', ' ') AS update_rule, REPLACE(fk.delete_referential_action_desc, '_', ' ') AS delete_rule, fk.is_system_named AS is_system_named";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.foreign_keys AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.parent_object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS fk";
	query = query + "\n" + "INNER JOIN sys.foreign_key_columns AS fkcol";
	query = query + "\n" + "ON fk.object_id = fkcol.constraint_object_id";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY fk.name ASC, fkcol.constraint_column_id ASC, fkcol.referenced_column_id ASC;";
	else query = query + "\n" + "ORDER BY fk.table_name ASC, fk.name ASC, fkcol.constraint_column_id ASC, fkcol.referenced_column_id ASC;";

	return query;
}

//***************************************************************************
// @brief 기본값 제약조건 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 기본값 제약조건 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetDefaultConstInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, ";
	query = query + "const.name AS default_const_name, COL_NAME(const.parent_object_id, const.parent_column_id) AS column_name, const.definition AS default_value, const.is_system_named AS is_system_named";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.default_constraints AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.parent_object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS const";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY const.name ASC;";
	else query = query + "\n" + "ORDER BY const.table_name ASC, const.name ASC;";

	return query;
}

//***************************************************************************
// @brief CHECK 제약조건 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return CHECK 제약조건 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetCheckConstInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, ";
	query = query + "const.name AS check_const_name, const.definition AS check_value, const.is_system_named AS is_system_named";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.check_constraints AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.parent_object_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS const";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY const.name ASC;";
	else query = query + "\n" + "ORDER BY const.table_name ASC, const.name ASC;";

	return query;
}

//***************************************************************************
// @brief 트리거 정보 조회 쿼리를 생성하는 함수
// @param tableName 대상 테이블 명 (기본값: 전체 테이블)
// @return 트리거 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetTriggerInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, tr.parent_id AS object_id, OBJECT_SCHEMA_NAME(tr.parent_id) AS schema_name, tr.table_name AS table_name, tr.name AS trigger_name";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
	query = query + "\n\t" + "FROM sys.triggers AS a";
	query = query + "\n\t" + "INNER JOIN sys.tables AS b";
	query = query + "\n\t" + "ON a.parent_id = b.object_id";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE b.type = 'U'";

	query = query + "\n" + ") AS tr";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY tr.name ASC;";
	else query = query + "\n" + "ORDER BY tr.table_name ASC, tr.name ASC;";

	return query;
}

//***************************************************************************
// @brief 프로시저 목록 조회 쿼리를 반환하는 함수
// @return 프로시저 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetProcedureListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, 2 AS object_type, name AS object_name";
	query = query + "\n" + "FROM sys.procedures";
	query = query + "\n" + "ORDER BY name ASC;";
	return query;
}

//***************************************************************************
// @brief 프로시저 정보 조회 쿼리를 생성하는 함수
// @param procName 대상 프로시저 명 (기본값: 전체 프로시저)
// @return 프로시저 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetProcedureInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, stproc.object_id AS object_id, SCHEMA_NAME(stproc.schema_id) AS schema_name, stproc.name AS proc_name, CAST(ep.[value] AS NVARCHAR(4000)) AS proc_comment, ";
	query = query + "CONVERT(VARCHAR(23), stproc.create_date, 121) AS create_date, CONVERT(VARCHAR(23), stproc.modify_date, 121) AS modify_date";
	query = query + "\n" + "FROM sys.procedures AS stproc";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON stproc.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";

	if( procName != "" )
		query = query + "\n" + "WHERE stproc.name = '" + procName + "';";
	else query = query + "\n" + "ORDER BY stproc.name ASC;";

	return query;
}

//***************************************************************************
// @brief 프로시저 매개변수 정보 조회 쿼리를 생성하는 함수
// @param procName 대상 프로시저 명 (기본값: 전체 프로시저)
// @return 프로시저 매개변수 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetProcedureParamInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.proc_name AS proc_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
	query = query + "param.name AS param_name, UPPER(TYPE_NAME(param.user_type_id)) AS datatype, ";
	query = query + "param.max_length, param.precision, param.scale, ";
	query = query + "(UPPER(TYPE_NAME(param.user_type_id)) + (CASE WHEN TYPE_NAME(param.user_type_id) = 'varchar' OR TYPE_NAME(param.user_type_id) = 'char' THEN '(' ";
	query = query + "+ (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(param.user_type_id) = 'nvarchar' ";
	query = query + "OR TYPE_NAME(param.user_type_id) = 'nchar' THEN '(' + (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length / 2 AS VARCHAR) END) + ')' ";
	query = query + "WHEN TYPE_NAME(param.user_type_id) = 'decimal' THEN '(' + CAST(param.precision AS VARCHAR) + ',' + CAST(param.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ";
	query = query + "CAST(ep.[value] AS NVARCHAR(4000)) AS param_comment";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS proc_name";
	query = query + "\n\t" + "FROM sys.parameters AS a";
	query = query + "\n\t" + "INNER JOIN sys.procedures AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( procName != "" )
		query = query + "\n\t" + "WHERE b.name = '" + procName + "'";

	query = query + "\n" + ") AS param";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'";

	if( procName != "" )
		query = query + "\n" + "ORDER BY param.parameter_id ASC;";
	else query = query + "\n" + "ORDER BY param.proc_name ASC, param.parameter_id ASC;";

	return query;
}

//***************************************************************************
// @brief 함수 목록 조회 쿼리를 반환하는 함수
// @return 함수 목록 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetFunctionListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, 3 AS object_type, name AS object_name";
	query = query + "\n" + "FROM sys.objects";
	query = query + "\n" + "WHERE type IN ('FN', 'IF', 'TF')";
	query = query + "\n" + "ORDER BY name ASC;";
	return query;
}

//***************************************************************************
// @brief 함수 정보 조회 쿼리를 생성하는 함수
// @param funcName 대상 함수 명 (기본값: 전체 함수)
// @return 함수 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetFunctionInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, so.object_id AS object_id, SCHEMA_NAME(so.schema_id) AS schema_name, so.name AS func_name, CAST(ep.[value] AS NVARCHAR(4000)) AS func_comment, ";
	query = query + "CONVERT(VARCHAR(23), so.create_date, 121) AS create_date, CONVERT(VARCHAR(23), so.modify_date, 121) AS modify_date";
	query = query + "\n" + "FROM sys.objects AS so";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON so.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";

	if( funcName != "" )
	{
		query = query + "\n" + "WHERE so.type IN ('FN', 'IF', 'TF') AND so.name = '" + funcName + "';";
	}
	else
	{
		query = query + "\n" + "WHERE so.type IN ('FN', 'IF', 'TF')";
		query = query + "\n" + "ORDER BY so.name ASC;";
	}

	return query;
}

//***************************************************************************
// @brief 함수 매개변수 정보 조회 쿼리를 생성하는 함수
// @param funcName 대상 함수 명 (기본값: 전체 함수)
// @return 함수 매개변수 정보 조회 쿼리 문자열
//***************************************************************************
inline _tstring MSSQLGetFunctionParamInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.func_name AS func_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
	query = query + "param.name AS param_name, UPPER(TYPE_NAME(param.user_type_id)) AS datatype, ";
	query = query + "param.max_length, param.precision, param.scale, ";
	query = query + "(UPPER(TYPE_NAME(param.user_type_id)) + (CASE WHEN TYPE_NAME(param.user_type_id) = 'varchar' OR TYPE_NAME(param.user_type_id) = 'char' THEN '(' ";
	query = query + "+ (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(param.user_type_id) = 'nvarchar' ";
	query = query + "OR TYPE_NAME(param.user_type_id) = 'nchar' THEN '(' + (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length / 2 AS VARCHAR) END) + ')' ";
	query = query + "WHEN TYPE_NAME(param.user_type_id) = 'decimal' THEN '(' + CAST(param.precision AS VARCHAR) + ',' + CAST(param.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ";
	query = query + "CAST(ep.[value] AS NVARCHAR(4000)) AS param_comment";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.name AS func_name";
	query = query + "\n\t" + "FROM sys.parameters AS a";
	query = query + "\n\t" + "INNER JOIN sys.objects AS b";
	query = query + "\n\t" + "ON a.object_id = b.object_id";

	if( funcName != "" )
		query = query + "\n\t" + "WHERE b.type IN ('FN', 'IF', 'TF') AND b.name = '" + funcName + "'";
	else query = query + "\n\t" + "WHERE b.type IN ('FN', 'IF', 'TF')";

	query = query + "\n" + ") AS param";
	query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
	query = query + "\n" + "ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'";

	if( funcName != "" )
		query = query + "\n" + "ORDER BY param.parameter_id ASC;";
	else query = query + "\n" + "ORDER BY param.func_name ASC, param.parameter_id ASC;";

	return query;
}

#endif // ndef UC_DBMSSQLQUERY_H