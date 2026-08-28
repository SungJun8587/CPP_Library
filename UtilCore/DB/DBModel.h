
//***************************************************************************
// DBModel.h : interface for the Database Model.
//
//***************************************************************************

#ifndef UC_DBMODEL_H
#define UC_DBMODEL_H

#include <regex>

NAMESPACE_BEGIN(DBModel)

USING_SHARED_PTR(Column);
USING_SHARED_PTR(Constraint);
USING_SHARED_PTR(IdentityColumn);
USING_SHARED_PTR(IndexColumn);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(IndexOption);
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(DefaultConstraint);
USING_SHARED_PTR(CheckConstraint);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

//***************************************************************************
// @brief 데이터베이스 컬럼 모델 클래스
// @detail 데이터베이스 테이블의 컬럼 속성 정보 및 DDL 생성/수정/삭제 쿼리 생성 기능을 제공합니다.
//***************************************************************************
class Column
{
public:
	//***************************************************************************
	// @brief Column 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	Column(EDBClass dbClass) { _dbSystemInfo.DBClass = dbClass; _pORACLEDBTableIdentityCols = NULL; };

	//***************************************************************************
	// @brief Column 생성자입니다.
	// @param dbSystemInfo 데이터베이스 시스템 정보
	// @param pORACLEDBTableIdentityCols 오라클 테이블 아이덴티티 컬럼 정보 포인터
	//***************************************************************************
	Column(DB_SYSTEM_INFO dbSystemInfo, ORACLEDBTableIdentityColumn* pORACLEDBTableIdentityCols = NULL) { _dbSystemInfo = dbSystemInfo; _pORACLEDBTableIdentityCols = pORACLEDBTableIdentityCols; };

	_tstring			CreateColumn();
	_tstring			AlterColumn();
	_tstring			DropColumn();
	_tstring			ChangeColumnName(const _tstring chgColumnName);
	_tstring			CreateColumnText();

public:
	_tstring			_schemaName;                   // 스키마 이름
	_tstring			_tableName;                    // 테이블 이름
	_tstring			_seq;                          // 컬럼 순번
	_tstring			_columnName;                   // 컬럼 이름
	_tstring			_columnComment;                // 컬럼 주석(설명)
	_tstring			_datatype;                     // 데이터 타입
	int64				_maxLength = 0;                // 최대 길이
	uint8				_precision = 0;                // 정밀도
	uint8				_scale = 0;                    // 소수점 자리수
	_tstring			_datatypedesc;                 // 데이터 타입 설명
	bool				_nullable = false;             // NULL 허용 여부
	bool				_identity = false;             // Identity(자동 증가) 여부
	_tstring			_identitydesc;                 // Identity 설명
	int64				_seedValue = 0;                // Identity 시작값
	int64				_incrementValue = 0;           // Identity 증가값
	_tstring			_characterset;                 // 문자집합
	_tstring			_collation;                    // 콜레이션
	_tstring			_defaultConstraintName;        // 기본값 제약조건 이름
	_tstring			_defaultDefinition;            // 기본값 정의

	DB_SYSTEM_INFO				_dbSystemInfo;              // DB 시스템 정보
	ORACLEDBTableIdentityColumn* _pORACLEDBTableIdentityCols; // 오라클 전용 Identity 컬럼 정보
};

//***************************************************************************
// @brief 데이터베이스 제약조건 모델 클래스
// @detail 제약조건(Constraint)의 기본 속성을 관리합니다.
//***************************************************************************
class Constraint
{
public:
	//***************************************************************************
	// @brief Constraint 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	Constraint(EDBClass dbClass) { _dbClass = dbClass; };

public:
	EDBClass			_dbClass;               // 데이터베이스 종류
	_tstring			_schemaName;            // 스키마 이름
	_tstring			_tableName;             // 테이블 이름
	_tstring			_constName;             // 제약조건 이름
	_tstring			_constType;             // 제약조건 타입
	_tstring			_constTypeDesc;         // 제약조건 타입 설명
	_tstring			_constValue;            // 제약조건 값
	bool				_systemNamed = false;   // 시스템 생성 이름 여부
	bool				_status = false;        // 활성화 상태
	_tstring			_sortValue;             // 정렬 값
};

