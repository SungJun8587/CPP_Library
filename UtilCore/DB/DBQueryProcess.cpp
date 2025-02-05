
//***************************************************************************
// DBQueryProcess.cpp: implementation of the Database Query Process.
//
//***************************************************************************

#include "pch.h"
#include "DBQueryProcess.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CDBQueryProcess::~CDBQueryProcess()
{
}

//***************************************************************************
//
bool CDBQueryProcess::GetDBSystemInfo(int32& iSystemCount, std::unique_ptr<DB_SYSTEM_INFO>& pDBSystemInfo)
{
	_tstring query;

	int32	iBaseSize = DATABASE_DATATYPEDESC_STRLEN;
	int32	iCharSetSize = DATABASE_CHARACTERSET_STRLEN;

	query = GetDBSystemQuery(_dbClass);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	pDBSystemInfo = unique_ptr<DB_SYSTEM_INFO>(new DB_SYSTEM_INFO);

	_dbConn.BindCol(pDBSystemInfo->tszVersion, iBaseSize);
	_dbConn.BindCol(pDBSystemInfo->tszCharacterSet, iCharSetSize);
	_dbConn.BindCol(pDBSystemInfo->tszCollation, iCharSetSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//  
bool CDBQueryProcess::GetDBSystemDataTypeInfo(int& iDatatypeCount, std::unique_ptr<DB_SYSTEM_DATATYPE[]>& pDBSystemDataType)
{
	_tstring query = _T("");

	int32	iDataTypeSize = DATABASE_DATATYPEDESC_STRLEN;
	int32	iCharSetSize = DATABASE_CHARACTERSET_STRLEN;

	if( _dbClass != EDBClass::MSSQL ) return false;

	query = query + "SELECT COUNT(*) AS [datatype_count] FROM sys.types WHERE system_type_id = user_type_id;";
	query = query + "\r\n" + GetDBSystemDataTypeQuery(_dbClass);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iDatatypeCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iDatatypeCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pDBSystemDataType = unique_ptr<DB_SYSTEM_DATATYPE[]>(new DB_SYSTEM_DATATYPE[iDatatypeCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(DB_SYSTEM_DATATYPE), iDatatypeCount);

	_dbConn.BindCol(pDBSystemDataType[0].SystemTypeId);
	_dbConn.BindCol(pDBSystemDataType[0].tszDataType, iDataTypeSize);
	_dbConn.BindCol(pDBSystemDataType[0].MaxLength);
	_dbConn.BindCol(pDBSystemDataType[0].Precision);
	_dbConn.BindCol(pDBSystemDataType[0].Scale);
	_dbConn.BindCol(pDBSystemDataType[0].tszCollation, iCharSetSize);
	_dbConn.BindCol(pDBSystemDataType[0].IsNullable);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::GetDatabaseList(int& iDBCount, std::unique_ptr<DB_INFO[]>& pDatabase)
{
	_tstring query = _T("");

	int32	iDBNameSize = DATABASE_NAME_STRLEN;

	if( _dbClass == EDBClass::MSSQL )
		query = query + "SELECT COUNT(*) AS [db_count] FROM sys.databases WHERE [name] NOT IN('model', 'msdb', 'pubs', 'Northwind', 'tempdb');";
	else if( _dbClass == EDBClass::MYSQL )
		query = query + "SELECT COUNT(*) AS `db_count` FROM INFORMATION_SCHEMA.SCHEMATA WHERE `SCHEMA_NAME` NOT IN('sys', 'mysql', 'information_schema', 'performance_schema', 'world');";

	query = query + "\r\n" + GetDatabaseListQuery(_dbClass);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iDBCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iDBCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pDatabase = unique_ptr<DB_INFO[]>(new DB_INFO[iDBCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(DB_INFO), iDBCount);

	_dbConn.BindCol(pDatabase[0].tszDBName, iDBNameSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLGetRowStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation)
{
	_tstring query = _T("");

	int32   iObjectNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32	iTableNameSize = DATABASE_TABLE_NAME_STRLEN;
	int32   iObjectTypeDescSize = DATABASE_OBJECT_TYPE_DESC_STRLEN;
	int32	iBuffSize = DATABASE_WVARCHAR_MAX;

	if( _dbClass != EDBClass::MSSQL ) return false;

	query = query + "SELECT COUNT(*) AS [index_count]";
	query = query + "\n" + "FROM sys.dm_db_index_physical_stats(DB_ID(), NULL, NULL, NULL, 'SAMPLED') AS ips";
	query = query + "\n" + "INNER JOIN sys.indexes AS i";
	query = query + "\n" + "ON ips.object_id = i.object_id AND ips.index_id = i.index_id";
	if( ptszTableName != _T("") )
		query = query + "\n" + "WHERE OBJECT_NAME(ips.object_id) = '" + ptszTableName + "'";
	query = query + ";";

	query = query + "\r\n" + MSSQLGetRowStoreIndexFragmentationCheckQuery(ptszTableName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iIndexCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iIndexCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pIndexFragmentation = unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>(new MSSQL_INDEX_FRAGMENTATION[iIndexCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MSSQL_INDEX_FRAGMENTATION), iIndexCount);

	_dbConn.BindCol(pIndexFragmentation[0].ObjectId);
	_dbConn.BindCol(pIndexFragmentation[0].tszSchemaName, iObjectNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].tszTableName, iTableNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].IndexId);
	_dbConn.BindCol(pIndexFragmentation[0].tszIndexName, iObjectNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].tszIndexType, iObjectTypeDescSize);
	_dbConn.BindCol(pIndexFragmentation[0].PartitionNum);
	_dbConn.BindCol(pIndexFragmentation[0].AvgFragmentationInPercent);
	_dbConn.BindCol(pIndexFragmentation[0].AvgPageSpaceUsedInPercent);
	_dbConn.BindCol(pIndexFragmentation[0].PageCount);
	_dbConn.BindCol(pIndexFragmentation[0].tszAllocUnitTypeDesc, iBuffSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLGetColumnStoreIndexFragmentationCheck(const TCHAR* ptszTableName, int32& iIndexCount, std::unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>& pIndexFragmentation)
{
	_tstring query = _T("");

	int32   iObjectNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32	iTableNameSize = DATABASE_TABLE_NAME_STRLEN;
	int32   iObjectTypeDescSize = DATABASE_OBJECT_TYPE_DESC_STRLEN;
	int32	iBuffSize = DATABASE_WVARCHAR_MAX;

	if( _dbClass != EDBClass::MSSQL ) return false;

	query = query + "SELECT COUNT(*) AS [index_count]";
	query = query + "\n" + "FROM sys.indexes AS i";
	query = query + "\n" + "INNER JOIN sys.dm_db_column_store_row_group_physical_stats AS rgs";
	query = query + "\n" + "ON i.object_id = rgs.object_id AND i.index_id = rgs.index_id";
	if( ptszTableName != _T("") )
	{
		query = query + "\n" + "WHERE OBJECT_NAME(i.object_id) = '" + ptszTableName + "' AND rgs.state_desc = 'COMPRESSED'";
		query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
	}
	else
	{
		query = query + "\n" + "WHERE rgs.state_desc = 'COMPRESSED'";
		query = query + "\n" + "GROUP BY i.object_id, i.index_id, i.name, i.type_desc";
	}
	query = query + ";";

	query = query + "\r\n" + MSSQLGetColumnStoreIndexFragmentationCheckQuery(ptszTableName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iIndexCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iIndexCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pIndexFragmentation = unique_ptr<MSSQL_INDEX_FRAGMENTATION[]>(new MSSQL_INDEX_FRAGMENTATION[iIndexCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MSSQL_INDEX_FRAGMENTATION), iIndexCount);

	_dbConn.BindCol(pIndexFragmentation[0].ObjectId);
	_dbConn.BindCol(pIndexFragmentation[0].tszSchemaName, iObjectNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].tszTableName, iTableNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].IndexId);
	_dbConn.BindCol(pIndexFragmentation[0].tszIndexName, iObjectNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].tszIndexType, iObjectTypeDescSize);
	_dbConn.BindCol(pIndexFragmentation[0].AvgFragmentationInPercent);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//  
bool CDBQueryProcess::MSSQLIndexOptionSet(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, unordered_map<_tstring, _tstring> indexOptions)
{
	if( _dbClass != EDBClass::MSSQL ) return false;

	_tstring query = MSSQLIndexOptionSetQuery(ptszSchemaName, ptszTableName, ptszIndexName, indexOptions);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//  
bool CDBQueryProcess::MSSQLAlterIndexFragmentationNonOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation)
{
	if( _dbClass != EDBClass::MSSQL ) return false;

	_tstring query = MSSQLAlterIndexFragmentationNonOptionQuery(ptszSchemaName, ptszTableName, ptszIndexName, indexFragmentation);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//  
bool CDBQueryProcess::MSSQLAlterIndexFragmentationOption(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszIndexName, const EMSSQLIndexFragmentation indexFragmentation, unordered_map<_tstring, _tstring> indexOptions)
{
	if( _dbClass != EDBClass::MSSQL ) return false;

	_tstring query = MSSQLAlterIndexFragmentationOptionQuery(ptszSchemaName, ptszTableName, ptszIndexName, indexFragmentation, indexOptions);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//  
_tstring CDBQueryProcess::MSSQLHelpText(const EDBObjectType dbObject, const TCHAR* ptszObjectName)
{
	_tstring query;
	_tstring body;

	int32	iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR	tszText[DATABASE_WVARCHAR_MAX] = { 0, };

	SQLLEN		sdwObjectName(SQL_NTS);

	if( _dbClass != EDBClass::MSSQL ) return body;
	if( ptszObjectName == nullptr || _tcslen(ptszObjectName) < 1 ) return body;

	// EXEC sp_helptext [ObjectName]
	//	ObjectName : [PROCEDURE|FUNCTION|TRIGGERS|EVENTS|VIEW] NAME
	query = MSSQLGetHelpTextQuery(dbObject);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return body;

	_dbConn.BindParamInput(1, ptszObjectName, sdwObjectName);
	_dbConn.BindCol(tszText, iBuffSize);

	if( _dbConn.Execute() == false ) return body;

	while( _dbConn.Fetch() )
	{
		body.append(tszText);
	}

	return body;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLRenameObject(const TCHAR* ptszObjectName, const TCHAR* ptszChgObjectName, const EMSSQLRenameObjectType renameObjectType)
{
	_tstring query;

	if( _dbClass != EDBClass::MSSQL ) return false;
	if( ptszObjectName == nullptr || _tcslen(ptszObjectName) < 1 ) return false;

	query = MSSQLGetRenameObjectQuery(ptszObjectName, ptszChgObjectName, renameObjectType);
	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MSSQLGetTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName)
{
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MSSQL ) return ret;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return ret;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return ret;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::TABLE);
	extendedProperty._level1_object_name = ptszTableName;
	extendedProperty._level2_object_type = _T("");
	extendedProperty._level2_object_name = _T("");
	if( ptszColumnName != nullptr && _tcslen(ptszColumnName) > 0 )
	{
		extendedProperty._level2_object_type = _T("COLUMN");
		extendedProperty._level2_object_name = ptszColumnName;
	}

	return MSSQLGetExtendedProperty(extendedProperty);
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLProcessTableColumnComment(const TCHAR* ptszSchemaName, const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MSSQL ) return false;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return false;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return false;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._propertyValue = ptszComment;
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::TABLE);
	extendedProperty._level1_object_name = ptszTableName;
	extendedProperty._level2_object_type = ToString(EMSSQLExtendedPropertyLevel2Type::COLUMN);
	extendedProperty._level2_object_name = ptszColumnName;

	_tstring comment = MSSQLGetTableColumnComment(ptszSchemaName, ptszTableName, ptszColumnName);
	if( ptszComment != nullptr && _tcslen(ptszComment) > 0 )
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_updateextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 테이블 주석 수정 : sp_updateextendedproperty 'MS_Description', '테이블 설명', 'SCHEMA', 'dbo', 'TABLE', '테이블명'
			//			- 컬럼 주석 수정   : sp_updateextendedproperty 'MS_Description', '컬럼 설명', 'SCHEMA', 'dbo', 'TABLE', '테이블명', 'COLUMN' , '컬럼명'
			return MSSQLUpdateExtendedProperty(extendedProperty);
		}
		else
		{
			//	- EXEC sp_addextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 테이블 주석 추가 : sp_addextendedproperty 'MS_Description', '테이블 설명', 'SCHEMA', 'dbo', 'TABLE', '테이블명'
			//			- 컬럼 주석 추가   : sp_addextendedproperty 'MS_Description', '컬럼 설명', 'SCHEMA', 'dbo', 'TABLE', '테이블명', 'COLUMN' , '컬럼명'
			return MSSQLAddExtendedProperty(extendedProperty);
		}
	}
	else
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_dropextendedproperty [속성명], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 테이블 주석 삭제 : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'TABLE', '테이블명'
			//			- 컬럼 주석 삭제   : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'TABLE', '테이블명', 'COLUMN' , '컬럼명'
			return MSSQLDropExtendedProperty(extendedProperty);
		}
	}

	return true;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MSSQLGetProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam)
{
	_tstring ret;

	int32 iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MSSQL ) return ret;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return ret;
	if( ptszProcName == nullptr || _tcslen(ptszProcName) < 1 ) return ret;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::PROCEDURE);
	extendedProperty._level1_object_name = ptszProcName;
	extendedProperty._level2_object_type = _T("");
	extendedProperty._level2_object_name = _T("");
	if( ptszProcParam != nullptr && _tcslen(ptszProcParam) > 0 )
	{
		extendedProperty._level2_object_type = _T("PARAMETER");
		extendedProperty._level2_object_name = ptszProcParam;
	}

	return MSSQLGetExtendedProperty(extendedProperty);
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLProcessProcedureParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszProcName, const TCHAR* ptszProcParam, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MSSQL ) return false;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return false;
	if( ptszProcName == nullptr || _tcslen(ptszProcName) < 1 ) return false;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._propertyValue = ptszComment;
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::PROCEDURE);
	extendedProperty._level1_object_name = ptszProcName;
	extendedProperty._level2_object_type = ToString(EMSSQLExtendedPropertyLevel2Type::PARAMETER);
	extendedProperty._level2_object_name = ptszProcParam;

	_tstring comment = MSSQLGetProcedureParamComment(ptszSchemaName, ptszProcName, ptszProcParam);
	if( ptszComment != nullptr && _tcslen(ptszComment) > 0 )
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_updateextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 프로시저 주석 수정 : sp_updateextendedproperty 'MS_Description', '프로시저 설명', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명'
			//			- 파라미터 주석 수정   : sp_updateextendedproperty 'MS_Description', '파라미터 설명', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명', 'PARAMETER' , '파라미터명'
			return MSSQLUpdateExtendedProperty(extendedProperty);
		}
		else
		{
			//	- EXEC sp_addextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 프로시저 주석 추가 : sp_addextendedproperty 'MS_Description', '프로시저 설명', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명'
			//			- 파라미터 주석 추가   : sp_addextendedproperty 'MS_Description', '파라미터 설명', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명', 'PARAMETER' , '파라미터명'
			return MSSQLAddExtendedProperty(extendedProperty);
		}
	}
	else
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_dropextendedproperty [속성명], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 프로시저 주석 삭제 : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명'
			//			- 파라미터 주석 삭제   : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'PROCEDURE', '프로시저명', 'PARAMETER' , '파라미터명'
			return MSSQLDropExtendedProperty(extendedProperty);
		}
	}

	return true;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MSSQLGetFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam)
{
	_tstring ret;

	int32 iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MSSQL ) return ret;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return ret;
	if( ptszFuncName == nullptr || _tcslen(ptszFuncName) < 1 ) return ret;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::FUNCTION);
	extendedProperty._level1_object_name = ptszFuncName;
	extendedProperty._level2_object_type = _T("");
	extendedProperty._level2_object_name = _T("");
	if( ptszFuncParam != nullptr && _tcslen(ptszFuncParam) > 0 )
	{
		extendedProperty._level2_object_type = _T("PARAMETER");
		extendedProperty._level2_object_name = ptszFuncParam;
	}

	return MSSQLGetExtendedProperty(extendedProperty);
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLProcessFunctionParamComment(const TCHAR* ptszSchemaName, const TCHAR* ptszFuncName, const TCHAR* ptszFuncParam, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MSSQL ) return false;
	if( ptszSchemaName == nullptr || _tcslen(ptszSchemaName) < 1 ) return false;
	if( ptszFuncName == nullptr || _tcslen(ptszFuncName) < 1 ) return false;

	MSSQL_ExtendedProperty extendedProperty;

	extendedProperty._propertyName = _T("MS_Description");
	extendedProperty._propertyValue = ptszComment;
	extendedProperty._level0_object_type = ToString(EMSSQLExtendedPropertyLevel0Type::SCHEMA);
	extendedProperty._level0_object_name = ptszSchemaName;
	extendedProperty._level1_object_type = ToString(EMSSQLExtendedPropertyLevel1Type::FUNCTION);
	extendedProperty._level1_object_name = ptszFuncName;
	extendedProperty._level2_object_type = ToString(EMSSQLExtendedPropertyLevel2Type::PARAMETER);
	extendedProperty._level2_object_name = ptszFuncParam;

	_tstring comment = MSSQLGetFunctionParamComment(ptszSchemaName, ptszFuncName, ptszFuncParam);
	if( ptszComment != nullptr && _tcslen(ptszComment) > 0 )
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_updateextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 함수 주석 수정 : sp_updateextendedproperty 'MS_Description', '함수 설명', 'SCHEMA', 'dbo', 'FUNCTION', '함수명'
			//			- 파라미터 주석 수정   : sp_updateextendedproperty 'MS_Description', '파라미터 설명', 'SCHEMA', 'dbo', 'FUNCTION', '함수명', 'PARAMETER' , '파라미터명'
			return MSSQLUpdateExtendedProperty(extendedProperty);
		}
		else
		{
			//	- EXEC sp_addextendedproperty [속성명], [속성값], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 함수 주석 추가 : sp_addextendedproperty 'MS_Description', '함수 설명', 'SCHEMA', 'dbo', 'FUNCTION', '함수명'
			//			- 파라미터 주석 추가   : sp_addextendedproperty 'MS_Description', '파라미터 설명', 'SCHEMA', 'dbo', 'FUNCTION', '함수명', 'PARAMETER' , '파라미터명'
			return MSSQLAddExtendedProperty(extendedProperty);
		}
	}
	else
	{
		if( comment.c_str() != nullptr && comment.size() > 0 )
		{
			//	- EXEC sp_dropextendedproperty [속성명], [level0type], [level0name], [level1type], [level1name], [level2type], [level2name]
			//		Ex)
			//			- 함수 주석 삭제 : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'FUNCTION', '함수명'
			//			- 파라미터 주석 삭제   : sp_dropextendedproperty 'MS_Description', 'SCHEMA', 'dbo', 'FUNCTION', '함수명', 'PARAMETER' , '파라미터명'
			return MSSQLDropExtendedProperty(extendedProperty);
		}
	}

	return true;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MSSQLGetExtendedProperty(const MSSQL_ExtendedProperty extendedProperty)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	SQLLEN		sdwPropertyName(SQL_NTS), sdwLevel0_object_type(SQL_NTS), sdwLevel0_object_name(SQL_NTS);
	SQLLEN		sdwLevel1_object_type(SQL_NTS), sdwLevel1_object_name(SQL_NTS);
	SQLLEN		sdwLevel2_object_type(SQL_NTS), sdwLevel2_object_name(SQL_NTS);

	if( _dbClass != EDBClass::MSSQL ) return ret;
	if( extendedProperty._level2_object_type.size() < 1 ) sdwLevel2_object_type = SQL_NULL_DATA;
	if( extendedProperty._level2_object_name.size() < 1 ) sdwLevel2_object_name = SQL_NULL_DATA;

	query = query + "SELECT CAST(fn.[value] AS NVARCHAR(4000)) AS comment ";
	query = query + "\n" + "FROM sys.fn_listextendedproperty(?, ?, ?, ?, ?, ?, ?) AS fn";
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindParamInput(1, extendedProperty._propertyName.c_str(), sdwPropertyName);
	_dbConn.BindParamInput(2, extendedProperty._level0_object_type.c_str(), sdwLevel0_object_type);
	_dbConn.BindParamInput(3, extendedProperty._level0_object_name.c_str(), sdwLevel0_object_name);
	_dbConn.BindParamInput(4, extendedProperty._level1_object_type.c_str(), sdwLevel1_object_type);
	_dbConn.BindParamInput(5, extendedProperty._level1_object_name.c_str(), sdwLevel1_object_name);
	_dbConn.BindParamInput(6, extendedProperty._level2_object_type.c_str(), sdwLevel2_object_type);
	_dbConn.BindParamInput(7, extendedProperty._level2_object_name.c_str(), sdwLevel2_object_name);

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	if( tszDescription != nullptr && _tcslen(tszDescription) > 0 )
	{
		ret.resize(iBuffSize);
		ret.assign(tszDescription);
	}

	return ret;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLAddExtendedProperty(const MSSQL_ExtendedProperty extendedProperty)
{
	_tstring query = _T("");

	if( _dbClass != EDBClass::MSSQL ) return false;
	if( extendedProperty._propertyName.size() == 0 || extendedProperty._propertyValue.size() == 0 ) return false;
	if( extendedProperty._level0_object_type.size() == 0 || extendedProperty._level0_object_name.size() == 0 ) return false;
	if( extendedProperty._level1_object_type.size() == 0 || extendedProperty._level1_object_name.size() == 0 ) return false;

	if( extendedProperty._level2_object_type.size() > 0 && extendedProperty._level2_object_name.size() > 0 )
	{
		query = _T("EXEC sp_addextendedproperty ?, ?, ?, ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._propertyValue.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_name.c_str());
	}
	else
	{
		query = _T("EXEC sp_addextendedproperty ?, ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._propertyValue.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
	}

	if( _dbConn.Execute() == false )
		return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLUpdateExtendedProperty(const MSSQL_ExtendedProperty extendedProperty)
{
	_tstring query = _T("");

	if( _dbClass != EDBClass::MSSQL ) return false;
	if( extendedProperty._propertyName.size() == 0 || extendedProperty._propertyValue.size() == 0 ) return false;
	if( extendedProperty._level0_object_type.size() == 0 || extendedProperty._level0_object_name.size() == 0 ) return false;
	if( extendedProperty._level1_object_type.size() == 0 || extendedProperty._level1_object_name.size() == 0 ) return false;

	if( extendedProperty._level2_object_type.size() > 0 && extendedProperty._level2_object_name.size() > 0 )
	{
		query = _T("EXEC sp_updateextendedproperty ?, ?, ?, ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._propertyValue.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_name.c_str());
	}
	else
	{
		query = _T("EXEC sp_updateextendedproperty ?, ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._propertyValue.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
	}

	if( _dbConn.Execute() == false )
		return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MSSQLDropExtendedProperty(const MSSQL_ExtendedProperty extendedProperty)
{
	_tstring query = _T("");

	if( _dbClass != EDBClass::MSSQL ) return false;
	if( extendedProperty._propertyName.size() == 0 ) return false;
	if( extendedProperty._level0_object_type.size() == 0 || extendedProperty._level0_object_name.size() == 0 ) return false;
	if( extendedProperty._level1_object_type.size() == 0 || extendedProperty._level1_object_name.size() == 0 ) return false;

	if( extendedProperty._level2_object_type.size() > 0 && extendedProperty._level2_object_name.size() > 0 )
	{
		query = _T("EXEC sp_dropextendedproperty ?, ?, ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level2_object_name.c_str());
	}
	else
	{
		query = _T("EXEC sp_dropextendedproperty ?, ?, ?, ?, ?");
		if( _dbConn.PrepareQuery(query.c_str()) == false ) return false;

		_dbConn.BindParamInput(extendedProperty._propertyName.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level0_object_name.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_type.c_str());
		_dbConn.BindParamInput(extendedProperty._level1_object_name.c_str());
	}

	if( _dbConn.Execute() == false )
		return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLGetCharacterSets(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET[]>& pCharacterSet)
{
	_tstring query = _T("");

	int32	iCharacterSetSize = DATABASE_CHARACTERSET_STRLEN;
	int32	iCollationSize = DATABASE_CHARACTERSET_STRLEN;
	int32   iDescriptionSize = DATABASE_WVARCHAR_MAX;

	if( ptszCharset != NULL && _tcslen(ptszCharset) > 0 )
		query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE `CHARACTER_SET_NAME` = '" + _tstring(ptszCharset) + "';";
	else query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.CHARACTER_SETS;";

	query = query + "\r\n" + MYSQLGetCharacterSetsQuery(ptszCharset);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iCharsetCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iCharsetCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pCharacterSet = unique_ptr<MYSQL_CHARACTER_SET[]>(new MYSQL_CHARACTER_SET[iCharsetCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MYSQL_CHARACTER_SET), iCharsetCount);

	_dbConn.BindCol(pCharacterSet[0].tszCharacterSet, iCharacterSetSize);
	_dbConn.BindCol(pCharacterSet[0].tszDefaultCollation, iCollationSize);
	_dbConn.BindCol(pCharacterSet[0].tszDescription, iDescriptionSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLGetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_COLLATION[]>& pCollation)
{
	_tstring query = _T("");

	int32	iCharacterSetSize = DATABASE_CHARACTERSET_STRLEN;
	int32	iCollationSize = DATABASE_CHARACTERSET_STRLEN;
	int32   iSize = 10;

	if( ptszCharset != NULL && _tcslen(ptszCharset) > 0 )
		query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.COLLATIONS WHERE `CHARACTER_SET_NAME` = '" + _tstring(ptszCharset) + "';";
	else query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.COLLATIONS;";

	query = query + "\r\n" + MYSQLGetCollationsQuery(ptszCharset);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iCharsetCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iCharsetCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pCollation = unique_ptr<MYSQL_COLLATION[]>(new MYSQL_COLLATION[iCharsetCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MYSQL_COLLATION), iCharsetCount);

	_dbConn.BindCol(pCollation[0].tszCharacterSet, iCharacterSetSize);
	_dbConn.BindCol(pCollation[0].tszCollation, iCollationSize);
	_dbConn.BindCol(pCollation[0].Id);
	_dbConn.BindCol(pCollation[0].tszIsCompiled, iSize);
	_dbConn.BindCol(pCollation[0].tszIsDefault, iSize);
	_dbConn.BindCol(pCollation[0].tszPadAttribute, iSize);
	_dbConn.BindCol(pCollation[0].SortLen);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLGetCharacterSetCollations(const TCHAR* ptszCharset, int32& iCharsetCount, std::unique_ptr<MYSQL_CHARACTER_SET_COLLATION[]>& pCharacterSetCollation)
{
	_tstring query = _T("");

	int32	iCharacterSetSize = DATABASE_CHARACTERSET_STRLEN;
	int32	iCollationSize = DATABASE_CHARACTERSET_STRLEN;

	if( ptszCharset != NULL && _tcslen(ptszCharset) > 0 )
		query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY WHERE `CHARACTER_SET_NAME` = '" + _tstring(ptszCharset) + "';";
	else query = query + "SELECT COUNT(*) AS `charset_count` FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY;";

	query = query + "\r\n" + MYSQLGetCharacterSetCollationsQuery(ptszCharset);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iCharsetCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iCharsetCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pCharacterSetCollation = unique_ptr<MYSQL_CHARACTER_SET_COLLATION[]>(new MYSQL_CHARACTER_SET_COLLATION[iCharsetCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MYSQL_CHARACTER_SET_COLLATION), iCharsetCount);

	_dbConn.BindCol(pCharacterSetCollation[0].tszCharacterSet, iCharacterSetSize);
	_dbConn.BindCol(pCharacterSetCollation[0].tszCollation, iCollationSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLGetStorageEngines(int32& iStorageEngineCount, std::unique_ptr<MYSQL_STORAGE_ENGINE[]>& pStorageEngine)
{
	_tstring query = _T("");

	int32	iEngineSize = 20;
	int32	iCommentSize = DATABASE_WVARCHAR_MAX;
	int32   iSize = 5;

	query = query + "SELECT COUNT(*) AS `engine_count` FROM INFORMATION_SCHEMA.ENGINES;";
	query = query + "\r\n" + MYSQLGetEnginesQuery();

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iStorageEngineCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iStorageEngineCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pStorageEngine = unique_ptr<MYSQL_STORAGE_ENGINE[]>(new MYSQL_STORAGE_ENGINE[iStorageEngineCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MYSQL_STORAGE_ENGINE), iStorageEngineCount);

	_dbConn.BindCol(pStorageEngine[0].tszEngine, iEngineSize);
	_dbConn.BindCol(pStorageEngine[0].tszSupport, iEngineSize);
	_dbConn.BindCol(pStorageEngine[0].tszComment, iCommentSize);
	_dbConn.BindCol(pStorageEngine[0].tszTransactions, iSize);
	_dbConn.BindCol(pStorageEngine[0].tszXA, iSize);
	_dbConn.BindCol(pStorageEngine[0].tszSavepoints, iSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLAlterTable(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation, const TCHAR* ptszEngine)
{
	if( _dbClass != EDBClass::MYSQL ) return false;

	_tstring query = MYSQLGetAlterTableQuery(ptszTableName, ptszCharacterSet, ptszCollation, ptszEngine);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLAlterTableCollation(const TCHAR* ptszTableName, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation)
{
	if( _dbClass != EDBClass::MYSQL ) return false;

	_tstring query = MYSQLGetAlterTableCollationQuery(ptszTableName, ptszCharacterSet, ptszCollation);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLGetTableFragmentationCheck(const TCHAR* ptszTableName, int32& iTableCount, std::unique_ptr<MYSQL_TABLE_FRAGMENTATION[]>& pTableFragmentation)
{
	_tstring query = _T("");

	int32	iTableNameSize = DATABASE_TABLE_NAME_STRLEN;

	if( ptszTableName != NULL && _tcslen(ptszTableName) > 0 )
		query = query + "SELECT COUNT(*) AS `table_count` FROM INFORMATION_SCHEMA.TABLES WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '" + _tstring(ptszTableName) + "';";
	else query = query + "SELECT COUNT(*) AS `table_count` FROM INFORMATION_SCHEMA.TABLES WHERE `TABLE_SCHEMA` = DATABASE();";

	query = query + "\r\n" + MYSQLGetTableFragmentationCheckQuery(ptszTableName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iTableCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iTableCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pTableFragmentation = unique_ptr<MYSQL_TABLE_FRAGMENTATION[]>(new MYSQL_TABLE_FRAGMENTATION[iTableCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(MYSQL_TABLE_FRAGMENTATION), iTableCount);

	_dbConn.BindCol(pTableFragmentation[0].tszTableName, iTableNameSize);
	_dbConn.BindCol(pTableFragmentation[0].TotalSize);
	_dbConn.BindCol(pTableFragmentation[0].DataFreeSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLOptimizeTable(const TCHAR* ptszTableName)
{
	if( _dbClass != EDBClass::MYSQL ) return false;

	_tstring query = MYSQLGetOptimizeTableQuery(ptszTableName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MYSQLShowTable(const TCHAR* ptszTableName)
{
	_tstring query = _T("");
	_tstring body;

	int32	iNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32   iBodySize = DATABASE_WVARCHAR_MAX;
	TCHAR	tszName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszBody[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MYSQL ) return body;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return body;

	query = MYSQLGetShowObjectQuery(EDBObjectType::TABLE, ptszTableName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return body;

	_dbConn.BindCol(tszName, iNameSize);
	_dbConn.BindCol(tszBody, iBodySize);

	if( _dbConn.Execute() == false ) return body;

	while( _dbConn.Fetch() )
	{
		body.append(tszBody);
	}

	return body;
}

//***************************************************************************
//
_tstring CDBQueryProcess::MYSQLShowObject(const EDBObjectType dbObject, const TCHAR* ptszObjectName)
{
	_tstring query = _T("");
	_tstring body;

	int32	iNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32	iBaseSize = DATABASE_BASE_STRLEN;
	int32   iBodySize = DATABASE_TEXT_MAX;
	TCHAR	tszName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszSqlmode[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszBody[DATABASE_TEXT_MAX] = { 0, };
	TCHAR	tszCharacterSetClient[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCollationConnection[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszDatabaseCollation[DATABASE_BASE_STRLEN] = { 0, };

	if( _dbClass != EDBClass::MYSQL ) return body;
	if( ptszObjectName == nullptr || _tcslen(ptszObjectName) < 1 ) return body;

	query = MYSQLGetShowObjectQuery(dbObject, ptszObjectName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return body;

	_dbConn.BindCol(tszName, iNameSize);
	_dbConn.BindCol(tszSqlmode, iBaseSize);
	_dbConn.BindCol(tszBody, iBodySize);
	_dbConn.BindCol(tszCharacterSetClient, iBaseSize);
	_dbConn.BindCol(tszCollationConnection, iBaseSize);
	_dbConn.BindCol(tszDatabaseCollation, iBaseSize);

	if( _dbConn.Execute() == false ) return body;

	while( _dbConn.Fetch() )
	{
		body.append(tszBody);
	}

	return body;
}

//***************************************************************************
//
bool CDBQueryProcess::MYSQLRenameObject(const TCHAR* ptszTableName, const TCHAR* ptszChgName, const TCHAR* ptszColumnName, const TCHAR* ptszDataTypeDesc, bool bIsNullable, const TCHAR* ptszDefaultDefinition, bool bIsIdentity, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MYSQL ) return false;

	_tstring query = MYSQLGetRenameObjectQuery(ptszTableName, ptszChgName, ptszColumnName, ptszDataTypeDesc, bIsNullable, ptszDefaultDefinition, bIsIdentity, ptszCharacterSet, ptszCollation, ptszComment);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// Ex)
//	SELECT `TABLE_COMMENT` AS `tableComment` FROM INFORMATION_SCHEMA.TABLES WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'tbl_table1';
//	SELECT `COLUMN_COMMENT` AS `columncomment` FROM INFORMATION_SCHEMA.COLUMNS WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'tbl_table1' AND `COLUMN_NAME` = 'Id';
_tstring CDBQueryProcess::MYSQLGetTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MYSQL ) return ret;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return ret;

	query = MYSQLGetTableColumnCommentQuery(ptszTableName, ptszColumnName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	ret.resize(iBuffSize);
	ret.assign(tszDescription);

	return ret;
}

//***************************************************************************
// Ex)
//	ALTER TABLE `tbl_table1` COMMENT '테스트 테이블';
//  ALTER TABLE `tbl_table1` MODIFY `Id` VARCHAR(50) NOT NULL COMMENT '아이디';
bool CDBQueryProcess::MYSQLProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDataTypeDesc, bool bIsNullable, const TCHAR* ptszDefaultDefinition, bool bIsIdentity, const TCHAR* ptszCharacterSet, const TCHAR* ptszCollation, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MYSQL ) return false;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return false;

	_tstring query = MYSQLProcessTableColumnCommentQuery(ptszTableName, ptszComment, ptszColumnName, ptszDataTypeDesc, bIsNullable, ptszDefaultDefinition, bIsIdentity, ptszCharacterSet, ptszCollation);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `procComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'PROCEDURE' AND `ROUTINE_NAME` = 'sp_procedure1';
_tstring CDBQueryProcess::MYSQLGetProcedureComment(const TCHAR* ptszProcName)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MYSQL ) return ret;
	if( ptszProcName == nullptr || _tcslen(ptszProcName) < 1 ) return ret;

	query = MYSQLGetProcedureCommentQuery(ptszProcName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	ret.resize(iBuffSize);
	ret.assign(tszDescription);

	return ret;
}

//***************************************************************************
// Ex)
//	ALTER PROCEDURE `sp_procedure1` COMMENT '테스트 저장프로시저';
bool CDBQueryProcess::MYSQLProcessProcedureComment(const TCHAR* ptszProcName, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MYSQL ) return false;
	if( ptszProcName == nullptr || _tcslen(ptszProcName) < 1 ) return false;

	_tstring query = MYSQLProcessProcedureCommentQuery(ptszProcName, ptszComment);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// Ex)
//	SELECT `ROUTINE_COMMENT` AS `funcComment` FROM INFORMATION_SCHEMA.ROUTINES WHERE `ROUTINE_SCHEMA` = DATABASE() AND `ROUTINE_TYPE` = 'FUNCTION' AND `ROUTINE_NAME` = 'sp_function1';
_tstring CDBQueryProcess::MYSQLGetFunctionComment(const TCHAR* ptszFuncName)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::MYSQL ) return ret;
	if( ptszFuncName == nullptr || _tcslen(ptszFuncName) < 1 ) return ret;

	query = MYSQLGetFunctionCommentQuery(ptszFuncName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	ret.resize(iBuffSize);
	ret.assign(tszDescription);

	return ret;
}

//***************************************************************************
// Ex)
//	ALTER FUNCTION `sp_function1` COMMENT '테스트 함수';
bool CDBQueryProcess::MYSQLProcessFunctionComment(const TCHAR* ptszFuncName, const TCHAR* ptszComment)
{
	if( _dbClass != EDBClass::MYSQL ) return false;
	if( ptszFuncName == nullptr || _tcslen(ptszFuncName) < 1 ) return false;

	_tstring query = MYSQLProcessFunctionCommentQuery(ptszFuncName, ptszComment);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// Ex)
//	COMMENT ON TABLE 테이블명 IS '코멘트';
//  COMMENT ON COLUMN 테이블명.컬럼명 IS '코멘트';
bool CDBQueryProcess::ORACLEProcessTableColumnComment(const TCHAR* ptszTableName, const TCHAR* ptszColumnName, const TCHAR* ptszDescription)
{
	if( _dbClass != EDBClass::ORACLE ) return false;
	if( ptszTableName == nullptr || _tcslen(ptszTableName) < 1 ) return false;

	_tstring query = ORACLEProcessTableColumnCommentQuery(ptszTableName, ptszDescription, ptszColumnName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// Ex)
//  SELECT DBMS_METADATA.GET_DDL('TABLE', '테이블명') SCRIPT FROM DUAL
//  SELECT DBMS_METADATA.GET_DDL('INDEX', '인덱스명') SCRIPT FROM DUAL
_tstring CDBQueryProcess::ORACLEMetaDataGetDDL(const EDBObjectType dbObject, const TCHAR* ptszObjectName, const TCHAR* ptszSchemaName)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::ORACLE ) return ret;
	if( ptszObjectName == nullptr || _tcslen(ptszObjectName) < 1 ) return ret;

	query = ORACLEMetaDataGetDDLQuery(dbObject, ptszObjectName, ptszSchemaName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	ret.resize(iBuffSize);
	ret.assign(tszDescription);

	return ret;
}

//***************************************************************************
// 
_tstring CDBQueryProcess::ORACLEGetUserSource(const EDBObjectType dbObject, const TCHAR* ptszObjectName)
{
	_tstring query = _T("");
	_tstring ret;

	int32		iBuffSize = DATABASE_WVARCHAR_MAX;
	TCHAR		tszDescription[DATABASE_WVARCHAR_MAX] = { 0, };

	if( _dbClass != EDBClass::ORACLE ) return ret;
	if( ptszObjectName == nullptr || _tcslen(ptszObjectName) < 1 ) return ret;

	query = ORACLEGetUserSourceQuery(dbObject, ptszObjectName);
	if( _dbConn.PrepareQuery(query.c_str()) == false ) return ret;

	_dbConn.BindCol(tszDescription, iBuffSize);

	if( _dbConn.Execute() == false ) return ret;
	if( _dbConn.Fetch() == false ) return ret;

	// 결과 레코드가 1개인 경우에만 성공 처리
	int64 iRowCount = _dbConn.RowCount();
	if( iRowCount != 1 ) return ret;

	ret.resize(iBuffSize);
	ret.assign(tszDescription);

	return ret;
}

//***************************************************************************
// 
bool CDBQueryProcess::ORACLEGetAnalyzeIndexFragmentationCheck(const TCHAR* ptszIndexName)
{
	if( _dbClass != EDBClass::ORACLE ) return false;
	if( ptszIndexName == nullptr || _tcslen(ptszIndexName) < 1 ) return false;

	_tstring query = ORACLEGetAnalyzeIndexFragmentationCheckQuery(ptszIndexName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// 
bool CDBQueryProcess::ORACLEGetIndexFragmentationCheck(const TCHAR* ptszIndexName, int32& iCount, std::unique_ptr<ORACLE_INDEX_FRAGMENTATION[]>& pIndexFragmentation)
{
	_tstring query = _T("");

	int32	iIndexNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32	iOKSize = 16;
	int32	iLastAnalyzedSize = DATETIME_STRLEN;

	query = query + "SELECT COUNT(*) AS `count` FROM SYS.USER_INDEXES WHERE INDEX_NAME = '" + _tstring(ptszIndexName) + "';";

	query = query + "\r\n" + ORACLEGetIndexFragmentationCheckQuery(ptszIndexName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pIndexFragmentation = unique_ptr<ORACLE_INDEX_FRAGMENTATION[]>(new ORACLE_INDEX_FRAGMENTATION[iCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(ORACLE_INDEX_FRAGMENTATION), iCount);

	_dbConn.BindCol(pIndexFragmentation[0].tszIndexName, iIndexNameSize);
	_dbConn.BindCol(pIndexFragmentation[0].BLevel);
	_dbConn.BindCol(pIndexFragmentation[0].tszOK, iOKSize);
	_dbConn.BindCol(pIndexFragmentation[0].tszLastAnalyzed, iLastAnalyzedSize);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
// 
bool CDBQueryProcess::ORACLEGetAnalyzeIndexStatFragmentationCheck(const TCHAR* ptszIndexName)
{
	if( _dbClass != EDBClass::ORACLE ) return false;
	if( ptszIndexName == nullptr || _tcslen(ptszIndexName) < 1 ) return false;

	_tstring query = ORACLEGetAnalyzeIndexStatFragmentationCheckQuery(ptszIndexName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}

//***************************************************************************
// 
bool CDBQueryProcess::ORACLEGetIndexStatFragmentationCheck(const TCHAR* ptszIndexName, int32& iCount, std::unique_ptr<ORACLE_INDEX_STAT_FRAGMENTATION[]>& pIndexStatFragmentation)
{
	_tstring query = _T("");

	int32	iIndexNameSize = DATABASE_OBJECT_NAME_STRLEN;
	int32	iOKSize = 16;
	int32	iLastAnalyzedSize = DATETIME_STRLEN;

	query = query + "SELECT COUNT(*) AS `count` FROM SYS.INDEX_STATS WHERE INDEX_NAME = '" + _tstring(ptszIndexName) + "';";

	query = query + "\r\n" + ORACLEGetIndexStatFragmentationCheckQuery(ptszIndexName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	_dbConn.BindCol(iCount);

	if( _dbConn.Fetch() == false ) return false;

	if( iCount == 0 ) return false;

	if( _dbConn.MoreResults() != SQL_SUCCESS ) return false;

	pIndexStatFragmentation = unique_ptr<ORACLE_INDEX_STAT_FRAGMENTATION[]>(new ORACLE_INDEX_STAT_FRAGMENTATION[iCount]);

	_dbConn.UnBindColStmt();
	_dbConn.AllSets(sizeof(ORACLE_INDEX_STAT_FRAGMENTATION), iCount);

	_dbConn.BindCol(pIndexStatFragmentation[0].tszIndexName, iIndexNameSize);
	_dbConn.BindCol(pIndexStatFragmentation[0].PctDeleted);
	_dbConn.BindCol(pIndexStatFragmentation[0].Distinctiveness);

	if( _dbConn.Fetch() == false ) return false;

	return true;
}

//***************************************************************************
// 
bool CDBQueryProcess::ORACLEGetIndexRebuild(const TCHAR* ptszIndexName)
{
	if( _dbClass != EDBClass::ORACLE ) return false;
	if( ptszIndexName == nullptr || _tcslen(ptszIndexName) < 1 ) return false;

	_tstring query = ORACLEGetIndexRebuildQuery(ptszIndexName);

	if( _dbConn.ExecDirect(query.c_str()) == false ) return false;

	return true;
}
