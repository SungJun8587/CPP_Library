
//***************************************************************************
// ServerConfig.h: interface for the CServerConfig class.
//
//***************************************************************************

#ifndef __SERVERCONFIG_H__
#define __SERVERCONFIG_H__

#ifndef	__SERVERCONNECTINFO_H__
#include <ServerConnectInfo.h>
#endif

class CServerConfig : public CSingleton<CServerConfig>
{
public:
	CServerConfig(void);
	virtual	~CServerConfig(void);

	bool							Init(const TCHAR* tszServerInfo);

	TCHAR*							GetServerName(void)				{ return _tszServerName; }
	TCHAR*							GetServiceName(void)			{ return _tszServiceName; }
	TCHAR*							GetDisplayName(void)			{ return _tszDisplayName; }
	TCHAR*							GetServerIP(void)				{ return _tszIP; }
	uint16&							GetServerPort(void)				{ return _nServerPort; }

	std::vector<CServerNode>&		GetServerNodeVec(void)			{ return _serverNodeVec; }
	std::vector<CDBNode>			GetDBNodeVec(void)				{ return _dbNodeVec; }
	std::vector<CRedisNode>			GetRedisNodeVec(void)			{ return _redisNodeVec; }

	CServerNode&					GetServerNode(int16& nID)		{ return _serverNodeVec[nID-1]; }
	CDBNode&						GetDBNode(int16& nID)			{ return _dbNodeVec[nID-1]; }	
	CRedisNode&						GetRedisNode(int16& nID)		{ return _redisNodeVec[nID-1]; }	

	void							PrintServerSettingInfo();

	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ServiceName"), _tValue(_tszServiceName, allocator), allocator);
		value.AddMember(_T("DisplayName"), _tValue(_tszDisplayName, allocator), allocator);
		value.AddMember(_T("Name"), _tValue(_tszServerName, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszIP, allocator), allocator);
		value.AddMember(_T("Port"), _nServerPort, allocator);
		value.AddMember(_T("KeepAliveSec"), _nKeepAliveSec, allocator);
		value.AddMember(_T("MaxUser"), _nMaxUser, allocator);
	}

	void FromJSON(const _tValue& value)
	{
		_tcsncpy_s(_tszServiceName, _countof(_tszServiceName), value[_T("ServiceName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDisplayName, _countof(_tszDisplayName), value[_T("DisplayName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), value[_T("Name")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), value[_T("IP")].GetString(), _TRUNCATE);
		_nServerPort = value[_T("Port")].GetInt();
		_nKeepAliveSec = value[_T("KeepAliveSec")].GetInt();
		_nMaxUser = value[_T("MaxUser")].GetInt();
	}

protected:
	void						Clear(void);

private:	
	TCHAR						_tszServiceName[MAX_BUFFER_SIZE];
	TCHAR						_tszDisplayName[MAX_BUFFER_SIZE];

	TCHAR						_tszServerName[HOSTNAME_STRLEN];
	TCHAR						_tszIP[HOSTNAME_STRLEN];
	uint16						_nServerPort;

	int32						_nKeepAliveSec;
	int32						_nMaxUser;

	std::vector<CServerNode>	_serverNodeVec;
	std::vector<CDBNode>		_dbNodeVec;
	std::vector<CRedisNode>		_redisNodeVec;
};

#endif // ndef __SERVERCONFIG_H__
