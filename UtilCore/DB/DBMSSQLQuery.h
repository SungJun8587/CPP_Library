
//***************************************************************************
// DBMSSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBMSSQLQUERY_H__
#define __DBMSSQLQUERY_H__

#pragma once

//***************************************************************************
//
enum class EMSSQLIndexFragmentation
{
	REORGANIZE,
	REBUILD,
	DISABLE
};

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
//
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
//
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
//
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
//
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
// MSSQL 인덱스타입 : sys.indexes 테이블 type_desc(nvarchar(60)) 컬럼
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
// MSSQL 확장 속성 처리 파라미터
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
// MSSQL 인덱스 조각화 정보
class MSSQL_INDEX_FRAGMENTATION
{
public:
public:
	int32	ObjectId;														// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };			// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };				// 테이블 명
	int32	IndexId;														// 인덱스 고유번호
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// 인덱스 명
	TCHAR   tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };		// 인덱스 타입
	int32   PartitionNum;													// 개체의 파티션 번호

	// 논리적 조각화(인덱스에서 순서가 잘못된 페이지) 수치
	// - 인덱스에 대한 논리적 조각화 또는 할당 단위의 힙에 IN_ROW_DATA 대한 익스텐트 조각화
	// - avg_fragmentation_in_percent(조각화 수치)가 30이상이면 리빌드(Rebuild) 30미만이면 리오그나이즈(Reorganize) 실행
	float	AvgFragmentationInPercent;

	// 평균 페이지 밀도(모든 페이지에서 사용되는 사용 가능한 데이터 스토리지 공간의 평균 백분율)
	float	AvgPageSpaceUsedInPercent;

	// 총 인덱스 또는 데이터 페이지 수
	// - 인덱스의 경우 할당 단위에서 B-트리의 현재 수준에 있는 IN_ROW_DATA 총 인덱스 페이지 수
	// - 힙의 경우 할당 단위의 총 데이터 페이지 IN_ROW_DATA 수(할당 단위의 ROW_OVERFLOW_DATA 경우 LOB_DATA 할당 단위의 총 페이지 수)
	int32	PageCount;

	// 할당 단위 유형에 대한 설명
	// - LOB_DATA : text, ntext, image, varchar(max), nvarchar(max), varbinary(max) 및 xml 형식의 열에 저장된 데이터가 포함
	// - ROW_OVERFLOW_DATA : varchar(n), nvarchar(n), varbinary(n) 및 행에서 푸시된 sql_variant 형식의 열에 저장된 데이터가 포함
	TCHAR	tszAllocUnitTypeDesc[DATABASE_WVARCHAR_MAX] = { 0, };
};

//***************************************************************************
//
class MSSQL_INDEX_OPTION_INFO
{
public:
	int32	ObjectId;															// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];							// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];							// 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];							// 인덱스 명
	int32	IndexId;															// 인덱스 고유번호
	bool    IsPrimaryKey;														// 기본키 여부(true/false : 유/무)
	bool    IsUnique;															// 유니크 여부(true/false : 유/무)

	/// <summary>인덱스 비활성화 여부(0/1 : 활성화/비활성화)</summary>
	bool	IsDisabled;

	/// <summary>PAD_INDEX = { ON | OFF }
	/// - 인덱스 패딩을 지정. 기본값은 OFF
	/// - 클러스터형 columnstore 인덱스의 경우 항상 0
	/// </summary>
	bool	IsPadded;

	/// <summary>FILLFACTOR = <fillfactor 값>
	/// - 각 인덱스 페이지의 리프 수준을 어느 정도 채울지 나타내는 비율을 지정
	/// - 클러스터형 columnstore 인덱스의 경우 항상 0
	/// </summary>
	int8	FillFactor;

	/// <summary>IGNORE_DUP_KEY = { ON | OFF }
	/// - 삽입 작업에서 고유 인덱스에 중복된 키 값을 삽입하려고 할 때 응답 유형을 지정
	/// - ON이면 중복된 키 값이 고유 인덱스에 삽입되는 경우 경고 메시지가 나타나고 고유성 제약 조건을 위반하는 행만 실패
	/// - OFF이면 중복된 키 값이 고유 인덱스에 삽입되는 경우 오류 메시지가 나타나고 전체 INSERT 작업이 롤백
	/// - 뷰, 비고유 인덱스, XML 인덱스, 공간 인덱스 및 필터링된 인덱스에 생성된 인덱스의 경우 IGNORE_DUP_KEY를 ON으로 설정할 수 없음
	/// </summary>
	bool	IgnoreDupKey;

