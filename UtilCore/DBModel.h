
//***************************************************************************
// DBModel.h : interface for the Database Model.
//
//***************************************************************************

#ifndef __DBMODEL_H__
#define __DBMODEL_H__

#include <regex>

NAMESPACE_BEGIN(DBModel)

USING_SHARED_PTR(Column);
USING_SHARED_PTR(IndexColumn);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(IndexOption);
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

class Column
{
public:
	_tstring			CreateColumn(EDBClass dbClass);
	_tstring			ModifyColumn(EDBClass dbClass);
	_tstring			DropColumn(EDBClass dbClass);
	_tstring			CreateText(EDBClass dbClass);
	_tstring			CreateDefaultConstraint(EDBClass dbClass);
	_tstring			DropDefaultConstraint(EDBClass dbClass);

public:
	_tstring			_schemaName;
	_tstring			_tableName;
	_tstring			_seq;
	_tstring			_name;
	_tstring			_desc;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	bool				_nullable = false;
	bool				_identity = false;
	_tstring			_identitydesc;
	int64				_seedValue = 0;
	int64				_incrementValue = 0;
	_tstring			_characterset;
	_tstring			_collation;
	_tstring			_defaultConstraintName;
	_tstring			_defaultDefinition;
};

class IndexColumn
{
public:
	_tstring	GetSortText();

public:
	_tstring	_seq;
	_tstring	_name;
	EIndexSort	_sort = EIndexSort::ASC;
};

class IndexOption
{
public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_name;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	bool					_isDisabled = false;
	bool					_isPadded = false;
	int8					_fillFactor;
	bool					_ignoreDupKey = false;
	bool					_allowRowLocks = false;
	bool					_allowPageLocks = false;
	bool					_hasFilter = false;
	_tstring				_filterDefinition;
	int32					_compressionDelay;
	_tstring				_optimizeForSequentialKey;
	bool					_statisticsNoRecompute = false;
	bool					_statisticsIncremental = false;
	int8					_dataCompression;
	_tstring				_dataCompressionDesc;
	bool					_xmlCompression = false;
	_tstring				_xmlCompressionDesc;
	_tstring				_fileGroupOrPartitionScheme;
	_tstring				_fileGroupOrPartitionSchemeName;
};

class Index
{
public:
	_tstring			CreateIndex(EDBClass dbClass);
	_tstring			DropIndex(EDBClass dbClass);

	_tstring			GetIndexName(EDBClass dbClass);
	_tstring			GetKindText();
	_tstring			GetTypeText();
	_tstring			GetKeyText();
	_tstring			CreateColumnsText(EDBClass dbClass);
	bool				DependsOn(const _tstring& columnName);

public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_name;			
	int32					_indexId = 0;		
	EIndexKind				_kind = EIndexKind::NONCLUSTERED;
	EIndexType				_type = EIndexType::NONE;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	bool					_systemNamed = false;
	CVector<IndexColumnRef>	_columns;
};

class ForeignKey
{
public:
	_tstring			CreateForeignKey(EDBClass dbClass);
	_tstring			DropForeignKey(EDBClass dbClass);

	_tstring			GetForeignKeyName(EDBClass dbClass);
	_tstring			CreateColumnsText(EDBClass dbClass, CVector<IndexColumnRef> columns);

public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_foreignKeyName;
	_tstring				_updateRule;
	_tstring				_deleteRule;

	_tstring				_foreignKeyTableName;
	CVector<IndexColumnRef>	_foreignKeyColumns;
	_tstring				_referenceKeyTableName;
	CVector<IndexColumnRef>	_referenceKeyColumns;
};

class Trigger
{
public:
	int32			_objectId = 0;
	_tstring		_schemaName;
	_tstring		_tableName;
	_tstring		_triggerName;
	_tstring		_fullBody;
};

class Table
{
public:
	_tstring			CreateTable(EDBClass dbClass);
	_tstring			DropTable(EDBClass dbClass);
	ColumnRef			FindColumn(const _tstring& columnName);

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_auto_increment_value;
	_tstring					_storageEngine;
	_tstring					_characterset;
	_tstring					_collation;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<ColumnRef>			_columns;
	CVector<IndexRef>			_indexes;
	CVector<IndexOptionRef>		_indexOptions;
	CVector<ForeignKeyRef>		_foreignKeys;
};

class ProcParam
{
public:
	_tstring			_paramId;
	EParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_desc;
};

class Procedure
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_fullBody;
	_tstring					_body;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<ProcParamRef>		_parameters;
};

class FuncParam
{
public:
	_tstring			_paramId;
	EParameterMode		_paramMode;
	_tstring			_name;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_desc;
};

class Function
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_name;
	_tstring					_desc;
	_tstring					_fullBody;
	_tstring					_body;
	_tstring					_createDate;
	_tstring					_modifyDate;
	CVector<FuncParamRef>		_parameters;
};

class Helpers
{
public:
	static _tstring			RemoveWhiteSpace(const _tstring& str);
	static void				LogFileWrite(EDBClass dbClass, _tstring title, _tstring sql, bool newline = false);
};

NAMESPACE_END

#endif // ndef __DBMODEL_H__
