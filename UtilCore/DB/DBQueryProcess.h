
//***************************************************************************
// DBQueryProcess.h : interface for the Database Synchronizer.
//
//***************************************************************************

#ifndef __DBQUERYPROCESS_H__
#define __DBQUERYPROCESS_H__

#pragma once

#ifndef __BASEDEFINE_H__
#include <BaseDefine.h>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __DBCOMMON_H__
#include <DBCommon.h> 
#endif

#ifndef __BASEODBC_H__
#include <BaseODBC.h> 
#endif

class CDBQueryProcess
{
public:
	CDBQueryProcess(CBaseODBC& conn) : _dbClass(conn.GetDBClass()), _dbConn(conn) { }
	~CDBQueryProcess();

	EDBClass GetDBClass() { return _dbClass; }

	// Database 시스템 관련 함수
	bool		GetDBSystemInfo(int32& iSystemCount, std::unique_ptr<DB_SYSTEM_INFO>& pDBSystemInfo);
	bool		GetDBSystemDataTypeInfo(int& iDatatypeCount, std::unique_ptr<DB_SYSTEM_DATATYPE[]>& pDBSystemDataType);
	bool		GetDatabaseList(int& iDBCount, std::unique_ptr<DB_INFO[]>& pDatabase);

	// MSSQL 인덱스 조각화 관련 함수
	bool		MSSQLGetRowStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation);
	bool		MSSQLGetColumnStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation);
	bool        MSSQLIndexOptionSet(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, unordered_map<_tstring, _tstring> indexOptions);
	bool		MSSQLAlterIndexFragmentationNonOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation);
	bool		MSSQLAlterIndexFragmentationOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation, unordered_map<_tstring, _tstring> indexOptions);

	// MSSQL 저장프로시저, 함수, 트리거 생성 쿼리 얻어오기 관련 함수 
	_tstring	MSSQLHelpText(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MSSQL 테이블, 컬럼 명 수정
	bool		MSSQLRenameObject(const TCHAR* ptszObjectName, const TCHAR* ptszChgObjectName, const EMSSQLRenameObjectType renameObjectType = EMSSQLRenameObjectType::NONE);

	// MSSQL 테이블, 컬럼 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MSSQLProcessTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDescription);

	// MSSQL 저장프로시저, 파라미터 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam);
	bool		MSSQLProcessProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam, const TCHAR* ptszDescription);

	// MSSQL 함수, 파라미터 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MSSQLGetFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam);
	bool		MSSQLProcessFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam, const TCHAR* ptszDescription);

	// MSSQL 확장 속성을 보기, 추가, 수정, 삭제
	_tstring	MSSQLGetExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLAddExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLUpdateExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);
	bool		MSSQLDropExtendedProperty(const MSSQL_ExtendedProperty extendedProperty);

	// MYSQL 테이블 캐릭터셋, 데이터정렬(문자비교규칙), 스토리지엔진 관련 함수
	bool		MYSQLGetCharacterSets(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET[]>& pCharacterSet);
	bool		MYSQLGetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_COLLATION[]>& pCollation);
	bool		MYSQLGetCharacterSetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET_COLLATION[]>& pCharacterSetCollation);
	bool		MYSQLGetStorageEngines(int32& iStorageEngineCount, std::unique_ptr<MYSQL_STORAGE_ENGINE[]>& pStorageEngine);
	bool		MYSQLAlterTable(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation, const TCHAR* ptszEngine);
	bool		MYSQLAlterTableCollation(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation);

	// MYSQL 테이블 조각화 관련 함수
	bool		MYSQLGetTableFragmentationCheck(const TCHAR* ptszTableName, int32& iTableCount, std::unique_ptr<MYSQL_TABLE_FRAGMENTATION[]>& pTableFragmentation);
	bool		MYSQLOptimizeTable(const TCHAR* ptszTableName);

	// MYSQL 테이블, 컬럼 생성 쿼리 얻어오기 관련 함수
	_tstring	MYSQLShowTable(const TCHAR* ptszTableName);

	// MYSQL 저장프로시저/함수/트리거/이벤트 생성 쿼리 얻어오기 관련 함수
	_tstring	MYSQLShowObject(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MYSQL 테이블, 컬럼 명 수정
	bool		MYSQLRenameObject(const TCHAR* ptszTableName, const TCHAR* ptszChgName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL 테이블, 컬럼 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MYSQLProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL 저장프로시저 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetProcedureComment(const TCHAR* ptszProcName);
	bool		MYSQLProcessProcedureComment(const TCHAR* ptszProcName, const TCHAR* ptszDescription);

	// MYSQL 함수 코멘트 보기/추가/수정/삭제 관련 함수
	_tstring	MYSQLGetFunctionComment(const TCHAR* ptszFuncName);
	bool		MYSQLProcessFunctionComment(const TCHAR* ptszFuncName, const TCHAR* ptszDescription);

	// ORACLE 테이블, 컬럼 코멘트 추가/수정/삭제 관련 함수
	bool		ORACLEProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDescription);

	// ORACLE 테이블, 인덱스 생성 쿼리 얻어오기 관련 함수
	_tstring	ORACLEMetaDataGetDDL(const EDBObjectType dbObject, const TCHAR* ptszObjectName, const TCHAR* ptszSchemaName);

	// ORACLE 저장프로시저/함수 생성 쿼리 얻어오기 관련 함수
	_tstring	ORACLEGetUserSource(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// ORACLE 인덱스 조각화 관련 함수
	bool		ORACLEGetAnalyzeIndexFragmentationCheck(const TCHAR* ptszIndexName);
	bool		ORACLEGetIndexFragmentationCheck(const TCHAR* ptszIndexName, int32& iCount, std::unique_ptr<ORACLE_INDEX_FRAGMENTATION[]>& pIndexFragmentation);
	bool		ORACLEGetAnalyzeIndexStatFragmentationCheck(const TCHAR* ptszIndexName);
	bool		ORACLEGetIndexStatFragmentationCheck(const TCHAR* ptszIndexName, int32& iCount, std::unique_ptr<ORACLE_INDEX_STAT_FRAGMENTATION[]>& pIndexStatFragmentation);
	bool		ORACLEGetIndexRebuild(const TCHAR* ptszIndexName);

private:
	EDBClass	_dbClass;
	CBaseODBC& _dbConn;
};

#endif // ndef __DBQUERYPROCESS_H__