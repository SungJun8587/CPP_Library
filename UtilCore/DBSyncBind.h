
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

namespace SP
{
	class GetDBTables : public CDBBind<0, 10>
	{
	public:
		GetDBTables(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetTableInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		void Out_AutoIncrementValue(OUT int64& value)
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_StorageEngine(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_CharacterSet(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_Collation(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_TableComment(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
	};

	class GetDBTableColumns : public CDBBind<0, 19>
	{
	public:
		GetDBTableColumns(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetTableColumnInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		void Out_Seq(OUT int32& value)
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_ColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		void Out_MaxLength(OUT uint64& value)
		{
			BindCol(6, value);
		}
		void Out_Precision(OUT uint8& value)
		{
			BindCol(7, value);
		}
		void Out_Scale(OUT uint8& value)
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_DataTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
		void Out_IsNullable(OUT BOOL& value)
		{
			BindCol(10, value);
		}
		void Out_IsIdentity(OUT BOOL& value)
		{
			BindCol(11, value);
		}
		void Out_SeedValue(OUT int64& value)
		{
			BindCol(12, value);
		}
		void Out_IncrementValue(OUT int64& value)
		{
			BindCol(13, value);
		}
		template<int32 N> void Out_DefaultConstraintName(OUT WCHAR(&value)[N])
		{
			BindCol(14, value);
		}
		template<int32 N> void Out_DefaultDefinition(OUT TCHAR(&value)[N])
		{
			BindCol(15, value);
		}
		template<int32 N> void Out_CharacterSet(OUT TCHAR(&value)[N])
		{
			BindCol(16, value);
		}
		template<int32 N> void Out_Collation(OUT TCHAR(&value)[N])
		{
			BindCol(17, value);
		}
		template<int32 N> void Out_ColumnComment(OUT TCHAR(&value)[N])
		{
			BindCol(18, value);
		}
	};

	class GetDBIndexes : public CDBBind<0, 12>
	{
	public:
		GetDBIndexes(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetIndexInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		template<int32 N> void Out_IndexName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_IndexId(OUT int32& value)
		{
			BindCol(4, value);
		}
		void Out_IndexKind(OUT int8& value)
		{
			BindCol(5, value);
		}
		void Out_IndexType(OUT int8& value)
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
	};

	class GetDBIndexOptions : public CDBBind<0, 22>
	{
	public:
		GetDBIndexOptions(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetIndexOptionInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		template<int32 N> void Out_IndexName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		void Out_IsPrimaryKey(OUT BOOL& value)
		{
			BindCol(4, value);
		}
		void Out_IsUnique(OUT BOOL& value)
		{
			BindCol(5, value);
		}
		void Out_IsDisabled(OUT BOOL& value)
		{
			BindCol(6, value);
		}
		void Out_IsPadded(OUT BOOL& value)
		{
			BindCol(7, value);
		}
		void Out_FillFactor(OUT int8& value)
		{
			BindCol(8, value);
		}
		void Out_IgnoreDupKey(OUT BOOL& value)
		{
			BindCol(9, value);
		}
		void Out_AllowRowLocks(OUT BOOL& value)
		{
			BindCol(10, value);
		}
		void Out_AllowPageLocks(OUT BOOL& value)
		{
			BindCol(11, value);
		}
		void Out_HasFilter(OUT BOOL& value)
		{
			BindCol(12, value);
		}
		template<int32 N> void Out_FilterDefinition(OUT TCHAR(&value)[N])
		{
			BindCol(13, value);
		}
		void Out_StatisticsNoRecompute(OUT BOOL& value)
		{
			BindCol(14, value);
		}
		void Out_DataCompression(OUT int8& value)
		{
			BindCol(15, value);
		}
		template<int32 N> void Out_DataCompressionDesc(OUT TCHAR(&value)[N])
		{
			BindCol(16, value);
		}
		template<int32 N> void Out_DataSpaceName(OUT TCHAR(&value)[N])
		{
			BindCol(17, value);
		}
		template<int32 N> void Out_DataSpaceTypeDesc(OUT TCHAR(&value)[N])
		{
			BindCol(18, value);
		}

		/*
		template<int32 N> void Out_OptimizeForSequentialKey(OUT TCHAR(&value)[N])
		{
			BindCol(15, value);
		}
		void Out_Resumable(OUT BOOL& value)
		{
			BindCol(16, value);
		}
		void Out_MaxDop(OUT int16& value)
		{
			BindCol(17, value);
		}
		void Out_XmlCompression(OUT BOOL& value)
		{
			BindCol(20, value);
		}
		template<int32 N> void Out_XmlCompressionDesc(OUT TCHAR(&value)[N])
		{
			BindCol(21, value);
		}
		*/
	};

	class GetDBForeignKeys : public CDBBind<0, 10>
	{
	public:
		GetDBForeignKeys(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetForeignKeyInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		template<int32 N> void Out_ForeignKeyName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_ForeignKeyTableName(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ForeignKeyColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_ReferenceKeyTableName(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		template<int32 N> void Out_ReferenceKeyColumnName(OUT TCHAR(&value)[N])
		{
			BindCol(7, value);
		}
		template<int32 N> void Out_UpdateRule(OUT TCHAR(&value)[N])
		{
			BindCol(8, value);
		}
		template<int32 N> void Out_DeleteRule(OUT TCHAR(&value)[N])
		{
			BindCol(9, value);
		}
	};

	class GetDBTriggers : public CDBBind<0, 4>
	{
	public:
		GetDBTriggers(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetTriggerInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
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
		template<int32 N> void Out_TriggerName(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
	};

	class GetDBStoredProcedures : public CDBBind<0, 6>
	{
	public:
		GetDBStoredProcedures(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetStoredProcedureInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(0, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SPName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_SPComment(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
	};

	class GetDBStoredProcedureParams : public CDBBind<0, 12>
	{
	public:
		GetDBStoredProcedureParams(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetStoredProcedureParamInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(0, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_SPName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		void Out_ParamId(OUT int32& value)
		{
			BindCol(3, value);
		}
		void Out_ParamMode(OUT int8& value)
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ParamName(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		void Out_MaxLength(OUT uint64& value)
		{
			BindCol(7, value);
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
		template<int32 N> void Out_ParamComment(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
	};

	class GetDBFunctions : public CDBBind<0, 6>
	{
	public:
		GetDBFunctions(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetFunctionInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(0, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_FCName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		template<int32 N> void Out_FCComment(OUT TCHAR(&value)[N])
		{
			BindCol(3, value);
		}
		template<int32 N> void Out_CreateDate(OUT TCHAR(&value)[N])
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ModifyDate(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
	};

	class GetDBFunctionParams : public CDBBind<0, 12>
	{
	public:
		GetDBFunctionParams(EDBClass dbClass, CBaseODBC& conn) : CDBBind(conn, GetFunctionParamInfo(dbClass))
		{
		}
		void Out_ObjectId(OUT int32& value)
		{
			BindCol(0, value);
		}
		template<int32 N> void Out_SchemaName(OUT TCHAR(&value)[N])
		{
			BindCol(1, value);
		}
		template<int32 N> void Out_FCName(OUT TCHAR(&value)[N])
		{
			BindCol(2, value);
		}
		void Out_ParamId(OUT int32& value)
		{
			BindCol(3, value);
		}
		void Out_ParamMode(OUT int8& value)
		{
			BindCol(4, value);
		}
		template<int32 N> void Out_ParamName(OUT TCHAR(&value)[N])
		{
			BindCol(5, value);
		}
		template<int32 N> void Out_DataType(OUT TCHAR(&value)[N])
		{
			BindCol(6, value);
		}
		void Out_MaxLength(OUT uint64& value)
		{
			BindCol(7, value);
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
		template<int32 N> void Out_ParamComment(OUT TCHAR(&value)[N])
		{
			BindCol(11, value);
		}
	};
}

#endif // ndef __DBSYNCBIND_H__