
//***************************************************************************
// BaseSQL.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __BASESQL_H__
#define __BASESQL_H__

#pragma once

//***************************************************************************
//
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

//***************************************************************************
//
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

//***************************************************************************
//
inline _tstring GetTableListQuery(EDBClass dbClass)
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT DB_NAME() AS db_name, 1 AS object_type, name AS object_name";
			query = query + "\n" + "FROM sys.tables AS t";
			query = query + "\n" + "WHERE type = 'U'";
			query = query + "\n" + "ORDER BY name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT `TABLE_SCHEMA` AS `db_name`, 1 AS `object_type`, `TABLE_NAME` AS `object_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";
			query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
			query = query + "\n" + "ORDER BY `TABLE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetTableInfo(EDBClass dbClass, _tstring tableName=_T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT t.object_id AS object_id, SCHEMA_NAME(t.schema_id) AS schema_name, t.name AS table_name, ";
			query = query + "ISNULL((SELECT CAST(ISNULL(last_value, 0) AS BIGINT) FROM sys.identity_columns WHERE object_id = t.object_id AND last_value > 0), 0) AS auto_increment, ";
			query = query + "'' AS engine, '' AS characterset, '' AS collation, ";
			query = query + "CAST(ep.[value] AS NVARCHAR(4000)) AS table_comment, CONVERT(VARCHAR(23), create_date, 121) AS create_date, CONVERT(VARCHAR(23), modify_date, 121) AS modify_date";
			query = query + "\n" + "FROM sys.tables AS t";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON t.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE t.type = 'U' AND t.name = '" + StringToTString(tableName) + "';";
			}
			else
			{
				query = query + "\n" + "WHERE t.type = 'U'";
				query = query + "\n" + "ORDER BY t.name ASC;";
			}
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, a.`TABLE_NAME` AS `table_name`, IFNULL(a.`AUTO_INCREMENT`, 0) AS `auto_increment`, ";
			query = query + "a.`ENGINE` AS `engine`, b.`CHARACTER_SET_NAME` AS `characterset`, a.`TABLE_COLLATION` AS `collation`, ";
			query = query + "a.`TABLE_COMMENT` AS `table_comment`, a.`CREATE_TIME` AS `create_date`, a.`UPDATE_TIME` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES AS a";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY AS b";
			query = query + "\n" + "ON a.`TABLE_COLLATION` = b.`COLLATION_NAME`";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE a.`TABLE_SCHEMA` = DATABASE() AND a.`TABLE_NAME` = '" + StringToTString(tableName) + "';";
			}
			else
			{
				query = query + "\n" + "WHERE a.`TABLE_SCHEMA` = DATABASE()";
				query = query + "\n" + "ORDER BY a.`TABLE_NAME` ASC;";
			}
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetTableColumnInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT col.object_id AS object_id, OBJECT_SCHEMA_NAME(col.object_id) AS schema_name, col.table_name AS table_name, col.column_id AS seq, col.name AS column_name, UPPER(TYPE_NAME(col.user_type_id)) AS datatype, ";
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
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS col";
			query = query + "\n" + "LEFT OUTER JOIN sys.identity_columns AS ic";
			query = query + "\n" + "ON col.object_id = ic.object_id AND col.column_id = ic.column_id";
			query = query + "\n" + "LEFT OUTER JOIN sys.default_constraints AS dc";
			query = query + "\n" + "ON col.default_object_id = dc.object_id";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON col.object_id = ep.major_id AND col.column_id = ep.minor_id AND ep.class = 1 AND ep.name = 'MS_Description'";
			if( tableName != _T("") ) query = query + "\n" + "ORDER BY col.column_id ASC;";
			else query = query + "\n" + "ORDER BY col.table_name ASC, col.column_id ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, `TABLE_NAME` AS `table_name`, `ORDINAL_POSITION` AS `seq`, `COLUMN_NAME` AS `column_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(COLUMN_TYPE) AS `datatype_desc`, ";
			query = query + "(CASE `IS_NULLABLE` WHEN 'YES' THEN true ELSE false END) AS `is_nullable`, ";
			query = query + "(CASE `EXTRA` WHEN 'auto_increment' THEN true ELSE false END) AS `is_identity`, 0 AS `seed_value`, 0 AS `inc_value`, ";
			query = query + "'' AS `default_constraintname`, (CASE `COLUMN_DEFAULT` WHEN 'CURRENT_TIMESTAMP' THEN 'Now()' ELSE `COLUMN_DEFAULT` END) AS `default_definition`, ";
			query = query + "`CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`, ";
			query = query + "`COLUMN_COMMENT` AS `column_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.COLUMNS";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + StringToTString(tableName) + "'";
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

