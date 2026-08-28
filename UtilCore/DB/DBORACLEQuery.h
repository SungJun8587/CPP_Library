
//***************************************************************************
// DBORACLEQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef UC_DBORACLEQUERY_H
#define UC_DBORACLEQUERY_H

//***************************************************************************
// @brief ORACLE 인덱스타입 : ALL_INDEXES 테이블 INDEX_TYPE(VARCHAR2(27)) 컬럼
//***************************************************************************
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

//***************************************************************************
// @brief EORACLEIndexType 열거형 값을 문자열 형태로 변환
// @param v 변환할 ORACLE 인덱스 타입 열거형 값
// @return 인덱스 타입에 대응하는 문자열 (TCHAR*)
//***************************************************************************
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

//***************************************************************************
// @brief 문자열 형태의 인덱스 타입을 EORACLEIndexType 열거형 값으로 변환
// @param ptszIndexType 변환할 인덱스 타입 문자열
// @return 대응하는 EORACLEIndexType 열거형 값 (매칭되지 않을 경우 NONE)
//***************************************************************************
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
// @brief ORACLE 인덱스 조각화 정보
// @details BLEVEL이 4이상(HIGH LEVEL)인 경우 REBUILD 대상으로 판단
//***************************************************************************
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
// @brief ORACLE 인덱스 조각화 정보
// @details PCT_DELETED가 20%이상으로 나타나면 인덱스는 REBUILD 대상으로 판단 / DISTINCTIVENESS 컬럼은 인덱스가 만들어진 컬럼의 값이 얼마나 자주 반복되는지를 보여주는 값(해당 값이 99%이상이면 BITMAP INDEX 대상)
//***************************************************************************
class ORACLE_INDEX_STAT_FRAGMENTATION
{
public:
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };				// 인덱스 명
	float   PctDeleted;														// 인덱스에 전체 리프 행의 수에서 삭제된 리프 행의 수 비율(삭제된 리프 행의 수 / 전체 리프 행의 수 * 100) 
	float	Distinctiveness;												// 인덱스가 만들어진 컬럼의 값이 얼마나 자주 반복되는지를 보여주는 값
};

//***************************************************************************
// @brief ORACLE 인덱스 조각화 정보
//***************************************************************************
class ORACLE_TABLE_IDENTITY_COLUMN
{
public:
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// 스키마 명
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// 테이블 명
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// 컬럼 명
	TCHAR   tszIdentityColumn[4];							// 컬럼이 Identity인지 여부(YES/NO : 유/무)
	TCHAR   tszDefaultOnNull[4];							// 컬럼이 DEFAULT ON NULL 의미를 가지는지 여부(YES/NO : 유/무)
	TCHAR	tszGenerationType[32];							// ID 열의 생성 유형 (ALWAYS / BY DEFAULT / BY DEFAULT ON NULL)
	TCHAR   tszSequenceName[DATABASE_OBJECT_NAME_STRLEN];	// ID 열과 연관된 시퀀스 이름(SYS.USER_SEQUENCES 테이블 SEQUENCE_NAME 컬럼 참조)
	uint64	MinValue;										// 시퀀스의 최소값
	uint64	MaxValue;										// 시퀀스의 최대값
	uint64	IncrementBy;									// 시퀀스가 증가되는 값
	TCHAR   tszCycleFlag[2];								// 한계에 도달하면 시퀀스가 순환 여부(Y/N : 유/무)
	TCHAR   tszOrderFlag[2];								// 시퀀스 번호가 순서대로 생성되는지 여부(Y/N : 유/무)
	uint64	CacheSize;										// 캐시할 시퀀스 번호 수
	uint64	LastNumber;										// 디스크에 기록된 마지막 시퀀스 번호
	TCHAR   tszScaleFlag[2];								// 확장 가능한 시퀀스인지 여부(Y/N : 유/무)
	TCHAR   tszExtendFlag[2];								// 이 확장 가능한 시퀀스의 생성된 값이 MAX_VALUE 또는 MIN_VALUE를 초과하는지 여부(Y/N : 유/무)
	TCHAR	tszShardedFlag[2];								// 이것이 분할된 시퀀스인지 여부(Y/N : 유/무)
	TCHAR   tszSessionFlag[2];								// 시퀀스 값이 세션 전용인지 여부(Y/N : 유/무)
	TCHAR   tszKeepValue[2];								// 시퀀스 값이 실패후 재생 중에 유지되는지 여부(Y/N : 유/무)
};