//***************************************************************************
// @brief 데이터베이스 Identity(자동 증가) 컬럼 모델 클래스
// @detail Identity 컬럼의 시퀀스 및 옵션 정보를 보관합니다.
//***************************************************************************
class IdentityColumn
{
public:
	_tstring			_schemaName;            // 스키마 이름
	_tstring			_tableName;             // 테이블 이름
	_tstring			_columnName;            // 컬럼 이름
	_tstring			_identityColumn;        // Identity 컬럼 속성
	_tstring			_defaultOnNull;         // NULL 시 기본값 적용 여부
	_tstring			_generationType;        // 생성 유형
	_tstring			_sequenceName;          // 시퀀스 이름
	uint64				_minValue;              // 최소값
	uint64				_maxValue;              // 최대값
	uint64				_incrementBy;           // 증가값
	_tstring			_cycleFlag;             // 순환 플래그
	_tstring			_orderFlag;             // 순서 보장 플래그
	uint64				_cacheSize;             // 캐시 크기
	uint64				_lastNumber;            // 마지막 발급 번호
	_tstring			_scaleFlag;             // 스케일 플래그
	_tstring			_extendFlag;            // 확장 플래그
	_tstring			_shardedFlag;           // 샤딩 플래그
	_tstring			_sessionFlag;           // 세션 플래그
	_tstring			_keepValue;             // 값 유지 플래그
};

//***************************************************************************
// @brief 인덱스 구성 컬럼 모델 클래스
// @detail 인덱스에 포함된 컬럼의 정렬 방식 및 순서 정보를 보관합니다.
//***************************************************************************
class IndexColumn
{
public:
	_tstring	GetSortText();

public:
	_tstring	_seq;                   // 컬럼 순번
	_tstring	_columnName;            // 컬럼 이름
	EIndexSort	_sort = EIndexSort::ASC; // 정렬 방식 (ASC/DESC)
};

//***************************************************************************
// @brief 인덱스 상세 옵션 모델 클래스
// @detail 물리적 저장 옵션, 압축 옵션, 락 옵션 등 인덱스의 상세 속성을 관리합니다.
//***************************************************************************
class IndexOption
{
public:
	_tstring				_schemaName;                        // 스키마 이름
	_tstring				_tableName;                         // 테이블 이름
	_tstring				_indexName;                         // 인덱스 이름
	bool					_primaryKey = false;                // 기본키 여부
	bool					_uniqueKey = false;                 // 유니크키 여부
	bool					_isDisabled = false;                // 비활성화 여부
	bool					_isPadded = false;                  // 패딩 적용 여부
	int8					_fillFactor;                        // Fill Factor 값
	bool					_ignoreDupKey = false;              // 중복 키 무시 여부
	bool					_allowRowLocks = false;             // 행 잠금 허용 여부
	bool					_allowPageLocks = false;            // 페이지 잠금 허용 여부
	bool					_hasFilter = false;                 // 필터 조건 존재 여부
	_tstring				_filterDefinition;                  // 필터 정의
	int32					_compressionDelay;                  // 압축 지연 시간
	_tstring				_optimizeForSequentialKey;          // 순차 키 최적화 옵션
	bool					_statisticsNoRecompute = false;     // 통계 자동 재계산 안함 여부
	bool					_statisticsIncremental = false;     // 증분 통계 여부
	int8					_dataCompression;                   // 데이터 압축 방식
	_tstring				_dataCompressionDesc;               // 데이터 압축 방식 설명
	bool					_xmlCompression = false;            // XML 압축 여부
	_tstring				_xmlCompressionDesc;                // XML 압축 설명
	_tstring				_fileGroupOrPartitionScheme;        // 파일그룹 또는 파티션 스키마 타입
	_tstring				_fileGroupOrPartitionSchemeName;    // 파일그룹 또는 파티션 스키마 이름
};

//***************************************************************************
// @brief 데이터베이스 인덱스 모델 클래스
// @detail 인덱스 정보 관리 및 생성/삭제 DDL 쿼리 생성 기능을 제공합니다.
//***************************************************************************
class Index
{
public:
	//***************************************************************************
	// @brief Index 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	Index(EDBClass dbClass) { _dbClass = dbClass; };

	_tstring			CreateIndex();
	_tstring			DropIndex();

	_tstring			GetIndexName();
	EIndexKind			GetStringToIndexKind();
	_tstring			GetKeyText();
	_tstring			ChangeIndexName(const _tstring chgIndexName);
	_tstring			CreateColumnsText(bool isOrderBy = true);
	bool				DependsOn(const _tstring& columnName);

public:
	EDBClass				_dbClass;                        // 데이터베이스 종류
	_tstring				_schemaName;                     // 스키마 이름
	_tstring				_tableName;                      // 테이블 이름
	_tstring				_indexName;                      // 인덱스 이름
	int32					_indexId = 0;                    // 인덱스 ID
	EIndexKind				_kind = EIndexKind::NONCLUSTERED; // 인덱스 종류
	_tstring				_type;                           // 인덱스 타입 문자열
	bool					_primaryKey = false;             // 기본키 여부
	bool					_uniqueKey = false;              // 유니크키 여부
	bool					_systemNamed = false;            // 시스템 생성 이름 여부
	CVector<IndexColumnRef>	_columns;                        // 인덱스 구성 컬럼 목록
};

