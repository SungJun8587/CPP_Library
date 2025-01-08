
//***************************************************************************
// DBModel.cpp: implementation of the Database Model.
//
//***************************************************************************

#include "pch.h"
#include "DBModel.h"

using namespace DBModel;

//***************************************************************************
//
_tstring Table::CreateTable()
{
	_tstring query = _T("");
	_tstring columnsStr = _T("");

	const int32 size = static_cast<int32>(_columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i != 0 )
			columnsStr += _T(",");
		columnsStr += _T("\n\t");
		columnsStr += _columns[i]->CreateColumnText();
	}

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// CREATE TABLE [스키마명].[테이블명] (
			//    [컬럼명1] [bigint] NOT NULL IDENTITY(1, 1) [CONSTRAINT [제약조건명]] PRIMARY KEY,                     -- 선언할 때 같이 추가(기본키, 제약조건명 설정 가능)
			//    [컬럼명2] [varchar](32) NOT NULL,                                            
			//    [컬럼명3] [varchar](32) NOT NULL [CONSTRAINT [제약조건명]] UNIQUE,                                    -- 선언할 때 같이 추가(유니크, 제약조건명 설정 가능) 
			//    [컬럼명4] [varchar](500) NOT NULL, 
			//    [컬럼명5] [bigint] NOT NULL [CONSTRAINT [제약조건명]] FOREIGN KEY REFERENCES [테이블명] ([컬럼명1]),    -- 선언할 때 같이 추가(외래키, 제약조건명 설정 가능)
			//    [컬럼명6] [int] NOT NULL [CONSTRAINT [제약조건명]] FOREIGN KEY REFERENCES [부모테이블명] ([PK컬럼명]),  -- 선언할 때 같이 추가(외래키, 제약조건명 설정 가능)
			//    [컬럼명7] [tinyint] NOT NULL [CONSTRAINT [제약조건명]] CHECK ([컬럼명7] >= 1 AND [컬럼명7] <= 10),     -- 선언할 때 같이 추가(체크제약조건, 제약조건명 설정 가능)
			//    [컬럼명8] [datetime] NOT NULL [CONSTRAINT [제약조건명]] DEFAULT getdate(),                            -- DEFAULT 값 설정(제약조건명 설정 가능)                       
			//    
			//    [CONSTRAINT [제약조건명]] PRIMARY KEY ([컬럼명1]),                                                    -- 뒤에 따로 추가(기본키, 제약조건명 설정 가능)
			//    [CONSTRAINT [제약조건명]] UNIQUE ([컬럼명3]),                                                         -- 뒤에 따로 추가(유니크, 제약조건명 설정 가능)
			//    INDEX 인덱스명 NONCLUSTERED COLUMNSTORE ([컬럼명4]),                                                  -- 뒤에 추가(Columnstore 인덱스)
			//    INDEX 인덱스명 NONCLUSTERED ([컬럼명5]),                                                              -- 뒤에 추가(인덱스)
			//    INDEX 인덱스명 NONCLUSTERED ([컬럼명6]),                                                              -- 뒤에 추가(인덱스)
			//    [CONSTRAINT [제약조건명]] CHECK ([컬럼명7] >= 1 AND [컬럼명7] <= 10),                                  -- 뒤에 따로 추가(체크제약조건, 제약조건명 설정 가능)   
			//    [CONSTRAINT [제약조건명]] FOREIGN KEY ([컬럼명5]) REFERENCES [테이블명] ([컬럼명1]),                    -- 뒤에 추가(자기 자신 외래키 참조, 제약조건명 설정 가능)
			//    [CONSTRAINT [제약조건명]] FOREIGN KEY ([컬럼명6]) REFERENCES [부모테이블명] ([PK컬럼명])                 -- 뒤에 추가(다른테이블 외래키 참조, 제약조건명 설정 가능)
			// )
			//
			//  - PRIMARY KEY, UNIQUE, CHECK, FOREIGN KEY 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 모든 컬럼 정의 마지막에 따로 추가할 수도 있음
			//  - DEFAULT 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 테이블 수정(ALTER TABLE)으로 설정할 수도 있음
			//  - INDEX 선언 위치는 무조건 모든 컬럼 정의 마지막에 따로 추가해야 하고, 필히 인덱스명을 명시해야 함(인덱스명을 생략할 수 없음)
			//  - 제약조건명을 명시하지 않고 추가하려면 [CONSTRAINT [제약조건명]] 이 부분을 생략하면 가능
			//  - 시스템이 제약조건명을 자동 할당했는지 아니면 생성시 설정한 제약조건명으로 설정했는지 확인 
			//    : sys.key_constraints 테이블에 is_system_named 컬럼(1이면 시스템 자동 할당, 0이면 설정한 제약조건명) 참조
			query = tstring_tcformat(_T("CREATE TABLE [%s].[%s] (%s)"), _schemaName.c_str(), _tableName.c_str(), columnsStr.c_str());
			break;
		case EDBClass::MYSQL:
			// CREATE TABLE `테이블명` (
			//    `컬럼명1` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,                                   -- 선언할 때 같이 추가(기본키, 제약조건명 시스템이 자동 생성)
			//    `컬럼명2` VARCHAR(32) NOT NULL,                                                        
			//    `컬럼명3` VARCHAR(32) NOT NULL UNIQUE,                                                           -- 선언할 때 같이 추가(유니크, 제약조건명 시스템이 자동 생성) 
			//    `컬럼명4` INT UNSIGNED NOT NULL DEFAULT 0,                                                       -- DEFAULT 값 설정            
			//    `컬럼명5` POLYGON NOT NULL SRID 0,
			//    `컬럼명6` TEXT NULL, 
			//    `컬럼명7` BIGINT UNSIGNED NOT NULL,                                                       
			//    `컬럼명8` INT UNSIGNED NOT NULL,                                                       
			//    `컬럼명9` TINYINT UNSIGNED NOT NULL [CONSTRAINT `제약조건명`] CHECK (`컬럼명9` >= 1 AND `컬럼명9` <= 10),   -- 선언할 때 같이 추가(체크제약조건)
			//    `컬럼명10` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,                                                     -- DEFAULT 값 설정
			//
			//    PRIMARY KEY [`제약조건명`] (`컬럼명1`),                                                          -- 뒤에 따로 추가(기본키, 제약조건명 시스템이 자동 생성)
			//    UNIQUE KEY [`제약조건명`] (`컬럼명3`),                                                           -- 뒤에 따로 추가(유니크, 제약조건명 설정 가능)
			//    KEY [`제약조건명`] (`컬럼명7`),                                                                  -- 뒤에 추가(인덱스, 제약조건명 설정 가능)
			//    KEY [`제약조건명`] (`컬럼명8`),                                                                  -- 뒤에 추가(인덱스, 제약조건명 설정 가능)
			//    SPATIAL KEY [`제약조건명`] (`컬럼명5`),                                                          -- 뒤에 추가(스파셜인덱스, 제약조건명 설정 가능)
			//    FULLTEXT KEY [`제약조건명`] (`컬럼명6`),                                                         -- 뒤에 추가(풀텍스트인덱스, 제약조건명 설정 가능)
			//    [CONSTRAINT `제약조건명`] CHECK (`컬럼명9` >= 1 AND `컬럼명9` <= 10),                            -- 뒤에 따로 추가(체크제약조건, 제약조건명 설정 가능)
			//    [CONSTRAINT `제약조건명`] FOREIGN KEY (`컬럼명7`) REFERENCES `테이블명` (`컬럼명1`),             -- 뒤에 추가(자기 자신 외래키 참조, 제약조건명 설정 가능)
			//    [CONSTRAINT `제약조건명`] FOREIGN KEY (`컬럼명8`) REFERENCES `부모테이블명` (`PK컬럼명`)         -- 뒤에 추가(다른테이블 외래키 참조, 제약조건명 설정 가능)
			// ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='코멘트';
			// 
			//  - PRIMARY KEY, UNIQUE, CHECK 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 모든 컬럼 정의 마지막에 따로 추가할 수도 있음
			//  - PRIMARY KEY, UNIQUE는 대상 컬럼 선언하는 곳에서는 제약조건명을 명시하지 않고 추가되며, 모든 컬럼 정의 마지막에 따로 추가할 경우 제약조건명을 명시하지 않고 추가할 수도 있고, 해당 제약조건명으로 설정할 수도 있음
			//  - CHECK는 대상 컬럼 선언하는 곳에 같이 추가하거나 또는 모든 컬럼 정의 마지막에 따로 추가할 경우 제약조건명을 명시하지 않고 추가할 수도 있고, 해당 제약조건명으로 설정할 수도 있음
			//  - INDEX, FOREIGN KEY 선언 위치는 무조건 모든 컬럼 정의 마지막에 따로 추가해야 함
			//  - DEFAULT 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 테이블 수정(ALTER TABLE)으로 설정할 수도 있음
			//  - 제약조건명을 명시하지 않고 추가하려면 [`제약조건명`], [CONSTRAINT `제약조건명`] 이 부분을 생략하면 가능
			//  - PRIMARY KEY는 제약조건명을 명시하든 안하든 시스템이 제약조건명을 PRIMARY로 자동 할당
			//  - 시스템이 제약조건명을 자동 할당했는지 아니면 생성시 설정한 제약조건명으로 설정했는지 확인 
			//    : MYSQL은 확인 방법이 존재하지 않음
			query = tstring_tcformat(_T("CREATE TABLE `%s` (%s) COMMENT = '%s'"), _tableName.c_str(), columnsStr.c_str(), _tableComment.c_str());
			break;
		case EDBClass::ORACLE:
			// CREATE TABLE 테이블명 (
			//    컬럼명1 NUMBER GENERATED [BY DEFAULT|DEFAULT ON NULL|ALWAYS] AS IDENTITY [CONSTRAINT 제약조건명] PRIMARY KEY,    -- 선언할 때 같이 추가(기본키, 제약조건명 설정 가능)
			//    컬럼명2 VARCHAR2(32) [CONSTRAINT 제약조건명] NOT NULL,                                                           -- 선언할 때 같이 추가(NOT NULL, 제약조건명 설정 가능)
			//    컬럼명3 VARCHAR2(32) [CONSTRAINT 제약조건명] UNIQUE NOT NULL,                                                     -- 선언할 때 같이 추가(유니크, 제약조건명 설정 가능) 
			//    컬럼명4 NUMBER DEFAULT 0 NOT NULL,                                                        -- DEFAULT 값 설정
			//    컬럼명5 NUMBER NOT NULL,                                                        
			//    컬럼명6 NUMBER NOT NULL,                                                        
			//    컬럼명7 NUMBER NOT NULL [CONSTRAINT 제약조건명] CHECK (컬럼명7 >= 1 AND 컬럼명7 <= 10),     -- 선언할 때 같이 추가(체크제약조건, 제약조건명 설정 가능)
			//    컬럼명8 LONG NULL,
			//    컬럼명9 TIMESTAMP(6) DEFAULT SYSDATE NOT NULL ,                                           -- DEFAULT 값 설정
			//    
			//    [CONSTRAINT 제약조건명] PRIMARY KEY (컬럼명1),                                           -- 뒤에 따로 추가(기본키, 제약조건명 설정 가능)
			//    [CONSTRAINT 제약조건명] UNIQUE (컬럼명3),                                                -- 뒤에 따로 추가(유니크, 제약조건명 설정 가능)
			//    [CONSTRAINT 제약조건명] CHECK (컬럼명7 >= 1 AND 컬럼명7 <= 10),                          -- 뒤에 따로 추가(체크제약조건, 제약조건명 설정 가능)   
			//    [CONSTRAINT 제약조건명] FOREIGN KEY (컬럼명5) REFERENCES 테이블명 (컬럼명1),              -- 뒤에 추가(자기 자신 외래키 참조, 제약조건명 설정 가능)
			//    [CONSTRAINT 제약조건명] FOREIGN KEY (컬럼명6) REFERENCES 부모테이블명 (PK컬럼명)           -- 뒤에 추가(다른테이블 외래키 참조, 제약조건명 설정 가능)
			// )
			//
			//  - PRIMARY KEY, UNIQUE, CHECK 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 모든 컬럼 정의 마지막에 따로 추가할 수도 있음
			//  - FOREIGN KEY 선언 위치는 무조건 모든 컬럼 정의 마지막에 따로 추가해야 함
			//  - NOT NULL 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가해야 하고 제약조건명을 명시하지 않고 추가할 수도 있고, 해당 제약조건명으로 설정할 수도 있음
			//  - DEFAULT 선언 위치는 대상 컬럼 선언하는 곳에 같이 추가할 수도 있고, 테이블 수정(ALTER TABLE)으로 설정할 수도 있음
			//  - INDEX는 테이블 생성시 추가할 수 없고, 인덱스 생성(CREATE INDEX)으로 설정할 수 있음
			//  - 제약조건명을 명시하지 않고 추가하려면 [CONSTRAINT 제약조건명] 이 부분을 생략하면 가능
			//  - 시스템이 제약조건명을 자동 할당했는지 아니면 생성시 설정한 제약조건명으로 설정했는지 확인 
			//    : SYS.USER_INDEXES 테이블에 GENERATED 컬럼(Y이면 시스템 자동 할당, N이면 설정한 제약조건명) 참조
			query = tstring_tcformat(_T("CREATE TABLE %s (%s)"), _tableName.c_str(), columnsStr.c_str());
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Table::AlterTableCollationEngine()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// MSSQL에서 TABLE 자체에 Collation은 없지만, TABLE 스키마에 기술된 Column들마다 Collation을 가지므로, Column별로 Collation을 변경해야 한다. 
			break;
		case EDBClass::MYSQL:
			// 테이블 캐릭터셋, 데이터정렬(문자비교규칙), 스토리지엔진 변경
			// ALTER TABLE `테이블명` CHARACTER SET = 캐릭터셋, COLLATE = 데이터정렬, ENGINE = 스토리지엔진;
			query = query + "ALTER TABLE `" + _tableName + "`";

			if( _characterset != "" )
				query = query + " CHARACTER SET = " + _characterset + ",";

			if( _collation != "" )
				query = query + " COLLATE = " + _collation + ",";

			if( _storageEngine != "" )
				query = query + " ENGINE = " + _storageEngine;
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Table::DropTable()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// DROP TABLE [테이블명]
			query = tstring_tcformat(_T("DROP TABLE [%s]"), _tableName.c_str());
			break;
		case EDBClass::MYSQL:
			// DROP TABLE `테이블명`
			query = tstring_tcformat(_T("DROP TABLE `%s`"), _tableName.c_str());
			break;
		case EDBClass::ORACLE:
			// DROP TABLE 테이블명
			query = tstring_tcformat(_T("DROP TABLE %s"), _tableName.c_str());
	}

	return query;
}