//***************************************************************************
//
inline _tstring GetIndexInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.index_id, i.type AS index_kind, 0 AS index_type, i.is_primary_key, ";
			query = query + "i.is_unique, ic.index_column_id AS column_seq, COL_NAME(ic.object_id, ic.column_id) AS column_name, ";
			query = query + "(CASE is_descending_key WHEN 1 THEN 2 ELSE 1 END) AS column_sort";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.indexes AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.object_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS i";
			query = query + "\n" + "INNER JOIN sys.index_columns AS ic";
			query = query + "\n" + "ON i.object_id = ic.object_id AND i.index_id = ic.index_id";
			query = query + "\n" + "WHERE i.type > 0";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY i.index_id ASC, ic.index_column_id ASC;";
			else query = query + "\n" + "ORDER BY i.table_name ASC, i.index_id ASC, ic.index_column_id ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, stat.`TABLE_NAME` AS `table_name`, stat.`INDEX_NAME` AS `index_name`, '0' AS `index_id`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 ELSE 2 END) AS `index_kind`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 WHEN 'UNIQUE' THEN 2 WHEN 'INDEX' THEN 3 WHEN 'FULLTEXT' THEN 4 WHEN 'SPATIAL' THEN 5 ELSE 0 END) AS `index_type`, ";
			query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN true ELSE false END) AS `is_primary_key`, ";
			query = query + "(CASE stat.`NON_UNIQUE` WHEN 0 THEN true ELSE false END) AS `is_unique`, stat.`SEQ_IN_INDEX` AS `column_seq`, ";
			query = query + "stat.`COLUMN_NAME` AS `column_name`, (CASE stat.`COLLATION` WHEN 'D' THEN 2 ELSE 1 END) AS `column_sort`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.STATISTICS AS stat";
			query = query + "\n" + "LEFT OUTER JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "ON stat.`TABLE_SCHEMA` = const.`TABLE_SCHEMA` AND stat.`TABLE_NAME` = const.`TABLE_NAME` AND stat.`INDEX_NAME` = const.`CONSTRAINT_NAME`";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE stat.`TABLE_SCHEMA` = DATABASE() AND stat.`TABLE_NAME` = '" + StringToTString(tableName) + "' AND (const.`CONSTRAINT_TYPE` IN('PRIMARY KEY', 'UNIQUE') OR const.CONSTRAINT_TYPE IS NULL)";
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