//***************************************************************************
// @brief 외래키(Foreign Key) 모델 클래스
// @detail 참조 테이블/컬럼 관계 관리 및 외래키 제약조건 DDL 생성 기능을 제공합니다.
//***************************************************************************
class ForeignKey
{
public:
	//***************************************************************************
	// @brief ForeignKey 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	ForeignKey(EDBClass dbClass) { _dbClass = dbClass; };

	_tstring			CreateForeignKey();
	_tstring			AlterForeignKey();
	_tstring			AlterForeignKeyCheckConstraint();
	_tstring			DropForeignKey();

	_tstring			GetForeignKeyName();
	_tstring			CreateColumnsText(CVector<IndexColumnRef> columns);

public:
	EDBClass				_dbClass;                   // 데이터베이스 종류
	_tstring				_schemaName;                // 스키마 이름
	_tstring				_tableName;                 // 테이블 이름
	_tstring				_foreignKeyName;            // 외래키 제약조건 이름
	bool					_isDisabled = false;        // 비활성화 여부
	bool					_isNotTrusted = false;      // Trusted 여부
	_tstring				_updateRule;                // UPDATE 규칙 (CASCADE 등)
	_tstring				_deleteRule;                // DELETE 규칙 (CASCADE 등)
	bool					_systemNamed = false;       // 시스템 생성 이름 여부

	_tstring				_foreignKeyTableName;       // 외래키 테이블 이름
	CVector<IndexColumnRef>	_foreignKeyColumns;         // 외래키 컬럼 목록
	_tstring				_referenceKeySchemaName;    // 참조 스키마 이름
	_tstring				_referenceKeyTableName;     // 참조 테이블 이름
	CVector<IndexColumnRef>	_referenceKeyColumns;       // 참조 컬럼 목록
};

//***************************************************************************
// @brief 기본값 제약조건(Default Constraint) 모델 클래스
// @detail 기본값 제약조건 DDL 생성 및 삭제 기능을 제공합니다.
//***************************************************************************
class DefaultConstraint
{
public:
	//***************************************************************************
	// @brief DefaultConstraint 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	DefaultConstraint(EDBClass dbClass) { _dbClass = dbClass; };

	EDBClass				_dbClass;               // 데이터베이스 종류
	_tstring				_schemaName;            // 스키마 이름
	_tstring				_tableName;             // 테이블 이름
	_tstring				_defaultConstName;      // 기본값 제약조건 이름
	_tstring				_columnName;            // 대상 컬럼 이름
	_tstring				_defaultValue;          // 기본값 정의
	bool					_systemNamed = false;   // 시스템 생성 이름 여부

public:
	_tstring			CreateDefaultConstraint();
	_tstring			DropDefaultConstraint();
};

//***************************************************************************
// @brief 체크 제약조건(Check Constraint) 모델 클래스
// @detail 체크 제약조건 DDL 생성 및 삭제 기능을 제공합니다.
//***************************************************************************
class CheckConstraint
{
public:
	//***************************************************************************
	// @brief CheckConstraint 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	CheckConstraint(EDBClass dbClass) { _dbClass = dbClass; };

	EDBClass				_dbClass;               // 데이터베이스 종류
	_tstring				_schemaName;            // 스키마 이름
	_tstring				_tableName;             // 테이블 이름
	_tstring				_checkConstName;        // 체크 제약조건 이름
	_tstring				_checkValue;            // 체크 조건식
	bool					_systemNamed = false;   // 시스템 생성 이름 여부

public:
	_tstring			CreateCheckConstraint();
	_tstring			DropCheckConstraint();
};

//***************************************************************************
// @brief 데이터베이스 트리거(Trigger) 모델 클래스
// @detail 트리거 객체의 정의 및 본문 정보를 관리합니다.
//***************************************************************************
class Trigger
{
public:
	int32			_objectId = 0;      // 개체 ID
	_tstring		_schemaName;        // 스키마 이름
	_tstring		_tableName;         // 테이블 이름
	_tstring		_triggerName;       // 트리거 이름
	_tstring		_fullBody;          // 트리거 전체 DDL 문장
};

//***************************************************************************
// @brief 데이터베이스 테이블(Table) 모델 클래스
// @detail 컬럼, 제약조건, 인덱스, 외래키 등 테이블의 하위 모델 요소를 포함하며 테이블 DDL 제어 기능을 제공합니다.
//***************************************************************************
class Table
{
public:
	//***************************************************************************
	// @brief Table 생성자입니다.
	// @param dbClass 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	Table(EDBClass dbClass) { _dbClass = dbClass; }

