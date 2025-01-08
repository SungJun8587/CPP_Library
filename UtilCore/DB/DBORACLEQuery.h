
//***************************************************************************
// DBORACLEQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBORACLEQUERY_H__
#define __DBORACLEQUERY_H__

#pragma once

//***************************************************************************
// ORACLE 인덱스타입 : ALL_INDEXES 테이블 INDEX_TYPE(VARCHAR2(27)) 컬럼
enum class EORACLEIndexType
{
	NONE = 0,
	LOB = 1,
	NORMAL = 2,
	NORMAL1REV = 3,
	BITMAP = 4,
	FUNCTION2BASED__NORMAL = 5,
	FUNCTION2BASED__NORMAL1REV = 6,
	FUNCTION2BASED__BITMAP = 7,
	FUNCTION2BASED__DOMAIN = 8,
	CLUSTER = 9,
	IOT__2__TOP = 10,
	DOMAIN_ = 11
};

inline const TCHAR* ToString(EORACLEIndexType v)
{
	switch( v )
	{
		case EORACLEIndexType::LOB:							return _T("LOB");
		case EORACLEIndexType::NORMAL:						return _T("NORMAL");
		case EORACLEIndexType::NORMAL1REV:					return _T("NORMAL/REV");
		case EORACLEIndexType::BITMAP:						return _T("BITMAP");
		case EORACLEIndexType::FUNCTION2BASED__NORMAL:		return _T("FUNCTION-BASED NORMAL");
		case EORACLEIndexType::FUNCTION2BASED__NORMAL1REV:	return _T("FUNCTION-BASED NORMAL/REV");
		case EORACLEIndexType::FUNCTION2BASED__BITMAP:		return _T("FUNCTION-BASED BITMAP");
		case EORACLEIndexType::FUNCTION2BASED__DOMAIN:		return _T("FUNCTION-BASED DOMAIN");
		case EORACLEIndexType::CLUSTER:						return _T("CLUSTER");
		case EORACLEIndexType::IOT__2__TOP:					return _T("IOT - TOP");
		case EORACLEIndexType::DOMAIN_:						return _T("DOMAIN");
		default:											return _T("NONE");
	}
}

inline const EORACLEIndexType StringToORACLEIndexType(const TCHAR* ptszIndexType)
{
	if( ::_tcsicmp(ptszIndexType, _T("LOB")) == 0 )
		return EORACLEIndexType::LOB;
	else if( ::_tcsicmp(ptszIndexType, _T("NORMAL")) == 0 )
		return EORACLEIndexType::NORMAL;
	else if( ::_tcsicmp(ptszIndexType, _T("NORMAL/REV")) == 0 )
		return EORACLEIndexType::NORMAL1REV;
	else if( ::_tcsicmp(ptszIndexType, _T("BITMAP")) == 0 )
		return EORACLEIndexType::BITMAP;
	else if( ::_tcsicmp(ptszIndexType, _T("FUNCTION-BASED NORMAL")) == 0 )
		return EORACLEIndexType::FUNCTION2BASED__NORMAL;
	else if( ::_tcsicmp(ptszIndexType, _T("FUNCTION-BASED NORMAL/REV")) == 0 )
		return EORACLEIndexType::FUNCTION2BASED__NORMAL1REV;
	else if( ::_tcsicmp(ptszIndexType, _T("FUNCTION-BASED BITMAP")) == 0 )
		return EORACLEIndexType::FUNCTION2BASED__BITMAP;
	else if( ::_tcsicmp(ptszIndexType, _T("FUNCTION-BASED DOMAIN")) == 0 )
		return EORACLEIndexType::FUNCTION2BASED__DOMAIN;
	else if( ::_tcsicmp(ptszIndexType, _T("CLUSTER")) == 0 )
		return EORACLEIndexType::CLUSTER;
	else if( ::_tcsicmp(ptszIndexType, _T("IOT - TOP")) == 0 )
		return EORACLEIndexType::IOT__2__TOP;
	else if( ::_tcsicmp(ptszIndexType, _T("DOMAIN")) == 0 )
		return EORACLEIndexType::DOMAIN_;

	return EORACLEIndexType::NONE;
}

//***************************************************************************
// ORACLE 인덱스 조각화 정보
//	- BLEVEL이 4이상(HIGH LEVEL)인 경우 REBUILD 대상으로 판단
class ORACLE_INDEX_FRAGMENTATION
{
public:
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };				// 테이블 명
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// 인덱스 명
	int32   BLevel;															// B*-트리 수준(루트 블록에서 리프 블록까지의 인덱스 깊이). 깊이는 0루트 블록과 리프 블록이 동일함을 나타냅니다.
	TCHAR   tszOK[16];														// BLevel 값에 따른 설명
	TCHAR	tszLastAnalyzed[DATETIME_STRLEN];								// 가장 최근에 분석된 일시
};

//***************************************************************************
// ORACLE 인덱스 조각화 정보
//	- PCT_DELETED가 20%이상으로 나타나면 인덱스는 REBUILD 대상으로 판단
//	- DISTINCTIVENESS 컬럼은 인덱스가 만들어진 컬럼의 값이 얼마나 자주 반복되는지를 보여주는 값(해당 값이 99%이상이면 BITMAP INDEX 대상)
class ORACLE_INDEX_STAT_FRAGMENTATION
{
public:
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// 인덱스 명
	float   PctDeleted;														// 인덱스에 전체 리프 행의 수에서 삭제된 리프 행의 수 비율(삭제된 리프 행의 수 / 전체 리프 행의 수 * 100) 
	float	Distinctiveness;												// 인덱스가 만들어진 컬럼의 값이 얼마나 자주 반복되는지를 보여주는 값
};

//***************************************************************************
// ORACLE 인덱스 조각화 정보
class ORACLE_TABLE_IDENTITY_COLUMN
{
public:
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// 컬럼 명
	TCHAR   tszIdentityColumn[4];							// 컬럼이 Identity인지 여부(YES/NO : 유/무)
	TCHAR   tszDefaultOnNull[4];							// 컬럼이 DEFAULT ON NULL 의미를 가지는지 여부(YES/NO : 유/무)

	/// <summary>ID 열의 생성 유형
	/// - ALWAYS : 기본 설정 옵션으로, 이 옵션을 사용하면 컬럼에 대한 사용자의 입력을 받지 않고 시스템이 자동으로 값을 할당. NULL 값 허용 안함(에러 발생).
	/// - BY DEFAULT : 이 옵션은 사용자가 컬럼에 대한 값을 지정하면 해당 값을 사용하고, 값이 제공되지 않으면 시스템이 값을 생성. NULL 값 허용 안함(에러 발생).
	/// - BY DEFAULT ON NULL : BY DEFAULT 옵션과 동일하지만, NULL 값을 입력되었을 경우 에러를 발생하지 않고 시스템이 자동으로 값을 할당.
	/// </summary>
	TCHAR	tszGenerationType[32];

	TCHAR   tszSequenceName[DATABASE_OBJECT_NAME_STRLEN];	// ID 열과 연관된 시퀀스 이름(SYS.USER_SEQUENCES 테이블 SEQUENCE_NAME 컬럼 참조)
	uint64	MinValue;										// 시퀀스의 최소값
	uint64	MaxValue;										// 시퀀스의 최대값
	uint64	IncrementBy;									// 시퀀스가 증가되는 값

