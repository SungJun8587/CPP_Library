
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

#ifndef __BASESQL_H__
#include <BaseSQL.h> 
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

#include "rapidxml.hpp"
#include "rapidxml_print.hpp"

struct stExtendedProperty
{
	_tstring _propertyName;			// �Ӽ��� �̸�
	_tstring _propertyValue;		// �Ӽ��� ������ ��
	_tstring _level0_object_type;	// ���� 0 ��ü�� ����
	_tstring _level0_object_name;	// ���� 0 ��ü ������ �̸�
	_tstring _level1_object_type;	// ���� 1 ��ü�� ����
	_tstring _level1_object_name;	// ���� 1 ��ü ������ �̸�
	_tstring _level2_object_type;	// ���� 2 ��ü�� ����
	_tstring _level2_object_name;	// ���� 2 ��ü ������ �̸�
};

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

	bool		GetDBSystemInfo(int32& system_count, std::unique_ptr<DBModel::DB_SYSTEMINFO[]>& pDBSystemInfo);
	bool		GetDBSystemDataTypeInfo(int& datatype_count, std::unique_ptr<DBModel::DB_SYSTEM_DATATYPE[]>& pDBSystemDataType);

	// MSSQL �������ν���, �Լ�, Ʈ���� ���� ���� ������ ���� �Լ� 
	_tstring	MSSQLDBHelpText(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MSSQL ���̺�, �÷� �� ����
	bool		MSSQLRenameObject(const TCHAR* ptszObjectName, const TCHAR* ptszChgObjectName, const EMSSQLRenameObjectType renameObjectType = EMSSQLRenameObjectType::NONE);

	// MSSQL ���̺�, �÷� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MSSQLGetTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MSSQLProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDescription);

	// MSSQL �������ν���, �Ķ���� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MSSQLGetProcedureParamComment(const TCHAR* ptszProcName, const TCHAR* ptszProcParam);
	bool		MSSQLProcessProcedureParamComment(const TCHAR* ptszProcName, const TCHAR* ptszProcParam, const TCHAR* ptszDescription);

	// MSSQL �Լ�, �Ķ���� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MSSQLGetFunctionParamComment(const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam);
	bool		MSSQLProcessFunctionParamComment(const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam, const TCHAR* ptszDescription);

	// MSSQL Ȯ�� �Ӽ��� ����, �߰�, ����, ����
	_tstring	MSSQLGetExtendedProperty(const stExtendedProperty extendedProperty);
	bool		MSSQLAddExtendedProperty(const stExtendedProperty extendedProperty);
	bool		MSSQLUpdateExtendedProperty(const stExtendedProperty extendedProperty);
	bool		MSSQLDropExtendedProperty(const stExtendedProperty extendedProperty);


	// MYSQL ���̺�, �÷� ���� ���� ������ ���� �Լ�
	_tstring	MYSQLDBShowTable(const TCHAR* ptszTableName);

	// MYSQL �������ν���/�Լ�/Ʈ����/�̺�Ʈ ���� ���� ������ ���� �Լ�
	_tstring	MYSQLDBShowObject(const EDBObjectType dbObject, const TCHAR* ptszObjectName);

	// MYSQL ���̺�, �÷� �� ����
	bool		MYSQLRenameObject(const TCHAR* ptszTableName, const TCHAR* ptszChgName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL ���̺�, �÷� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MYSQLGetTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName);
	bool		MYSQLProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName = _T(""), const TCHAR* ptszDataTypeDesc = _T(""), bool bIsNullable = false, const TCHAR* ptszDefaultDefinition = _T(""), bool bIsIdentity = false, const TCHAR* ptszCharacterSet = _T(""), const TCHAR* ptszCollation = _T(""), const TCHAR* ptszComment = _T(""));

	// MYSQL �������ν��� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MYSQLGetProcedureComment(const TCHAR* ptszProcName);
	bool		MYSQLProcessProcedureComment(const TCHAR* ptszProcName, const TCHAR* ptszDescription);

	// MYSQL �Լ� �ڸ�Ʈ ����/�߰�/����/���� ���� �Լ�
	_tstring	MYSQLGetFunctionComment(const TCHAR* ptszFuncName);
	bool		MYSQLProcessFunctionComment(const TCHAR* ptszFuncName, const TCHAR* ptszDescription);

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