//***************************************************************************
//
inline _tstring GetIndexOptionInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT i.object_id AS object_id, OBJECT_SCHEMA_NAME(i.object_id) AS schema_name, i.table_name AS table_name, i.name AS index_name, i.type_desc, i.is_primary_key, i.is_unique, ";
			query = query + "i.ignore_dup_key, i.fill_factor, i.is_padded, i.is_disabled, i.allow_row_locks, i.allow_page_locks, i.has_filter, i.filter_definition, ";
			query = query + "s.no_recompute AS statistics_norecompute, ";
			query = query + "p.data_compression, p.data_compression_desc, ";
			query = query + "ds.type_desc AS filegroup_or_partition_scheme, ds.name AS filegroup_or_partition_scheme_name";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.indexes AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.object_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS i";
			query = query + "\n" + "INNER JOIN sys.stats AS s";
			query = query + "\n" + "ON i.object_id = s.object_id AND i.index_id = s.stats_id";
			query = query + "\n" + "INNER JOIN sys.data_spaces AS ds";
			query = query + "\n" + "ON i.data_space_id = ds.data_space_id";
			query = query + "\n" + "INNER JOIN";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT object_id, index_id, data_compression, data_compression_desc, ROW_NUMBER() OVER(PARTITION BY object_id, index_id ORDER BY COUNT(*) DESC) AS main_compression";
			query = query + "\n\t" + "FROM sys.partitions";
			query = query + "\n\t" + "GROUP BY object_id, index_id, data_compression, data_compression_desc";
			query = query + "\n" + ") AS p";
			query = query + "\n" + "ON i.object_id = p.object_id AND i.index_id = p.index_id AND p.main_compression = 1";
			query = query + "\n" + "WHERE i.is_hypothetical = 0 AND i.index_id <> 0";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY i.index_id ASC;";
			else query = query + "\n" + "ORDER BY i.table_name ASC, i.index_id ASC;";
			break;
		case EDBClass::MYSQL:
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetForeignKeyInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT fk.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(fk.parent_object_id) AS schema_name, fk.table_name AS table_name, fk.name AS foreignkey_name, fk.is_disabled, fk.is_not_trusted, ";
			query = query + "OBJECT_NAME(fkcol.parent_object_id) AS foreignkey_table_name, COL_NAME(fkcol.parent_object_id, fkcol.parent_column_id) AS foreignkey_column_name, ";
			query = query + "OBJECT_SCHEMA_NAME(fkcol.referenced_object_id) AS referencekey_schema_name, OBJECT_NAME(fkcol.referenced_object_id) AS referencekey_table_name, COL_NAME(fkcol.referenced_object_id, fkcol.referenced_column_id) AS referencekey_column_name, ";
			query = query + "REPLACE(fk.update_referential_action_desc, '_', ' ') AS update_rule, REPLACE(fk.delete_referential_action_desc, '_', ' ') AS delete_rule";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.foreign_keys AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.parent_object_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS fk";
			query = query + "\n" + "INNER JOIN sys.foreign_key_columns AS fkcol";
			query = query + "\n" + "ON fk.object_id = fkcol.constraint_object_id";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY fk.name ASC, fkcol.constraint_column_id ASC, fkcol.referenced_column_id ASC;";
			else query = query + "\n" + "ORDER BY fk.table_name ASC, fk.name ASC, fkcol.constraint_column_id ASC, fkcol.referenced_column_id ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, colusage.`CONSTRAINT_NAME` AS `foreignkey_name`, 0 AS `is_disabled`, 0 AS `is_not_trusted`, ";
			query = query + "colusage.`TABLE_NAME` AS `foreignkey_table_name`, colusage.`COLUMN_NAME` AS `foreignkey_column_name`, ";
			query = query + "'' AS referencekey_schema_name, colusage.`REFERENCED_TABLE_NAME` AS `referencekey_table_name`, colusage.`REFERENCED_COLUMN_NAME` AS `referencekey_column_name`, ";
			query = query + "refconst.`UPDATE_RULE` AS `update_rule`, refconst.`DELETE_RULE` AS `delete_rule`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS colusage";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = colusage.TABLE_SCHEMA AND const.`TABLE_NAME` = colusage.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = colusage.`CONSTRAINT_NAME`";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS refconst";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = refconst.`CONSTRAINT_SCHEMA` AND const.`TABLE_NAME` = refconst.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = refconst.`CONSTRAINT_NAME`";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + StringToTString(tableName) + "' AND const.`CONSTRAINT_TYPE` IN('FOREIGN KEY')";
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

//***************************************************************************
//
inline _tstring GetDefaultConstInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, const.name AS default_const_name, ";
			query = query + "COL_NAME(const.parent_object_id, const.parent_column_id) AS column_name, const.definition AS default_value";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.default_constraints AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.parent_object_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS const";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY const.name ASC;";
			else query = query + "\n" + "ORDER BY const.table_name ASC, const.name ASC;";
			break;
		case EDBClass::MYSQL:
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetCheckConstInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT const.parent_object_id AS object_id, OBJECT_SCHEMA_NAME(const.parent_object_id) AS schema_name, const.table_name AS table_name, const.name AS check_const_name, const.definition AS check_value";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.check_constraints AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.parent_object_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS const";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY const.name ASC;";
			else query = query + "\n" + "ORDER BY const.table_name ASC, const.name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, const.`CONSTRAINT_NAME` AS `check_const_name`, ckconst.`CHECK_CLAUSE` AS `check_value`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
			query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS ckconst";
			query = query + "\n" + "ON const.`TABLE_SCHEMA` = ckconst.`CONSTRAINT_SCHEMA` AND const.`CONSTRAINT_NAME` = ckconst.`CONSTRAINT_NAME`";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + StringToTString(tableName) + "' AND const.`CONSTRAINT_TYPE` IN('CHECK')";
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

