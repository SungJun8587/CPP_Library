
//***************************************************************************
// MySQLConnPool.cpp : implementation of the CMySQLConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLConnPool.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CMySQLConnPool::CMySQLConnPool(int32& nMaxThreadCnt)
	: m_nMaxThreadCnt(nMaxThreadCnt)
{
	m_pMySQLConns = new CBaseMySQL * [m_nMaxThreadCnt];
	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
		m_pMySQLConns[i] = nullptr;

	memset(m_szDBHost, 0, DATABASE_SERVER_NAME_STRLEN);
	memset(m_szDBUserId, 0, DATABASE_DSN_USER_ID_STRLEN);
	memset(m_szDBPasswd, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	memset(m_szDBName, 0, DATABASE_NAME_STRLEN);
	m_uiPort = 0;
}

CMySQLConnPool::~CMySQLConnPool(void)
{
	clear();
	SAFE_DELETE_ARRAY(m_pMySQLConns);
	m_nMaxThreadCnt = 0;
}

//***************************************************************************
//
BOOL CMySQLConnPool::Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort)
{
	clear();

	strncpy_s(m_szDBHost, DATABASE_SERVER_NAME_STRLEN, pszDBHost, _TRUNCATE);
	strncpy_s(m_szDBUserId, DATABASE_DSN_USER_ID_STRLEN, pszDBPasswd, _TRUNCATE);
	strncpy_s(m_szDBPasswd, DATABASE_DSN_USER_PASSWORD_STRLEN, pszDBName, _TRUNCATE);
	strncpy_s(m_szDBName, DATABASE_NAME_STRLEN, pszDBUserId, _TRUNCATE);
	m_uiPort = uiPort;

	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
	{
		m_pMySQLConns[i] = new CBaseMySQL();
		if( !m_pMySQLConns[i]->Connect(m_szDBHost, m_szDBUserId, m_szDBPasswd, m_szDBName, m_uiPort) )
		{
			clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
BOOL CMySQLConnPool::Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort)
{
	clear();

	int nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_SERVER_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, m_szDBHost, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_ID_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, m_szDBUserId, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_PASSWORD_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, m_szDBPasswd, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, m_szDBName, nLength, NULL, NULL) == 0 ) return false;

	m_uiPort = uiPort;

	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
	{
		m_pMySQLConns[i] = new CBaseMySQL();
		if( !m_pMySQLConns[i]->Connect(m_szDBHost, m_szDBUserId, m_szDBPasswd, m_szDBName, m_uiPort) )
		{
			clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
CBaseMySQL* CMySQLConnPool::GetMySQLConn(int32 nType)
{
	CBaseMySQL* pMySQLConn = m_pMySQLConns[nType];

	if( pMySQLConn == nullptr || !pMySQLConn->IsConnected() )
	{
		if( pMySQLConn )
			SAFE_DELETE(pMySQLConn);

		pMySQLConn = new CBaseMySQL();
		if( !pMySQLConn->Connect(m_szDBHost, m_szDBUserId, m_szDBPasswd, m_szDBName, m_uiPort) )
		{
			SAFE_DELETE(pMySQLConn);
			m_pMySQLConns[nType] = nullptr;
			return nullptr;
		}

		LOG_DEBUG(_T("ReConnect MySQL...."));

		m_pMySQLConns[nType] = pMySQLConn;
	}

	return pMySQLConn;
}

//***************************************************************************
//
void CMySQLConnPool::clear(void)
{
	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
		SAFE_DELETE(m_pMySQLConns[i]);
}
