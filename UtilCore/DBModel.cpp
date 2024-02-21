
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
			query = Helpers::Format(_T("CREATE TABLE [dbo].[%s] (%s)"), _name.c_str(), columnsStr);
			break;
		case EDBClass::MYSQL:
			query = Helpers::Format(_T("CREATE TABLE `%s` (%s) COMMENT = '%s';"), _name.c_str(), columnsStr, _desc);
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
			query = Helpers::Format(_T("DROP TABLE [%s]"), _name);
			break;
		case EDBClass::MYSQL:
			// DROP TABLE [테이블명]
			query = Helpers::Format(_T("DROP TABLE `%s`"), _name);
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
			query = Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD %s"), _tableName, CreateText(dbClass));
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ADD [컬럼명] [데이터타입] [NOT NULL|NULL] [DEFAULT] [AUTO_INCREMENT] [COMMENT]
			query = Helpers::Format(_T("ALTER TABLE `%s` ADD %s COMMENT '%s';"), _tableName, CreateText(dbClass), _desc);
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
			query = Helpers::Format(_T("ALTER TABLE [dbo].[%s] ALTER COLUMN [%s] %s %s"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"));
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] MODIFY [컬럼명] [변경할데이터타입] [NULL|NOT NULL] [COMMENT]
			query = Helpers::Format(_T("ALTER TABLE `%s` MODIFY `%s` %s %s COMMENT '%s'"), _tableName, _name, _datatypedesc, _nullable ? _T("NULL") : _T("NOT NULL"), _desc);
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
			query = Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP COLUMN [%s]"), _tableName, _name);
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] DROP [컬럼명]
			query = Helpers::Format(_T("ALTER TABLE `%s` DROP COLUMN `%s`"), _tableName, _name);
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
				query = Helpers::Format(_T("[%s] %s %s IDENTITY(%d,%d)"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _seedValue, _incrementValue);
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL]
				query = Helpers::Format(_T("[%s] %s %s"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"));
			}
			break;
		case EDBClass::MYSQL:
			if( _identity )
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [AUTO_INCREMENT] [COMMENT]
				query = Helpers::Format(_T("`%s` %s %s AUTO_INCREMENT COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
			}
			else
			{
				// [컬럼명] [데이터타입] [NULL|NOT NULL] [COMMENT]
				query = Helpers::Format(_T("`%s` %s %s COMMENT '%s';"), _name.c_str(), _datatypedesc.c_str(), _nullable ? _T("NULL") : _T("NOT NULL"), _desc.c_str());
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
			query = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [%s] DEFAULT (%s) FOR [%s]"), _tableName.c_str(), _defaultConstraintName.c_str(), _defaultDefinition.c_str(), _name.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] SET DEFAULT `값`
			query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` SET DEFAULT %s;"), _tableName.c_str(), _name.c_str(), _defaultDefinition.c_str());
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
			query = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _defaultConstraintName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ALTER COLUMN [컬럼명] DROP DEFAULT
			query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ALTER COLUMN `%s` DROP DEFAULT;"), _tableName.c_str(), _name.c_str());
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
				query = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] ADD CONSTRAINT [%s] %s %s (%s)"), _tableName.c_str(), GetIndexName(dbClass).c_str(), GetKeyText().c_str(), GetKindText().c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{ 
				// CREATE CLUSTERED|NONCLUSTERED INDEX [인덱스명] ON [테이블명] ([컬럼명] [정렬방식])
				query = DBModel::Helpers::Format(_T("CREATE %s INDEX [%s] ON [dbo].[%s] (%s)"), GetKindText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] ADD `제약조건명` PRIMARY KEY ([컬럼명] [정렬방식])
				query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ADD %s PRIMARY KEY (%s);"), _tableName.c_str(), GetIndexName(dbClass).c_str(), CreateColumnsText(dbClass).c_str());
			}
			else
			{
				// CREATE UNIQUE|INDEX|FULLTEXT|SPATIAL INDEX `제약조건명` ON [테이블명] ([컬럼명] [정렬방식])
				query = DBModel::Helpers::Format(_T("CREATE %s INDEX `%s` ON `%s` (%s);"), GetTypeText().c_str(), GetIndexName(dbClass).c_str(), _tableName.c_str(), CreateColumnsText(dbClass).c_str());
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
				query = DBModel::Helpers::Format(_T("ALTER TABLE [dbo].[%s] DROP CONSTRAINT [%s]"), _tableName, _name);
			}
			else
			{
				// DROP INDEX [인덱스명] ON [테이블명]
				query = DBModel::Helpers::Format(_T("DROP INDEX [%s] ON [dbo].[%s]"), _name, _tableName);
			}
			break;
		case EDBClass::MYSQL:
			if( _primaryKey )
			{
				// ALTER TABLE [테이블명] DROP CONSTRAINT [인덱스명]
				query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName, _name);
			}
			else
			{ 
				// DROP INDEX [인덱스명] ON [테이블명]
				query = DBModel::Helpers::Format(_T("DROP INDEX `%s` ON `%s`;"), _name, _tableName);
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
				query += Helpers::Format(_T("[%s] %s"), _columns[i]->_name.c_str(), _columns[i]->GetSortText());
				break;
			case EDBClass::MYSQL:
				// [컬럼명] 정렬방식
				query += Helpers::Format(_T("`%s` %s"), _columns[i]->_name.c_str(), _columns[i]->_sort == EIndexSort::DESC ? _T("DESC") : _T(""));
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
			query = DBModel::Helpers::Format(_T("ALTER TABLE [%s] ADD CONSTRAINT [%s] FOREIGN KEY (%s) REFERENCES [%s] (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + DBModel::Helpers::Format(_T("\r\nON UPDATE %s\r\nON DELETE %s"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + DBModel::Helpers::Format(_T("\r\nON UPDATE %s"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + DBModel::Helpers::Format(_T("\r\nON DELETE %s"), _deleteRule.c_str());
			}
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] ADD CONSTRAINT `제약조건명` FOREIGN KEY ([컬럼명]) REFERENCES [테이블명] ([컬럼명]) [ON DELETE RESTRICT|CASCADE|SET NULL|NO ACTION]|[ON UPDATE RESTRICT|CASCADE|SET NULL|NO ACTION|SET_DEFAULT]
			query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` ADD CONSTRAINT `%s` FOREIGN KEY (%s) REFERENCES `%s` (%s)"), _tableName.c_str(), _foreignKeyName.c_str(),
										   CreateColumnsText(dbClass, _foreignKeyColumns).c_str(), _referenceKeyTableName.c_str(), CreateColumnsText(dbClass, _referenceKeyColumns).c_str());

			if( _updateRule.size() > 0 && _deleteRule.size() > 0 )
			{
				query = query + DBModel::Helpers::Format(_T(" ON UPDATE %s ON DELETE %s;"), _updateRule.c_str(), _deleteRule.c_str());
			}
			else if( _updateRule.size() > 0 && _deleteRule.size() < 1 )
			{
				query = query + DBModel::Helpers::Format(_T(" ON UPDATE %s;"), _updateRule.c_str());
			}
			else if( _updateRule.size() < 1 && _deleteRule.size() > 0 )
			{
				query = query + DBModel::Helpers::Format(_T(" ON DELETE %s;"), _deleteRule.c_str());
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
			query = DBModel::Helpers::Format(_T("ALTER TABLE [%s] DROP CONSTRAINT [%s]"), _tableName.c_str(), _foreignKeyName.c_str());
			break;
		case EDBClass::MYSQL:
			// ALTER TABLE [테이블명] DROP CONSTRAINT [제약조건명]
			query = DBModel::Helpers::Format(_T("ALTER TABLE `%s` DROP CONSTRAINT `%s`;"), _tableName.c_str(), _foreignKeyName.c_str());
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
				query += Helpers::Format(_T("[%s]"), columns[i]->_name.c_str());
				break;
			case EDBClass::MYSQL:
				// [컬럼명]
				query += Helpers::Format(_T("`%s`"), columns[i]->_name.c_str());
				break;
		}
	}

	return query;
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
_tstring ProcParam::GetParameterModeText()
{
	return ToString(_paramMode);
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
_tstring FuncParam::GetParameterModeText()
{
	return ToString(_paramMode);
}
//***************************************************************************
//
EIndexKind Helpers::StringToIndexKind(const TCHAR* ptszIndexKind)
{
	if( ::_tcsicmp(ptszIndexKind, _T("CLUSTERED")) == 0 )
		return EIndexKind::CLUSTERED;
	else if( ::_tcsicmp(ptszIndexKind, _T("NONCLUSTERED")) == 0 )
		return EIndexKind::NONCLUSTERED;

	return EIndexKind::NONE;
}

//***************************************************************************
//
EIndexType Helpers::StringToIndexType(const TCHAR* ptszIndexType)
{
	if( ::_tcsicmp(ptszIndexType, _T("PRIMARYKEY")) == 0 )
		return EIndexType::PRIMARYKEY;
	else if( ::_tcsicmp(ptszIndexType, _T("UNIQUE")) == 0 )
		return EIndexType::UNIQUE;
	else if( ::_tcsicmp(ptszIndexType, _T("INDEX")) == 0 )
		return EIndexType::INDEX;
	else if( ::_tcsicmp(ptszIndexType, _T("FULLTEXT")) == 0 )
		return EIndexType::FULLTEXT;
	else if( ::_tcsicmp(ptszIndexType, _T("SPATIAL")) == 0 )
		return EIndexType::SPATIAL;

	return EIndexType::NONE;
}

//***************************************************************************
//
EIndexSort Helpers::StringToIndexSort(const TCHAR* ptszIndexSort)
{
	if( ::_tcsicmp(ptszIndexSort, _T("ASC")) == 0 )
		return EIndexSort::ASC;
	else if( ::_tcsicmp(ptszIndexSort, _T("DESC")) == 0 )
		return EIndexSort::DESC;

	return EIndexSort::ASC;
}

//***************************************************************************
//
EDataType Helpers::StringToDataType(const TCHAR* str, OUT uint64& maxLen)
{
	_tregex reg(_T("([a-z]+)(\\((max|\\d+)\\))?"));
	_tcmatch ret;

	if( std::regex_match(str, OUT ret, reg) == false )
		return EDataType::NONE;

	if( ret[3].matched )
		maxLen = ::_tcsicmp(ret[3].str().c_str(), _T("max")) == 0 ? -1 : _ttoi(ret[3].str().c_str());
	else
		maxLen = 0;

	if( ::_tcsicmp(ret[1].str().c_str(), _T("TEXT")) == 0 ) return EDataType::TEXT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("TINYINT")) == 0 ) return EDataType::TINYINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("SMALLINT")) == 0 ) return EDataType::SMALLINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("INT")) == 0 ) return EDataType::INT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("REAL")) == 0 ) return EDataType::REAL;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("DATETIME")) == 0 ) return EDataType::DATETIME;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("FLOAT")) == 0 ) return EDataType::FLOAT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NTEXT")) == 0 ) return EDataType::NTEXT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BIT")) == 0 ) return EDataType::BIT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("DECIMAL")) == 0 ) return EDataType::DECIMAL;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NUMERIC")) == 0 ) return EDataType::NUMERIC;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BIGINT")) == 0 ) return EDataType::BIGINT;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("VARBINARY")) == 0 ) return EDataType::VARBINARY;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("VARCHAR")) == 0 ) return EDataType::VARCHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("BINARY")) == 0 ) return EDataType::BINARY;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("CHAR")) == 0 ) return EDataType::CHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NVARCHAR")) == 0 ) return EDataType::NVARCHAR;
	if( ::_tcsicmp(ret[1].str().c_str(), _T("NCHAR")) == 0 ) return EDataType::NCHAR;

	return EDataType::NONE;
}