//***************************************************************************
// @brief ORACLE DB 테이블 아이덴티티 컬럼 정보를 관리하는 클래스
//***************************************************************************
class ORACLEDBTableIdentityColumn
{
public:
	_tstring	SchemaName;			// 스키마 명
	_tstring	TableName;			// 테이블 명
	_tstring	ColumnName;			// 컬럼 명
	_tstring	IdentityColumn;		// 컬럼이 Identity인지 여부
	_tstring	DefaultOnNull;		// 컬럼이 DEFAULT ON NULL 의미를 가지는지 여부
	_tstring	GenerationType;		// ID 열의 생성 유형
	_tstring	SequenceName;		// ID 열과 연관된 시퀀스 이름
	uint64		MinValue;			// 시퀀스의 최소값
	uint64		MaxValue;			// 시퀀스의 최대값
	uint64		IncrementBy;		// 시퀀스가 증가되는 값
	_tstring	CycleFlag;			// 한계 도달 시 순환 여부
	_tstring	OrderFlag;			// 시퀀스 번호 순서 생성 여부
	uint64		CacheSize;			// 캐시할 시퀀스 번호 수
	uint64		LastNumber;			// 디스크에 기록된 마지막 시퀀스 번호
	_tstring	ScaleFlag;			// 확장 가능한 시퀀스 여부
	_tstring	ExtendFlag;			// 생성된 값이 범위 초과 여부
	_tstring	ShardedFlag;		// 분할된 시퀀스 여부
	_tstring	SessionFlag;		// 세션 전용 시퀀스 여부
	_tstring	KeepValue;			// 재생 중 유지 여부
};

//***************************************************************************
// @brief ORACLE 테이블 컬럼 옵션 구문 생성
// @param dataTypeDesc 데이터 타입 설명
// @param isNullable Null 허용 여부
// @param defaultDefinition 기본값 정의
// @param isIdentity Identity 컬럼 여부
// @param pdbTabIdentityCols Identity 컬럼 상세 정보 포인터
// @return 생성된 컬럼 옵션 SQL 구문 문자열
//***************************************************************************
inline _tstring ORACLEGetTableColumnOption(_tstring dataTypeDesc, bool isNullable, _tstring defaultDefinition, bool isIdentity, ORACLEDBTableIdentityColumn* pdbTabIdentityCols)
{
	_tstring columnOption = _T("");

	columnOption = dataTypeDesc;

	if( isIdentity && pdbTabIdentityCols != NULL )
	{
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
// @brief 제약 조건 삭제 쿼리 생성
// @param tableName 테이블 명
// @param constType 제약 조건 타입 (P, U, R, C 등)
// @param constName 제약 조건 명
// @return 제약 조건 삭제 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetDropConstraintQuery(_tstring tableName, _tstring constType, _tstring constName)
{
	_tstring query = _T("");

	if( constType == "P" || constType == "U" || constType == "R" || constType == "C" )
	{
		query = tstring_tcformat(_T("ALTER TABLE %s DROP CONSTRAINT %s"), tableName.c_str(), constName.c_str());
	}
	return query;
}

//***************************************************************************
// @brief DB 시스템 정보 조회 쿼리 생성
// @return DB 시스템 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 사용자 목록 조회 쿼리 생성
// @return 사용자 목록 조회 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetUserListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT USERNAME AS \"name\" FROM ALL_USERS";
	return query;
}

//***************************************************************************
// @brief 테이블스페이스 목록 조회 쿼리 생성
// @return 테이블스페이스 목록 조회 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetTableSpaceListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT TABLESPACE_NAME AS \"name\" FROM DBA_DATA_FILES";
	return query;
}

//***************************************************************************
// @brief 데이터베이스 목록 조회 쿼리 생성
// @return 데이터베이스 목록 조회 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetDatabaseListQuery()
{
	_tstring query = _T("");

	query = query + "SELECT NAME AS \"name\" FROM V$DATABASE";
	return query;
}

//***************************************************************************
// @brief 테이블 또는 컬럼 코멘트 처리 쿼리 생성
// @param tableName 테이블 명
// @param setComment 설정할 주석 내용
// @param columnName 컬럼 명 (기본값: 빈 문자열, 입력 시 컬럼 주석 생성)
// @return 코멘트 설정 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEProcessTableColumnCommentQuery(_tstring tableName, _tstring setComment, _tstring columnName = _T(""))
{
	_tstring query = _T("");

	if( columnName != "" )
		query = query + "COMMENT ON TABLE " + tableName + " IS '" + setComment + "'";
	else query = query + "COMMENT ON COLUMN " + tableName + "." + columnName + " IS '" + setComment + "'";

	return query;
}