	/// <summary>ALLOW_ROW_LOCKS = { ON | OFF }
	/// - 인덱스에서 행 잠금 허용 여부(0/1 : 비허용/허용)
	/// - 클러스터형 columnstore 인덱스의 경우 항상 0
	/// </summary>
	bool	AllowRowLocks;

	/// <summary>ALLOW_PAGE_LOCKS = { ON | OFF }
	/// - 인덱스에서 페이지 잠금 허용 여부(0/1 : 비허용/허용)
	/// - 클러스터형 columnstore 인덱스의 경우 항상 0
	/// </summary>    	
	bool	AllowPageLocks;

	/// <summary>인덱스 필터 존재 여부(0/1 : 무/유)</summary>
	bool	HasFilter;

	/// <summary>필터링된 인덱스에 포함된 행의 하위 집합에 대한 식
	/// - 힙, 필터링되지 않은 인덱스 또는 테이블에 대한 권한이 부족한 경우 NULL
	/// </summary>
	TCHAR   tszFilterDefinition[DATABASE_WVARCHAR_MAX];

	/// <summary>COMPRESSION_DELAY = { 0 | duration [Minutes] }
	/// - SQL Server 2016(13.x) 버전 이상
	/// - 디스크 기반 테이블의 경우 delay은 CLOSED 상태의 델타 rowgroup이 델타 rowgroup에 남아 있어야 하는 최소 시간(분)을 지정. 기본값은 0분
	/// - > 0이면 Columnstore 인덱스 압축 지연 시간(분)
	/// - NULL이면 Columnstore 인덱스 행 그룹 압축 지연이 자동으로 관리
	/// </summary>	
	int32	CompressionDelay;

	/// <summary>OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF }
	/// - SQL Server 2019(15.x) 버전 이상 및 Azure SQL Database
	/// - 마지막 페이지 삽입 경합에 최적화할지 여부를 지정. 기본값은 OFF
	/// - ON이면 인덱스가 마지막 페이지 삽입 최적화를 사용하도록 설정
	/// - OFF이면 인덱스가 마지막 페이지 삽입 최적화를 사용하지 않도록 설정
	/// </summary>
	bool	OptimizeForSequentialKey;

	/// <summary>STATISTICS_NORECOMPUTE = { ON | OFF }
	/// - sys.stats 테이블 참조
	/// - 통계를 다시 계산할지 여부를 지정. 기본값은 OFF
	/// - ON이면 옵션을 사용하여 통계가 생성됨
	/// - OFF이면 옵션을 사용하여 통계가 생성되지 않음
	/// </summary>	
	bool	StatisticsNoRecompute;

	/// <summary>STATISTICS_INCREMENTAL = { ON | OFF }
	/// - SQL Server 2014(12.x) 버전 이상 및 Azure SQL Database
	/// - sys.stats 테이블 참조
	/// - 통계를 증분 통계로 생성할지 여부. 기본값은 OFF
	/// - ON이면 통계가 증분됨
	/// - OFF이면 통계가 증분되지 않음
	/// </summary>	
	bool	StatisticsIncremental;

	/// <summary>DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
	/// - 각 파티션에 대한 압축 상태(0/1/2/3/4 : NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	/// - SQL Server 2014(12.x) 버전 이상 및 Azure SQL Database
	/// - sys.partitions 테이블 참조
	/// </summary>
	int8	DataCompression;

	/// <summary>각 파티션에 대한 압축 상태 설명 문자열(NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	/// - SQL Server 2014(12.x) 버전 이상 및 Azure SQL Database
	/// - sys.partitions 테이블 참조
	/// </summary>
	TCHAR   tszDataCompressionDesc[DATABASE_BASE_STRLEN];

