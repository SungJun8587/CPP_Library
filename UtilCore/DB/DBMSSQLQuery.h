
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
// MSSQL �ε���Ÿ�� : sys.indexes ���̺� type_desc(nvarchar(60)) �÷�
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
// MSSQL Ȯ�� �Ӽ� ó�� �Ķ����
class MSSQL_ExtendedProperty
{
public:
	_tstring _propertyName;			// �Ӽ��� �̸�
	_tstring _propertyValue;		// �Ӽ��� ������ ��
	_tstring _level0_object_type;	// ���� 0 ��ü�� ����
	_tstring _level0_object_name;	// ���� 0 ��ü ������ �̸�
	_tstring _level1_object_type;	// ���� 1 ��ü�� ����
	_tstring _level1_object_name;	// ���� 1 ��ü ������ �̸�
	_tstring _level2_object_type;	// ���� 2 ��ü�� ����
	_tstring _level2_object_name;	// ���� 2 ��ü ������ �̸�
};

//***************************************************************************
// MSSQL �ε��� ����ȭ ����
class MSSQL_INDEX_FRAGMENTATION
{
public:
public:
	int32	ObjectId;														// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };			// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };				// ���̺� ��
	int32	IndexId;														// �ε��� ������ȣ
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// �ε��� ��
	TCHAR   tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };		// �ε��� Ÿ��
	int32   PartitionNum;													// ��ü�� ��Ƽ�� ��ȣ

	// ���� ����ȭ(�ε������� ������ �߸��� ������) ��ġ
	// - �ε����� ���� ���� ����ȭ �Ǵ� �Ҵ� ������ ���� IN_ROW_DATA ���� �ͽ���Ʈ ����ȭ
	// - avg_fragmentation_in_percent(����ȭ ��ġ)�� 30�̻��̸� ������(Rebuild) 30�̸��̸� �����׳�����(Reorganize) ����
	float	AvgFragmentationInPercent;

	// ��� ������ �е�(��� ���������� ���Ǵ� ��� ������ ������ ���丮�� ������ ��� �����)
	float	AvgPageSpaceUsedInPercent;

	// �� �ε��� �Ǵ� ������ ������ ��
	// - �ε����� ��� �Ҵ� �������� B-Ʈ���� ���� ���ؿ� �ִ� IN_ROW_DATA �� �ε��� ������ ��
	// - ���� ��� �Ҵ� ������ �� ������ ������ IN_ROW_DATA ��(�Ҵ� ������ ROW_OVERFLOW_DATA ��� LOB_DATA �Ҵ� ������ �� ������ ��)
	int32	PageCount;

	// �Ҵ� ���� ������ ���� ����
	// - LOB_DATA : text, ntext, image, varchar(max), nvarchar(max), varbinary(max) �� xml ������ ���� ����� �����Ͱ� ����
	// - ROW_OVERFLOW_DATA : varchar(n), nvarchar(n), varbinary(n) �� �࿡�� Ǫ�õ� sql_variant ������ ���� ����� �����Ͱ� ����
	TCHAR	tszAllocUnitTypeDesc[DATABASE_WVARCHAR_MAX] = { 0, };
};

