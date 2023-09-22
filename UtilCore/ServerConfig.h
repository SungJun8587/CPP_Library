
//***************************************************************************
// ServerConfig.h: interface for the CServerConfig class.
//
//***************************************************************************

#ifndef __SERVERCONFIG_H__
#define __SERVERCONFIG_H__

class CServerNode
{
public:
	CServerNode(void)
		: m_nID(0), m_nPort(0)
	{
		_tmemset(m_tszServerName, 0, HOSTNAME_STRLEN);
		_tmemset(m_tszIP, 0, HOSTNAME_STRLEN);
	}

	virtual BOOL Deserialize(const _tValue& obj)
	{
		m_nID = obj[_T("ID")].GetInt();
		m_nPort = obj[_T("Port")].GetInt();
		_tcsncpy_s(m_tszServerName, _countof(m_tszServerName), obj[_T("ServerName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszIP, _countof(m_tszIP), obj[_T("IP")].GetString(), _TRUNCATE);

		return true;
	};

	virtual BOOL Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual BOOL Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
	};

private:
	template< typename WriterType >
	BOOL SerializeT(WriterType* writer) const
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
		: m_nID(0), m_nPort(0)
	{
		_tmemset(m_tszDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
		_tmemset(m_tszDBName, 0, DATABASE_NAME_STRLEN);
		_tmemset(m_tszDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(m_tszDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
		_tmemset(m_tszDSN, 0, DATABASE_DSN_STRLEN);
	}

	void Init(DB_CLASS dbClass, const TCHAR* ptszDBHost, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName, const unsigned int nPort)
	{
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), ptszDBHost, _TRUNCATE);
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), ptszDBUserId, _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), ptszDBPasswd, _TRUNCATE);
		_tcsncpy_s(m_tszDBName, _countof(m_tszDBName), ptszDBName, _TRUNCATE);
		m_nPort = nPort;

		switch( dbClass )
		{
			case DB_MSSQL:
				_sntprintf_s(m_tszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={SQL Server};SERVER=%s,%u;Database=%s;UID=%s;PWD=%s"),
							 m_tszDBHost, m_nPort, m_tszDBName, m_tszDBUserId, m_tszDBPasswd);
				break;
			case DB_MYSQL:
#ifdef _UNICODE	
				_sntprintf_s(m_tszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={MySQL ODBC 8.1 UNICODE Driver};SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;"),
							 m_tszDBHost, m_nPort, m_tszDBName, m_tszDBUserId, m_tszDBPasswd);
#else
				_sntprintf_s(m_tszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER={MySQL ODBC 8.1 ANSI Driver};SERVER=%s,Port=%u;Database=%s;Uid=%s;Pwd=%s;MULTI_HOST=1;"),
							 m_tszDBHost, m_nPort, m_tszDBName, m_tszDBUserId, m_tszDBPasswd);
#endif
				break;
			case DB_ORACLE:
				_sntprintf_s(m_tszDSN, DATABASE_DSN_STRLEN, _TRUNCATE, _T("DRIVER=Microsoft ODBC for Oracle};SERVER=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=%s)(PORT=%u))(CONNECT_DATA=(SID=%s)));Uid=%s;Pwd=%s"),
							 m_tszDBHost, m_nPort, m_tszDBName, m_tszDBUserId, m_tszDBPasswd);
		}
	}

