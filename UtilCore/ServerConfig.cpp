
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
	: m_nServerPort(0), m_nMaxUser(0), m_nKeepAliveSec(0)
{
	memset(m_tszServerName, 0x00, sizeof(m_tszServerName));
	memset(m_tszIP, 0x00, sizeof(m_tszIP));
	memset(m_tszServiceName, 0x00, sizeof(m_tszServiceName));
	memset(m_tszDisplayName, 0x00, sizeof(m_tszDisplayName));

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

	CJSONParser jsonParser;
	_tstring jsonstring = jsonParser.ReadFile(tszServerInfo);
	jsonParser.Parse(jsonstring);

	_tcsncpy_s(m_tszServerName, _countof(m_tszServerName), ((_tstring)jsonParser[_T("Name")]).c_str(), _TRUNCATE);
	_tcsncpy_s(m_tszIP, _countof(m_tszIP), ((_tstring)jsonParser[_T("IP")]).c_str(), _TRUNCATE);
	_tcsncpy_s(m_tszServiceName, _countof(m_tszServiceName), ((_tstring)jsonParser[_T("ServiceName")]).c_str(), _TRUNCATE);
	_tcsncpy_s(m_tszDisplayName, _countof(m_tszDisplayName), ((_tstring)jsonParser[_T("DisplayName")]).c_str(), _TRUNCATE);
	m_nServerPort = (uint16)jsonParser[_T("Port")];
	m_nMaxUser = (int32)jsonParser[_T("MaxUser")];
	m_nKeepAliveSec = (int32)jsonParser[_T("KeepAliveSec")];

	//////////////////////////////////////////////////////////////////////////
	CJSONParser serverNodeValue = jsonParser[_T("ServerNode")];
	nSize = serverNodeValue.ArrayCount();
	if( 0 < nSize )
	{
		m_ServerNodeVec.resize(nSize);

		for( int32 i = 0; i < nSize; ++i )
		{
			CJSONParser node = serverNodeValue[i];
			m_ServerNodeVec[i].m_nID = static_cast<int16>(node[_T("ID")]);
			_tcsncpy_s(m_ServerNodeVec[i].m_tszServerName, DATABASE_SERVER_NAME_STRLEN, ((_tstring)node[_T("Name")]).c_str(), _TRUNCATE);
			_tcsncpy_s(m_ServerNodeVec[i].m_tszIP, IP_STRLEN, ((_tstring)node[_T("IP")]).c_str(), _TRUNCATE);
			m_ServerNodeVec[i].m_nPort = static_cast<int32>(node[_T("Port")]);
		}
	}

	CJSONParser dbNodeValue = jsonParser[_T("DBNode")];
	nSize = dbNodeValue.ArrayCount();
	if( 0 < nSize )
	{
		m_DBNodeVec.resize(nSize);

		for( int32 i = 0; i < nSize; ++i )
		{
			CJSONParser node = dbNodeValue[i];

			m_DBNodeVec[i].m_nID = static_cast<int16>(node[_T("ID")]);
			m_DBNodeVec[i].m_dbClass = GetInt8ToDBClass(static_cast<int8>(node[_T("DBClass")]));
			_tcsncpy_s(m_DBNodeVec[i].m_tszDSNDriver, DATABASE_DSN_DRIVER_STRLEN, ((_tstring)node[_T("DSNDriver")]).c_str(), _TRUNCATE);
			_tcsncpy_s(m_DBNodeVec[i].m_tszDBHost, IP_STRLEN, ((_tstring)node[_T("IP")]).c_str(), _TRUNCATE);
			m_DBNodeVec[i].m_nPort = static_cast<int32>(node[_T("Port")]);
			_tcsncpy_s(m_DBNodeVec[i].m_tszDBUserId, DATABASE_DSN_USER_ID_STRLEN, ((_tstring)node[_T("UID")]).c_str(), _TRUNCATE);
			_tcsncpy_s(m_DBNodeVec[i].m_tszDBPasswd, DATABASE_DSN_USER_PASSWORD_STRLEN, ((_tstring)node[_T("PWD")]).c_str(), _TRUNCATE);
			_tcsncpy_s(m_DBNodeVec[i].m_tszDBName, DATABASE_NAME_STRLEN, ((_tstring)node[_T("DBName")]).c_str(), _TRUNCATE);
			
			GetDBDSNString(m_DBNodeVec[i].m_tszDSN, m_DBNodeVec[i].m_dbClass, m_DBNodeVec[i].m_tszDSNDriver, m_DBNodeVec[i].m_tszDBHost, m_DBNodeVec[i].m_nPort, m_DBNodeVec[i].m_tszDBUserId, m_DBNodeVec[i].m_tszDBPasswd, m_DBNodeVec[i].m_tszDBName);
		}
	}

	return true;
}

//***************************************************************************
//
void CServerConfig::Clear(void)
{
	m_ServerNodeVec.clear();
	m_DBNodeVec.clear();
}

//***************************************************************************
//
void CServerConfig::PrintServerSettingInfo()
{
	LOG_INFO(_T("###################################################################"));
	LOG_INFO(_T("--------------- [Start Print : Server Setting Info] ---------------"));
	LOG_INFO(_T("Name : %s"), m_tszServerName);
	LOG_INFO(_T("IP : %s"), m_tszIP);
	LOG_INFO(_T("Port : %d"), m_nServerPort);
	LOG_INFO(_T("KeepAliveSec : %d"), m_nKeepAliveSec);
	LOG_INFO(_T("MaxUser : %d"), m_nMaxUser);

	LOG_INFO(_T("--------------- Connect ServerNode size : %d ---------------"), static_cast<int>(m_ServerNodeVec.size()));
	for( uint32 i = 0; i < m_ServerNodeVec.size(); i++ )
	{
		LOG_INFO(_T("ID : %d"), m_ServerNodeVec[i].m_nID);
		LOG_INFO(_T("ServerName : %s"), m_ServerNodeVec[i].m_tszServerName);
		LOG_INFO(_T("IP : %s"), m_ServerNodeVec[i].m_tszIP);
		LOG_INFO(_T("Port : %d"), m_ServerNodeVec[i].m_nPort);
		LOG_INFO(_T("------------------------------"));
	}

	LOG_INFO(_T("--------------- Connect DBNode size : %d ---------------"), static_cast<int>(m_DBNodeVec.size()));
	for( uint32 i = 0; i < m_DBNodeVec.size(); i++ )
	{
		LOG_INFO(_T("ID : %d"), m_DBNodeVec[i].m_nID);
		LOG_INFO(_T("DSNDriver : %s"), m_DBNodeVec[i].m_tszDSNDriver);
		LOG_INFO(_T("DBHost : %s"), m_DBNodeVec[i].m_tszDBHost);
		LOG_INFO(_T("Port : %d"), m_DBNodeVec[i].m_nPort);
		LOG_INFO(_T("DBName : %s"), m_DBNodeVec[i].m_tszDBName);
		LOG_INFO(_T("DBUserId : %s"), m_DBNodeVec[i].m_tszDBUserId);
		LOG_INFO(_T("DBPasswd : %s"), m_DBNodeVec[i].m_tszDBPasswd);
		LOG_INFO(_T("------------------------------"));
	}

	LOG_INFO(_T("--------------- [End Print] ---------------"));
	LOG_INFO(_T("###################################################################"));
}