	TCHAR   tszCycleFlag[2];								// 한계에 도달하면 시퀀스가 ​​순환 여부(Y/N : 유/무)
	TCHAR   tszOrderFlag[2];								// 시퀀스 번호가 순서대로 생성되는지 여부(Y/N : 유/무)
	uint64	CacheSize;										// 캐시할 시퀀스 번호 수

	/// <summary>디스크에 기록된 마지막 시퀀스 번호
	///     - 시퀀스가 캐싱을 사용하는 경우 디스크에 기록된 숫자는 시퀀스 캐시에 배치된 마지막 숫자입니다. 
	///     - 이 번호는 사용된 마지막 시퀀스 번호보다 클 가능성이 높습니다.
	///     - 세션 순서의 경우 이 열의 값은 무시되어야 합니다.
	/// </summary>
	uint64	LastNumber;

	TCHAR   tszScaleFlag[2];								// 확장 가능한 시퀀스인지 여부(Y/N : 유/무)
	TCHAR   tszExtendFlag[2];								// 이 확장 가능한 시퀀스의 생성된 값이 MAX_VALUE 또는 MIN_VALUE를 초과하는지 여부(Y/N : 유/무)
	TCHAR	tszShardedFlag[2];								// 이것이 분할된 시퀀스인지 여부(Y/N : 유/무)
	TCHAR   tszSessionFlag[2];								// 시퀀스 값이 세션 전용인지 여부(Y/N : 유/무)
	TCHAR   tszKeepValue[2];								// 시퀀스 값이 실패후 재생 중에 유지되는지 여부(Y/N : 유/무)
};

class ORACLEDBTableIdentityColumn
{
public:
	_tstring	SchemaName;
	_tstring	TableName;
	_tstring	ColumnName;
	_tstring	IdentityColumn;
	_tstring	DefaultOnNull;
	_tstring	GenerationType;
	_tstring	SequenceName;
	uint64		MinValue;
	uint64		MaxValue;
	uint64		IncrementBy;
	_tstring	CycleFlag;
	_tstring	OrderFlag;
	uint64		CacheSize;
	uint64		LastNumber;
	_tstring	ScaleFlag;
	_tstring	ExtendFlag;
	_tstring	ShardedFlag;
	_tstring	SessionFlag;
	_tstring	KeepValue;
};

//***************************************************************************
//
inline _tstring ORACLEGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, _tstring defaultDefinition, bool isIdentity, ORACLEDBTableIdentityColumn* pdbTabIdentityCols)
{
	_tstring columnOption = _T("");

	// <컬럼속성> : DEFAULT, {NULL|NOT NULL} 설정이 포함됨
	//  - 만약 컬럼 속성에 포함된 설정 값을 변경할 경우 아래와 같은 순서로 나열해서 변경하면 됨
	//  - DEFAULT 값 {NULL|NOT NULL}
	//
	// 컬럼명 데이터타입 <컬럼속성>
	//  - 컬럼명 데이터타입 [IDENTITY 컬럼 정의] {NULL|NOT NULL}
	//  - 컬럼명 데이터타입 {NULL|NOT NULL}
	//  - 컬럼명 데이터타입 DEFAULT 값 {NULL|NOT NULL}
	columnOption = dataTypeDesc;

	if( isIdentity && pdbTabIdentityCols != NULL )
	{
		// [IDENTITY 컬럼 정의]
		//  GENERATED        
		//  [ ALWAYS | BY DEFAULT [ ON NULL ] ]
		//  AS IDENTITY [ ( identity_options ) ]
		//  : IDENTITY 컬럼은 오라클 12c 버전부터 사용 가능하며, 테이블을 생성할 때 특정 컬럼에 자동증가 속성을 부여합니다.
		//    IDENTITY 컬럼은 내부적으로 시퀀스를 사용하여 값을 생성하지만, 사용자는 이 과정을 신경 쓸 필요가 없습니다. 
		//    테이블 정의 시에 해당 컬럼을 IDENTITY로 지정함으로써, 행이 추가될 때마다 오라클이 자동으로 값을 생성하고 할당합니다. 
		//      - GENERATED ALWAYS AS IDENTITY : 이 옵션을 사용하면 오라클은 항상 고유한 값을 자동으로 생성합니다. 
		//                                       사용자가 명시적으로 값을 지정하려고 하거나 NULL 값을 삽입하려고 할 때 에러가 발생합니다. 
		//                                       이는 기본 설정 옵션으로, 이 옵션을 사용하면 컬럼에 대한 사용자의 입력을 받지 않고 오라클 시스템이 자동으로 값을 할당합니다. 
		//      - GENERATED BY DEFAULT AS IDENTITY : 이 옵션은 사용자가 컬럼에 대해 값을 명시적으로 제공하지 않을 때만 오라클이 자동으로 값을 생성합니다. 
		//                                           사용자가 컬럼에 대한 값을 지정하면 해당 값을 사용하고, 값이 제공되지 않으면 시스템이 값을 생성합니다. 
		//                                           하지만 NULL 값을 삽입하려고 하면 에러가 발생합니다.
		//      - GENERATED BY DEFAULT ON NULL AS IDENTITY : 이 옵션은 BY DEFAULT 옵션과 유사하지만, 차이점은 NULL 값을 컬럼에 삽입할 수 있으며, 이 경우 시스템이 자동으로 값을 생성한다는 것입니다. 
		//                                                   사용자가 값을 명시적으로 제공하거나, 값을 제공하지 않거나, NULL 값을 제공하면, 오라클이 자동으로 고유한 값을 생성합니다.
		//      
		//          - START WITH: 시작 값 지정. 기본값은 1입니다.
		//          - INCREMENT BY: 증가량 지정. 기본값은 1입니다.
		//          - MINVALUE, MAXVALUE: 값의 최소값 및 최대값을 지정할 수 있습니다.
		//          - CACHE/NOCACHE: 성능 최적화를 위해 특정 수의 값들을 미리 생성하고 캐시에 저장합니다. NOCACHE는 이 기능을 비활성화합니다.
		//          - ORDER/NOORDER: 시퀀스 번호가 순서대로 생성되었는지 여부를 결정합니다.
		//          - CYCLE/NOCYCLE: 최대값에 도달한 후 다시 최소값으로 돌아갈지 여부를 결정합니다.
		//          - KEEP/NOKEEP: 시퀀스 값이 실패후 재생 중에 유지되는지 여부를 결정합니다.
		//          - SCALE/NOSCALE: 확장 가능한 시퀀스인지 여부를 결정합니다.
		//  Ex)
		//      CREATE TABLE users (
		//          user_id NUMBER GENERATED BY DEFAULT AS IDENTITY
		//          START WITH 1000 INCREMENT BY 1,
		//          username VARCHAR2(100)
		//      );
		//
		//      INSERT INTO users (username) VALUES ('user1');
		//
		columnOption = columnOption + " GENERATED " + pdbTabIdentityCols->GenerationType + (pdbTabIdentityCols->DefaultOnNull == "YES" ? " ON NULL" : "") + " AS IDENTITY";
		columnOption = columnOption + " MINVALUE " + to_tstring(pdbTabIdentityCols->MinValue);
		columnOption = columnOption + " MAXVALUE " + to_tstring(pdbTabIdentityCols->MaxValue);
		columnOption = columnOption + " INCREMENT BY " + to_tstring(pdbTabIdentityCols->IncrementBy);
		columnOption = columnOption + " START WITH " + to_tstring(pdbTabIdentityCols->MinValue);
		columnOption = columnOption + (pdbTabIdentityCols->CacheSize > 0 ? " CACHE " + pdbTabIdentityCols->CacheSize : " NOCACHE");
		columnOption = columnOption + (pdbTabIdentityCols->OrderFlag == "Y" ? " ORDER" : " NOORDER");
		columnOption = columnOption + (pdbTabIdentityCols->CycleFlag == "Y" ? " CYCLE" : " NOCYCLE");
		columnOption = columnOption + (pdbTabIdentityCols->KeepValue == "Y" ? " KEEP" : " NOKEEP");
		columnOption = columnOption + (pdbTabIdentityCols->ScaleFlag == "Y" ? " SCALE" : " NOSCALE");
		columnOption = columnOption + (isNullable ? " NULL" : " NOT NULL");
	}
	else columnOption = columnOption + (defaultDefinition != "" ? _T(" DEFAULT ") + defaultDefinition : _T("")) + (isNullable ? " NULL" : " NOT NULL");

	return columnOption;
}