	virtual BOOL Deserialize(const _tValue& obj)
	{
		m_nID = obj[_T("ID")].GetInt();
		m_nPort = obj[_T("Port")].GetInt();
		_tcsncpy_s(m_tszDBHost, _countof(m_tszDBHost), obj[_T("DBHost")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBName, _countof(m_tszDBName), obj[_T("DBName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBUserId, _countof(m_tszDBUserId), obj[_T("DBUserId")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDBPasswd, _countof(m_tszDBPasswd), obj[_T("DBPasswd")].GetString(), _TRUNCATE);

		return true;
	};

	virtual BOOL Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual BOOL Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
	};

private:
	template< typename WriterType >
	BOOL SerializeT(WriterType* writer) const
	{
		writer->StartObject();

		writer->String(_T("ID"));
		writer->Int(m_nID);

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

	TCHAR			m_tszDSN[DATABASE_DSN_STRLEN];
	TCHAR			m_tszDBHost[DATABASE_SERVER_NAME_STRLEN];
	uint16			m_nPort;
	TCHAR			m_tszDBName[DATABASE_NAME_STRLEN];
	TCHAR			m_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			m_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
};

class CServerConfig : public CSingleton<CServerConfig>, public CJSONBase
{
public:
	CServerConfig(void);
	virtual	~CServerConfig(void);

	BOOL						Init(const TCHAR* tszServerInfo);

	TCHAR*						GetServerName(void)					{ return m_tszServerName; }
	TCHAR*						GetServiceName(void)				{ return m_tszServiceName; }
	TCHAR*						GetDisplayName(void)				{ return m_tszDisplayName; }
	TCHAR*						GetServerIP(void)					{ return m_tszIP; }
	uint16&						GetServerPort(void)					{ return m_nServerPort; }

	CVector<CServerNode>&		GetServerNodeVec(void)				{ return m_ServerNodeVec; }
	CVector<CDBNode>			GetDBNodeVec(void)					{ return m_DBNodeVec; }

	CServerNode&				GetServerNode(int16& nID)			{ return m_ServerNodeVec[nID-1]; }
	CDBNode&					GetDBNode(int16& nID)				{ return m_DBNodeVec[nID-1]; }	

	void						PrintServerSettingInfo();

	virtual BOOL Deserialize(const _tstring& jsonString)
	{
		_tDocument doc;
		
		doc.Parse(jsonString.c_str());
		if( kParseErrorNone != doc.GetParseError() )
			return false;

		_tValue& obj = doc.Move();

		return DeserializeT(obj);
	};

	virtual BOOL Deserialize(const _tValue& obj)
	{
		return DeserializeT(obj);
	}

	virtual BOOL Serialize(_tWriter* writer) const
	{
		return SerializeT(writer);
	};

	virtual BOOL Serialize(_tPrettyWriter* writer) const
	{
		return SerializeT(writer);
	};

private:
	template< typename WriterType >
	BOOL SerializeT(WriterType* writer) const
	{
		writer->StartObject();

		writer->String(_T("ServiceName"));
		writer->String(m_tszServiceName);

		writer->String(_T("DisplayName"));
		writer->String(m_tszDisplayName);

		writer->String(_T("Name"));
		writer->String(m_tszServerName);

		writer->String(_T("IP"));
		writer->String(m_tszIP);

		writer->String(_T("Port"));
		writer->Int(m_nServerPort);

		writer->String(_T("KeepAliveSec"));
		writer->Int(m_nKeepAliveSec);

		writer->String(_T("MaxUser"));
		writer->Int(m_nMaxUser);

		writer->String(_T("DBNode"));
		writer->StartArray();
		for( CVector<CDBNode>::const_iterator it = m_DBNodeVec.begin(); it != m_DBNodeVec.end(); it++ )
		{
			(*it).Serialize(writer);
		}
		writer->EndArray();

		writer->String(_T("ServerNode"));
		writer->StartArray();
		for( CVector<CServerNode>::const_iterator it = m_ServerNodeVec.begin(); it != m_ServerNodeVec.end(); it++ )
		{
			(*it).Serialize(writer);
		}
		writer->EndArray();

		writer->EndObject();

		return true;
	}

	BOOL DeserializeT(const _tValue& obj)
	{
		_tcsncpy_s(m_tszServiceName, _countof(m_tszServiceName), obj[_T("ServiceName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszDisplayName, _countof(m_tszDisplayName), obj[_T("DisplayName")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszServerName, _countof(m_tszServerName), obj[_T("Name")].GetString(), _TRUNCATE);
		_tcsncpy_s(m_tszIP, _countof(m_tszIP), obj[_T("IP")].GetString(), _TRUNCATE);
		m_nServerPort = obj[_T("Port")].GetInt();
		m_nKeepAliveSec = obj[_T("KeepAliveSec")].GetInt();
		m_nMaxUser = obj[_T("MaxUser")].GetInt();

		if( !obj[_T("DBNode")].IsArray() ) return false;

		m_DBNodeVec.clear();

		const _tValue& arrDBNode = obj[_T("DBNode")].GetArray();
		for( _tValue::ConstValueIterator itr = arrDBNode.Begin(); itr != arrDBNode.End(); ++itr )
		{
			CDBNode DBNode;
			DBNode.Deserialize(*itr);
			m_DBNodeVec.push_back(DBNode);
		}

		if( !obj[_T("ServerNode")].IsArray() ) return false;

		m_ServerNodeVec.clear();

		const _tValue& arrServerNode = obj[_T("ServerNode")].GetArray();
		for( _tValue::ConstValueIterator itr = arrServerNode.Begin(); itr != arrServerNode.End(); ++itr )
		{
			CServerNode ServerNode;
			ServerNode.Deserialize(*itr);
			m_ServerNodeVec.push_back(ServerNode);
		}

		return true;
	}

protected:
	void						Clear(void);

private:	
	TCHAR						m_tszServiceName[MAX_BUFFER_SIZE];
	TCHAR						m_tszDisplayName[MAX_BUFFER_SIZE];

	TCHAR						m_tszServerName[HOSTNAME_STRLEN];
	TCHAR						m_tszIP[HOSTNAME_STRLEN];
	uint16						m_nServerPort;

	int32						m_nKeepAliveSec;
	int32						m_nMaxUser;

	CVector<CServerNode>		m_ServerNodeVec;
	CVector<CDBNode>			m_DBNodeVec;
};

#endif // ndef __SERVERCONFIG_H__
