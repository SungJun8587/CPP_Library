
//***************************************************************************
// DBSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBSQLQUERY_H__
#define __DBSQLQUERY_H__

#pragma once

inline _tstring GetDBSystemQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT CONCAT('MSSQL ', CAST(SERVERPROPERTY('PRODUCTVERSION') AS VARCHAR(50))) AS [version], @@LANGUAGE AS [characterset], collation_name AS [collation]";
			query = query + "\n" + "FROM sys.databases";
			query = query + "\n" + "WHERE name = DB_NAME();";
			break;
		case EDBClass::MYSQL:
			query = "SELECT CONCAT('MYSQL ', VERSION()) AS `version`, `DEFAULT_CHARACTER_SET_NAME` AS `characterset`, DEFAULT_COLLATION_NAME AS 'collation'";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.SCHEMATA";
			query = query + "\n" + "WHERE `SCHEMA_NAME` = DATABASE();";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetDBSystemDataTypeQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT system_type_id, UPPER([name]) AS datatype, max_length, [precision], scale, collation_name, is_nullable";
			query = query + "\n" + "FROM sys.types";
			query = query + "\n" + "WHERE system_type_id = user_type_id";
			query = query + "\n" + "ORDER BY [name] ASC;";
			break;
		case EDBClass::MYSQL:
			// MYSQL은 시스템 데이터타입 목록에 대한 시스템 테이블이 존재하지 않음
			break;
	}
	return TStringToString(query);
}

