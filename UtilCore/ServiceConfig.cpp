
//***************************************************************************
// ServiceConfig.cpp: implementation of the CServiceConfig class.
//
//***************************************************************************

#include "pch.h"
#include "ServiceConfig.h"

//***************************************************************************
// @brief JSON 설정 파일로부터 파일 서버 구성 정보를 읽어와 초기화합니다.
// @details ServiceName/DisplayName/GroupId/ChannelId/KeepAliveSec/
//          ServerNode/DBNode는 파일 서버 개념에 없으므로 읽지 않는다 —
//          그 필드들은 상속만 받고 기본값 그대로 남는다.
//***************************************************************************
bool CServiceConfig::Init(const TCHAR* tszServerInfo)
{
	CRapidJSONUtil jsonUtil;
	jsonUtil.LoadFromFile(tszServerInfo);

	_tcsncpy_s(_tszServiceName, _countof(_tszServiceName), jsonUtil[_T("ServiceName")], _TRUNCATE);
	_tcsncpy_s(_tszDisplayName, _countof(_tszDisplayName), jsonUtil[_T("DisplayName")], _TRUNCATE);
	_tcsncpy_s(_tszServerName, _countof(_tszServerName), jsonUtil[_T("Name")], _TRUNCATE);
	_nServerGroupId = jsonUtil[_T("GroupId")];
	_nServerChannelId = jsonUtil[_T("ChannelId")];

	_tcsncpy_s(_tszIP, _countof(_tszIP), jsonUtil[_T("IP")], _TRUNCATE);
	_nServerPort = jsonUtil[_T("Port")];
	_nMaxSessionCount = jsonUtil[_T("MaxSessionCount")];
	_nWorkerThreadCnt = jsonUtil[_T("WorkerThreadCnt")];
	_nKeepAliveSec = jsonUtil[_T("KeepAliveSec")];

	_serverNodeVec = jsonUtil.Deserialize<CVector<CServerNode>>(_T("ServerNode"));
	_dbNodeVec = jsonUtil.Deserialize<CVector<CDBNode>>(_T("DBNode"));
	_redisNodeVec = jsonUtil.Deserialize<CVector<CRedisNode>>(_T("RedisNode"));

	return true;
}

//***************************************************************************
// @brief 로드된 설정 정보를 로그로 출력합니다.
//***************************************************************************
void CServiceConfig::PrintServerSettingInfo()
{
	LOG_INFO(_T("###################################################################"));
	LOG_INFO(_T("--------------- [Start Print : Server Setting Info] ---------------"));
	LOG_INFO(_T("ServiceName : %s"), _tszServiceName);
	LOG_INFO(_T("DisplayName : %s"), _tszDisplayName);
	LOG_INFO(_T("ServerName : %s"), _tszServerName);
	LOG_INFO(_T("ServerGroupId : %d"), _nServerGroupId);
	LOG_INFO(_T("ServerChannelId : %d"), _nServerChannelId);
	LOG_INFO(_T("IP : %s"), _tszIP);
	LOG_INFO(_T("Port : %d"), _nServerPort);
	LOG_INFO(_T("KeepAliveSec : %d"), _nKeepAliveSec);
	LOG_INFO(_T("MaxSessionCount : %d"), _nMaxSessionCount);
	LOG_INFO(_T("WorkerThreadCnt : %d"), _nWorkerThreadCnt);

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