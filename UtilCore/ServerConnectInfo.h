
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
		: m_nID(0), m_nPort(0)
	{
		_tmemset(m_tszServerName, 0, HOSTNAME_STRLEN);
		_tmemset(m_tszIP, 0, HOSTNAME_STRLEN);
	}

	void Init(const TCHAR* ptszServerName, const unsigned int nPort, const TCHAR* ptszIP)
	{
		_tcsncpy_s(m_tszServerName, _countof(m_tszServerName), ptszServerName, _TRUNCATE);
		_tcsncpy_s(m_tszIP, _countof(m_tszIP), ptszIP, _TRUNCATE);
		m_nPort = nPort;
	}

	virtual bool Deserialize(const _tValue& obj)
	{
		m_nID = obj[_T("ID")].GetInt();
		_tcsncpy_s(m_tszServerName, _countof(m_tszServerName), obj[_T("ServerName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszIP, _countof(m_tszIP), obj[_T("IP")].GetString(), _TRUNCATE);
		m_nPort = obj[_T("Port")].GetInt();

		return true;
	};

	virtual bool Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual bool Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
	};

private:
	template< typename WriterType >
	bool SerializeT(WriterType* writer) const
	{
		writer->StartObject();

		writer->String(_T("ID"));
		writer->Int(m_nID);

		writer->String(_T("ServerName"));
		writer->String(m_tszServerName);

		writer->String(_T("IP"));
		writer->String(m_tszIP);

		writer->String(_T("Port"));
		writer->Int(m_nPort);

		writer->EndObject();

		return true;
	}

public:
	int16			m_nID;
	TCHAR			m_tszServerName[HOSTNAME_STRLEN];
	TCHAR			m_tszIP[HOSTNAME_STRLEN];
	uint16			m_nPort;
};

class CDBNode
{
public:
	CDBNode(void)
		: m_nID(0), m_dbClass(EDBClass::NONE), m_nPort(0)
	{
		_tmemset(m_tszDSN, 0, DATABASE_DSN_STRLEN);
		_tmemset(m_tszDSNDriver, 0, DATABASE_DSN_DRIVER_STRLEN);
		_tmemset(m_tszDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
		_tmemset(m_tszDBName, 0, DATABASE_NAME_STRLEN);
		_tmemset(m_tszDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(m_tszDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	}

	void Init(EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
	{
		m_dbClass = dbClass;
		_tcsncpy_s(m_tszDSNDriver, _countof(m_tszDSNDriver), ptszDSNDriver, _TRUNCATE);
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), ptszDBHost, _TRUNCATE);
		m_nPort = nPort;
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), ptszDBUserId, _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), ptszDBPasswd, _TRUNCATE);
		_tcsncpy_s(m_tszDBName, _countof(m_tszDBName), ptszDBName, _TRUNCATE);

		GetDBDSNString(m_tszDSN, m_dbClass, m_tszDSNDriver, m_tszDBHost, m_nPort, m_tszDBUserId, m_tszDBPasswd, m_tszDBName);
	}

	virtual bool Deserialize(const _tValue& obj)
	{
		m_nID = obj[_T("ID")].GetInt();
		m_dbClass = ConvertDBClassToInt(obj[_T("DBClass")].GetInt());
		m_nPort = obj[_T("Port")].GetInt();
		_tcsncpy_s(m_tszDSNDriver, _countof(m_tszDSNDriver), obj[_T("DSNDriver")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), obj[_T("DBHost")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBName, _countof(m_tszDBName), obj[_T("DBName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), obj[_T("DBUserId")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), obj[_T("DBPasswd")].GetString(), _TRUNCATE);

		GetDBDSNString(m_tszDSN, m_dbClass, m_tszDSNDriver, m_tszDBHost, m_nPort, m_tszDBUserId, m_tszDBPasswd, m_tszDBName);

		return true;
	};

	virtual bool Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual bool Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
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

private:
	template< typename WriterType >
	bool SerializeT(WriterType* writer) const
	{
		writer->StartObject();

		writer->String(_T("ID"));
		writer->Int(m_nID);

		writer->String(_T("DBClass"));
		writer->Int(static_cast<int>(m_dbClass));

		writer->String(_T("DSNDriver"));
		writer->String(m_tszDSNDriver);

		writer->String(_T("DBHost"));
		writer->String(m_tszDBHost);

		writer->String(_T("Port"));
		writer->Int(m_nPort);

		writer->String(_T("DBName"));
		writer->String(m_tszDBName);

		writer->String(_T("DBUserId"));
		writer->String(m_tszDBUserId);

		writer->String(_T("DBPasswd"));
		writer->String(m_tszDBPasswd);

		writer->EndObject();

		return true;
	}

public:
	int16			m_nID;

	EDBClass        m_dbClass;
	TCHAR			m_tszDSN[DATABASE_DSN_STRLEN];
	TCHAR			m_tszDSNDriver[DATABASE_DSN_DRIVER_STRLEN];
	TCHAR			m_tszDBHost[DATABASE_SERVER_NAME_STRLEN];
	uint16			m_nPort;
	TCHAR			m_tszDBName[DATABASE_NAME_STRLEN];
	TCHAR			m_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			m_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
};

class CRedisNode
{
	CRedisNode(void)
		: m_nID(0), m_nPort(0), m_nDbIndex(0)
	{
		_tmemset(m_tszDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
		_tmemset(m_tszDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(m_tszDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	}

	void Init(const TCHAR* ptszHost, const unsigned int nPort, const TCHAR* ptszUserId, const TCHAR* ptszPasswd, const unsigned int nDbIndex)
	{
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), ptszHost, _TRUNCATE);
		m_nPort = nPort;
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), ptszUserId, _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), ptszPasswd, _TRUNCATE);
		m_nDbIndex = nDbIndex;
	}

	virtual bool Deserialize(const _tValue& obj)
	{
		m_nID = obj[_T("ID")].GetInt();
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), obj[_T("Host")].GetString(), _TRUNCATE);
		m_nPort = obj[_T("Port")].GetInt();
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), obj[_T("UserId")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), obj[_T("Passwd")].GetString(), _TRUNCATE);
		m_nDbIndex = obj[_T("DbIndex")].GetInt();

		return true;
	};

	virtual bool Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual bool Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
	};

private:
	template< typename WriterType >
	bool SerializeT(WriterType* writer) const
	{
		writer->StartObject();

		writer->String(_T("ID"));
		writer->Int(m_nID);

		writer->String(_T("Host"));
		writer->String(m_tszDBHost);

		writer->String(_T("Port"));
		writer->Int(m_nPort);

		writer->String(_T("UserId"));
		writer->String(m_tszDBUserId);

		writer->String(_T("Passwd"));
		writer->String(m_tszDBPasswd);

		writer->String(_T("DbIndex"));
		writer->Int(m_nDbIndex);

		writer->EndObject();

		return true;
	}

public:
	int16			m_nID;
	TCHAR			m_tszDBHost[DATABASE_SERVER_NAME_STRLEN];
	uint16			m_nPort;
	TCHAR			m_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			m_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	uint32			m_nDbIndex;
};

#endif // ndef __SERVERCONNECTINFO_H__
