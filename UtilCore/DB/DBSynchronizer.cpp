
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
	GatherDBTableConstraints();
	if( _dbClass == EDBClass::ORACLE ) GatherDBIdentityColumns();
	GatherDBIndexes();
	if( _dbClass == EDBClass::MSSQL ) GatherDBIndexOptions();
	GatherDBForeignKeys();
	if( _dbClass == EDBClass::MSSQL ) GatherDBDefaultConstraints();
	GatherDBCheckConstraints();
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


	if( _dbClass == EDBClass::ORACLE )
	{
		query = ORACLEGetTableIdentityColumnInfoQuery();
		DBModel::Helpers::LogFileWrite(_dbClass, _T("[테이블 컬럼 Identity 명세]"), query, true);
	}

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
void CDBSynchronizer::DBToSaveExcel(const _tstring path)
{
	try
	{
		Xlnt::CXlntUtil excel;

		AddExcelTableInfo(excel);
		AddExcelTableColumnInfo(excel);
		AddExcelConstraintsInfo(excel);
		AddExcelIndexInfo(excel);
		AddExcelForeignKeyInfo(excel);

		// 저장
		excel.SaveAs(path);
	}
	catch( const std::exception& e ) 
	{
		std::cerr << "오류 발생: " << e.what() << std::endl;
	}
}

//***************************************************************************
//
void CDBSynchronizer::ToHeaderStyle(Xlnt::CXlntUtil& excel, const std::string& start_cell, const std::string& end_cell)
{
	excel.SetRowHeight(1, 20);

	auto borderRange = excel.CreateRange(start_cell, end_cell);

	// 배경색 설정
	xlnt::fill header_fill;
	header_fill = xlnt::pattern_fill().type(xlnt::pattern_fill_type::solid);
	header_fill = header_fill.pattern_fill().foreground(xlnt::color::black());
	header_fill = header_fill.pattern_fill().background(xlnt::color::black());

	// 테두리 설정
	xlnt::border cell_border;
	xlnt::border::border_property outer_prop;
	outer_prop.style(xlnt::border_style::thin);

	cell_border.side(xlnt::border_side::bottom, outer_prop);
	cell_border.side(xlnt::border_side::start, outer_prop);
	cell_border.side(xlnt::border_side::top, outer_prop);
	cell_border.side(xlnt::border_side::end, outer_prop);

	// 폰트 스타일 설정
	xlnt::font title_font;
	title_font.name("Gulim");					// 폰트 패밀리
	title_font.bold(true);						// 폰트 굵게
	title_font.size(10);						// 폰트 크기
	title_font.color(xlnt::color::white());		// 폰트 색깔

	// 정렬 설정
	xlnt::alignment center_align;
	center_align.horizontal(xlnt::horizontal_alignment::center);
	center_align.vertical(xlnt::vertical_alignment::center);

	borderRange.border(cell_border);
	borderRange.fill(header_fill);
	borderRange.font(title_font);
	borderRange.alignment(center_align);
}

