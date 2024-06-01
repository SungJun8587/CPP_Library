
//***************************************************************************
// DBSynchronizer.h : interface for the Database Synchronizer.
//
//***************************************************************************

#ifndef __DBSYNCHRONIZER_H__
#define __DBSYNCHRONIZER_H__

#pragma once

#ifndef __CUSTOMALLOCATOR_H__
#include <CustomAllocator.h>
#endif

#ifndef __DBSQLQUEQY_H__
#include <DBSQLQuery.h> 
#endif

#ifndef __BASEODBC_H__
#include <BaseODBC.h> 
#endif

#ifndef __DBBIND_H__
#include <DBBind.h> 
#endif

#ifndef __DBMODEL_H__
#include <DBModel.h> 
#endif

#ifndef __DBSYNCBIND_H__
#include <DBSyncBind.h> 
#endif

#ifndef __DBQUERYPROCESS_H__
#include <DBQueryProcess.h> 
#endif

#include "rapidxml.hpp"
#include "rapidxml_print.hpp"

class CDBSynchronizer
{
	enum UpdateStep : uint8
	{
		DropIndex,
		DropForeignKey,
		AlterColumn,
		AddColumn,
		CreateTable,
		DefaultConstraint,
		CreateIndex,
		CreateForeignKey,
		DropColumn,
		DropTable,
		DropStoredProcecure,
		CreateStoredProcecure,
		DropFunction,
		CreateFunction,

		Max
	};

	enum ColumnFlag : uint8
	{
		Type = 1 << 0,
		Nullable = 1 << 1,
		Identity = 1 << 2,
		Default = 1 << 3,
		Length = 1 << 4,
	};

public:
	CDBSynchronizer(CBaseODBC& conn) : _dbClass(conn.GetDBClass()), _dbConn(conn) { }
	~CDBSynchronizer();

	bool		Synchronize(const TCHAR* path);
	
	void		PrintDBSchema();
	void		ParseXmlToDB(const TCHAR* path);
	bool		DBToCreateXml(const TCHAR* path);

	bool		GatherDBTables();
	bool		GatherDBTableColumns();
	bool		GatherDBIndexes();
	bool		GatherDBIndexOptions();
	bool		GatherDBForeignKeys();
	bool		GatherDBTrigger();

	bool		GatherDBStoredProcedures();
	bool		GatherDBStoredProcedureParams();
	bool		GatherDBFunctions();
	bool		GatherDBFunctionParams();

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
	void		CompareDBModel();
	void		CompareTables(DBModel::TableRef dbTable, DBModel::TableRef xmlTable);
	void		CompareColumns(DBModel::TableRef dbTable, DBModel::ColumnRef dbColumn, DBModel::ColumnRef xmlColumn);
	void		CompareStoredProcedures();
	void		CompareFunctions();

	void		ExecuteUpdateQueries();

private:
	EDBClass	_dbClass;
	CBaseODBC&	_dbConn;

	CVector<DBModel::TableRef>			_xmlTables;
	CVector<DBModel::ProcedureRef>		_xmlProcedures;
	CVector<DBModel::FunctionRef>		_xmlFunctions;
	CSet<_tstring>						_xmlRemovedTables;

	CVector<DBModel::TableRef>			_dbTables;
	CVector<DBModel::TriggerRef>		_dbTriggers;
	CVector<DBModel::ProcedureRef>		_dbProcedures;
	CVector<DBModel::FunctionRef>		_dbFunctions;

private:
	CSet<_tstring>				_dependentIndexes;
	CSet<_tstring>				_dependentReferenceKeys;
	CVector<_tstring>			_updateQueries[UpdateStep::Max];
};

#endif // ndef __DBSYNCHRONIZER_H__