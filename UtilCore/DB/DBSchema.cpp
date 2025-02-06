
//***************************************************************************
// DBSchema.cpp: implementation of the Database Schema.
//
//***************************************************************************

#include "pch.h"
#include "DBSchema.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CDBSchema::~CDBSchema()
{
}

bool CDBSchema::GatherDBSchema()
{
	GatherDBTables();
	GatherDBTableColumns();
	GatherDBTableConstraints();
	GatherDBIndexes();
	GatherDBForeignKeys();
	GatherDBCheckConstraints();
	GatherDBTrigger();

	if( _dbClass == EDBClass::MSSQL )
	{
		GatherDBIndexOptions();
		GatherDBDefaultConstraints();
	}

	if( _dbClass == EDBClass::ORACLE ) GatherDBIdentityColumns();

	GatherDBStoredProcedures();
	GatherDBStoredProcedureParams();
	GatherDBFunctions();
	GatherDBFunctionParams();

	return true;
}

//***************************************************************************
//
void CDBSchema::PrintDBSchema()
{
	_tstring query;

	query = GetTableInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 명세]"), query, false);

	query = GetTableColumnInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 컬럼 명세]"), query, true);

	if( _dbClass == EDBClass::ORACLE )
	{
		query = ORACLEGetTableIdentityColumnInfoQuery();
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 컬럼 Identity 명세]"), query, true);
	}

	query = GetConstraintsInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[제약조건 명세]"), query, true);

	query = GetIndexInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[인덱스 명세]"), query, true);

	if( _dbClass == EDBClass::MSSQL )
	{
		query = GetIndexOptionInfoQuery(_dbClass);
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[인덱스 옵션 명세]"), query, true);

		query = GetPartitionInfoQuery(_dbClass);
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[파티션 명세]"), query, true);
	}
	else if( _dbClass == EDBClass::MYSQL )
	{
		query = GetPartitionInfoQuery(_dbClass);
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[파티션 명세]"), query, true);
	}

	query = GetForeignKeyInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[외래키 명세]"), query, true);

	if( _dbClass == EDBClass::MSSQL )
	{
		query = GetDefaultConstInfoQuery(_dbClass);
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[기본값 제약 명세]"), query, true);
	}

	query = GetCheckConstInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[체크 제약 명세]"), query, true);

	query = GetTriggerInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[트리거 명세]"), query, true);

	query = GetProcedureInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[저장프로시저 명세]"), query, true);

	query = GetProcedureParamInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[저장프로시저 파라미터 명세]"), query, true);

	query = GetFunctionInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[함수 명세]"), query, true);

	query = GetFunctionParamInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[함수 파라미터 명세]"), query, true);
}

