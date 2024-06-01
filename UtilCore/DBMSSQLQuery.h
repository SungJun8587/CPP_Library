
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
    // columnstore �ε����� ����ȭ Ȯ��
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
	// ���� ��Ƽ�ǿ� ���� ���� �ٸ� ������ ���� ������ �����Ϸ��� DATA_COMPRESSION �ɼ��� �� �� �̻� ����
	// ALTER INDEX [�ε�����] ON [��Ű����].[���̺��]
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