//***************************************************************************
//
inline _tstring GetTriggerInfo(EDBClass dbClass, _tstring tableName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT tr.parent_id AS object_id, OBJECT_SCHEMA_NAME(tr.parent_id) AS schema_name, tr.table_name AS table_name, tr.name AS trigger_name";
			query = query + "\n" + "FROM";
			query = query + "\n" + "(";
			query = query + "\n\t" + "SELECT a.*, b.name AS table_name";
			query = query + "\n\t" + "FROM sys.triggers AS a";
			query = query + "\n\t" + "INNER JOIN sys.tables AS b";
			query = query + "\n\t" + "ON a.parent_id = b.object_id";
			if( tableName != _T("") )
				query = query + "\n\t" + "WHERE b.type = 'U' AND b.name = '" + StringToTString(tableName) + "'";
			else query = query + "\n\t" + "WHERE b.type = 'U'";
			query = query + "\n" + ") AS tr";
			if( tableName != _T("") )
				query = query + "\n" + "ORDER BY tr.name ASC;";
			else query = query + "\n" + "ORDER BY tr.table_name ASC, tr.name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, `EVENT_OBJECT_TABLE` AS `table_name`, `TRIGGER_NAME` AS `trigger_name`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.TRIGGERS";
			if( tableName != _T("") )
			{
				query = query + "\n" + "WHERE `TRIGGER_SCHEMA` = DATABASE() AND `EVENT_OBJECT_TABLE` = '" + StringToTString(tableName) + "'";
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

//***************************************************************************
//
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

//***************************************************************************
//
inline _tstring GetStoredProcedureInfo(EDBClass dbClass, _tstring procName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = query + "SELECT stproc.object_id AS object_id, SCHEMA_NAME(stproc.schema_id) AS schema_name, stproc.name AS proc_name, CAST(ep.[value] AS NVARCHAR(4000)) AS proc_comment, ";
			query = query + "CONVERT(VARCHAR(23), stproc.create_date, 121) AS create_date, CONVERT(VARCHAR(23), stproc.modify_date, 121) AS modify_date";
			query = query + "\n" + "FROM sys.procedures AS stproc";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON stproc.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";
			if( procName != _T("") )
				query = query + "\n" + "WHERE stproc.name = '" + StringToTString(procName) + "';";
			else query = query + "\n" + "ORDER BY stproc.name ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `proc_name`, `ROUTINE_COMMENT` AS `proc_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			if( procName != _T("") )
			{
				query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = '" + StringToTString(procName) + "';";
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

//***************************************************************************
//
inline _tstring GetStoredProcedureParamInfo(EDBClass dbClass, _tstring procName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.proc_name AS proc_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
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
			if( procName != _T("") )
				query = query + "\n\t" + "WHERE b.name = '" + StringToTString(procName) + "'";
			query = query + "\n" + ") AS param";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'";
			if( procName != _T("") )
				query = query + "\n" + "ORDER BY param.parameter_id ASC;";
			else query = query + "\n" + "ORDER BY param.proc_name ASC, param.parameter_id ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, `SPECIFIC_NAME` AS `proc_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
			query = query + "`PARAMETER_NAME` AS `param_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.PARAMETERS";
			if( procName != _T("") )
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'PROCEDURE') AND `SPECIFIC_NAME` = '" + StringToTString(procName) + "'";
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

//***************************************************************************
//
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

//***************************************************************************
//
inline _tstring GetFunctionInfo(EDBClass dbClass, _tstring funcName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = query + "SELECT so.object_id AS object_id, SCHEMA_NAME(so.schema_id) AS schema_name, so.name AS func_name, CAST(ep.[value] AS NVARCHAR(4000)) AS func_comment, ";
			query = query + "CONVERT(VARCHAR(23), so.create_date, 121) AS create_date, CONVERT(VARCHAR(23), so.modify_date, 121) AS modify_date";
			query = query + "\n" + "FROM sys.objects AS so";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON so.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'";
			if( funcName != _T("") )
			{
				query = query + "\n" + "WHERE so.type IN ('FN', 'IF', 'TF') AND so.name = '" + StringToTString(funcName) + "';";
			}
			else
			{
				query = query + "\n" + "WHERE so.type IN ('FN', 'IF', 'TF')";
				query = query + "\n" + "ORDER BY so.name ASC;";
			}
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `func_name`, `ROUTINE_COMMENT` AS `func_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
			if( funcName != _T("") )
				query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = '" + StringToTString(funcName) + "'";
			else query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'";
			query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";
			break;
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetFunctionParamInfo(EDBClass dbClass, _tstring funcName = _T(""))
{
	string query = "";

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = "SELECT param.object_id AS object_id, OBJECT_SCHEMA_NAME(param.object_id) AS schema_name, param.func_name AS func_name, param.parameter_id, (CASE param.is_output WHEN 1 THEN (CASE WHEN (param.name IS NULL OR param.name = '') THEN 0 ELSE 2 END) ELSE 1 END) AS param_mode, ";
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
			if( funcName != _T("") )
				query = query + "\n\t" + "WHERE b.type IN ('FN', 'IF', 'TF') AND b.name = '" + StringToTString(funcName) + "'";
			else query = query + "\n\t" + "WHERE b.type IN ('FN', 'IF', 'TF')";
			query = query + "\n" + ") AS param";
			query = query + "\n" + "LEFT OUTER JOIN sys.extended_properties AS ep";
			query = query + "\n" + "ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'";
			if( funcName != _T("") )
				query = query + "\n" + "ORDER BY param.parameter_id ASC;";
			else query = query + "\n" + "ORDER BY param.func_name ASC, param.parameter_id ASC;";
			break;
		case EDBClass::MYSQL:
			query = "SELECT 0 AS `object_id`, `SPECIFIC_NAME` AS `func_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
			query = query + "`PARAMETER_NAME` AS `param_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
			query = query + "IFNULL((CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END), 0) AS `max_length`, ";
			query = query + "IFNULL((CASE `DATA_TYPE` WHEN 'datetime' OR 'time' OR 'timestamp' THEN DATETIME_PRECISION ELSE `NUMERIC_PRECISION` END), 0) AS `precision`, ";
			query = query + "IFNULL(`NUMERIC_SCALE`, 0) AS `scale`, ";
			query = query + "UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`";
			query = query + "\n" + "FROM INFORMATION_SCHEMA.PARAMETERS";
			if( funcName != _T("") )
			{
				query = query + "\n" + "WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'FUNCTION') AND `SPECIFIC_NAME` = '" + StringToTString(funcName) + "'";
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
	_tstring identity = _T("");
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

			columnOption = dataTypeDesc + (isNullable ? _T(" NULL") : _T(" NOT NULL")) + (isIdentity ? string_format(_T(" IDENTITY(%ld,%ld)"), seedValue, incrementValue) : _T(""));
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

//***************************************************************************
//
inline _tstring GetMSSQLDatabaseBackupQuery(_tstring databaseName, _tstring backupFilePath)
{
	_tstring query = _T("BACKUP DATABASE ") + databaseName + _T(" TO DISK = '") + backupFilePath + _T("'");
	return query;
}

//***************************************************************************
//
inline _tstring GetMSSQLDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath)
{
	// query = "RESTORE FILELISTONLY FROM DISK = '" + restoreFilePath + "'";
	// query = "RESTORE DATABASE " + databaseName + " FROM DISK = '" + restoreFilePath + "' WITH REPLACE";
	_tstring query = _T("RESTORE FILELISTONLY FROM DISK = '") + restoreFilePath + _T("'");
	return query;
}

//***************************************************************************
//
inline _tstring GetMSSQLDatabaseRestoreQuery(_tstring databaseName, _tstring restoreFilePath, _tstring dataFilePath, _tstring logFilePath)
{
	_tstring query = _T("RESTORE DATABASE ") + databaseName + _T(" FROM DISK = '") + restoreFilePath + _T("'");
	query = query + _T(" WITH MOVE '") + databaseName + _T(" TO '") + dataFilePath + _T("'");
	query = query + _T(", MOVE '") + databaseName + _T("_log' TO '") + logFilePath + _T("'");
	return query;
}

//***************************************************************************
//
inline _tstring GetMSSQLIndexFragmentationCheck(_tstring tableName = _T(""))
{
	string query = "SELECT ips.object_id AS object_id, OBJECT_SCHEMA_NAME(ips.object_id) AS schema_name, OBJECT_NAME(ips.object_id) AS table_name, ips.index_id AS index_id, i.name AS index_name, ips.partition_number AS partitionnum, ips.avg_fragmentation_in_percent";
	query = query + "\n" + "FROM sys.dm_db_index_physical_stats(DB_ID(), NULL, NULL, NULL, NULL) AS ips";
	query = query + "\n" + "INNER JOIN sys.indexes AS i";
	query = query + "\n" + "ON ips.object_id = i.object_id AND ips.index_id = i.index_id";
	if( tableName != _T("") )
		query = query + "\n" + "WHERE OBJECT_NAME(ips.object_id) = '" + StringToTString(tableName) + "'";
	else query = query + "\n" + "ORDER BY ips.avg_fragmentation_in_percent DESC";
	return TStringToString(query);
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
			query = string_format(_T("EXEC sp_rename '%s', '%s'"), objectName.c_str(), chgObjectName.c_str());
			break;
		default:
			query = string_format(_T("EXEC sp_rename '%s', '%s', '%s'"), objectName.c_str(), chgObjectName.c_str(), ToString(renameObjectType));
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetMYSQLDatabaseBackupQuery(_tstring loginPath, _tstring dbName, _tstring backupFilePath, _tstring defaultCharacterSet = _T(""), bool isNoData = false)
{
	// mysqldump -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] > [백업 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	// --set-gtid-purged=OFF : GTID(Global Transaction Identifier) 활성화 여부 설정(GTID를 사용하지 않는 MySQL DB를 복구하려면 백업 수행 시 --set-gtid-purged=OFF 옵션을 추가).
	// --single-transaction : lock 을 걸지 않고도 dump 파일의 정합성 보장. InnoDB 일때만 사용 가능.
	// --no-tablespaces : 해당 옵션을 줄 경우 CREATE LOGFILE GROUP과 CREATE TABLESPACE문을 생성하지 않음
	// --default-character-set=utf8mb4 : 기본 문자 집합을 utf8mb4로 지정
	// --no-data : 데이터를 백업하지 않고, DDL만 백업
	_tstring query = string_format(_T("mysqldump --login-path=%s %s --routines --events --single-transaction --set-gtid-purged=OFF --no-tablespaces%s%s > %s"),
		loginPath,
		dbName,
		defaultCharacterSet != _T("") ? _T(" --default-character-set=") + defaultCharacterSet : _T(""),
		isNoData ? _T(" --no-data") : _T(""),
		backupFilePath);
	return query;
}

//***************************************************************************
//
inline _tstring GetMYSQLDatabaseRestoreQuery(_tstring loginPath, _tstring dbName, _tstring restoreFilePath)
{
	// mysql -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] < [복원할 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	_tstring query = string_format(_T("mysql --login-path=%s %s < %s"), loginPath, dbName, restoreFilePath);
	return query;
}

//***************************************************************************
//
inline _tstring GetMYSQLCharacterSetCollations(_tstring charset)
{
	string query = "SELECT `CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY";
	if( charset != _T("") )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + StringToTString(charset) + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC, `COLLATION_NAME` ASC";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetMYSQLEngines()
{
	string query = "SELECT `ENGINE` AS `engine`, `SUPPORT` AS `support`, `COMMENT` AS `comment`, `TRANSACTIONS` AS `transactions`, `XA` AS `xa`, `SAVEPOINTS` AS `savepoints`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ENGINES";
	query = query + "\n" + "ORDER BY `ENGINE` ASC";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetMYSQLTableFragmentationCheck(_tstring tableName = _T(""))
{
	// 테이블 단편화(Fragmentation) 확인
	// [발생 원인]
	//  - fragmentation이란 INSERT & DELETE가 수차례 반복되면서 Page 안에 회수가 안되는(사용되지 않는) 부분이 많아지면서 발생하게 되는데
	//    그 영향으로 테이블이 실제로 가져야 하는 OS 공간 보다 더 많은 공간을 차지하게 됨.
	// [확인 방법]
	//  - OS 서버에서 해당 테이블에 ibd 파일과 아래 쿼리에 total의 사이즈를 비교하여 간극 만큼을 단편화(Fragmentation)로 판단할 수 있고,
	//    이 경우에 테이블 최적화(OPTIMIZE TABLE)를 수행해서 성능 향상
	string query = "SELECT `TABLE_NAME` AS `table_name`, ";
	query = query + "ROUND((DATA_LENGTH + INDEX_LENGTH) / (1024 * 1024), 2) AS `totalsize`, ";
	query = query + "ROUND((DATA_FREE) / (1024 * 1024 ), 2) AS `datafreesize`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";
	if( tableName != _T("") )
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + StringToTString(tableName) + "'";
	}
	else
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
		query = query + "\n" + "ORDER BY `total` DESC;";
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring GetMYSQLShowObject(EDBObjectType dbObject, _tstring objectName)
{
	_tstring query = _T("");

	switch( dbObject )
	{
		case EDBObjectType::TABLE:
			query = _T("SHOW CREATE TABLE ") + objectName + _T(";");
			break;
		case EDBObjectType::PROCEDURE:
			query = _T("SHOW CREATE PROCEDURE ") + objectName + _T(";");
			break;
		case EDBObjectType::FUNCTION:
			query = _T("SHOW CREATE FUNCTION ") + objectName + _T(";");
			break;
		case EDBObjectType::TRIGGERS:
			query = _T("SHOW CREATE TRIGGER ") + objectName + _T(";");
			break;
		case EDBObjectType::EVENTS:
			query = _T("SHOW CREATE EVENT ") + objectName + _T(";");
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetMYSQLRenameObject(_tstring tableName, _tstring chgName, _tstring columnName = _T(""), _tstring dataTypeDesc = _T(""), bool isNullable = false, _tstring defaultDefinition = _T(""), bool isIdentity = false, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring comment = _T(""))
{
	_tstring query = _T("");

	if( columnName != _T("") )
	{
		// ALTER TABLE `테이블명` RENAME `변경할테이블명`;
		query = string_format(_T("ALTER TABLE `{%s}` RENAME `{%s}`;"), tableName.c_str(), chgName.c_str());
	}
	else
	{
		// ALTER TABLE `테이블명` CHANGE COLUMN `컬럼명` `변경할컬럼명` 데이터타입 컬럼속성;
		_tstring columnOption = GetTableColumnOption(EDBClass::MYSQL, dataTypeDesc, isNullable, defaultDefinition, isIdentity, 0, 0, characterSet, collation, comment);
		query = string_format(_T("ALTER TABLE `{%s}` CHANGE COLUMN `{%s}` `{%s}` {%s};"), tableName.c_str(), columnName.c_str(), chgName.c_str(), columnOption.c_str());
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetMYSQLAlterTable(_tstring tableName, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring engine = _T(""))
{
	_tstring query = _T("");

	// 테이블 캐릭터셋, 데이터정렬(문자비교규칙), 스토리지엔진 변경
	// ALTER TABLE `테이블명` CHARACTER SET = 캐릭터셋, COLLATE = 데이터정렬, ENGINE = 스토리지엔진;
	query = _T("ALTER TABLE `") + tableName + _T("`");

	if( characterSet != _T("") )
		query = query + _T(" CHARACTER SET = ") + characterSet + _T(",");

	if( collation != _T("") )
		query = query + _T(" COLLATE = ") + collation + _T(",");

	if( engine != _T("") )
		query = query + _T(" ENGINE = ") + engine;

	query = query + _T(";");
	return query;
}

//***************************************************************************
//
inline _tstring  GetMYSQLAlterTableCollation(_tstring tableName, _tstring characterSet, _tstring collation)
{
	// 테이블 캐릭터셋, 데이터정렬(문자비교규칙)을 변경
	// ALTER TABLE `테이블명` CONVERT TO CHARACTER SET 캐릭터셋 COLLATE 데이터정렬;
	_tstring query = _T("ALTER TABLE `") + tableName + _T("` CONVERT TO CHARACTER SET ") + characterSet + _T(" COLLATE ") + collation + _T(";");
	return query;
}

//***************************************************************************
// Ex)
//	SELECT `TABLE_COMMENT` AS `tableComment` FROM INFORMATION_SCHEMA.TABLES WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'tbl_table1';
//	SELECT `COLUMN_COMMENT` AS `columncomment` FROM INFORMATION_SCHEMA.COLUMNS WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'tbl_table1' AND `COLUMN_NAME` = 'Id';
inline _tstring MYSQLGetTableColumnCommentQuery(_tstring tableName, _tstring columnName)
{
	_tstring query = _T("");

	if( columnName.size() > 0 )
		query = _T("SELECT `COLUMN_COMMENT` AS `columncomment` FROM INFORMATION_SCHEMA.COLUMNS WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '") + tableName + _T("' AND `COLUMN_NAME` = '") + columnName + _T("';");
	else query = _T("SELECT `COLUMN_COMMENT` AS `columncomment` FROM INFORMATION_SCHEMA.COLUMNS WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '") + tableName + _T("';");

	return query;
}

//***************************************************************************
// Ex)
//	ALTER TABLE `tbl_table1` COMMENT '테스트 테이블';
//  ALTER TABLE `tbl_table1` MODIFY `Id` VARCHAR(50) NOT NULL COMMENT '아이디';
inline _tstring MYSQLProcessTableColumnCommentQuery(_tstring tableName, _tstring setComment, _tstring columnName = _T(""), _tstring dataTypeDesc = _T(""), bool isNullable = false, _tstring defaultDefinition = _T(""), bool isIdentity = false, _tstring characterSet = _T(""), _tstring collation = _T(""))
{
	_tstring query = _T("");

	if( columnName.size() > 0 )
	{
		_tstring columnOption = GetTableColumnOption(EDBClass::MYSQL, dataTypeDesc, isNullable, defaultDefinition, isIdentity, 0, 0, characterSet, collation);
		query = _T("ALTER TABLE `") + tableName + _T("` MODIFY `") + columnName + _T("` ") + columnOption + _T(" COMMENT '") + setComment + _T("';");
	}
	else query = _T("ALTER TABLE `") + tableName + _T("` COMMENT '") + setComment + _T("';");

	return query;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `procComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = 'sp_procedure1';
inline _tstring MYSQLGetProcedureCommentQuery(_tstring procName)
{
	_tstring query = _T("SELECT `ROUTINE_COMMENT` AS `procComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = '") + procName + _T("';");
	return query;
}

//***************************************************************************
// Ex)
//	ALTER PROCEDURE `sp_procedure1` COMMENT '테스트 저장프로시저';
inline _tstring MYSQLProcessProcedureCommentQuery(_tstring procName, _tstring comment)
{
	_tstring query = _T("ALTER PROCEDURE `") + procName + _T("` COMMENT '") + comment + _T("';");
	return query;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `funcComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = 'sp_function1';
inline _tstring MYSQLGetFunctionCommentQuery(_tstring funcName)
{
	_tstring query = _T("SELECT `ROUTINE_COMMENT` AS `funcComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = '") + funcName + _T("';");
	return query;
}

//***************************************************************************
// Ex)
//	ALTER FUNCTION `sp_function1` COMMENT '테스트 함수';
inline _tstring MYSQLProcessFunctionCommentQuery(_tstring funcName, _tstring comment)
{
	_tstring query = _T("ALTER FUNCTION `") + funcName + _T("` COMMENT '") + comment + _T("';");
	return query;
}
#endif // ndef __BASESQL_H__