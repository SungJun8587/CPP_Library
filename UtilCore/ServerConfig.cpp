
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
	, _nWorkerThreadCnt(0)
{
	memset(_tszServiceName, 0, sizeof(_tszServiceName));
	memset(_tszDisplayName, 0, sizeof(_tszDisplayName));
	memset(_tszServerName, 0, sizeof(_tszServerName));
	memset(_tszIP, 0, sizeof(_tszIP));
}

//***************************************************************************
// @brief CServerConfig 클래스의 소멸자
//***************************************************************************
CServerConfig::~CServerConfig()
{
}
