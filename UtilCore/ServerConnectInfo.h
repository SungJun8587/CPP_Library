
//***************************************************************************
// ServerConnectInfo.h: Implementation for Server Connection Information Management.
//
//***************************************************************************

#ifndef __SERVERCONNECTINFO_H__
#define __SERVERCONNECTINFO_H__

#ifndef	__COMMONUTIL_H__
#include <CommonUtil.h>
#endif

class CServerNode
{
public:
	CServerNode(void)
		: _nID(0), _nPort(0)
	{
		_tmemset(_tszServerName, 0, HOSTNAME_STRLEN);
		_tmemset(_tszIP, 0, HOSTNAME_STRLEN);
	}

	void Init(const TCHAR* ptszServerName, const unsigned int nPort, const TCHAR* ptszIP)
	{
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), ptszServerName, _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), ptszIP, _TRUNCATE);
		_nPort = nPort;
	}

	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("Name"), _tValue(_tszServerName, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszIP, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
	}

	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), value[_T("Name")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
	};

public:
	int16			_nID;
	TCHAR			_tszServerName[HOSTNAME_STRLEN];
	TCHAR			_tszIP[HOSTNAME_STRLEN];
	uint16			_nPort;
};

class CDBNode
{
public:
	CDBNode(void)
		: _nID(0), _dbClass(EDBClass::NONE), _nPort(0)
	{
		_tmemset(_tszDSN, 0, DATABASE_DSN_STRLEN);
		_tmemset(_tszDSNDriver, 0, DATABASE_DSN_DRIVER_STRLEN);
		_tmemset(_tszDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
		_tmemset(_tszDBName, 0, DATABASE_NAME_STRLEN);
		_tmemset(_tszDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(_tszDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	}

	void Init(EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
	{
		_dbClass = dbClass;
		_tcsncpy_s(_tszDSNDriver, _countof(_tszDSNDriver), ptszDSNDriver, _TRUNCATE);
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), ptszDBHost, _TRUNCATE);
		_nPort = nPort;
		_tcsncpy_s(_tszDBName, _countof(_tszDBName), ptszDBName, _TRUNCATE);
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), ptszDBUserId, _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), ptszDBPasswd, _TRUNCATE);

		GetDBDSNString(_tszDSN, _dbClass, _tszDSNDriver, _tszDBHost, _nPort, _tszDBUserId, _tszDBPasswd, _tszDBName);
	}

	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("DBClass"), static_cast<int>(_dbClass), allocator);
		value.AddMember(_T("DSNDriver"), _tValue(_tszDSNDriver, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszDBHost, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
		value.AddMember(_T("DBName"), _tValue(_tszDBName, allocator), allocator);
		value.AddMember(_T("UID"), _tValue(_tszDBUserId, allocator), allocator);
		value.AddMember(_T("PWD"), _tValue(_tszDBPasswd, allocator), allocator);
	}

	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_dbClass = ConvertDBClassToInt(value[_T("DBClass")].GetInt());
		_tcsncpy_s(_tszDSNDriver, _countof(_tszDSNDriver), value[_T("DSNDriver")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
		_tcsncpy_s(_tszDBName, _countof(_tszDBName), value[_T("DBName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), value[_T("UID")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), value[_T("PWD")].GetString(), _TRUNCATE);

		GetDBDSNString(_tszDSN, _dbClass, _tszDSNDriver, _tszDBHost, _nPort, _tszDBUserId, _tszDBPasswd, _tszDBName);
	};

	EDBClass ConvertDBClassToInt(int dbClass)
	{
		switch( dbClass )
		{
			case 1:
				return EDBClass::MSSQL;
				break;
			case 2:
				return EDBClass::MYSQL;
				break;
			case 3:
				return EDBClass::ORACLE;
				break;
		}

		return EDBClass::NONE;
	}

public:
	int16			_nID;

	EDBClass        _dbClass;
	TCHAR			_tszDSN[DATABASE_DSN_STRLEN];
	TCHAR			_tszDSNDriver[DATABASE_DSN_DRIVER_STRLEN];
	TCHAR			_tszDBHost[DATABASE_SERVER_NAME_STRLEN];
	uint16			_nPort;
	TCHAR			_tszDBName[DATABASE_NAME_STRLEN];
	TCHAR			_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
};

class CRedisNode
{
public:
	CRedisNode(void)
		: _nID(0), _nPort(0), _nDbIndex(0)
	{
		_tmemset(_tszDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
		_tmemset(_tszDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(_tszDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	}

	void Init(const TCHAR* ptszHost, const unsigned int nPort, const TCHAR* ptszUserId, const TCHAR* ptszPasswd, const unsigned int nDbIndex)
	{
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), ptszHost, _TRUNCATE);
		_nPort = nPort;
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), ptszUserId, _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), ptszPasswd, _TRUNCATE);
		_nDbIndex = nDbIndex;
	}

	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("IP"), _tValue(_tszDBHost, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
		value.AddMember(_T("UID"), _tValue(_tszDBUserId, allocator), allocator);
		value.AddMember(_T("PWD"), _tValue(_tszDBPasswd, allocator), allocator);
		value.AddMember(_T("DBIndex"), _nDbIndex, allocator);
	}

	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), value[_T("UID")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), value[_T("PWD")].GetString(), _TRUNCATE);
		_nDbIndex = value[_T("DBIndex")].GetInt();
	};

public:
	int16			_nID;
	TCHAR			_tszDBHost[DATABASE_SERVER_NAME_STRLEN];
	uint16			_nPort;
	TCHAR			_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	uint32			_nDbIndex;
};

#endif // ndef __SERVERCONNECTINFO_H__