	/// <summary>XML_COMPRESSION = { ON | OFF }
	/// - 각 파티션에 대한 XML 압축 상태
	/// - SQL Server 2022(16.x) 버전 이상 및 Azure SQL Database
	/// - sys.partitions 테이블 참조
	/// </summary>	
	bool	XmlCompression;

	/// <summary>각 파티션에 대한 XML 압축 상태 설명 문자열(OFF/ON)
	/// - SQL Server 2022(16.x) 버전 이상 및 Azure SQL Database
	/// - sys.partitions 테이블 참조
	/// </summary>
	TCHAR   tszXmlCompressionDesc[DATABASE_BASE_STRLEN];

	/// <summary>데이터베이스 내에서 고유한 데이터 공간의 이름
	/// - sys.data_spaces 테이블 참조
	/// </summary>
	TCHAR   tszFileGroupOrPartitionScheme[DATABASE_OBJECT_TYPE_DESC_STRLEN];

	/// <summary>데이터 공간 유형에 대한 설명
	/// - sys.data_spaces 테이블 참조
	/// </summary>
	TCHAR   tszFileGroupOrPartitionSchemeName[DATABASE_OBJECT_NAME_STRLEN];

	/// <summary>SORT_IN_TEMPDB = { ON | OFF }
	/// - tempdb에 정렬 결과를 저장할지 여부를 지정. Azure SQL Database 하이퍼스케일을 제외하고 기본값은 OFF
	/// - 정렬 작업이 필요하지 않거나 메모리에서 정렬을 수행할 수 있으면 SORT_IN_TEMPDB 옵션이 무시
	/// - ON이면 인덱스 작성에 사용된 중간 정렬 결과가 tempdb에 저장(tempdb가 사용자 데이터베이스와는 다른 디스크 집합에 존재)
	/// - ON이면 인덱스를 만드는 데 필요한 시간이 줄어들 수 있습니다. 그러나 인덱스 작성 중에 사용되는 디스크 공간의 크기는 커집니다.
	/// - OFF이면 중간 정렬 결과가 인덱스와 같은 데이터베이스에 저장
	/// </summary>
	bool SortInTempDB;

	/// <summary>ONLINE = { ON | OFF }
	/// - SQL Server 2008(10.0.x) 버전 이상. 온라인 인덱스 작업은 SQL Server Enterprise Edition 또는 Azure SQL Edge에서만 수행
	/// - 인덱스 작업 중 쿼리 및 데이터 수정에 기본 테이블과 관련 인덱스를 사용할 수 있는지 여부를 지정. 기본값은 OFF
	/// - rebuild_index_option에 적용
	/// - ON이면 인덱스 작업 중에 장기 테이블 잠금이 유지되지 않음
	/// - OFF이면 인덱스 작업 중에 테이블 잠금이 적용
	/// </summary>
	bool Online;

	/// <summary>MAXDOP = max_degree_of_parallelism
	/// - 인덱스 작업 동안 max degree of parallelism 구성 옵션을 재정의 
	///     1 : 병렬 계획이 생성되지 않습니다.
	///     > 1 : 병렬 인덱스 작업에 사용되는 최대 프로세서 수를 지정된 값으로 제한합니다.
	///     0(기본값) : 현재 시스템 작업에 따라 실제 프로세서 수 이하의 프로세서를 사용합니다.
	/// - MAXDOP 옵션은 모든 XML 인덱스에 대해 구문이 지원되지만 공간 인덱스 또는 기본 XML 인덱스의 경우 ALTER INDEX는 현재 단일 프로세서만 사용합니다.
	/// - SQL Server 인스턴스의 구성 설정 중 하나
	///     SELECT value 
	///     FROM sys.configurations 
	///     WHERE name = 'max degree of parallelism';
	/// </summary>
	uint8 MaxDop;