inline _tstring GetDatabaseListQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT [name] FROM sys.databases WHERE [name] NOT IN('model', 'msdb', 'pubs', 'Northwind', 'tempdb') ORDER BY [name] ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `SCHEMA_NAME` FROM INFORMATION_SCHEMA.SCHEMATA WHERE `SCHEMA_NAME` NOT IN('sys', 'mysql', 'information_schema', 'performance_schema', 'world') ORDER BY `SCHEMA_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetTableListQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, 1 AS object_type, name AS object_name";
			query = query + "\n" + "FROM sys.tables";
			query = query + "\n" + "WHERE type = 'U'";
			query = query + "\n" + "ORDER BY name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `TABLE_SCHEMA` AS `db_name`, 1 AS object_type, `TABLE_NAME` AS `object_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";
			query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
			query = query + "\n" + "ORDER BY `TABLE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetTableInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, t.object_id AS object_id, SCHEMA_NAME(t.schema_id) AS schema_name, t.name AS table_name, ";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT a.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, a.`TABLE_NAME` AS `table_name`, IFNULL(a.`AUTO_INCREMENT`, 0) AS `auto_increment`, ";
			query = query + "a.`ENGINE` AS `engine`, b.`CHARACTER_SET_NAME` AS `characterset`, a.`TABLE_COLLATION` AS `collation`, ";
			query = query + "a.`TABLE_COMMENT` AS `table_comment`, a.`CREATE_TIME` AS `create_date`, a.`UPDATE_TIME` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES AS a";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY AS b";
			query = query + "\n" + "ON a.`TABLE_COLLATION` = b.`COLLATION_NAME`";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE a.`TABLE_SCHEMA` = DATABASE() AND a.`TABLE_TYPE` = 'BASE TABLE' AND a.`TABLE_NAME` = '" + tableName + "';";
			}
			else
			{
				query = query + "\n" + "WHERE a.`TABLE_SCHEMA` = DATABASE() AND a.`TABLE_TYPE` = 'BASE TABLE'";
				query = query + "\n" + "ORDER BY a.`TABLE_NAME` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetTableColumnInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, col.object_id AS object_id, OBJECT_SCHEMA_NAME(col.object_id) AS schema_name, col.table_name AS table_name, col.column_id AS seq, col.name AS column_name, UPPER(TYPE_NAME(col.user_type_id)) AS datatype, ";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT `TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `TABLE_NAME` AS `table_name`, `ORDINAL_POSITION` AS `seq`, `COLUMN_NAME` AS `column_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(`COLUMN_TYPE`) AS `datatype_desc`, ";
			query = query + "(CASE `IS_NULLABLE` WHEN 'YES' THEN true ELSE false END) AS `is_nullable`, ";
			query = query + "(CASE `EXTRA` WHEN 'auto_increment' THEN true ELSE false END) AS `is_identity`, 0 AS `seed_value`, 0 AS `inc_value`, ";
			query = query + "'' AS `default_constraintname`, (CASE `COLUMN_DEFAULT` WHEN 'CURRENT_TIMESTAMP' THEN 'Now()' ELSE CONCAT('''', `COLUMN_DEFAULT`, '''') END) AS `default_definition`, ";
			query = query + "`CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`, ";
			query = query + "`COLUMN_COMMENT` AS `column_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.COLUMNS";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "'";
				query = query + "\n" + "ORDER BY `ORDINAL_POSITION` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
				query = query + "\n" + "ORDER BY `TABLE_NAME` ASC, `ORDINAL_POSITION` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetIndexInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.index_id, i.type AS index_kind, 0 AS index_type, i.is_primary_key, ";
			query = query + "i.is_unique, ISNULL(kc.is_system_named, 0) AS is_system_named, ic.index_column_id AS column_seq, COL_NAME(ic.object_id, ic.column_id) AS column_name, ";
			query = query + "(CASE is_descending_key WHEN 1 THEN 2 ELSE 1 END) AS column_sort";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT stat.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, stat.`TABLE_NAME` AS `table_name`, stat.`INDEX_NAME` AS `index_name`, '0' AS `index_id`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 ELSE 2 END) AS `index_kind`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 WHEN 'UNIQUE' THEN 2 WHEN 'INDEX' THEN 3 WHEN 'FULLTEXT' THEN 4 WHEN 'SPATIAL' THEN 5 ELSE 0 END) AS `index_type`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN true ELSE false END) AS `is_primary_key`, ";
			query = query + "(CASE stat.`NON_UNIQUE` WHEN 0 THEN true ELSE false END) AS `is_unique`, 0 AS `is_system_named`, stat.`SEQ_IN_INDEX` AS `column_seq`, ";
			query = query + "stat.`COLUMN_NAME` AS `column_name`, (CASE stat.`COLLATION` WHEN 'D' THEN 2 ELSE 1 END) AS `column_sort`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.STATISTICS AS stat";
			query = query + "\n" + "LEFT OUTER JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "ON stat.`TABLE_SCHEMA` = const.`TABLE_SCHEMA` AND stat.`TABLE_NAME` = const.`TABLE_NAME` AND stat.`INDEX_NAME` = const.`CONSTRAINT_NAME`";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE stat.`TABLE_SCHEMA` = DATABASE() AND stat.`TABLE_NAME` = '" + tableName + "' AND (const.`CONSTRAINT_TYPE` IN('PRIMARY KEY', 'UNIQUE') OR const.CONSTRAINT_TYPE IS NULL)";
				query = query + "\n" + "ORDER BY stat.`INDEX_NAME` DESC, stat.`SEQ_IN_INDEX` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE stat.`TABLE_SCHEMA` = DATABASE() AND (const.`CONSTRAINT_TYPE` IN('PRIMARY KEY', 'UNIQUE') OR const.CONSTRAINT_TYPE IS NULL)";
				query = query + "\n" + "ORDER BY stat.`TABLE_NAME` ASC, stat.`INDEX_NAME` DESC, stat.`SEQ_IN_INDEX` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetIndexOptionInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.type_desc, i.is_primary_key, i.is_unique, ";
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
			break;
		case EDBClass::MYSQL:
			break;
	}
	return TStringToString(query);
}

