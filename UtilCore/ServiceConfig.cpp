
//***************************************************************************
// ServiceConfig.cpp: implementation of the CServiceConfig class.
//
//***************************************************************************

#include "pch.h"
#include "ServiceConfig.h"
#include <ConfigFile/JSONConfigFieldReader.h>
#include <ConfigFile/JSONConfigFieldWriter.h>

//***************************************************************************
// @brief JSON 설정 파일로부터 서버 구성 정보를 읽어와 초기화합니다.
// @detail 키가 없으면 기존 값을 유지하고, 타입/범위 오류는 모두 모아 로그로 남긴다.
//         ServerNode/DBNode/RedisNode는 각 항목의 필수 필드와 범위를 검사한 뒤 역직렬화한다.
// @param tszServerInfo 설정 파일 경로 (UTF-8)
// @return 파일 열기/문법/타입/범위 오류가 없으면 true
//***************************************************************************
bool CServiceConfig::Init(const TCHAR* tszServerInfo)
{
	CRapidJSONUtil jsonUtil;
	if( !jsonUtil.LoadFromFile(tszServerInfo) )
	{
		LOG_ERROR(_T("[CServiceConfig] load failed. path=%s"), tszServerInfo);
		return false;
	}
	if( !jsonUtil.IsObject() )
	{
		LOG_ERROR(_T("[CServiceConfig] top-level value must be an object. path=%s"), tszServerInfo);
		return false;
	}

	CJSONConfigFieldReader reader(jsonUtil);

	reader.Read(_T("ServiceName"), _tszServiceName);
	reader.Read(_T("DisplayName"), _tszDisplayName);
	reader.Read(_T("Name"), _tszServerName);
	reader.Read(_T("GroupId"), _nServerGroupId);
	reader.Read(_T("ChannelId"), _nServerChannelId);

	reader.Read(_T("IP"), _tszIP);
	reader.Read(_T("Port"), _nServerPort, 1, 65535);
	reader.Read(_T("MaxSessionCount"), _nMaxSessionCount, 1, INT32_MAX);
	reader.Read(_T("WorkerThreadCnt"), _nWorkerThreadCnt, 0, 1024);
	reader.Read(_T("KeepAliveSec"), _nKeepAliveSec, 0, INT32_MAX);

	reader.ReadObjects(_T("ServerNode"), _serverNodeVec, {
		{ _T("ID"),   EFieldType::Int,    0, INT16_MAX },
		{ _T("Name"), EFieldType::String },
		{ _T("IP"),   EFieldType::String },
		{ _T("Port"), EFieldType::Int,    1, 65535 },
		});
	reader.ReadObjects(_T("DBNode"), _dbNodeVec, {
		{ _T("ID"),        EFieldType::Int,    0, INT16_MAX },
		{ _T("DBClass"),   EFieldType::Int,    0, 3 },
		{ _T("DSNDriver"), EFieldType::String },
		{ _T("IP"),        EFieldType::String },
		{ _T("Port"),      EFieldType::Int,    1, 65535 },
		{ _T("DBName"),    EFieldType::String },
		{ _T("UID"),       EFieldType::String },
		{ _T("PWD"),       EFieldType::String },
		});
	reader.ReadObjects(_T("RedisNode"), _redisNodeVec, {
		{ _T("ID"),      EFieldType::Int,    0, INT16_MAX },
		{ _T("IP"),      EFieldType::String },
		{ _T("Port"),    EFieldType::Int,    1, 65535 },
		{ _T("UID"),     EFieldType::String },
		{ _T("PWD"),     EFieldType::String },
		{ _T("DBIndex"), EFieldType::Int,    0, INT32_MAX },
		});

	return reader.Validate();
}

//***************************************************************************
// @brief 현재 설정을 JSON 파일로 저장합니다.
// @detail 임시 파일에 먼저 쓴 뒤 교체하므로 저장에 실패해도 기존 파일은 그대로 남는다.
// @param tszServerInfo 저장할 파일 경로
// @return 저장에 성공하면 true
//***************************************************************************
bool CServiceConfig::Save(const TCHAR* tszServerInfo) const
{
	CRapidJSONUtil jsonUtil;
	CJSONConfigFieldWriter writer(jsonUtil);

	writer.Write(_T("ServiceName"), static_cast<const TCHAR*>(_tszServiceName));
	writer.Write(_T("DisplayName"), static_cast<const TCHAR*>(_tszDisplayName));
	writer.Write(_T("Name"), static_cast<const TCHAR*>(_tszServerName));
	writer.Write(_T("GroupId"), _nServerGroupId);
	writer.Write(_T("ChannelId"), _nServerChannelId);

	writer.Write(_T("IP"), static_cast<const TCHAR*>(_tszIP));
	writer.Write(_T("Port"), _nServerPort);
	writer.Write(_T("MaxSessionCount"), _nMaxSessionCount);
	writer.Write(_T("WorkerThreadCnt"), _nWorkerThreadCnt);
	writer.Write(_T("KeepAliveSec"), _nKeepAliveSec);

	writer.WriteObjects(_T("ServerNode"), _serverNodeVec);
	writer.WriteObjects(_T("DBNode"), _dbNodeVec);
	writer.WriteObjects(_T("RedisNode"), _redisNodeVec);

	return writer.Save(tszServerInfo);
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