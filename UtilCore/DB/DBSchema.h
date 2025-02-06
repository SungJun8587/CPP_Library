
//***************************************************************************
// DBSchema.h : interface for the Database Schema.
//
//***************************************************************************

#ifndef __DBSCHEMA_H__
#define __DBSCHEMA_H__

#pragma once

#ifndef __CUSTOMALLOCATOR_H__
#include <Memory/CustomAllocator.h>
#endif

#ifndef __DBCOMMON_H__
#include <DB/DBCommon.h> 
#endif

#ifndef __BASEODBC_H__
#include <DB/BaseODBC.h> 
#endif

#ifndef __DBBIND_H__
#include <DB/DBBind.h> 
#endif

#ifndef __DBMODEL_H__
#include <DB/DBModel.h> 
#endif

#ifndef __DBSYNCBIND_H__
#include <DB/DBSyncBind.h> 
#endif

#ifndef __DBQUERYPROCESS_H__
#include <DB/DBQueryProcess.h> 
#endif

class CDBSchema
{
public:
	CDBSchema(CBaseODBC& conn) : _dbClass(conn.GetDBClass()), _dbConn(conn) { }
	~CDBSchema();

	bool		GatherDBSchema();
	void		PrintDBSchema();

	CVector<DBModel::TableRef>&		GetDBModelTable()			{ return _dbTables; }
	CVector<DBModel::TriggerRef>&	GetDBModelTrigger()			{ return _dbTriggers; }
	CVector<DBModel::ProcedureRef>&	GetDBModelProcedure()		{ return _dbProcedures; }
	CVector<DBModel::FunctionRef>&	GetDBModelFunction()		{ return _dbFunctions; }

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

	void Out_Body(OUT TCHAR* value, int32 len)
	{
		BindCol(0, value, len);
	}

	void BindCol(int32 idx, TCHAR* value, int32 len)
	{
		SQLLEN let;
		int32 iLength = len - 1;

		_dbConn.BindCol(idx + 1, value, iLength, let);
	}

	EDBClass GetDBClass() { return _dbClass; }

private:
	EDBClass	_dbClass;
	CBaseODBC&	_dbConn;

	CVector<DBModel::TableRef>			_dbTables;
	CVector<DBModel::TriggerRef>		_dbTriggers;
	CVector<DBModel::ProcedureRef>		_dbProcedures;
	CVector<DBModel::FunctionRef>		_dbFunctions;
};

#endif // ndef __DBSCHEMA_H__