inline _tstring GetPartitionInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.name AS table_name, ic.column_id AS partition_column_id, c.name AS partition_column_name, i.name AS index_name, ";
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
			{
				query = query + "\n" + "ORDER BY p.partition_number ASC;";
			}
			else
			{
				query = query + "\n" + "ORDER BY i.table_name ASC, p.partition_number ASC;";
			}
			break;
		case EDBClass::MYSQL:
			query = "SELECT `TABLE_SCHEMA`, `TABLE_NAME`, `PARTITION_NAME`, `SUBPARTITION_NAME`, `PARTITION_ORDINAL_POSITION`, `SUBPARTITION_ORDINAL_POSITION`, `PARTITION_METHOD`, `SUBPARTITION_METHOD`, `PARTITION_EXPRESSION`, `SUBPARTITION_EXPRESSION`, `PARTITION_DESCRIPTION`, `TABLE_ROWS`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.PARTITIONS";
			if( tableName != "" )
				query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "' AND `PARTITION_NAME` IS NOT NULL";
			else query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `PARTITION_NAME` IS NOT NULL";
			query = query + "\n" + "GROUP BY `TABLE_NAME`, `PARTITION_NAME`, `SUBPARTITION_NAME`, `PARTITION_ORDINAL_POSITION`, `SUBPARTITION_ORDINAL_POSITION`, `PARTITION_METHOD`, `SUBPARTITION_METHOD`, `PARTITION_EXPRESSION`, `SUBPARTITION_EXPRESSION`,  `PARTITION_DESCRIPTION`, `TABLE_ROWS`";
			if( tableName != "" )
				query = query + "\n" + "ORDER BY `PARTITION_METHOD` ASC, `PARTITION_ORDINAL_POSITION` ASC,  `SUBPARTITION_METHOD` ASC, `SUBPARTITION_ORDINAL_POSITION` ASC;";
			else query = query + "\n" + "ORDER BY `TABLE_NAME` ASC, `PARTITION_METHOD` ASC, `PARTITION_ORDINAL_POSITION` ASC,  `SUBPARTITION_METHOD` ASC, `SUBPARTITION_ORDINAL_POSITION` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetForeignKeyInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, fk.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(fk.parent_object_id) AS schema_name, fk.table_name AS table_name, fk.name AS foreignkey_name, fk.is_disabled, fk.is_not_trusted, ";
			query = query + "OBJECT_NAME(fkcol.parent_object_id) AS foreignkey_table_name, COL_NAME(fkcol.parent_object_id, fkcol.parent_column_id) AS foreignkey_column_name, ";
			query = query + "OBJECT_SCHEMA_NAME(fkcol.referenced_object_id) AS referencekey_schema_name, OBJECT_NAME(fkcol.referenced_object_id) AS referencekey_table_name, COL_NAME(fkcol.referenced_object_id, fkcol.referenced_column_id) AS referencekey_column_name, ";
			query = query + "REPLACE(fk.update_referential_action_desc, '_', ' ') AS update_rule, REPLACE(fk.delete_referential_action_desc, '_', ' ') AS delete_rule";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT const.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, colusage.`CONSTRAINT_NAME` AS `foreignkey_name`, 0 AS `is_disabled`, 0 AS `is_not_trusted`, ";
			query = query + "colusage.`TABLE_NAME` AS `foreignkey_table_name`, colusage.`COLUMN_NAME` AS `foreignkey_column_name`, ";
			query = query + "'' AS referencekey_schema_name, colusage.`REFERENCED_TABLE_NAME` AS `referencekey_table_name`, colusage.`REFERENCED_COLUMN_NAME` AS `referencekey_column_name`, ";
			query = query + "refconst.`UPDATE_RULE` AS `update_rule`, refconst.`DELETE_RULE` AS `delete_rule`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS colusage";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = colusage.TABLE_SCHEMA AND const.`TABLE_NAME` = colusage.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = colusage.`CONSTRAINT_NAME`";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS refconst";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = refconst.`CONSTRAINT_SCHEMA` AND const.`TABLE_NAME` = refconst.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = refconst.`CONSTRAINT_NAME`";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + tableName + "' AND const.`CONSTRAINT_TYPE` IN('FOREIGN KEY')";
				query = query + "\n" + "ORDER BY const.`CONSTRAINT_NAME` ASC, colusage.`ORDINAL_POSITION` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`CONSTRAINT_TYPE` IN('FOREIGN KEY')";
				query = query + "\n" + "ORDER BY const.`TABLE_NAME` ASC, const.`CONSTRAINT_NAME` ASC, colusage.`ORDINAL_POSITION` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetDefaultConstInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, const.name AS default_const_name, ";
			query = query + "COL_NAME(const.parent_object_id, const.parent_column_id) AS column_name, const.definition AS default_value";
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
			break;
		case EDBClass::MYSQL:
			break;
	}
	return TStringToString(query);
}

