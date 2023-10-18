
//***************************************************************************
// ServerConfig.h: interface for the CServerConfig class.
//
//***************************************************************************

#ifndef __SERVERCONFIG_H__
#define __SERVERCONFIG_H__

#ifndef	__SERVERCONNECTINFO_H__
#include <ServerConnectInfo.h>
#endif

class CServerConfig : public CSingleton<CServerConfig>, public CJSONBase
{
public:
	CServerConfig(void);
	virtual	~CServerConfig(void);

	bool						Init(const TCHAR* tszServerInfo);

	TCHAR*						GetServerName(void)					{ return m_tszServerName; }
	TCHAR*						GetServiceName(void)				{ return m_tszServiceName; }
	TCHAR*						GetDisplayName(void)				{ return m_tszDisplayName; }
	TCHAR*						GetServerIP(void)					{ return m_tszIP; }
	uint16&						GetServerPort(void)					{ return m_nServerPort; }

	CVector<CServerNode>&		GetServerNodeVec(void)				{ return m_ServerNodeVec; }
	CVector<CDBNode>			GetDBNodeVec(void)					{ return m_DBNodeVec; }
	CVector<CRedisNode>			GetRedisNodeVec(void)				{ return m_RedisNodeVec; }

	CServerNode&				GetServerNode(int16& nID)			{ return m_ServerNodeVec[nID-1]; }
	CDBNode&					GetDBNode(int16& nID)				{ return m_DBNodeVec[nID-1]; }	
	CRedisNode&					GetRedisNode(int16& nID)			{ return m_RedisNodeVec[nID-1]; }	

	void						PrintServerSettingInfo();

	virtual bool Deserialize(const _tstring& jsonString)
	{
		_tDocument doc;
		
		doc.Parse(jsonString.c_str());
		if( kParseErrorNone != doc.GetParseError() )
			return false;

		_tValue& obj = doc.Move();

		return DeserializeT(obj);
	};

	virtual bool Deserialize(const _tValue& obj)
	{
		return DeserializeT(obj);
	}

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

	bool DeserializeT(const _tValue& obj)
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
	CVector<CRedisNode>			m_RedisNodeVec;
};

#endif // ndef __SERVERCONFIG_H__
