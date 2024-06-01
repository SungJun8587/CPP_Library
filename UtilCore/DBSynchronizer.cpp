
//***************************************************************************
// DBSynchronizer.cpp: implementation of the Database Synchronizer.
//
//***************************************************************************

#include "pch.h"
#include "DBSynchronizer.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CDBSynchronizer::~CDBSynchronizer()
{
}

bool CDBSynchronizer::Synchronize(const TCHAR* path)
{
	//ParseXmlToDB(path);

	GatherDBTables();
	GatherDBTableColumns();
	GatherDBIndexes();
	GatherDBIndexOptions();
	GatherDBForeignKeys();
	GatherDBTrigger();
	GatherDBStoredProcedures();
	GatherDBStoredProcedureParams();
	GatherDBFunctions();
	GatherDBFunctionParams();

	//CompareDBModel();
	/*
	ExecuteUpdateQueries();
	*/

	return true;
}

//***************************************************************************
//
void CDBSynchronizer::PrintDBSchema()
{
	_tstring query;

	query = GetTableInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 명세]"), query, false);

	query = GetTableColumnInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 컬럼 명세]"), query, true);

	query = GetIndexInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[인덱스 명세]"), query, true);

	query = GetIndexOptionInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[인덱스 옵션 명세]"), query, true);

	query = GetPartitionInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[파티션 명세]"), query, true);

	query = GetForeignKeyInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[외래키 명세]"), query, true);

	query = GetDefaultConstInfoQuery(_dbClass);
	DBModel::Helpers::LogFileWrite(_dbClass, _T("[기본값 제약 명세]"), query, true);

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
void CDBSynchronizer::ParseXmlToDB(const TCHAR* path)
{
	CXMLNode root;
	CXMLParser parser;

	ASSERT_CRASH(parser.ParseFromFile(path, OUT root));

	CVector<CXMLNode> tables = root.FindChildren(_T("Table"));
	for( CXMLNode& table : tables )
	{
		DBModel::TableRef t = MakeShared<DBModel::Table>();
		t->_schemaName = table.GetStringAttr(_T("schemaname"));
		t->_name = table.GetStringAttr(_T("name"));
		t->_desc = table.GetStringAttr(_T("desc"));
		t->_auto_increment_value = table.GetStringAttr(_T("auto_increment_value"));

		CVector<CXMLNode> columns = table.FindChildren(_T("Column"));
		for( CXMLNode& column : columns )
		{
			DBModel::ColumnRef c = MakeShared<DBModel::Column>();
			c->_tableName = t->_name;
			c->_seq = column.GetStringAttr(_T("seq"));
			c->_name = column.GetStringAttr(_T("name"));
			c->_desc = column.GetStringAttr(_T("desc"));
			c->_datatypedesc = column.GetStringAttr(_T("type"));

			EDataType columnType = StringToDBDataType(c->_datatypedesc.c_str(), OUT c->_maxLength);
			ASSERT_CRASH(columnType != EDataType::NONE);

			c->_datatype = _tstring(ToString(columnType));
			c->_nullable = !column.GetBoolAttr(_T("notnull"), false);

			const TCHAR* identityStr = column.GetStringAttr(_T("identity"));
			if( ::_tcslen(identityStr) > 0 )
			{
				_tregex pt(_T("(\\d+),(\\d+)"));
				_tcmatch match;
				ASSERT_CRASH(std::regex_match(identityStr, OUT match, pt));
				c->_identity = true;
				c->_seedValue = _ttoi(match[1].str().c_str());
				c->_incrementValue = _ttoi(match[2].str().c_str());
			}

			c->_defaultDefinition = column.GetStringAttr(_T("default"));
			t->_columns.push_back(c);
		}

		CVector<CXMLNode> indexes = table.FindChildren(_T("Index"));
		for( CXMLNode& index : indexes )
		{
			DBModel::IndexRef i = MakeShared<DBModel::Index>();
			i->_tableName = t->_name;
			i->_name = index.GetStringAttr(_T("name"));

			const TCHAR* kindStr = index.GetStringAttr(_T("kind"));
			i->_kind = StringToDBIndexKind(kindStr);

			const TCHAR* typeStr = index.GetStringAttr(_T("type"));
			i->_type = StringToDBIndexType(typeStr);

			i->_primaryKey = index.FindChild(_T("PrimaryKey")).IsValid();
			i->_uniqueKey = index.FindChild(_T("UniqueKey")).IsValid();
			i->_systemNamed = index.FindChild(_T("SystemNamed")).IsValid();

			CVector<CXMLNode> columns = index.FindChildren(_T("Column"));
			for( CXMLNode& column : columns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_seq = column.GetInt32Attr(_T("seq"), 0);
				c->_name = column.GetStringAttr(_T("name"));
				const TCHAR* indexOrderStr = column.GetStringAttr(_T("order"));
				if( ::_tcsicmp(indexOrderStr, _T("DESC")) == 0 )
					c->_sort = static_cast<EIndexSort>(2);
				else c->_sort = static_cast<EIndexSort>(1);
				i->_columns.push_back(c);
			}

			t->_indexes.push_back(i);
		}

		CVector<CXMLNode> foreignKeys = table.FindChildren(_T("ForeignKey"));
		for( CXMLNode& foreignKey : foreignKeys )
		{
			DBModel::ForeignKeyRef fk = MakeShared<DBModel::ForeignKey>();
			fk->_tableName = t->_name;
			fk->_foreignKeyName = foreignKey.GetStringAttr(_T("name"));
			fk->_updateRule = foreignKey.GetStringAttr(_T("update_rule"));
			fk->_deleteRule = foreignKey.GetStringAttr(_T("delete_rule"));

			CXMLNode foreignKeyTable = foreignKey.FindChild(_T("ForeignKeyTable"));
			fk->_foreignKeyTableName = foreignKeyTable.GetStringAttr(_T("name"));
			CVector<CXMLNode> foreignKeyColumns = foreignKeyTable.FindChildren(_T("Column"));
			for( CXMLNode& foreignKeyColumn : foreignKeyColumns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_name = foreignKeyColumn.GetStringAttr(_T("name"));
				fk->_foreignKeyColumns.push_back(c);
			}

			CXMLNode referenceKeyTable = foreignKey.FindChild(_T("ReferenceKeyTable"));
			fk->_referenceKeyTableName = referenceKeyTable.GetStringAttr(_T("name"));
			CVector<CXMLNode> referenceKeyColumns = referenceKeyTable.FindChildren(_T("Column"));
			for( CXMLNode& referenceKeyColumn : referenceKeyColumns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_name = referenceKeyColumn.GetStringAttr(_T("name"));
				fk->_referenceKeyColumns.push_back(c);
			}
			t->_foreignKeys.push_back(fk);
		}

		_xmlTables.push_back(t);
	}

	CVector<CXMLNode> procedures = root.FindChildren(_T("Procedure"));
	for( CXMLNode& procedure : procedures )
	{
		DBModel::ProcedureRef p = MakeShared<DBModel::Procedure>();
		p->_schemaName = procedure.GetStringAttr(_T("schemaname"));
		p->_name = procedure.GetStringAttr(_T("name"));
		p->_desc = procedure.GetStringAttr(_T("desc"));
		p->_body = procedure.FindChild(_T("body")).GetStringValue();

		CVector<CXMLNode> params = procedure.FindChildren(_T("Param"));
		for( CXMLNode& param : params )
		{
			DBModel::ProcParamRef procParam = MakeShared<DBModel::ProcParam>();
			procParam->_paramId = param.GetStringAttr(_T("seq"));
			procParam->_paramMode = StringToDBParamMode(param.GetStringAttr(_T("mode")));
			procParam->_name = param.GetStringAttr(_T("name"));
			procParam->_desc = param.GetStringAttr(_T("desc"));
			procParam->_datatypedesc = param.GetStringAttr(_T("type"));
			p->_parameters.push_back(procParam);
		}
		_xmlProcedures.push_back(p);
	}

	CVector<CXMLNode> functions = root.FindChildren(_T("Function"));
	for( CXMLNode& function : functions )
	{
		DBModel::FunctionRef f = MakeShared<DBModel::Function>();
		f->_schemaName = function.GetStringAttr(_T("schemaname"));
		f->_name = function.GetStringAttr(_T("name"));
		f->_desc = function.GetStringAttr(_T("desc"));
		f->_body = function.FindChild(_T("body")).GetStringValue();

		CVector<CXMLNode> params = function.FindChildren(_T("Param"));
		for( CXMLNode& param : params )
		{
			DBModel::FuncParamRef funcParam = MakeShared<DBModel::FuncParam>();
			funcParam->_paramId = param.GetStringAttr(_T("seq"));
			funcParam->_paramMode = StringToDBParamMode(param.GetStringAttr(_T("mode")));
			funcParam->_name = param.GetStringAttr(_T("name"));
			funcParam->_desc = param.GetStringAttr(_T("desc"));
			funcParam->_datatypedesc = param.GetStringAttr(_T("type"));
			f->_parameters.push_back(funcParam);
		}
		_xmlFunctions.push_back(f);
	}

	CVector<CXMLNode> removedTables = root.FindChildren(_T("RemovedTable"));
	for( CXMLNode& removedTable : removedTables )
	{
		_xmlRemovedTables.insert(removedTable.GetStringAttr(_T("name")));
	}
}