//***************************************************************************
// @brief 객체 DDL 추출 쿼리 생성 (DBMS_METADATA.GET_DDL)
// @param dbObjectType DB 객체 유형 (테이블, 인덱스 등)
// @param objectName 객체 명
// @param schemaName 스키마 명 (기본값: 빈 문자열)
// @return DDL 추출 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEMetaDataGetDDLQuery(EDBObjectType dbObjectType, _tstring objectName, _tstring schemaName = _T(""))
{
	_tstring query = _T("");

	if( schemaName != "" )
		query = tstring_tcformat(_T("SELECT DBMS_METADATA.GET_DDL('%s', '%s', '%s') SCRIPT FROM DUAL"), ToString(dbObjectType), objectName.c_str(), schemaName.c_str());
	else query = tstring_tcformat(_T("SELECT DBMS_METADATA.GET_DDL('%s, '%s') SCRIPT FROM DUAL"), ToString(dbObjectType), objectName.c_str());

	return query;
}

//***************************************************************************
// @brief 테이블 인덱스 DDL 메타데이터 조회 쿼리 생성
// @param tableName 테이블 명
// @param indexName 인덱스 명 (기본값: 빈 문자열)
// @param schemaName 스키마 명 (기본값: 빈 문자열)
// @return 인덱스 DDL 메타데이터 조회 SQL 쿼리
//***************************************************************************
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
// @brief 테이블 외래키 생성 SQL 쿼리 구문 추출
// @param tableName 테이블 명
// @param constraintName 제약 조건 명 (기본값: 빈 문자열)
// @return 외래키 생성 쿼리문 출력 SQL
//***************************************************************************
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
// @brief 테이블 및 컬럼 코멘트 생성 SQL 쿼리 구문 추출
// @param tableName 테이블 명
// @return 코멘트 생성 쿼리문 출력 SQL
//***************************************************************************
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
// @brief 객체(PROCEDURE, FUNCTION, TRIGGER 등) 소스 코드 조회 쿼리 생성
// @param dbObjectType DB 객체 유형
// @param objectName 객체 명
// @return 소스 코드 조회 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetUserSourceQuery(EDBObjectType dbObjectType, _tstring objectName)
{
	_tstring query = tstring_tcformat(_T("SELECT TEXT FROM SYS.USER_SOURCE WHERE TYPE = '%s' AND NAME = '%s'"), ToString(dbObjectType), objectName.c_str());
	return query;
}

//***************************************************************************
// @brief 인덱스 조각화 분석용 통계 생성 쿼리 (Step 1)
// @param indexName 분석할 인덱스 명
// @return 인덱스 분석 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetAnalyzeIndexFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	query = query + "ANALYZE INDEX " + indexName + " COMPUTE STATISTICS";
	return query;
}

//***************************************************************************
// @brief 인덱스 B-Tree 깊이(BLEVEL) 기반 조각화 점검 쿼리 (Step 2)
// @param indexName 인덱스 명
// @return 조각화 점검 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetIndexFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	query = query + "SELECT TABLE_NAME AS \"table_name\", INDEX_NAME AS \"index_name\", BLEVEL AS \"blevel\", DECODE(BLEVEL, 0, 'OK BLEVEL', 1, 'OK BLEVEL', 2, 'OK BLEVEL', 3, 'OK BLEVEL', 4, 'OK BLEVEL', 'BLEVEL HIGH') \"ok?\", ";
	query = query + "\n" + "TO_CHAR(LAST_ANALYZED, 'yyyy-mm-dd hh24:mi:ss') AS \"last_analyzed\"";
	query = query + "\n" + "FROM SYS.USER_INDEXES";
	query = query + "\n" + "WHERE INDEX_NAME = '" + indexName + "'";
	query = query + "\n" + "ORDER BY BLEVEL DESC";
	return query;
}

//***************************************************************************
// @brief INDEX_STATS 테이블 구조 검증 통계 생성 쿼리 (Step 1)
// @param indexName 분석할 인덱스 명
// @return 구조 검증 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetAnalyzeIndexStatFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	query = query + "ANALYZE INDEX " + indexName + " VALIDATE STRUCTURE";
	return query;
}