//***************************************************************************
//
inline _tstring ORACLEGetDropConstraintQuery(_tstring tableName, _tstring constType, _tstring constName)
{
	_tstring query = _T("");

	if( constType == "P" || constType == "U" || constType == "R" || constType == "C" )
	{
		// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
		query = tstring_tcformat(_T("ALTER TABLE %s DROP CONSTRAINT %s"), tableName.c_str(), constName.c_str());
	}
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetDBSystemQuery()
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT BANNER_FULL FROM v$version) AS \"version\", ";
	query = query + "\n" + "(SELECT VALUE FROM NLS_DATABASE_PARAMETERS WHERE PARAMETER = 'NLS_CHARACTERSET') AS \"characterset\", ";
	query = query + "(SELECT VALUE FROM NLS_DATABASE_PARAMETERS WHERE PARAMETER = 'NLS_SORT') AS \"collation\"";
	query = query + "FROM DUAL";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetUserListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT USERNAME AS \"name\" FROM ALL_USERS";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTableSpaceListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT TABLESPACE_NAME AS \"name\" FROM DBA_DATA_FILES";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetDatabaseListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT NAME AS \"name\" FROM V$DATABASE";
	return query;
}

//***************************************************************************
// Ex)
//	COMMENT ON TABLE 테이블명 IS '코멘트';
//  COMMENT ON COLUMN 테이블명.컬럼명 IS '코멘트';
inline _tstring ORACLEProcessTableColumnCommentQuery(_tstring tableName, _tstring setComment, _tstring columnName = _T(""))
{
	_tstring query = _T("");

	if( columnName != "" )
		query = query + "COMMENT ON TABLE " + tableName + " IS '" + setComment + "'";
	else query = query + "COMMENT ON COLUMN " + tableName + "." + columnName + " IS '" + setComment + "'";

	return query;
}

//***************************************************************************
// Ex)
//  SELECT DBMS_METADATA.GET_DDL('TABLE', '테이블명') SCRIPT FROM DUAL
//  SELECT DBMS_METADATA.GET_DDL('INDEX', '인덱스명') SCRIPT FROM DUAL
inline _tstring ORACLEMetaDataGetDDLQuery(EDBObjectType dbObjectType, _tstring objectName, _tstring schemaName = _T(""))
{
	_tstring query = _T("");

	if( schemaName != "" )
		query = tstring_tcformat(_T("SELECT DBMS_METADATA.GET_DDL('%s', '%s', '%s') SCRIPT FROM DUAL"), ToString(dbObjectType), objectName.c_str(), schemaName.c_str());
	else query = tstring_tcformat(_T("SELECT DBMS_METADATA.GET_DDL('%s, '%s') SCRIPT FROM DUAL"), ToString(dbObjectType), objectName.c_str());

	return query;
}


//***************************************************************************
// 해당 인덱스 생성 쿼리 확인
inline _tstring ORACLETableIndexMetaDataGetDDLQuery(_tstring tableName, _tstring indexName = _T(""), _tstring schemaName = _T(""))
{
	_tstring query = _T("");

	if( schemaName != "" )
	{
		if( indexName != "" )
			query = query + "SELECT a.TABLE_NAME AS \"table_name\", a.INDEX_NAME AS \"index_name\", DBMS_METADATA.GET_DDL('INDEX', '" + indexName + "', '" + schemaName + "') AS \"create_script\"";
		else query = query + "SELECT a.TABLE_NAME AS \"table_name\", a.INDEX_NAME AS \"index_name\", DBMS_METADATA.GET_DDL('INDEX', a.INDEX_NAME, '" + schemaName + "') AS \"create_script\"";
		query = query + "\n" + "FROM SYS.USER_INDEXES a";
		query = query + "\n" + "LEFT OUTER JOIN";
		query = query + "\n" + "(";
		query = query + "\n\t" + "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE";
		query = query + "\n\t" + "FROM SYS.USER_CONSTRAINTS";
		query = query + "\n\t" + "WHERE CONSTRAINT_TYPE IN('P')";
		query = query + "\n" + ") b";
		query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.INDEX_NAME = b.CONSTRAINT_NAME";
		query = query + "\n" + "WHERE a.TABLE_OWNER = '" + schemaName + "' AND a.TABLE_NAME = '" + tableName + "' AND b.CONSTRAINT_NAME IS NULL";
	}
	else
	{
		if( indexName != "" )
			query = query + "SELECT a.TABLE_NAME AS \"table_name\", a.INDEX_NAME AS \"index_name\", DBMS_METADATA.GET_DDL('INDEX', '" + indexName + "') AS \"create_script\"";
		else query = query + "SELECT a.TABLE_NAME AS \"table_name\", a.INDEX_NAME AS \"index_name\", DBMS_METADATA.GET_DDL('INDEX', a.INDEX_NAME) AS \"create_script\"";
		query = query + "\n" + "FROM SYS.USER_INDEXES a";
		query = query + "\n" + "LEFT OUTER JOIN";
		query = query + "\n" + "(";
		query = query + "\n\t" + "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE";
		query = query + "\n\t" + "FROM SYS.USER_CONSTRAINTS";
		query = query + "\n\t" + "WHERE CONSTRAINT_TYPE IN('P')";
		query = query + "\n" + ") b";
		query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.INDEX_NAME = b.CONSTRAINT_NAME";
		query = query + "\n" + "WHERE a.TABLE_NAME = '" + tableName + "' AND b.CONSTRAINT_NAME IS NULL";
	}

	return query;
}

