
//***************************************************************************
// DBModel.cpp: implementation of the Database Model.
//
//***************************************************************************

#include "pch.h"
#include "DBModel.h"

using namespace DBModel;

//***************************************************************************
//
_tstring Table::CreateTable(EDBClass dbClass)
{
	_tstring query;

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
		case EDBClass::MSSQL:
			query = tstring_format(_T("CREATE TABLE [dbo].[%s] (%s)"), _name.c_str(), columnsStr);
			break;
		case EDBClass::MYSQL:
			query = tstring_format(_T("CREATE TABLE `%s` (%s) COMMENT = '%s';"), _name.c_str(), columnsStr, _desc);
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Table::DropTable(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// DROP TABLE [테이블명]
			query = tstring_format(_T("DROP TABLE [%s]"), _name);
			break;
		case EDBClass::MYSQL:
			// DROP TABLE [테이블명]
			query = tstring_format(_T("DROP TABLE `%s`"), _name);
			break;
	}

	return query;
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
_tstring Column::CreateColumn(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] [데이터타입] [NOT NULL|NULL] [DEFAULT]
			// 신규 컬럼 추가시 DEFAULT 지정 방법
			//		- ALTER TABLE [테이블명 ADD [컬럼명] [데이터타입] [NOT NULL|NULL] DEFAULT '값'
			query = tstring_format(_T("ALTER TABLE [dbo].[%s] ADD %s"), _tableName, CreateText(dbClass));
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] [데이터타입] [NOT NULL|NULL] [DEFAULT] [AUTO_INCREMENT] [COMMENT]
			query = tstring_format(_T("ALTER TABLE `%s` ADD %s COMMENT '%s';"), _tableName, CreateText(dbClass), _desc);
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Column::ModifyColumn(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] [변경할데이터타입] [NULL|NOT NULL]
			query = tstring_format(_T("ALTER TABLE [dbo].[%s] ALTER COLUMN [%s] %s %s"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"));
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] MODIFY [컬럼명] [변경할데이터타입] [NULL|NOT NULL] [COMMENT]
			query = tstring_format(_T("ALTER TABLE `%s` MODIFY `%s` %s %s COMMENT '%s'"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"), _desc);
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Column::DropColumn(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			query = tstring_format(_T("ALTER TABLE [dbo].[%s] DROP COLUMN [%s]"), _tableName, _name);
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			query = tstring_format(_T("ALTER TABLE `%s` DROP COLUMN `%s`"), _tableName, _name);
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Column::CreateText(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			if( _identity )
			{ 
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [IDENTITY(%d, %d)]
				query = tstring_format(_T("[%s] %s %s IDENTITY(%d,%d)"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _seedValue, _incrementValue);
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL]
				query = tstring_format(_T("[%s] %s %s"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"));
			}
			break;
		case EDBClass::MYSQL:
			if( _identity )
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [AUTO_INCREMENT] [COMMENT]
				query = tstring_format(_T("`%s` %s %s AUTO_INCREMENT COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [COMMENT]
				query = tstring_format(_T("`%s` %s %s COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
			}
			break;
	}

	return query;
}

//***************************************************************************
// DB_MSSQL : 기존 default 값을 변경하고자 한다면, 기존 default 제약 조건을 삭제(DropDefaultConstraint 함수 실행)하고, 새로운 제약 조건을 추가(CreateDefaultConstraint 함수 실행)
// DB_MYSQL : 기존 default 값을 변경하고자 한다면, CreateDefaultConstraint() 함수 사용
//
_tstring Column::CreateDefaultConstraint(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] DEFAULT ([값]) FOR [컬럼명]
			query = tstring_format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [%s] DEFAULT (%s) FOR [%s]"), _tableName.c_str(), _defaultConstraintName.c_str(), _defaultDefinition.c_str(), _name.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] SET DEFAULT `값`
			query = tstring_format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` SET DEFAULT %s;"), _tableName.c_str(), _name.c_str(), _defaultDefinition.c_str());
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Column::DropDefaultConstraint(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = tstring_format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _defaultConstraintName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] DROP DEFAULT
			query = tstring_format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` DROP DEFAULT;"), _tableName.c_str(), _name.c_str());
			break;
	}

	return query;
}

_tstring IndexColumn::GetSortText()
{
	return ToString(_sort);
}

//***************************************************************************
//
_tstring Index::GetIndexName(EDBClass dbClass)
{
	_tstring uniqueName;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// [인덱스명]
			//	- [PK|UQ|IX]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			uniqueName = _T("IX_");
			if( _primaryKey ) uniqueName = _T("PK_");
			else
			{
				if( _uniqueKey ) uniqueName = _T("UQ_");
			}
			uniqueName = uniqueName + (_kind == EIndexKind::CLUSTERED ? _T("C_") : _T("NC_"));
			uniqueName = uniqueName + _tableName;
			for( const IndexColumnRef& column : _columns )
			{
				uniqueName += _T("_");
				uniqueName += column->_name;
			}
			break;
		case EDBClass::MYSQL:
			// [인덱스명]
			//	- PRIMARY : PRIMARY
			//	- 인덱스 : [UQ|IX|FT|ST]_[C|NC]_[테이블명]_[컬럼명1]_[컬럼명2]...
			if( _primaryKey ) uniqueName = _T("PRIMARY");
			else
			{
				uniqueName = _T("IX_");
				switch( _type )
				{
					case EIndexType::UNIQUE:
						uniqueName = _T("UQ_");
						break;
					case EIndexType::INDEX:
						uniqueName = _T("IX_");
						break;
					case EIndexType::FULLTEXT:
						uniqueName = _T("FT_");
						break;
					case EIndexType::SPATIAL:
						uniqueName = _T("ST_");
						break;
				}

				uniqueName = uniqueName + (_kind == EIndexKind::CLUSTERED ? _T("C_") : _T("NC_"));
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
_tstring Index::CreateIndex(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			if( _primaryKey || _uniqueKey )
			{
				// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] PRIMARY KEY|UNIQUE CLUSTERED|NONCLUSTERED ([컬럼명] [정렬방식])
				query = tstring_format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [%s] %s %s (%s)"), _tableName.c_str(), GetIndexName(dbClass).c_str(), GetKeyText().c_str(), GetKindText().c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{ 
				// CREATE CLUSTERED|NONCLUSTERED INDEX [인덱스명] ON [테이블명] ([컬럼명] [정렬방식])
				query = tstring_format(_T("CREATE %s INDEX [%s] ON [dbo].[%s] (%s)"), GetKindText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] ADD `제약조건명` PRIMARY KEY ([컬럼명] [정렬방식])
				query = tstring_format(_T("ALTER TABLE `%s` ADD %s PRIMARY KEY (%s);"), _tableName.c_str(), GetIndexName(dbClass).c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{
				// CREATE UNIQUE|INDEX|FULLTEXT|SPATIAL INDEX `제약조건명` ON [테이블명] ([컬럼명] [정렬방식])
				query = tstring_format(_T("CREATE %s INDEX `%s` ON `%s` (%s);"), GetTypeText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
			}

			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Index::DropIndex(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			if( _primaryKey || _uniqueKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [인덱스명]
				query = tstring_format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName, _name);
			}
			else
			{
				// DROP INDEX [인덱스명] ON [테이블명]
				query = tstring_format(_T("DROP INDEX [%s] ON [dbo].[%s]"), _name, _tableName);
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [인덱스명]
				query = tstring_format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName, _name);
			}
			else
			{ 
				// DROP INDEX [인덱스명] ON [테이블명]
				query = tstring_format(_T("DROP INDEX `%s` ON `%s`;"), _name, _tableName);
			}
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring Index::GetKindText()
{
	return ToString(_kind);
}

//***************************************************************************
//
_tstring Index::GetTypeText()
{
	return ToString(_type);
}

//***************************************************************************
//
_tstring Index::GetKeyText()
{
	return (_primaryKey ? _T("PRIMARY KEY") : (_uniqueKey ? _T("UNIQUE") : _T("")));
}

//***************************************************************************
//
_tstring Index::CreateColumnsText(EDBClass dbClass)
{
	_tstring query;

	const int32 size = static_cast<int32>(_columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			query += _T(", ");

		switch( dbClass )
		{
			case EDBClass::MSSQL:
				// [컬럼명] 정렬방식
				query += tstring_format(_T("[%s] %s"), _columns[i]->_name.c_str(), _columns[i]->GetSortText());
				break;
			case EDBClass::MYSQL:
				// [컬럼명] 정렬방식
				query += tstring_format(_T("`%s` %s"), _columns[i]->_name.c_str(), _columns[i]->_sort == EIndexSort::DESC ? _T("DESC") : _T(""));
				break;
		}
	}

	return query;
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
_tstring ForeignKey::CreateForeignKey(EDBClass dbClass)
{
	_tstring query;

	// [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
	//	- RESTRICT : 참조하는 테이블에 데이터가 남아 있으면, 참조되는 테이블의 데이터를 삭제하거나 수정할 수 없습니다.
	//	- CASCADE : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블에서도 삭제와 수정이 같이 이루어집니다.
	//	- SET NULL : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 NULL로 변경됩니다.
	//	- NO ACTION : 참조되는 테이블에서 데이터를 삭제하거나 수정해도, 참조하는 테이블의 데이터는 변경되지 않습니다.
	//	- SET DEFAULT : 참조되는 테이블에서 데이터를 삭제하거나 수정하면, 참조하는 테이블의 데이터는 기본값으로 변경됩니다.
	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT [제약조건명] FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
			query = tstring_format(_T("ALTER TABLE [%s] ADD CONSTRAINT [%s] FOREIGN KEY (%s) REFERENCES [%s] (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + tstring_format(_T("\r\nON UPDATE %s\r\nON DELETE %s"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + tstring_format(_T("\r\nON UPDATE %s"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + tstring_format(_T("\r\nON DELETE %s"), _deleteRule.c_str());
			}
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT `제약조건명` FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
			query = tstring_format(_T("ALTER TABLE `%s` ADD CONSTRAINT `%s` FOREIGN KEY (%s) REFERENCES `%s` (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + tstring_format(_T(" ON UPDATE %s ON DELETE %s;"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + tstring_format(_T(" ON UPDATE %s;"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + tstring_format(_T(" ON DELETE %s;"), _deleteRule.c_str());
			}
			else
			{
				query = query + _T(";");
			}
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring ForeignKey::DropForeignKey(EDBClass dbClass)
{
	_tstring query;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = tstring_format(_T("ALTER TABLE [%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _foreignKeyName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = tstring_format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName.c_str(), _foreignKeyName.c_str());
			break;
	}

	return query;
}

//***************************************************************************
//
_tstring ForeignKey::GetForeignKeyName(EDBClass dbClass)
{
	_tstring uniqueName;

	switch( dbClass )
	{
		// [외래키명]
		//	- FK_[외래키테이블명]_[참조키테이블명]_[외래키컬럼명1]_[외래키컬럼명2]...
		case EDBClass::MSSQL:
		case EDBClass::MYSQL:
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
_tstring ForeignKey::CreateColumnsText(EDBClass dbClass, CVector<IndexColumnRef> columns)
{
	_tstring query;

	const int32 size = static_cast<int32>(columns.size());
	for( int32 i = 0; i < size; i++ )
	{
		if( i > 0 )
			query += _T(", ");

		switch( dbClass )
		{
			case EDBClass::MSSQL:
				// [컬럼명]
				query += tstring_format(_T("[%s]"), columns[i]->_name.c_str());
				break;
			case EDBClass::MYSQL:
				// [컬럼명]
				query += tstring_format(_T("`%s`"), columns[i]->_name.c_str());
				break;
		}
	}

	return query;
}

//***************************************************************************
//
_tstring Procedure::CreateQuery()
{
	return tstring_format(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Procedure::DropQuery()
{
	return tstring_format(_T("DROP PROCEDURE IF EXISTS %s"), _name.c_str());
}

//***************************************************************************
//
_tstring Function::CreateQuery()
{
	return tstring_format(_T("%s"), _body.c_str());
}

//***************************************************************************
//
_tstring Function::DropQuery()
{
	return tstring_format(_T("DROP FUNCTION IF EXISTS %s"), _name.c_str());
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
void Helpers::LogFileWrite(EDBClass dbClass, _tstring title, _tstring query, bool newline)
{
	_tstring message;

	if( query.size() < 1 ) return;

	switch( dbClass )
	{
		case EDBClass::MSSQL:
			message = (newline ? _T("\n\n--") : _T("--")) + title;
			break;
		case EDBClass::MYSQL:
			message = (newline ? _T("\n\n#") : _T("#")) + title;
			break;
		case EDBClass::ORACLE:
			break;
	}

	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, message.c_str());
	LOG_WRITE(ELOG_TYPE::LOG_TYPE_INFO, false, query.c_str());
}