inline _tstring GetCheckConstInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, const.name AS check_const_name, const.definition AS check_value";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT const.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, const.`CONSTRAINT_NAME` AS `check_const_name`, ckconst.`CHECK_CLAUSE` AS `check_value`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS ckconst";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = ckconst.`CONSTRAINT_SCHEMA` AND const.`CONSTRAINT_NAME` = ckconst.`CONSTRAINT_NAME`";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + tableName + "' AND const.`CONSTRAINT_TYPE` IN('CHECK')";
				query = query + "\n" + "ORDER BY const.`CONSTRAINT_NAME` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`CONSTRAINT_TYPE` IN('CHECK')";
				query = query + "\n" + "ORDER BY const.`TABLE_NAME` ASC, const.`CONSTRAINT_NAME` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetTriggerInfoQuery(EDBClass dbClass, string tableName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, tr.parent_id AS object_id, OBJECT_SCHEMA_NAME(tr.parent_id) AS schema_name, tr.table_name AS table_name, tr.name AS trigger_name";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT `TRIGGER_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `EVENT_OBJECT_TABLE` AS `table_name`, `TRIGGER_NAME` AS `trigger_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TRIGGERS";
			if( tableName != "" )
			{
				query = query + "\n" + "WHERE `TRIGGER_SCHEMA` = DATABASE() AND `EVENT_OBJECT_TABLE` = '" + tableName + "'";
				query = query + "\n" + "ORDER BY `TRIGGER_NAME` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE `TRIGGER_SCHEMA` = DATABASE()";
				query = query + "\n" + "ORDER BY `EVENT_OBJECT_TABLE` ASC, `TRIGGER_NAME` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetProcedureListQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, 2 AS object_type, name AS object_name";
			query = query + "\n" + "FROM sys.procedures";
			query = query + "\n" + "ORDER BY name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `ROUTINE_SCHEMA` AS `db_name`, 2 AS `object_type`, `ROUTINE_NAME` AS `object_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE'";
			query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetProcedureInfoQuery(EDBClass dbClass, string procName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = query + "SELECT DB_NAME() AS db_name, stproc.object_id AS object_id, SCHEMA_NAME(stproc.schema_id) AS schema_name, stproc.name AS proc_name, CAST(ep.[value] AS NVARCHAR(4000)) AS proc_comment, ";
			query = query + "CONVERT(VARCHAR(23), stproc.create_date, 121) AS create_date, CONVERT(VARCHAR(23), stproc.modify_date, 121) AS modify_date";
			query = query + "\n" + "FROM sys.procedures AS stproc";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON stproc.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";
			if( procName != "" )
				query = query + "\n" + "WHERE stproc.name = '" + procName + "';";
			else query = query + "\n" + "ORDER BY stproc.name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `ROUTINE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `proc_name`, `ROUTINE_COMMENT` AS `proc_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			if( procName != "" )
			{
				query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = '" + procName + "';";
			}
			else
			{
				query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE'";
				query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetProcedureParamInfoQuery(EDBClass dbClass, string procName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.proc_name AS proc_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT `SPECIFIC_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `SPECIFIC_NAME` AS `proc_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
			query = query + "`PARAMETER_NAME` AS `param_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.PARAMETERS";
			if( procName != "" )
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'PROCEDURE') AND `SPECIFIC_NAME` = '" + procName + "'";
				query = query + "\n" + "ORDER BY `ORDINAL_POSITION` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'PROCEDURE')";
				query = query + "\n" + "ORDER BY `SPECIFIC_NAME` ASC, `ORDINAL_POSITION` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

inline _tstring GetFunctionListQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, 3 AS object_type, name AS object_name";
			query = query + "\n" + "FROM sys.objects";
			query = query + "\n" + "WHERE type IN ('FN', 'IF', 'TF')";
			query = query + "\n" + "ORDER BY name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `ROUTINE_SCHEMA` AS `db_name`, 3 AS `object_type`, `ROUTINE_NAME` AS `object_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'";
			query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetFunctionInfoQuery(EDBClass dbClass, string funcName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT `ROUTINE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `func_name`, `ROUTINE_COMMENT` AS `func_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			if( funcName != "" )
				query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = '" + funcName + "'";
			else query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'";
			query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

inline _tstring GetFunctionParamInfoQuery(EDBClass dbClass, string funcName = "")
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.func_name AS func_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
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
			break;
		case EDBClass::MYSQL:
			query = "SELECT `SPECIFIC_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `SPECIFIC_NAME` AS `func_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
			query = query + "`PARAMETER_NAME` AS `param_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.PARAMETERS";
			if( funcName != "" )
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'FUNCTION') AND `SPECIFIC_NAME` = '" + funcName + "'";
				query = query + "\n" + "ORDER BY `ORDINAL_POSITION` ASC;";
			}
			else
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'FUNCTION')";
				query = query + "\n" + "ORDER BY `SPECIFIC_NAME` ASC, `ORDINAL_POSITION` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetTableColumnOption(EDBClass dbClass, _tstring dataTypeDesc, bool isNullable, _tstring defaultDefinition, bool isIdentity, long seedValue, long incrementValue, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring comment = _T(""))
{
	_tstring columnOption = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// [컬럼명] 데이터타입 컬럼속성
			//  - [컬럼명] 데이터타입 <NULL|NOT NULL> IDENTITY(%d, %d)
			//  - [컬럼명] 데이터타입 <NULL|NOT NULL>
			//  - [컬럼명] 데이터타입 <NULL|NOT NULL> DEFAULT 값
			//  - [컬럼명] 데이터타입 COLLATE 데이터정렬 <NULL|NOT NULL> DEFAULT 값 
			columnOption = dataTypeDesc;
			if( collation != _T("") )
				columnOption = columnOption + _T(" COLLATE ") + collation;

			columnOption = dataTypeDesc + (isNullable ? _T(" NULL") : _T(" NOT NULL")) + (isIdentity ? tstring_format(_T(" IDENTITY(%ld,%ld)"), seedValue, incrementValue) : _T(""));
			break;
		case EDBClass::MYSQL:
			// `컬럼명` 데이터타입 컬럼속성
			//  - `컬럼명` 데이터타입 <NULL|NOT NULL> AUTO_INCREMENT COMMENT 코멘트
			//  - `컬럼명` 데이터타입 <NULL|NOT NULL>
			//  - `컬럼명` 데이터타입 <NULL|NOT NULL> DEFAULT 값 COMMENT 코멘트
			//  - `컬럼명` 데이터타입 CHARACTER SET '캐릭터셋' COLLATE '데이터정렬' <NULL|NOT NULL> COMMENT 코멘트
			//  - `컬럼명` 데이터타입 <NULL|NOT NULL> COMMENT 코멘트
			columnOption = dataTypeDesc;
			if( characterSet != _T("") && collation != _T("") )
				columnOption = columnOption + _T(" CHARACTER SET '") + characterSet + _T("' COLLATE '") + collation + _T("'");

			columnOption = (isNullable ? _T(" NULL") : _T(" NOT NULL")) + (defaultDefinition != _T("") ? _T(" DEFAULT ") + defaultDefinition : _T("")) + (isIdentity ? _T(" AUTO_INCREMENT") : _T(""));

			if( comment != _T("") )
				columnOption = columnOption + _T(" COMMENT '") + comment + _T("'");
			break;
	}
	return columnOption;
}

#endif // ndef __DBSQLQUERY_H__