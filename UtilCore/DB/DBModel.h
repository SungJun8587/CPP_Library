
//***************************************************************************
// DBModel.h : interface for the Database Model.
//
//***************************************************************************

#ifndef __DBMODEL_H__
#define __DBMODEL_H__

#include <regex>

NAMESPACE_BEGIN(DBModel)

USING_SHARED_PTR(Column);
USING_SHARED_PTR(IdentityColumn);
USING_SHARED_PTR(IndexColumn);
USING_SHARED_PTR(Index);
USING_SHARED_PTR(IndexOption);
USING_SHARED_PTR(ForeignKey);
USING_SHARED_PTR(DefaultConstraint);
USING_SHARED_PTR(CheckConstraint);
USING_SHARED_PTR(Table);
USING_SHARED_PTR(Trigger);
USING_SHARED_PTR(ProcParam);
USING_SHARED_PTR(Procedure);
USING_SHARED_PTR(FuncParam);
USING_SHARED_PTR(Function);

class Column
{
public:
	Column(EDBClass dbClass) { _dbSystemInfo.DBClass = dbClass; _pORACLEDBTableIdentityCols = NULL; };
	Column(DB_SYSTEM_INFO dbSystemInfo, ORACLEDBTableIdentityColumn* pORACLEDBTableIdentityCols = NULL) { _dbSystemInfo = dbSystemInfo; _pORACLEDBTableIdentityCols = pORACLEDBTableIdentityCols; };

	_tstring			CreateColumn();
	_tstring			AlterColumn();
	_tstring			DropColumn();
	_tstring			ChangeColumnName(const _tstring chgColumnName);
	_tstring			CreateColumnText();

public:
	_tstring			_schemaName;
	_tstring			_tableName;
	_tstring			_seq;
	_tstring			_columnName;
	_tstring			_columnComment;
	_tstring			_datatype;
	int64				_maxLength = 0;
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

	DB_SYSTEM_INFO				_dbSystemInfo;
	ORACLEDBTableIdentityColumn* _pORACLEDBTableIdentityCols;
};

class IdentityColumn
{
public:
	_tstring			_schemaName;
	_tstring			_tableName;
	_tstring			_columnName;
	_tstring			_identityColumn;
	_tstring			_defaultOnNull;
	_tstring			_generationType;
	_tstring			_sequenceName;
	uint64				_minValue;
	uint64				_maxValue;
	uint64				_incrementBy;
	_tstring			_cycleFlag;
	_tstring			_orderFlag;
	uint64				_cacheSize;
	uint64				_lastNumber;
	_tstring			_scaleFlag;
	_tstring			_extendFlag;
	_tstring			_shardedFlag;
	_tstring			_sessionFlag;
	_tstring			_keepValue;
};

class IndexColumn
{
public:
	_tstring	GetSortText();

public:
	_tstring	_seq;
	_tstring	_columnName;
	EIndexSort	_sort = EIndexSort::ASC;
};

class IndexOption
{
public:
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_indexName;
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
	Index(EDBClass dbClass) { _dbClass = dbClass; };

	_tstring			CreateIndex();
	_tstring			DropIndex();

	_tstring			GetIndexName();
	EIndexKind			GetStringToIndexKind();
	_tstring			GetKeyText();
	_tstring			ChangeIndexName(const _tstring chgIndexName);
	_tstring			CreateColumnsText(bool isOrderBy = true);
	bool				DependsOn(const _tstring& columnName);

public:
	EDBClass				_dbClass;
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_indexName;
	int32					_indexId = 0;
	EIndexKind				_kind = EIndexKind::NONCLUSTERED;
	_tstring				_type;
	bool					_primaryKey = false;
	bool					_uniqueKey = false;
	bool					_systemNamed = false;
	CVector<IndexColumnRef>	_columns;
};

class ForeignKey
{
public:
	ForeignKey(EDBClass dbClass) { _dbClass = dbClass; };

	_tstring			CreateForeignKey();
	_tstring			AlterForeignKey();
	_tstring			AlterForeignKeyCheckConstraint();
	_tstring			DropForeignKey();

	_tstring			GetForeignKeyName();
	_tstring			CreateColumnsText(CVector<IndexColumnRef> columns);

public:
	EDBClass				_dbClass;
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

class DefaultConstraint
{
public:
	DefaultConstraint(EDBClass dbClass) { _dbClass = dbClass; };

	EDBClass				_dbClass;
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_defaultConstName;
	_tstring				_columnName;
	_tstring				_defaultValue;
	bool					_systemNamed = false;

public:
	_tstring			CreateDefaultConstraint();
	_tstring			DropDefaultConstraint();
};

class CheckConstraint
{
public:
	CheckConstraint(EDBClass dbClass) { _dbClass = dbClass; };

	EDBClass				_dbClass;
	_tstring				_schemaName;
	_tstring				_tableName;
	_tstring				_checkConstName;
	_tstring				_checkValue;
	bool					_systemNamed = false;

public:
	_tstring			CreateCheckConstraint();
	_tstring			DropCheckConstraint();
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
	Table(EDBClass dbClass) { _dbClass = dbClass; }

	_tstring			CreateTable();
	_tstring			AlterTableCollationEngine();
	_tstring			DropTable();
	_tstring			ChangeTableName(const _tstring chgTableName);
	ColumnRef			FindColumn(const _tstring& columnName);

public:
	EDBClass					_dbClass;
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_tableName;
	_tstring					_tableComment;
	_tstring					_auto_increment_value;
	_tstring					_storageEngine;
	_tstring					_characterset;
	_tstring					_collation;
	_tstring					_createDate;
	_tstring					_modifyDate;

	CVector<ColumnRef>				_columns;
	CVector<IdentityColumnRef>		_identityColumns;
	CVector<IndexRef>				_indexes;
	CVector<IndexOptionRef>			_indexOptions;
	CVector<ForeignKeyRef>			_foreignKeys;
	CVector<DefaultConstraintRef>	_defaultConstraints;
	CVector<CheckConstraintRef>     _checkConstraints;
};

class ProcParam
{
public:
	_tstring			_paramId;
	EParameterMode		_paramMode;
	_tstring			_tableName;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_tableComment;
};

class Procedure
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_tableName;
	_tstring					_tableComment;
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
	_tstring			_tableName;
	_tstring			_datatype;
	uint64				_maxLength = 0;
	uint8				_precision = 0;
	uint8				_scale = 0;
	_tstring			_datatypedesc;
	_tstring			_tableComment;
};

class Function
{
public:
	_tstring				CreateQuery();
	_tstring				DropQuery();

public:
	int32						_objectId = 0;
	_tstring					_schemaName;
	_tstring					_tableName;
	_tstring					_tableComment;
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
