
//***************************************************************************
// DBEnum.h : interface for database enum definition.
//
//***************************************************************************

#ifndef __DBENUM_H__
#define __DBENUM_H__

#pragma once

//***************************************************************************
//
enum class EDBClass : unsigned char
{
	NONE = 0,
	MSSQL = 1,
	MYSQL,
	ORACLE
};

inline const TCHAR* ToString(EDBClass v)
{
	switch( v )
	{
		case EDBClass::MSSQL:		return _T("MSSQL");
		case EDBClass::MYSQL:		return _T("MYSQL");
		case EDBClass::ORACLE:		return _T("ORACLE");
		default:					return _T("NONE");
	}
}

inline const EDBClass StringToDBClass(const TCHAR* ptszDBClass)
{
	if( ::_tcsicmp(ptszDBClass, _T("MSSQL")) == 0 )
		return EDBClass::MSSQL;
	else if( ::_tcsicmp(ptszDBClass, _T("MYSQL")) == 0 )
		return EDBClass::MYSQL;
	else if( ::_tcsicmp(ptszDBClass, _T("ORACLE")) == 0 )
		return EDBClass::ORACLE;

	return EDBClass::NONE;
}


//***************************************************************************
//
enum class EDBReturnType : int
{
	INVALID = -1,
	OK = 0,
	TIMEOUT,
	FETCH_NOT_FIND,
};

inline const TCHAR* ToString(EDBReturnType v)
{
	switch( v )
	{
		case EDBReturnType::OK:					return _T("OK");
		case EDBReturnType::TIMEOUT:			return _T("TIMEOUT");
		case EDBReturnType::FETCH_NOT_FIND:		return _T("FETCH_NOT_FIND");
		default:								return _T("INVALID");
	}
}

inline const EDBReturnType StringToDBReturnType(const TCHAR* ptszDBReturnType)
{
	if( ::_tcsicmp(ptszDBReturnType, _T("OK")) == 0 )
		return EDBReturnType::OK;
	else if( ::_tcsicmp(ptszDBReturnType, _T("TIMEOUT")) == 0 )
		return EDBReturnType::TIMEOUT;
	else if( ::_tcsicmp(ptszDBReturnType, _T("FETCH_NOT_FIND")) == 0 )
		return EDBReturnType::FETCH_NOT_FIND;
	else if( ::_tcsicmp(ptszDBReturnType, _T("INVALID")) == 0 )
		return EDBReturnType::INVALID;

	return EDBReturnType::INVALID;
}


//***************************************************************************
//
enum class EDBObjectType : unsigned char
{
	TABLE = 1,
	PROCEDURE,
	FUNCTION,
	TRIGGERS,
	EVENTS,
	COLUMN = 6,
	PARAMETER = 7
};

inline const TCHAR* ToString(EDBObjectType v)
{
	switch( v )
	{
		case EDBObjectType::TABLE:			return _T("TABLE");
		case EDBObjectType::PROCEDURE:		return _T("PROCEDURE");
		case EDBObjectType::FUNCTION:		return _T("FUNCTION");
		case EDBObjectType::TRIGGERS:		return _T("TRIGGERS");
		case EDBObjectType::EVENTS:			return _T("EVENTS");
		case EDBObjectType::COLUMN:			return _T("COLUMN");
		case EDBObjectType::PARAMETER:		return _T("PARAMETER");
		default:							return _T("");
	}
}

inline const EDBObjectType StringToDBObjectType(const TCHAR* ptszDBObjectType)
{
	if( ::_tcsicmp(ptszDBObjectType, _T("TABLE")) == 0 )
		return EDBObjectType::TABLE;
	else if( ::_tcsicmp(ptszDBObjectType, _T("PROCEDURE")) == 0 )
		return EDBObjectType::PROCEDURE;
	else if( ::_tcsicmp(ptszDBObjectType, _T("FUNCTION")) == 0 )
		return EDBObjectType::FUNCTION;
	else if( ::_tcsicmp(ptszDBObjectType, _T("TRIGGERS")) == 0 )
		return EDBObjectType::TRIGGERS;
	else if( ::_tcsicmp(ptszDBObjectType, _T("EVENTS")) == 0 )
		return EDBObjectType::EVENTS;
	else if( ::_tcsicmp(ptszDBObjectType, _T("COLUMN")) == 0 )
		return EDBObjectType::COLUMN;
	else if( ::_tcsicmp(ptszDBObjectType, _T("PARAMETER")) == 0 )
		return EDBObjectType::PARAMETER;

	return EDBObjectType::TABLE;
}


