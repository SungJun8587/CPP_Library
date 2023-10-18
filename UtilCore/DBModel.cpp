
//***************************************************************************
// DBModel.cpp: implementation of the Database Model.
//
//***************************************************************************

#include "pch.h"
#include "DBModel.h"

using namespace DBModel;

//***************************************************************************
//
_tstring Table::CreateTableDesc(DB_CLASS dbClass)
{
	int32 i = 0;
	_tstring sql;

	if( dbClass == DB_CLASS::DB_MSSQL )
	{
		sql = _T("CREATE TABLE [dbo].[") + _name + _T("](");

		// Column
		for( DBModel::ColumnRef& columnRef : _columns )
		{
			if( i > 0 ) sql = sql + _T(",");
			_tstring nullable = columnRef->_nullable ? _T("NULL") : _T("NOT NULL");
			if( columnRef->_identity )
				sql = sql + _T("\r\n\t") + DBModel::Helpers::Format(_T("[%s] %s %s IDENTITY(%d, %d)"), columnRef->_name.c_str(), columnRef->_datatypedesc.c_str(), nullable.c_str(), columnRef->_seedValue, columnRef->_incrementValue);
			else sql = sql + _T("\r\n\t") + DBModel::Helpers::Format(_T("[%s] %s %s"), columnRef->_name.c_str(), columnRef->_datatypedesc.c_str(), nullable.c_str());
			i++;
		}

		// PrimaryKey, UniqueKey
		for( DBModel::IndexRef& indexRef : _indexes )
		{
			if( indexRef->_primaryKey || indexRef->_uniqueKey )
			{
				sql = sql + _T(",\r\n") + DBModel::Helpers::Format(_T(" CONSTRAINT [%s] %s %s\r\n("), indexRef->_name.c_str(),
																   indexRef->_primaryKey ? _T("PRIMARY KEY") : (indexRef->_uniqueKey ? _T("UNIQUE") : _T("")),
																   indexRef->_kind == DBModel::IndexKind::Clustered ? _T("CLUSTERED") : _T("NONCLUSTERED"));
				i = 0;
				for( DBModel::IndexColumnRef& indexColumnRef : indexRef->_columns )
				{
					if( i > 0 ) sql = sql + _T(",");
					sql = sql + _T("\r\n\t") + DBModel::Helpers::Format(_T("[%s] %s"), indexColumnRef->_name.c_str(), indexColumnRef->_sort == 1 ? _T("ASC") : _T("DESC"));
					i++;
				}
				sql = sql + _T("\r\n") + _T(")WITH (PAD_INDEX = OFF, STATISTICS_NORECOMPUTE = OFF, IGNORE_DUP_KEY = OFF, ALLOW_ROW_LOCKS = ON, ALLOW_PAGE_LOCKS = ON) ON[PRIMARY]");
			}
		}
		sql = sql + _T("\r\n") + _T(") ON [PRIMARY]");
		sql = sql + _T("\r\n\r\n") + _T("GO") + _T("\r\n\r\n");

		// Default Constraint
		for( DBModel::ColumnRef& defaultColumn : _columns )
		{
			if( !defaultColumn->_default.empty() )
			{
				sql = sql + DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [DF_%s_%s] DEFAULT (%s) FOR [%s]"),
													 defaultColumn->_name.c_str(), defaultColumn->_name.c_str(),
													 defaultColumn->_name.c_str(), defaultColumn->_default.c_str(), defaultColumn->_name.c_str());
			}
		}
		sql = sql + _T("\r\n") + _T("GO") + _T("\r\n\r\n");

		// ForeignKey
		for( DBModel::ForeignKeyRef& foreignKeyRef : _foreignKeys )
		{
			sql = sql + _T("ALTER TABLE [dbo].[") + foreignKeyRef->_foreignKeyTableName + _T("] WITH CHECK ADD CONSTRAINT ") + foreignKeyRef->_foreignKeyName;
			sql = sql + _T(" FOREIGN KEY (");
			i = 0;
			for( DBModel::IndexColumnRef& foreignKeyColumnRef : foreignKeyRef->_foreignKeyColumns )
			{
				if( i > 0 ) sql += _T(", ");
				sql = sql + DBModel::Helpers::Format(_T("[%s]"), foreignKeyColumnRef->_name.c_str());
				i++;
			}
			sql = sql + _T(")") + _T("\r\n");
			sql = sql + _T("REFERENCES [dbo].[") + foreignKeyRef->_referenceKeyTableName + _T("] (");
			i = 0;
			for( DBModel::IndexColumnRef& referenceKeyColumnRef : foreignKeyRef->_referenceKeyColumns )
			{
				if( i > 0 ) sql += _T(", ");
				sql = sql + DBModel::Helpers::Format(_T("[%s]"), referenceKeyColumnRef->_name.c_str());
				i++;
			}
			sql = sql + _T(")");

			if( foreignKeyRef->_updateRule.size() > 0 && foreignKeyRef->_deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON UPDATE %s\r\nON DELETE %s"), foreignKeyRef->_updateRule.c_str(), foreignKeyRef->_deleteRule.c_str());
			}
			else if( foreignKeyRef->_updateRule.size() > 0 && foreignKeyRef->_deleteRule.size() < 1 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON UPDATE %s"), foreignKeyRef->_updateRule.c_str());
			}
			else if( foreignKeyRef->_updateRule.size() < 1 && foreignKeyRef->_deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON DELETE %s"), foreignKeyRef->_deleteRule.c_str());
			}
			
			sql = sql + _T("\r\n") + _T("GO") + _T("\r\n\r\n");
			sql = sql + _T("ALTER TABLE [dbo].[") + foreignKeyRef->_foreignKeyTableName + _T("] CHECK CONSTRAINT [") + foreignKeyRef->_foreignKeyName + _T("]");
			sql = sql + _T("\r\n") + _T("GO");
		}
	}
	else if( dbClass == DB_CLASS::DB_MYSQL )
	{
		sql = _T("CREATE TABLE ") + _name + _T("(");

		// Column
		i = 0;
		for( DBModel::ColumnRef& columnRef : _columns )
		{
			if( i > 0 ) sql = sql + _T(",");
			_tstring nullable = columnRef->_nullable ? _T("NULL") : _T("NOT NULL");
			if( columnRef->_identity )
				sql = sql + _T("\r\n\t") + DBModel::Helpers::Format(_T("`%s` %s %s AUTO_INCREMENT COMMENT '%s'"), columnRef->_name.c_str(), columnRef->_datatypedesc.c_str(), nullable.c_str(), columnRef->_desc.c_str());
			else sql = sql + _T("\r\n\t") + DBModel::Helpers::Format(_T("`%s` %s %s COMMENT '%s'"), columnRef->_name.c_str(), columnRef->_datatypedesc.c_str(), nullable.c_str(), columnRef->_desc.c_str());
			i++;
		}

		// PrimaryKey, UniqueKey, Index
		for( DBModel::IndexRef& indexRef : _indexes )
		{
			sql = sql + _T(",\r\n\t");
			_tstring keyClass = (indexRef->_primaryKey ? _T("PRIMARY KEY") : (indexRef->_uniqueKey ? _T("UNIQUE KEY") : _T("KEY")));

			if( indexRef->_primaryKey )
				sql = sql + DBModel::Helpers::Format(_T("%s ("), keyClass.c_str());
			else sql = sql + DBModel::Helpers::Format(_T("%s `%s` ("), keyClass.c_str(), indexRef->_name.c_str());

			i = 0;
			for( DBModel::IndexColumnRef& indexColumnRef : indexRef->_columns )
			{
				if( i > 0 ) sql = sql + _T(",");
				if( indexColumnRef->_sort == 2 )
					sql = sql + DBModel::Helpers::Format(_T("`%s` %s"), indexColumnRef->_name.c_str(), _T("DESC"));
				else sql = sql + DBModel::Helpers::Format(_T("`%s`"), indexColumnRef->_name.c_str());
				i++;
			}
			sql = sql + _T(")");
		}

		// ForeignKey
		for( DBModel::ForeignKeyRef& foreignKeyRef : _foreignKeys )
		{
			sql = sql + _T(",\r\n\t");
			sql = sql + _T("CONSTRAINT `") + foreignKeyRef->_foreignKeyName + _T("` FOREIGN KEY (");

			i = 0;
			for( DBModel::IndexColumnRef& foreignKeyColumnRef : foreignKeyRef->_foreignKeyColumns )
			{
				if( i > 0 ) sql += _T(", ");
				sql = sql + DBModel::Helpers::Format(_T("`%s`"), foreignKeyColumnRef->_name.c_str());
				i++;
			}
			sql = sql + _T(")") + _T(" REFERENCES `") + foreignKeyRef->_referenceKeyTableName + _T("` (");
			i = 0;
			for( DBModel::IndexColumnRef& referenceKeyColumnRef : foreignKeyRef->_referenceKeyColumns )
			{
				if( i > 0 ) sql += _T(", ");
				sql = sql + DBModel::Helpers::Format(_T("`%s`"), referenceKeyColumnRef->_name.c_str());
				i++;
			}
			sql = sql + _T(")");

			if( foreignKeyRef->_updateRule.size() > 0 && foreignKeyRef->_deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON UPDATE %s ON DELETE %s"), foreignKeyRef->_updateRule.c_str(), foreignKeyRef->_deleteRule.c_str());
			}
			else if( foreignKeyRef->_updateRule.size() > 0 && foreignKeyRef->_deleteRule.size() < 1 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON UPDATE %s"), foreignKeyRef->_updateRule.c_str());
			}
			else if( foreignKeyRef->_updateRule.size() < 1 && foreignKeyRef->_deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON DELETE %s"), foreignKeyRef->_deleteRule.c_str());
			}
		}

		if( ::_ttoi64(_auto_increment_value.c_str()) > 0 )
			sql = sql + DBModel::Helpers::Format(_T("\r\n) ENGINE=InnoDB AUTO_INCREMENT=%s DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;"), _auto_increment_value.c_str());
		else sql = sql + _T("\r\n) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;");
	}

	return sql;
}