//***************************************************************************
//
bool CDBSchema::GatherDBTables(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	uint64	auto_increment_value;
	TCHAR	tszStorageEngine[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCharacterSet[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCollation[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX] = { 0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = { 0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = { 0, };

	SP::GetDBTables getDBTables(_dbClass, _dbConn, ptszTableName);
	getDBTables.Out_DBName(OUT tszDBName);
	getDBTables.Out_ObjectId(OUT objectId);
	getDBTables.Out_SchemaName(OUT tszSchemaName);
	getDBTables.Out_TableName(OUT tszTableName);
	getDBTables.Out_AutoIncrementValue(OUT auto_increment_value);
	getDBTables.Out_StorageEngine(OUT tszStorageEngine);
	getDBTables.Out_CharacterSet(OUT tszCharacterSet);
	getDBTables.Out_Collation(OUT tszCollation);
	getDBTables.Out_TableComment(OUT tszTableComment);
	getDBTables.Out_CreateDate(OUT tszCreateDate);
	getDBTables.Out_ModifyDate(OUT tszModifyDate);

	if( getDBTables.ExecDirect() == false )
		return false;

	while( getDBTables.Fetch() )
	{
		DBModel::TableRef tableRef = MakeShared<DBModel::Table>(_dbClass);
		tableRef->_objectId = objectId;
		tableRef->_schemaName = tszSchemaName;
		tableRef->_tableName = tszTableName;
		tableRef->_tableComment = tszTableComment;
		tableRef->_auto_increment_value = tstring_tcformat(_T("%lld"), auto_increment_value);
		tableRef->_storageEngine = tszStorageEngine;
		tableRef->_characterset = tszCharacterSet;
		tableRef->_collation = tszCollation;
		tableRef->_createDate = tszCreateDate;
		tableRef->_modifyDate = tszModifyDate;
		_dbTables.push_back(tableRef);

		tszTableComment[0] = _T('\0');
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBTableColumns(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN] = { 0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	int32	seq;
	int64	maxLength;
	uint8	precision;
	uint8	scale;
	BOOL	isNullable;
	BOOL	isIdentity;
	int64	seedValue;
	int64	incValue;
	TCHAR	tszCharacterSet[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCollation[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszDefaultConstraintName[101] = { 0, };
	TCHAR	tszDefaultDefinition[101] = { 0, };
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX] = { 0, };

	SP::GetDBTableColumns getDBTableColumns(_dbClass, _dbConn, ptszTableName);
	getDBTableColumns.Out_DBName(OUT tszDBName);
	getDBTableColumns.Out_ObjectId(OUT objectId);
	getDBTableColumns.Out_SchemaName(OUT tszSchemaName);
	getDBTableColumns.Out_TableName(OUT tszTableName);
	getDBTableColumns.Out_Seq(OUT seq);
	getDBTableColumns.Out_ColumnName(OUT tszColumnName);
	getDBTableColumns.Out_DataType(OUT tszDataType);
	getDBTableColumns.Out_MaxLength(OUT maxLength);
	getDBTableColumns.Out_Precision(OUT precision);
	getDBTableColumns.Out_Scale(OUT scale);
	getDBTableColumns.Out_DataTypeDesc(OUT tszDataTypeDesc);
	getDBTableColumns.Out_IsNullable(OUT isNullable);
	getDBTableColumns.Out_IsIdentity(OUT isIdentity);
	getDBTableColumns.Out_SeedValue(OUT seedValue);
	getDBTableColumns.Out_IncrementValue(OUT incValue);
	getDBTableColumns.Out_DefaultConstraintName(OUT tszDefaultConstraintName);
	getDBTableColumns.Out_DefaultDefinition(OUT tszDefaultDefinition);
	getDBTableColumns.Out_CharacterSet(OUT tszCharacterSet);
	getDBTableColumns.Out_Collation(OUT tszCollation);
	getDBTableColumns.Out_ColumnComment(OUT tszColumnComment);

	if( getDBTableColumns.ExecDirect() == false )
		return false;

	while( getDBTableColumns.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});

		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::ColumnRef>& columns = (*findTable)->_columns;
		auto findColumn = std::find_if(columns.begin(), columns.end(), [tszColumnName](DBModel::ColumnRef& column) { return _tcsicmp(column->_columnName.c_str(), tszColumnName) == 0 ? true : false; });
		if( findColumn == columns.end() )
		{
			DBModel::ColumnRef columnRef = MakeShared<DBModel::Column>(_dbClass);
			{
				columnRef->_schemaName = (*findTable)->_schemaName;
				columnRef->_tableName = (*findTable)->_tableName;
				columnRef->_seq = tstring_tcformat(_T("%d"), seq);
				columnRef->_tableName = tszColumnName;
				columnRef->_datatype = tszDataType;
				columnRef->_maxLength = (_tcsicmp(columnRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(columnRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				columnRef->_precision = precision;
				columnRef->_scale = scale;
				columnRef->_datatypedesc = tszDataTypeDesc;
				columnRef->_nullable = isNullable;
				columnRef->_identity = isIdentity;
				if( columnRef->_identity ) columnRef->_identitydesc = tstring_tcformat(_T("%lld,%lld"), columnRef->_seedValue, columnRef->_incrementValue);
				columnRef->_seedValue = (isIdentity ? seedValue : 0);
				columnRef->_incrementValue = (isIdentity ? incValue : 0);
				columnRef->_defaultDefinition = tszDefaultDefinition;
				columnRef->_defaultConstraintName = tszDefaultConstraintName;
				columnRef->_characterset = tszCharacterSet;
				columnRef->_collation = tszCollation;
				columnRef->_columnComment = tszColumnComment;
			}

			columns.push_back(columnRef);
			findColumn = columns.end() - 1;
		}

		tszColumnComment[0] = _T('\0');
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBTableConstraints(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR   tszConstName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR   tszConstType[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR   tszConstTypeDesc[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR   tszConstValue[DATABASE_WVARCHAR_MAX] = { 0, };
	BOOL	isSystemNamed;
	BOOL	isStatus;
	TCHAR   tszSortValue[DATABASE_OBJECT_NAME_STRLEN] = { 0, };

	SP::GetDBConstraints getDBConstraints(_dbClass, _dbConn, ptszTableName);
	getDBConstraints.Out_DBName(OUT tszDBName);
	getDBConstraints.Out_ObjectId(OUT objectId);
	getDBConstraints.Out_SchemaName(OUT tszSchemaName);
	getDBConstraints.Out_TableName(OUT tszTableName);
	getDBConstraints.Out_ConstName(OUT tszConstName);
	getDBConstraints.Out_ConstType(OUT tszConstType);
	getDBConstraints.Out_ConstTypeDesc(OUT tszConstTypeDesc);
	getDBConstraints.Out_ConstValue(OUT tszConstValue);
	getDBConstraints.Out_IsSystemNamed(OUT isSystemNamed);
	getDBConstraints.Out_IsStatus(OUT isStatus);
	getDBConstraints.Out_SortValue(OUT tszSortValue);

	if( getDBConstraints.ExecDirect() == false )
		return false;

	while( getDBConstraints.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});

		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::ConstraintRef>& constraints = (*findTable)->_constraints;
		auto findConstraint = std::find_if(constraints.begin(), constraints.end(), [tszConstName](DBModel::ConstraintRef& constraint) { return _tcsicmp(constraint->_constName.c_str(), tszConstName) == 0 ? true : false; });
		if( findConstraint == constraints.end() )
		{
			DBModel::ConstraintRef constraintRef = MakeShared<DBModel::Constraint>(_dbClass);
			{
				constraintRef->_schemaName = (*findTable)->_schemaName;
				constraintRef->_tableName = (*findTable)->_tableName;
				constraintRef->_constName = tszConstName;
				constraintRef->_constType = tszConstType;
				constraintRef->_constTypeDesc = tszConstTypeDesc;
				constraintRef->_constValue = tszConstValue;
				constraintRef->_systemNamed = isSystemNamed;
				constraintRef->_status = isStatus;
				constraintRef->_sortValue = tszSortValue;
			}
			constraints.push_back(constraintRef);
			findConstraint = constraints.end() - 1;
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBIdentityColumns(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN] = { 0, };
	TCHAR   tszIdentityColumn[5] = { 0, };
	TCHAR   tszDefaultOnNull[5] = { 0, };
	TCHAR	tszGenerationType[32] = { 0, };
	TCHAR   tszSequenceName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR   tszMinValue[24] = { 0, };
	TCHAR   tszMaxValue[24] = { 0, };
	TCHAR   tszIncrementBy[24] = { 0, };
	TCHAR   tszCycleFlag[3] = { 0, };
	TCHAR   tszOrderFlag[3] = { 0, };
	TCHAR   tszCacheSize[24] = { 0, };
	TCHAR   tszLastNumber[24] = { 0, };
	TCHAR   tszScaleFlag[3] = { 0, };
	TCHAR   tszExtendFlag[3] = { 0, };
	TCHAR	tszShardedFlag[3] = { 0, };
	TCHAR   tszSessionFlag[3] = { 0, };
	TCHAR   tszKeepValue[3] = { 0, };

	SP::GetDBIdentityColumns getDBIdentityColumns(_dbClass, _dbConn, ptszTableName);
	getDBIdentityColumns.Out_DBName(OUT tszDBName);
	getDBIdentityColumns.Out_SchemaName(OUT tszSchemaName);
	getDBIdentityColumns.Out_TableName(OUT tszTableName);
	getDBIdentityColumns.Out_ColumnName(OUT tszColumnName);
	getDBIdentityColumns.Out_IdentityColumn(OUT tszIdentityColumn);
	getDBIdentityColumns.Out_DefaultOnNull(OUT tszDefaultOnNull);
	getDBIdentityColumns.Out_GenerationType(OUT tszGenerationType);
	getDBIdentityColumns.Out_SequenceName(OUT tszSequenceName);
	getDBIdentityColumns.Out_MinValue(OUT tszMinValue);
	getDBIdentityColumns.Out_MaxValue(OUT tszMaxValue);
	getDBIdentityColumns.Out_IncrementBy(OUT tszIncrementBy);
	getDBIdentityColumns.Out_CycleFlag(OUT tszCycleFlag);
	getDBIdentityColumns.Out_OrderFlag(OUT tszOrderFlag);
	getDBIdentityColumns.Out_CacheSize(OUT tszCacheSize);
	getDBIdentityColumns.Out_LastNumber(OUT tszLastNumber);
	getDBIdentityColumns.Out_ScaleFlag(OUT tszScaleFlag);
	getDBIdentityColumns.Out_ExtendFlag(OUT tszExtendFlag);
	getDBIdentityColumns.Out_ShardedFlag(OUT tszShardedFlag);
	getDBIdentityColumns.Out_SessionFlag(OUT tszSessionFlag);
	getDBIdentityColumns.Out_KeepValue(OUT tszKeepValue);

	if( getDBIdentityColumns.ExecDirect() == false )
		return false;

	while( getDBIdentityColumns.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::IdentityColumnRef>& identityColumns = (*findTable)->_identityColumns;
		auto findIdentityColumn = std::find_if(identityColumns.begin(), identityColumns.end(), [tszIdentityColumn](DBModel::IdentityColumnRef& identityColumn) { return _tcsicmp(identityColumn->_identityColumn.c_str(), tszIdentityColumn) == 0 ? true : false; });
		if( findIdentityColumn == identityColumns.end() )
		{
			DBModel::IdentityColumnRef identityColumnRef = MakeShared<DBModel::IdentityColumn>();
			{
				identityColumnRef->_schemaName = (*findTable)->_schemaName;
				identityColumnRef->_tableName = (*findTable)->_tableName;
				identityColumnRef->_columnName = tszColumnName;
				identityColumnRef->_identityColumn = tszIdentityColumn;
				identityColumnRef->_defaultOnNull = tszDefaultOnNull;
				identityColumnRef->_generationType = tszGenerationType;
				identityColumnRef->_sequenceName = tszSequenceName;
				identityColumnRef->_minValue = GetUInt64(tszMinValue);
				identityColumnRef->_maxValue = GetUInt64(tszMaxValue);
				identityColumnRef->_incrementBy = GetUInt64(tszIncrementBy);
				identityColumnRef->_cycleFlag = tszCycleFlag;
				identityColumnRef->_orderFlag = tszOrderFlag;
				identityColumnRef->_cacheSize = GetUInt64(tszCacheSize);
				identityColumnRef->_lastNumber = GetUInt64(tszLastNumber);
				identityColumnRef->_scaleFlag = tszScaleFlag;
				identityColumnRef->_extendFlag = tszExtendFlag;
				identityColumnRef->_shardedFlag = tszShardedFlag;
				identityColumnRef->_sessionFlag = tszSessionFlag;
				identityColumnRef->_keepValue = tszKeepValue;
			}
			identityColumns.push_back(identityColumnRef);
			findIdentityColumn = identityColumns.end() - 1;
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBIndexes(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	int32	indexId;
	TCHAR	tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };
	BOOL	isPrimaryKey;
	BOOL	isUnique;
	int32	columnSeq;
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN] = { 0, };
	int8	columnSort;
	BOOL	isSystemNamed;

	SP::GetDBIndexes getDBIndexes(_dbClass, _dbConn, ptszTableName);
	getDBIndexes.Out_DBName(OUT tszDBName);
	getDBIndexes.Out_ObjectId(OUT objectId);
	getDBIndexes.Out_SchemaName(OUT tszSchemaName);
	getDBIndexes.Out_TableName(OUT tszTableName);
	getDBIndexes.Out_IndexName(OUT tszIndexName);
	getDBIndexes.Out_IndexId(OUT indexId);
	getDBIndexes.Out_IndexType(OUT tszIndexType);
	getDBIndexes.Out_IsPrimaryKey(OUT isPrimaryKey);
	getDBIndexes.Out_IsUnique(OUT isUnique);
	getDBIndexes.Out_ColumnSeq(OUT columnSeq);
	getDBIndexes.Out_ColumnName(OUT tszColumnName);
	getDBIndexes.Out_ColumnSort(OUT columnSort);
	getDBIndexes.Out_IsSystemNamed(OUT isSystemNamed);

	if( getDBIndexes.ExecDirect() == false )
		return false;

	while( getDBIndexes.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::IndexRef>& indexes = (*findTable)->_indexes;
		auto findIndex = std::find_if(indexes.begin(), indexes.end(), [tszIndexName](DBModel::IndexRef& index) { return _tcsicmp(index->_indexName.c_str(), tszIndexName) == 0 ? true : false; });
		if( findIndex == indexes.end() )
		{
			DBModel::IndexRef indexRef = MakeShared<DBModel::Index>(_dbClass);
			{
				indexRef->_schemaName = (*findTable)->_schemaName;
				indexRef->_tableName = (*findTable)->_tableName;
				indexRef->_tableName = tszIndexName;
				indexRef->_indexId = indexId;
				indexRef->_type = tszIndexType;
				indexRef->_primaryKey = isPrimaryKey;
				indexRef->_uniqueKey = isUnique;
				indexRef->_systemNamed = isSystemNamed;
			}
			indexes.push_back(indexRef);
			findIndex = indexes.end() - 1;
		}

		DBModel::IndexColumnRef indexColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			indexColumnRef->_seq = tstring_tcformat(_T("%d"), columnSeq);
			indexColumnRef->_columnName = tszColumnName;
			indexColumnRef->_sort = static_cast<EIndexSort>(columnSort);
		};
		(*findIndex)->_columns.push_back(indexColumnRef);
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBIndexOptions(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszIndexType[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };
	BOOL	isPrimaryKey;
	BOOL	isUnique;
	BOOL	isDisabled;
	BOOL	isPadded;
	int8	fillFactor;
	BOOL	ignoreDupKey;
	BOOL	allowRowLocks;
	BOOL	allowPageLocks;
	BOOL	hasFilter;
	TCHAR	tszFilterDefinition[DATABASE_WVARCHAR_MAX] = { 0, };
	int32	compressionDelay;
	BOOL	optimizeForSequentialKey;
	BOOL	statisticsNoRecompute;
	BOOL    statisticsIncremental;
	int8	dataCompression;
	TCHAR	tszDataCompressionDesc[DATABASE_BASE_STRLEN] = { 0, };
	BOOL	xmlCompression;
	TCHAR	tszXmlCompressionDesc[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszFileGroupOrPartitionScheme[DATABASE_OBJECT_TYPE_DESC_STRLEN] = { 0, };
	TCHAR	tszFileGroupOrPartitionSchemeName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };

	SP::GetDBIndexOptions getDBIndexOptions(_dbClass, _dbConn, ptszTableName);
	getDBIndexOptions.Out_DBName(OUT tszDBName);
	getDBIndexOptions.Out_ObjectId(OUT objectId);
	getDBIndexOptions.Out_SchemaName(OUT tszSchemaName);
	getDBIndexOptions.Out_TableName(OUT tszTableName);
	getDBIndexOptions.Out_IndexName(OUT tszIndexName);
	getDBIndexOptions.Out_IndexType(OUT tszIndexType);
	getDBIndexOptions.Out_IsPrimaryKey(OUT isPrimaryKey);
	getDBIndexOptions.Out_IsUnique(OUT isUnique);
	getDBIndexOptions.Out_IsDisabled(OUT isDisabled);
	getDBIndexOptions.Out_IsPadded(OUT isPadded);
	getDBIndexOptions.Out_FillFactor(OUT fillFactor);
	getDBIndexOptions.Out_IgnoreDupKey(OUT ignoreDupKey);
	getDBIndexOptions.Out_AllowRowLocks(OUT allowRowLocks);
	getDBIndexOptions.Out_AllowPageLocks(OUT allowPageLocks);
	getDBIndexOptions.Out_HasFilter(OUT hasFilter);
	getDBIndexOptions.Out_FilterDefinition(OUT tszFilterDefinition);
	getDBIndexOptions.Out_CompressionDelay(OUT compressionDelay);
	getDBIndexOptions.Out_OptimizeForSequentialKey(OUT optimizeForSequentialKey);
	getDBIndexOptions.Out_StatisticsNoRecompute(OUT statisticsNoRecompute);
	getDBIndexOptions.Out_StatisticsIncremental(OUT statisticsIncremental);
	getDBIndexOptions.Out_DataCompression(OUT dataCompression);
	getDBIndexOptions.Out_DataCompressionDesc(OUT tszDataCompressionDesc);
	getDBIndexOptions.Out_XmlCompression(OUT xmlCompression);
	getDBIndexOptions.Out_XmlCompressionDesc(OUT tszXmlCompressionDesc);
	getDBIndexOptions.Out_FileGroupOrPartitionScheme(OUT tszFileGroupOrPartitionScheme);
	getDBIndexOptions.Out_FileGroupOrPartitionSchemeName(OUT tszFileGroupOrPartitionSchemeName);

	if( getDBIndexOptions.ExecDirect() == false )
		return false;

	while( getDBIndexOptions.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::IndexOptionRef>& indexOptions = (*findTable)->_indexOptions;
		auto findIndexOption = std::find_if(indexOptions.begin(), indexOptions.end(), [tszIndexName](DBModel::IndexOptionRef& indexOption) { return _tcsicmp(indexOption->_indexName.c_str(), tszIndexName) == 0 ? true : false; });
		if( findIndexOption == indexOptions.end() )
		{
			DBModel::IndexOptionRef indexOptionRef = MakeShared<DBModel::IndexOption>();
			{
				indexOptionRef->_schemaName = (*findTable)->_schemaName;
				indexOptionRef->_tableName = (*findTable)->_tableName;
				indexOptionRef->_tableName = tszIndexName;
				indexOptionRef->_primaryKey = isPrimaryKey;
				indexOptionRef->_uniqueKey = isUnique;
				indexOptionRef->_isDisabled = isDisabled;
				indexOptionRef->_isPadded = isPadded;
				indexOptionRef->_fillFactor = fillFactor;
				indexOptionRef->_ignoreDupKey = ignoreDupKey;
				indexOptionRef->_allowRowLocks = allowRowLocks;
				indexOptionRef->_allowPageLocks = allowPageLocks;
				indexOptionRef->_hasFilter = hasFilter;
				indexOptionRef->_filterDefinition = tszFilterDefinition;
				indexOptionRef->_compressionDelay = compressionDelay;
				indexOptionRef->_optimizeForSequentialKey = optimizeForSequentialKey;
				indexOptionRef->_statisticsNoRecompute = statisticsNoRecompute;
				indexOptionRef->_statisticsIncremental = statisticsIncremental;
				indexOptionRef->_dataCompression = dataCompression;
				indexOptionRef->_dataCompressionDesc = tszDataCompressionDesc;
				indexOptionRef->_fileGroupOrPartitionScheme = tszFileGroupOrPartitionScheme;
				indexOptionRef->_fileGroupOrPartitionSchemeName = tszFileGroupOrPartitionSchemeName;
			}
			indexOptions.push_back(indexOptionRef);
			findIndexOption = indexOptions.end() - 1;
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBForeignKeys(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszForeignKeyName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	BOOL	isDisabled;
	BOOL	isNotTrusted;
	TCHAR	tszForeignKeyTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszForeignKeyColumnName[DATABASE_COLUMN_NAME_STRLEN] = { 0, };
	TCHAR	tszReferenceKeySchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszReferenceKeyTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszReferenceKeyColumnName[DATABASE_COLUMN_NAME_STRLEN] = { 0, };
	TCHAR	tszUpdateRule[101] = { 0, };
	TCHAR	tszDeleteRule[101] = { 0, };
	BOOL	isSystemNamed;

	SP::GetDBForeignKeys getDBForeignKeys(_dbClass, _dbConn, ptszTableName);
	getDBForeignKeys.Out_DBName(OUT tszDBName);
	getDBForeignKeys.Out_ObjectId(OUT objectId);
	getDBForeignKeys.Out_SchemaName(OUT tszSchemaName);
	getDBForeignKeys.Out_TableName(OUT tszTableName);
	getDBForeignKeys.Out_ForeignKeyName(OUT tszForeignKeyName);
	getDBForeignKeys.Out_IsDisabled(OUT isDisabled);
	getDBForeignKeys.Out_IsNotTrusted(OUT isNotTrusted);
	getDBForeignKeys.Out_ForeignKeyTableName(OUT tszForeignKeyTableName);
	getDBForeignKeys.Out_ForeignKeyColumnName(OUT tszForeignKeyColumnName);
	getDBForeignKeys.Out_ReferenceKeySchemaName(OUT tszReferenceKeySchemaName);
	getDBForeignKeys.Out_ReferenceKeyTableName(OUT tszReferenceKeyTableName);
	getDBForeignKeys.Out_ReferenceKeyColumnName(OUT tszReferenceKeyColumnName);
	getDBForeignKeys.Out_UpdateRule(OUT tszUpdateRule);
	getDBForeignKeys.Out_DeleteRule(OUT tszDeleteRule);
	getDBForeignKeys.Out_IsSystemNamed(OUT isSystemNamed);

	if( getDBForeignKeys.ExecDirect() == false )
		return false;

	while( getDBForeignKeys.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());

		CVector<DBModel::ForeignKeyRef>& referenceKeys = (*findTable)->_foreignKeys;
		auto findReferenceKey = std::find_if(referenceKeys.begin(), referenceKeys.end(), [tszForeignKeyName](DBModel::ForeignKeyRef& referenceKey) { return _tcsicmp(referenceKey->_foreignKeyName.c_str(), tszForeignKeyName) == 0 ? true : false; });
		if( findReferenceKey == referenceKeys.end() )
		{
			DBModel::ForeignKeyRef foreignKeyRef = MakeShared<DBModel::ForeignKey>(_dbClass);
			{
				foreignKeyRef->_schemaName = (*findTable)->_schemaName;
				foreignKeyRef->_tableName = (*findTable)->_tableName;
				foreignKeyRef->_foreignKeyName = tszForeignKeyName;
				foreignKeyRef->_isDisabled = isDisabled;
				foreignKeyRef->_isNotTrusted = isNotTrusted;
				foreignKeyRef->_foreignKeyTableName = tszForeignKeyTableName;
				foreignKeyRef->_referenceKeySchemaName = tszReferenceKeySchemaName;
				foreignKeyRef->_referenceKeyTableName = tszReferenceKeyTableName;
				foreignKeyRef->_updateRule = tszUpdateRule;
				foreignKeyRef->_deleteRule = tszDeleteRule;
				foreignKeyRef->_systemNamed = isSystemNamed;
			}
			referenceKeys.push_back(foreignKeyRef);
			findReferenceKey = referenceKeys.end() - 1;
		}

		DBModel::IndexColumnRef foreignKeyColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			foreignKeyColumnRef->_columnName = tszForeignKeyColumnName;
		};
		(*findReferenceKey)->_foreignKeyColumns.push_back(foreignKeyColumnRef);

		DBModel::IndexColumnRef referenceKeyColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			referenceKeyColumnRef->_columnName = tszReferenceKeyColumnName;
		};
		(*findReferenceKey)->_referenceKeyColumns.push_back(referenceKeyColumnRef);
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBDefaultConstraints(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];
	TCHAR   tszDefaultConstName[DATABASE_OBJECT_NAME_STRLEN];
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN];
	TCHAR   tszDefaultValue[DATABASE_WVARCHAR_MAX];
	BOOL	isSystemNamed;

	SP::GetDBDefaultConstraints getDBDefaultConstraints(_dbClass, _dbConn, ptszTableName);
	getDBDefaultConstraints.Out_DBName(OUT tszDBName);
	getDBDefaultConstraints.Out_ObjectId(OUT objectId);
	getDBDefaultConstraints.Out_SchemaName(OUT tszSchemaName);
	getDBDefaultConstraints.Out_TableName(OUT tszTableName);
	getDBDefaultConstraints.Out_DefaultConstName(OUT tszDefaultConstName);
	getDBDefaultConstraints.Out_ColumnName(OUT tszColumnName);
	getDBDefaultConstraints.Out_DefaultValue(OUT tszDefaultValue);
	getDBDefaultConstraints.Out_IsSystemNamed(OUT isSystemNamed);

	if( getDBDefaultConstraints.ExecDirect() == false )
		return false;

	while( getDBDefaultConstraints.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());

		CVector<DBModel::DefaultConstraintRef>& defaultConstraints = (*findTable)->_defaultConstraints;
		auto findDefaultConstraint = std::find_if(defaultConstraints.begin(), defaultConstraints.end(), [tszDefaultConstName](DBModel::DefaultConstraintRef& defaultConstraint) { return _tcsicmp(defaultConstraint->_defaultConstName.c_str(), tszDefaultConstName) == 0 ? true : false; });
		if( findDefaultConstraint == defaultConstraints.end() )
		{
			DBModel::DefaultConstraintRef defaultConstraintRef = MakeShared<DBModel::DefaultConstraint>(_dbClass);
			{
				defaultConstraintRef->_schemaName = (*findTable)->_schemaName;
				defaultConstraintRef->_tableName = (*findTable)->_tableName;
				defaultConstraintRef->_defaultConstName = tszDefaultConstName;
				defaultConstraintRef->_columnName = tszColumnName;
				defaultConstraintRef->_defaultValue = tszDefaultValue;
				defaultConstraintRef->_systemNamed = isSystemNamed;
			}

			defaultConstraints.push_back(defaultConstraintRef);
			findDefaultConstraint = defaultConstraints.end() - 1;
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBCheckConstraints(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR   tszSchemaName[DATABASE_OBJECT_NAME_STRLEN];
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN];
	TCHAR   tszCheckConstName[DATABASE_OBJECT_NAME_STRLEN];
	TCHAR   tszCheckValue[DATABASE_WVARCHAR_MAX];
	BOOL	isSystemNamed;

	SP::GetDBCheckConstraints getDBCheckConstraints(_dbClass, _dbConn, ptszTableName);
	getDBCheckConstraints.Out_DBName(OUT tszDBName);
	getDBCheckConstraints.Out_ObjectId(OUT objectId);
	getDBCheckConstraints.Out_SchemaName(OUT tszSchemaName);
	getDBCheckConstraints.Out_TableName(OUT tszTableName);
	getDBCheckConstraints.Out_CheckConstName(OUT tszCheckConstName);
	getDBCheckConstraints.Out_CheckValue(OUT tszCheckValue);
	getDBCheckConstraints.Out_IsSystemNamed(OUT isSystemNamed);

	if( getDBCheckConstraints.ExecDirect() == false )
		return false;

	while( getDBCheckConstraints.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else return _tcsicmp(table->_tableName.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());

		CVector<DBModel::CheckConstraintRef>& checkConstraints = (*findTable)->_checkConstraints;
		auto findCheckConstraint = std::find_if(checkConstraints.begin(), checkConstraints.end(), [tszCheckConstName](DBModel::CheckConstraintRef& checkConstraint) { return _tcsicmp(checkConstraint->_checkConstName.c_str(), tszCheckConstName) == 0 ? true : false; });
		if( findCheckConstraint == checkConstraints.end() )
		{
			DBModel::CheckConstraintRef checkConstraintRef = MakeShared<DBModel::CheckConstraint>(_dbClass);
			{
				checkConstraintRef->_schemaName = (*findTable)->_schemaName;
				checkConstraintRef->_tableName = (*findTable)->_tableName;
				checkConstraintRef->_checkConstName = tszCheckConstName;
				checkConstraintRef->_checkValue = tszCheckValue;
				checkConstraintRef->_systemNamed = isSystemNamed;
			}
			checkConstraints.push_back(checkConstraintRef);
			findCheckConstraint = checkConstraints.end() - 1;
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBTrigger(const TCHAR* ptszTableName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszTriggerName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };

	SP::GetDBTriggers getDBTriggers(_dbClass, _dbConn, ptszTableName);
	getDBTriggers.Out_DBName(OUT tszDBName);
	getDBTriggers.Out_ObjectId(OUT objectId);
	getDBTriggers.Out_SchemaName(OUT tszSchemaName);
	getDBTriggers.Out_TableName(OUT tszTableName);
	getDBTriggers.Out_TriggerName(OUT tszTriggerName);

	if( getDBTriggers.ExecDirect() == false )
		return false;

	while( getDBTriggers.Fetch() )
	{
		DBModel::TriggerRef triggerRef = MakeShared<DBModel::Trigger>();
		triggerRef->_objectId = objectId;
		triggerRef->_schemaName = tszSchemaName;
		triggerRef->_tableName = tszTableName;
		triggerRef->_triggerName = tszTriggerName;
		_dbTriggers.push_back(triggerRef);
	}

	CDBQueryProcess dbQuery(_dbConn);

	for( auto iter = _dbTriggers.begin(); iter != _dbTriggers.end(); iter++ )
	{
		DBModel::TriggerRef iterTriggerRef = *iter;
		if( _dbClass == EDBClass::MSSQL )
		{
			_tstring objectName = iterTriggerRef->_schemaName + _T(".") + iterTriggerRef->_triggerName;
			iterTriggerRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::TRIGGERS, objectName.c_str());
		}
		else if( _dbClass == EDBClass::MYSQL )
			iterTriggerRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::TRIGGERS, iterTriggerRef->_triggerName.c_str());
		else if( _dbClass == EDBClass::ORACLE )
			iterTriggerRef->_fullBody = dbQuery.ORACLEGetUserSource(EDBObjectType::TRIGGERS, iterTriggerRef->_triggerName.c_str());
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBStoredProcedures(const TCHAR* ptszProcName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszProcName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszProcComment[DATABASE_WVARCHAR_MAX] = { 0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = { 0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = { 0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBStoredProcedures getDBStoredProcedures(_dbClass, _dbConn, ptszProcName);
	getDBStoredProcedures.Out_DBName(OUT tszDBName);
	getDBStoredProcedures.Out_ObjectId(OUT objectId);
	getDBStoredProcedures.Out_SchemaName(OUT tszSchemaName);
	getDBStoredProcedures.Out_SPName(OUT tszProcName);
	getDBStoredProcedures.Out_SPComment(OUT tszProcComment);
	getDBStoredProcedures.Out_CreateDate(OUT tszCreateDate);
	getDBStoredProcedures.Out_ModifyDate(OUT tszModifyDate);

	if( getDBStoredProcedures.ExecDirect() == false )
		return false;

	while( getDBStoredProcedures.Fetch() )
	{
		DBModel::ProcedureRef procRef = MakeShared<DBModel::Procedure>();
		procRef->_objectId = objectId;
		procRef->_schemaName = tszSchemaName;
		procRef->_procName = tszProcName;
		procRef->_procComment = tszProcComment;
		procRef->_fullBody = _tstring(body.begin(), std::find(body.begin(), body.end(), 0));
		procRef->_createDate = tszCreateDate;
		procRef->_modifyDate = tszModifyDate;
		_dbProcedures.push_back(procRef);

		tszProcComment[0] = _T('\0');
	}

	CDBQueryProcess dbQuery(_dbConn);

	if( _dbClass == EDBClass::MSSQL )
	{
		for( auto iter = _dbProcedures.begin(); iter != _dbProcedures.end(); iter++ )
		{
			DBModel::ProcedureRef iterProcRef = *iter;
			_tstring objectName = iterProcRef->_schemaName + _T(".") + iterProcRef->_procName;
			iterProcRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::PROCEDURE, objectName.c_str());
		}
	}
	else if( _dbClass == EDBClass::MYSQL )
	{
		for( auto iter = _dbProcedures.begin(); iter != _dbProcedures.end(); iter++ )
		{
			DBModel::ProcedureRef iterProcRef = *iter;
			iterProcRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::PROCEDURE, iterProcRef->_procName.c_str());
		}
	}
	else if( _dbClass == EDBClass::ORACLE )
	{
		for( auto iter = _dbProcedures.begin(); iter != _dbProcedures.end(); iter++ )
		{
			DBModel::ProcedureRef iterProcRef = *iter;
			iterProcRef->_fullBody = dbQuery.ORACLEGetUserSource(EDBObjectType::PROCEDURE, iterProcRef->_procName.c_str());
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBStoredProcedureParams(const TCHAR* ptszProcName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszProcName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	int32	paramId;
	int8	paramMode;
	TCHAR	tszParamName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	int64	maxLength;
	uint8	precision;
	uint8	scale;
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX] = { 0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBStoredProcedureParams getDBStoredProcedureParams(_dbClass, _dbConn, ptszProcName);
	getDBStoredProcedureParams.Out_DBName(OUT tszDBName);
	getDBStoredProcedureParams.Out_ObjectId(OUT objectId);
	getDBStoredProcedureParams.Out_SchemaName(OUT tszSchemaName);
	getDBStoredProcedureParams.Out_SPName(OUT tszProcName);
	getDBStoredProcedureParams.Out_ParamId(OUT paramId);
	getDBStoredProcedureParams.Out_ParamMode(OUT paramMode);
	getDBStoredProcedureParams.Out_ParamName(OUT tszParamName);
	getDBStoredProcedureParams.Out_DataType(OUT tszDataType);
	getDBStoredProcedureParams.Out_MaxLength(OUT maxLength);
	getDBStoredProcedureParams.Out_Precision(OUT precision);
	getDBStoredProcedureParams.Out_Scale(OUT scale);
	getDBStoredProcedureParams.Out_DataTypeDesc(OUT tszDataTypeDesc);
	getDBStoredProcedureParams.Out_ParamComment(OUT tszParamComment);

	if( getDBStoredProcedureParams.ExecDirect() == false )
		return false;

	while( getDBStoredProcedureParams.Fetch() )
	{
		auto findProc = std::find_if(_dbProcedures.begin(), _dbProcedures.end(), [=](const DBModel::ProcedureRef& proc)
		{
			if( _dbClass == EDBClass::MSSQL ) return proc->_objectId == objectId;
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(proc->_procName.c_str(), tszProcName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findProc != _dbProcedures.end());
		CVector<DBModel::ProcParamRef>& procParams = (*findProc)->_parameters;
		auto findProcParam = std::find_if(procParams.begin(), procParams.end(), [tszParamName](DBModel::ProcParamRef& procParam) { return _tcsicmp(procParam->_paramName.c_str(), tszParamName) == 0 ? true : false; });
		if( findProcParam == procParams.end() )
		{
			DBModel::ProcParamRef procParamRef = MakeShared<DBModel::ProcParam>();
			{
				procParamRef->_paramId = tstring_tcformat(_T("%d"), paramId);;
				procParamRef->_paramMode = static_cast<EParameterMode>(paramMode);
				procParamRef->_paramName = tszParamName;
				procParamRef->_datatype = tszDataType;
				procParamRef->_maxLength = (_tcsicmp(procParamRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(procParamRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				procParamRef->_precision = precision;
				procParamRef->_scale = scale;
				procParamRef->_datatypedesc = tszDataTypeDesc;
				procParamRef->_paramComment = tszParamComment;
			}

			procParams.push_back(procParamRef);
			findProcParam = procParams.end() - 1;
		}

		tszParamComment[0] = _T('\0');
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBFunctions(const TCHAR* ptszFuncName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszFuncName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszFuncComment[DATABASE_WVARCHAR_MAX] = { 0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = { 0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = { 0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBFunctions getDBFunctions(_dbClass, _dbConn, ptszFuncName);
	getDBFunctions.Out_DBName(OUT tszDBName);
	getDBFunctions.Out_ObjectId(OUT objectId);
	getDBFunctions.Out_SchemaName(OUT tszSchemaName);
	getDBFunctions.Out_FCName(OUT tszFuncName);
	getDBFunctions.Out_FCComment(OUT tszFuncComment);
	getDBFunctions.Out_CreateDate(OUT tszCreateDate);
	getDBFunctions.Out_ModifyDate(OUT tszModifyDate);

	if( getDBFunctions.ExecDirect() == false )
		return false;

	while( getDBFunctions.Fetch() )
	{
		DBModel::FunctionRef funcRef = MakeShared<DBModel::Function>();
		funcRef->_objectId = objectId;
		funcRef->_schemaName = tszSchemaName;
		funcRef->_funcName = tszFuncName;
		funcRef->_funcComment = tszFuncComment;
		funcRef->_fullBody = _tstring(body.begin(), std::find(body.begin(), body.end(), 0));
		funcRef->_createDate = tszCreateDate;
		funcRef->_modifyDate = tszModifyDate;
		_dbFunctions.push_back(funcRef);

		tszFuncComment[0] = _T('\0');
	}

	CDBQueryProcess dbQuery(_dbConn);

	if( _dbClass == EDBClass::MSSQL )
	{
		for( auto iter = _dbFunctions.begin(); iter != _dbFunctions.end(); iter++ )
		{
			DBModel::FunctionRef iterFuncRef = *iter;
			_tstring objectName = iterFuncRef->_schemaName + _T(".") + iterFuncRef->_funcName;
			iterFuncRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::FUNCTION, objectName.c_str());
		}
	}
	else if( _dbClass == EDBClass::MYSQL )
	{
		for( auto iter = _dbFunctions.begin(); iter != _dbFunctions.end(); iter++ )
		{
			DBModel::FunctionRef iterFuncRef = *iter;
			iterFuncRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::FUNCTION, iterFuncRef->_funcName.c_str());
		}
	}
	else if( _dbClass == EDBClass::ORACLE )
	{
		for( auto iter = _dbFunctions.begin(); iter != _dbFunctions.end(); iter++ )
		{
			DBModel::FunctionRef iterFuncRef = *iter;
			iterFuncRef->_fullBody = dbQuery.ORACLEGetUserSource(EDBObjectType::FUNCTION, iterFuncRef->_funcName.c_str());
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSchema::GatherDBFunctionParams(const TCHAR* ptszFuncName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszFuncName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	int32	paramId;
	int8	paramMode;
	TCHAR	tszParamName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	int64	maxLength;
	uint8	precision;
	uint8	scale;
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX] = { 0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBFunctionParams getDBFunctionParams(_dbClass, _dbConn, ptszFuncName);
	getDBFunctionParams.Out_DBName(OUT tszDBName);
	getDBFunctionParams.Out_ObjectId(OUT objectId);
	getDBFunctionParams.Out_SchemaName(OUT tszSchemaName);
	getDBFunctionParams.Out_FCName(OUT tszFuncName);
	getDBFunctionParams.Out_ParamId(OUT paramId);
	getDBFunctionParams.Out_ParamMode(OUT paramMode);
	getDBFunctionParams.Out_ParamName(OUT tszParamName);
	getDBFunctionParams.Out_DataType(OUT tszDataType);
	getDBFunctionParams.Out_MaxLength(OUT maxLength);
	getDBFunctionParams.Out_Precision(OUT precision);
	getDBFunctionParams.Out_Scale(OUT scale);
	getDBFunctionParams.Out_DataTypeDesc(OUT tszDataTypeDesc);
	getDBFunctionParams.Out_ParamComment(OUT tszParamComment);

	if( getDBFunctionParams.ExecDirect() == false )
		return false;

	while( getDBFunctionParams.Fetch() )
	{
		auto findFunc = std::find_if(_dbFunctions.begin(), _dbFunctions.end(), [=](const DBModel::FunctionRef& func)
		{
			if( _dbClass == EDBClass::MSSQL ) return func->_objectId == objectId;
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(func->_funcName.c_str(), tszFuncName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findFunc != _dbFunctions.end());
		CVector<DBModel::FuncParamRef>& funcParams = (*findFunc)->_parameters;
		auto findFuncParam = std::find_if(funcParams.begin(), funcParams.end(), [tszParamName](DBModel::FuncParamRef& funcParam) { return _tcsicmp(funcParam->_paramName.c_str(), tszParamName) == 0 ? true : false; });
		if( findFuncParam == funcParams.end() )
		{
			DBModel::FuncParamRef funcParamRef = MakeShared<DBModel::FuncParam>();
			{
				funcParamRef->_paramId = tstring_tcformat(_T("%d"), paramId);;
				funcParamRef->_paramMode = static_cast<EParameterMode>(paramMode);
				funcParamRef->_paramName = tszParamName;
				funcParamRef->_datatype = tszDataType;
				funcParamRef->_maxLength = (_tcsicmp(funcParamRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(funcParamRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				funcParamRef->_precision = precision;
				funcParamRef->_scale = scale;
				funcParamRef->_datatypedesc = tszDataTypeDesc;
				funcParamRef->_paramComment = tszParamComment;
			}

			funcParams.push_back(funcParamRef);
			findFuncParam = funcParams.end() - 1;
		}

		tszParamComment[0] = _T('\0');
	}

	return true;
}