	_tstring			CreateTable();
	_tstring			AlterTableCollationEngine();
	_tstring			DropTable();
	_tstring			ChangeTableName(const _tstring chgTableName);
	ColumnRef			FindColumn(const _tstring& columnName);

public:
	EDBClass					_dbClass;               // 데이터베이스 종류
	int32						_objectId = 0;          // 개체 ID
	_tstring					_schemaName;            // 스키마 이름
	_tstring					_tableName;             // 테이블 이름
	_tstring					_tableComment;          // 테이블 주석(설명)
	_tstring					_auto_increment_value;  // AUTO_INCREMENT 초기/현재 값
	_tstring					_storageEngine;         // 스토리지 엔진 (MySQL 등)
	_tstring					_characterset;          // 문자집합
	_tstring					_collation;             // 콜레이션
	_tstring					_createDate;            // 생성 일시
	_tstring					_modifyDate;            // 수정 일시

	CVector<ColumnRef>				_columns;           // 컬럼 목록
	CVector<ConstraintRef>			_constraints;       // 제약조건 목록
	CVector<IdentityColumnRef>		_identityColumns;   // Identity 컬럼 목록
	CVector<IndexRef>				_indexes;           // 인덱스 목록
	CVector<IndexOptionRef>			_indexOptions;      // 인덱스 옵션 목록
	CVector<ForeignKeyRef>			_foreignKeys;       // 외래키 목록
	CVector<DefaultConstraintRef>	_defaultConstraints;// 기본값 제약조건 목록
	CVector<CheckConstraintRef>     _checkConstraints;  // 체크 제약조건 목록
};

//***************************************************************************
// @brief 저장 프로시저 파라미터 모델 클래스
// @detail 저장 프로시저의 매개변수 속성을 관리합니다.
//***************************************************************************
class ProcParam
{
public:
	_tstring			_paramId;               // 파라미터 ID
	EParameterMode		_paramMode;             // 파라미터 모드 (IN, OUT, RET)
	_tstring			_paramName;             // 파라미터 이름
	_tstring			_datatype;              // 데이터 타입
	uint64				_maxLength = 0;         // 최대 길이
	uint8				_precision = 0;         // 정밀도
	uint8				_scale = 0;             // 소수점 자리수
	_tstring			_datatypedesc;          // 데이터 타입 설명
	_tstring			_paramComment;          // 파라미터 주석(설명)
};

//***************************************************************************
// @brief 저장 프로시저(Stored Procedure) 모델 클래스
// @detail 프로시저 정의, 파라미터 및 DDL 쿼리 생성 기능을 제공합니다.
//***************************************************************************
class Procedure
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;      // 개체 ID
	_tstring					_schemaName;        // 스키마 이름
	_tstring					_procName;          // 프로시저 이름
	_tstring					_procComment;       // 프로시저 주석(설명)
	_tstring					_fullBody;          // 전체 CREATE DDL 문장
	_tstring					_body;              // 프로시저 본문
	_tstring					_createDate;        // 생성 일시
	_tstring					_modifyDate;        // 수정 일시
	CVector<ProcParamRef>		_parameters;        // 파라미터 목록
};

//***************************************************************************
// @brief 함수 파라미터 모델 클래스
// @detail 사용자 정의 함수의 매개변수 속성을 관리합니다.
//***************************************************************************
class FuncParam
{
public:
	_tstring			_paramId;               // 파라미터 ID
	EParameterMode		_paramMode;             // 파라미터 모드 (IN, OUT, RET)
	_tstring			_paramName;             // 파라미터 이름
	_tstring			_datatype;              // 데이터 타입
	uint64				_maxLength = 0;         // 최대 길이
	uint8				_precision = 0;         // 정밀도
	uint8				_scale = 0;             // 소수점 자리수
	_tstring			_datatypedesc;          // 데이터 타입 설명
	_tstring			_paramComment;          // 파라미터 주석(설명)
};

//***************************************************************************
// @brief 사용자 정의 함수(Function) 모델 클래스
// @detail 함수 정의, 파라미터 및 DDL 쿼리 생성 기능을 제공합니다.
//***************************************************************************
class Function
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;      // 개체 ID
	_tstring					_schemaName;        // 스키마 이름
	_tstring					_funcName;          // 함수 이름
	_tstring					_funcComment;       // 함수 주석(설명)
	_tstring					_fullBody;          // 전체 CREATE DDL 문장
	_tstring					_body;              // 함수 본문
	_tstring					_createDate;        // 생성 일시
	_tstring					_modifyDate;        // 수정 일시
	CVector<FuncParamRef>		_parameters;        // 파라미터 목록
};

//***************************************************************************
// @brief DB 모델 관련 헬퍼 유틸리티 클래스
// @detail 공백 제거, 로그 파일 출력 등 공통 유틸리티 함수를 정적으로 제공합니다.
//***************************************************************************
class Helpers
{
public:
	static _tstring			RemoveWhiteSpace(const _tstring& str);
	static void				LogFileWrite(EDBClass dbClass, _tstring title, _tstring sql, bool newline = false);
};

NAMESPACE_END

#endif // ndef UC_DBMODEL_H