//***************************************************************************
//
_tstring Table::CreateTable(DB_CLASS dbClass)
{
	_tstring sql;

	_tstring columnsStr;
	const int32 size = static_cast<int32>(_columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i != 0 )
			columnsStr += _T(",");
		columnsStr += _T("\n\t");
		columnsStr += _columns[i]->CreateText(dbClass);
	}

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			sql = Helpers::Format(_T("CREATE TABLE [dbo].[%s] (%s)"), _name.c_str(), columnsStr);
			break;
		case DB_CLASS::DB_MYSQL:
			sql = Helpers::Format(_T("CREATE TABLE `%s` (%s) COMMENT = '%s';"), _name.c_str(), columnsStr, _desc);
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Table::DropTable(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// DROP TABLE [테이블명]
			sql = Helpers::Format(_T("DROP TABLE [%s]"), _name);
			break;
		case DB_CLASS::DB_MYSQL:
			// DROP TABLE [테이블명]
			sql = Helpers::Format(_T("DROP TABLE `%`"), _name);
			break;
	}

	return sql;
}

//***************************************************************************
//
ColumnRef Table::FindColumn(const _tstring& columnName)
{
	auto findIt = std::find_if(_columns.begin(), _columns.end(),
							   [&](const ColumnRef& column) { return column->_name == columnName; });

	if( findIt != _columns.end() )
		return *findIt;

	return nullptr;
}

