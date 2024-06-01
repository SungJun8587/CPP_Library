
//***************************************************************************
// DBQueryProcess.h : interface for the Database Synchronizer.
//
//***************************************************************************

#ifndef __DBQUERYPROCESS_H__
#define __DBQUERYPROCESS_H__

#pragma once

#ifndef __BASEDEFINE_H__
#include <BaseDefine.h>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __DBENUM_H__
#include <DBEnum.h> 
#endif

#ifndef __DBSQLQUEQY_H__
#include <DBSQLQuery.h> 
#endif

#ifndef __DBMSSQLQUEQY_H__
#include <DBMSSQLQuery.h> 
#endif

#ifndef __DBMYSQLQUEQY_H__
#include <DBMYSQLQuery.h> 
#endif

#ifndef __BASEODBC_H__
#include <BaseODBC.h> 
#endif

//***************************************************************************
//
class DB_INFO
{
public:
	TCHAR tszDBName[DATABASE_NAME_STRLEN] = { 0, };				// 데이터베이스 명
};

//***************************************************************************
//
class DB_SYSTEM_INFO
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
// MYSQL 캐릭터셋
class MYSQL_CHARACTER_SET
{
public:
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, };			// 캐릭터셋
	TCHAR tszDefaultCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };		// 기본 데이터 정렬(문자비교규칙)
	TCHAR tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };					// 설명
	int32 MaxLen;															// 한 문자를 저장하는 데 필요한 최대 바이트 수
};

//***************************************************************************
// MYSQL 데이터 정렬(문자비교규칙)
class MYSQL_COLLATION
{
public:
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, };			// 캐릭터셋
	TCHAR tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };				// 데이터 정렬(문자비교규칙)
	int64 Id;																// 데이터 정렬 ID
	TCHAR tszIsCompiled[5] = { 0, };										// 문자 집합이 서버에 컴파일되는지 여부
	TCHAR tszIsDefault[5] = { 0, };											// 데이터 정렬이 해당 문자 집합의 기본값인지 여부
	TCHAR tszPadAttribute[10] = { 0, };										// ENUM('PAD SPACE','NO PAD')
	int32 SortLen;															// 문자 집합에 표현된 문자열을 정렬하는 데 필요한 메모리 양
};

//***************************************************************************
// MYSQL 캐릭터셋과 문자비교규칙 연결 정보
class MYSQL_CHARACTER_SET_COLLATION
{
public:
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, };			// 캐릭터셋
	TCHAR tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };				// 데이터 정렬(문자비교규칙)
};

//***************************************************************************
// MYSQL 스토리지 엔진 정보
class MYSQL_STORAGE_ENGINE
{
public:
	TCHAR tszEngine[20] = { 0, };											// 스토리지 엔진 명
	
	// 스토리지 엔진에 대한 서버의 지원 수준
	// - YES : 엔진이 지원되고 활성 상태
	// - DEFAULT : YES와 마찬가지로, 그리고 이것은 기본 엔진
	// - NO : 엔진이 지원되지 않음
	// - DISABLED : 엔진이 지원되지만 비활성 상태	
	TCHAR tszSupport[20] = { 0, };											

	TCHAR tszComment[DATABASE_WVARCHAR_MAX] = { 0, };						// 설명
	TCHAR tszTransactions[5] = { 0, };										// 스토리지 엔진이 트랜잭션을 지원하는지 여부
	TCHAR tszXA[5] = { 0, };												// 스토리지 엔진이 XA 트랜잭션을 지원하는지 여부
	TCHAR tszSavepoints[5] = { 0, };										// 스토리지 엔진이 저장점을 지원하는지 여부
};

//***************************************************************************
// MYSQL 테이블 조각화 정보
class MYSQL_TABLE_FRAGMENTATION
{
public:
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };				// 테이블 명

	// 전체 크기
	// - DATA_LENGTH + INDEX_LENGTH
	// - DATA_LENGTH : MyISAM의 경우 데이터 파일의 길이(바이트), InnoDB의 경우 클러스터형 인덱스에 할당된 대략적인 공간의 양(바이트)
	// - INDEX_LENGTH : MyISAM의 경우 INDEX_LENGTH 인덱스 파일의 길이(바이트), InnoDB의 경우 INDEX_LENGTH 클러스터 되지 않은 인덱스에 할당된 대략적인 공간의 양(바이트)
	uint64  TotalSize;

	// 테이블에 할당되었지만 사용되지 않은 바이트 수
	uint64 DataFreeSize;
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
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN];		// 캐릭터셋
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN];			// 문자비교규칙
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
	bool	IsSystemNamed;									// 시스템이 인덱스명을 할당했는지 여부(true/false : 유/무)
	int32   ColumnSeq;										// 컬럼 순서
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// 컬럼 명
	int8    ColumnSort;										// 정렬(1/2 : ASC/DESC)
};