	/// <summary>RESUMABLE = { ON | OFF }
	/// SQL Server 2017(14.x) 버전 이상 및 Azure SQL Database
	/// - ALTER TABLE ADD CONSTRAINT 작업이 다시 시작될 수 있는지 여부를 지정. 기본값은 OFF
	/// - ON이면 테이블 제약 조건 추가 작업 다시 시작 가능
	/// - OFF이면 테이블 제약 조건 추가 작업 다시 시작 불가능
	/// - RESUMABLE 옵션이 ON으로 설정되면 ONLINE = ON 옵션이 필요
	/// - RESUMABLE 옵션이 ON으로 설정되어 있지 않으면 MAX_DURATION(= time [MINUTES]) 옵션이 허용되지 않음
	/// - MAX_DURATION은 RESUMABLE 작업의 최대 지속 시간을 설정하는 데 사용될 수 있음
	/// - SQL Server 에이전트 작업 설정에서 지정
	///     SELECT s.name, s.command
	///     FROM msdb.dbo.sysjobs AS j
	///     INNER JOIN msdb.dbo.sysjobsteps AS s 
	///     ON j.job_id = s.job_id
	///     WHERE command LIKE '%RESUMABLE=ON%';        
	/// </summary>
	bool Resumable;

	/// <summary>MAX_DURATION = time [MINUTES] 
	/// - SQL Server 2017(14.x) 버전 이상
	/// - 인덱스 재구성 작업의 최대 지속 시간
	/// - 다시 시작할 수 있는 온라인 인덱스 작업이 일시 중지하기 전에 실행된 시간을 나타냄(분 단위로 지정된 정수 값)
	/// - SQL Server 에이전트 작업 설정에서 지정
	///     SELECT s.name, s.command
	///     FROM msdb.dbo.sysjobs AS j
	///     INNER JOIN msdb.dbo.sysjobsteps AS s 
	///     ON j.job_id = s.job_id
	///     WHERE command LIKE '%MAX_DURATION%';        
	/// </summary>   
	int32 MaxDuration;

	/// <summary>ABORT_AFTER_WAIT = [ NONE | SELF | BLOCKERS ]
	/// - MAX_DURATION 시간 동안 차단되면 지정한 ABORT_AFTER_WAIT 작업이 실행
	/// - NONE : 보통(일반) 우선 순위로 잠금을 계속 대기합니다.
	/// - SELF : 어떤 동작도 수행하지 않고 현재 실행 중인 온라인 인덱스 다시 작성 DDL 작업을 종료. MAX_DURATION이 0이면 SELF 옵션을 사용할 수 없음.
	/// - BLOCKERS : 작업을 계속할 수 있도록 온라인 인덱스 다시 작성 DDL 작업을 차단하는 모든 사용자 트랜잭션을 종료. BLOCKERS 옵션을 사용하려면 로그인에 ALTER ANY CONNECTION 권한이 있어야 함.
	/// </summary>
	int8 AbortAfterWait;

	/// <summary>LOB_COMPACTION = { ON | OFF }
	/// - rowstore 인덱스에 적용
	/// - image, text, ntext, varchar(max), nvarchar(max), varbinary(max), xml과 같은 큰 개체(LOB) 데이터 형식의 데이터를 포함하는 모든 페이지를 압축하도록 지정
	/// - reorganize_option에 적용
	/// - ON이면 클러스터형 인덱스의 경우 테이블에 포함된 모든 LOB 열이 압축, 비클러스터형 인덱스의 경우 인덱스에서 키가 아닌(포괄) 열인 LOB 열이 모두 압축
	/// - OFF이면 큰 개체 데이터가 포함된 페이지가 압축되지 않으며, 힙에는 아무 영향이 없음 
	/// </summary>
	bool LobCompaction;

	/// <summary>COMPRESS_ALL_ROW_GROUPS = { ON | OFF }
	/// - SQL Server 2016(13.x) 버전 이상 및 Azure SQL Database
	/// - columnstore 인덱스에 적용
	/// - 열린(OPEN) 또는 닫힌(CLOSED) 델타 rowgroup을 columnstore로 강제 적용하는 방법을 제공
	/// - reorganize_option에 적용
	/// - ON은 크기 및 상태(CLOSED 또는 OPEN)와 상관없이 모든 rowgroup을 columnstore에 강제 적용
	/// - OFF는 모든 닫힌(CLOSED) rowgroup을 columnstore에 강제 적용
	/// </summary>
	bool CompressAllRowGroups;
};

