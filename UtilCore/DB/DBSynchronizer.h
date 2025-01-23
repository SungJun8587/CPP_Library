
//***************************************************************************
// DBSynchronizer.h : interface for the Database Synchronizer.
//
//***************************************************************************

#ifndef __DBSYNCHRONIZER_H__
#define __DBSYNCHRONIZER_H__

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

#include "rapidxml.hpp"
#include "rapidxml_print.hpp"

namespace ExcelColumnWidth
{
	const int cnDefaultWidth = 16;
	const int cnSchemaName = 18;
	const int cnObjectName = 30;
	const int cnColumnName = 25;
	const int cnSequence = 20;
	const int cnParamMode = 22;
	const int cnParamName = 25;
	const int cnConstraintName = 38;
	const int cnIndexType = 33;
	const int cnPartitionName = 25;
	const int cnConstraintValue = 50;
	const int cnDefaultConstraintValue = 24;
	const int cnSystemNamed = 22;
	const int cnComment = 70;
	const int cnDataType = 15;
	const int cnDataTypeDesc = 21;
	const int cnEngine = 18;
	const int cnCharacterSet = 20;
	const int cnCollation = 26;
	const int cnDateTime = 21;
}

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
	void		DBToSaveExcel(const _tstring path);

	void		ParseXmlToDB(const TCHAR* path);
	bool		DBToCreateXml(const TCHAR* path);

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
	void		CompareDBModel();
	void		CompareTables(DBModel::TableRef dbTable, DBModel::TableRef xmlTable);
	void		CompareColumns(DBModel::TableRef dbTable, DBModel::ColumnRef dbColumn, DBModel::ColumnRef xmlColumn);
	void		CompareStoredProcedures();
	void		CompareFunctions();

	void		ExecuteUpdateQueries();

	void        ToHeaderStyle(Xlnt::CXlntUtil& excel, const std::string& start_cell, const std::string& end_cell);

	void		AddExcelTableInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelTableColumnInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelConstraintsInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelIndexInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelForeignKeyInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelCheckConstraintsInfo(Xlnt::CXlntUtil& excel);

	void		AddExcelMSSQLTableIndexOptionInfo(Xlnt::CXlntUtil& excel);
	void		AddExcelMSSQLDefaultConstraintsInfo(Xlnt::CXlntUtil& excel);

	void		AddExcelORACLEIdentityColumnInfo(Xlnt::CXlntUtil& excel);

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