//***************************************************************************
//
void CDBSynchronizer::AddExcelTableInfo(Xlnt::CXlntUtil& excel)
{
	int32 currentRow = 1;

	// 활성화 되어있는 기존 시트명('Sheet1')을 다른 시트명('TABLE')로 변경
	excel.RenameSheet("TABLE");
	ToHeaderStyle(excel, "A1", "I1");

	excel.WriteCell(currentRow, 1, "SCHEMA_NAME");
	excel.WriteCell(currentRow, 2, "TABLE_NAME");
	excel.WriteCell(currentRow, 3, "IDENTITY");
	excel.WriteCell(currentRow, 4, "CHARACTER_SET");
	excel.WriteCell(currentRow, 5, "COLLATION");
	excel.WriteCell(currentRow, 6, "ENGINE");
	excel.WriteCell(currentRow, 7, "TABLE_COMMENT");
	excel.WriteCell(currentRow, 8, "CREATE_DATE");
	excel.WriteCell(currentRow, 9, "MODIFY_DATE");

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		currentRow++;
		excel.WriteCell(currentRow, 1, dbTable->_schemaName);
		excel.WriteCell(currentRow, 2, dbTable->_tableName);
		excel.WriteCell(currentRow, 3, dbTable->_auto_increment_value);
		excel.WriteCell(currentRow, 4, dbTable->_characterset);
		excel.WriteCell(currentRow, 5, dbTable->_collation);
		excel.WriteCell(currentRow, 6, dbTable->_storageEngine);
		excel.WriteCell(currentRow, 7, dbTable->_tableComment);
		excel.WriteCell(currentRow, 8, dbTable->_createDate);
		excel.WriteCell(currentRow, 9, dbTable->_modifyDate);
		excel.SetRowHeight(currentRow, 16);
	}

	excel.SetAllCellTextFormat(3);

	excel.SetColumnWidth(1, ExcelColumnWidth::cnSchemaName);
	excel.SetColumnWidth(2, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(3, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(4, ExcelColumnWidth::cnCharacterSet);
	excel.SetColumnWidth(5, ExcelColumnWidth::cnCollation);
	excel.SetColumnWidth(6, ExcelColumnWidth::cnEngine);
	excel.SetColumnWidth(7, ExcelColumnWidth::cnComment);
	excel.SetColumnWidth(8, ExcelColumnWidth::cnDateTime);
	excel.SetColumnWidth(9, ExcelColumnWidth::cnDateTime);
}

//***************************************************************************
//
void CDBSynchronizer::AddExcelTableColumnInfo(Xlnt::CXlntUtil& excel)
{
	int32 currentRow = 1;

	excel.AddSheet("TABLE_COLUMN");
	ToHeaderStyle(excel, "A1", "Q1");

	excel.WriteCell(currentRow, 1, "SCHEMA_NAME");
	excel.WriteCell(currentRow, 2, "TABLE_NAME");
	excel.WriteCell(currentRow, 3, "COLUMN_SEQ");
	excel.WriteCell(currentRow, 4, "COLUMN_NAME");
	excel.WriteCell(currentRow, 5, "DATATYPE");
	excel.WriteCell(currentRow, 6, "MAX_LENGTH");
	excel.WriteCell(currentRow, 7, "PRECISION");
	excel.WriteCell(currentRow, 8, "SCALE");
	excel.WriteCell(currentRow, 9, "DATATYPEDESC");
	excel.WriteCell(currentRow, 10, "IS_NULLABLE");
	excel.WriteCell(currentRow, 11, "DEFAULT_DEFINITION");
	excel.WriteCell(currentRow, 12, "IS_IDENTITY");
	excel.WriteCell(currentRow, 13, "SEED_VALUE");
	excel.WriteCell(currentRow, 14, "INC_VALUE");
	excel.WriteCell(currentRow, 15, "CHARACTER_SET");
	excel.WriteCell(currentRow, 16, "COLLATION");
	excel.WriteCell(currentRow, 17, "COLUMN_COMMENT");

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		for( DBModel::ColumnRef& dbColumn : dbTable->_columns )
		{
			currentRow++;
			excel.WriteCell(currentRow, 1, dbColumn->_schemaName);
			excel.WriteCell(currentRow, 2, dbColumn->_tableName);
			excel.WriteCell(currentRow, 3, dbColumn->_seq);
			excel.WriteCell(currentRow, 4, dbColumn->_columnName);
			excel.WriteCell(currentRow, 5, dbColumn->_datatype);
			excel.WriteCell(currentRow, 6, std::to_string(dbColumn->_maxLength));
			excel.WriteCell(currentRow, 7, std::to_string(dbColumn->_precision));
			excel.WriteCell(currentRow, 8, std::to_string(dbColumn->_scale));
			excel.WriteCell(currentRow, 9, dbColumn->_datatypedesc);
			excel.WriteCell(currentRow, 10, std::to_string(dbColumn->_nullable));
			excel.WriteCell(currentRow, 11, dbColumn->_defaultConstraintName);
			excel.WriteCell(currentRow, 12, dbColumn->_identitydesc);
			excel.WriteCell(currentRow, 13, std::to_string(dbColumn->_seedValue));
			excel.WriteCell(currentRow, 14, std::to_string(dbColumn->_incrementValue));
			excel.WriteCell(currentRow, 15, dbColumn->_characterset);
			excel.WriteCell(currentRow, 16, dbColumn->_collation);
			excel.WriteCell(currentRow, 17, dbColumn->_columnComment);
			excel.SetRowHeight(currentRow, 16);
		}
	}

	excel.SetAllCellTextFormat(3);

	excel.SetColumnWidth(1, ExcelColumnWidth::cnSchemaName);
	excel.SetColumnWidth(2, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(3, ExcelColumnWidth::cnSequence);
	excel.SetColumnWidth(4, ExcelColumnWidth::cnColumnName);
	excel.SetColumnWidth(5, ExcelColumnWidth::cnDataType);
	excel.SetColumnWidth(6, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(7, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(8, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(9, ExcelColumnWidth::cnDataTypeDesc);
	excel.SetColumnWidth(10, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(11, ExcelColumnWidth::cnDefaultConstraintValue);
	excel.SetColumnWidth(12, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(13, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(14, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(15, ExcelColumnWidth::cnCharacterSet);
	excel.SetColumnWidth(16, ExcelColumnWidth::cnCollation);
	excel.SetColumnWidth(17, ExcelColumnWidth::cnComment);
}

//***************************************************************************
//
void CDBSynchronizer::AddExcelConstraintsInfo(Xlnt::CXlntUtil& excel)
{
	int32 currentRow = 1;

	excel.AddSheet("TABLE_CONSTRAINTS");
	ToHeaderStyle(excel, "A1", "H1");

	excel.WriteCell(currentRow, 1, "SCHEMA_NAME");
	excel.WriteCell(currentRow, 2, "TABLE_NAME");
	excel.WriteCell(currentRow, 3, "CONSTRAINT_NAME");
	excel.WriteCell(currentRow, 4, "CONSTRAINT_TYPE");
	excel.WriteCell(currentRow, 5, "CONSTRAINT_TYPE_DESC");
	excel.WriteCell(currentRow, 6, "CONST_VALUE");
	excel.WriteCell(currentRow, 7, "IS_SYSTEM_NAMED");
	excel.WriteCell(currentRow, 8, "IS_STATUS");

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		for( DBModel::ConstraintRef& dbConstraint : dbTable->_constraints )
		{
			currentRow++;
			excel.WriteCell(currentRow, 1, dbConstraint->_schemaName);
			excel.WriteCell(currentRow, 2, dbConstraint->_tableName);
			excel.WriteCell(currentRow, 3, dbConstraint->_constName);
			excel.WriteCell(currentRow, 4, dbConstraint->_constType);
			excel.WriteCell(currentRow, 5, dbConstraint->_constTypeDesc);
			excel.WriteCell(currentRow, 6, dbConstraint->_constValue);
			excel.WriteCell(currentRow, 7, std::to_string(dbConstraint->_systemNamed));
			excel.WriteCell(currentRow, 8, std::to_string(dbConstraint->_status));
			excel.SetRowHeight(currentRow, 16);
		}
	}

	excel.SetAllCellTextFormat(3);

	excel.SetColumnWidth(1, ExcelColumnWidth::cnSchemaName);
	excel.SetColumnWidth(2, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(3, ExcelColumnWidth::cnConstraintName);
	excel.SetColumnWidth(4, ExcelColumnWidth::cnDefaultWidth + 6);
	excel.SetColumnWidth(5, ExcelColumnWidth::cnDefaultWidth + 14);
	excel.SetColumnWidth(6, ExcelColumnWidth::cnConstraintValue);
	excel.SetColumnWidth(7, ExcelColumnWidth::cnSystemNamed);
	excel.SetColumnWidth(8, ExcelColumnWidth::cnDefaultWidth);
}

//***************************************************************************
//
void CDBSynchronizer::AddExcelIndexInfo(Xlnt::CXlntUtil& excel)
{
	int32 currentRow = 1;

	excel.AddSheet("TABLE_INDEX");
	ToHeaderStyle(excel, "A1", "J1");

	excel.WriteCell(currentRow, 1, "SCHEMA_NAME");
	excel.WriteCell(currentRow, 2, "TABLE_NAME");
	excel.WriteCell(currentRow, 3, "INDEX_NAME");
	excel.WriteCell(currentRow, 4, "INDEX_TYPE");
	excel.WriteCell(currentRow, 5, "IS_PRIMARYKEY");
	excel.WriteCell(currentRow, 6, "IS_UNIQUE");
	excel.WriteCell(currentRow, 7, "IS_SYSTEM_NAMED");
	excel.WriteCell(currentRow, 8, "COLUMN_SEQ");
	excel.WriteCell(currentRow, 9, "COLUMN_NAME");
	excel.WriteCell(currentRow, 10, "COLUMN_SORT");

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
		{
			for( DBModel::IndexColumnRef& dbIndexColumn : dbIndex->_columns )
			{
				currentRow++;
				excel.WriteCell(currentRow, 1, dbIndex->_schemaName);
				excel.WriteCell(currentRow, 2, dbIndex->_tableName);
				excel.WriteCell(currentRow, 3, dbIndex->_indexName);
				excel.WriteCell(currentRow, 4, dbIndex->_type);
				excel.WriteCell(currentRow, 5, std::to_string(dbIndex->_primaryKey));
				excel.WriteCell(currentRow, 6, std::to_string(dbIndex->_uniqueKey));
				excel.WriteCell(currentRow, 7, std::to_string(dbIndex->_systemNamed));
				excel.WriteCell(currentRow, 8, dbIndexColumn->_seq);
				excel.WriteCell(currentRow, 9, dbIndexColumn->_columnName);
				excel.WriteCell(currentRow, 10, ToString(dbIndexColumn->_sort));
				excel.SetRowHeight(currentRow, 16);
			}
		}
	}

	excel.SetAllCellTextFormat(3);

	excel.SetColumnWidth(1, ExcelColumnWidth::cnSchemaName);
	excel.SetColumnWidth(2, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(3, ExcelColumnWidth::cnConstraintName);
	excel.SetColumnWidth(4, ExcelColumnWidth::cnIndexType);
	excel.SetColumnWidth(5, ExcelColumnWidth::cnDefaultWidth + 2);
	excel.SetColumnWidth(6, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(7, ExcelColumnWidth::cnSystemNamed);
	excel.SetColumnWidth(8, ExcelColumnWidth::cnSequence);
	excel.SetColumnWidth(9, ExcelColumnWidth::cnColumnName);
	excel.SetColumnWidth(10, ExcelColumnWidth::cnDefaultWidth);
}

//***************************************************************************
//
void CDBSynchronizer::AddExcelForeignKeyInfo(Xlnt::CXlntUtil& excel)
{
	int32 currentRow = 1;

	excel.AddSheet("TABLE_FOREIGNKEY");
	ToHeaderStyle(excel, "A1", "M1");

	excel.WriteCell(currentRow, 1, "SCHEMA_NAME");
	excel.WriteCell(currentRow, 2, "TABLE_NAME");
	excel.WriteCell(currentRow, 3, "FOREIGNKEY_NAME");
	excel.WriteCell(currentRow, 4, "IS_DISABLED");
	excel.WriteCell(currentRow, 5, "IS_NOT_TRUSTED");
	excel.WriteCell(currentRow, 6, "FKEY_TABLE_NAME");
	excel.WriteCell(currentRow, 7, "FKEY_COLUMN_NAME");
	excel.WriteCell(currentRow, 8, "RKEY_SCHEMA_NAME");
	excel.WriteCell(currentRow, 9, "RKEY_TABLE_NAME");
	excel.WriteCell(currentRow, 10, "RKEY_COLUMN_NAME");
	excel.WriteCell(currentRow, 11, "UPDATE_RULE");
	excel.WriteCell(currentRow, 12, "DELETE_RULE");
	excel.WriteCell(currentRow, 13, "IS_SYSTEM_NAMED");

	for( DBModel::TableRef& dbTable : _dbTables )
	{
		for( DBModel::ForeignKeyRef& dbForeignKey : dbTable->_foreignKeys )
		{
			int columnIndex = 0;
			for( DBModel::IndexColumnRef& dbForeignKeyColumn : dbForeignKey->_foreignKeyColumns )
			{
				currentRow++;
				excel.WriteCell(currentRow, 1, dbForeignKey->_schemaName);
				excel.WriteCell(currentRow, 2, dbForeignKey->_tableName);
				excel.WriteCell(currentRow, 3, dbForeignKey->_foreignKeyName);
				excel.WriteCell(currentRow, 4, std::to_string(dbForeignKey->_isDisabled));
				excel.WriteCell(currentRow, 5, std::to_string(dbForeignKey->_isNotTrusted));
				excel.WriteCell(currentRow, 6, dbForeignKey->_foreignKeyTableName);
				excel.WriteCell(currentRow, 7, dbForeignKeyColumn->_columnName);
				excel.WriteCell(currentRow, 8, dbForeignKey->_referenceKeySchemaName);
				excel.WriteCell(currentRow, 9, dbForeignKey->_referenceKeyTableName);
				excel.WriteCell(currentRow, 10, dbForeignKey->_referenceKeyColumns[columnIndex++]->_columnName);
				excel.WriteCell(currentRow, 11, dbForeignKey->_updateRule);
				excel.WriteCell(currentRow, 12, dbForeignKey->_deleteRule);
				excel.WriteCell(currentRow, 13, std::to_string(dbForeignKey->_systemNamed));
				excel.SetRowHeight(currentRow, 16);
			}
		}
	}

	excel.SetAllCellTextFormat(3);

	excel.SetColumnWidth(1, ExcelColumnWidth::cnSchemaName);
	excel.SetColumnWidth(2, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(3, ExcelColumnWidth::cnConstraintName);
	excel.SetColumnWidth(4, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(5, ExcelColumnWidth::cnDefaultWidth + 2);
	excel.SetColumnWidth(6, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(7, ExcelColumnWidth::cnColumnName);
	excel.SetColumnWidth(8, ExcelColumnWidth::cnSchemaName + 4);
	excel.SetColumnWidth(9, ExcelColumnWidth::cnObjectName);
	excel.SetColumnWidth(10, ExcelColumnWidth::cnColumnName);
	excel.SetColumnWidth(11, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(12, ExcelColumnWidth::cnDefaultWidth);
	excel.SetColumnWidth(13, ExcelColumnWidth::cnSystemNamed);
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
		DBModel::TableRef t = MakeShared<DBModel::Table>(_dbClass);
		t->_schemaName = table.GetStringAttr(_T("schemaname"));
		t->_tableName = table.GetStringAttr(_T("name"));
		t->_tableComment = table.GetStringAttr(_T("desc"));
		t->_auto_increment_value = table.GetStringAttr(_T("auto_increment_value"));

		CVector<CXMLNode> columns = table.FindChildren(_T("Column"));
		for( CXMLNode& column : columns )
		{
			DBModel::ColumnRef c = MakeShared<DBModel::Column>(_dbClass);
			c->_tableName = t->_tableName;
			c->_seq = column.GetStringAttr(_T("seq"));
			c->_tableName = column.GetStringAttr(_T("name"));
			c->_columnComment = column.GetStringAttr(_T("desc"));
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
			DBModel::IndexRef i = MakeShared<DBModel::Index>(_dbClass);
			i->_tableName = t->_tableName;
			i->_tableName = index.GetStringAttr(_T("name"));

			const TCHAR* kindStr = index.GetStringAttr(_T("kind"));
			i->_kind = StringToDBIndexKind(kindStr);

			const TCHAR* typeStr = index.GetStringAttr(_T("type"));
			i->_type = typeStr;

			i->_primaryKey = index.FindChild(_T("PrimaryKey")).IsValid();
			i->_uniqueKey = index.FindChild(_T("UniqueKey")).IsValid();
			i->_systemNamed = index.FindChild(_T("SystemNamed")).IsValid();

			CVector<CXMLNode> columns = index.FindChildren(_T("Column"));
			for( CXMLNode& column : columns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_seq = column.GetInt32Attr(_T("seq"), 0);
				c->_columnName = column.GetStringAttr(_T("name"));
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
			DBModel::ForeignKeyRef fk = MakeShared<DBModel::ForeignKey>(_dbClass);
			fk->_tableName = t->_tableName;
			fk->_foreignKeyName = foreignKey.GetStringAttr(_T("name"));
			fk->_updateRule = foreignKey.GetStringAttr(_T("update_rule"));
			fk->_deleteRule = foreignKey.GetStringAttr(_T("delete_rule"));

			CXMLNode foreignKeyTable = foreignKey.FindChild(_T("ForeignKeyTable"));
			fk->_foreignKeyTableName = foreignKeyTable.GetStringAttr(_T("name"));
			CVector<CXMLNode> foreignKeyColumns = foreignKeyTable.FindChildren(_T("Column"));
			for( CXMLNode& foreignKeyColumn : foreignKeyColumns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_columnName = foreignKeyColumn.GetStringAttr(_T("name"));
				fk->_foreignKeyColumns.push_back(c);
			}

			CXMLNode referenceKeyTable = foreignKey.FindChild(_T("ReferenceKeyTable"));
			fk->_referenceKeyTableName = referenceKeyTable.GetStringAttr(_T("name"));
			CVector<CXMLNode> referenceKeyColumns = referenceKeyTable.FindChildren(_T("Column"));
			for( CXMLNode& referenceKeyColumn : referenceKeyColumns )
			{
				DBModel::IndexColumnRef c = MakeShared<DBModel::IndexColumn>();
				c->_columnName = referenceKeyColumn.GetStringAttr(_T("name"));
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
		p->_procName = procedure.GetStringAttr(_T("name"));
		p->_procComment = procedure.GetStringAttr(_T("desc"));
		p->_body = procedure.FindChild(_T("body")).GetStringValue();

		CVector<CXMLNode> params = procedure.FindChildren(_T("Param"));
		for( CXMLNode& param : params )
		{
			DBModel::ProcParamRef procParam = MakeShared<DBModel::ProcParam>();
			procParam->_paramId = param.GetStringAttr(_T("seq"));
			procParam->_paramMode = StringToDBParamMode(param.GetStringAttr(_T("mode")));
			procParam->_paramName = param.GetStringAttr(_T("name"));
			procParam->_paramComment = param.GetStringAttr(_T("desc"));
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
		f->_funcName = function.GetStringAttr(_T("name"));
		f->_funcComment = function.GetStringAttr(_T("desc"));
		f->_body = function.FindChild(_T("body")).GetStringValue();

		CVector<CXMLNode> params = function.FindChildren(_T("Param"));
		for( CXMLNode& param : params )
		{
			DBModel::FuncParamRef funcParam = MakeShared<DBModel::FuncParam>();
			funcParam->_paramId = param.GetStringAttr(_T("seq"));
			funcParam->_paramMode = StringToDBParamMode(param.GetStringAttr(_T("mode")));
			funcParam->_paramName = param.GetStringAttr(_T("name"));
			funcParam->_paramComment = param.GetStringAttr(_T("desc"));
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
		table->append_attribute(doc.allocate_attribute(_T("name"), dbTable->_tableName.c_str()));
		table->append_attribute(doc.allocate_attribute(_T("desc"), dbTable->_tableComment.c_str()));
		table->append_attribute(doc.allocate_attribute(_T("auto_increment_value"), dbTable->_auto_increment_value.c_str()));
		root->append_node(table);

		for( DBModel::ColumnRef& dbColumn : dbTable->_columns )
		{
			_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));
			column->append_attribute(doc.allocate_attribute(_T("seq"), dbColumn->_seq.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("name"), dbColumn->_tableName.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("type"), dbColumn->_datatypedesc.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("notnull"), !dbColumn->_nullable ? _T("true") : _T("false")));
			if( dbColumn->_defaultDefinition.size() > 0 ) column->append_attribute(doc.allocate_attribute(_T("default"), dbColumn->_defaultDefinition.c_str()));
			if( dbColumn->_identity ) column->append_attribute(doc.allocate_attribute(_T("identity"), dbColumn->_identitydesc.c_str()));
			column->append_attribute(doc.allocate_attribute(_T("desc"), dbColumn->_columnComment.c_str()));
			table->append_node(column);
		}

		for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
		{
			_tXmlNodeType* index = doc.allocate_node(rapidxml::node_type::node_element, _T("Index"));
			index->append_attribute(doc.allocate_attribute(_T("name"), dbIndex->_tableName.c_str()));

			index->append_attribute(doc.allocate_attribute(_T("type"), doc.allocate_string(dbIndex->_type.c_str())));

			if( dbIndex->_primaryKey ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("PrimaryKey")));
			if( dbIndex->_uniqueKey ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("UniqueKey")));
			if( dbIndex->_systemNamed ) index->append_node(doc.allocate_node(rapidxml::node_type::node_element, _T("SystemNamed")));

			for( DBModel::IndexColumnRef& dbIndexColumn : dbIndex->_columns )
			{
				_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));

				column->append_attribute(doc.allocate_attribute(_T("seq"), dbIndexColumn->_seq.c_str()));
				column->append_attribute(doc.allocate_attribute(_T("name"), dbIndexColumn->_columnName.c_str()));
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
				column->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKeyColumn->_columnName.c_str()));
				foreignKeyTable->append_node(column);
			}
			referenceKey->append_node(foreignKeyTable);

			_tXmlNodeType* referenceKeyTable = doc.allocate_node(rapidxml::node_type::node_element, _T("ReferenceKeyTable"));
			referenceKeyTable->append_attribute(doc.allocate_attribute(_T("name"), dbForeignKey->_referenceKeyTableName.c_str()));
			for( DBModel::IndexColumnRef& dbReferenceKeyColumn : dbForeignKey->_referenceKeyColumns )
			{
				_tXmlNodeType* column = doc.allocate_node(rapidxml::node_type::node_element, _T("Column"));
				column->append_attribute(doc.allocate_attribute(_T("name"), dbReferenceKeyColumn->_columnName.c_str()));
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
		procedure->append_attribute(doc.allocate_attribute(_T("name"), dbProcedure->_procName.c_str()));
		procedure->append_attribute(doc.allocate_attribute(_T("desc"), dbProcedure->_procComment.c_str()));
		for( DBModel::ProcParamRef& dbProcParam : dbProcedure->_parameters )
		{
			_tXmlNodeType* procParam = doc.allocate_node(rapidxml::node_type::node_element, _T("Param"));
			procParam->append_attribute(doc.allocate_attribute(_T("seq"), dbProcParam->_paramId.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("mode"), doc.allocate_string(ToString(dbProcParam->_paramMode))));
			procParam->append_attribute(doc.allocate_attribute(_T("name"), dbProcParam->_paramName.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("type"), dbProcParam->_datatypedesc.c_str()));
			procParam->append_attribute(doc.allocate_attribute(_T("desc"), dbProcParam->_paramComment.c_str()));
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
		function->append_attribute(doc.allocate_attribute(_T("name"), dbFunction->_funcName.c_str()));
		function->append_attribute(doc.allocate_attribute(_T("desc"), dbFunction->_funcComment.c_str()));
		for( DBModel::FuncParamRef& dbFuncParam : dbFunction->_parameters )
		{
			_tXmlNodeType* funcParam = doc.allocate_node(rapidxml::node_type::node_element, _T("Param"));
			funcParam->append_attribute(doc.allocate_attribute(_T("seq"), dbFuncParam->_paramId.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("mode"), doc.allocate_string(ToString(dbFuncParam->_paramMode))));
			funcParam->append_attribute(doc.allocate_attribute(_T("name"), dbFuncParam->_paramName.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("type"), dbFuncParam->_datatypedesc.c_str()));
			funcParam->append_attribute(doc.allocate_attribute(_T("desc"), dbFuncParam->_paramComment.c_str()));
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
bool CDBSynchronizer::GatherDBTables(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBTableColumns(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBTableConstraints(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBIdentityColumns(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBIndexes(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBIndexOptions(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBForeignKeys(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBDefaultConstraints(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBCheckConstraints(const TCHAR* ptszTableName)
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
bool CDBSynchronizer::GatherDBTrigger(const TCHAR* ptszTableName)
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
			_tstring triggerName = iterTriggerRef->_schemaName + _T(".") + iterTriggerRef->_triggerName;
			iterTriggerRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::TRIGGERS, triggerName.c_str());
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
bool CDBSynchronizer::GatherDBStoredProcedures(const TCHAR* ptszProcName)
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
			_tstring procName = iterProcRef->_schemaName + _T(".") + iterProcRef->_procName;
			iterProcRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::PROCEDURE, procName.c_str());
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
bool CDBSynchronizer::GatherDBStoredProcedureParams(const TCHAR* ptszProcName)
{
	TCHAR   tszDBName[DATABASE_NAME_STRLEN] = { 0, };
	int32	objectId;
	TCHAR	tszSchemaName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszProcName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	int32	paramId;
	int8	paramMode;
	TCHAR	tszParamName[DATABASE_OBJECT_NAME_STRLEN] = { 0, };
	TCHAR	tszDataType[DATABASE_DATATYPEDESC_STRLEN] = { 0, };
	uint64	maxLength;
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
bool CDBSynchronizer::GatherDBFunctions(const TCHAR* ptszFuncName)
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
			_tstring funcName = iterFuncRef->_schemaName + _T(".") + iterFuncRef->_funcName;
			iterFuncRef->_fullBody = dbQuery.MSSQLHelpText(EDBObjectType::FUNCTION, funcName.c_str());
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
bool CDBSynchronizer::GatherDBFunctionParams(const TCHAR* ptszFuncName)
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
	uint64	maxLength;
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
		xmlTableMap[xmlTable->_tableName] = xmlTable;

	// DB에 실존하는 테이블들을 돌면서 XML에 정의된 테이블들과 비교한다.
	for( DBModel::TableRef& dbTable : _dbTables )
	{
		auto findTable = xmlTableMap.find(dbTable->_tableName);
		if( findTable != xmlTableMap.end() )
		{
			DBModel::TableRef xmlTable = findTable->second;
			CompareTables(dbTable, xmlTable);
			xmlTableMap.erase(findTable);
		}
		else
		{
			if( _xmlRemovedTables.find(dbTable->_tableName) != _xmlRemovedTables.end() )
			{
				LOG_INFO(_T("Removing Table : [dbo].[%s]"), dbTable->_tableName.c_str());
				_updateQueries[UpdateStep::DropTable].push_back(dbTable->DropTable());
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
			columnsStr += xmlTable->_columns[i]->CreateColumnText();
		}

		// 테이블 생성
		LOG_INFO(_T("Creating Table : [dbo].[%s]"), xmlTable->_tableName.c_str());
		_updateQueries[UpdateStep::CreateTable].push_back(xmlTable->CreateTable());

		/*
		for( DBModel::ColumnRef& xmlColumn : xmlTable->_columns )
		{
			if( xmlColumn->_defaultDefinition.empty() )
				continue;

			_updateQueries[UpdateStep::DefaultConstraint].push_back(xmlColumn->CreateDefaultConstraint());
		}
		*/

		for( DBModel::IndexRef& xmlIndex : xmlTable->_indexes )
		{
			LOG_INFO(_T("Creating Index : [%s] %s [%s]"), xmlTable->_tableName.c_str(), xmlIndex->GetKeyText().c_str(), xmlIndex->GetIndexName().c_str());
			_updateQueries[UpdateStep::CreateIndex].push_back(xmlIndex->CreateIndex());
		}

		for( DBModel::ForeignKeyRef& xmlForeignKey : xmlTable->_foreignKeys )
		{
			LOG_INFO(_T("Creating ForeignKey : [%s] [%s]"), xmlTable->_tableName.c_str(), xmlForeignKey->GetForeignKeyName().c_str());
			_updateQueries[UpdateStep::CreateForeignKey].push_back(xmlForeignKey->CreateForeignKey());
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
		xmlColumnMap[xmlColumn->_tableName] = xmlColumn;

	// DB에 실존하는 테이블 컬럼들을 돌면서 XML에 정의된 컬럼들과 비교한다.
	for( DBModel::ColumnRef& dbColumn : dbTable->_columns )
	{
		auto findColumn = xmlColumnMap.find(dbColumn->_tableName);
		if( findColumn != xmlColumnMap.end() )
		{
			DBModel::ColumnRef& xmlColumn = findColumn->second;
			CompareColumns(dbTable, dbColumn, xmlColumn);
			xmlColumnMap.erase(findColumn);
		}
		else
		{
			LOG_INFO(_T("Dropping Column : [%s].[%s]"), dbTable->_tableName.c_str(), dbColumn->_tableName.c_str());

			_updateQueries[UpdateStep::DropColumn].push_back(dbColumn->DropColumn());
		}
	}

	// 맵에서 제거되지 않은 XML 컬럼 정의는 새로 추가.
	for( auto& mapIt : xmlColumnMap )
	{
		DBModel::ColumnRef& xmlColumn = mapIt.second;
		DBModel::Column newColumn = *xmlColumn;
		newColumn._nullable = true;

		LOG_INFO(_T("Adding Column : [%s].[%s]"), dbTable->_tableName.c_str(), xmlColumn->_tableName.c_str());
		_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->CreateColumn());

		if( xmlColumn->_nullable == false && xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(tstring_tcformat(_T("SET NOCOUNT ON; UPDATE [dbo].[%s] SET [%s] = %s WHERE [%s] IS NULL"),
				dbTable->_tableName.c_str(), xmlColumn->_tableName.c_str(), xmlColumn->_defaultDefinition.c_str(), xmlColumn->_tableName.c_str()));
		}

		if( xmlColumn->_nullable == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->AlterColumn());
		}

		/*
		if( xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AddColumn].push_back(xmlColumn->CreateDefaultConstraint());
		}
		*/
	}

	// XML에 있는 인덱스 목록을 갖고 온다.
	CMap<_tstring, DBModel::IndexRef> xmlIndexMap;
	for( DBModel::IndexRef& xmlIndex : xmlTable->_indexes )
		xmlIndexMap[xmlIndex->GetIndexName()] = xmlIndex;

	// DB에 실존하는 테이블 인덱스들을 돌면서 XML에 정의된 인덱스들과 비교한다.
	for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
	{
		auto findIndex = xmlIndexMap.find(dbIndex->GetIndexName());
		if( findIndex != xmlIndexMap.end() && _dependentIndexes.find(dbIndex->GetIndexName()) == _dependentIndexes.end() )
		{
			DBModel::IndexRef xmlIndex = findIndex->second;
			xmlIndexMap.erase(findIndex);
		}
		else
		{
			LOG_INFO(_T("Dropping Index : [%s] [%s] %s"), dbTable->_tableName.c_str(), dbIndex->_indexName.c_str(), dbIndex->_type.c_str());
			_updateQueries[UpdateStep::DropIndex].push_back(dbIndex->DropIndex());
		}
	}

	// 맵에서 제거되지 않은 XML 인덱스 정의는 새로 추가.
	for( auto& mapIt : xmlIndexMap )
	{
		DBModel::IndexRef xmlIndex = mapIt.second;
		LOG_INFO(_T("Creating Index : [%s] %s [%s]"), dbTable->_tableName.c_str(), xmlIndex->GetIndexName().c_str(), xmlIndex->_type.c_str());
		_updateQueries[UpdateStep::CreateIndex].push_back(xmlIndex->CreateIndex());
	}

	// XML에 있는 참조키 목록을 갖고 온다.
	CMap<_tstring, DBModel::ForeignKeyRef> xmlForeignKeyMap;
	for( DBModel::ForeignKeyRef& xmlForeignKey : xmlTable->_foreignKeys )
		xmlForeignKeyMap[xmlForeignKey->GetForeignKeyName()] = xmlForeignKey;

	// DB에 실존하는 테이블 참조키들을 돌면서 XML에 정의된 참조키들과 비교한다.
	for( DBModel::ForeignKeyRef& dbForeignKey : dbTable->_foreignKeys )
	{
		auto findReferenceKeys = xmlForeignKeyMap.find(dbForeignKey->GetForeignKeyName());
		if( findReferenceKeys != xmlForeignKeyMap.end() && _dependentReferenceKeys.find(dbForeignKey->GetForeignKeyName()) == _dependentReferenceKeys.end() )
		{
			DBModel::ForeignKeyRef xmlForeignKey = findReferenceKeys->second;
			xmlForeignKeyMap.erase(findReferenceKeys);
		}
		else
		{
			LOG_INFO(_T("Dropping ForeignKey : [%s] [%s]"), dbTable->_tableName.c_str(), dbForeignKey->_foreignKeyName.c_str());
			_updateQueries[UpdateStep::DropForeignKey].push_back(dbForeignKey->DropForeignKey());
		}
	}

	// 맵에서 제거되지 않은 XML 참조키 정의는 새로 추가.
	for( auto& mapIt : xmlForeignKeyMap )
	{
		DBModel::ForeignKeyRef xmlForeignKey = mapIt.second;
		LOG_INFO(_T("Creating ForeignKey : [%s] [%s]"), dbTable->_tableName.c_str(), xmlForeignKey->GetForeignKeyName().c_str());
		_updateQueries[UpdateStep::CreateForeignKey].push_back(xmlForeignKey->CreateForeignKey());
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
		LOG_INFO(_T("Updating Column [%s] : (%s) -> (%s)"), dbTable->_tableName.c_str(), dbColumn->CreateColumnText().c_str(), xmlColumn->CreateColumnText().c_str());
	}

	// 연관된 인덱스가 있으면 나중에 삭제하기 위해 기록한다.
	if( flag & (ColumnFlag::Type | ColumnFlag::Length | ColumnFlag::Nullable) )
	{
		for( DBModel::IndexRef& dbIndex : dbTable->_indexes )
			if( dbIndex->DependsOn(dbColumn->_tableName) )
				_dependentIndexes.insert(dbIndex->GetIndexName());

		flag |= ColumnFlag::Default;
	}

	/*
	if( flag & ColumnFlag::Default )
	{
		if( dbColumn->_defaultConstraintName.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(dbColumn->DropDefaultConstraint());
		}
	}
	*/

	DBModel::Column newColumn = *dbColumn;
	newColumn._defaultDefinition = _T("");
	newColumn._datatype = xmlColumn->_datatype;
	newColumn._maxLength = xmlColumn->_maxLength;
	newColumn._datatypedesc = xmlColumn->_datatypedesc;
	newColumn._seedValue = xmlColumn->_seedValue;
	newColumn._incrementValue = xmlColumn->_incrementValue;

	if( flag & (ColumnFlag::Type | ColumnFlag::Length | ColumnFlag::Identity) )
	{
		_updateQueries[UpdateStep::AlterColumn].push_back(newColumn.AlterColumn());
	}

	newColumn._nullable = xmlColumn->_nullable;
	if( flag & ColumnFlag::Nullable )
	{
		if( xmlColumn->_defaultDefinition.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(tstring_tcformat(
				_T("SET NOCOUNT ON; UPDATE [dbo].[%s] SET [%s] = %s WHERE [%s] IS NULL"),
				dbTable->_tableName.c_str(),
				xmlColumn->_tableName.c_str(),
				xmlColumn->_tableName.c_str(),
				xmlColumn->_tableName.c_str()));
		}

		_updateQueries[UpdateStep::AlterColumn].push_back(newColumn.AlterColumn());
	}

	/*
	if( flag & ColumnFlag::Default )
	{
		if( dbColumn->_defaultConstraintName.empty() == false )
		{
			_updateQueries[UpdateStep::AlterColumn].push_back(dbColumn->CreateDefaultConstraint());
		}
	}
	*/
}

//***************************************************************************
//
void CDBSynchronizer::CompareStoredProcedures()
{
	// XML에 있는 프로시저 목록을 갖고 온다.
	CMap<_tstring, DBModel::ProcedureRef> xmlProceduresMap;
	for( DBModel::ProcedureRef& xmlProcedure : _xmlProcedures )
		xmlProceduresMap[xmlProcedure->_procName] = xmlProcedure;

	// DB에 실존하는 테이블 프로시저들을 돌면서 XML에 정의된 프로시저들과 비교한다.
	for( DBModel::ProcedureRef& dbProcedure : _dbProcedures )
	{
		auto findProcedure = xmlProceduresMap.find(dbProcedure->_procName);
		if( findProcedure != xmlProceduresMap.end() )
		{
			DBModel::ProcedureRef xmlProcedure = findProcedure->second;
			_tstring xmlBody = xmlProcedure->CreateQuery();
			if( DBModel::Helpers::RemoveWhiteSpace(dbProcedure->_fullBody) != DBModel::Helpers::RemoveWhiteSpace(xmlBody) )
			{
				LOG_INFO(_T("Updating Procedure : %s"), dbProcedure->_procName.c_str());
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
		xmlFunctionsMap[xmlFunction->_funcName] = xmlFunction;

	// DB에 실존하는 테이블 프로시저들을 돌면서 XML에 정의된 프로시저들과 비교한다.
	for( DBModel::FunctionRef& dbFunction : _dbFunctions )
	{
		auto findFunction = xmlFunctionsMap.find(dbFunction->_funcName);
		if( findFunction != xmlFunctionsMap.end() )
		{
			DBModel::FunctionRef xmlFunction = findFunction->second;
			_tstring xmlBody = xmlFunction->CreateQuery();
			if( DBModel::Helpers::RemoveWhiteSpace(dbFunction->_fullBody) != DBModel::Helpers::RemoveWhiteSpace(xmlBody) )
			{
				LOG_INFO(_T("Updating Procedure : %s"), dbFunction->_funcName.c_str());
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