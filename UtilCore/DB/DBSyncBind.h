
//***************************************************************************
// DBSyncBind.h : implementation for the DBSync Bind.
//
//***************************************************************************

#ifndef __DBSYNCBIND_H__
#define __DBSYNCBIND_H__

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

#ifndef __DBBIND_H__
#include <DBBind.h> 
#endif

#ifndef __DBMODEL_H__
#include <DBModel.h> 
#endif

namespace SP
{
	class GetDBTables : public CDBBind<0, 11>
	{
	public:
		EDBClass _dbClass;

	public:
		GetDBTables(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetTableInfoQuery(dbClass, ptszTableName))
		{
			_dbClass = dbClass;
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_AutoIncrementValue(OUT uint64& value)
		{
			// 오라클 ODBC 드라이버가 SQL_C_SBIGINT, SQL_C_UBIGINT 타입을 지원하지 않아서 SQL_C_DOUBLE 타입으로 예외 처리
			if( _dbClass == EDBClass::ORACLE ) 
				BindCol(4, SQL_C_DOUBLE, value);
			else BindCol(4, value);
		}
		template<int32 N> void Out_StorageEngine(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_CharacterSet(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_Collation(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		template<int32 N> void Out_TableComment(OUT TCHAR(&value)[N])
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
	};

	class GetDBTableColumns : public CDBBind<0, 20>
	{
	public:
		EDBClass _dbClass;

	public:
		GetDBTableColumns(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetTableColumnInfoQuery(dbClass, ptszTableName))
		{
			_dbClass = dbClass;
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_Seq(OUT int32& value)
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		void Out_MaxLength(OUT int64& value)
		{
			if( _dbClass == EDBClass::ORACLE )
				BindCol(7, SQL_C_DOUBLE, value);
			else BindCol(7, value);
		}
		void Out_Precision(OUT uint8& value)
		{
			BindCol(8, value);
		}
		void Out_Scale(OUT uint8& value)
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_DataTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
		void Out_IsNullable(OUT BOOL& value)
		{
			BindCol(11, value);
		}
		void Out_IsIdentity(OUT BOOL& value)
		{
			BindCol(12, value);
		}
		void Out_SeedValue(OUT int64& value)
		{
			if( _dbClass == EDBClass::ORACLE )
				BindCol(13, SQL_C_DOUBLE, value);
			else BindCol(13, value);
		}
		void Out_IncrementValue(OUT int64& value)
		{
			if( _dbClass == EDBClass::ORACLE )
				BindCol(14, SQL_C_DOUBLE, value);
			else BindCol(14, value);
		}
		template<int32 N> void Out_DefaultConstraintName(OUT TCHAR(&value)[N])
		{
			BindCol(15, value);
		}
		template<int32 N> void Out_DefaultDefinition(OUT TCHAR(&value)[N])
		{
			BindCol(16, value);
		}
		template<int32 N> void Out_CharacterSet(OUT TCHAR(&value)[N])
		{
			BindCol(17, value);
		}
		template<int32 N> void Out_Collation(OUT TCHAR(&value)[N])
		{
			BindCol(18, value);
		}
		template<int32 N> void Out_ColumnComment(OUT TCHAR(&value)[N])
		{
			BindCol(19, value);
		}
	};

	class GetDBConstraints : public CDBBind<0, 11>
	{
	public:
		GetDBConstraints(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetConstraintsInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_ConstName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ConstType(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ConstTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_ConstValue(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		void Out_IsSystemNamed(OUT BOOL& value)
		{
			BindCol(8, value);
		}
		void Out_IsStatus(OUT BOOL& value)
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_SortValue(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
	};

	class GetDBIdentityColumns : public CDBBind<0, 20>
	{
	public:
		GetDBIdentityColumns(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetTableIdentityColumnInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_ColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_IdentityColumn(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_DefaultOnNull(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_GenerationType(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_SequenceName(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		template<int32 N> void Out_MinValue(OUT TCHAR(&value)[N])
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_MaxValue(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_IncrementBy(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
		template<int32 N> void Out_CycleFlag(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
		template<int32 N> void Out_OrderFlag(OUT TCHAR(&value)[N])
		{
			BindCol(12, value);
		}
		template<int32 N> void Out_CacheSize(OUT TCHAR(&value)[N])
		{
			BindCol(13, value);
		}
		template<int32 N> void Out_LastNumber(OUT TCHAR(&value)[N])
		{
			BindCol(14, value);
		}
		template<int32 N> void Out_ScaleFlag(OUT TCHAR(&value)[N])
		{
			BindCol(15, value);
		}
		template<int32 N> void Out_ExtendFlag(OUT TCHAR(&value)[N])
		{
			BindCol(16, value);
		}
		template<int32 N> void Out_ShardedFlag(OUT TCHAR(&value)[N])
		{
			BindCol(17, value);
		}
		template<int32 N> void Out_SessionFlag(OUT TCHAR(&value)[N])
		{
			BindCol(18, value);
		}
		template<int32 N> void Out_KeepValue(OUT TCHAR(&value)[N])
		{
			BindCol(19, value);
		}
	};

	class GetDBIndexes : public CDBBind<0, 13>
	{
	public:
		GetDBIndexes(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetIndexInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_IndexName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		void Out_IndexId(OUT int32& value)
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_IndexType(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		void Out_IsPrimaryKey(OUT BOOL& value)
		{
			BindCol(7, value);
		}
		void Out_IsUnique(OUT BOOL& value)
		{
			BindCol(8, value);
		}
		void Out_ColumnSeq(OUT int32& value)
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_ColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
		void Out_ColumnSort(OUT int8& value)
		{
			BindCol(11, value);
		}
		void Out_IsSystemNamed(OUT BOOL& value)
		{
			BindCol(12, value);
		}
	};

	class GetDBIndexOptions : public CDBBind<0, 26>
	{
	public:
		GetDBIndexOptions(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetIndexOptionInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_IndexName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_IndexType(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		void Out_IsPrimaryKey(OUT BOOL& value)
		{
			BindCol(6, value);
		}
		void Out_IsUnique(OUT BOOL& value)
		{
			BindCol(7, value);
		}
		void Out_IsDisabled(OUT BOOL& value)
		{
			BindCol(8, value);
		}
		void Out_IsPadded(OUT BOOL& value)
		{
			BindCol(9, value);
		}
		void Out_FillFactor(OUT int8& value)
		{
			BindCol(10, value);
		}
		void Out_IgnoreDupKey(OUT BOOL& value)
		{
			BindCol(11, value);
		}
		void Out_AllowRowLocks(OUT BOOL& value)
		{
			BindCol(12, value);
		}
		void Out_AllowPageLocks(OUT BOOL& value)
		{
			BindCol(13, value);
		}
		void Out_HasFilter(OUT BOOL& value)
		{
			BindCol(14, value);
		}
		template<int32 N> void Out_FilterDefinition(OUT TCHAR(&value)[N])
		{
			BindCol(15, value);
		}
		void Out_CompressionDelay(OUT int32& value)
		{
			BindCol(16, value);
		}
		void Out_OptimizeForSequentialKey(OUT BOOL& value)
		{
			BindCol(17, value);
		}
		void Out_StatisticsNoRecompute(OUT BOOL& value)
		{
			BindCol(18, value);
		}
		void Out_StatisticsIncremental(OUT BOOL& value)
		{
			BindCol(19, value);
		}
		void Out_DataCompression(OUT int8& value)
		{
			BindCol(20, value);
		}
		template<int32 N> void Out_DataCompressionDesc(OUT TCHAR(&value)[N])
		{
			BindCol(21, value);
		}
		void Out_XmlCompression(OUT BOOL& value)
		{
			BindCol(22, value);
		}
		template<int32 N> void Out_XmlCompressionDesc(OUT TCHAR(&value)[N])
		{
			BindCol(23, value);
		}
		template<int32 N> void Out_FileGroupOrPartitionScheme(OUT TCHAR(&value)[N])
		{
			BindCol(24, value);
		}
		template<int32 N> void Out_FileGroupOrPartitionSchemeName(OUT TCHAR(&value)[N])
		{
			BindCol(25, value);
		}
	};

	class GetDBForeignKeys : public CDBBind<0, 15>
	{
	public:
		GetDBForeignKeys(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetForeignKeyInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_ForeignKeyName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		void Out_IsDisabled(OUT BOOL& value)
		{
			BindCol(5, value);
		}
		void Out_IsNotTrusted(OUT BOOL& value)
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_ForeignKeyTableName(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		template<int32 N> void Out_ForeignKeyColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_ReferenceKeySchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
		template<int32 N> void Out_ReferenceKeyTableName(OUT TCHAR(&value)[N])
		{
			BindCol(10, value);
		}
		template<int32 N> void Out_ReferenceKeyColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
		template<int32 N> void Out_UpdateRule(OUT TCHAR(&value)[N])
		{
			BindCol(12, value);
		}
		template<int32 N> void Out_DeleteRule(OUT TCHAR(&value)[N])
		{
			BindCol(13, value);
		}
		void Out_IsSystemNamed(OUT BOOL& value)
		{
			BindCol(14, value);
		}
	};

	class GetDBDefaultConstraints : public CDBBind<0, 8>
	{
	public:
		GetDBDefaultConstraints(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetDefaultConstInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_DefaultConstName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_DefaultValue(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		void Out_IsSystemNamed(OUT BOOL& value)
		{
			BindCol(7, value);
		}
	};

	class GetDBCheckConstraints : public CDBBind<0, 7>
	{
	public:
		GetDBCheckConstraints(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetCheckConstInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_CheckConstName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_CheckValue(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		void Out_IsSystemNamed(OUT BOOL& value)
		{
			BindCol(6, value);
		}
	};

	class GetDBTriggers : public CDBBind<0, 5>
	{
	public:
		GetDBTriggers(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszTableName) : CDBBind(conn, GetTriggerInfoQuery(dbClass, ptszTableName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_TableName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_TriggerName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
	};

	class GetDBStoredProcedures : public CDBBind<0, 7>
	{
	public:
		GetDBStoredProcedures(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszProcName) : CDBBind(conn, GetProcedureInfoQuery(dbClass, ptszProcName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_SPName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_SPComment(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
	};

	class GetDBStoredProcedureParams : public CDBBind<0, 13>
	{
	public:
		GetDBStoredProcedureParams(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszProcName) : CDBBind(conn, GetProcedureParamInfoQuery(dbClass, ptszProcName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_SPName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_ParamId(OUT int32& value)
		{
			BindCol(4, value);
		}
		void Out_ParamMode(OUT int8& value)
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ParamName(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		void Out_MaxLength(OUT int64& value)
		{
			BindCol(8, value);
		}
		void Out_Precision(OUT uint8& value)
		{
			BindCol(9, value);
		}
		void Out_Scale(OUT uint8& value)
		{
			BindCol(10, value);
		}
		template<int32 N> void Out_DataTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
		template<int32 N> void Out_ParamComment(OUT TCHAR(&value)[N])
		{
			BindCol(12, value);
		}
	};

	class GetDBFunctions : public CDBBind<0, 7>
	{
	public:
		GetDBFunctions(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszFuncName) : CDBBind(conn, GetFunctionInfoQuery(dbClass, ptszFuncName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_FCName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_FCComment(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
	};

	class GetDBFunctionParams : public CDBBind<0, 13>
	{
	public:
		GetDBFunctionParams(EDBClass dbClass, CBaseODBC& conn, const TCHAR* ptszFuncName) : CDBBind(conn, GetFunctionParamInfoQuery(dbClass, ptszFuncName))
		{
		}
		template<int32 N> void Out_DBName(OUT TCHAR(&value)[N])
		{
			BindCol(0, value);
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_FCName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_ParamId(OUT int32& value)
		{
			BindCol(4, value);
		}
		void Out_ParamMode(OUT int8& value)
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ParamName(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		void Out_MaxLength(OUT int64& value)
		{
			BindCol(8, value);
		}
		void Out_Precision(OUT uint8& value)
		{
			BindCol(9, value);
		}
		void Out_Scale(OUT uint8& value)
		{
			BindCol(10, value);
		}
		template<int32 N> void Out_DataTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
		template<int32 N> void Out_ParamComment(OUT TCHAR(&value)[N])
		{
			BindCol(12, value);
		}
	};
}

#endif // ndef __DBSYNCBIND_H__