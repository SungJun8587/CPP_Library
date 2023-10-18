
//***************************************************************************
// BaseSQL.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __BASESQL_H__
#define __BASESQL_H__

#pragma once

//***************************************************************************
//
inline _tstring GetTableInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT t.object_id, t.name AS table_name, CAST(ep.[value] AS NVARCHAR(4000)) AS table_comment, ");
			sql = sql + _T("(SELECT CAST(ISNULL(last_value, 0) AS BIGINT) FROM sys.identity_columns WHERE object_id = t.object_id AND last_value > 0) AS auto_increment");
			sql = sql + _T("\n") + _T("FROM sys.tables AS t");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON t.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("WHERE t.type = 'U'");
			sql = sql + _T("\n") + _T("ORDER BY t.name ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `TABLE_NAME` AS `tablename`, `TABLE_COMMENT` AS `table_comment`, ");
			sql = sql + _T("`AUTO_INCREMENT` AS auto_increment");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.TABLES");
			sql = sql + _T("\n") + _T("WHERE `TABLE_SCHEMA` = DATABASE()");
			sql = sql + _T("\n") + _T("ORDER BY `TABLE_NAME` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetTableColumnInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT col.object_id, col.table_name AS table_name, col.column_id AS seq, col.name AS column_name, UPPER(TYPE_NAME(col.user_type_id)) AS datatype, col.max_length, ");
			sql = sql + _T("(UPPER(TYPE_NAME(col.user_type_id)) + (CASE WHEN TYPE_NAME(col.user_type_id) = 'varchar' OR TYPE_NAME(col.user_type_id) = 'char' THEN '(' ");
			sql = sql + _T("+ (CASE WHEN col.max_length = -1 THEN 'MAX' ELSE CAST(col.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(col.user_type_id) = 'nvarchar' ");
			sql = sql + _T("OR TYPE_NAME(col.user_type_id) = 'nchar' THEN '(' + (CASE WHEN col.max_length = -1 THEN 'MAX' ELSE CAST(col.max_length / 2 AS VARCHAR) END) + ')' ");
			sql = sql + _T("WHEN TYPE_NAME(col.user_type_id) = 'decimal' THEN '(' + CAST(col.precision AS VARCHAR) + ',' + CAST(col.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ");
			sql = sql + _T("col.is_nullable, col.is_identity, CAST(ic.seed_value AS BIGINT) AS seed_value, CAST(ic.increment_value AS BIGINT) AS inc_value, ");
			sql = sql + _T("comm.text AS default_definition, CAST(ep.[value] AS NVARCHAR(4000)) AS column_comment");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS table_name");
			sql = sql + _T("\n\t") + _T("FROM sys.columns AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.tables AS b");
			sql = sql + _T("\n\t") + _T("ON a.object_id = b.object_id");
			sql = sql + _T("\n\t") + _T("WHERE b.type = 'U'");
			sql = sql + _T("\n") + _T(") AS col");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.identity_columns AS ic");
			sql = sql + _T("\n") + _T("ON col.object_id = ic.object_id AND col.column_id = ic.column_id");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.syscomments AS comm");
			sql = sql + _T("\n") + _T("ON col.default_object_id = comm.id AND comm.colid = 1");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON col.object_id = ep.major_id AND col.column_id = ep.minor_id AND ep.class = 1 AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("ORDER BY col.object_id ASC, col.column_id ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, col.`TABLE_NAME` AS `table_name`, col.`ORDINAL_POSITION` AS `seq`, col.`COLUMN_NAME` AS `column_name`, UPPER(col.`DATA_TYPE`) AS `datatype`, ");
			sql = sql + _T("(CASE WHEN col.`CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN col.`CHARACTER_MAXIMUM_LENGTH` ELSE col.`NUMERIC_PRECISION` END) AS `max_length`, ");
			sql = sql + _T("UPPER(col.COLUMN_TYPE) AS `datatype_desc`, ");
			sql = sql + _T("(CASE col.`IS_NULLABLE` WHEN 'YES' THEN true ELSE false END) AS `is_nullable`, ");
			sql = sql + _T("(CASE col.`EXTRA` WHEN 'auto_increment' THEN true ELSE false END) AS `is_identity`, 0 AS `seed_value`, 0 AS `inc_value`, ");
			sql = sql + _T("(CASE col.`COLUMN_DEFAULT` WHEN 'CURRENT_TIMESTAMP' THEN 'Now()' ELSE col.`COLUMN_DEFAULT` END) AS `default_definition`, col.`COLUMN_COMMENT` AS `column_comment`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.COLUMNS AS col");
			sql = sql + _T("\n") + _T("WHERE col.`TABLE_SCHEMA` = DATABASE()");
			sql = sql + _T("\n") + _T("ORDER BY col.`TABLE_NAME` ASC, col.`ORDINAL_POSITION` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetIndexInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT i.object_id, i.table_name AS table_name, i.name AS index_name, i.index_id, i.type AS index_kind, 0 AS index_type, i.is_primary_key, ");
			sql = sql + _T("i.is_unique, ic.index_column_id AS index_seq, COL_NAME(ic.object_id, ic.column_id) AS column_name, ");
			sql = sql + _T("(CASE is_descending_key WHEN 1 THEN 2 ELSE 1 END) AS index_sort");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS table_name");
			sql = sql + _T("\n\t") + _T("FROM sys.indexes AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.tables AS b");
			sql = sql + _T("\n\t") + _T("ON a.object_id = b.object_id");
			sql = sql + _T("\n\t") + _T("WHERE b.type = 'U'");
			sql = sql + _T("\n") + _T(") AS i");
			sql = sql + _T("\n") + _T("INNER JOIN sys.index_columns AS ic");
			sql = sql + _T("\n") + _T("ON i.object_id = ic.object_id AND i.index_id = ic.index_id");
			sql = sql + _T("\n") + _T("WHERE i.type > 0 AND i.object_id IN(SELECT object_id FROM sys.tables WHERE type = 'U')");
			sql = sql + _T("\n") + _T("ORDER BY i.object_id ASC, i.index_id ASC, ic.index_column_id ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, stat.`TABLE_NAME` AS `table_name`, stat.`INDEX_NAME` AS `index_name`, '0' AS `index_id`, ");
			sql = sql + _T("(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 ELSE 2 END) AS `index_kind`, ");
			sql = sql + _T("(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 WHEN 'UNIQUE' THEN 2 WHEN 'INDEX' THEN 3 WHEN 'FULLTEXT' THEN 4 WHEN 'SPATIAL' THEN 5 ELSE 0 END) AS `index_type`, ");
			sql = sql + _T("(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN true ELSE false END) AS `is_primary_key`, ");
			sql = sql + _T("(CASE stat.`NON_UNIQUE` WHEN 0 THEN true ELSE false END) AS `is_unique`, stat.`SEQ_IN_INDEX` AS `index_seq`, ");
			sql = sql + _T("stat.`COLUMN_NAME` AS `column_name`, (CASE stat.`COLLATION` WHEN 'D' THEN 2 ELSE 1 END) AS `index_sort`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.STATISTICS AS stat");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const");
			sql = sql + _T("\n") + _T("ON stat.`TABLE_SCHEMA` = const.`TABLE_SCHEMA` AND stat.`TABLE_NAME` = const.`TABLE_NAME` AND stat.`INDEX_NAME` = const.`CONSTRAINT_NAME`");
			sql = sql + _T("\n") + _T("WHERE stat.`TABLE_SCHEMA` = DATABASE() AND (const.`CONSTRAINT_TYPE` IN('PRIMARY KEY', 'UNIQUE') OR const.CONSTRAINT_TYPE IS NULL)");
			sql = sql + _T("\n") + _T("ORDER BY stat.`TABLE_NAME` ASC, stat.`INDEX_NAME` DESC, stat.`SEQ_IN_INDEX` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetForeignKeyInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT fk.parent_object_id AS object_id, fk.table_name AS table_name, fk.name AS foreignkey_name, ");
			sql = sql + _T("OBJECT_NAME(fkcol.parent_object_id) AS foreignkey_table_name, COL_NAME(fkcol.parent_object_id, fkcol.parent_column_id) AS foreignkey_column_name, ");
			sql = sql + _T("OBJECT_NAME(fkcol.referenced_object_id) AS referencekey_table_name, COL_NAME(fkcol.referenced_object_id, fkcol.referenced_column_id) AS referencekey_column_name, ");
			sql = sql + _T("REPLACE(fk.update_referential_action_desc, '_', ' ') AS update_rule, REPLACE(fk.delete_referential_action_desc, '_', ' ') AS delete_rule");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS table_name");
			sql = sql + _T("\n\t") + _T("FROM sys.foreign_keys AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.tables AS b");
			sql = sql + _T("\n\t") + _T("ON a.parent_object_id = b.object_id");
			sql = sql + _T("\n\t") + _T("WHERE b.type = 'U'");
			sql = sql + _T("\n") + _T(") AS fk");
			sql = sql + _T("\n") + _T("INNER JOIN sys.foreign_key_columns AS fkcol");
			sql = sql + _T("\n") + _T("ON fk.object_id = fkcol.constraint_object_id");
			sql = sql + _T("\n") + _T("WHERE fk.parent_object_id IN(SELECT object_id FROM sys.tables WHERE type = 'U')");
			sql = sql + _T("\n") + _T("ORDER BY fk.parent_object_id ASC, fk.name ASC, fkcol.constraint_column_id ASC, fkcol.referenced_column_id ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, const.`TABLE_NAME` AS `table_name`, colusage.`CONSTRAINT_NAME` AS `foreignkey_name`, ");
			sql = sql + _T("colusage.`TABLE_NAME` AS `foreignkey_table_name`, colusage.`COLUMN_NAME` AS `foreignkey_column_name`, ");
			sql = sql + _T("colusage.`REFERENCED_TABLE_NAME` AS `referencekey_table_name`, colusage.`REFERENCED_COLUMN_NAME` AS `referencekey_column_name`, ");
			sql = sql + _T("refconst.`UPDATE_RULE` AS `update_rule`, refconst.`DELETE_RULE` AS `delete_rule`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const");
			sql = sql + _T("\n") + _T("INNER JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS colusage");
			sql = sql + _T("\n") + _T("ON const.`TABLE_SCHEMA` = colusage.TABLE_SCHEMA AND const.`TABLE_NAME` = colusage.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = colusage.`CONSTRAINT_NAME`");
			sql = sql + _T("\n") + _T("INNER JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS refconst");
			sql = sql + _T("\n") + _T("ON const.`TABLE_SCHEMA` = refconst.`CONSTRAINT_SCHEMA` AND const.`TABLE_NAME` = refconst.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = refconst.`CONSTRAINT_NAME`");
			sql = sql + _T("\n") + _T("WHERE const.`CONSTRAINT_TYPE` IN('FOREIGN KEY') AND const.`TABLE_SCHEMA` = DATABASE()");
			sql = sql + _T("\n") + _T("ORDER BY const.`TABLE_NAME` ASC, const.`CONSTRAINT_NAME` ASC, colusage.`ORDINAL_POSITION` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetTriggerInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT tr.parent_id AS object_id, tr.table_name AS table_name, tr.name AS trigger_name");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS table_name");
			sql = sql + _T("\n\t") + _T("FROM sys.triggers AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.tables AS b");
			sql = sql + _T("\n\t") + _T("ON a.parent_id = b.object_id");
			sql = sql + _T("\n\t") + _T("WHERE b.type = 'U'");
			sql = sql + _T("\n") + _T(") AS tr");
			sql = sql + _T("\n") + _T("ORDER BY parent_id ASC, name ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `EVENT_OBJECT_TABLE` AS `table_name`, `TRIGGER_NAME` AS `trigger_name`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.TRIGGERS");
			sql = sql + _T("\n") + _T("WHERE `TRIGGER_SCHEMA` = DATABASE()");
			sql = sql + _T("\n") + _T("ORDER BY `EVENT_OBJECT_TABLE` ASC, `TRIGGER_NAME` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetStoredProcedureInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = sql + _T("SELECT stproc.object_id, stproc.name AS proc_name, CAST(ep.[value] AS NVARCHAR(4000)) AS proc_comment, OBJECT_DEFINITION(stproc.object_id) AS proc_body");
			sql = sql + _T("\n") + _T("FROM sys.procedures AS stproc");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON stproc.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("ORDER BY stproc.name ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `ROUTINE_NAME` AS `proc_name`, `ROUTINE_COMMENT` AS `proc_comment`, '' AS `proc_body`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.ROUTINES");
			sql = sql + _T("\n") + _T("WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE'");
			sql = sql + _T("\n") + _T("ORDER BY `ROUTINE_NAME` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetStoredProcedureParamInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT param.object_id, param.proc_name AS proc_name, param.name AS param_name, param.parameter_id, UPPER(TYPE_NAME(param.user_type_id)) AS datatype, param.max_length, ");
			sql = sql + _T("(UPPER(TYPE_NAME(param.user_type_id)) + (CASE WHEN TYPE_NAME(param.user_type_id) = 'varchar' OR TYPE_NAME(param.user_type_id) = 'char' THEN '(' ");
			sql = sql + _T("+ (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(param.user_type_id) = 'nvarchar' ");
			sql = sql + _T("OR TYPE_NAME(param.user_type_id) = 'nchar' THEN '(' + (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length / 2 AS VARCHAR) END) + ')' ");
			sql = sql + _T("WHEN TYPE_NAME(param.user_type_id) = 'decimal' THEN '(' + CAST(param.precision AS VARCHAR) + ',' + CAST(param.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ");
			sql = sql + _T("CAST(ep.[value] AS NVARCHAR(4000)) AS param_comment");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS proc_name");
			sql = sql + _T("\n\t") + _T("FROM sys.parameters AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.procedures AS b");
			sql = sql + _T("\n\t") + _T("ON a.object_id = b.object_id");
			sql = sql + _T("\n") + _T(") AS param");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("ORDER BY param.object_id ASC, param.parameter_id ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `SPECIFIC_NAME` AS `proc_name`, `PARAMETER_NAME` AS `param_name`, `ORDINAL_POSITION` AS `parameter_id`, UPPER(`DATA_TYPE`) AS `datatype`, ");
			sql = sql + _T("(CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END) AS `max_length`, ");
			sql = sql + _T("UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.PARAMETERS");
			sql = sql + _T("\n") + _T("WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'PROCEDURE')");
			sql = sql + _T("\n") + _T("ORDER BY `SPECIFIC_NAME` ASC, `ORDINAL_POSITION` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetFunctionInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = sql + _T("SELECT so.object_id, so.name AS func_name, CAST(ep.[value] AS NVARCHAR(4000)) AS func_comment, OBJECT_DEFINITION(so.object_id) AS func_body");
			sql = sql + _T("\n") + _T("FROM sys.objects AS so");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON so.object_id = ep.major_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("WHERE so.type IN ('FN', 'IF', 'TF')");
			sql = sql + _T("\n") + _T("ORDER BY so.name ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `ROUTINE_NAME` AS `func_name`, `ROUTINE_COMMENT` AS `func_comment`, '' AS `func_body`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.ROUTINES");
			sql = sql + _T("\n") + _T("WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'");
			sql = sql + _T("\n") + _T("ORDER BY `ROUTINE_NAME` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetFunctionParamInfo(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = _T("SELECT param.object_id, param.func_name AS func_name, param.name AS param_name, param.parameter_id, UPPER(TYPE_NAME(param.user_type_id)) AS datatype, param.max_length, ");
			sql = sql + _T("(UPPER(TYPE_NAME(param.user_type_id)) + (CASE WHEN TYPE_NAME(param.user_type_id) = 'varchar' OR TYPE_NAME(param.user_type_id) = 'char' THEN '(' ");
			sql = sql + _T("+ (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length AS VARCHAR) END) + ')' WHEN TYPE_NAME(param.user_type_id) = 'nvarchar' ");
			sql = sql + _T("OR TYPE_NAME(param.user_type_id) = 'nchar' THEN '(' + (CASE WHEN param.max_length = -1 THEN 'MAX' ELSE CAST(param.max_length / 2 AS VARCHAR) END) + ')' ");
			sql = sql + _T("WHEN TYPE_NAME(param.user_type_id) = 'decimal' THEN '(' + CAST(param.precision AS VARCHAR) + ',' + CAST(param.scale AS VARCHAR) + ')' ELSE '' END)) AS datatype_desc, ");
			sql = sql + _T("CAST(ep.[value] AS NVARCHAR(4000)) AS param_comment");
			sql = sql + _T("\n") + _T("FROM");
			sql = sql + _T("\n") + _T("(");
			sql = sql + _T("\n\t") + _T("SELECT a.*, b.name AS func_name");
			sql = sql + _T("\n\t") + _T("FROM sys.parameters AS a");
			sql = sql + _T("\n\t") + _T("INNER JOIN sys.objects AS b");
			sql = sql + _T("\n\t") + _T("ON a.object_id = b.object_id");
			sql = sql + _T("\n\t") + _T("WHERE b.type IN ('FN', 'IF', 'TF')");
			sql = sql + _T("\n") + _T(") AS param");
			sql = sql + _T("\n") + _T("LEFT OUTER JOIN sys.extended_properties AS ep");
			sql = sql + _T("\n") + _T("ON param.object_id = ep.major_id AND param.parameter_id = ep.minor_id AND ep.name = 'MS_Description'");
			sql = sql + _T("\n") + _T("ORDER BY param.object_id ASC, param.parameter_id ASC;");
			break;
		case DB_CLASS::DB_MYSQL:
			sql = _T("SELECT 0 AS `object_id`, `SPECIFIC_NAME` AS `func_name`, `PARAMETER_NAME` AS `param_name`, `ORDINAL_POSITION` AS `parameter_id`, UPPER(`DATA_TYPE`) AS `datatype`, ");
			sql = sql + _T("(CASE WHEN `CHARACTER_MAXIMUM_LENGTH` IS NOT NULL THEN `CHARACTER_MAXIMUM_LENGTH` ELSE `NUMERIC_PRECISION` END) AS `max_length`, ");
			sql = sql + _T("UPPER(`DTD_IDENTIFIER`) AS `datatype_desc`, '' AS `param_comment`");
			sql = sql + _T("\n") + _T("FROM INFORMATION_SCHEMA.PARAMETERS");
			sql = sql + _T("\n") + _T("WHERE `SPECIFIC_SCHEMA` = DATABASE() AND `SPECIFIC_NAME` IN(SELECT `ROUTINE_NAME` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_TYPE` = 'FUNCTION')");
			sql = sql + _T("\n") + _T("ORDER BY `SPECIFIC_NAME` ASC, `ORDINAL_POSITION` ASC;");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetMSSQLHelpText(DB_OBJECT dbObject)
{
	_tstring sql;

	switch( dbObject )
	{
		case DB_OBJECT::PROCEDURE:
		case DB_OBJECT::FUNCTION:
		case DB_OBJECT::TRIGGERS:
		case DB_OBJECT::EVENTS:
			sql = _T("EXEC sp_helptext ?");
			break;
	}

	return sql;
}

//***************************************************************************
//
inline _tstring GetMySQLShowObject(DB_OBJECT dbObject, _tstring objectName)
{
	_tstring sql;

	switch( dbObject )
	{
		case DB_OBJECT::TABLE:
			sql = _T("SHOW CREATE TABLE ") + objectName + _T(";");
			break;
		case DB_OBJECT::PROCEDURE:
			sql = _T("SHOW CREATE PROCEDURE ") + objectName + _T(";");
			break;
		case DB_OBJECT::FUNCTION:
			sql = _T("SHOW CREATE FUNCTION ") + objectName + _T(";");
			break;
		case DB_OBJECT::TRIGGERS:
			sql = _T("SHOW CREATE TRIGGERS ") + objectName + _T(";");
			break;
		case DB_OBJECT::EVENTS:
			sql = _T("SHOW CREATE EVENTS ") + objectName + _T(";");
			break;
	}

	return sql;
}

#endif // ndef __BASESQL_H__