//***************************************************************************
//
class MSSQL_INDEX_OPTION_INFO
{
public:
	int32	ObjectId;															// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];							// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];							// ���̺� ��
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];							// �ε��� ��
	int32	IndexId;															// �ε��� ������ȣ
	bool    IsPrimaryKey;														// �⺻Ű ����(true/false : ��/��)
	bool    IsUnique;															// ����ũ ����(true/false : ��/��)

	/// <summary>�ε��� ��Ȱ��ȭ ����(0/1 : Ȱ��ȭ/��Ȱ��ȭ)</summary>
	bool	IsDisabled;

	/// <summary>PAD_INDEX = { ON | OFF }
	/// - �ε��� �е��� ����. �⺻���� OFF
	/// - Ŭ�������� columnstore �ε����� ��� �׻� 0
	/// </summary>
	bool	IsPadded;

	/// <summary>FILLFACTOR = <fillfactor ��>
	/// - �� �ε��� �������� ���� ������ ��� ���� ä���� ��Ÿ���� ������ ����
	/// - Ŭ�������� columnstore �ε����� ��� �׻� 0
	/// </summary>
	int8	FillFactor;

	/// <summary>IGNORE_DUP_KEY = { ON | OFF }
	/// - ���� �۾����� ���� �ε����� �ߺ��� Ű ���� �����Ϸ��� �� �� ���� ������ ����
	/// - ON�̸� �ߺ��� Ű ���� ���� �ε����� ���ԵǴ� ��� ��� �޽����� ��Ÿ���� ������ ���� ������ �����ϴ� �ุ ����
	/// - OFF�̸� �ߺ��� Ű ���� ���� �ε����� ���ԵǴ� ��� ���� �޽����� ��Ÿ���� ��ü INSERT �۾��� �ѹ�
	/// - ��, ����� �ε���, XML �ε���, ���� �ε��� �� ���͸��� �ε����� ������ �ε����� ��� IGNORE_DUP_KEY�� ON���� ������ �� ����
	/// </summary>
	bool	IgnoreDupKey;

	/// <summary>ALLOW_ROW_LOCKS = { ON | OFF }
	/// - �ε������� �� ��� ��� ����(0/1 : �����/���)
	/// - Ŭ�������� columnstore �ε����� ��� �׻� 0
	/// </summary>
	bool	AllowRowLocks;

	/// <summary>ALLOW_PAGE_LOCKS = { ON | OFF }
	/// - �ε������� ������ ��� ��� ����(0/1 : �����/���)
	/// - Ŭ�������� columnstore �ε����� ��� �׻� 0
	/// </summary>    	
	bool	AllowPageLocks;

	/// <summary>�ε��� ���� ���� ����(0/1 : ��/��)</summary>
	bool	HasFilter;

	/// <summary>���͸��� �ε����� ���Ե� ���� ���� ���տ� ���� ��
	/// - ��, ���͸����� ���� �ε��� �Ǵ� ���̺� ���� ������ ������ ��� NULL
	/// </summary>
	TCHAR   tszFilterDefinition[DATABASE_WVARCHAR_MAX];

	/// <summary>COMPRESSION_DELAY = { 0 | duration [Minutes] }
	/// - SQL Server 2016(13.x) ���� �̻�
	/// - ��ũ ��� ���̺��� ��� delay�� CLOSED ������ ��Ÿ rowgroup�� ��Ÿ rowgroup�� ���� �־�� �ϴ� �ּ� �ð�(��)�� ����. �⺻���� 0��
	/// - > 0�̸� Columnstore �ε��� ���� ���� �ð�(��)
	/// - NULL�̸� Columnstore �ε��� �� �׷� ���� ������ �ڵ����� ����
	/// </summary>	
	int32	CompressionDelay;

	/// <summary>OPTIMIZE_FOR_SEQUENTIAL_KEY = { ON | OFF }
	/// - SQL Server 2019(15.x) ���� �̻� �� Azure SQL Database
	/// - ������ ������ ���� ���տ� ����ȭ���� ���θ� ����. �⺻���� OFF
	/// - ON�̸� �ε����� ������ ������ ���� ����ȭ�� ����ϵ��� ����
	/// - OFF�̸� �ε����� ������ ������ ���� ����ȭ�� ������� �ʵ��� ����
	/// </summary>
	bool	OptimizeForSequentialKey;

	/// <summary>STATISTICS_NORECOMPUTE = { ON | OFF }
	/// - sys.stats ���̺� ����
	/// - ��踦 �ٽ� ������� ���θ� ����. �⺻���� OFF
	/// - ON�̸� �ɼ��� ����Ͽ� ��谡 ������
	/// - OFF�̸� �ɼ��� ����Ͽ� ��谡 �������� ����
	/// </summary>	
	bool	StatisticsNoRecompute;

	/// <summary>STATISTICS_INCREMENTAL = { ON | OFF }
	/// - SQL Server 2014(12.x) ���� �̻� �� Azure SQL Database
	/// - sys.stats ���̺� ����
	/// - ��踦 ���� ���� �������� ����. �⺻���� OFF
	/// - ON�̸� ��谡 ���е�
	/// - OFF�̸� ��谡 ���е��� ����
	/// </summary>	
	bool	StatisticsIncremental;

	/// <summary>DATA_COMPRESSION = { NONE | ROW | PAGE | COLUMNSTORE | COLUMNSTORE_ARCHIVE }
	/// - �� ��Ƽ�ǿ� ���� ���� ����(0/1/2/3/4 : NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	/// - SQL Server 2014(12.x) ���� �̻� �� Azure SQL Database
	/// - sys.partitions ���̺� ����
	/// </summary>
	int8	DataCompression;

	/// <summary>�� ��Ƽ�ǿ� ���� ���� ���� ���� ���ڿ�(NONE/ROW/PAGE/COLUMNSTORE/COLUMNSTORE_ARCHIVE)
	/// - SQL Server 2014(12.x) ���� �̻� �� Azure SQL Database
	/// - sys.partitions ���̺� ����
	/// </summary>
	TCHAR   tszDataCompressionDesc[DATABASE_BASE_STRLEN];

	/// <summary>XML_COMPRESSION = { ON | OFF }
	/// - �� ��Ƽ�ǿ� ���� XML ���� ����
	/// - SQL Server 2022(16.x) ���� �̻� �� Azure SQL Database
	/// - sys.partitions ���̺� ����
	/// </summary>	
	bool	XmlCompression;

	/// <summary>�� ��Ƽ�ǿ� ���� XML ���� ���� ���� ���ڿ�(OFF/ON)
	/// - SQL Server 2022(16.x) ���� �̻� �� Azure SQL Database
	/// - sys.partitions ���̺� ����
	/// </summary>
	TCHAR   tszXmlCompressionDesc[DATABASE_BASE_STRLEN];

	/// <summary>�����ͺ��̽� ������ ������ ������ ������ �̸�
	/// - sys.data_spaces ���̺� ����
	/// </summary>
	TCHAR   tszFileGroupOrPartitionScheme[DATABASE_OBJECT_TYPE_DESC_STRLEN];

	/// <summary>������ ���� ������ ���� ����
	/// - sys.data_spaces ���̺� ����
	/// </summary>
	TCHAR   tszFileGroupOrPartitionSchemeName[DATABASE_OBJECT_NAME_STRLEN];

	/// <summary>SORT_IN_TEMPDB = { ON | OFF }
	/// - tempdb�� ���� ����� �������� ���θ� ����. Azure SQL Database �����۽������� �����ϰ� �⺻���� OFF
	/// - ���� �۾��� �ʿ����� �ʰų� �޸𸮿��� ������ ������ �� ������ SORT_IN_TEMPDB �ɼ��� ����
	/// - ON�̸� �ε��� �ۼ��� ���� �߰� ���� ����� tempdb�� ����(tempdb�� ����� �����ͺ��̽��ʹ� �ٸ� ��ũ ���տ� ����)
	/// - ON�̸� �ε����� ����� �� �ʿ��� �ð��� �پ�� �� �ֽ��ϴ�. �׷��� �ε��� �ۼ� �߿� ���Ǵ� ��ũ ������ ũ��� Ŀ���ϴ�.
	/// - OFF�̸� �߰� ���� ����� �ε����� ���� �����ͺ��̽��� ����
	/// </summary>
	bool SortInTempDB;

	/// <summary>ONLINE = { ON | OFF }
	/// - SQL Server 2008(10.0.x) ���� �̻�. �¶��� �ε��� �۾��� SQL Server Enterprise Edition �Ǵ� Azure SQL Edge������ ����
	/// - �ε��� �۾� �� ���� �� ������ ������ �⺻ ���̺�� ���� �ε����� ����� �� �ִ��� ���θ� ����. �⺻���� OFF
	/// - rebuild_index_option�� ����
	/// - ON�̸� �ε��� �۾� �߿� ��� ���̺� ����� �������� ����
	/// - OFF�̸� �ε��� �۾� �߿� ���̺� ����� ����
	/// </summary>
	bool Online;

	/// <summary>MAXDOP = max_degree_of_parallelism
	/// - �ε��� �۾� ���� max degree of parallelism ���� �ɼ��� ������ 
	///     1 : ���� ��ȹ�� �������� �ʽ��ϴ�.
	///     > 1 : ���� �ε��� �۾��� ���Ǵ� �ִ� ���μ��� ���� ������ ������ �����մϴ�.
	///     0(�⺻��) : ���� �ý��� �۾��� ���� ���� ���μ��� �� ������ ���μ����� ����մϴ�.
	/// - MAXDOP �ɼ��� ��� XML �ε����� ���� ������ ���������� ���� �ε��� �Ǵ� �⺻ XML �ε����� ��� ALTER INDEX�� ���� ���� ���μ����� ����մϴ�.
	/// - SQL Server �ν��Ͻ��� ���� ���� �� �ϳ�
	///     SELECT value 
	///     FROM sys.configurations 
	///     WHERE name = 'max degree of parallelism';
	/// </summary>
	uint8 MaxDop;

	/// <summary>RESUMABLE = { ON | OFF }
	/// SQL Server 2017(14.x) ���� �̻� �� Azure SQL Database
	/// - ALTER TABLE ADD CONSTRAINT �۾��� �ٽ� ���۵� �� �ִ��� ���θ� ����. �⺻���� OFF
	/// - ON�̸� ���̺� ���� ���� �߰� �۾� �ٽ� ���� ����
	/// - OFF�̸� ���̺� ���� ���� �߰� �۾� �ٽ� ���� �Ұ���
	/// - RESUMABLE �ɼ��� ON���� �����Ǹ� ONLINE = ON �ɼ��� �ʿ�
	/// - RESUMABLE �ɼ��� ON���� �����Ǿ� ���� ������ MAX_DURATION(= time [MINUTES]) �ɼ��� ������ ����
	/// - MAX_DURATION�� RESUMABLE �۾��� �ִ� ���� �ð��� �����ϴ� �� ���� �� ����
	/// - SQL Server ������Ʈ �۾� �������� ����
	///     SELECT s.name, s.command
	///     FROM msdb.dbo.sysjobs AS j
	///     INNER JOIN msdb.dbo.sysjobsteps AS s 
	///     ON j.job_id = s.job_id
	///     WHERE command LIKE '%RESUMABLE=ON%';        
	/// </summary>
	bool Resumable;

	/// <summary>MAX_DURATION = time [MINUTES] 
	/// - SQL Server 2017(14.x) ���� �̻�
	/// - �ε��� �籸�� �۾��� �ִ� ���� �ð�
	/// - �ٽ� ������ �� �ִ� �¶��� �ε��� �۾��� �Ͻ� �����ϱ� ���� ����� �ð��� ��Ÿ��(�� ������ ������ ���� ��)
	/// - SQL Server ������Ʈ �۾� �������� ����
	///     SELECT s.name, s.command
	///     FROM msdb.dbo.sysjobs AS j
	///     INNER JOIN msdb.dbo.sysjobsteps AS s 
	///     ON j.job_id = s.job_id
	///     WHERE command LIKE '%MAX_DURATION%';        
	/// </summary>   
	int32 MaxDuration;

	/// <summary>ABORT_AFTER_WAIT = [ NONE | SELF | BLOCKERS ]
	/// - MAX_DURATION �ð� ���� ���ܵǸ� ������ ABORT_AFTER_WAIT �۾��� ����
	/// - NONE : ����(�Ϲ�) �켱 ������ ����� ��� ����մϴ�.
	/// - SELF : � ���۵� �������� �ʰ� ���� ���� ���� �¶��� �ε��� �ٽ� �ۼ� DDL �۾��� ����. MAX_DURATION�� 0�̸� SELF �ɼ��� ����� �� ����.
	/// - BLOCKERS : �۾��� ����� �� �ֵ��� �¶��� �ε��� �ٽ� �ۼ� DDL �۾��� �����ϴ� ��� ����� Ʈ������� ����. BLOCKERS �ɼ��� ����Ϸ��� �α��ο� ALTER ANY CONNECTION ������ �־�� ��.
	/// </summary>
	int8 AbortAfterWait;

	/// <summary>LOB_COMPACTION = { ON | OFF }
	/// - rowstore �ε����� ����
	/// - image, text, ntext, varchar(max), nvarchar(max), varbinary(max), xml�� ���� ū ��ü(LOB) ������ ������ �����͸� �����ϴ� ��� �������� �����ϵ��� ����
	/// - reorganize_option�� ����
	/// - ON�̸� Ŭ�������� �ε����� ��� ���̺� ���Ե� ��� LOB ���� ����, ��Ŭ�������� �ε����� ��� �ε������� Ű�� �ƴ�(����) ���� LOB ���� ��� ����
	/// - OFF�̸� ū ��ü �����Ͱ� ���Ե� �������� ������� ������, ������ �ƹ� ������ ���� 
	/// </summary>
	bool LobCompaction;

	/// <summary>COMPRESS_ALL_ROW_GROUPS = { ON | OFF }
	/// - SQL Server 2016(13.x) ���� �̻� �� Azure SQL Database
	/// - columnstore �ε����� ����
	/// - ����(OPEN) �Ǵ� ����(CLOSED) ��Ÿ rowgroup�� columnstore�� ���� �����ϴ� ����� ����
	/// - reorganize_option�� ����
	/// - ON�� ũ�� �� ����(CLOSED �Ǵ� OPEN)�� ������� ��� rowgroup�� columnstore�� ���� ����
	/// - OFF�� ��� ����(CLOSED) rowgroup�� columnstore�� ���� ����
	/// </summary>
	bool CompressAllRowGroups;
};

