
//***************************************************************************
// DBSchema.h : interface for the Database Schema.
//
//***************************************************************************

#ifndef UC_DBSCHEMA_H
#define UC_DBSCHEMA_H

#include <Memory/Allocator.h>
#include <DB/DBCommon.h> 
#include <DB/BaseODBC.h> 
#include <DB/DBBind.h> 
#include <DB/DBModel.h> 
#include <DB/DBSyncBind.h> 
#include <DB/DBQueryProcess.h> 

//***************************************************************************
// @brief 데이터베이스 스키마 정보 수집 및 출력을 담당하는 클래스
// @details ODBC 연결을 이용하여 테이블, 컬럼, 제약조건, 인덱스, 프로시저 등의 스키마 정보를 수집합니다.
//***************************************************************************
class CDBSchema
{
public:
	CDBSchema(CBaseODBC& conn);
	~CDBSchema();

	bool		GatherDBSchema();
	void		PrintDBSchema();

	//***************************************************************************
	// @brief 수집된 DB 테이블 모델 목록을 반환합니다.
	// @return 수집된 DB 테이블 Vector 참조
	//***************************************************************************
	CVector<DBModel::TableRef>& GetDBModelTable() { return _dbTables; }

	//***************************************************************************
	// @brief 수집된 DB 트리거 모델 목록을 반환합니다.
	// @return 수집된 DB 트리거 Vector 참조
	//***************************************************************************
	CVector<DBModel::TriggerRef>& GetDBModelTrigger() { return _dbTriggers; }

	//***************************************************************************
	// @brief 수집된 DB 저장프로시저 모델 목록을 반환합니다.
	// @return 수집된 DB 저장프로시저 Vector 참조
	//***************************************************************************
	CVector<DBModel::ProcedureRef>& GetDBModelProcedure() { return _dbProcedures; }

	//***************************************************************************
	// @brief 수집된 DB 함수 모델 목록을 반환합니다.
	// @return 수집된 DB 함수 Vector 참조
	//***************************************************************************
	CVector<DBModel::FunctionRef>& GetDBModelFunction() { return _dbFunctions; }

	bool		GatherDBTables(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBTableColumns(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBTableConstraints(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBIdentityColumns(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBIndexes(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBIndexOptions(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBForeignKeys(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBDefaultConstraints(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBCheckConstraints(const TCHAR* ptszTableName = _T(""));
	bool		GatherDBTrigger(const TCHAR* ptszTableName = _T(""));

	bool		GatherDBStoredProcedures(const TCHAR* ptszProcName = _T(""));
	bool		GatherDBStoredProcedureParams(const TCHAR* ptszProcName = _T(""));
	bool		GatherDBFunctions(const TCHAR* ptszFuncName = _T(""));
	bool		GatherDBFunctionParams(const TCHAR* ptszFuncName = _T(""));

	//***************************************************************************
	// @brief DB 함수 파라미터 정보를 수집합니다.
	// @param ptszFuncName 대상 함수명 (기본값: 전체)
	// @return 성공 여부 (true/false)
	//***************************************************************************
	void Out_Body(OUT TCHAR* value, int32 len)
	{
		BindCol(0, value, len);
	}

	//***************************************************************************
	// @brief 컬럼 데이터를 바인딩합니다.
	// @param idx 컬럼 인덱스
	// @param value 바인딩할 문자열 버퍼
	// @param len 버퍼 길이
	//***************************************************************************
	void BindCol(int32 idx, TCHAR* value, int32 len)
	{
		SQLLEN let;
		int32 iLength = len - 1;

		_dbConn.BindCol(idx + 1, value, iLength, let);
	}

	//***************************************************************************
	// @brief DB 클래스 타입을 반환합니다.
	// @return 데이터베이스 종류 (EDBClass)
	//***************************************************************************
	EDBClass GetDBClass() { return _dbClass; }

private:
	EDBClass	_dbClass;	// 데이터베이스 종류
	CBaseODBC&	_dbConn;	// ODBC 데이터베이스 연결 참조

	CVector<DBModel::TableRef>			_dbTables;		// 수집된 DB 테이블 목록
	CVector<DBModel::TriggerRef>		_dbTriggers;	// 수집된 DB 트리거 목록
	CVector<DBModel::ProcedureRef>		_dbProcedures;	// 수집된 DB 저장프로시저 목록
	CVector<DBModel::FunctionRef>		_dbFunctions;	// 수집된 DB 함수 목록
};

#endif // ndef UC_DBSCHEMA_H