//***************************************************************************
//
class MSSQL_DEFAULT_CONSTRAINT_INFO
{
public:
	int32	ObjectId;												// MSSQL 테이블 고유번호
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// 테이블 명
	TCHAR   tszDefaultConstName[DATABASE_OBJECT_NAME_STRLEN];		// 기본값 제약조건 명
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];				// 컬럼 명
	TCHAR   tszDefaultValue[DATABASE_WVARCHAR_MAX];					// 컬럼 제약조건 정의값
	bool	IsSystemNamed;											// 시스템이 인덱스명을 할당했는지 여부(true/false : 유/무)
};

//***************************************************************************
//
inline _tstring MSSQLGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, bool isIdentity, ulong seedValue, ulong incrementValue, _tstring collation = _T(""))
{
	_tstring columnOption = _T("");

	// <컬럼속성> : COLLATE, {NULL|NOT NULL}, IDENTITY(시드값, 증분값) 설정이 포함됨
	//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
	//  - 만약 데이터정렬 값이 해당 데이터베이스에 설정된 데이터정렬 값과 동일한 경우 따로 명시하지 않아도 됨
	//  - COLLATE 데이터정렬 {NULL|NOT NULL} IDENTITY(시드값, 증분값)
	//
	// [컬럼명] 데이터타입 <컬럼속성>
	//  - [컬럼명] 데이터타입 NOT NULL IDENTITY(시드값, 증분값)
	//  - [컬럼명] 데이터타입 {NULL|NOT NULL}
	//  - [컬럼명] 데이터타입 COLLATE 데이터정렬 {NULL|NOT NULL}
	columnOption = dataTypeDesc;
	if( collation != "" )
		columnOption = columnOption + " COLLATE " + collation;

	columnOption = columnOption + (isNullable ? " NULL" : " NOT NULL") + (isIdentity ? tstring_tcformat(_T(" IDENTITY(%ld,%ld)"), seedValue, incrementValue) : _T(""));

	return columnOption;
}

//***************************************************************************
//
inline _tstring MSSQLGetDropConstraintQuery(_tstring schemaName, _tstring tableName, _tstring constType, _tstring constName)
{
	_tstring query = _T("");

	if( constType == "PK" || constType == "UQ" || constType == "F" || constType == "D" || constType == "C" )
	{
		// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
		query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] DROP CONSTRAINT [%s]"), schemaName.c_str(), tableName.c_str(), constName.c_str());
	}
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetDBSystemQuery()
{
	_tstring query = _T("");

	query = query + "SELECT CONCAT('MSSQL ', CAST(SERVERPROPERTY('PRODUCTVERSION') AS VARCHAR(50))) AS [version], @@LANGUAGE AS [characterset], collation_name AS [collation]";
	query = query + "\n" + "FROM sys.databases";
	query = query + "\n" + "WHERE name = DB_NAME()";
	return query;
}