//***************************************************************************
//
_tstring Table::ChangeTableName(const _tstring chgTableName)
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// EXEC sp_rename [스키마명.기존테이블명], [변경할테이블명]
			query = tstring_tcformat(_T("EXEC sp_rename '[%s].[%s]', '[%s]'"), _schemaName.c_str(), _tableName.c_str(), chgTableName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `기존테이블명` RENAME `변경할테이블명`;
			query = tstring_tcformat(_T("ALTER TABLE `%s` RENAME `%s`;"), _tableName.c_str(), chgTableName.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 기존테이블명 RENAME TO 변경할테이블명
			query = tstring_tcformat(_T("ALTER TABLE %s RENAME TO %s"), _tableName.c_str(), chgTableName.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
ColumnRef Table::FindColumn(const _tstring& columnName)
{
	auto findIt = std::find_if(_columns.begin(), _columns.end(),
							   [&](const ColumnRef& column) { return column->_tableName == columnName; });

	if( findIt != _columns.end() )
		return *findIt;

	return nullptr;
}

//***************************************************************************
//
_tstring Column::CreateColumn()
{
	_tstring query = _T("");

	switch( _dbSystemInfo.DBClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] 데이터타입 <컬럼속성>
			//
			// <컬럼속성> : COLLATE, {NULL|NOT NULL}, IDENTITY(시드값, 증분값) 설정이 포함됨
			//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
			//  - 만약 데이터정렬 값이 해당 데이터베이스에 설정된 데이터정렬 값과 동일한 경우 따로 명시하지 않아도 됨
			//  - COLLATE 데이터정렬 {NULL|NOT NULL} IDENTITY(시드값, 증분값)
			//
			// ex)
			//  - IDENTITY 설정     : 기존 컬럼을 IDENTITY열로 수정하는 것은 안됨(테이블을 삭제하고, 다시 만들어야 함)
			//	- NULL 허용         : ALTER TABLE T1 ADD Col2 NVARCHAR(20) NULL
			//  - NULL 비허용       : ALTER TABLE T1 ADD Col3 CHAR(10) NOT NULL
			//  - DEFAULT 설정      : ALTER TABLE T1 ADD Col4 DECIMAL(38,0) NOT NULL DEFAULT 0
			//                       신규로 컬럼을 추가할 때 DEFAULT 값을 함께 설정할 수 있지만, 
			//                       이 방법으로 추가된 DEFAULT 제약조건명은 시스템에서 임의로 생성하므로 관리가 되지 않음
			//                       신규 컬럼을 추가한 후에 DEFAULT 제약조건명을 직접 지정하여 추가하는 것이 관리 차원에서 좋음
			//                       ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] DEFAULT 0 FOR [컬럼명]
			//  - 데이터정렬 설정    : ALTER TABLE T1 ADD Col5 NVARCHAR(20) COLLATE Korean_Wansung_CI_AS NULL
			query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] ADD %s"), _schemaName.c_str(), _tableName.c_str(), CreateColumnText().c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` ADD `컬럼명` 데이터타입 <컬럼속성>;
			//
			// <컬럼속성> : CHARACTER SET, COLLATE, {NULL|NOT NULL}, DEFAULT, AUTO_INCREMENT, COMMENT 설정이 포함됨
			//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
			//  - 만약 캐릭터셋, 데이터정렬 값이 해당 데이터베이스에 설정된 캐릭터셋, 데이터정렬 값과 동일한 경우 따로 명시하지 않아도 됨
			//  - CHARACTER SET '캐릭터셋' COLLATE '데이터정렬' {NULL|NOT NULL} DEFAULT 값 AUTO_INCREMENT COMMENT '코멘트'
			//
			// ex)
			//  - AUTO_INCREMENT 설정       : ALTER TABLE `T1` ADD COLUMN `Col1` INT UNSIGNED NOT NULL AUTO_INCREMENT;
			//  - NULL 허용                 : ALTER TABLE `T1` ADD COLUMN `Col2` VARCHAR(200) NULL;
			//  - NULL 비허용               : ALTER TABLE `T1` ADD COLUMN `Col3` CHAR(10) NOT NULL;
			//  - DEFAULT 설정              : ALTER TABLE `T1` ADD COLUMN `Col4` DECIMAL(38,0) NOT NULL DEFAULT 0;
			//  - 캐릭터셋, 데이터정렬 설정   : ALTER TABLE `T1` ADD COLUMN `Col5` VARCHAR(200) CHARACTER SET 'utf8mb4' COLLATE 'utf8mb4_0900_ai_ci' NULL;
			query = tstring_tcformat(_T("ALTER TABLE `%s` ADD %s;"), _tableName.c_str(), CreateColumnText().c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 ADD (컬럼명 데이터타입 <컬럼속성>)
			//
			// <컬럼속성> : DEFAULT, {NULL|NOT NULL} 설정이 포함됨
			//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
			//  - DEFAULT 값 {NULL|NOT NULL}
			//
			// ex)
			//  - IDENTITY 설정     : 시퀀스와 트리거를 이용한 방법, MAX + 1 방법, IDENTITY 컬럼
			//	- NULL 허용         : ALTER TABLE T1 ADD (Col2 VARCHAR2(20) NULL)
			//  - NULL 비허용       : ALTER TABLE T1 ADD (Col3 CHAR(10) NOT NULL)
			//  - DEFAULT 설정      : ALTER TABLE T1 ADD (Col4 NUMBER(16,0) DEFAULT 0 NOT NULL)
			query = tstring_tcformat(_T("ALTER TABLE %s ADD (%s)"), _tableName.c_str(), CreateColumnText().c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Column::AlterColumn()
{
	_tstring query = _T("");

	switch( _dbSystemInfo.DBClass )
	{
		case EDBClass::MSSQL:
			// 컬럼명 변경                 : EXEC sp_rename [테이블명.기존컬럼명], [변경할컬럼명], 'COLUMN'
			// 컬럼 데이터타입 변경         : ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] 변경할데이터타입 <컬럼속성>
			// 컬럼 데이터정렬 변경         : ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] 데이터타입 COLLATE 데이터정렬 <컬럼속성>
			// 컬럼 DEFAULT 값 변경        : 기존 DEFAULT 값을 변경하고자 한다면, 기존 DEFAULT 제약 조건을 삭제하고, 새로운 제약 조건을 추가
			//                               - ALTER TABLE [스키마명].[테이블명] DROP CONSTRAINT [제약조건명]
			//                               - ALTER TABLE [스키마명].[테이블명] ADD [CONSTRAINT [제약조건명]] DEFAULT 값 FOR [컬럼명]
			query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] ALTER COLUMN %s"), _schemaName.c_str(), _tableName.c_str(), CreateColumnText().c_str());
			break;
		case EDBClass::MYSQL:
			// 컬럼명 변경                  : ALTER TABLE `테이블명` CHANGE [COLUMN] `기존컬럼명` `변경할컬럼명` 기존데이터타입 <컬럼속성>;
			// 컬럼명, 데이터타입 변경       : ALTER TABLE `테이블명` CHANGE [COLUMN] `기존컬럼명` `변경할컬럼명` 변경할데이터타입 <컬럼속성>;
			// 컬럼 데이터타입 변경          : ALTER TABLE `테이블명` MODIFY [COLUMN] `컬럼명` 변경할데이터타입 <컬럼속성>;
			// 컬럼 캐릭터셋, 데이터정렬 변경 : ALTER TABLE `테이블명` MODIFY [COLUMN] `컬럼명` 데이터타입 CHARACTER SET '캐릭터셋' COLLATE '데이터정렬' <컬럼속성> COMMENT 코멘트;
			// 코멘트 변경                   : ALTER TABLE `테이블명` MODIFY [COLUMN] `컬럼명` 데이터타입 <컬럼속성> COMMENT '값';
			// 컬럼 순서 변경                : ALTER TABLE `테이블명` MODIFY [COLUMN] `컬럼명` 데이터타입 <컬럼속성> AFTER `다른컬럼`;
			// 컬럼 DEFAULT 값 변경          : 
			//                                  - ALTER TABLE `테이블명` MODIFY [COLUMN] `컬럼명` 데이터타입 {NULL|NOT NULL} DEFAULT 값;
			//                                  - ALTER TABLE `테이블명` ALTER COLUMN `컬럼명` SET DEFAULT 수정할값;
			query = tstring_tcformat(_T("ALTER TABLE `%s` MODIFY %s;"), _tableName.c_str(), CreateColumnText().c_str());
			break;
		case EDBClass::ORACLE:
			// 컬럼명 변경                 : ALTER TABLE 테이블명 RENAME COLUMN 기존컬럼명 TO 변경할컬럼명
			// 컬럼 데이터타입 변경         : ALTER TABLE 테이블명 MODIFY (컬럼명 변경할데이터타입 <컬럼속성>)
			// 컬럼 DEFAULT 값 변경        : 
			//                              - ALTER TABLE 테이블명 MODIFY (컬럼명 데이터타입 DEFAULT 값 {NULL|NOT NULL})
			//                              - ALTER TABLE 테이블명 MODIFY 컬럼명 DEFAULT 값
			query = tstring_tcformat(_T("ALTER TABLE %s MODIFY (%s)"), _tableName.c_str(), CreateColumnText().c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Column::DropColumn()
{
	_tstring query = _T("");

	switch( _dbSystemInfo.DBClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] DROP COLUMN [%s]"), _schemaName.c_str(), _tableName.c_str(), _columnName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` DROP `컬럼명`;
			query = tstring_tcformat(_T("ALTER TABLE `%s` DROP COLUMN `%s`;"), _tableName.c_str(), _columnName.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 DROP 컬럼명
			query = tstring_tcformat(_T("ALTER TABLE %s DROP COLUMN %s"), _tableName.c_str(), _columnName.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Column::ChangeColumnName(const _tstring chgColumnName)
{
	_tstring query = _T("");
	_tstring columnOption = _T("");

	switch( _dbSystemInfo.DBClass )
	{
		case EDBClass::MSSQL:
			// EXEC sp_rename [스키마명.기존테이블명], [변경할컬럼명], 'COLUMN'
			query = tstring_tcformat(_T("EXEC sp_rename '[%s].[%s].[%s]', '[%s]', 'COLUMN'"), _schemaName.c_str(), _tableName.c_str(), _columnName.c_str(), chgColumnName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` CHANGE [COLUMN] `기존컬럼명` `변경할컬럼명` 기존데이터타입 <컬럼속성>;
			if( ::_tcsicmp(_dbSystemInfo.tszCharacterSet, _characterset.c_str()) == 0 ) _characterset = _T("");
			if( ::_tcsicmp(_dbSystemInfo.tszCollation, _collation.c_str()) == 0 ) _collation = _T("");

			columnOption = MYSQLGetTableColumnOption(_datatypedesc, _nullable, _defaultDefinition, _identity, _characterset, _collation, _columnComment);
			query = tstring_tcformat(_T("ALTER TABLE `%s` CHANGE [COLUMN] `%s` `%s` %s;"), _tableName.c_str(), _columnName.c_str(), chgColumnName.c_str(), columnOption.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 RENAME COLUMN 기존컬럼명 TO 변경할컬럼명
			query = tstring_tcformat(_T("ALTER TABLE %s RENAME COLUMN %s TO %s"), _tableName.c_str(), _columnName.c_str(), chgColumnName.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Column::CreateColumnText()
{
	_tstring query = _T("");
	_tstring columnOption = _T("");

	if( ::_tcsicmp(_dbSystemInfo.tszCharacterSet, _characterset.c_str()) == 0 ) _characterset = _T("");
	if( ::_tcsicmp(_dbSystemInfo.tszCollation, _collation.c_str()) == 0 ) _collation = _T("");

	switch( _dbSystemInfo.DBClass )
	{
		case EDBClass::MSSQL:
			columnOption = MSSQLGetTableColumnOption(_datatypedesc, _nullable, _identity, (ulong)_seedValue, (ulong)_incrementValue, _collation);
			query = tstring_tcformat(_T("[%s] %s"), _columnName.c_str(), columnOption.c_str());
			break;
		case EDBClass::MYSQL:
			columnOption = MYSQLGetTableColumnOption(_datatypedesc, _nullable, _defaultDefinition, _identity, _characterset, _collation, _columnComment);
			query = tstring_tcformat(_T("`%s` %s"), _columnName.c_str(), columnOption.c_str());
			break;
		case EDBClass::ORACLE:
			columnOption = ORACLEGetTableColumnOption(_datatypedesc, _nullable, _defaultDefinition, _identity, _pORACLEDBTableIdentityCols);
			query = tstring_tcformat(_T("%s %s"), _columnName.c_str(), columnOption.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring IndexColumn::GetSortText()
{
	return ToString(_sort);
}

//***************************************************************************
//
_tstring Index::GetIndexName()
{
	_tstring uniqueName = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// [인덱스명]
			//	- [PK|UQ|IX]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			uniqueName = _T("IX_");
			if( _primaryKey ) uniqueName = _T("PK_");
			else
			{
				if( _uniqueKey ) uniqueName = _T("UQ_");
			}
			uniqueName = uniqueName + (_kind == EIndexKind::CLUSTERED ? _T("C_") : _T("NC_"));
			uniqueName = uniqueName + _tableName;
			for( const IndexColumnRef& column : _columns )
			{
				uniqueName += _T("_");
				uniqueName += column->_columnName;
			}
			break;
		case EDBClass::MYSQL:
			// [인덱스명]
			//	- PRIMARY : PRIMARY
			//	- 인덱스 : [UQ|IX|FT|ST]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			if( _primaryKey ) uniqueName = _T("PRIMARY");
			else
			{
				EMYSQLIndexType indexType = StringToMYSQLIndexType(_type.c_str());
				uniqueName = _T("IX_");
				if( indexType == EMYSQLIndexType::UNIQUE )
					uniqueName = _T("UQ_");
				else if( indexType == EMYSQLIndexType::INDEX )
					uniqueName = _T("IX_");
				else if( indexType == EMYSQLIndexType::FULLTEXT )
					uniqueName = _T("FT_");
				else if( indexType == EMYSQLIndexType::SPATIAL )
					uniqueName = _T("ST_");

				uniqueName = uniqueName + (_kind == EIndexKind::CLUSTERED ? _T("C_") : _T("NC_"));
				uniqueName = uniqueName + _tableName;
				for( const IndexColumnRef& column : _columns )
				{
					uniqueName += _T("_");
					uniqueName += column->_columnName;
				}
			}
			break;
		case EDBClass::ORACLE:
			// [인덱스명]
			//	- [PK|UQ|IX]_[테이블명]_[컬럼명1]_[컬럼명2]...
			uniqueName = _T("IX_");
			if( _primaryKey ) uniqueName = _T("PK_");
			else
			{
				if( _uniqueKey ) uniqueName = _T("UQ_");
			}
			uniqueName = uniqueName + _tableName;
			for( const IndexColumnRef& column : _columns )
			{
				uniqueName += _T("_");
				uniqueName += column->_columnName;
			}
			break;
	}
	return uniqueName;
}

//***************************************************************************
//
_tstring Index::CreateIndex()
{
	_tstring query = _T("");
	EMSSQLIndexType indexType = StringToMSSQLIndexType(_type.c_str());

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] PRIMARY KEY [CLUSTERED|NONCLUSTERED] ([컬럼명] [정렬방식])
				_tstring kindText = indexType == EMSSQLIndexType::CLUSTERED ? _T("CLUSTERED ") : _T("");

				query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] ADD CONSTRAINT [%s] PRIMARY KEY %s(%s)"), _schemaName.c_str(), _tableName.c_str(), _indexName.c_str(), kindText.c_str(), CreateColumnsText().c_str());
			}
			else
			{
				if( _uniqueKey )
				{
					// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] UNIQUE [CLUSTERED|NONCLUSTERED] ([컬럼명] [정렬방식])
					_tstring kindText = indexType == EMSSQLIndexType::CLUSTERED ? _T("CLUSTERED ") : _T("");

					query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] ADD CONSTRAINT [%s] UNIQUE %s(%s)"), _schemaName.c_str(), _tableName.c_str(), _indexName.c_str(), kindText.c_str(), CreateColumnsText().c_str());
				}
				else
				{
					if( indexType == EMSSQLIndexType::CLUSTERED__COLUMNSTORE )
					{
						// CREATE CLUSTERED COLUMNSTORE INDEX [인덱스명] ON [테이블명]
						query = tstring_tcformat(_T("CREATE CLUSTERED COLUMNSTORE INDEX [%s] ON [%s].[%s]"), _indexName.c_str(), _schemaName.c_str(), _tableName.c_str());
					}
					else if( indexType == EMSSQLIndexType::NONCLUSTERED__COLUMNSTORE )
					{
						// CREATE NONCLUSTERED COLUMNSTORE INDEX [인덱스명] ON [테이블명] ([컬럼명])
						query = tstring_tcformat(_T("CREATE %s INDEX [%s] ON [%s].[%s] (%s)"), _type.c_str(), _indexName.c_str(), _schemaName.c_str(), _tableName.c_str(), CreateColumnsText(false).c_str());
					}
					else
					{
						// CREATE [CLUSTERED|NONCLUSTERED] INDEX [인덱스명] ON [테이블명] ([컬럼명] [정렬방식])
						query = tstring_tcformat(_T("CREATE %s INDEX [%s] ON [%s].[%s] (%s)"), _type.c_str(), _indexName.c_str(), _schemaName.c_str(), _tableName.c_str(), CreateColumnsText().c_str());
					}
				}
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE `테이블명` ADD PRIMARY KEY (`컬럼명`);
				query = tstring_tcformat(_T("ALTER TABLE `%s` ADD PRIMARY KEY (%s);"), _tableName.c_str(), CreateColumnsText().c_str());
			}
			else
			{
				// ALTER TABLE `테이블명` ADD [UNIQUE|FULLTEXT|SPATIAL] INDEX `인덱스명` (`컬럼명1` 정렬방식);
				// CREATE [UNIQUE|FULLTEXT|SPATIAL] INDEX `인덱스명` ON `테이블명` (`컬럼명` 정렬방식);
				_tstring typeText = _type;
				typeText = typeText != _T("") ? typeText + _T(" ") : _T("");

				query = tstring_tcformat(_T("CREATE %sINDEX `%s` ON `%s` (%s);"), typeText.c_str(), _indexName.c_str(), _tableName.c_str(), CreateColumnsText().c_str());
			}
			break;
		case EDBClass::ORACLE:
			if( _primaryKey )
			{
				// ALTER TABLE 테이블명 ADD CONSTRAINT PRIMARY KEY (컬럼명 정렬방식)
				query = tstring_tcformat(_T("ALTER TABLE %s ADD CONSTRAINT PRIMARY KEY (%s)"), _tableName.c_str(), CreateColumnsText().c_str());
			}
			else
			{
				if( _uniqueKey )
				{
					// CREATE UNIQUE INDEX 인덱스명 ON 테이블명 (컬럼명 정렬방식)
					query = tstring_tcformat(_T("CREATE UNIQUE INDEX %s ON %s (%s)"), _indexName.c_str(), _tableName.c_str(), CreateColumnsText().c_str());
				}
				else
				{
					// CREATE [BITMAP] INDEX 인덱스명 ON 테이블명 (컬럼명 정렬방식)
					_tstring typeText = StringToORACLEIndexType(_type.c_str()) == EORACLEIndexType::BITMAP ? _T("BITMAP ") : _T("");
					query = tstring_tcformat(_T("CREATE %sINDEX %s ON %s (%s)"), typeText.c_str(), _indexName.c_str(), _tableName.c_str(), CreateColumnsText().c_str());
				}
			}
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Index::DropIndex()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
				query = MSSQLGetDropConstraintQuery(_schemaName, _tableName, _T("PK"), _indexName);
			}
			else if( _uniqueKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
				query = MSSQLGetDropConstraintQuery(_schemaName, _tableName, _T("UQ"), _indexName);
			}
			else
			{
				// DROP INDEX [인덱스명] ON [테이블명]
				query = tstring_tcformat(_T("DROP INDEX [%s] ON [%s].[%s]"), _indexName.c_str(), _schemaName.c_str(), _tableName.c_str());
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE `테이블명` DROP PRIMARY KEY;
				query = MYSQLGetDropConstraintQuery(_tableName, _T("PRIMARY KEY"), _indexName);
			}
			else if( _uniqueKey )
			{
				// ALTER TABLE `테이블명` DROP INDEX `인덱스명`;
				query = MYSQLGetDropConstraintQuery(_tableName, _T("UNIQUE"), _indexName);
			}
			else
			{
				// ALTER TABLE `테이블명` DROP INDEX `인덱스명`;
				query = tstring_tcformat(_T("ALTER TABLE `%s` DROP INDEX `%s`;"), _tableName.c_str(), _indexName.c_str());
			}
			break;
		case EDBClass::ORACLE:
			//  PRIMARY KEY 또는 UNIQUE 제약 조건을 생성할 때 인덱스와 제약 조건을 동시에 생성하면 삭제할 때도 동시에 삭제가 되고,
			//  이미 생성된 인덱스를 사용해서 PRIMARY KEY 또는 UNIQUE 제약 조건을 생성하면 삭제시 제약 조건만 삭제가 되고 인덱스는 남아 있게 됩니다.
			//  항상 인덱스와 제약 조건을 한번에 삭제하고 싶을 때 
			//      - PRIMARY KEY : ALTER TABLE 테이블명 DROP PRIMARY KEY DROP INDEX;
			//      - UNIQUE : ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명 DROP INDEX;
			//  항상 제약 조건만 삭제하고 인덱스는 남겨 놓고 싶을 때
			//      - PRIMARY KEY : ALTER TABLE 테이블명 DROP PRIMARY KEY KEEP INDEX;
			//      - UNIQUE : ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명 KEEP INDEX;
			if( _primaryKey )
			{
				// ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명
				query = ORACLEGetDropConstraintQuery(_tableName, _T("P"), _indexName);
			}
			else if( _uniqueKey )
			{
				// ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명
				query = ORACLEGetDropConstraintQuery(_tableName, _T("U"), _indexName);
			}
			else
			{
				// DROP INDEX 인덱스명
				query = tstring_tcformat(_T("DROP INDEX %s"), _indexName.c_str());
			}
			break;
	}

	return query;
}

//***************************************************************************
//
EIndexKind Index::GetStringToIndexKind()
{
	EIndexKind indexKind = EIndexKind::NONE;

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			if( StringToMSSQLIndexType(_type.c_str()) == EMSSQLIndexType::CLUSTERED 
				|| StringToMSSQLIndexType(_type.c_str()) == EMSSQLIndexType::CLUSTERED__COLUMNSTORE )
				indexKind = EIndexKind::CLUSTERED;
			else indexKind = EIndexKind::NONCLUSTERED;

			break;
		case EDBClass::MYSQL:
			break;
		case EDBClass::ORACLE:
			break;
	}

	return indexKind;
}
			
//***************************************************************************
//
_tstring Index::GetKeyText()
{
	return (_primaryKey ? _T("PRIMARY KEY") : (_uniqueKey ? _T("UNIQUE") : _T("")));
}

//***************************************************************************
//
_tstring Index::ChangeIndexName(_tstring chgIndexName)
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// EXEC sp_rename [스키마명.테이블명.기존인덱스명], [변경할인덱스명], 'INDEX'
			query = tstring_tcformat(_T("EXEC sp_rename '[%s].[%s].[%s]', '[%s]', 'INDEX'"), _schemaName.c_str(), _tableName.c_str(), _indexName.c_str(), chgIndexName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` RENAME INDEX `기존인덱스명` TO `변경할인덱스명`;
			query = tstring_tcformat(_T("ALTER TABLE `%s` RENAME INDEX `%s` TO `%s`;"), _tableName.c_str(), _indexName.c_str(), chgIndexName.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER INDEX 기존인덱스명 RENAME TO 변경할인덱스명
			query = tstring_tcformat(_T("ALTER INDEX %s RENAME TO %s"), _tableName.c_str(), chgIndexName.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Index::CreateColumnsText(bool isOrderBy)
{
	_tstring query = _T("");

	const int32 size = static_cast<int32>(_columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			query += _T(", ");

		switch( _dbClass )
		{
			case EDBClass::MSSQL:
				// [컬럼명] 정렬방식
				query += tstring_tcformat(_T("[%s] %s"), _columns[i]->_columnName.c_str(), isOrderBy ? (_columns[i]->_sort == EIndexSort::DESC ? _T(" DESC") : _T(" ASC")) : _T(""));
				break;
			case EDBClass::MYSQL:
				// `컬럼명` 정렬방식
				query += tstring_tcformat(_T("`%s` %s"), _columns[i]->_columnName.c_str(), isOrderBy ? (_columns[i]->_sort == EIndexSort::DESC ? _T(" DESC") : _T("")) : _T(""));
				break;
			case EDBClass::ORACLE:
				// 컬럼명 정렬방식
				query += tstring_tcformat(_T("%s %s"), _columns[i]->_columnName.c_str(), isOrderBy ? (_columns[i]->_sort == EIndexSort::DESC ? _T(" DESC") : _T(" ASC")) : _T(""));
				break;
		}
	}

	return query;
}

//***************************************************************************
//
bool Index::DependsOn(const _tstring& columnName)
{
	auto findIt = std::find_if(_columns.begin(), _columns.end(),
							   [&](const IndexColumnRef& column) { return column->_columnName == columnName; });

	return findIt != _columns.end();
}

//***************************************************************************
//
_tstring ForeignKey::CreateForeignKey()
{
	_tstring query = _T("");

	// [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT]
	//	- RESTRICT : 참조하는 테이블에 데이터가 남아 있으면, 참조되는 테이블의 데이터를 삭제하거나 수정할 수 없습니다.
	//	- CASCADE : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블에서도 삭제와 수정이 같이 이루어집니다.
	//	- SET NULL : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 NULL로 변경됩니다.
	//	- NO ACTION : 참조되는 테이블에서 데이터를 삭제하거나 수정해도, 참조하는 테이블의 데이터는 변경되지 않습니다.
	//	- SET DEFAULT : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 기본값으로 변경됩니다.
	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) <ON DELETE NO ACTION|CASCADE|SET NULL|SET DEFAULT>|<ON UPDATE NO ACTION|CASCADE|SET NULL|SET DEFAULT>
			query = tstring_tcformat(_T("ALTER TABLE [%s] ADD CONSTRAINT [%s] FOREIGN KEY (%s) REFERENCES [%s] (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(_foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(_referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + tstring_tcformat(_T("\r\nON UPDATE %s\r\nON DELETE %s"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + tstring_tcformat(_T("\r\nON UPDATE %s"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + tstring_tcformat(_T("\r\nON DELETE %s"), _deleteRule.c_str());
			}
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` ADD CONSTRAINT `제약조건명` FOREIGN KEY (`컬럼명`) REFERENCES `테이블명` (`컬럼명`) <ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT>|<ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT>;
			query = tstring_tcformat(_T("ALTER TABLE `%s` ADD CONSTRAINT `%s` FOREIGN KEY (%s) REFERENCES `%s` (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(_foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(_referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + tstring_tcformat(_T(" ON UPDATE %s ON DELETE %s;"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + tstring_tcformat(_T(" ON UPDATE %s;"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + tstring_tcformat(_T(" ON DELETE %s;"), _deleteRule.c_str());
			}
			else
			{
				query = query + _T(";");
			}
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 ADD CONSTRAINT 제약조건명 FOREIGN KEY (컬럼명) REFERENCES 테이블명 (컬럼명) <ON DELETE CASCADE|SET NULL|NO ACTION>;
			query = tstring_tcformat(_T("ALTER TABLE %s\r\nADD CONSTRAINT %s\r\n\tFOREIGN KEY (%s)\r\n\tREFERENCES %s (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
				CreateColumnsText(_foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(_referenceKeyColumns).c_str());

			if( _deleteRule.size() > 0 )
			{
				query = query + tstring_tcformat(_T("\r\n\tON DELETE {0}"), _deleteRule.c_str());
			}
			break;
	}
	return query;
}

//***************************************************************************
// 외래 키 제약 조건을 수정할 경우 삭제하고, 재생성
_tstring ForeignKey::AlterForeignKey()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// 1. 외래 키 제약 조건을 삭제 : ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			// 2. 외래 키 제약 조건을 생성 : ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) <ON DELETE NO ACTION|CASCADE|SET NULL|SET DEFAULT>|<ON UPDATE NO ACTION|CASCADE|SET NULL|SET DEFAULT>
			query = DropForeignKey();
			query = query + ";" + CreateForeignKey();
			break;
		case EDBClass::MYSQL:
			// 1. 외래 키 제약 조건을 삭제 : ALTER TABLE `테이블명` DROP CONSTRAINT `제약조건명`;
			// 2. 외래 키 제약 조건을 생성 : ALTER TABLE `테이블명` ADD CONSTRAINT `제약조건명` FOREIGN KEY (`컬럼명`) REFERENCES `테이블명` (`컬럼명`) <ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT>|<ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET DEFAULT>;
			query = DropForeignKey();
			query = query + CreateForeignKey();
			break;
		case EDBClass::ORACLE:
			// 1. 외래 키 제약 조건을 삭제 : ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명;
			// 2. 외래 키 제약 조건을 생성 : ALTER TABLE 테이블명 ADD CONSTRAINT 제약조건명 FOREIGN KEY (컬럼명) REFERENCES 테이블명 (컬럼명) <ON DELETE CASCADE|SET NULL|NO ACTION>;
			query = DropForeignKey();
			query = query + CreateForeignKey();
			break;
	}

	return query;
}

//***************************************************************************
// INSERT 및 UPDATE 문에서 외래 키 체크 제약 조건 사용 여부 설정
_tstring ForeignKey::AlterForeignKeyCheckConstraint()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] NOCHECK|CHECK CONSTRAINT [제약조건명]
			//  - NOCHECK : INSERT 및 UPDATE 문에 대한 FOREIGN KEY 제약 조건을 사용하지 않게 설정  
			//  - CHECK : INSERT 및 UPDATE 문에 대한 FOREIGN KEY 제약 조건을 다시 사용하도록 설정
			// ALTER TABLE [테이블명] WITH CHECK CHECK CONSTRAINT [제약조건명]
			//  - 테이블의 기존 데이터가 외래 키 제약 조건을 준수하는 경우 외래 키 제약 조건을 신뢰할 수 있는 것으로 설정(sys.foreign_keys 테이블에서 제약조건명에 해당하는 정보에서 is_not_trusted 컬럼을 0으로 변경) 
			break;
		case EDBClass::MYSQL:
		case EDBClass::ORACLE:
			// 해당 기능 존재하지 않음
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring ForeignKey::DropForeignKey()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = MSSQLGetDropConstraintQuery(_schemaName, _tableName, _T("F"), _foreignKeyName);
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` DROP CONSTRAINT `제약조건명`;
			query = MYSQLGetDropConstraintQuery(_tableName, _T("FOREIGN KEY"), _foreignKeyName);
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명;
			query = ORACLEGetDropConstraintQuery(_tableName, _T("R"), _foreignKeyName);
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring ForeignKey::GetForeignKeyName()
{
	_tstring uniqueName;

	switch( _dbClass )
	{
		// [외래키명]
		//	- FK_[외래키테이블명]_[참조키테이블명]_[외래키컬럼명1]_[외래키컬럼명2]...
		case EDBClass::MSSQL:
		case EDBClass::MYSQL:
		case EDBClass::ORACLE:
			uniqueName = _T("FK_") + _foreignKeyTableName + _T("_") + _referenceKeyTableName;
			for( const IndexColumnRef& column : _foreignKeyColumns )
			{
				uniqueName += _T("_");
				uniqueName += column->_columnName;
			}
			break;
	}

	return uniqueName;
}

//***************************************************************************
//
_tstring ForeignKey::CreateColumnsText(CVector<IndexColumnRef> columns)
{
	_tstring query;

	const int32 size = static_cast<int32>(columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			query += _T(", ");

		switch( _dbClass )
		{
			case EDBClass::MSSQL:
				// [컬럼명]
				query += tstring_tcformat(_T("[%s]"), columns[i]->_columnName.c_str());
				break;
			case EDBClass::MYSQL:
				// `컬럼명`
				query += tstring_tcformat(_T("`%s`"), columns[i]->_columnName.c_str());
				break;
			case EDBClass::ORACLE:
				// 컬럼명
				query += tstring_tcformat(_T("%s"), columns[i]->_columnName.c_str());
				break;
		}
	}
	return query;
}

//***************************************************************************
// MSSQL : 기존 default 값을 변경하고자 한다면, 기존 default 제약 조건을 삭제(DropDefaultConstraint 함수 실행)하고, 새로운 제약 조건을 추가(CreateDefaultConstraint 함수 실행)
// MYSQL : 기존 default 값을 변경하고자 한다면, CreateDefaultConstraint() 함수 사용
_tstring DefaultConstraint::CreateDefaultConstraint()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] DEFAULT 값 FOR [컬럼명]
			query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] ADD CONSTRAINT [%s] DEFAULT %s FOR [%s]"), _schemaName.c_str(), _tableName.c_str(), _defaultConstName.c_str(), _defaultValue.c_str(), _columnName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` ALTER COLUMN `컬럼명` SET DEFAULT 값;
			query = tstring_tcformat(_T("ALTER TABLE `%s` ALTER COLUMN `%s` SET DEFAULT %s;"), _tableName.c_str(), _columnName.c_str(), _defaultValue.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 MODIFY 컬럼명 DEFAULT 값
			query = tstring_tcformat(_T("ALTER TABLE %s MODIFY %s DEFAULT %s"), _tableName.c_str(), _columnName.c_str(), _defaultValue.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring DefaultConstraint::DropDefaultConstraint()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = MSSQLGetDropConstraintQuery(_schemaName, _tableName, _T("D"), _defaultConstName);
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` ALTER COLUMN `컬럼명` DROP DEFAULT;
			query = tstring_tcformat(_T("ALTER TABLE `%s` ALTER COLUMN `%s` DROP DEFAULT;"), _tableName.c_str(), _columnName.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 MODIFY 컬럼명 DEFAULT NULL
			query = tstring_tcformat(_T("ALTER TABLE %s MODIFY %s DEFAULT NULL"), _tableName.c_str(), _columnName.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring CheckConstraint::CreateCheckConstraint()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] WITH CHECK ADD CONSTRAINT [제약조건명] CHECK (제약조건정의)
			query = tstring_tcformat(_T("ALTER TABLE [%s].[%s] WITH CHECK ADD CONSTRAINT [%s] CHECK (%s)"), _schemaName.c_str(), _tableName.c_str(), _checkConstName.c_str(), _checkValue.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` ADD CONSTRAINT `제약조건명` CHECK (제약조건정의);
			query = tstring_tcformat(_T("ALTER TABLE `%s` ADD CONSTRAINT `%s` CHECK (%s);"), _tableName.c_str(), _checkConstName.c_str(), _checkValue.c_str());
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 ADD CONSTRAINT 제약조건명 CHECK (제약조건정의)
			query = tstring_tcformat(_T("ALTER TABLE %s ADD CONSTRAINT %s CHECK (%s)"), _tableName.c_str(), _checkConstName.c_str(), _checkValue.c_str());
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring CheckConstraint::DropCheckConstraint()
{
	_tstring query = _T("");

	switch( _dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = MSSQLGetDropConstraintQuery(_schemaName, _tableName, _T("C"), _checkConstName);
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE `테이블명` DROP CHECK `제약조건명`;
			query = MYSQLGetDropConstraintQuery(_tableName, _T("CHECK"), _checkConstName);
			break;
		case EDBClass::ORACLE:
			// ALTER TABLE 테이블명 DROP CONSTRAINT 제약조건명
			query = ORACLEGetDropConstraintQuery(_tableName, _T("C"), _checkConstName);
			break;
	}
	return query;
}

//***************************************************************************
//
_tstring Procedure::CreateQuery()
{
	return tstring_tcformat(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Procedure::DropQuery()
{
	return tstring_tcformat(_T("DROP PROCEDURE IF EXISTS %s"), _tableName.c_str());
}

//***************************************************************************
//
_tstring Function::CreateQuery()
{
	return tstring_tcformat(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Function::DropQuery()
{
	return tstring_tcformat(_T("DROP FUNCTION IF EXISTS %s"), _tableName.c_str());
}

//***************************************************************************
//
_tstring Helpers::RemoveWhiteSpace(const _tstring& str)
{
	_tstring ret = str;

	ret.erase(
		std::remove_if(ret.begin(), ret.end(), [=](TCHAR ch) { return ::isspace(ch); }),
		ret.end());

	return ret;
}

//***************************************************************************
//
void Helpers::LogFileWrite(EDBClass dbClass, _tstring title, _tstring query, bool newline)
{
	_tstring message;

	if( query.size() < 1 ) return;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			message = (newline ? _T("\n\n--") : _T("--")) + title;
			break;
		case EDBClass::MYSQL:
			message = (newline ? _T("\n\n#") : _T("#")) + title;
			break;
		case EDBClass::ORACLE:
			break;
	}

	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, message.c_str());
	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, query.c_str());
}


