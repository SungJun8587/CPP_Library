
//***************************************************************************
// ServerConfig.cpp: implementation of the CServerConfig class.
//
//***************************************************************************

#include "pch.h"
#include "ServerConfig.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CServerConfig::CServerConfig(void)
	: _nServerPort(0), _nMaxUser(0), _nKeepAliveSec(0)
{
	memset(_tszServerName, 0x00, sizeof(_tszServerName));
	memset(_tszIP, 0x00, sizeof(_tszIP));
	memset(_tszServiceName, 0x00, sizeof(_tszServiceName));
	memset(_tszDisplayName, 0x00, sizeof(_tszDisplayName));

	Clear();
}

CServerConfig::~CServerConfig(void)
{
	Clear();
}

//***************************************************************************
//
bool CServerConfig::Init(const TCHAR* tszServerInfo)
{
	int32 nSize = 0;

	CRapidJSONUtil jsonUtil;
	jsonUtil.LoadFromFile(tszServerInfo);

	_tcsncpy_s(_tszServerName, _countof(_tszServerName), jsonUtil[_T("Name")], _TRUNCATE);
	_tcsncpy_s(_tszIP, _countof(_tszIP), jsonUtil[_T("IP")], _TRUNCATE);
	_tcsncpy_s(_tszServiceName, _countof(_tszServiceName), jsonUtil[_T("ServiceName")], _TRUNCATE);
	_tcsncpy_s(_tszDisplayName, _countof(_tszDisplayName), jsonUtil[_T("DisplayName")], _TRUNCATE);
	_nServerPort = jsonUtil[_T("Port")];
	_nMaxUser = jsonUtil[_T("MaxUser")];
	_nKeepAliveSec = jsonUtil[_T("KeepAliveSec")];

	_serverNodeVec = jsonUtil.Deserialize<std::vector<CServerNode>>(_T("ServerNode"));
	_dbNodeVec = jsonUtil.Deserialize<std::vector<CDBNode>>(_T("DBNode"));
	_redisNodeVec = jsonUtil.Deserialize<std::vector<CRedisNode>>(_T("RedisNode"));

	return true;
}

//***************************************************************************
//
void CServerConfig::Clear(void)
{
	_serverNodeVec.clear();
	_dbNodeVec.clear();
	_redisNodeVec.clear();
}

//***************************************************************************
//
void CServerConfig::PrintServerSettingInfo()
{
	LOG_INFO(_T("###################################################################"));
	LOG_INFO(_T("--------------- [Start Print : Server Setting Info] ---------------"));
	LOG_INFO(_T("Name : %s"), _tszServerName);
	LOG_INFO(_T("IP : %s"), _tszIP);
	LOG_INFO(_T("Port : %d"), _nServerPort);
	LOG_INFO(_T("KeepAliveSec : %d"), _nKeepAliveSec);
	LOG_INFO(_T("MaxUser : %d"), _nMaxUser);

	LOG_INFO(_T("--------------- Connect ServerNode size : %d ---------------"), static_cast<int>(_serverNodeVec.size()));
	for( uint32 i = 0; i < _serverNodeVec.size(); i++ )
	{
		LOG_INFO(_T("ID : %d"), _serverNodeVec[i]._nID);
		LOG_INFO(_T("ServerName : %s"), _serverNodeVec[i]._tszServerName);
		LOG_INFO(_T("IP : %s"), _serverNodeVec[i]._tszIP);
		LOG_INFO(_T("Port : %d"), _serverNodeVec[i]._nPort);
		LOG_INFO(_T("------------------------------"));
	}

	LOG_INFO(_T("--------------- Connect DBNode size : %d ---------------"), static_cast<int>(_dbNodeVec.size()));
	for( uint32 i = 0; i < _dbNodeVec.size(); i++ )
	{
		LOG_INFO(_T("ID : %d"), _dbNodeVec[i]._nID);
		LOG_INFO(_T("DSNDriver : %s"), _dbNodeVec[i]._tszDSNDriver);
		LOG_INFO(_T("DBHost : %s"), _dbNodeVec[i]._tszDBHost);
		LOG_INFO(_T("Port : %d"), _dbNodeVec[i]._nPort);
		LOG_INFO(_T("DBName : %s"), _dbNodeVec[i]._tszDBName);
		LOG_INFO(_T("DBUserId : %s"), _dbNodeVec[i]._tszDBUserId);
		LOG_INFO(_T("DBPasswd : %s"), _dbNodeVec[i]._tszDBPasswd);
		LOG_INFO(_T("------------------------------"));
	}

	LOG_INFO(_T("--------------- Connect RedisNode size : %d ---------------"), static_cast<int>(_redisNodeVec.size()));
	for( uint32 i = 0; i < _redisNodeVec.size(); i++ )
	{
		LOG_INFO(_T("ID : %d"), _redisNodeVec[i]._nID);
		LOG_INFO(_T("DBHost : %s"), _redisNodeVec[i]._tszDBHost);
		LOG_INFO(_T("Port : %d"), _redisNodeVec[i]._nPort);
		LOG_INFO(_T("DBUserId : %s"), _redisNodeVec[i]._tszDBUserId);
		LOG_INFO(_T("DBPasswd : %s"), _redisNodeVec[i]._tszDBPasswd);
		LOG_INFO(_T("DBIndex : %d"), _redisNodeVec[i]._nDbIndex);
		LOG_INFO(_T("------------------------------"));
	}

	LOG_INFO(_T("--------------- [End Print] ---------------"));
	LOG_INFO(_T("###################################################################"));
}