//***************************************************************************
//
bool CDBSynchronizer::DBToCreateXml(const TCHAR* path)
{
	_tXmlDocumentType doc;

	// append xml declaration
	_tXmlNodeType* header = doc.allocate_node(rapidxml::node_type::node_declaration);
	header->append_attribute(doc.allocate_attribute(_T("version"), _T("1.0")));
	header->append_attribute(doc.allocate_attribute(_T("encoding"), _T("utf-8")));
	doc.append_node(header);

	// append root node
	_tXmlNodeType* root = doc.allocate_node(rapidxml::node_type::node_element, _T("GameDB"));
	doc.append_node(root);

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		_tXmlNodeType* table = doc.allocate_node(rapidxml::node_type::node_element, _T("Table"));
		table->append_attribute(doc.allocate_attribute(_T("schemaname"), dbTable->_schemaName.c_str()));
		table->append_attribute(doc.allocate_attribute(_T("name"), dbTable->_name.c_str()));
		table->append_attribute(doc.allocate_attribute(_T("desc"), dbTable->_desc.c_str()));
		table->append_attribute(doc.allocate_attribute(_T("auto_increment_value"), dbTable->_auto_increment_value.c_str()));
		root->append_node(table);

		for( DBModel::ColumnRef& dbColumn : dbTable->_columns )
		{
			_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));
			column->append_attribute(doc.allocate_attribute(_T("seq"), dbColumn->_seq.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("name"), dbColumn->_name.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("type"), dbColumn->_datatypedesc.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("notnull"), !dbColumn->_nullable ? _T("true") : _T("false")));
			if( dbColumn->_defaultDefinition.size() > 0 ) column->append_attribute(doc.allocate_attribute(_T("default"), dbColumn->_defaultDefinition.c_str()));
			if( dbColumn->_identity ) column->append_attribute(doc.allocate_attribute(_T("identity"), dbColumn->_identitydesc.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("desc"), dbColumn->_desc.c_str()));
			table->append_node(column);
		}

		for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
		{
			_tXmlNodeType* index = doc.allocate_node(rapidxml::node_type::node_element, _T("Index"));
			index->append_attribute(doc.allocate_attribute(_T("name"), dbIndex->_name.c_str()));

			index->append_attribute(doc.allocate_attribute(_T("kind"), doc.allocate_string(dbIndex->GetKindText().c_str())));
			index->append_attribute(doc.allocate_attribute(_T("type"), doc.allocate_string(dbIndex->GetTypeText().c_str())));

			if( dbIndex->_primaryKey ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("PrimaryKey")));
			if( dbIndex->_uniqueKey ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("UniqueKey")));
			if( dbIndex->_systemNamed ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("SystemNamed")));

			for( DBModel::IndexColumnRef& dbIndexColumn : dbIndex->_columns )
			{
				_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));

				column->append_attribute(doc.allocate_attribute(_T("seq"), dbIndexColumn->_seq.c_str()));
				column->append_attribute(doc.allocate_attribute(_T("name"), dbIndexColumn->_name.c_str()));
				column->append_attribute(doc.allocate_attribute(_T("order"), dbIndexColumn->GetSortText().c_str()));
				index->append_node(column);
			}
			table->append_node(index);
		}

		for( DBModel::ForeignKeyRef& dbForeignKey : dbTable->_foreignKeys )
		{
			_tXmlNodeType* referenceKey = doc.allocate_node(rapidxml::node_type::node_element, _T("ForeignKey"));
			referenceKey->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKey->_foreignKeyName.c_str()));
			referenceKey->append_attribute(doc.allocate_attribute(_T("update_rule"), dbForeignKey->_updateRule.c_str()));
			referenceKey->append_attribute(doc.allocate_attribute(_T("delete_rule"), dbForeignKey->_deleteRule.c_str()));

			_tXmlNodeType* foreignKeyTable = doc.allocate_node(rapidxml::node_type::node_element, _T("ForeignKeyTable"));
			foreignKeyTable->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKey->_foreignKeyTableName.c_str()));
			for( DBModel::IndexColumnRef& dbForeignKeyColumn : dbForeignKey->_foreignKeyColumns )
			{
				_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));
				column->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKeyColumn->_name.c_str()));
				foreignKeyTable->append_node(column);
			}
			referenceKey->append_node(foreignKeyTable);

			_tXmlNodeType* referenceKeyTable = doc.allocate_node(rapidxml::node_type::node_element, _T("ReferenceKeyTable"));
			referenceKeyTable->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKey->_referenceKeyTableName.c_str()));
			for( DBModel::IndexColumnRef& dbReferenceKeyColumn : dbForeignKey->_referenceKeyColumns )
			{
				_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));
				column->append_attribute(doc.allocate_attribute(_T("name"), dbReferenceKeyColumn->_name.c_str()));
				referenceKeyTable->append_node(column);
			}
			referenceKey->append_node(referenceKeyTable);

			table->append_node(referenceKey);
		}
	}

	for( DBModel::ProcedureRef& dbProcedure : _dbProcedures )
	{
		_tXmlNodeType* procedure = doc.allocate_node(rapidxml::node_type::node_element, _T("Procedure"));
		procedure->append_attribute(doc.allocate_attribute(_T("schemaname"), dbProcedure->_schemaName.c_str()));
		procedure->append_attribute(doc.allocate_attribute(_T("name"), dbProcedure->_name.c_str()));
		procedure->append_attribute(doc.allocate_attribute(_T("desc"), dbProcedure->_desc.c_str()));
		for( DBModel::ProcParamRef& dbProcParam : dbProcedure->_parameters )
		{
			_tXmlNodeType* procParam = doc.allocate_node(rapidxml::node_type::node_element, _T("Param"));
			procParam->append_attribute(doc.allocate_attribute(_T("seq"), dbProcParam->_paramId.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("mode"), doc.allocate_string(ToString(dbProcParam->_paramMode))));
			procParam->append_attribute(doc.allocate_attribute(_T("name"), dbProcParam->_name.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("type"), dbProcParam->_datatypedesc.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("desc"), dbProcParam->_desc.c_str()));
			procedure->append_node(procParam);
		}

		_tXmlNodeType* Body = doc.allocate_node(rapidxml::node_type::node_element, _T("body"));
		Body->value(dbProcedure->_fullBody.c_str());
		procedure->append_node(Body);

		root->append_node(procedure);
	}

	for( DBModel::FunctionRef& dbFunction : _dbFunctions )
	{
		_tXmlNodeType* function = doc.allocate_node(rapidxml::node_type::node_element, _T("Function"));
		function->append_attribute(doc.allocate_attribute(_T("schemaname"), dbFunction->_schemaName.c_str()));
		function->append_attribute(doc.allocate_attribute(_T("name"), dbFunction->_name.c_str()));
		function->append_attribute(doc.allocate_attribute(_T("desc"), dbFunction->_desc.c_str()));
		for( DBModel::FuncParamRef& dbFuncParam : dbFunction->_parameters )
		{
			_tXmlNodeType* funcParam = doc.allocate_node(rapidxml::node_type::node_element, _T("Param"));
			funcParam->append_attribute(doc.allocate_attribute(_T("seq"), dbFuncParam->_paramId.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("mode"), doc.allocate_string(ToString(dbFuncParam->_paramMode))));
			funcParam->append_attribute(doc.allocate_attribute(_T("name"), dbFuncParam->_name.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("type"), dbFuncParam->_datatypedesc.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("desc"), dbFuncParam->_desc.c_str()));
			function->append_node(funcParam);
		}

		_tXmlNodeType* Body = doc.allocate_node(rapidxml::node_type::node_element, _T("body"));
		Body->value(dbFunction->_fullBody.c_str());
		function->append_node(Body);

		root->append_node(function);
	}

	_tstring xmlString;
	rapidxml::print(std::back_inserter(xmlString), doc);

	SaveUTF8NOBOMFile(path, xmlString.c_str(), xmlString.size());

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBTables()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	int64	auto_increment_value;
	TCHAR	tszStorageEngine[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCharacterSet[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCollation[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszTableComment[DATABASE_WVARCHAR_MAX] = {0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = {0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = {0, };

	SP::GetDBTables getDBTables(_dbClass, _dbConn);
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
		DBModel::TableRef tableRef = MakeShared<DBModel::Table>();
		tableRef->_objectId = objectId;
		tableRef->_schemaName = tszSchemaName;
		tableRef->_name = tszTableName;
		tableRef->_desc = tszTableComment;
		tableRef->_auto_increment_value = tstring_format(_T("%lld"), auto_increment_value);
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
bool CDBSynchronizer::GatherDBTableColumns()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN] = {0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	int32	seq;
	uint64	maxLength;
	uint8	precision;
	uint8	scale;
	BOOL	isNullable;
	BOOL	isIdentity;
	int64	seedValue;
	int64	incValue;
	TCHAR	tszCharacterSet[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszCollation[DATABASE_BASE_STRLEN] = { 0, };
	TCHAR	tszDefaultConstraintName[101] = { 0, };
	TCHAR	tszDefaultDefinition[101] = {0, };
	TCHAR	tszColumnComment[DATABASE_WVARCHAR_MAX] = {0, };

	SP::GetDBTableColumns getDBTableColumns(_dbClass, _dbConn);
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
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(table->_name.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});

		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::ColumnRef>& columns = (*findTable)->_columns;
		auto findColumn = std::find_if(columns.begin(), columns.end(), [tszColumnName](DBModel::ColumnRef& column) { return _tcsicmp(column->_name.c_str(), tszColumnName) == 0 ? true : false; });
		if( findColumn == columns.end() )
		{
			DBModel::ColumnRef columnRef = MakeShared<DBModel::Column>();
			{
				columnRef->_schemaName = (*findTable)->_schemaName;
				columnRef->_tableName = (*findTable)->_name;
				columnRef->_seq = tstring_format(_T("%d"), seq);
				columnRef->_name = tszColumnName;
				columnRef->_datatype = tszDataType;
				columnRef->_maxLength = (_tcsicmp(columnRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(columnRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				columnRef->_precision = precision;
				columnRef->_scale = scale;
				columnRef->_datatypedesc = tszDataTypeDesc;
				columnRef->_nullable = isNullable;
				columnRef->_identity = isIdentity;
				if( columnRef->_identity ) columnRef->_identitydesc = tstring_format(_T("%lld,%lld"), columnRef->_seedValue, columnRef->_incrementValue);
				columnRef->_seedValue = (isIdentity ? seedValue : 0);
				columnRef->_incrementValue = (isIdentity ? incValue : 0);
				columnRef->_defaultDefinition = tszDefaultDefinition;
				columnRef->_defaultConstraintName = tszDefaultConstraintName;
				columnRef->_characterset = tszCharacterSet;
				columnRef->_collation = tszCollation;
				columnRef->_desc = tszColumnComment;
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
bool CDBSynchronizer::GatherDBIndexes()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	int32	indexId;
	int8	indexKind;
	int8	indexType;
	BOOL	isPrimaryKey;
	BOOL	isUnique;
	BOOL	isSystemNamed;
	int32	columnSeq;
	TCHAR	tszColumnName[DATABASE_COLUMN_NAME_STRLEN] = {0, };
	int8	columnSort;

	SP::GetDBIndexes getDBIndexes(_dbClass, _dbConn);
	getDBIndexes.Out_ObjectId(OUT objectId);
	getDBIndexes.Out_SchemaName(OUT tszSchemaName);
	getDBIndexes.Out_TableName(OUT tszTableName);
	getDBIndexes.Out_IndexName(OUT tszIndexName);
	getDBIndexes.Out_IndexId(OUT indexId);
	getDBIndexes.Out_IndexKind(OUT indexKind);
	getDBIndexes.Out_IndexType(OUT indexType);
	getDBIndexes.Out_IsPrimaryKey(OUT isPrimaryKey);
	getDBIndexes.Out_IsUnique(OUT isUnique);
	getDBIndexes.Out_IsSystemNamed(OUT isSystemNamed);
	getDBIndexes.Out_ColumnSeq(OUT columnSeq);
	getDBIndexes.Out_ColumnName(OUT tszColumnName);
	getDBIndexes.Out_ColumnSort(OUT columnSort);

	if( getDBIndexes.ExecDirect() == false )
		return false;

	while( getDBIndexes.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(table->_name.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::IndexRef>& indexes = (*findTable)->_indexes;
		auto findIndex = std::find_if(indexes.begin(), indexes.end(), [tszIndexName](DBModel::IndexRef& index) { return _tcsicmp(index->_name.c_str(), tszIndexName) == 0 ? true : false; });
		if( findIndex == indexes.end() )
		{
			DBModel::IndexRef indexRef = MakeShared<DBModel::Index>();
			{
				indexRef->_schemaName = (*findTable)->_schemaName;
				indexRef->_tableName = (*findTable)->_name;
				indexRef->_name = tszIndexName;
				indexRef->_indexId = indexId;
				indexRef->_kind = static_cast<EIndexKind>(indexKind);
				indexRef->_type = static_cast<EIndexType>(indexType);
				indexRef->_primaryKey = isPrimaryKey;
				indexRef->_uniqueKey = isUnique;
				indexRef->_systemNamed = isSystemNamed;
			}
			indexes.push_back(indexRef);
			findIndex = indexes.end() - 1;
		}

		DBModel::IndexColumnRef indexColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			indexColumnRef->_seq = tstring_format(_T("%d"), columnSeq);
			indexColumnRef->_name = tszColumnName;
			indexColumnRef->_sort = static_cast<EIndexSort>(columnSort);
		};		
		(*findIndex)->_columns.push_back(indexColumnRef);
	}

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBIndexOptions()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = { 0, };
	TCHAR	tszIndexName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
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

	SP::GetDBIndexOptions getDBIndexOptions(_dbClass, _dbConn);
	getDBIndexOptions.Out_ObjectId(OUT objectId);
	getDBIndexOptions.Out_SchemaName(OUT tszSchemaName);
	getDBIndexOptions.Out_TableName(OUT tszTableName);
	getDBIndexOptions.Out_IndexName(OUT tszIndexName);
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
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(table->_name.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());
		CVector<DBModel::IndexOptionRef>& indexOptions = (*findTable)->_indexOptions;
		auto findIndexOption = std::find_if(indexOptions.begin(), indexOptions.end(), [tszIndexName](DBModel::IndexOptionRef& indexOption) { return _tcsicmp(indexOption->_name.c_str(), tszIndexName) == 0 ? true : false; });
		if( findIndexOption == indexOptions.end() )
		{
			DBModel::IndexOptionRef indexOptionRef = MakeShared<DBModel::IndexOption>();
			{
				indexOptionRef->_schemaName = (*findTable)->_schemaName;
				indexOptionRef->_tableName = (*findTable)->_name;
				indexOptionRef->_name = tszIndexName;
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
bool CDBSynchronizer::GatherDBForeignKeys()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszForeignKeyName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszForeignKeyTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszForeignKeyColumnName[DATABASE_COLUMN_NAME_STRLEN] = {0, };
	TCHAR	tszReferenceKeyTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszReferenceKeyColumnName[DATABASE_COLUMN_NAME_STRLEN] = {0, };
	TCHAR	tszUpdateRule[101] = {0, };
	TCHAR	tszDeleteRule[101] = {0, };

	SP::GetDBForeignKeys getDBForeignKeys(_dbClass, _dbConn);
	getDBForeignKeys.Out_ObjectId(OUT objectId);
	getDBForeignKeys.Out_SchemaName(OUT tszSchemaName);
	getDBForeignKeys.Out_TableName(OUT tszTableName);
	getDBForeignKeys.Out_ForeignKeyName(OUT tszForeignKeyName);
	getDBForeignKeys.Out_ForeignKeyTableName(OUT tszForeignKeyTableName);
	getDBForeignKeys.Out_ForeignKeyColumnName(OUT tszForeignKeyColumnName);
	getDBForeignKeys.Out_ReferenceKeyTableName(OUT tszReferenceKeyTableName);
	getDBForeignKeys.Out_ReferenceKeyColumnName(OUT tszReferenceKeyColumnName);
	getDBForeignKeys.Out_UpdateRule(OUT tszUpdateRule);
	getDBForeignKeys.Out_DeleteRule(OUT tszDeleteRule);

	if( getDBForeignKeys.ExecDirect() == false )
		return false;

	while( getDBForeignKeys.Fetch() )
	{
		auto findTable = std::find_if(_dbTables.begin(), _dbTables.end(), [=](const DBModel::TableRef& table)
		{
			if( _dbClass == EDBClass::MSSQL ) return table->_objectId == objectId;
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(table->_name.c_str(), tszTableName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findTable != _dbTables.end());

		CVector<DBModel::ForeignKeyRef>& referenceKeys = (*findTable)->_foreignKeys;
		auto findReferenceKey = std::find_if(referenceKeys.begin(), referenceKeys.end(), [tszForeignKeyName](DBModel::ForeignKeyRef& referenceKey) { return _tcsicmp(referenceKey->_foreignKeyName.c_str(), tszForeignKeyName) == 0 ? true : false; });
		if( findReferenceKey == referenceKeys.end() )
		{
			DBModel::ForeignKeyRef ForeignKeyRef = MakeShared<DBModel::ForeignKey>();
			{
				ForeignKeyRef->_schemaName = (*findTable)->_schemaName;
				ForeignKeyRef->_tableName = (*findTable)->_name;
				ForeignKeyRef->_foreignKeyName = tszForeignKeyName;
				ForeignKeyRef->_foreignKeyTableName = tszForeignKeyTableName;
				ForeignKeyRef->_referenceKeyTableName = tszReferenceKeyTableName;
				ForeignKeyRef->_updateRule = tszUpdateRule;
				ForeignKeyRef->_deleteRule = tszDeleteRule;
			}
			referenceKeys.push_back(ForeignKeyRef);
			findReferenceKey = referenceKeys.end() - 1;
		}

		DBModel::IndexColumnRef foreignKeyColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			foreignKeyColumnRef->_name = tszForeignKeyColumnName;
		};
		(*findReferenceKey)->_foreignKeyColumns.push_back(foreignKeyColumnRef);

		DBModel::IndexColumnRef referenceKeyColumnRef = MakeShared<DBModel::IndexColumn>();
		{
			referenceKeyColumnRef->_name = tszReferenceKeyColumnName;
		};
		(*findReferenceKey)->_referenceKeyColumns.push_back(referenceKeyColumnRef);
	}

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBTrigger()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszTableName[DATABASE_TABLE_NAME_STRLEN] = {0, };
	TCHAR	tszTriggerName[DATABASE_OBJECT_NAME_STRLEN] = {0, };

	SP::GetDBTriggers getDBTriggers(_dbClass, _dbConn);
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
			iterTriggerRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::TRIGGERS, iterTriggerRef->_triggerName.c_str());
		else iterTriggerRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::TRIGGERS, iterTriggerRef->_triggerName.c_str());
	}

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBStoredProcedures()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszProcName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszProcComment[DATABASE_WVARCHAR_MAX] = {0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = {0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = {0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBStoredProcedures getDBStoredProcedures(_dbClass, _dbConn);
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
		procRef->_name = tszProcName;
		procRef->_desc = tszProcComment;
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
			iterProcRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::PROCEDURE, iterProcRef->_name.c_str());
		}
	}
	else if( _dbClass == EDBClass::MYSQL )
	{
		for( auto iter = _dbProcedures.begin(); iter != _dbProcedures.end(); iter++ )
		{
			DBModel::ProcedureRef iterProcRef = *iter;
			iterProcRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::PROCEDURE, iterProcRef->_name.c_str());
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBStoredProcedureParams()
{
	int32	objectId;
	TCHAR	tszProcName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	int32	paramId;
	int8	paramMode;
	TCHAR	tszParamName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	uint64	maxLength;
	uint8	precision;
	uint8	scale;	
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX] = {0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBStoredProcedureParams getDBStoredProcedureParams(_dbClass, _dbConn);
	getDBStoredProcedureParams.Out_ObjectId(OUT objectId);
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
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(proc->_name.c_str(), tszProcName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findProc != _dbProcedures.end());
		CVector<DBModel::ProcParamRef>& procParams = (*findProc)->_parameters;
		auto findProcParam = std::find_if(procParams.begin(), procParams.end(), [tszParamName](DBModel::ProcParamRef& procParam) { return _tcsicmp(procParam->_name.c_str(), tszParamName) == 0 ? true : false; });
		if( findProcParam == procParams.end() )
		{
			DBModel::ProcParamRef procParamRef = MakeShared<DBModel::ProcParam>();
			{
				procParamRef->_paramId = tstring_format(_T("%d"), paramId);;
				procParamRef->_paramMode = static_cast<EParameterMode>(paramMode);
				procParamRef->_name = tszParamName;
				procParamRef->_datatype = tszDataType;
				procParamRef->_maxLength = (_tcsicmp(procParamRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(procParamRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				procParamRef->_precision = precision;
				procParamRef->_scale = scale;
				procParamRef->_datatypedesc = tszDataTypeDesc;
				procParamRef->_desc = tszParamComment;
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
bool CDBSynchronizer::GatherDBFunctions()
{
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszFuncName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszFuncComment[DATABASE_WVARCHAR_MAX] = {0, };
	TCHAR	tszCreateDate[DATETIME_STRLEN] = {0, };
	TCHAR	tszModifyDate[DATETIME_STRLEN] = {0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBFunctions getDBFunctions(_dbClass, _dbConn);
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
		funcRef->_name = tszFuncName;
		funcRef->_desc = tszFuncComment;
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
			iterFuncRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::FUNCTION, iterFuncRef->_name.c_str());
		}
	}
	else if( _dbClass == EDBClass::MYSQL )
	{
		for( auto iter = _dbFunctions.begin(); iter != _dbFunctions.end(); iter++ )
		{
			DBModel::FunctionRef iterFuncRef = *iter;
			iterFuncRef->_fullBody = dbQuery.MYSQLShowObject(EDBObjectType::FUNCTION, iterFuncRef->_name.c_str());
		}
	}

	return true;
}

//***************************************************************************
//
bool CDBSynchronizer::GatherDBFunctionParams()
{
	int32	objectId;
	TCHAR	tszFuncName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	int32	paramId;
	int8	paramMode;
	TCHAR	tszParamName[DATABASE_OBJECT_NAME_STRLEN] = {0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	TCHAR	tszDataTypeDesc[DATABASE_DATATYPEDESC_STRLEN] = {0, };
	uint64	maxLength;
	uint8	precision;
	uint8	scale;
	TCHAR	tszParamComment[DATABASE_WVARCHAR_MAX] = {0, };

	CVector<TCHAR> body;
	body.resize(DATABASE_OBJECT_CONTENTTEXT_STRLEN);

	SP::GetDBFunctionParams getDBFunctionParams(_dbClass, _dbConn);
	getDBFunctionParams.Out_ObjectId(OUT objectId);
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
			else if( _dbClass == EDBClass::MYSQL ) return _tcsicmp(func->_name.c_str(), tszFuncName) == 0 ? true : false;

			return false;
		});
		ASSERT_CRASH(findFunc != _dbFunctions.end());
		CVector<DBModel::FuncParamRef>& funcParams = (*findFunc)->_parameters;
		auto findFuncParam = std::find_if(funcParams.begin(), funcParams.end(), [tszParamName](DBModel::FuncParamRef& funcParam) { return _tcsicmp(funcParam->_name.c_str(), tszParamName) == 0 ? true : false; });
		if( findFuncParam == funcParams.end() )
		{
			DBModel::FuncParamRef funcParamRef = MakeShared<DBModel::FuncParam>();
			{
				funcParamRef->_paramId = tstring_format(_T("%d"), paramId);;
				funcParamRef->_paramMode = static_cast<EParameterMode>(paramMode);
				funcParamRef->_name = tszParamName;
				funcParamRef->_datatype = tszDataType;
				funcParamRef->_maxLength = (_tcsicmp(funcParamRef->_datatype.c_str(), _T("nvarchar")) == 0 || _tcsicmp(funcParamRef->_datatype.c_str(), _T("nchar")) == 0 ? maxLength / 2 : maxLength);
				funcParamRef->_precision = precision;
				funcParamRef->_scale = scale;
				funcParamRef->_datatypedesc = tszDataTypeDesc;
				funcParamRef->_desc = tszParamComment;
			}

			funcParams.push_back(funcParamRef);
			findFuncParam = funcParams.end() - 1;
		}

		tszParamComment[0] = _T('\0');
	}

	return true;
}

//***************************************************************************
//
void CDBSynchronizer::CompareDBModel()
{
	// 업데이트 목록 초기화.
	_dependentIndexes.clear();
	_dependentReferenceKeys.clear();
	for( CVector<_tstring>& queries : _updateQueries )
		queries.clear();

	// XML에 있는 목록을 우선 갖고 온다.
	CMap<_tstring, DBModel::TableRef> xmlTableMap;
	for( DBModel::TableRef& xmlTable : _xmlTables )
		xmlTableMap[xmlTable->_name] = xmlTable;

	// DB에 실존하는 테이블들을 돌면서 XML에 정의된 테이블들과 비교한다.
	for( DBModel::TableRef& dbTable : _dbTables )
	{
		auto findTable = xmlTableMap.find(dbTable->_name);
		if( findTable != xmlTableMap.end() )
		{
			DBModel::TableRef xmlTable = findTable->second;
			CompareTables(dbTable, xmlTable);
			xmlTableMap.erase(findTable);
		}
		else
		{
			if( _xmlRemovedTables.find(dbTable->_name) != _xmlRemovedTables.end() )
			{
				LOG_INFO(_T("Removing Table : [dbo].[%s]"), dbTable->_name.c_str());
				_updateQueries[UpdateStep::DropTable].push_back(dbTable->DropTable(_dbClass));
			}
		}
	}

	// 맵에서 제거되지 않은 XML 테이블 정의는 새로 추가.
	for( auto& mapIt : xmlTableMap )
	{
		DBModel::TableRef& xmlTable = mapIt.second;

		_tstring columnsStr;
		const int32 size = static_cast<int32>(xmlTable->_columns.size());
		for( int32 i = 0; i < size; i++ )
		{
			if( i != 0 )
				columnsStr += _T(",");
			columnsStr += _T("\n\t");
			columnsStr += xmlTable->_columns[i]->CreateText(_dbClass);
		}

		// 테이블 생성
		LOG_INFO(_T("Creating Table : [dbo].[%s]"), xmlTable->_name.c_str());
		_updateQueries[UpdateStep::CreateTable].push_back(xmlTable->CreateTable(_dbClass));

		for( DBModel::ColumnRef& xmlColumn : xmlTable->_columns )
		{
			if( xmlColumn->_defaultDefinition.empty() )
				continue;

			_updateQueries[UpdateStep::DefaultConstraint].push_back(xmlColumn->CreateDefaultConstraint(_dbClass));
		}

		for( DBModel::IndexRef& xmlIndex : xmlTable->_indexes )
		{
			LOG_INFO(_T("Creating Index : [%s] %s %s [%s]"), xmlTable->_name.c_str(), xmlIndex->GetKeyText().c_str(), xmlIndex->GetKindText().c_str(), xmlIndex->GetIndexName(_dbClass).c_str());
			_updateQueries[UpdateStep::CreateIndex].push_back(xmlIndex->CreateIndex(_dbClass));
		}

		for( DBModel::ForeignKeyRef& xmlForeignKey : xmlTable->_foreignKeys )
		{
			LOG_INFO(_T("Creating ForeignKey : [%s] [%s]"), xmlTable->_name.c_str(), xmlForeignKey->GetForeignKeyName(_dbClass).c_str());
			_updateQueries[UpdateStep::CreateForeignKey].push_back(xmlForeignKey->CreateForeignKey(_dbClass));
		}
	}

	CompareStoredProcedures();
	CompareFunctions();
}

//***************************************************************************
//
void CDBSynchronizer::ExecuteUpdateQueries()
{
	for( int32 step = 0; step < UpdateStep::Max; step++ )
	{
		for( _tstring& query : _updateQueries[step] )
		{
			_dbConn.ClearStmt();
			ASSERT_CRASH(_dbConn.ExecDirect(query.c_str()));
		}
	}
}

//***************************************************************************
//
void CDBSynchronizer::CompareTables(DBModel::TableRef dbTable, DBModel::TableRef xmlTable)
{
	// XML에 있는 컬럼 목록을 갖고 온다.
	CMap<_tstring, DBModel::ColumnRef> xmlColumnMap;
	for( DBModel::ColumnRef& xmlColumn : xmlTable->_columns )
		xmlColumnMap[xmlColumn->_name] = xmlColumn;

	// DB에 실존하는 테이블 컬럼들을 돌면서 XML에 정의된 컬럼들과 비교한다.
	for( DBModel::ColumnRef& dbColumn : dbTable->_columns )
	{
		auto findColumn = xmlColumnMap.find(dbColumn->_name);
		if( findColumn != xmlColumnMap.end() )
		{
			DBModel::ColumnRef& xmlColumn = findColumn->second;
			CompareColumns(dbTable, dbColumn, xmlColumn);
			xmlColumnMap.erase(findColumn);
		}
		else
		{
			LOG_INFO(_T("Dropping Column : [%s].[%s]"), dbTable->_name.c_str(), dbColumn->_name.c_str());
			if( dbColumn->_defaultConstraintName.empty() == false )
				_updateQueries[UpdateStep::DropColumn].push_back(dbColumn->DropDefaultConstraint(_dbClass));

			_updateQueries[UpdateStep::DropColumn].push_back(dbColumn->DropColumn(_dbClass));
		}
	}

	// 맵에서 제거되지 않은 XML 컬럼 정의는 새로 추가.
	for( auto& mapIt : xmlColumnMap )
	{
		DBModel::ColumnRef& xmlColumn = mapIt.second;
		DBModel::Column newColumn = *xmlColumn;
		newColumn._nullable = true;

		LOG_INFO(_T("Adding Column : [%s].[%s]"), dbTable->_name.c_str(), xmlColumn->_name.c_str());
		_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->CreateColumn(_dbClass));

		if( xmlColumn->_nullable == false && xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(tstring_format(_T("SET NOCOUNT ON; UPDATE [dbo].[%s] SET [%s] = %s WHERE [%s] IS NULL"),
															dbTable->_name.c_str(), xmlColumn->_name.c_str(), xmlColumn->_defaultDefinition.c_str(), xmlColumn->_name.c_str()));
		}

		if( xmlColumn->_nullable == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->ModifyColumn(_dbClass));
		}

		if( xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->CreateDefaultConstraint(_dbClass));
		}
	}

	// XML에 있는 인덱스 목록을 갖고 온다.
	CMap<_tstring, DBModel::IndexRef> xmlIndexMap;
	for( DBModel::IndexRef& xmlIndex : xmlTable->_indexes )
		xmlIndexMap[xmlIndex->GetIndexName(_dbClass)] = xmlIndex;

	// DB에 실존하는 테이블 인덱스들을 돌면서 XML에 정의된 인덱스들과 비교한다.
	for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
	{
		auto findIndex = xmlIndexMap.find(dbIndex->GetIndexName(_dbClass));
		if( findIndex != xmlIndexMap.end() && _dependentIndexes.find(dbIndex->GetIndexName(_dbClass)) == _dependentIndexes.end() )
		{
			DBModel::IndexRef xmlIndex = findIndex->second;
			xmlIndexMap.erase(findIndex);
		}
		else
		{
			LOG_INFO(_T("Dropping Index : [%s] [%s] %s %s"), dbTable->_name.c_str(), dbIndex->_name.c_str(), dbIndex->GetKeyText().c_str(), dbIndex->GetKindText().c_str());
			_updateQueries[UpdateStep::DropIndex].push_back(dbIndex->DropIndex(_dbClass));
		}
	}

	// 맵에서 제거되지 않은 XML 인덱스 정의는 새로 추가.
	for( auto& mapIt : xmlIndexMap )
	{
		DBModel::IndexRef xmlIndex = mapIt.second;
		LOG_INFO(_T("Creating Index : [%s] %s %s [%s]"), dbTable->_name.c_str(), xmlIndex->GetKeyText().c_str(), xmlIndex->GetKindText().c_str(), xmlIndex->GetIndexName(_dbClass).c_str());
		_updateQueries[UpdateStep::CreateIndex].push_back(xmlIndex->CreateIndex(_dbClass));
	}

	// XML에 있는 참조키 목록을 갖고 온다.
	CMap<_tstring, DBModel::ForeignKeyRef> xmlForeignKeyMap;
	for( DBModel::ForeignKeyRef& xmlForeignKey : xmlTable->_foreignKeys )
		xmlForeignKeyMap[xmlForeignKey->GetForeignKeyName(_dbClass)] = xmlForeignKey;

	// DB에 실존하는 테이블 참조키들을 돌면서 XML에 정의된 참조키들과 비교한다.
	for( DBModel::ForeignKeyRef& dbForeignKey : dbTable->_foreignKeys )
	{
		auto findReferenceKeys = xmlForeignKeyMap.find(dbForeignKey->GetForeignKeyName(_dbClass));
		if( findReferenceKeys != xmlForeignKeyMap.end() && _dependentReferenceKeys.find(dbForeignKey->GetForeignKeyName(_dbClass)) == _dependentReferenceKeys.end() )
		{
			DBModel::ForeignKeyRef xmlForeignKey = findReferenceKeys->second;
			xmlForeignKeyMap.erase(findReferenceKeys);
		}
		else
		{
			LOG_INFO(_T("Dropping ForeignKey : [%s] [%s]"), dbTable->_name.c_str(), dbForeignKey->_foreignKeyName.c_str());
			_updateQueries[UpdateStep::DropForeignKey].push_back(dbForeignKey->DropForeignKey(_dbClass));
		}
	}

	// 맵에서 제거되지 않은 XML 참조키 정의는 새로 추가.
	for( auto& mapIt : xmlForeignKeyMap )
	{
		DBModel::ForeignKeyRef xmlForeignKey = mapIt.second;
		LOG_INFO(_T("Creating ForeignKey : [%s] [%s]"), dbTable->_name.c_str(), xmlForeignKey->GetForeignKeyName(_dbClass).c_str());
		_updateQueries[UpdateStep::CreateForeignKey].push_back(xmlForeignKey->CreateForeignKey(_dbClass));
	}
}

//***************************************************************************
//
void CDBSynchronizer::CompareColumns(DBModel::TableRef dbTable, DBModel::ColumnRef dbColumn, DBModel::ColumnRef xmlColumn)
{
	uint8 flag = 0;

	if( dbColumn->_datatype != xmlColumn->_datatype )
		flag |= ColumnFlag::Type;
	if( dbColumn->_maxLength != xmlColumn->_maxLength && xmlColumn->_maxLength > 0 )
		flag |= ColumnFlag::Length;
	if( dbColumn->_nullable != xmlColumn->_nullable )
		flag |= ColumnFlag::Nullable;
	if( dbColumn->_identity != xmlColumn->_identity || (dbColumn->_identity && dbColumn->_incrementValue != xmlColumn->_incrementValue) )
		flag |= ColumnFlag::Identity;
	if( dbColumn->_defaultDefinition != xmlColumn->_defaultDefinition )
		flag |= ColumnFlag::Default;

	if( flag )
	{
		LOG_INFO(_T("Updating Column [%s] : (%s) -> (%s)"), dbTable->_name.c_str(), dbColumn->CreateText(_dbClass).c_str(), xmlColumn->CreateText(_dbClass).c_str());
	}

	// 연관된 인덱스가 있으면 나중에 삭제하기 위해 기록한다.
	if( flag & (ColumnFlag::Type | ColumnFlag::Length | ColumnFlag::Nullable) )
	{
		for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
			if( dbIndex->DependsOn(dbColumn->_name) )
				_dependentIndexes.insert(dbIndex->GetIndexName(_dbClass));

		flag |= ColumnFlag::Default;
	}

	if( flag & ColumnFlag::Default )
	{
		if( dbColumn->_defaultConstraintName.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(dbColumn->DropDefaultConstraint(_dbClass));
		}
	}

	DBModel::Column newColumn = *dbColumn;
	newColumn._defaultDefinition = _T("");
	newColumn._datatype = xmlColumn->_datatype;
	newColumn._maxLength = xmlColumn->_maxLength;
	newColumn._datatypedesc = xmlColumn->_datatypedesc;
	newColumn._seedValue = xmlColumn->_seedValue;
	newColumn._incrementValue = xmlColumn->_incrementValue;

	if( flag & (ColumnFlag::Type | ColumnFlag::Length | ColumnFlag::Identity) )
	{
		_updateQueries[UpdateStep::AlterColumn].push_back(newColumn.ModifyColumn(_dbClass));
	}

	newColumn._nullable = xmlColumn->_nullable;
	if( flag & ColumnFlag::Nullable )
	{
		if( xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(tstring_format(
				_T("SET NOCOUNT ON; UPDATE [dbo].[%s] SET [%s] = %s WHERE [%s] IS NULL"),
				dbTable->_name.c_str(),
				xmlColumn->_name.c_str(),
				xmlColumn->_name.c_str(),
				xmlColumn->_name.c_str()));
		}

		_updateQueries[UpdateStep::AlterColumn].push_back(newColumn.ModifyColumn(_dbClass));
	}

	if( flag & ColumnFlag::Default )
	{
		if( dbColumn->_defaultConstraintName.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(dbColumn->CreateDefaultConstraint(_dbClass));
		}
	}
}

//***************************************************************************
//
void CDBSynchronizer::CompareStoredProcedures()
{
	// XML에 있는 프로시저 목록을 갖고 온다.
	CMap<_tstring, DBModel::ProcedureRef> xmlProceduresMap;
	for( DBModel::ProcedureRef& xmlProcedure : _xmlProcedures )
		xmlProceduresMap[xmlProcedure->_name] = xmlProcedure;

	// DB에 실존하는 테이블 프로시저들을 돌면서 XML에 정의된 프로시저들과 비교한다.
	for( DBModel::ProcedureRef& dbProcedure : _dbProcedures )
	{
		auto findProcedure = xmlProceduresMap.find(dbProcedure->_name);
		if( findProcedure != xmlProceduresMap.end() )
		{
			DBModel::ProcedureRef xmlProcedure = findProcedure->second;
			_tstring xmlBody = xmlProcedure->CreateQuery();
			if( DBModel::Helpers::RemoveWhiteSpace(dbProcedure->_fullBody) != DBModel::Helpers::RemoveWhiteSpace(xmlBody) )
			{
				LOG_INFO(_T("Updating Procedure : %s"), dbProcedure->_name.c_str());
				_updateQueries[UpdateStep::DropStoredProcecure].push_back(xmlProcedure->DropQuery());
				_updateQueries[UpdateStep::CreateStoredProcecure].push_back(xmlProcedure->CreateQuery());
			}
			xmlProceduresMap.erase(findProcedure);
		}
	}

	// 맵에서 제거되지 않은 XML 프로시저 정의는 새로 추가.
	for( auto& mapIt : xmlProceduresMap )
	{
		LOG_INFO(_T("Updating Procedure : %s"), mapIt.first.c_str());
		_updateQueries[UpdateStep::CreateStoredProcecure].push_back(mapIt.second->CreateQuery());
	}
}

//***************************************************************************
//
void CDBSynchronizer::CompareFunctions()
{
	// XML에 있는 프로시저 목록을 갖고 온다.
	CMap<_tstring, DBModel::FunctionRef> xmlFunctionsMap;
	for( DBModel::FunctionRef& xmlFunction : _xmlFunctions )
		xmlFunctionsMap[xmlFunction->_name] = xmlFunction;

	// DB에 실존하는 테이블 프로시저들을 돌면서 XML에 정의된 프로시저들과 비교한다.
	for( DBModel::FunctionRef& dbFunction : _dbFunctions )
	{
		auto findFunction = xmlFunctionsMap.find(dbFunction->_name);
		if( findFunction != xmlFunctionsMap.end() )
		{
			DBModel::FunctionRef xmlFunction = findFunction->second;
			_tstring xmlBody = xmlFunction->CreateQuery();
			if( DBModel::Helpers::RemoveWhiteSpace(dbFunction->_fullBody) != DBModel::Helpers::RemoveWhiteSpace(xmlBody) )
			{
				LOG_INFO(_T("Updating Procedure : %s"), dbFunction->_name.c_str());
				_updateQueries[UpdateStep::DropFunction].push_back(xmlFunction->DropQuery());
				_updateQueries[UpdateStep::CreateFunction].push_back(xmlFunction->CreateQuery());
			}
			xmlFunctionsMap.erase(findFunction);
		}
	}

	// 맵에서 제거되지 않은 XML 프로시저 정의는 새로 추가.
	for( auto& mapIt : xmlFunctionsMap )
	{
		LOG_INFO(_T("Updating Procedure : %s"), mapIt.first.c_str());
		_updateQueries[UpdateStep::CreateFunction].push_back(mapIt.second->CreateQuery());
	}
}