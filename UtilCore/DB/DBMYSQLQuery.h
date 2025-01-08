
//***************************************************************************
// DBMYSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBMYSQLQUERY_H__
#define __DBMYSQLQUERY_H__

#pragma once

//***************************************************************************
// MYSQL 인덱스타입 : (CASE INFORMATION_SCHEMA.TABLE_CONSTRAINTS.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 WHEN 'UNIQUE' THEN 2 ELSE (CASE INFORMATION_SCHEMA.STATISTICS.`INDEX_TYPE` WHEN 'BTREE' THEN 3 WHEN 'FULLTEXT' THEN 4 WHEN 'SPATIAL' THEN 5 ELSE 0 END) END)
enum class EMYSQLIndexType
{
	NONE = 0,
	PRIMARY__KEY = 1,
	UNIQUE = 2,
	INDEX = 3,
	FULLTEXT = 4,
	SPATIAL = 5
};

inline const TCHAR* ToString(EMYSQLIndexType v)
{
	switch( v )
	{
		case EMYSQLIndexType::NONE:			return _T("NONE");
		case EMYSQLIndexType::PRIMARY__KEY:	return _T("PRIMARY KEY");
		case EMYSQLIndexType::UNIQUE:		return _T("UNIQUE");
		case EMYSQLIndexType::INDEX:		return _T("INDEX");
		case EMYSQLIndexType::FULLTEXT:		return _T("FULLTEXT");
		case EMYSQLIndexType::SPATIAL:		return _T("SPATIAL");
		default:							return _T("NONE");
	}
}

inline const EMYSQLIndexType StringToMYSQLIndexType(const TCHAR* ptszIndexType)
{
	if( ::_tcsicmp(ptszIndexType, _T("PRIMARY KEY")) == 0 )
		return EMYSQLIndexType::PRIMARY__KEY;
	else if( ::_tcsicmp(ptszIndexType, _T("UNIQUE")) == 0 )
		return EMYSQLIndexType::UNIQUE;
	else if( ::_tcsicmp(ptszIndexType, _T("INDEX")) == 0 )
		return EMYSQLIndexType::INDEX;
	else if( ::_tcsicmp(ptszIndexType, _T("FULLTEXT")) == 0 )
		return EMYSQLIndexType::FULLTEXT;
	else if( ::_tcsicmp(ptszIndexType, _T("SPATIAL")) == 0 )
		return EMYSQLIndexType::SPATIAL;

	return EMYSQLIndexType::NONE;
}

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
inline _tstring MYSQLGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, _tstring defaultDefinition, bool isIdentity, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring comment = _T(""))
{
	_tstring columnOption = _T("");

	// <컬럼속성> : CHARACTER SET, COLLATE, {NULL|NOT NULL}, DEFAULT, AUTO_INCREMENT, COMMENT 설정이 포함됨
	//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
	//  - 만약 캐릭터셋, 데이터정렬 값이 해당 데이터베이스에 설정된 캐릭터셋, 데이터정렬 값과 동일한 경우 따로 명시하지 않아도 됨
	//  - CHARACTER SET '캐릭터셋' COLLATE '데이터정렬' {NULL|NOT NULL} DEFAULT 값 AUTO_INCREMENT COMMENT '코멘트'
	//
	// `컬럼명` 데이터타입 <컬럼속성>
	//  - `컬럼명` 데이터타입 NOT NULL AUTO_INCREMENT COMMENT '코멘트'
	//  - `컬럼명` 데이터타입 {NULL|NOT NULL}
	//  - `컬럼명` 데이터타입 {NULL|NOT NULL} DEFAULT 값 COMMENT '코멘트'
	//  - `컬럼명` 데이터타입 CHARACTER SET '캐릭터셋' COLLATE '데이터정렬' {NULL|NOT NULL} COMMENT '코멘트'
	//  - `컬럼명` 데이터타입 {NULL|NOT NULL} COMMENT '코멘트'
	columnOption = dataTypeDesc;
	if( characterSet != "" && collation != "" )
		columnOption = columnOption + " CHARACTER SET '" + characterSet + "' COLLATE '" + collation + "'";

	columnOption = columnOption + (isNullable ? " NULL" : " NOT NULL") + (defaultDefinition != "" ? _T(" DEFAULT ") + defaultDefinition : _T("")) + (isIdentity ? " AUTO_INCREMENT" : "");

	if( comment != "" )
		columnOption = columnOption + " COMMENT '" + comment + "'";

	return columnOption;
}