//***************************************************************************
// @brief 인덱스 삭제 비율(PCT_DELETED) 및 카디널리티 조각화 점검 쿼리 (Step 2)
// @param indexName 인덱스 명
// @return 삭제 비율 및 조각화 점검 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetIndexStatFragmentationCheckQuery(_tstring indexName)
{
	_tstring query = _T("");

	query = query + "SELECT NAME AS \"index_name\", DEL_LF_ROWS * 100 / DECODE(LF_ROWS, 0, 1, LF_ROWS) AS \"PCT_DELETED\", ";
	query = query + "\n" + "(LF_ROWS - DISTINCT_KEYS) * 100 / DECODE(LF_ROWS, 0, 1, LF_ROWS) AS \"distinctiveness\"";
	query = query + "\n" + "FROM SYS.INDEX_STATS";
	query = query + "\n" + "WHERE NAME = '" + indexName + "'";
	query = query + "\n" + "ORDER BY PCT_DELETED DESC";
	return query;
}

//***************************************************************************
// @brief 인덱스 REBUILD 쿼리 생성
// @param indexName 인덱스 명
// @return 인덱스 재구축 SQL 쿼리
//***************************************************************************
inline _tstring ORACLEGetIndexRebuildQuery(_tstring indexName)
{
	_tstring query = _T("");

	query = query + "ALTER INDEX " + indexName + " REBUILD";
	return query;
}

//***************************************************************************
// @brief 테이블 목록 조회 쿼리 생성
// @return 테이블 목록 조회 SQL 쿼리
//***************************************************************************
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
// @brief 테이블 기본 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열, 전체 테이블 대상)
// @return 테이블 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 테이블 컬럼 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 컬럼 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 테이블 Identity 컬럼 상세 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return Identity 컬럼 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 제약 조건 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 제약 조건 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 인덱스 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 인덱스 상세 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 파티션 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 파티션 및 서브파티션 상세 정보 조회 SQL 쿼리
//***************************************************************************
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
	query = query + "\n" + "ON a.TABLE_NAME = c.PARTITION_NAME = c.PARTITION_NAME";

	if( tableName != "" )
		query = query + "\n" + "ORDER BY b.PARTITION_NAME ASC, b.PARTITION_POSITION ASC, c.SUBPARTITION_POSITION ASC, a.COLUMN_POSITION ASC";
	else query = query + "\n" + "ORDER BY a.TABLE_NAME ASC, b.PARTITION_NAME ASC, b.PARTITION_POSITION ASC, c.SUBPARTITION_POSITION ASC, a.COLUMN_POSITION ASC";

	return query;
}

//***************************************************************************
// @brief 외래키 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 외래키 상세 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief CHECK 제약 조건 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return CHECK 제약 조건 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 트리거 목록 정보 조회 쿼리 생성
// @param tableName 테이블 명 (기본값: 빈 문자열)
// @return 트리거 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 프로시저 목록 조회 쿼리 생성
// @return 프로시저 목록 조회 SQL 쿼리
//***************************************************************************
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
// @brief 프로시저 정보 조회 쿼리 생성
// @param procName 프로시저 명 (기본값: 빈 문자열)
// @return 프로시저 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 프로시저 매개변수 정보 조회 쿼리 생성
// @param procName 프로시저 명 (기본값: 빈 문자열)
// @return 프로시저 매개변수 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 함수 목록 조회 쿼리 생성
// @return 함수 목록 조회 SQL 쿼리
//***************************************************************************
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
// @brief 함수 정보 조회 쿼리 생성
// @param funcName 함수 명 (기본값: 빈 문자열)
// @return 함수 정보 조회 SQL 쿼리
//***************************************************************************
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
// @brief 함수 매개변수 정보 조회 쿼리 생성
// @param funcName 함수 명 (기본값: 빈 문자열)
// @return 함수 매개변수 정보 조회 SQL 쿼리
//***************************************************************************
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
	query = query + " WHEN b.DATA_TYPE IN('CHAR', 'NCHAR', 'VARCHAR2', 'NVARCHAR2', 'DATE') THEN (CASE WHEN b.DATA_LENGTH > 0 THEN '(' || TO_CHAR(a.DATA_LENGTH) || ')' ELSE '' END)";
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

#endif // ndef UC_DBORACLEQUERY_H