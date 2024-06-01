
//***************************************************************************
// DBMYSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBMYSQLQUERY_H__
#define __DBMYSQLQUERY_H__

#pragma once

//***************************************************************************
//
inline _tstring MYSQLGetDatabaseBackupQuery(string loginPath, string dbName, string backupFilePath, string defaultCharacterSet = "", bool isNoData = false)
{
	// mysqldump -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] > [백업 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	// --set-gtid-purged=OFF : GTID(Global Transaction Identifier) 활성화 여부 설정(GTID를 사용하지 않는 MySQL DB를 복구하려면 백업 수행 시 --set-gtid-purged=OFF 옵션을 추가).
	// --single-transaction : lock 을 걸지 않고도 dump 파일의 정합성 보장. InnoDB 일때만 사용 가능.
	// --no-tablespaces : 해당 옵션을 줄 경우 CREATE LOGFILE GROUP과 CREATE TABLESPACE문을 생성하지 않음
	// --default-character-set=utf8mb4 : 기본 문자 집합을 utf8mb4로 지정
	// --no-data : 데이터를 백업하지 않고, DDL만 백업
	string query = string_format("mysqldump --login-path=%s %s --routines --events --single-transaction --set-gtid-purged=OFF --no-tablespaces%s%s > %s",
		loginPath.c_str(),
		dbName.c_str(),
		defaultCharacterSet != "" ? " --default-character-set=" + defaultCharacterSet : "",
		isNoData ? " --no-data" : "",
		backupFilePath.c_str());
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetSystemViewTableQuery(string tableName = "")
{
	string query = "SELECT `TABLE_NAME` AS `table_name`, `TABLE_COMMENT` AS `table_comment`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";
	if( tableName != "" )
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = 'information_schema' AND `TABLE_NAME` = '" + tableName + "';";
	}
	else
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = 'information_schema'";
		query = query + "\n" + "ORDER BY `TABLE_NAME` ASC;";
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetSystemViewTableColumnQuery(string tableName = "")
{
	string query = "SELECT `TABLE_NAME` AS `table_name`, `ORDINAL_POSITION` AS `seq`, `COLUMN_NAME` AS `column_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
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
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = 'information_schema' AND `TABLE_NAME` = '" + tableName + "'";
		query = query + "\n" + "ORDER BY `ORDINAL_POSITION` ASC;";
	}
	else
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = 'information_schema'";
		query = query + "\n" + "ORDER BY `TABLE_NAME` ASC, `ORDINAL_POSITION` ASC;";
	}
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetDatabaseRestoreQuery(string loginPath, string dbName, string restoreFilePath)
{
	// mysql -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] < [복원할 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	string query = string_format("mysql --login-path=%s %s < %s", loginPath.c_str(), dbName.c_str(), restoreFilePath.c_str());
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetCharacterSetsQuery(string charset = "")
{
	string query = "SELECT `CHARACTER_SET_NAME` AS `characterset`, `DEFAULT_COLLATE_NAME` AS `default_collation`, `DESCRIPTION` AS `description`, `MAXLEN` AS `maxlen`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.CHARACTER_SETS";
	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC;";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetCollationsQuery(string charset = "")
{
	string query = "SELECT `CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`, `ID` AS `id`, `IS_COMPILED` AS `is_compiled`, `IS_DEFAULT` AS `is_default`, ";
	query = query + "`PAD_ATTRIBUTE` AS `pad_attribute`, `SORTLEN` AS `sortlen`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.COLLATIONS";
	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC, `COLLATION_NAME` ASC;";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetCharacterSetCollationsQuery(string charset)
{
	string query = "SELECT `CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY";
	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC, `COLLATION_NAME` ASC";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetEnginesQuery()
{
	string query = "SELECT `ENGINE` AS `engine`, `SUPPORT` AS `support`, `COMMENT` AS `comment`, `TRANSACTIONS` AS `transactions`, `XA` AS `xa`, `SAVEPOINTS` AS `savepoints`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ENGINES";
	query = query + "\n" + "ORDER BY `ENGINE` ASC";
	return TStringToString(query);
}

//***************************************************************************
//
inline _tstring MYSQLGetAlterTableQuery(_tstring tableName, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring engine = _T(""))
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
inline _tstring MYSQLGetAlterTableCollationQuery(_tstring tableName, _tstring characterSet, _tstring collation)
{
	// 테이블 캐릭터셋, 데이터정렬(문자비교규칙)을 변경
	// ALTER TABLE `테이블명` CONVERT TO CHARACTER SET 캐릭터셋 COLLATE 데이터정렬;
	_tstring query = _T("ALTER TABLE `") + tableName + _T("` CONVERT TO CHARACTER SET ") + characterSet + _T(" COLLATE ") + collation + _T(";");
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTableFragmentationCheckQuery(string tableName = "")
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
	if( tableName != "" )
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "'";
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
inline _tstring MYSQLGetOptimizeTableQuery(_tstring tableName)
{
	// OPTIMIZE TABLE `테이블명`
	// MYSQL은 인덱스 리빌드가 존재하지 않고, 테이블 최적화(OPTIMIZE TABLE)를 진행해야 함. 
	// OPTIMIZE [NO_WRITE_TO_BINLOG | LOCAL] TABLE 테이블명 [, 테이블명] ...
	// 기본적으로 서버는 OPTIMIZE TABLE 복제본에 복제되도록 바이너리 로그에 명령문을 기록
	//  - 로깅을 억제하려면 선택적 NO_WRITE_TO_BINLOG 키워드 또는 별칭 LOCAL 키워드 지정
	_tstring query = tstring_format(_T("OPTIMIZE TABLE `{0}`;"), tableName.c_str());
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetShowObjectQuery(EDBObjectType dbObject, _tstring objectName)
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
inline _tstring MYSQLGetRenameObjectQuery(_tstring tableName, _tstring chgName, _tstring columnName = _T(""), _tstring dataTypeDesc = _T(""), bool isNullable = false, _tstring defaultDefinition = _T(""), bool isIdentity = false, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring comment = _T(""))
{
	_tstring query = _T("");

	if( columnName != _T("") )
	{
		// ALTER TABLE `테이블명` RENAME `변경할테이블명`;
		query = tstring_format(_T("ALTER TABLE `{%s}` RENAME `{%s}`;"), tableName.c_str(), chgName.c_str());
	}
	else
	{
		// ALTER TABLE `테이블명` CHANGE COLUMN `컬럼명` `변경할컬럼명` 데이터타입 컬럼속성;
		_tstring columnOption = GetTableColumnOption(EDBClass::MYSQL, dataTypeDesc, isNullable, defaultDefinition, isIdentity, 0, 0, characterSet, collation, comment);
		query = tstring_format(_T("ALTER TABLE `{%s}` CHANGE COLUMN `{%s}` `{%s}` {%s};"), tableName.c_str(), columnName.c_str(), chgName.c_str(), columnOption.c_str());
	}
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

#endif // ndef __DBMYSQLQUERY_H__