//***************************************************************************
//
_tstring Column::CreateColumn(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] [데이터타입] [NOT NULL|NULL] [DEFAULT]
			// 신규 컬럼 추가시 DEFAULT 지정 방법
			//		- ALTER TABLE [테이블명 ADD [컬럼명] [데이터타입] [NOT NULL|NULL] DEFAULT '값'
			sql = Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD %s"), _tableName, CreateText(dbClass));
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] [데이터타입] [NOT NULL|NULL] [DEFAULT] [AUTO_INCREMENT] [COMMENT]
			sql = Helpers::Format(_T("ALTER TABLE `%s` ADD %s COMMENT '%s';"), _tableName, CreateText(dbClass), _desc);
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Column::ModifyColumnType(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] [변경할데이터타입] [NULL|NOT NULL]
			sql = Helpers::Format(_T("ALTER TABLE [dbo].[%s] ALTER COLUMN [%s] %s %s"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"));
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] MODIFY [컬럼명] [변경할데이터타입] [NULL|NOT NULL] [COMMENT]
			sql = Helpers::Format(_T("ALTER TABLE `%s` MODIFY `%s` %s %s COMMENT '%s'"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"), _desc);
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Column::DropColumn(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			sql = Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP COLUMN [%s]"), _tableName, _name);
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			sql = Helpers::Format(_T("ALTER TABLE `%s` DROP COLUMN `%s`"), _tableName, _name);
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Column::CreateText(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			if( _identity )
			{ 
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [IDENTITY(%d, %d)]
				sql = Helpers::Format(_T("[%s] %s %s IDENTITY(%d, %d)"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _seedValue, _incrementValue);
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL]
				sql = Helpers::Format(_T("[%s] %s %s"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"));
			}
			break;
		case DB_CLASS::DB_MYSQL:
			if( _identity )
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [AUTO_INCREMENT] [COMMENT]
				sql = Helpers::Format(_T("`%s` %s %s AUTO_INCREMENT COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [COMMENT]
				sql = Helpers::Format(_T("`%s` %s %s COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
			}
			break;
	}

	return sql;
}

//***************************************************************************
// DB_MSSQL : 기존 default 값을 변경하고자 한다면, 기존 default 제약 조건을 삭제(DropDefaultConstraint 함수 실행)하고, 새로운 제약 조건을 추가(CreateDefaultConstraint 함수 실행)
// DB_MYSQL : 기존 default 값을 변경하고자 한다면, CreateDefaultConstraint() 함수 사용
//
_tstring Column::CreateDefaultConstraint(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] DEFAULT ([값]) FOR [컬럼명]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [DF_%s_%s] DEFAULT (%s) FOR [%s]"), _tableName.c_str(), _tableName.c_str(), _name.c_str(), _default.c_str(), _name.c_str());
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] SET DEFAULT `값`
			sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` SET DEFAULT %s;"), _tableName.c_str(), _name.c_str(), _default.c_str());
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Column::DropDefaultConstraint(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _defaultConstraintName.c_str());
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] DROP DEFAULT
			sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` DROP DEFAULT;"), _tableName.c_str(), _name.c_str());
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Index::GetIndexName(DB_CLASS dbClass)
{
	_tstring uniqueName;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// [인덱스명]
			//	- [PK|UQ|IX]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			uniqueName = _T("IX_");
			if( _primaryKey ) uniqueName = _T("PK_");
			else
			{
				if( _uniqueKey ) uniqueName = _T("UQ_");
			}
			uniqueName = uniqueName + (_kind == IndexKind::Clustered ? _T("C_") : _T("NC_"));
			uniqueName = uniqueName + _tableName;
			for( const IndexColumnRef& column : _columns )
			{
				uniqueName += _T("_");
				uniqueName += column->_name;
			}
			break;
		case DB_CLASS::DB_MYSQL:
			// [인덱스명]
			//	- PRIMARY : PRIMARY
			//	- 인덱스 : [UQ|IX|FT|ST]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			if( _primaryKey ) uniqueName = _T("PRIMARY");
			else
			{
				uniqueName = _T("IX_");
				switch( _type )
				{
					case IndexType::Unique:
						uniqueName = _T("UQ_");
						break;
					case IndexType::Index:
						uniqueName = _T("IX_");
						break;
					case IndexType::Fulltext:
						uniqueName = _T("FT_");
						break;
					case IndexType::Spatial:
						uniqueName = _T("ST_");
						break;
				}

				uniqueName = uniqueName + (_kind == IndexKind::Clustered ? _T("C_") : _T("NC_"));
				uniqueName = uniqueName + _tableName;
				for( const IndexColumnRef& column : _columns )
				{
					uniqueName += _T("_");
					uniqueName += column->_name;
				}
			}
			break;
	}

	return uniqueName;
}

//***************************************************************************
//
_tstring Index::CreateIndex(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			if( _primaryKey || _uniqueKey )
			{
				// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] PRIMARY KEY|UNIQUE CLUSTERED|NONCLUSTERED ([컬럼명] [정렬방식])
				sql = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [%s] %s %s (%s)"), _tableName.c_str(), GetIndexName(dbClass).c_str(), GetKeyText().c_str(), GetKindText().c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{ 
				// CREATE CLUSTERED|NONCLUSTERED INDEX [인덱스명] ON [테이블명] ([컬럼명] [정렬방식])
				sql = DBModel::Helpers::Format(_T("CREATE %s INDEX [%s] ON [dbo].[%s] (%s)"), GetKindText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
			}
			break;
		case DB_CLASS::DB_MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] ADD `제약조건명` PRIMARY KEY ([컬럼명] [정렬방식])
				sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ADD %s PRIMARY KEY (%s);"), _tableName.c_str(), GetIndexName(dbClass).c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{
				// CREATE UNIQUE|INDEX|FULLTEXT|SPATIAL INDEX `제약조건명` ON [테이블명] ([컬럼명] [정렬방식])
				sql = DBModel::Helpers::Format(_T("CREATE %s INDEX `%s` ON `%s` (%s);"), GetTypeText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
			}

			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Index::DropIndex(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			if( _primaryKey || _uniqueKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [인덱스명]
				sql = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName, _name);
			}
			else
			{
				// DROP INDEX [인덱스명] ON [테이블명]
				sql = DBModel::Helpers::Format(_T("DROP INDEX [%s] ON [dbo].[%s]"), _name, _tableName);
			}
			break;
		case DB_CLASS::DB_MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [인덱스명]
				sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName, _name);
			}
			else
			{ 
				// DROP INDEX [인덱스명] ON [테이블명]
				sql = DBModel::Helpers::Format(_T("DROP INDEX `%s` ON `%s`;"), _name, _tableName);
			}
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring Index::GetKindText()
{
	return (_kind == IndexKind::Clustered ? _T("CLUSTERED") : _T("NONCLUSTERED"));
}

//***************************************************************************
//
_tstring Index::GetTypeText()
{
	switch( _type )
	{
		case IndexType::PrimaryKey:
			return _T("PRIMARY KEY");
			break;
		case IndexType::Unique:
			return _T("UNIQUE");
			break;
		case IndexType::Index:
			return _T("INDEX");
			break;
		case IndexType::Fulltext:
			return _T("FULLTEXT");
			break;
		case IndexType::Spatial:
			return _T("SPATIAL");
			break;
	}

	return _T("");
}

//***************************************************************************
//
_tstring Index::GetKeyText()
{
	return (_primaryKey ? _T("PRIMARY KEY") : (_uniqueKey ? _T("UNIQUE") : _T("")));
}

//***************************************************************************
//
_tstring Index::CreateColumnsText(DB_CLASS dbClass)
{
	_tstring sql;

	const int32 size = static_cast<int32>(_columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			sql += _T(", ");

		switch( dbClass )
		{
			case DB_CLASS::DB_MSSQL:
				// [컬럼명] 정렬방식
				sql += Helpers::Format(_T("[%s] %s"), _columns[i]->_name.c_str(), _columns[i]->_sort == 2 ? _T("DESC") : _T("ASC"));
				break;
			case DB_CLASS::DB_MYSQL:
				// [컬럼명] 정렬방식
				sql += Helpers::Format(_T("`%s` %s"), _columns[i]->_name.c_str(), _columns[i]->_sort == 2 ? _T("DESC") : _T(""));
				break;
		}
	}

	return sql;
}

//***************************************************************************
//
bool Index::DependsOn(const _tstring& columnName)
{
	auto findIt = std::find_if(_columns.begin(), _columns.end(),
							   [&](const IndexColumnRef& column) { return column->_name == columnName; });

	return findIt != _columns.end();
}

//***************************************************************************
//
_tstring ForeignKey::CreateForeignKey(DB_CLASS dbClass)
{
	_tstring sql;

	// [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
	//	- RESTRICT : 참조하는 테이블에 데이터가 남아 있으면, 참조되는 테이블의 데이터를 삭제하거나 수정할 수 없습니다.
	//	- CASCADE : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블에서도 삭제와 수정이 같이 이루어집니다.
	//	- SET NULL : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 NULL로 변경됩니다.
	//	- NO ACTION : 참조되는 테이블에서 데이터를 삭제하거나 수정해도, 참조하는 테이블의 데이터는 변경되지 않습니다.
	//	- SET DEFAULT : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 기본값으로 변경됩니다.
	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE [%s] ADD CONSTRAINT [%s] FOREIGN KEY (%s) REFERENCES [%s] (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON UPDATE %s\r\nON DELETE %s"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON UPDATE %s"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T("\r\nON DELETE %s"), _deleteRule.c_str());
			}
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT `제약조건명` FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ADD CONSTRAINT `%s` FOREIGN KEY (%s) REFERENCES `%s` (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON UPDATE %s ON DELETE %s;"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON UPDATE %s;"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				sql = sql + DBModel::Helpers::Format(_T(" ON DELETE %s;"), _deleteRule.c_str());
			}
			else
			{
				sql = sql + _T(";");
			}
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring ForeignKey::DropForeignKey(DB_CLASS dbClass)
{
	_tstring sql;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE [%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _foreignKeyName.c_str());
			break;
		case DB_CLASS::DB_MYSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			sql = DBModel::Helpers::Format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName.c_str(), _foreignKeyName.c_str());
			break;
	}

	return sql;
}

//***************************************************************************
//
_tstring ForeignKey::GetForeignKeyName(DB_CLASS dbClass)
{
	_tstring uniqueName;

	switch( dbClass )
	{
		// [외래키명]
		//	- FK_[외래키테이블명]_[참조키테이블명]_[외래키컬럼명1]_[외래키컬럼명2]...
		case DB_CLASS::DB_MSSQL:
		case DB_CLASS::DB_MYSQL:
			uniqueName = _T("FK_") + _foreignKeyTableName + _T("_") + _referenceKeyTableName;
			for( const IndexColumnRef& column : _foreignKeyColumns )
			{
				uniqueName += _T("_");
				uniqueName += column->_name;
			}
			break;
	}

	return uniqueName;
}

//***************************************************************************
//
_tstring ForeignKey::CreateColumnsText(DB_CLASS dbClass, CVector<IndexColumnRef> columns)
{
	_tstring sql;

	const int32 size = static_cast<int32>(columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			sql += _T(", ");

		switch( dbClass )
		{
			case DB_CLASS::DB_MSSQL:
				// [컬럼명] 정렬방식
				sql += Helpers::Format(_T("[%s] %s"), columns[i]->_name.c_str(), columns[i]->_sort == 2 ? _T("DESC") : _T("ASC"));
				break;
			case DB_CLASS::DB_MYSQL:
				// [컬럼명] 정렬방식
				sql += Helpers::Format(_T("`%s` %s"), columns[i]->_name.c_str(), columns[i]->_sort == 2 ? _T("DESC") : _T(""));
				break;
		}
	}

	return sql;
}

//***************************************************************************
//
_tstring Procedure::CreateQuery()
{
	return Helpers::Format(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Procedure::DropQuery()
{
	return Helpers::Format(_T("DROP PROCEDURE IF EXISTS %s"), _name.c_str());
}

//***************************************************************************
//
_tstring Function::CreateQuery()
{
	return Helpers::Format(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Function::DropQuery()
{
	return Helpers::Format(_T("DROP FUNCTION IF EXISTS %s"), _name.c_str());
}

//***************************************************************************
//
IndexKind Helpers::StringToIndexKind(const TCHAR* ptszIndexKind)
{
	if( ::_tcsicmp(ptszIndexKind, _T("clustered")) == 0 )
		return IndexKind::Clustered;
	else if( ::_tcsicmp(ptszIndexKind, _T("nonclustered")) == 0 )
		return IndexKind::NonClustered;

	return IndexKind::None;
}

//***************************************************************************
//
IndexType Helpers::StringToIndexType(const TCHAR* ptszIndexType)
{
	if( ::_tcsicmp(ptszIndexType, _T("PRIMARYKEY")) == 0 )
		return IndexType::PrimaryKey;
	else if( ::_tcsicmp(ptszIndexType, _T("UNIQUE")) == 0 )
		return IndexType::Unique;
	else if( ::_tcsicmp(ptszIndexType, _T("INDEX")) == 0 )
		return IndexType::Index;
	else if( ::_tcsicmp(ptszIndexType, _T("FULLTEXT")) == 0 )
		return IndexType::Fulltext;
	else if( ::_tcsicmp(ptszIndexType, _T("SPATIAL")) == 0 )
		return IndexType::Spatial;

	return IndexType::None;
}

//***************************************************************************
//
_tstring Helpers::Format(const TCHAR* format, ...)
{
	TCHAR buf[4096];

	va_list ap;
	va_start(ap, format);
	::_vstprintf_s(buf, 4096, format, ap);
	va_end(ap);

	return _tstring(buf);
}

//***************************************************************************
//
_tstring Helpers::DataType2String(DataType type)
{
	switch( type )
	{
		case DataType::TEXT:		return _T("TEXT");
		case DataType::TINYINT:		return _T("TINYINT");
		case DataType::SMALLINT:	return _T("SMALLINT");
		case DataType::INT:			return _T("INT");
		case DataType::REAL:		return _T("REAL");
		case DataType::DATETIME:	return _T("DATETIME");
		case DataType::FLOAT:		return _T("FLOAT");
		case DataType::NTEXT:		return _T("NTEXT");
		case DataType::BIT:			return _T("BIT");
		case DataType::DECIMAL:		return _T("DECIMAL");
		case DataType::NUMERIC:		return _T("NUMERIC");
		case DataType::BIGINT:		return _T("BIGINT");
		case DataType::VARBINARY:	return _T("VARBINARY");
		case DataType::VARCHAR:		return _T("VARCHAR");
		case DataType::BINARY:		return _T("BINARY");
		case DataType::CHAR:		return _T("CHAR");
		case DataType::NVARCHAR:	return _T("NVARCHAR");
		case DataType::NCHAR:		return _T("NCHAR");
		default:					return _T("NONE");
	}
}

//***************************************************************************
//
_tstring Helpers::RemoveWhiteSpace(const _tstring& str)
{
	_tstring ret = str;

	ret.erase(
		std::remove_if(ret.begin(), ret.end(), [=](TCHAR ch) { return ::isspace(ch); }),
		ret.end());

	return ret;
}

//***************************************************************************
//
DataType Helpers::StringToDataType(const TCHAR* str, OUT int32& maxLen)
{
	_tregex reg(_T("([a-z]+)(\\((max|\\d+)\\))?"));
	_tcmatch ret;

	if( std::regex_match(str, OUT ret, reg) == false )
		return DataType::NONE;

	if( ret[3].matched )
		maxLen = ::_tcsicmp(ret[3].str().c_str(), _T("max")) == 0 ? -1 : _ttoi(ret[3].str().c_str());
	else
		maxLen = 0;

	if( ::_tcsicmp(ret[1].str().c_str(), _T("TEXT")) == 0 ) return DataType::TEXT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("TINYINT")) == 0 ) return DataType::TINYINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("SMALLINT")) == 0 ) return DataType::SMALLINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("INT")) == 0 ) return DataType::INT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("REAL")) == 0 ) return DataType::REAL;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("DATETIME")) == 0 ) return DataType::DATETIME;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("FLOAT")) == 0 ) return DataType::FLOAT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NTEXT")) == 0 ) return DataType::NTEXT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BIT")) == 0 ) return DataType::BIT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("DECIMAL")) == 0 ) return DataType::DECIMAL;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NUMERIC")) == 0 ) return DataType::NUMERIC;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BIGINT")) == 0 ) return DataType::BIGINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("VARBINARY")) == 0 ) return DataType::VARBINARY;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("VARCHAR")) == 0 ) return DataType::VARCHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BINARY")) == 0 ) return DataType::BINARY;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("CHAR")) == 0 ) return DataType::CHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NVARCHAR")) == 0 ) return DataType::NVARCHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NCHAR")) == 0 ) return DataType::NCHAR;

	return DataType::NONE;
}

