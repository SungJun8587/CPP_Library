
//***************************************************************************
// DBSQLQuery.h : implementation for the System SQL.
//
//***************************************************************************

#ifndef __DBSQLQUERY_H__
#define __DBSQLQUERY_H__

#pragma once

#ifndef __DBMSSQLQUERY_H__
#include <DBMSSQLQuery.h> 
#endif

#ifndef __DBMYSQLQUERY_H__
#include <DBMYSQLQuery.h> 
#endif

#ifndef __DBORACLEQUERY_H__
#include <DBORACLEQuery.h> 
#endif

//***************************************************************************
//
class DB_INFO
{
public:
	TCHAR tszDBName[DATABASE_NAME_STRLEN] = { 0, };				// �����ͺ��̽� ��
};

//***************************************************************************
//
class DB_SYSTEM_INFO
{
public:
	/// <summary>DB ����(1/2/3 : MSSQL/MYSQL/ORACLE)</summary>
	EDBClass DBClass;

	/// <summary>����</summary>
	TCHAR tszVersion[DATABASE_BASE_STRLEN] = { 0, };
	
	/// <summary>ĳ���ͼ�</summary>
	TCHAR tszCharacterSet[DATABASE_CHARACTERSET_STRLEN] = { 0, };
	
	/// <summary>������ ����(���ں񱳱�Ģ)</summary>
	TCHAR tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };
};

//***************************************************************************
//
class DB_SYSTEM_DATATYPE
{
public:
	uint8	SystemTypeId;
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	int16	MaxLength;
	uint8	Precision;
	uint8	Scale;
	TCHAR   tszCollation[DATABASE_CHARACTERSET_STRLEN] = { 0, };
	bool	IsNullable = false;
};

//***************************************************************************
//
class TABLE_INFO
{
public:
	int32	ObjectId;										// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// ���̺� ��
	int64	AutoIncrementValue;								// Identity ������ ������ ��
	TCHAR	tszStorageEngine[DATABASE_BASE_STRLEN];			// ���丮�� ����
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN];	// ĳ���ͼ�
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN];		// ���ں񱳱�Ģ
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX];			// ���̺� �ּ�
	TCHAR	tszCreateDate[DATETIME_STRLEN];					// ���� �Ͻ�
	TCHAR	tszModifyDate[DATETIME_STRLEN];					// ���� �Ͻ�
};

//***************************************************************************
//
class COLUMN_INFO
{
public:
	int32	ObjectId;											// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];			// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];			// ���̺� ��
	int32	Seq;												// �÷� ����
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];			// �÷� ��
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];			// �÷� ������Ÿ��
	int64	MaxLength;											// �÷��� �ִ� ����(����Ʈ)
	uint8   Precision;											// �ִ� ��ü �ڸ���(���� ��� ������ ��쿡�� ������ �ִ� ��ü �ڸ���, �׷��� ������ 0)
	uint8	Scale;												// �ִ� �Ҽ� �ڸ���(���� ����� ��� ������ �ִ� �Ҽ� �ڸ���, �׷��� ������ 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];		// �÷� ������Ÿ�� ��(Ex. VARCHAR[100])
	bool	IsNullable;											// NULL ��� ����(true/false : ���/�����)
	bool	IsIdentity;											// Identity �� ����(true/false : ��/��)
	uint64	SeedValue;											// Identity �õ� ��
	uint64  IncValue;											// Identity ���� ��
	TCHAR   tszDefaultConstraintName[DATABASE_WVARCHAR_MAX];	// �⺻���� ���� ��
	TCHAR   tszDefaultDefinition[DATABASE_WVARCHAR_MAX];		// �⺻�� ����
	TCHAR	tszCharacterSet[DATABASE_CHARACTERSET_STRLEN];		// ĳ���ͼ�
	TCHAR	tszCollation[DATABASE_CHARACTERSET_STRLEN];			// ���ں񱳱�Ģ
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX];			// �÷� �ּ�
};

//***************************************************************************
//
class CONSTRAINT_INFO
{
public:
	int32	ObjectId;												// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// ���̺� ��
	TCHAR   tszConstName[DATABASE_OBJECT_NAME_STRLEN];				// �������� ��
	TCHAR   tszConstType[DATABASE_OBJECT_NAME_STRLEN];				// �������� Ÿ��
	TCHAR   tszConstTypeDesc[DATABASE_OBJECT_NAME_STRLEN];			// �������� Ÿ�� ����
	TCHAR   tszConstValue[DATABASE_WVARCHAR_MAX];					// �������� ���ǰ�
	bool	IsSystemNamed;											// �ý����� �ε������� �Ҵ��ߴ��� ����(true/false : ��/��)
	bool	IsStatus;												// ���� ������ ���� ����. ORACLE�� ���(true/false : ���� ������ Ȱ��(ENABLED)/���� ������ ��Ȱ��(DISABLED))
	TCHAR   tszSortValue[DATABASE_OBJECT_NAME_STRLEN];				// ���� ��
};