//***************************************************************************
//
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
//
inline _tstring MSSQLGetUserListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT [name] FROM sys.server_principals WHERE is_disabled = 0 AND type IN ('S')";
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetDatabaseListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT [name] FROM sys.databases WHERE [name] NOT IN('model', 'msdb', 'pubs', 'Northwind', 'tempdb') ORDER BY [name] ASC";
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetDatabaseBackupQuery(_tstring databaseName, _tstring backupFilePath)
{
	_tstring query = _T("");

	query = query + "BACKUP DATABASE " + databaseName + " TO DISK = '" + backupFilePath + "'";
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath)
{
	_tstring query = _T("");

	// query = "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	// query = "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "' WITH REPLACE";
	query = query + "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath, _tstring dataFilePath, _tstring logFilePath)
{
	_tstring query = _T("");

	query = query + "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "'";
	query = query + " WITH MOVE '" + databaseName + " TO '" + dataFilePath + "'";
	query = query + ", MOVE '" + databaseName + "_log' TO '" + logFilePath + "'";
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLGetRowStoreIndexFragmentationCheckQuery(_tstring tableName = _T(""))
{
	// - https://learn.microsoft.com/ko-kr/sql/relational-databases/indexes/reorganize-and-rebuild-indexes?view=sql-server-ver16
	// rowstore 인덱스의 조각화 및 페이지 밀도 확인
	// avg_fragmentation_in_percent(조각화 수치)가 30이상이면 리빌드(Rebuild) 30미만이면 리오그나이즈(Reorganize) 실행
	// sys.dm_db_index_physical_stats([database_id], [object_id], [index_id], [partition_number], [mode]) 
	//  [database_id] | NULL | 0 | 기본 : 데이터베이스의 ID
	//  [object_id] | NULL | 0 | 기본 : 인덱스가 있는 테이블 또는 뷰의 개체 ID
	//  [index_id] | 0 | NULL | -1 | 기본 : 인덱스의 ID
	//  [partition_number] | NULL | 0 | 기본 : 개체의 파티션 번호
	//  [mode] | NULL | 기본 : mode의 이름. mode는 통계를 가져오는 데 사용되는 검사 수준을 지정
	//  - DEFAULT/NULL/LIMITED/SAMPLED/DETAILED. 기본값(NULL)은 LIMITED
	//  - DETAILED일 경우 모든 인덱스 페이지를 검사해야 하며 시간이 오래 걸릴 수 있습니다
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
//
inline _tstring MSSQLGetColumnStoreIndexFragmentationCheckQuery(_tstring tableName = _T(""))
{
	// - https://learn.microsoft.com/ko-kr/sql/relational-databases/indexes/reorganize-and-rebuild-indexes?view=sql-server-ver16
	// columnstore 인덱스의 조각화 확인
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
// - https://learn.microsoft.com/ko-kr/sql/t-sql/statements/alter-index-transact-sql?view=sql-server-ver16
// - https://learn.microsoft.com/ko-kr/sql/relational-databases/indexes/set-index-options?view=sql-server-ver16
//
// ALTER INDEX { index_name | ALL } ON <object>
// {
//     REBUILD {
//             [ PARTITION = ALL [ WITH ( <rebuild_index_option> [ ,...n ] ) ] ]
//         | [ PARTITION = partition_number [ WITH ( <single_partition_rebuild_index_option> [ ,...n ] ) ] ]
//     }
//     | DISABLE
//     | REORGANIZE  [ PARTITION = partition_number ] [ WITH ( <reorganize_option>  ) ]
//     | SET ( <set_index_option> [ ,...n ] )
//     | RESUME [WITH (<resumable_index_option> [, ...n])]
//     | PAUSE
//     | ABORT
// }
// [ ; ]
//
// <object> ::=
// {
//     { database_name.schema_name.table_or_view_name | schema_name.table_or_view_name | table_or_view_name }
// }
//
// <rebuild_index_option> ::=
// {
//     PAD_INDEX = { ON | OFF }
//     | FILLFACTOR = fillfactor
//     | SORT_IN_TEMPDB = { ON | OFF }
//     | IGNORE_DUP_KEY = { ON | OFF }
//     | STATISTICS_NORECOMPUTE = { ON | OFF }
//     | STATISTICS_INCREMENTAL = { ON | OFF }
//     | ONLINE = { ON [ ( <low_priority_lock_wait> ) ] | OFF }
//     | RESUMABLE = { ON | OFF }
//     | MAX_DURATION = <time> [MINUTES]
//     | ALLOW_ROW_LOCKS = { ON | OFF }
//     | ALLOW_PAGE_LOCKS = { ON | OFF }
//     | MAXDOP = max_degree_of_parallelism
//     | DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
//         [ ON PARTITIONS ( {<partition_number> [ TO <partition_number>] } [ , ...n ] ) ]
//     | XML_COMPRESSION = { ON | OFF }
//         [ ON PARTITIONS ( {<partition_number> [ TO <partition_number>] } [ , ...n ] ) ]
// }
//
// <single_partition_rebuild_index_option> ::=
// {
//     SORT_IN_TEMPDB = { ON | OFF }
//     | MAXDOP = max_degree_of_parallelism
//     | RESUMABLE = { ON | OFF }
//     | MAX_DURATION = <time> [MINUTES]
//     | DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
//     | XML_COMPRESSION = { ON | OFF }
//     | ONLINE = { ON [ ( <low_priority_lock_wait> ) ] | OFF }
// }
//
// <reorganize_option> ::=
// {
//     LOB_COMPACTION = { ON | OFF }
//     | COMPRESS_ALL_ROW_GROUPS =  { ON | OFF}
// }
//
// <set_index_option> ::=
// {
//     ALLOW_ROW_LOCKS = { ON | OFF }
//     | ALLOW_PAGE_LOCKS = { ON | OFF }
//     | OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF }
//     | IGNORE_DUP_KEY = { ON | OFF }
//     | STATISTICS_NORECOMPUTE = { ON | OFF }
//     | COMPRESSION_DELAY = { 0 | delay [Minutes] }
// }
//
// <resumable_index_option> ::=
// {
//     MAXDOP = max_degree_of_parallelism
//     | MAX_DURATION = <time> [MINUTES]
//     | <low_priority_lock_wait>
// }
//
// <low_priority_lock_wait> ::=
// {
//     WAIT_AT_LOW_PRIORITY ( MAX_DURATION = <time> [ MINUTES ] ,
//                         ABORT_AFTER_WAIT = { NONE | SELF | BLOCKERS } )
// }
//
inline _tstring MSSQLIndexOptionSetQuery(_tstring schemaName, _tstring tableName, _tstring indexName, unordered_map<_tstring, _tstring> indexOptions)
{
	int32 i = 0;
	_tstring query = _T("");

	// <set_index_option> ::=
	// {
	//     ALLOW_ROW_LOCKS = { ON | OFF }
	//     | ALLOW_PAGE_LOCKS = { ON | OFF }
	//     | OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF }
	//     | IGNORE_DUP_KEY = { ON | OFF }
	//     | STATISTICS_NORECOMPUTE = { ON | OFF }
	//     | COMPRESSION_DELAY = { 0 | delay [Minutes] }
	// }
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
// 인덱스 Reorganization(재구성)
//  - 기존에 사용되던 Page 정보를 순서대로 다시 구성하는 작업
//  - Rebuilding 보다 리소스가 덜 사용되므로, 이 방법을 기본 인덱스 유지 관리 방법으로 사용하는 게 바람직함
//  - 인덱스 조각화가 비교적 적은 경우에 사용하는 것이 좋음(약 30% 미만의 조각화가 발생한 경우)
//  - 온라인 작업이기 때문에, 장기간의 object-level locks가 발생하지 않으며 Reorganization 작업중에 기본 테이블에 대한 쿼리나 업데이트 작업을 계속 진행할 수 있다
// 인덱스 Rebuilding(재생성)
//  - 기존의 인덱스를 삭제하고 재생성하는 방법
//  - 인덱스의 모든 Row가 검사되며, 통계도 업데이트(Full-Scan) 되어 최신 상태가 되므로, 기본 샘플링된 통계 업데이트에 비해 DB의 성능이 향상되는 경우도 있다
//  - 인덱스 조각화가 심한 경우 사용하는 것이 좋음(약 30% 이상의 조각화가 발생한 경우)
//  - 인덱스 유형 및 DB 엔진에 따라 온라인/오프라인으로 나뉘며, 오프라인 인덱스 리빌딩은 온라인 방식에 비해 시간은 덜 걸리지만, 리빌딩 작업 중에 object-level의 Lock을 유발함
// 인덱스 Disable(비활성화)
inline _tstring MSSQLAlterIndexFragmentationNonOptionQuery(_tstring schemaName, _tstring tableName, _tstring indexName, EMSSQLIndexFragmentation eMSSQLIndexFragmentation)
{
	_tstring query = _T("");

	// avg_fragmentation_in_percent(조각화 수치)가 30이상이면 리빌드(Rebuild) 30미만이면 리오그나이즈(Reorganize) 실행
	// - 인덱스 다시 구성 : ALTER INDEX [인덱스명] ON [테이블명] REORGANIZE;
	// - 테이블의 모든 인덱스 다시 구성 : ALTER INDEX ALL ON [테이블명] REORGANIZE;
	// - 인덱스 다시 작성 : ALTER INDEX [인덱스명] ON [테이블명] REBUILD;
	// - 테이블의 모든 인덱스 다시 작성 : ALTER INDEX ALL ON [테이블명] REBUILD;
	// - 인덱스를 비활성화 : ALTER INDEX [인덱스명] ON [테이블명] DISABLE;
	// - 테이블의 모든 인덱스를 사용하지 않도록 설정 : ALTER INDEX ALL ON [테이블명] DISABLE;
	if( indexName != "" )
		query = tstring_tcformat(_T("ALTER INDEX [%s] ON [%s].[%s] %s"), indexName.c_str(), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
	else query = tstring_tcformat(_T("ALTER INDEX %s ON [%s].[%s] %s"), _T("ALL"), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
	return query;
}

//***************************************************************************
//
inline _tstring MSSQLAlterIndexFragmentationOptionQuery(_tstring schemaName, _tstring tableName, _tstring indexName, EMSSQLIndexFragmentation eMSSQLIndexFragmentation, unordered_map<_tstring, _tstring> indexOptions)
{
	int i = 0;
	_tstring query = _T("");

	// <rebuild_index_option> ::=
	// {
	//     PAD_INDEX = { ON | OFF }
	//     | FILLFACTOR = fillfactor
	//     | SORT_IN_TEMPDB = { ON | OFF }
	//     | IGNORE_DUP_KEY = { ON | OFF }
	//     | STATISTICS_NORECOMPUTE = { ON | OFF }
	//     | STATISTICS_INCREMENTAL = { ON | OFF }
	//     | ONLINE = { ON [ ( <low_priority_lock_wait> ) ] | OFF }
	//     | RESUMABLE = { ON | OFF }
	//     | MAX_DURATION = <time> [MINUTES]
	//     | ALLOW_ROW_LOCKS = { ON | OFF }
	//     | ALLOW_PAGE_LOCKS = { ON | OFF }
	//     | MAXDOP = max_degree_of_parallelism
	//     | DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
	//         [ ON PARTITIONS ( {<partition_number> [ TO <partition_number>] } [ , ...n ] ) ]
	//     | XML_COMPRESSION = { ON | OFF }
	//         [ ON PARTITIONS ( {<partition_number> [ TO <partition_number>] } [ , ...n ] ) ]
	// }
	//
	// <reorganize_option> ::=
	// {
	//     LOB_COMPACTION = { ON | OFF }
	//     | COMPRESS_ALL_ROW_GROUPS =  { ON | OFF}
	// }  
	//
	// 여러 파티션에 대해 서로 다른 데이터 압축 유형을 설정하려면 DATA_COMPRESSION 옵션을 두 번 이상 지정
	// ALTER INDEX [인덱스명] ON [스키마명].[테이블명]
	// REBUILD WITH (
	//     DATA_COMPRESSION = NONE ON PARTITIONS (1),
	//     DATA_COMPRESSION = ROW ON PARTITIONS (2, 4, 6 TO 8),
	//     DATA_COMPRESSION = PAGE ON PARTITIONS (3, 5)
	// );
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
//
inline _tstring MSSQLGetHelpCollationsQuery()
{
	_tstring query = _T("");

	query = query + "SELECT name FROM sys.fn_helpcollations()";
	return query;
}

//***************************************************************************
//
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
//
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
//
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
//
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
//
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
//
inline _tstring MSSQLGetConstraintsInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	if( tableName != "" )
	{
		query = query + "SELECT DB_NAME() AS db_name, object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, OBJECT_NAME(const.parent_object_id) AS table_name, const.name AS const_name, const.type AS const_type, const.type_desc AS const_type_desc, const.const_value AS const_value, const.is_system_named AS is_system_named, ";
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
		query = query + "SELECT DB_NAME() AS db_name, object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, OBJECT_NAME(const.parent_object_id) AS table_name, const.name AS const_name, const.type AS const_type, const.type_desc AS const_type_desc, const.const_value AS const_value, const.is_system_named AS is_system_named, ";
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
//
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
//
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
//
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
//
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
//
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
//
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
//
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
//
inline _tstring MSSQLGetProcedureListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT DB_NAME() AS db_name, 2 AS object_type, name AS object_name";
	query = query + "\n" + "FROM sys.procedures";
	query = query + "\n" + "ORDER BY name ASC;";
	return query;
}

//***************************************************************************
//
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
//
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
//
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
//
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
//
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

#endif // ndef __DBMSSQLQUERY_H__