//***************************************************************************
//
enum class EParameterMode
{
	PARAM_RETURN = 0,
	PARAM_IN = 1,
	PARAM_OUT = 2
};

inline const TCHAR* ToString(EParameterMode v)
{
	switch( v )
	{
		case EParameterMode::PARAM_RETURN:	return _T("RET");
		case EParameterMode::PARAM_IN:		return _T("IN");
		case EParameterMode::PARAM_OUT:		return _T("OUT");
		default:							return _T("");
	}
}

inline const EParameterMode StringToDBParamMode(const TCHAR* ptszParamMode)
{
	if( ::_tcsicmp(ptszParamMode, _T("RET")) == 0 )
		return EParameterMode::PARAM_RETURN;
	else if( ::_tcsicmp(ptszParamMode, _T("IN")) == 0 )
		return EParameterMode::PARAM_IN;
	else if( ::_tcsicmp(ptszParamMode, _T("OUT")) == 0 )
		return EParameterMode::PARAM_OUT;

	return EParameterMode::PARAM_IN;
}


//***************************************************************************
//
enum class EIndexKind
{
	NONE = 0,
	CLUSTERED = 1,
	NONCLUSTERED = 2
};

inline const TCHAR* ToString(EIndexKind v)
{
	switch( v )
	{
		case EIndexKind::CLUSTERED:			return _T("CLUSTERED");
		case EIndexKind::NONCLUSTERED:		return _T("NONCLUSTERED");
		default:							return _T("NONE");
	}
}

inline const EIndexKind StringToDBIndexKind(const TCHAR* ptszIndexKind)
{
	if( ::_tcsicmp(ptszIndexKind, _T("CLUSTERED")) == 0 )
		return EIndexKind::CLUSTERED;
	else if( ::_tcsicmp(ptszIndexKind, _T("NONCLUSTERED")) == 0 )
		return EIndexKind::NONCLUSTERED;

	return EIndexKind::NONE;
}


//***************************************************************************
//
enum class EIndexSort
{
	ASC = 1,
	DESC = 2
};

inline const TCHAR* ToString(EIndexSort v)
{
	switch( v )
	{
		case EIndexSort::ASC:			return _T("ASC");
		case EIndexSort::DESC:			return _T("DESC");
		default:						return _T("");
	}
}

inline const EIndexSort StringToDBIndexSort(const TCHAR* ptszIndexSort)
{
	if( ::_tcsicmp(ptszIndexSort, _T("ASC")) == 0 )
		return EIndexSort::ASC;
	else if( ::_tcsicmp(ptszIndexSort, _T("DESC")) == 0 )
		return EIndexSort::DESC;

	return EIndexSort::ASC;
}


//***************************************************************************
//
enum class EDataType
{
	NONE = 0,
	TEXT = 35,
	TINYINT = 48,
	SMALLINT = 52,
	INT = 56,
	REAL = 59,
	DATETIME = 61,
	FLOAT = 62,
	NTEXT = 99,
	BIT = 104,
	DECIMAL = 106,
	NUMERIC = 108,
	BIGINT = 127,
	VARBINARY = 165,
	VARCHAR = 167,
	BINARY = 173,
	CHAR = 175,
	NVARCHAR = 231,
	NCHAR = 239
};

inline const TCHAR* ToString(EDataType v)
{
	switch( v )
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

inline const EDataType StringToDBDataType(const TCHAR* ptszDataType, OUT int64& maxLen)
{
	_tregex reg(_T("([a-z]+)(\\((max|\\d+)\\))?"));
	_tcmatch ret;

	if( std::regex_match(ptszDataType, OUT ret, reg) == false )
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

inline const _tstring DataTypeLengthToString(EDataType type, uint64 uiLength)
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
			return _tstring(ToString(type));
		case EDataType::VARBINARY:
		case EDataType::VARCHAR:
		case EDataType::BINARY:
		case EDataType::CHAR:
		case EDataType::NVARCHAR:
		case EDataType::NCHAR:
		{
			TCHAR tszDataLength[20];
			_stprintf_s(tszDataLength, _countof(tszDataLength), _T("%llu"), uiLength);
			return (_tstring(ToString(type)) + _T("(") + _tstring(tszDataLength) + _T(")"));
		}
	}

	return _T("");
}

#endif // ndef __DBENUM_H__