//***************************************************************************
//
_tstring Helpers::StringToDataTypeLength(DataType type, int32 iLength)
{
	switch( type )
	{
		case DataType::TEXT:
		case DataType::TINYINT:		
		case DataType::SMALLINT:	
		case DataType::INT:			
		case DataType::REAL:		
		case DataType::DATETIME:	
		case DataType::FLOAT:	
		case DataType::NTEXT:
		case DataType::BIT:	
		case DataType::DECIMAL:
		case DataType::NUMERIC:		
		case DataType::BIGINT:	
			return DataType2String(type);
		case DataType::VARBINARY:
		case DataType::VARCHAR:
		case DataType::BINARY:
		case DataType::CHAR:
		case DataType::NVARCHAR:
		case DataType::NCHAR:
			return (DataType2String(type) + _T("(") + Format(_T("%d"), iLength) + _T(")"));
	}

	return _T("");
}

//***************************************************************************
//
ParameterMode Helpers::StringToParamMode(const _tstring str)
{
	if( ::_tcsicmp(str.c_str(), _T("RET")) == 0 ) return ParameterMode::PARAM_RETURN;
	if( ::_tcsicmp(str.c_str(), _T("IN")) == 0 ) return ParameterMode::PARAM_IN;
	if( ::_tcsicmp(str.c_str(), _T("OUT")) == 0 ) return ParameterMode::PARAM_OUT;

	return ParameterMode::PARAM_IN;
}

//***************************************************************************
//
void Helpers::LogFileWrite(DB_CLASS dbClass, _tstring title, _tstring sql, bool newline)
{
	_tstring message;

	switch( dbClass )
	{
		case DB_CLASS::DB_MSSQL:
			message = (newline ? _T("\n\n--") : _T("--")) + title;
			break;
		case DB_CLASS::DB_MYSQL:
			message = (newline ? _T("\n\n#") : _T("#")) + title;
			break;
		case DB_CLASS::DB_ORACLE:
			break;
	}

	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, message.c_str());
	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, sql.c_str());
}