//***************************************************************************
// 해당 테이블에 외래키 생성 쿼리 만들기
inline _tstring ORACLETableForeignKeyCreateSQLQuery(_tstring tableName, _tstring constraintName = _T(""))
{
	_tstring query = _T("");

	if( constraintName != "" )
	{
		query = query + "SELECT 'ALTER TABLE ' || TABLE_NAME || ' ADD CONSTRAINT ' || CONSTRAINT_NAME || CHR(10)";
		query = query + "|| '  FOREIGN KEY (' || (SELECT LISTAGG(COLUMN_NAME, ',') WITHIN GROUP (ORDER BY POSITION) FROM DBA_CONS_COLUMNS WHERE CONSTRAINT_NAME = A.CONSTRAINT_NAME) || ')' || CHR(10)";
		query = query + "|| '  REFERENCES ' || (SELECT TABLE_NAME FROM DBA_CONSTRAINTS WHERE CONSTRAINT_NAME = TRIM(A.R_CONSTRAINT_NAME))";
		query = query + "|| '(' || (SELECT LISTAGG(COLUMN_NAME, ',') WITHIN GROUP (ORDER BY POSITION) FROM DBA_CONS_COLUMNS WHERE CONSTRAINT_NAME = A.R_CONSTRAINT_NAME) || ')' || CHR(10)";
		query = query + "|| '  ON DELETE ' || DELETE_RULE || ' ' || (CASE STATUS WHEN 'ENABLED' THEN 'ENABLE' ELSE 'DISABLE' END)  || ';' AS SCRIPT";
		query = query + "\n" + "FROM USER_CONSTRAINTS A";
		query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "' AND CONSTRAINT_NAME = '" + constraintName + "' AND CONSTRAINT_TYPE = 'R'";
	}
	else
	{
		query = query + "SELECT 'ALTER TABLE ' || TABLE_NAME || ' ADD CONSTRAINT ' || CONSTRAINT_NAME || CHR(10)";
		query = query + "|| '  FOREIGN KEY (' || (SELECT LISTAGG(COLUMN_NAME, ',') WITHIN GROUP (ORDER BY POSITION) FROM DBA_CONS_COLUMNS WHERE CONSTRAINT_NAME = A.CONSTRAINT_NAME) || ')' || CHR(10)";
		query = query + "|| '  REFERENCES ' || (SELECT TABLE_NAME FROM DBA_CONSTRAINTS WHERE CONSTRAINT_NAME = TRIM(A.R_CONSTRAINT_NAME))";
		query = query + "|| '(' || (SELECT LISTAGG(COLUMN_NAME, ',') WITHIN GROUP (ORDER BY POSITION) FROM DBA_CONS_COLUMNS WHERE CONSTRAINT_NAME = A.R_CONSTRAINT_NAME) || ')' || CHR(10)";
		query = query + "|| '  ON DELETE ' || DELETE_RULE || ' ' || (CASE STATUS WHEN 'ENABLED' THEN 'ENABLE' ELSE 'DISABLE' END)  || ';' AS SCRIPT";
		query = query + "\n" + "FROM USER_CONSTRAINTS A";
		query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "' AND CONSTRAINT_TYPE = 'R'";
	}

	return query;
}

//***************************************************************************
// 해당 테이블, 컬럼 코멘트 생성 쿼리 만들기
inline _tstring ORACLETableCommentCreateSQLQuery(_tstring tableName)
{
	_tstring query = _T("");

	query = query + "SELECT 'COMMENT ON TABLE ' || TABLE_NAME || ' IS ' || '''' || COMMENTS || '''' || ';'";
	query = query + "\n" + "FROM SYS.USER_TAB_COMMENTS";
	query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "'";
	query = query + "\n" + "UNION";
	query = query + "\n" + "SELECT 'COMMENT ON COLUMN ' || TABLE_NAME || '.' || COLUMN_NAME || ' IS ' || '''' || COMMENTS || '''' || ';'";
	query = query + "\n" + "FROM SYS.USER_COL_COMMENTS";
	query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "'";
	return query;
}

//***************************************************************************
// PROCEDURE, FUNCTION, TRIGGER 등 객체의 텍스트 소스 확인
// TYPE 컬럼 : PROCEDURE, FUNCTION, TRIGGER 등으로 구분
inline _tstring ORACLEGetUserSourceQuery(EDBObjectType dbObjectType, _tstring objectName)
{
	_tstring query = tstring_tcformat(_T("SELECT TEXT FROM SYS.USER_SOURCE WHERE TYPE = '%s' AND NAME = '%s'"), ToString(dbObjectType), objectName.c_str());
	return query;
}

//***************************************************************************
// 인덱스 리빌드 대상 확인 1(1, 2 순서대로 처리)
//  1. ORACLEGetAnalyzeIndexFragmentationCheckQuery : 분석하고자 하는 인덱스에 대한 통계정보를 생성
//  2. ORACLEGetIndexFragmentationCheckQuery : BLEVEL이 4이상(HIGH LEVEL)인 경우 REBUILD 대상으로 판단
inline _tstring ORACLEGetAnalyzeIndexFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	// 분석하고자 하는 인덱스에 대한 통계정보를 생성
	// 수백만건 이상의 row를 지닌 테이블에 대한 인덱스인 경우 COMPUTE STATISTICS 대신에 ESTIMATE 옵션을 사용할 것.
	query = query + "ANALYZE INDEX " + indexName + " COMPUTE STATISTICS";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetIndexFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	// BLEVEL이 4이상(HIGH LEVEL)인 경우 REBUILD 대상으로 판단
	// 이 BLEVEL(Branch Level)이 의미하는 것은 오라클이 Index Access를 할 때 몇 단계를 거쳐서 블럭의 위치를 찾아가는가와 관계가 있음
	// 아래 SQL 수행시 통계가 수집되지 않은 인덱스에 대해서는 "BLEVEL HIGH" 로 나타남
	query = query + "SELECT TABLE_NAME AS \"table_name\", INDEX_NAME AS \"index_name\", BLEVEL AS \"blevel\", DECODE(BLEVEL, 0, 'OK BLEVEL', 1, 'OK BLEVEL', 2, 'OK BLEVEL', 3, 'OK BLEVEL', 4, 'OK BLEVEL', 'BLEVEL HIGH') \"ok?\", ";
	query = query + "\n" + "TO_CHAR(LAST_ANALYZED, 'yyyy-mm-dd hh24:mi:ss') AS \"last_analyzed\"";
	query = query + "\n" + "FROM SYS.USER_INDEXES";
	query = query + "\n" + "WHERE INDEX_NAME = '" + indexName + "'";
	query = query + "\n" + "ORDER BY BLEVEL DESC";
	return query;
}