//***************************************************************************
//
class INDEX_INFO
{
public:
	int32	ObjectId;										// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];		// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];		// ���̺� ��
	TCHAR   tszIndexName[DATABASE_OBJECT_NAME_STRLEN];		// �ε��� ��
	int32	IndexId;										// �ε��� ������ȣ
	TCHAR   tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN];	// �ε��� Ÿ��(PRIMARYKEY/UNIQUE/INDEX/FULLTEXT/SPATIAL)
	bool    IsPrimaryKey;									// �⺻Ű ����(true/false : ��/��)
	bool    IsUnique;										// ����ũ ����(true/false : ��/��)
	int32   ColumnSeq;										// �÷� ����
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];		// �÷� ��
	int8    ColumnSort;										// ����(1/2 : ASC/DESC)
	bool	IsSystemNamed;									// �ý����� �ε������� �Ҵ��ߴ��� ����(true/false : ��/��)
};

//***************************************************************************
//
class FOREIGNKEYS_INFO
{
public:
	int32	ObjectId;												// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// ���̺� ��
	TCHAR   tszForeignKeyName[DATABASE_OBJECT_NAME_STRLEN];			// �ܷ�Ű ��
	TCHAR	tszForeignKeyTableName[DATABASE_TABLE_NAME_STRLEN];		// �ܷ�Ű ���̺� ��
	TCHAR	tszForeignKeyColumnName[DATABASE_COLUMN_NAME_STRLEN];	// �ܷ�Ű �÷� ��
	TCHAR	tszReferenceKeyTableName[DATABASE_TABLE_NAME_STRLEN];	// ����Ű ���̺� ��
	TCHAR	tszReferenceKeyColumnName[DATABASE_COLUMN_NAME_STRLEN];	// ����Ű �÷� ��
	TCHAR   tszUpdateReferentialAction[60];							// ������Ʈ �߻��� �� �ܷ�Ű�� ���� ����� ���� �۾�(NO_ACTION/CASCADE/RESTRICT/SET NULL/SET DEFAULT)
	TCHAR   tszDeleteReferentialAction[60];							// ���� �߻��� �� �ܷ�Ű�� ���� ����� ���� �۾�(NO_ACTION/CASCADE/RESTRICT/SET NULL/SET DEFAULT)
	bool	IsSystemNamed;											// �ý����� �ε������� �Ҵ��ߴ��� ����(true/false : ��/��)
};

//***************************************************************************
//
class CHECK_CONSTRAINT_INFO
{
public:
	int32	ObjectId;												// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// ���̺� ��
	TCHAR   tszCheckConstName[DATABASE_OBJECT_NAME_STRLEN];			// üũ �������� ��
	TCHAR   tszCheckValue[DATABASE_WVARCHAR_MAX];					// �÷� �������� ���ǰ�
	bool	IsSystemNamed;											// �ý����� �ε������� �Ҵ��ߴ��� ����(true/false : ��/��)
};

//***************************************************************************
//
class TRIGGER_INFO
{
public:
	int32	ObjectId;												// MSSQL ���̺� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];				// ���̺� ��
	TCHAR   tszTriggerName[DATABASE_OBJECT_NAME_STRLEN];			// Ʈ���� ��
};

//***************************************************************************
//
class PROCEDURE_INFO
{
public:
	int32	ObjectId;												// MSSQL �������ν��� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN];				// �������ν��� ��
	TCHAR	tszProcComment[DATABASE_WVARCHAR_MAX];					// �������ν��� �ּ�
	TCHAR   tszProcText[DATABASE_OBJECT_CONTENTTEXT_STRLEN];		// �������ν��� ����
	TCHAR	tszCreateDate[DATETIME_STRLEN];							// ���� �Ͻ�
	TCHAR	tszModifyDate[DATETIME_STRLEN];							// ���� �Ͻ�
};

//***************************************************************************
//
class PROCEDURE_PARAM_INFO
{
public:
	int32	ObjectId;												// MSSQL �������ν��� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR   tszProcName[DATABASE_OBJECT_NAME_STRLEN];				// �������ν��� ��
	int32	Seq;													// �Ķ���� ����
	int8	ParamMode;												// �Ķ���� ����� ����(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// �Ķ���� ��
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// �Ķ���� ������Ÿ��
	uint64	MaxLength;												// �Ķ������ �ִ� ����(����Ʈ)
	uint8   Precision;												// �ִ� ��ü �ڸ���(���� ��� ������ ��쿡�� ������ �ִ� ��ü �ڸ���, �׷��� ������ 0)
	uint8	Scale;													// �ִ� �Ҽ� �ڸ���(���� ����� ��� ������ �ִ� �Ҽ� �ڸ���, �׷��� ������ 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// �Ķ���� ������Ÿ�� ��(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// �Ķ���� �ּ�
};