//***************************************************************************
//
inline _tstring MYSQLGetDropConstraintQuery(_tstring tableName, _tstring constType, _tstring constName)
{
	_tstring query = _T("");

	if( constType == "PRIMARY KEY" )
	{
		// ALTER TABLE `테이블명` DROP PRIMARY KEY;
		query = tstring_tcformat(_T("ALTER TABLE `%s` DROP PRIMARY KEY;"), tableName.c_str());
	}
	else if( constType == "UNIQUE" )
	{
		// ALTER TABLE `테이블명` DROP INDEX `제약조건명`;
		query = tstring_tcformat(_T("ALTER TABLE `%s` DROP INDEX `%s`;"), tableName.c_str(), constName.c_str());
	}
	else if( constType == "FOREIGN KEY" )
	{
		// ALTER TABLE `테이블명` DROP FOREIGN KEY `제약조건명`;
		query = tstring_tcformat(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), tableName.c_str(), constName.c_str());
	}
	else if( constType == "CHECK" )
	{
		// ALTER TABLE `테이블명` DROP CHECK `제약조건명`;
		query = tstring_tcformat(_T("ALTER TABLE `%s` DROP CHECK `%s`;"), tableName.c_str(), constName.c_str());
	}
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetDBSystemQuery()
{
	_tstring query = _T("");

	query = query + "SELECT CONCAT('MYSQL ', VERSION()) AS `version`, `DEFAULT_CHARACTER_SET_NAME` AS `characterset`, DEFAULT_COLLATION_NAME AS 'collation'";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.SCHEMATA";
	query = query + "\n" + "WHERE `SCHEMA_NAME` = DATABASE();";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetUserListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT USER AS `name` FROM mysql.user;";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetDatabaseListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT `SCHEMA_NAME` FROM INFORMATION_SCHEMA.SCHEMATA WHERE `SCHEMA_NAME` NOT IN('sys', 'mysql', 'information_schema', 'performance_schema', 'world') ORDER BY `SCHEMA_NAME` ASC;";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetDatabaseBackupQuery(_tstring loginPath, _tstring dbName, _tstring backupFilePath, _tstring defaultCharacterSet = _T(""), bool isNoData = false)
{
	// mysqldump -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] > [백업 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	// --set-gtid-purged=OFF : GTID(Global Transaction Identifier) 활성화 여부 설정(GTID를 사용하지 않는 MySQL DB를 복구하려면 백업 수행 시 --set-gtid-purged=OFF 옵션을 추가).
	// --single-transaction : lock 을 걸지 않고도 dump 파일의 정합성 보장. InnoDB 일때만 사용 가능.
	// --no-tablespaces : 해당 옵션을 줄 경우 CREATE LOGFILE GROUP과 CREATE TABLESPACE문을 생성하지 않음
	// --default-character-set=utf8mb4 : 기본 문자 집합을 utf8mb4로 지정
	// --no-data : 데이터를 백업하지 않고, DDL만 백업
	_tstring query = tstring_tcformat(_T("mysqldump --login-path=%s %s --routines --events --single-transaction --set-gtid-purged=OFF --no-tablespaces%s%s > %s"),
		loginPath.c_str(),
		dbName.c_str(),
		defaultCharacterSet != "" ? _T(" --default-character-set=") + defaultCharacterSet : _T(""),
		isNoData ? _T(" --no-data") : _T(""),
		backupFilePath.c_str());
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetDatabaseRestoreQuery(_tstring loginPath, _tstring dbName, _tstring restoreFilePath)
{
	// mysql -h [원격호스트명(IP)] port=[연결포트] -u [사용자 계정] -p [패스워드] [데이터베이스명] < [복원할 파일 경로]
	// --login-path : mysql_config_editor를 이용하여 MySQL 서버 연결에 대한 자격 정보(login-path 이름, user, password, host, port, socket 정보가 난독화되어 들어있음)를 저장
	_tstring query = tstring_tcformat(_T("mysql --login-path=%s %s < %s"), loginPath.c_str(), dbName.c_str(), restoreFilePath.c_str());
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetSystemViewTableQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `TABLE_NAME` AS `table_name`, `TABLE_COMMENT` AS `table_comment`";
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
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetSystemViewTableColumnQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `TABLE_NAME` AS `table_name`, `ORDINAL_POSITION` AS `seq`, `COLUMN_NAME` AS `column_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
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
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetCharacterSetsQuery(_tstring charset = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `CHARACTER_SET_NAME` AS `characterset`, `DEFAULT_COLLATE_NAME` AS `default_collation`, `DESCRIPTION` AS `description`, `MAXLEN` AS `maxlen`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.CHARACTER_SETS";
	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetCollationsQuery(_tstring charset = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`, `ID` AS `id`, `IS_COMPILED` AS `is_compiled`, `IS_DEFAULT` AS `is_default`, ";
	query = query + "`PAD_ATTRIBUTE` AS `pad_attribute`, `SORTLEN` AS `sortlen`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.COLLATIONS";
	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC, `COLLATION_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetCharacterSetCollationsQuery(_tstring charset = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `CHARACTER_SET_NAME` AS `characterset`, `COLLATION_NAME` AS `collation`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY";

	if( charset != "" )
		query = query + "\n" + "WHERE `CHARACTER_SET_NAME` = '" + charset + "'";
	query = query + "\n" + "ORDER BY `CHARACTER_SET_NAME` ASC, `COLLATION_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetEnginesQuery()
{
	_tstring query = _T("");

	query = query + "SELECT `ENGINE` AS `engine`, `SUPPORT` AS `support`, `COMMENT` AS `comment`, `TRANSACTIONS` AS `transactions`, `XA` AS `xa`, `SAVEPOINTS` AS `savepoints`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ENGINES";
	query = query + "\n" + "ORDER BY `ENGINE` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetAlterTableQuery(_tstring tableName, _tstring characterSet = _T(""), _tstring collation = _T(""), _tstring engine = _T(""))
{
	_tstring query = _T("");

	// 테이블 캐릭터셋, 데이터정렬(문자비교규칙), 스토리지엔진 변경
	// ALTER TABLE `테이블명` CHARACTER SET = 캐릭터셋, COLLATE = 데이터정렬, ENGINE = 스토리지엔진;
	query = query + "ALTER TABLE `" + tableName + "`";

	if( characterSet != "" )
		query = query + " CHARACTER SET = " + characterSet + ",";

	if( collation != "" )
		query = query + " COLLATE = " + collation + ",";

	if( engine != "" )
		query = query + " ENGINE = " + engine;

	query = query + ";";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetAlterTableCollationQuery(_tstring tableName, _tstring characterSet, _tstring collation)
{
	_tstring query = _T("");

	// 테이블 캐릭터셋, 데이터정렬(문자비교규칙)을 변경
	// ALTER TABLE `테이블명` CONVERT TO CHARACTER SET 캐릭터셋 COLLATE 데이터정렬;
	query = query + "ALTER TABLE `" + tableName + "` CONVERT TO CHARACTER SET " + characterSet + " COLLATE " + collation + ";";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTableFragmentationCheckQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	// 테이블 단편화(Fragmentation) 확인
	// [발생 원인]
	//  - fragmentation이란 INSERT & DELETE가 수차례 반복되면서 Page 안에 회수가 안되는(사용되지 않는) 부분이 많아지면서 발생하게 되는데
	//    그 영향으로 테이블이 실제로 가져야 하는 OS 공간 보다 더 많은 공간을 차지하게 됨.
	// [확인 방법]
	//  - OS 서버에서 해당 테이블에 ibd 파일과 아래 쿼리에 total의 사이즈를 비교하여 간극 만큼을 단편화(Fragmentation)로 판단할 수 있고,
	//    이 경우에 테이블 최적화(OPTIMIZE TABLE)를 수행해서 성능 향상
	query = query + "SELECT `TABLE_NAME` AS `table_name`, ";
	query = query + "ROUND((DATA_LENGTH + INDEX_LENGTH) / (1024 * 1024), 2) AS `totalsize`, ";
	query = query + "ROUND((DATA_FREE) / (1024 * 1024 ), 2) AS `datafreesize`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "';";
	}
	else
	{
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
		query = query + "\n" + "ORDER BY `total` DESC;";
	}
	return query;
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
	_tstring query = tstring_tcformat(_T("OPTIMIZE TABLE `%s`;"), tableName.c_str());
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
			query = query + "SHOW CREATE TABLE " + objectName + ";";
			break;
		case EDBObjectType::PROCEDURE:
			query = query + "SHOW CREATE PROCEDURE " + objectName + ";";
			break;
		case EDBObjectType::FUNCTION:
			query = query + "SHOW CREATE FUNCTION " + objectName + ";";
			break;
		case EDBObjectType::TRIGGERS:
			query = query + "SHOW CREATE TRIGGER " + objectName + ";";
			break;
		case EDBObjectType::EVENTS:
			query = query + "SHOW CREATE EVENT " + objectName + ";";
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
		query = tstring_tcformat(_T("ALTER TABLE `%s` RENAME `%s`;"), tableName.c_str(), chgName.c_str());
	}
	else
	{
		// ALTER TABLE `테이블명` CHANGE COLUMN `컬럼명` `변경할컬럼명` 데이터타입 컬럼속성;
		_tstring columnOption = MYSQLGetTableColumnOption(dataTypeDesc, isNullable, defaultDefinition, isIdentity, characterSet, collation, comment);
		query = tstring_tcformat(_T("ALTER TABLE `%s` CHANGE COLUMN `%s` `%s` %s;"), tableName.c_str(), columnName.c_str(), chgName.c_str(), columnOption.c_str());
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

	if( columnName != "" )
	{
		query = query + "SELECT `COLUMN_COMMENT` AS `columncomment` FROM INFORMATION_SCHEMA.COLUMNS WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "' AND `COLUMN_NAME` = '" + columnName + "';";
	}
	else query = query + "SELECT `TABLE_COMMENT` AS `tableComment` FROM INFORMATION_SCHEMA.TABLES WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "';";

	return query;
}

//***************************************************************************
// Ex)
//	ALTER TABLE `tbl_table1` COMMENT '테스트 테이블';
//  ALTER TABLE `tbl_table1` MODIFY `Id` VARCHAR(50) NOT NULL COMMENT '아이디';
inline _tstring MYSQLProcessTableColumnCommentQuery(_tstring tableName, _tstring setComment, _tstring columnName = _T(""), _tstring dataTypeDesc = _T(""), bool isNullable = false, _tstring defaultDefinition = _T(""), bool isIdentity = false, _tstring characterSet = _T(""), _tstring collation = _T(""))
{
	_tstring query = _T("");

	if( columnName != "" )
	{
		_tstring columnOption = MYSQLGetTableColumnOption(dataTypeDesc, isNullable, defaultDefinition, isIdentity, characterSet, collation);
		query = query + "ALTER TABLE `" + tableName + "` MODIFY `" + columnName + "` " + columnOption + " COMMENT '" + setComment + "';";
	}
	else query = query + "ALTER TABLE `" + tableName + "` COMMENT '" + setComment + "';";

	return query;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `procComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = 'sp_procedure1';
inline _tstring MYSQLGetProcedureCommentQuery(_tstring procName)
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_COMMENT` AS `procComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = '" + procName + "';";
	return query;
}

//***************************************************************************
// Ex)
//	ALTER PROCEDURE `sp_procedure1` COMMENT '테스트 저장프로시저';
inline _tstring MYSQLProcessProcedureCommentQuery(_tstring procName, _tstring comment)
{
	_tstring query = _T("");

	query = query + "ALTER PROCEDURE `" + procName + "` COMMENT '" + comment + "';";
	return query;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `funcComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = 'sp_function1';
inline _tstring MYSQLGetFunctionCommentQuery(_tstring funcName)
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_COMMENT` AS `funcComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = '" + funcName + "';";
	return query;
}

//***************************************************************************
// Ex)
//	ALTER FUNCTION `sp_function1` COMMENT '테스트 함수';
inline _tstring MYSQLProcessFunctionCommentQuery(_tstring funcName, _tstring comment)
{
	_tstring query = _T("");

	query = query + "ALTER FUNCTION `" + funcName + "` COMMENT '" + comment + "';";
	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTableListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT `TABLE_SCHEMA` AS `db_name`, 1 AS object_type, `TABLE_NAME` AS `object_name`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLES";
	query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE()";
	query = query + "\n" + "ORDER BY `TABLE_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTableInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT a.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, a.`TABLE_NAME` AS `table_name`, IFNULL(a.`AUTO_INCREMENT`, 0) AS `auto_increment`, ";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTableColumnInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `TABLE_NAME` AS `table_name`, `ORDINAL_POSITION` AS `seq`, `COLUMN_NAME` AS `column_name`, UPPER(`DATA_TYPE`) AS `datatype`, ";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetConstraintsInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT const.`TABLE_SCHEMA` AS `db_name`, '' AS `schema_name`, 0 AS `object_id`, const.`TABLE_NAME` AS `table_name`, const.`CONSTRAINT_NAME` AS `const_name`, ";
	query = query + "const.`CONSTRAINT_TYPE` AS `const_type`, '' AS `const_type_desc`, ckconst.`CHECK_CLAUSE` AS `const_value`, 0 AS `is_system_named`, ";
	query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN 1 WHEN 'UNIQUE' THEN 2 WHEN 'FOREIGN KEY' THEN 3 WHEN 'CHECK' THEN 5 END) AS `sort_value`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
	query = query + "\n" + "LEFT OUTER JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS ckconst";
	query = query + "\n" + "ON const.`TABLE_SCHEMA` = ckconst.`CONSTRAINT_SCHEMA` AND const.`CONSTRAINT_NAME` = ckconst.`CONSTRAINT_NAME`";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + tableName + "'";
		query = query + "\n" + "ORDER BY `sort_value` ASC;";
	}
	else
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE()";
		query = query + "\n" + "ORDER BY const.`TABLE_NAME` ASC, `sort_value` ASC;";
	}

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetIndexInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT stat.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, stat.`TABLE_NAME` AS `table_name`, stat.`INDEX_NAME` AS `index_name`, 0 AS `index_id`, ";
	query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' || 'UNIQUE' THEN const.`CONSTRAINT_TYPE` ELSE (CASE stat.`INDEX_TYPE` WHEN 'BTREE' THEN 'INDEX' ELSE stat.`INDEX_TYPE` END) END) AS `index_type`, ";
	query = query + "(CASE const.`CONSTRAINT_TYPE` WHEN 'PRIMARY KEY' THEN true ELSE false END) AS `is_primary_key`, ";
	query = query + "(CASE stat.`NON_UNIQUE` WHEN 0 THEN true ELSE false END) AS `is_unique`, stat.`SEQ_IN_INDEX` AS `column_seq`, ";
	query = query + "stat.`COLUMN_NAME` AS `column_name`, (CASE stat.`COLLATION` WHEN 'D' THEN 2 ELSE 1 END) AS `column_sort`, 0 AS `is_system_named`";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetPartitionInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `TABLE_SCHEMA`, `TABLE_NAME`, `PARTITION_NAME`, `SUBPARTITION_NAME`, `PARTITION_ORDINAL_POSITION`, `SUBPARTITION_ORDINAL_POSITION`, `PARTITION_METHOD`, `SUBPARTITION_METHOD`, `PARTITION_EXPRESSION`, `SUBPARTITION_EXPRESSION`, `PARTITION_DESCRIPTION`, `TABLE_ROWS`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.PARTITIONS";

	if( tableName != "" )
		query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + tableName + "' AND `PARTITION_NAME` IS NOT NULL";
	else query = query + "\n" + "WHERE `TABLE_SCHEMA` = DATABASE() AND `PARTITION_NAME` IS NOT NULL";

	query = query + "\n" + "GROUP BY `TABLE_NAME`, `PARTITION_NAME`, `SUBPARTITION_NAME`, `PARTITION_ORDINAL_POSITION`, `SUBPARTITION_ORDINAL_POSITION`, `PARTITION_METHOD`, `SUBPARTITION_METHOD`, `PARTITION_EXPRESSION`, `SUBPARTITION_EXPRESSION`,  `PARTITION_DESCRIPTION`, `TABLE_ROWS`";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY `PARTITION_METHOD` ASC, `PARTITION_ORDINAL_POSITION` ASC,  `SUBPARTITION_METHOD` ASC, `SUBPARTITION_ORDINAL_POSITION` ASC;";
	else query = query + "\n" + "ORDER BY `TABLE_NAME` ASC, `PARTITION_METHOD` ASC, `PARTITION_ORDINAL_POSITION` ASC,  `SUBPARTITION_METHOD` ASC, `SUBPARTITION_ORDINAL_POSITION` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetForeignKeyInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT const.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, colusage.`CONSTRAINT_NAME` AS `foreignkey_name`, 0 AS `is_disabled`, 0 AS `is_not_trusted`, ";
	query = query + "colusage.`TABLE_NAME` AS `foreignkey_table_name`, colusage.`COLUMN_NAME` AS `foreignkey_column_name`, ";
	query = query + "'' AS `referencekey_schema_name`, colusage.`REFERENCED_TABLE_NAME` AS `referencekey_table_name`, colusage.`REFERENCED_COLUMN_NAME` AS `referencekey_column_name`, ";
	query = query + "refconst.`UPDATE_RULE` AS `update_rule`, refconst.`DELETE_RULE` AS `delete_rule`, 0 AS `is_system_named`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
	query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS colusage";
	query = query + "\n" + "ON const.`TABLE_SCHEMA` = colusage.TABLE_SCHEMA AND const.`TABLE_NAME` = colusage.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = colusage.`CONSTRAINT_NAME`";
	query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS refconst";
	query = query + "\n" + "ON const.`TABLE_SCHEMA` = refconst.`CONSTRAINT_SCHEMA` AND const.`TABLE_NAME` = refconst.`TABLE_NAME` AND const.`CONSTRAINT_NAME` = refconst.`CONSTRAINT_NAME`";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + tableName + "' AND const.`CONSTRAINT_TYPE` = 'FOREIGN KEY'";
		query = query + "\n" + "ORDER BY const.`CONSTRAINT_NAME` ASC, colusage.`ORDINAL_POSITION` ASC;";
	}
	else
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`CONSTRAINT_TYPE` = 'FOREIGN KEY'";
		query = query + "\n" + "ORDER BY const.`TABLE_NAME` ASC, const.`CONSTRAINT_NAME` ASC, colusage.`ORDINAL_POSITION` ASC;";
	}

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetCheckConstInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT const.`TABLE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, const.`TABLE_NAME` AS `table_name`, const.`CONSTRAINT_NAME` AS `check_const_name`, ";
	query = query + "ckconst.`CHECK_CLAUSE` AS `check_value`, 0 AS `is_system_named`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS const";
	query = query + "\n" + "INNER JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS ckconst";
	query = query + "\n" + "ON const.`TABLE_SCHEMA` = ckconst.`CONSTRAINT_SCHEMA` AND const.`CONSTRAINT_NAME` = ckconst.`CONSTRAINT_NAME`";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`TABLE_NAME` = '" + tableName + "' AND const.`CONSTRAINT_TYPE` = 'CHECK'";
		query = query + "\n" + "ORDER BY const.`CONSTRAINT_NAME` ASC;";
	}
	else
	{
		query = query + "\n" + "WHERE const.`TABLE_SCHEMA` = DATABASE() AND const.`CONSTRAINT_TYPE` = 'CHECK'";
		query = query + "\n" + "ORDER BY const.`TABLE_NAME` ASC, const.`CONSTRAINT_NAME` ASC;";
	}

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetTriggerInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `TRIGGER_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `EVENT_OBJECT_TABLE` AS `table_name`, `TRIGGER_NAME` AS `trigger_name`";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetProcedureListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_SCHEMA` AS `db_name`, 2 AS `object_type`, `ROUTINE_NAME` AS `object_name`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
	query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE'";
	query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetProcedureInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `proc_name`, `ROUTINE_COMMENT` AS `proc_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetProcedureParamInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `SPECIFIC_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `SPECIFIC_NAME` AS `proc_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
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

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetFunctionListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_SCHEMA` AS `db_name`, 3 AS `object_type`, `ROUTINE_NAME` AS `object_name`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";
	query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'";
	query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetFunctionInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `ROUTINE_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `ROUTINE_NAME` AS `func_name`, `ROUTINE_COMMENT` AS `func_comment`, `CREATED` AS `create_date`, `LAST_ALTERED` AS `modify_date`";
	query = query + "\n" + "FROM INFORMATION_SCHEMA.ROUTINES";

	if( funcName != "" )
		query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = '" + funcName + "'";
	else query = query + "\n" + "WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION'";
	query = query + "\n" + "ORDER BY `ROUTINE_NAME` ASC;";

	return query;
}

//***************************************************************************
//
inline _tstring MYSQLGetFunctionParamInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT `SPECIFIC_SCHEMA` AS `db_name`, 0 AS `object_id`, '' AS `schema_name`, `SPECIFIC_NAME` AS `func_name`, `ORDINAL_POSITION` AS `parameter_id`, (CASE `PARAMETER_MODE` WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS `param_mode`, ";
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

	return query;
}

#endif // ndef __DBMYSQLQUERY_H__