//***************************************************************************
// 인덱스 리빌드 대상 확인 2(1, 2 순서대로 처리)
//  1. ORACLEGetAnalyzeIndexStatFragmentationCheckQuery : INDEX_STATS 테이블에 추가적인 인덱스 정보를 생성
//  2. ORACLEGetIndexStatFragmentationCheckQuery : PCT_DELETED가 20%이상으로 나타나면 인덱스는 REBUILD 대상으로 판단
inline _tstring ORACLEGetAnalyzeIndexStatFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	// INDEX_STATS 테이블에 추가적인 인덱스 정보를 생성
	// 주의할 점은 아래의 쿼리 실행시 Lock이 발생하므로, 점검시에 해야함.
	query = query + "ANALYZE INDEX " + indexName + " VALIDATE STRUCTURE";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetIndexStatFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	// PCT_DELETED가 20%이상으로 나타나면 인덱스는 REBUILD 대상으로 판단
	// DISTINCTIVENESS 컬럼은 인덱스가 만들어진 컬럼의 값이 얼마나 자주 반복되는지를 보여주는 값(해당 값이 99%이상이면 BITMAP INDEX 대상)
	//  - 1만건의 row와 9000건의 서로 다른 값을 가진 테이블이 있을 때 DISTINCTIVENESS값 : (10000 - 9000) * 100 / 10000 = 10     => 컬럼의 값이 잘 분산되어 있음
	//  - 1만건의 row가 있지만 2가지 값으로만 중복되어 있을 때 DISTINCTIVENESS값 : (10000 - 2) * 100 / 10000 = 99.98            => rebuild 대상이 아니라 BITMAP INDEX로 만들 대상(99%이상이면 BITMAP INDEX 대상)
	query = query + "SELECT NAME AS \"index_name\", DEL_LF_ROWS * 100 / DECODE(LF_ROWS, 0, 1, LF_ROWS) AS \"PCT_DELETED\", ";
	query = query + "\n" + "(LF_ROWS - DISTINCT_KEYS) * 100 / DECODE(LF_ROWS, 0, 1, LF_ROWS) AS \"distinctiveness\"";
	query = query + "\n" + "FROM SYS.INDEX_STATS";
	query = query + "\n" + "WHERE NAME = '" + indexName + "'";
	query = query + "\n" + "ORDER BY PCT_DELETED DESC";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetIndexRebuildQuery(_tstring indexName)
{
	_tstring query = _T("");

	// ALTER INDEX 인덱스명 REBUILD;
	query = query + "ALTER INDEX " + indexName + " REBUILD";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTableListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 1 AS \"object_type\", OBJECT_NAME AS \"object_name\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS";
	query = query + "\n" + "WHERE OBJECT_TYPE = 'TABLE'";
	query = query + "\n" + "ORDER BY OBJECT_NAME ASC";
	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTableInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.OBJECT_NAME AS \"table_name\", 0 AS \"auto_increment\", ";
	query = query + "'' AS \"engine\", '' AS \"characterset\", '' AS \"collation\", ";
	query = query + "b.COMMENTS AS \"table_comment\", TO_CHAR(a.CREATED, 'YYYY-MM-DD HH24:MI:SS') AS \"create_date\", TO_CHAR(a.LAST_DDL_TIME, 'YYYY-MM-DD HH24:MI:SS') AS \"modify_date\"";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT OBJECT_ID, OBJECT_NAME, CREATED, LAST_DDL_TIME";
	query = query + "\n\t" + "FROM SYS.USER_OBJECTS";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE OBJECT_TYPE = 'TABLE' AND OBJECT_NAME = '" + tableName + "'";
	else query = query + "\n\t" + "WHERE OBJECT_TYPE = 'TABLE'";

	query = query + "\n" + ") a";
	query = query + "\n" + "LEFT OUTER JOIN SYS.USER_TAB_COMMENTS b";
	query = query + "\n" + "ON a.OBJECT_NAME = b.TABLE_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY a.OBJECT_NAME ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTableColumnInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.TABLE_NAME AS \"table_name\", a.COLUMN_ID AS \"seq\", a.COLUMN_NAME AS \"column_name\", ";
	query = query + "a.DATA_TYPE AS \"datatype\", (CASE a.DATA_TYPE WHEN 'NUMBER' THEN TO_CHAR(a.DATA_LENGTH) WHEN 'DATE' THEN ' ' ELSE TO_CHAR(a.DATA_LENGTH) END) AS \"max_length\", ";
	query = query + "a.DATA_PRECISION AS \"precision\", a.DATA_SCALE AS \"scale\", ";
	query = query + "a.DATA_TYPE || ";
	query = query + "(CASE";
	query = query + " WHEN a.DATA_TYPE = 'NUMBER' AND a.DATA_SCALE > 0 THEN '(' || TO_CHAR(a.DATA_PRECISION) || ',' || TO_CHAR(a.DATA_SCALE) || ')'";
	query = query + " WHEN a.DATA_TYPE = 'NUMBER' AND a.DATA_PRECISION > 0 AND a.DATA_SCALE = 0 THEN '(' || TO_CHAR(a.DATA_PRECISION) || ')'";
	query = query + " WHEN a.DATA_TYPE = 'NUMBER' AND a.DATA_PRECISION IS NULL THEN ''";
	query = query + " WHEN a.DATA_TYPE IN('CHAR', 'NCHAR', 'VARCHAR2', 'NVARCHAR2', 'DATE') THEN (CASE WHEN a.DATA_LENGTH > 0 THEN '(' || TO_CHAR(a.DATA_LENGTH) || ')' ELSE '' END)";
	query = query + " ELSE '' END";
	query = query + ") AS \"datatype_desc\", ";
	query = query + "(CASE a.NULLABLE WHEN 'Y' THEN 1 ELSE 0 END) AS \"is_nullable\", ";
	query = query + "(CASE a.IDENTITY_COLUMN WHEN 'YES' THEN 1 ELSE 0 END) AS \"is_identity\", ";
	query = query + "0 AS \"seed_value\", 0 AS \"inc_value\", ";
	query = query + "'' AS \"default_constraintname\", a.DATA_DEFAULT AS \"default_definition\", ";
	query = query + "a.COLLATION AS \"collation\", b.COMMENTS AS \"column_comment\"";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT TABLE_NAME, COLUMN_ID, COLUMN_NAME, DATA_TYPE, DATA_LENGTH, DATA_PRECISION, DATA_SCALE, NULLABLE, IDENTITY_COLUMN, DATA_DEFAULT, COLLATION";
	query = query + "\n\t" + "FROM SYS.USER_TAB_COLUMNS";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE TABLE_NAME = '" + tableName + "'";

	query = query + "\n" + ") a";
	query = query + "\n" + "LEFT OUTER JOIN SYS.USER_COL_COMMENTS b";
	query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.COLUMN_NAME = b.COLUMN_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY a.COLUMN_ID ASC";
	else query = query + "\n" + "ORDER BY a.TABLE_NAME ASC, a.COLUMN_ID ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTableIdentityColumnInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", USER AS \"schema_name\", a.TABLE_NAME AS \"table_name\", a.COLUMN_NAME AS \"column_name\", a.IDENTITY_COLUMN AS \"identity_column\", a.DEFAULT_ON_NULL AS \"default_on_null\", ";
	query = query + "b.GENERATION_TYPE AS \"generation_type\", b.SEQUENCE_NAME AS \"sequence_name\", b.MIN_VALUE AS \"min_value\", ";
	query = query + "b.MAX_VALUE AS \"max_value\", b.INCREMENT_BY AS \"increment_by\", b.CYCLE_FLAG AS \"cycle_flag\", b.ORDER_FLAG AS \"order_flag\", ";
	query = query + "b.CACHE_SIZE AS \"cache_size\", b.LAST_NUMBER AS \"last_number\", b.SCALE_FLAG AS \"scale_flag\", b.EXTEND_FLAG AS \"extend_flag\", ";
	query = query + "b.SHARDED_FLAG AS \"sharded_flag\", b.SESSION_FLAG AS \"session_flag\", b.KEEP_VALUE AS \"keep_value\"";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT TABLE_NAME, COLUMN_NAME, DEFAULT_ON_NULL, IDENTITY_COLUMN";
	query = query + "\n\t" + "FROM SYS.USER_TAB_COLUMNS";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE TABLE_NAME = '" + tableName + "' AND IDENTITY_COLUMN = 'YES'";
	else query = query + "\n\t" + "WHERE IDENTITY_COLUMN = 'YES'";

	query = query + "\n" + ") a";
	query = query + "\n" + "INNER JOIN";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.TABLE_NAME, a.COLUMN_NAME, a.GENERATION_TYPE, b.*";
	query = query + "\n\t" + "FROM SYS.USER_TAB_IDENTITY_COLS a";
	query = query + "\n\t" + "INNER JOIN SYS.USER_SEQUENCES b";
	query = query + "\n\t" + "ON a.SEQUENCE_NAME = b.SEQUENCE_NAME";
	query = query + "\n" + ") b";
	query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.COLUMN_NAME = b.COLUMN_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY a.TABLE_NAME ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetConstraintsInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", TABLE_NAME AS \"table_name\", CONSTRAINT_NAME AS \"const_name\", ";
	query = query + "CONSTRAINT_TYPE AS \"const_type\", '' AS \"const_type_desc\", SEARCH_CONDITION AS \"const_value\", INDEX_NAME AS \"index_name\", (CASE GENERATED WHEN 'GENERATED NAME' THEN 1 ELSE 0 END) AS \"is_system_named\", ";
	query = query + "(CASE STATUS WHEN 'DISABLED' THEN 0 ELSE 1 END) AS \"is_status\", ";
	query = query + "(CASE CONSTRAINT_TYPE WHEN 'P' THEN 1 WHEN 'U' THEN 2 WHEN 'R' THEN 3 WHEN 'C' THEN 5 END) AS \"SORT_VALUE\"";
	query = query + "\n" + "FROM SYS.USER_CONSTRAINTS";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "'";
		query = query + "\n" + "ORDER BY SORT_VALUE ASC";
	}
	else
	{
		query = query + "\n" + "ORDER BY TABLE_NAME ASC, SORT_VALUE ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetIndexInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.TABLE_NAME AS \"table_name\", a.INDEX_NAME AS \"index_name\", 0 AS \"index_id\", ";
	query = query + "a.INDEX_TYPE AS \"index_type\", (CASE WHEN b.CONSTRAINT_TYPE = 'P' THEN 1 ELSE 0 END) AS \"is_primary_key\", (CASE a.UNIQUENESS WHEN 'UNIQUE' THEN 1 ELSE 0 END) AS \"is_unique\", ";
	query = query + "c.COLUMN_POSITION AS \"column_seq\", c.COLUMN_NAME AS \"column_name\", (CASE WHEN c.DESCEND = 'ASC' THEN 1 ELSE 2 END) AS \"column_sort\", (CASE a.GENERATED WHEN 'Y' THEN 1 ELSE 0 END) AS \"is_system_named\"";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT TABLE_NAME, INDEX_NAME, INDEX_TYPE, UNIQUENESS, GENERATED";
	query = query + "\n\t" + "FROM SYS.USER_INDEXES";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE TABLE_NAME = '" + tableName + "'";

	query = query + "\n" + ") a";
	query = query + "\n" + "LEFT OUTER JOIN";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, INDEX_NAME";
	query = query + "\n\t" + "FROM SYS.USER_CONSTRAINTS";
	query = query + "\n\t" + "WHERE CONSTRAINT_TYPE IN('P', 'U')";
	query = query + "\n" + ") b";
	query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.INDEX_NAME = b.INDEX_NAME";
	query = query + "\n" + "INNER JOIN SYS.USER_IND_COLUMNS c";
	query = query + "\n" + "ON a.TABLE_NAME = c.TABLE_NAME AND a.INDEX_NAME = c.INDEX_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY a.INDEX_NAME ASC, c.COLUMN_POSITION ASC";
	else query = query + "\n" + "ORDER BY a.TABLE_NAME ASC, a.INDEX_NAME ASC, c.COLUMN_POSITION ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetPartitionInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.TABLE_NAME, a.PARTITIONING_TYPE, a.SUBPARTITIONING_TYPE, ";
	query = query + "a.PARTITION_COUNT, a.DEF_SUBPARTITION_COUNT, a.PARTITIONING_KEY_COUNT, a.SUBPARTITIONING_KEY_COUNT, a.DEF_TABLESPACE_NAME, ";
	query = query + "b.COMPOSITE, b.PARTITION_NAME, b.SUBPARTITION_COUNT, b.HIGH_VALUE, b.HIGH_VALUE_LENGTH, b.PARTITION_POSITION, b.TABLESPACE_NAME, b.PCT_FREE, b.PCT_USED, b.INI_TRANS, b.MAX_TRANS, b.INITIAL_EXTENT, b.NEXT_EXTENT, b.MIN_EXTENT, b.MAX_EXTENT, b.MAX_SIZE, b.PCT_INCREASE, b.FREELISTS, b.FREELIST_GROUPS, b.LOGGING, b.COMPRESSION, b.COMPRESS_FOR, b.NUM_ROWS, b.BLOCKS, b.EMPTY_BLOCKS, b.AVG_SPACE, b.CHAIN_CNT, b.AVG_ROW_LEN, b.SAMPLE_SIZE, b.LAST_ANALYZED, b.BUFFER_POOL, b.FLASH_CACHE, b.CELL_FLASH_CACHE, b.GLOBAL_STATS, b.USER_STATS, b.IS_NESTED, b.PARENT_TABLE_PARTITION, b.INTERVAL, b.SEGMENT_CREATED, b.INDEXING, b.READ_ONLY, b.INMEMORY, b.INMEMORY_PRIORITY, b.INMEMORY_DISTRIBUTE, b.INMEMORY_COMPRESSION, b.INMEMORY_DUPLICATE, b.CELLMEMORY, b.INMEMORY_SERVICE, b.INMEMORY_SERVICE_NAME, b.MEMOPTIMIZE_READ, b.MEMOPTIMIZE_WRITE, ";
	query = query + "c.SUBPARTITION_NAME, c.HIGH_VALUE, c.HIGH_VALUE_LENGTH, c.PARTITION_POSITION, c.SUBPARTITION_POSITION, c.TABLESPACE_NAME, c.PCT_FREE, c.PCT_USED, c.INI_TRANS, c.MAX_TRANS, c.INITIAL_EXTENT, c.NEXT_EXTENT, c.MIN_EXTENT, c.MAX_EXTENT, c.MAX_SIZE, c.PCT_INCREASE, c.FREELISTS, c.FREELIST_GROUPS, c.LOGGING, c.COMPRESSION, c.COMPRESS_FOR, c.NUM_ROWS, c.BLOCKS, c.EMPTY_BLOCKS, c.AVG_SPACE, c.CHAIN_CNT, c.AVG_ROW_LEN, c.SAMPLE_SIZE, c.LAST_ANALYZED, c.BUFFER_POOL, c.FLASH_CACHE, c.CELL_FLASH_CACHE, c.GLOBAL_STATS, c.USER_STATS, c.INTERVAL, c.SEGMENT_CREATED, c.INDEXING, c.READ_ONLY, c.INMEMORY, c.INMEMORY_PRIORITY, c.INMEMORY_DISTRIBUTE, c.INMEMORY_COMPRESSION, c.INMEMORY_DUPLICATE, c.INMEMORY_SERVICE, c.INMEMORY_SERVICE_NAME, c.CELLMEMORY, c.MEMOPTIMIZE_READ, c.MEMOPTIMIZE_WRITE, ";
	query = query + "a.COLUMN_POSITION, a.COLUMN_NAME";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.TABLE_NAME, a.PARTITIONING_TYPE, a.SUBPARTITIONING_TYPE, a.PARTITION_COUNT, a.DEF_SUBPARTITION_COUNT, a.PARTITIONING_KEY_COUNT, a.SUBPARTITIONING_KEY_COUNT, a.DEF_TABLESPACE_NAME, b.COLUMN_POSITION, b.COLUMN_NAME";
	query = query + "\n\t" + "FROM SYS.USER_PART_TABLES a";
	query = query + "\n\t" + "INNER JOIN SYS.USER_PART_KEY_COLUMNS b";
	query = query + "\n\t" + "ON a.TABLE_NAME = b.NAME";

	if( tableName != "" )
		query = query + "\n\t" + "WHERE a.TABLE_NAME = '" + tableName + "'";

	query = query + "\n" + ") a";
	query = query + "\n" + "INNER JOIN SYS.USER_TAB_PARTITIONS b";
	query = query + "\n" + "ON a.TABLE_NAME = b.TABLE_NAME";
	query = query + "\n" + "LEFT OUTER JOIN SYS.USER_TAB_SUBPARTITIONS c";
	query = query + "\n" + "ON a.TABLE_NAME = c.TABLE_NAME AND b.PARTITION_NAME = c.PARTITION_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY b.PARTITION_NAME ASC, b.PARTITION_POSITION ASC, c.SUBPARTITION_POSITION ASC, a.COLUMN_POSITION ASC";
	else query = query + "\n" + "ORDER BY a.TABLE_NAME ASC, b.PARTITION_NAME ASC, b.PARTITION_POSITION ASC, c.SUBPARTITION_POSITION ASC, a.COLUMN_POSITION ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetForeignKeyInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.TABLE_NAME AS \"table_name\", a.CONSTRAINT_NAME AS \"foreignkey_name\", 0 AS \"is_disabled\", 0 AS \"is_not_trusted\", ";
	query = query + "a.TABLE_NAME AS \"foreignkey_table_name\", a.COLUMN_NAME AS \"foreignkey_column_name\", ";
	query = query + "'' AS \"referencekey_schema_name\", b.TABLE_NAME AS \"referencekey_table_name\", b.COLUMN_NAME AS \"referencekey_column_name\", ";
	query = query + "'' AS \"update_rule\", a.DELETE_RULE AS \"delete_rule\", (CASE a.GENERATED WHEN 'GENERATED NAME' THEN 1 ELSE 0 END) AS \"is_system_named\"";
	query = query + "\n" + "FROM";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.*, b.POSITION, b.COLUMN_NAME";
	query = query + "\n\t" + "FROM";
	query = query + "\n\t" + "(";
	query = query + "\n\t\t" + "SELECT TABLE_NAME, CONSTRAINT_NAME, R_CONSTRAINT_NAME, DELETE_RULE, GENERATED";
	query = query + "\n\t\t" + "FROM SYS.USER_CONSTRAINTS";

	if( tableName != "" )
		query = query + "\n\t\t" + "WHERE TABLE_NAME = '" + tableName + "' AND CONSTRAINT_TYPE = 'R' AND STATUS = 'ENABLED'";
	else query = query + "\n\t\t" + "WHERE CONSTRAINT_TYPE = 'R' AND STATUS = 'ENABLED'";

	query = query + "\n\t" + ") a";
	query = query + "\n\t" + "INNER JOIN SYS.USER_CONS_COLUMNS b";
	query = query + "\n\t" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.CONSTRAINT_NAME = b.CONSTRAINT_NAME";
	query = query + "\n" + ") a";
	query = query + "\n" + "INNER JOIN";
	query = query + "\n" + "(";
	query = query + "\n\t" + "SELECT a.TABLE_NAME, a.CONSTRAINT_NAME, b.POSITION, b.COLUMN_NAME";
	query = query + "\n\t" + "FROM";
	query = query + "\n\t" + "(";
	query = query + "\n\t\t" + "SELECT TABLE_NAME, CONSTRAINT_NAME";
	query = query + "\n\t\t" + "FROM SYS.USER_CONSTRAINTS";
	query = query + "\n\t\t" + "WHERE CONSTRAINT_TYPE IN('P', 'U')";
	query = query + "\n\t" + ") a";
	query = query + "\n\t" + "INNER JOIN SYS.USER_CONS_COLUMNS b";
	query = query + "\n\t" + "ON a.TABLE_NAME = b.TABLE_NAME AND a.CONSTRAINT_NAME = b.CONSTRAINT_NAME";
	query = query + "\n" + ") b";
	query = query + "\n" + "ON a.R_CONSTRAINT_NAME = b.CONSTRAINT_NAME AND a.POSITION = b.POSITION";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY a.CONSTRAINT_NAME ASC, a.POSITION ASC";
	else query = query + "\n" + "ORDER BY a.TABLE_NAME ASC, a.CONSTRAINT_NAME ASC, a.POSITION ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetCheckConstInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", TABLE_NAME AS \"table_name\", ";
	query = query + "CONSTRAINT_NAME AS \"check_const_name\", SEARCH_CONDITION AS \"check_value\", (CASE GENERATED WHEN 'GENERATED NAME' THEN 1 ELSE 0 END) AS \"is_system_named\"";
	query = query + "\n" + "FROM SYS.USER_CONSTRAINTS";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "' AND CONSTRAINT_TYPE = 'C' AND STATUS = 'ENABLED'";
		query = query + "\n" + "ORDER BY CONSTRAINT_NAME ASC";
	}
	else
	{
		query = query + "\n" + "WHERE CONSTRAINT_TYPE = 'C' AND STATUS = 'ENABLED'";
		query = query + "\n" + "ORDER BY TABLE_NAME ASC, CONSTRAINT_NAME ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetTriggerInfoQuery(_tstring tableName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", TABLE_NAME AS \"table_name\", TRIGGER_NAME AS \"trigger_name\"";
	query = query + "\n" + "FROM SYS.USER_TRIGGERS";

	if( tableName != "" )
	{
		query = query + "\n" + "WHERE TABLE_NAME = '" + tableName + "'";
		query = query + "\n" + "ORDER BY TRIGGER_NAME ASC";
	}
	else
	{
		query = query + "\n" + "ORDER BY TABLE_NAME ASC, TRIGGER_NAME ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetProcedureListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 2 AS \"object_type\", OBJECT_NAME AS \"object_name\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS";
	query = query + "\n" + " WHERE OBJECT_TYPE = 'PROCEDURE'";
	query = query + "\n" + "ORDER BY OBJECT_NAME ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetProcedureInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", OBJECT_NAME AS \"proc_name\", '' AS \"proc_comment\", TO_CHAR(CREATED, 'YYYY-MM-DD HH24:MI:SS') AS \"create_date\", TO_CHAR(LAST_DDL_TIME, 'YYYY-MM-DD HH24:MI:SS') AS \"modify_date\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS";

	if( procName != "" )
	{
		query = query + "\n" + "WHERE OBJECT_TYPE = 'PROCEDURE' AND OBJECT_NAME = '" + procName + "'";
	}
	else
	{
		query = query + "\n" + "WHERE OBJECT_TYPE = 'PROCEDURE'";
		query = query + "\n" + "ORDER BY OBJECT_NAME ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetProcedureParamInfoQuery(_tstring procName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.OBJECT_NAME AS \"proc_name\", b.SEQUENCE AS \"parameter_id\", ";
	query = query + "(CASE b.IN_OUT WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS \"param_mode\", b.ARGUMENT_NAME AS \"param_name\", ";
	query = query + "b.DATA_TYPE AS \"datatype\", (CASE b.DATA_TYPE WHEN 'NUMBER' THEN TO_CHAR(b.DATA_LENGTH) WHEN 'DATE' THEN ' ' ELSE TO_CHAR(b.DATA_LENGTH) END) AS \"max_length\", ";
	query = query + "b.DATA_PRECISION AS \"precision\", b.DATA_SCALE AS \"scale\", ";
	query = query + "b.DATA_TYPE || ";
	query = query + "(CASE";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_SCALE > 0 THEN '(' || TO_CHAR(b.DATA_PRECISION) || ',' || TO_CHAR(b.DATA_SCALE) || ')'";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_PRECISION > 0 AND b.DATA_SCALE = 0 THEN '(' || TO_CHAR(b.DATA_PRECISION) || ')'";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_PRECISION IS NULL THEN ''";
	query = query + " WHEN b.DATA_TYPE IN('CHAR', 'NCHAR', 'VARCHAR2', 'NVARCHAR2', 'DATE') THEN (CASE WHEN b.DATA_LENGTH > 0 THEN '(' || TO_CHAR(b.DATA_LENGTH) || ')' ELSE '' END)";
	query = query + " ELSE '' END";
	query = query + ") AS \"datatype_desc\", '' AS \"param_comment\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS a";
	query = query + "\n" + "INNER JOIN SYS.USER_ARGUMENTS b";
	query = query + "\n" + "ON a.OBJECT_ID = b.OBJECT_ID";

	if( procName != "" )
	{
		query = query + "\n" + "WHERE a.OBJECT_TYPE = 'PROCEDURE' AND a.OBJECT_NAME = '" + procName + "'";
		query = query + "\n" + "ORDER BY b.SEQUENCE ASC";
	}
	else
	{
		query = query + "\n" + "WHERE a.OBJECT_TYPE = 'PROCEDURE'";
		query = query + "\n" + "ORDER BY a.OBJECT_NAME ASC, b.SEQUENCE ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetFunctionListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 3 AS \"object_type\", OBJECT_NAME AS \"object_name\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS";
	query = query + "\n" + " WHERE OBJECT_TYPE = 'FUNCTION'";
	query = query + "\n" + "ORDER BY OBJECT_NAME ASC";

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetFunctionInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", OBJECT_NAME AS \"func_name\", '' AS \"func_comment\", TO_CHAR(CREATED, 'YYYY-MM-DD HH24:MI:SS') AS \"create_date\", TO_CHAR(LAST_DDL_TIME, 'YYYY-MM-DD HH24:MI:SS') AS \"modify_date\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS";

	if( funcName != "" )
	{
		query = query + "\n" + "WHERE OBJECT_TYPE = 'FUNCTION' AND OBJECT_NAME = '" + funcName + "'";
	}
	else
	{
		query = query + "\n" + "WHERE OBJECT_TYPE = 'FUNCTION'";
		query = query + "\n" + "ORDER BY OBJECT_NAME ASC";
	}

	return query;
}

//***************************************************************************
//
inline _tstring ORACLEGetFunctionParamInfoQuery(_tstring funcName = _T(""))
{
	_tstring query = _T("");

	query = query + "SELECT (SELECT DEFAULT_TABLESPACE FROM USER_USERS) AS \"db_name\", 0 AS \"object_id\", USER AS \"schema_name\", a.OBJECT_NAME AS \"proc_name\", b.SEQUENCE AS \"parameter_id\", ";
	query = query + "(CASE b.IN_OUT WHEN 'IN' THEN 1 WHEN 'OUT' THEN 2 ELSE 0 END) AS \"param_mode\", b.ARGUMENT_NAME AS \"param_name\", ";
	query = query + "b.DATA_TYPE AS \"datatype\", (CASE b.DATA_TYPE WHEN 'NUMBER' THEN TO_CHAR(b.DATA_LENGTH) WHEN 'DATE' THEN ' ' ELSE TO_CHAR(b.DATA_LENGTH) END) AS \"max_length\", ";
	query = query + "b.DATA_PRECISION AS \"precision\", b.DATA_SCALE AS \"scale\", ";
	query = query + "b.DATA_TYPE || ";
	query = query + "(CASE";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_SCALE > 0 THEN '(' || TO_CHAR(b.DATA_PRECISION) || ',' || TO_CHAR(b.DATA_SCALE) || ')'";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_PRECISION > 0 AND b.DATA_SCALE = 0 THEN '(' || TO_CHAR(b.DATA_PRECISION) || ')'";
	query = query + " WHEN b.DATA_TYPE = 'NUMBER' AND b.DATA_PRECISION IS NULL THEN ''";
	query = query + " WHEN b.DATA_TYPE IN('CHAR', 'NCHAR', 'VARCHAR2', 'NVARCHAR2', 'DATE') THEN (CASE WHEN b.DATA_LENGTH > 0 THEN '(' || TO_CHAR(b.DATA_LENGTH) || ')' ELSE '' END)";
	query = query + " ELSE '' END";
	query = query + ") AS \"datatype_desc\", '' AS \"param_comment\"";
	query = query + "\n" + "FROM SYS.USER_OBJECTS a";
	query = query + "\n" + "INNER JOIN SYS.USER_ARGUMENTS b";
	query = query + "\n" + "ON a.OBJECT_ID = b.OBJECT_ID";

	if( funcName != "" )
	{
		query = query + "\n" + "WHERE a.OBJECT_TYPE = 'FUNCTION' AND a.OBJECT_NAME = '" + funcName + "'";
		query = query + "\n" + "ORDER BY b.SEQUENCE ASC";
	}
	else
	{
		query = query + "\n" + "WHERE a.OBJECT_TYPE = 'FUNCTION'";
		query = query + "\n" + "ORDER BY a.OBJECT_NAME ASC, b.SEQUENCE ASC";
	}

	return query;
}

#endif // ndef __DBORACLEQUERY_H__