//***************************************************************************
//
class FUNCTION_INFO
{
public:
	int32	ObjectId;												// MSSQL �Լ� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN];				// �Լ� ��
	TCHAR	tszFuncComment[DATABASE_WVARCHAR_MAX];					// �Լ� �ּ�
	TCHAR   tszFuncText[DATABASE_OBJECT_CONTENTTEXT_STRLEN];		// �Լ� ����
	TCHAR	tszCreateDate[DATETIME_STRLEN];							// ���� �Ͻ�
	TCHAR	tszModifyDate[DATETIME_STRLEN];							// ���� �Ͻ�
};

//***************************************************************************
//
class FUNCTION_PARAM_INFO
{
public:
	int32	ObjectId;												// MSSQL �Լ� ������ȣ
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];				// MSSQL ��Ű�� ��
	TCHAR   tszFuncName[DATABASE_OBJECT_NAME_STRLEN];				// �Լ� ��
	int32	Seq;													// �Ķ���� ����
	int8	ParamMode;												// �Ķ���� ����� ����(0/1/2 : RETURN/IN/OUT)
	TCHAR	tszParamName[DATABASE_COLUMN_NAME_STRLEN];				// �Ķ���� ��
	TCHAR   tszDataType[DATABASE_DATATYPEDESC_STRLEN];				// �Ķ���� ������Ÿ��
	uint64	MaxLength;												// �Ķ������ �ִ� ����(����Ʈ)
	uint8   Precision;												// �ִ� ��ü �ڸ���(���� ��� ������ ��쿡�� ������ �ִ� ��ü �ڸ���, �׷��� ������ 0)
	uint8	Scale;													// �ִ� �Ҽ� �ڸ���(���� ����� ��� ������ �ִ� �Ҽ� �ڸ���, �׷��� ������ 0)
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN];			// �Ķ���� ������Ÿ�� ��(Ex. VARCHAR[100])
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX];					// �Ķ���� �ּ�
};

//***************************************************************************
//
inline _tstring GetDBSystemQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetDBSystemQuery();
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetDBSystemQuery();
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetDBSystemQuery();
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetDBSystemDataTypeQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetDBSystemDataTypeQuery();
			break;
		case EDBClass::MYSQL:
			// MYSQL�� �ý��� ������Ÿ�� ��Ͽ� ���� �ý��� ���̺��� �������� ����
			break;
		case EDBClass::ORACLE:
			// ORACLE�� �ý��� ������Ÿ�� ��Ͽ� ���� �ý��� ���̺��� �������� ����
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetDatabaseListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetDatabaseListQuery();
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetDatabaseListQuery();
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetDatabaseListQuery();
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetTableListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetTableListQuery();
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetTableListQuery();
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetTableListQuery();
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetTableInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetTableInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetTableInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetTableInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetTableIdentityColumnInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			break;
		case EDBClass::MYSQL:
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetTableIdentityColumnInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetTableColumnInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetTableColumnInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetTableColumnInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetTableColumnInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetConstraintsInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetConstraintsInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetConstraintsInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetConstraintsInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetIndexInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetIndexInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetIndexInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetIndexInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetIndexOptionInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetIndexOptionInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			break;
		case EDBClass::ORACLE:
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetPartitionInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetPartitionInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetPartitionInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetPartitionInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetForeignKeyInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetForeignKeyInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetForeignKeyInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetForeignKeyInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetDefaultConstInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetDefaultConstInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			break;
		case EDBClass::ORACLE:
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetCheckConstInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetCheckConstInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetCheckConstInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetCheckConstInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetTriggerInfoQuery(EDBClass dbClass, _tstring tableName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetTriggerInfoQuery(tableName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetTriggerInfoQuery(tableName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetTriggerInfoQuery(tableName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetProcedureListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetProcedureListQuery();
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetProcedureListQuery();
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetProcedureListQuery();
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetProcedureInfoQuery(EDBClass dbClass, _tstring procName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetProcedureInfoQuery(procName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetProcedureInfoQuery(procName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetProcedureInfoQuery(procName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetProcedureParamInfoQuery(EDBClass dbClass, _tstring procName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetProcedureParamInfoQuery(procName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetProcedureParamInfoQuery(procName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetProcedureParamInfoQuery(procName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetFunctionListQuery(EDBClass dbClass)
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetFunctionListQuery();
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetFunctionListQuery();
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetFunctionListQuery();
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetFunctionInfoQuery(EDBClass dbClass, _tstring funcName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetFunctionInfoQuery(funcName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetFunctionInfoQuery(funcName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetFunctionInfoQuery(funcName);
			break;
	}
	return query;
}

//***************************************************************************
//
inline _tstring GetFunctionParamInfoQuery(EDBClass dbClass, _tstring funcName = _T(""))
{
	_tstring query = _T("");

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			query = MSSQLGetFunctionParamInfoQuery(funcName);
			break;
		case EDBClass::MYSQL:
			query = MYSQLGetFunctionParamInfoQuery(funcName);
			break;
		case EDBClass::ORACLE:
			query = ORACLEGetFunctionParamInfoQuery(funcName);
			break;
	}
	return query;
}

#endif // ndef __DBSQLQUERY_H__