//***************************************************************************
//
class MSSQL_DEFAULT_CONSTRAINT_INFO
{
public:
	int32	ObjectId;												// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// ���̺� ��
	TCHAR   tszDefaultConstName[DATABASE_OBJECT_NAME_STRLEN];		// �⺻�� �������� ��
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];				// �÷� ��
	TCHAR   tszDefaultValue[DATABASE_WVARCHAR_MAX];					// �÷� �������� ���ǰ�
	bool	IsSystemNamed;											// �ý����� �ε������� �Ҵ��ߴ��� ����(true/false : ��/��)
};

//***************************************************************************
//
inline _tstring MSSQLGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, bool isIdentity, ulong seedValue, ulong incrementValue, _tstring collation = _T(""))
{
	_tstring columnOption = _T("");

	// <�÷��Ӽ�> : COLLATE, {NULL|NOT NULL}, IDENTITY(�õ尪, ���а�) ������ ���Ե�
	//  - ���� �÷� �Ӽ��� ���Ե� ���� ���� ������ ��� �Ʒ��� ���� ������ �����ؼ� �����ϸ� ��
	//  - ���� ���������� ���� �ش� �����ͺ��̽��� ������ ���������� ���� ������ ��� ���� ������� �ʾƵ� ��
	//  - COLLATE ���������� {NULL|NOT NULL} IDENTITY(�õ尪, ���а�)
	//
	// [�÷���] ������Ÿ�� <�÷��Ӽ�>
	//  - [�÷���] ������Ÿ�� NOT NULL IDENTITY(�õ尪, ���а�)
	//  - [�÷���] ������Ÿ�� {NULL|NOT NULL}
	//  - [�÷���] ������Ÿ�� COLLATE ���������� {NULL|NOT NULL}
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
		// ALTER TABLE [���̺��] DROP CONSTRAINT [�������Ǹ�]
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
	// rowstore �ε����� ����ȭ �� ������ �е� Ȯ��
	// avg_fragmentation_in_percent(����ȭ ��ġ)�� 30�̻��̸� ������(Rebuild) 30�̸��̸� �����׳�����(Reorganize) ����
	// sys.dm_db_index_physical_stats([database_id], [object_id], [index_id], [partition_number], [mode]) 
	//  [database_id] | NULL | 0 | �⺻ : �����ͺ��̽��� ID
	//  [object_id] | NULL | 0 | �⺻ : �ε����� �ִ� ���̺� �Ǵ� ���� ��ü ID
	//  [index_id] | 0 | NULL | -1 | �⺻ : �ε����� ID
	//  [partition_number] | NULL | 0 | �⺻ : ��ü�� ��Ƽ�� ��ȣ
	//  [mode] | NULL | �⺻ : mode�� �̸�. mode�� ��踦 �������� �� ���Ǵ� �˻� ������ ����
	//  - DEFAULT/NULL/LIMITED/SAMPLED/DETAILED. �⺻��(NULL)�� LIMITED
	//  - DETAILED�� ��� ��� �ε��� �������� �˻��ؾ� �ϸ� �ð��� ���� �ɸ� �� �ֽ��ϴ�
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
	// columnstore �ε����� ����ȭ Ȯ��
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
// �ε��� Reorganization(�籸��)
//  - ������ ���Ǵ� Page ������ ������� �ٽ� �����ϴ� �۾�
//  - Rebuilding ���� ���ҽ��� �� ���ǹǷ�, �� ����� �⺻ �ε��� ���� ���� ������� ����ϴ� �� �ٶ�����
//  - �ε��� ����ȭ�� ���� ���� ��쿡 ����ϴ� ���� ����(�� 30% �̸��� ����ȭ�� �߻��� ���)
//  - �¶��� �۾��̱� ������, ��Ⱓ�� object-level locks�� �߻����� ������ Reorganization �۾��߿� �⺻ ���̺� ���� ������ ������Ʈ �۾��� ��� ������ �� �ִ�
// �ε��� Rebuilding(�����)
//  - ������ �ε����� �����ϰ� ������ϴ� ���
//  - �ε����� ��� Row�� �˻�Ǹ�, ��赵 ������Ʈ(Full-Scan) �Ǿ� �ֽ� ���°� �ǹǷ�, �⺻ ���ø��� ��� ������Ʈ�� ���� DB�� ������ ���Ǵ� ��쵵 �ִ�
//  - �ε��� ����ȭ�� ���� ��� ����ϴ� ���� ����(�� 30% �̻��� ����ȭ�� �߻��� ���)
//  - �ε��� ���� �� DB ������ ���� �¶���/������������ ������, �������� �ε��� �������� �¶��� ��Ŀ� ���� �ð��� �� �ɸ�����, ������ �۾� �߿� object-level�� Lock�� ������
// �ε��� Disable(��Ȱ��ȭ)
inline _tstring MSSQLAlterIndexFragmentationNonOptionQuery(_tstring schemaName, _tstring tableName, _tstring indexName, EMSSQLIndexFragmentation eMSSQLIndexFragmentation)
{
	_tstring query = _T("");

	// avg_fragmentation_in_percent(����ȭ ��ġ)�� 30�̻��̸� ������(Rebuild) 30�̸��̸� �����׳�����(Reorganize) ����
	// - �ε��� �ٽ� ���� : ALTER INDEX [�ε�����] ON [���̺��] REORGANIZE;
	// - ���̺��� ��� �ε��� �ٽ� ���� : ALTER INDEX ALL ON [���̺��] REORGANIZE;
	// - �ε��� �ٽ� �ۼ� : ALTER INDEX [�ε�����] ON [���̺��] REBUILD;
	// - ���̺��� ��� �ε��� �ٽ� �ۼ� : ALTER INDEX ALL ON [���̺��] REBUILD;
	// - �ε����� ��Ȱ��ȭ : ALTER INDEX [�ε�����] ON [���̺��] DISABLE;
	// - ���̺��� ��� �ε����� ������� �ʵ��� ���� : ALTER INDEX ALL ON [���̺��] DISABLE;
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
	// ���� ��Ƽ�ǿ� ���� ���� �ٸ� ������ ���� ������ �����Ϸ��� DATA_COMPRESSION �ɼ��� �� �� �̻� ����
	// ALTER INDEX [�ε�����] ON [��Ű����].[���̺��]
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