//***************************************************************************
//
_tstring Helpers::DataTypeToString(EDataType type)
{
	switch( type )
	{
		case EDataType::TEXT:		return _T("TEXT");
		case EDataType::TINYINT:	return _T("TINYINT");
		case EDataType::SMALLINT:	return _T("SMALLINT");
		case EDataType::INT:		return _T("INT");
		case EDataType::REAL:		return _T("REAL");
		case EDataType::DATETIME:	return _T("DATETIME");
		case EDataType::FLOAT:		return _T("FLOAT");
		case EDataType::NTEXT:		return _T("NTEXT");
		case EDataType::BIT:		return _T("BIT");
		case EDataType::DECIMAL:	return _T("DECIMAL");
		case EDataType::NUMERIC:	return _T("NUMERIC");
		case EDataType::BIGINT:		return _T("BIGINT");
		case EDataType::VARBINARY:	return _T("VARBINARY");
		case EDataType::VARCHAR:	return _T("VARCHAR");
		case EDataType::BINARY:		return _T("BINARY");
		case EDataType::CHAR:		return _T("CHAR");
		case EDataType::NVARCHAR:	return _T("NVARCHAR");
		case EDataType::NCHAR:		return _T("NCHAR");
		default:					return _T("NONE");
	}
}

//***************************************************************************
//
_tstring Helpers::DataTypeLengthToString(EDataType type, uint64 iLength)
{
	switch( type )
	{
		case EDataType::TEXT:
		case EDataType::TINYINT:		
		case EDataType::SMALLINT:	
		case EDataType::INT:			
		case EDataType::REAL:		
		case EDataType::DATETIME:	
		case EDataType::FLOAT:	
		case EDataType::NTEXT:
		case EDataType::BIT:	
		case EDataType::DECIMAL:
		case EDataType::NUMERIC:		
		case EDataType::BIGINT:	
			return DataTypeToString(type);
		case EDataType::VARBINARY:
		case EDataType::VARCHAR:
		case EDataType::BINARY:
		case EDataType::CHAR:
		case EDataType::NVARCHAR:
		case EDataType::NCHAR:
			return (DataTypeToString(type) + _T("(") + Format(_T("%d"), iLength) + _T(")"));
	}

	return _T("");
}

//***************************************************************************
//
EParameterMode Helpers::StringToParamMode(const _tstring str)
{
	if( ::_tcsicmp(str.c_str(), _T("RET")) == 0 ) return EParameterMode::PARAM_RETURN;
	if( ::_tcsicmp(str.c_str(), _T("IN")) == 0 ) return EParameterMode::PARAM_IN;
	if( ::_tcsicmp(str.c_str(), _T("OUT")) == 0 ) return EParameterMode::PARAM_OUT;

	return EParameterMode::PARAM_IN;
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


