
//***************************************************************************
// ServerConfig.cpp: implementation of the CServerConfig class.
//
//***************************************************************************

#include "pch.h"
#include "ServerConfig.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CServerConfig 클래스의 생성자
//***************************************************************************
CServerConfig::CServerConfig()
	: _nServerGroupId(0), _nServerChannelId(0), _nServerPort(0), _nKeepAliveSec(0), _nMaxSessionCount(0)
	, _nWorkerThreadCnt(0), _nRedisPoolSize(0), _nDbWorkerThreadCnt(0), _nHeartbeatTtlSec(0), _nHeartbeatIntervalSec(0)
{
	memset(_tszServiceName, 0, sizeof(_tszServiceName));
	memset(_tszDisplayName, 0, sizeof(_tszDisplayName));
	memset(_tszServerName, 0, sizeof(_tszServerName));
	memset(_tszIP, 0, sizeof(_tszIP));
	memset(_tszFileServerUrl, 0, sizeof(_tszFileServerUrl));

	Clear();
}

//***************************************************************************
// @brief CServerConfig 클래스의 소멸자
//***************************************************************************
CServerConfig::~CServerConfig()
{
	Clear();
}

//***************************************************************************
// @brief JSON 설정 파일로부터 서버 구성 정보를 읽어와 초기화합니다.
// @param tszServerInfo JSON 설정 파일 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CServerConfig::Init(const TCHAR* tszServerInfo)
{
	int32 nSize = 0;

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

	_nRedisPoolSize = jsonUtil[_T("RedisPoolSize")];
	_nDbWorkerThreadCnt = jsonUtil[_T("DbWorkerThreadCnt")];
	_nHeartbeatTtlSec = jsonUtil[_T("HeartbeatTtlSec")];
	_nHeartbeatIntervalSec = jsonUtil[_T("HeartbeatIntervalSec")];

	// heartbeat 주기가 TTL보다 같거나 크면 갱신 전에 TTL이 만료되는 창이
	// 생긴다(CRedisServerHeartbeat::Start()도 동일하게 검증하지만, 설정
	// 파일 단계에서 미리 걸러 잘못된 값으로 서버가 뜨는 걸 막는다).
	if( _nHeartbeatIntervalSec <= 0 || _nHeartbeatTtlSec <= _nHeartbeatIntervalSec )
	{
		LOG_ERROR(_T("CServerConfig::Init: invalid heartbeat config (TtlSec=%d, IntervalSec=%d) — IntervalSec must be > 0 and < TtlSec"),
			_nHeartbeatTtlSec, _nHeartbeatIntervalSec);
		return false;
	}

	_tcsncpy_s(_tszFileServerUrl, _countof(_tszFileServerUrl), jsonUtil[_T("FileServerUrl")], _TRUNCATE);

	_serverNodeVec = jsonUtil.Deserialize<CVector<CServerNode>>(_T("ServerNode"));
	_dbNodeVec = jsonUtil.Deserialize<CVector<CDBNode>>(_T("DBNode"));
	_redisNodeVec = jsonUtil.Deserialize<CVector<CRedisNode>>(_T("RedisNode"));

	return true;
}

//***************************************************************************
// @brief 내부 동적 컨테이너 데이터를 소거하여 초기화합니다.
//***************************************************************************
void CServerConfig::Clear()
{
	_serverNodeVec.clear();
	_dbNodeVec.clear();
	_redisNodeVec.clear();
}

//***************************************************************************
// @brief 로드된 서버 설정 정보 및 노드 목록을 로그로 출력합니다.
//***************************************************************************
void CServerConfig::PrintServerSettingInfo()
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
	LOG_INFO(_T("RedisPoolSize : %d"), _nRedisPoolSize);
	LOG_INFO(_T("DbWorkerThreadCnt : %d"), _nDbWorkerThreadCnt);
	LOG_INFO(_T("HeartbeatTtlSec : %d"), _nHeartbeatTtlSec);
	LOG_INFO(_T("HeartbeatIntervalSec : %d"), _nHeartbeatIntervalSec);
	LOG_INFO(_T("FileServerUrl : %s"), _tszFileServerUrl);

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