//***************************************************************************
//
class INDEX_OPTION_INFO
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

class CDBQueryProcess
{
public:
	CDBQueryProcess(CBaseODBC& conn) : _dbClass(conn.GetDBClass()), _dbConn(conn) { }
	~CDBQueryProcess();

	EDBClass GetDBClass() { return _dbClass; }

	// Database 시스템 관련 함수
	bool		GetDBSystemInfo(int32& iSystemCount, std::unique_ptr<DB_SYSTEM_INFO[]>& pDBSystemInfo);
	bool		GetDBSystemDataTypeInfo(int& iDatatypeCount, std::unique_ptr<DB_SYSTEM_DATATYPE[]>& pDBSystemDataType);
	bool		GetDatabaseList(int& iDBCount, std::unique_ptr<DB_INFO[]>& pDatabase);

	// MSSQL 인덱스 조각화 관련 함수
	bool		MSSQLGetRowStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation);
	bool		MSSQLGetColumnStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation);
	bool        MSSQLIndexOptionSet(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, unordered_map<_tstring, _tstring> indexOptions);
	bool		MSSQLAlterIndexFragmentationNonOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation);
	bool		MSSQLAlterIndexFragmentationOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation, unordered_map<_tstring, _tstring> indexOptions);

	// MSSQL 저장프로시저, 함수, 트리거 생성 쿼리 얻어오기 관련 함수 
	_tstring	MSSQLHelpText(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MSSQL 테이블, 컬럼 명 수정
	bool		MSSQLRenameObject(const TCHAR* ptszObjectName, const TCHAR* ptszChgObjectName, const EMSSQLRenameObjectType renameObjectType = EMSSQLRenameObjectType::NONE);

	// MSSQL 테이블, 컬럼 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MSSQLProcessTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDescription);

	// MSSQL 저장프로시저, 파라미터 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam);
	bool		MSSQLProcessProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam, const TCHAR* ptszDescription);

	// MSSQL 함수, 파라미터 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam);
	bool		MSSQLProcessFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam, const TCHAR* ptszDescription);

	// MSSQL 확장 속성을 보기, 추가, 수정, 삭제
	_tstring	MSSQLGetExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLAddExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLUpdateExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLDropExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);

	// MYSQL 테이블 캐릭터셋, 데이터정렬(문자비교규칙), 스토리지엔진 관련 함수
	bool		MYSQLGetCharacterSets(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET[]>& pCharacterSet);
	bool		MYSQLGetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_COLLATION[]>& pCollation);
	bool		MYSQLGetCharacterSetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET_COLLATION[]>& pCharacterSetCollation);
	bool		MYSQLGetStorageEngines(int32& iStorageEngineCount, std::unique_ptr<MYSQL_STORAGE_ENGINE[]>& pStorageEngine);
	bool		MYSQLAlterTable(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation, const TCHAR* ptszEngine);
	bool		MYSQLAlterTableCollation(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation);

	// MYSQL 테이블 조각화 관련 함수
	bool		MYSQLGetTableFragmentationCheck(const TCHAR* ptszTableName, int32& iTableCount, std::unique_ptr<MYSQL_TABLE_FRAGMENTATION[]>& pTableFragmentation);
	bool		MYSQLOptimizeTable(const TCHAR* ptszTableName);

	// MYSQL 테이블, 컬럼 생성 쿼리 얻어오기 관련 함수
	_tstring	MYSQLShowTable(const TCHAR* ptszTableName);

	// MYSQL 저장프로시저/함수/트리거/이벤트 생성 쿼리 얻어오기 관련 함수
	_tstring	MYSQLShowObject(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MYSQL 테이블, 컬럼 명 수정
	bool		MYSQLRenameObject(const TCHAR* ptszTableName, const TCHAR* ptszChgName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL 테이블, 컬럼 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MYSQLProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL 저장프로시저 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetProcedureComment(const TCHAR* ptszProcName);
	bool		MYSQLProcessProcedureComment(const TCHAR* ptszProcName, const TCHAR* ptszDescription);

	// MYSQL 함수 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetFunctionComment(const TCHAR* ptszFuncName);
	bool		MYSQLProcessFunctionComment(const TCHAR* ptszFuncName, const TCHAR* ptszDescription);

private:
	EDBClass	_dbClass;
	CBaseODBC&	_dbConn;
};

#endif // ndef __DBQUERYPROCESS_H__