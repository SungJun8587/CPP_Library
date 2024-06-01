
//***************************************************************************
// DBMSSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBMSSQLQUERY_H__
#define __DBMSSQLQUERY_H__

#pragma once

enum class EMSSQLIndexFragmentation
{
	REORGANIZE,
	REBUILD,
	DISABLE
};

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
		default:															return _T("NULL");
	}
}

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
		default:															return _T("NULL");
	}
}

inline const TCHAR* ToString(EMSSQLExtendedPropertyLevel2Type v)
{
	switch( v )
	{
		case EMSSQLExtendedPropertyLevel2Type::COLUMN:						return _T("COLUMN");
		case EMSSQLExtendedPropertyLevel2Type::CONSTRAINT:					return _T("CONSTRAINT");
		case EMSSQLExtendedPropertyLevel2Type::EVENT__NOTIFICATION :		return _T("EVENT NOTIFICATION");
		case EMSSQLExtendedPropertyLevel2Type::INDEX:						return _T("INDEX");
		case EMSSQLExtendedPropertyLevel2Type::PARAMETER:					return _T("PARAMETER");
		case EMSSQLExtendedPropertyLevel2Type::TRIGGER:						return _T("TRIGGER");
		default:															return _T("NULL");
	}
}

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
//
inline _tstring GetMSSQLDatabaseBackupQuery(string databaseName, string backupFilePath)
{
	string query = "BACKUP DATABASE " + databaseName + " TO DISK = '" + backupFilePath + "'";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetMSSQLDatabaseRestoreQuery(string databaseName, string restoreFilePath)
{
	// query = "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	// query = "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "' WITH REPLACE";
	string query = "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetMSSQLDatabaseRestoreQuery(string databaseName, string restoreFilePath, string dataFilePath, string logFilePath)
{
	string query = "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "'";
	query = query + " WITH MOVE '" + databaseName + " TO '" + dataFilePath + "'";
	query = query + ", MOVE '" + databaseName + "_log' TO '" + logFilePath + "'";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MSSQLGetRowStoreIndexFragmentationCheckQuery(string tableName = "", bool isCount = false)
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
	string query = "";
	
	if( isCount )
		query = "SELECT COUNT(*) AS [index_count]";
	else query = "SELECT ips.object_id AS object_id, OBJECT_SCHEMA_NAME(ips.object_id) AS schema_name, OBJECT_NAME(ips.object_id) AS table_name, ips.index_id AS index_id, ISNULL(i.name, '') AS index_name, i.type_desc AS index_type, ips.partition_number AS partitionnum, ips.avg_fragmentation_in_percent, ips.avg_page_space_used_in_percent, ips.page_count, ips.alloc_unit_type_desc";
    query = query + "\n" + "FROM sys.dm_db_index_physical_stats(DB_ID(), NULL, NULL, NULL, 'SAMPLED') AS ips";
    query = query + "\n" + "INNER JOIN sys.indexes AS i";
    query = query + "\n" + "ON ips.object_id = i.object_id AND ips.index_id = i.index_id";
	if( tableName != "" )
        query = query + "\n" + "WHERE OBJECT_NAME(ips.object_id) = '" + tableName + "'";
    
	if( !isCount ) query = query + "\n" + "ORDER BY ips.avg_fragmentation_in_percent DESC";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MSSQLGetColumnStoreIndexFragmentationCheckQuery(string tableName = "", bool isCount = false)
{
    // - https://learn.microsoft.com/ko-kr/sql/relational-databases/indexes/reorganize-and-rebuild-indexes?view=sql-server-ver16
    // columnstore 인덱스의 조각화 확인
	string query = "";

	if( isCount )
		query = "SELECT COUNT(*) AS [index_count]";
	else query = "SELECT i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, OBJECT_NAME(i.object_id) AS table_name, i.index_id AS index_id, ISNULL(i.name, '') AS index_name, i.type_desc AS index_type, 100.0 * (ISNULL(SUM(rgs.deleted_rows), 0)) / NULLIF(SUM(rgs.total_rows), 0) AS avg_fragmentation_in_percent";
    query = query + "\n" + "FROM sys.indexes AS i";
    query = query + "\n" + "INNER JOIN sys.dm_db_column_store_row_group_physical_stats AS rgs";
    query = query + "\n" + "ON i.object_id = rgs.object_id AND i.index_id = rgs.index_id";
	if( tableName != "" )
    {
        query = query + "\n" + "WHERE OBJECT_NAME(i.object_id) = '" + tableName + "' AND rgs.state_desc = 'COMPRESSED'";
        query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
		if( !isCount ) query = query + "\n" + "ORDER BY index_name, index_type, avg_fragmentation_in_percent DESC";
    }
    else
    {
        query = query + "\n" + "WHERE rgs.state_desc = 'COMPRESSED'";
        query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
		if( !isCount ) query = query + "\n" + "ORDER BY table_name, index_name, index_type, avg_fragmentation_in_percent DESC";
    }
	return TStringToString(query);
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
	if( indexName != _T("") )
		query = tstring_format(_T("ALTER INDEX [%s] ON [%s].[%s]"), indexName.c_str(), schemaName.c_str(), tableName.c_str());
	else query = tstring_format(_T("ALTER INDEX %s ON [%s].[%s]"), _T("ALL"), schemaName.c_str(), tableName.c_str());

	if( !indexOptions.empty() )
	{
		query = query + _T("\r\n") + _T("SET (");
		for( auto indexOption = indexOptions.begin(); indexOption != indexOptions.end(); indexOption++ )
		{
			if( i > 0 ) query = query + _T(", ");

			query = query + _T("\r\n\t") + indexOption->first + _T(" = ") + indexOption->second;
			i++;
		}
		query = query + _T("\r\n") + _T(");");
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
	if( indexName != _T("") )
		query = tstring_format(_T("ALTER INDEX [%s] ON [%s].[%s] %s"), indexName.c_str(), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
	else query = tstring_format(_T("ALTER INDEX %s ON [%s].[%s] %s"), _T("ALL"), schemaName.c_str(), tableName.c_str(), ToString(eMSSQLIndexFragmentation));
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
	if( indexName != _T("") )
		query = tstring_format(_T("ALTER INDEX [%s] ON [%s].[%s]"), indexName.c_str(), schemaName.c_str(), tableName.c_str());
	else query = tstring_format(_T("ALTER INDEX %s ON [%s].[%s]"), _T("ALL"), schemaName.c_str(), tableName.c_str());

	if( !indexOptions.empty() )
	{
		query = query + _T("\r\n") + _tstring(ToString(eMSSQLIndexFragmentation)) + _T(" WITH (");
		for( auto indexOption = indexOptions.begin(); indexOption != indexOptions.end(); indexOption++ )
		{
			if( i > 0 ) query = query + _T(", ");

			query = query + _T("\r\n\t") + indexOption->first + _T(" = ") + indexOption->second;
			i++;
		}
		query = query + _T("\r\n") + _T(");");
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetMSSQLHelpText(EDBObjectType dbObject)
{
	_tstring query = _T("");

	switch( dbObject )
	{
	case EDBObjectType::PROCEDURE:
	case EDBObjectType::FUNCTION:
	case EDBObjectType::TRIGGERS:
	case EDBObjectType::EVENTS:
		query = _T("EXEC sp_helptext ?");
		break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetMSSQLRenameObject(_tstring objectName, _tstring chgObjectName, EMSSQLRenameObjectType renameObjectType = EMSSQLRenameObjectType::NONE)
{
	_tstring query = _T("");

	switch( renameObjectType )
	{
		case EMSSQLRenameObjectType::NONE:
			query = tstring_format(_T("EXEC sp_rename '%s', '%s'"), objectName.c_str(), chgObjectName.c_str());
			break;
		default:
			query = tstring_format(_T("EXEC sp_rename '%s', '%s', '%s'"), objectName.c_str(), chgObjectName.c_str(), ToString(renameObjectType));
			break;
	}
	return query;
}

#endif // ndef __DBMSSQLQUERY_H__