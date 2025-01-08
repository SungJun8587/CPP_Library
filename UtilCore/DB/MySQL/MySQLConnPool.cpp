
//***************************************************************************
// MySQLConnPool.cpp : implementation of the CMySQLConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLConnPool.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CMySQLConnPool::CMySQLConnPool(int32& nMaxPoolSize)
	: _nMaxPoolSize(nMaxPoolSize)
{
	_pMySQLConns = new CBaseMySQL * [_nMaxPoolSize];
	for( int32 i = 0; i < _nMaxPoolSize; ++i )
		_pMySQLConns[i] = nullptr;

	_pInUseFlag = new bool[_nMaxPoolSize];
	for( int32 i = 0; i < _nMaxPoolSize; ++i )
		_pInUseFlag[i] = false;

	memset(&_szDBHost[0], 0, DATABASE_SERVER_NAME_STRLEN);
	memset(&_szDBUserId[0], 0, DATABASE_DSN_USER_ID_STRLEN);
	memset(&_szDBPasswd[0], 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	memset(&_szDBName[0], 0, DATABASE_NAME_STRLEN);
	_uiPort = 0;
}

CMySQLConnPool::~CMySQLConnPool(void)
{
	Clear();
	SAFE_DELETE_ARRAY(_pMySQLConns);
	SAFE_DELETE_ARRAY(_pInUseFlag);
	_nMaxPoolSize = 0;
}

//***************************************************************************
//
bool CMySQLConnPool::Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort)
{
	//Clear();
	strncpy_s(_szDBHost, DATABASE_SERVER_NAME_STRLEN, pszDBHost, _TRUNCATE);
	strncpy_s(_szDBUserId, DATABASE_DSN_USER_ID_STRLEN, pszDBPasswd, _TRUNCATE);
	strncpy_s(_szDBPasswd, DATABASE_DSN_USER_PASSWORD_STRLEN, pszDBName, _TRUNCATE);
	strncpy_s(_szDBName, DATABASE_NAME_STRLEN, pszDBUserId, _TRUNCATE);
	_uiPort = uiPort;

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		_pMySQLConns[i] = new CBaseMySQL(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( !_pMySQLConns[i]->Connect() )
		{
			Clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
bool CMySQLConnPool::Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort)
{
	//Clear();

	int nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_SERVER_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBHost, -1, _szDBHost, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_ID_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBUserId, -1, _szDBUserId, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_DSN_USER_PASSWORD_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBPasswd, -1, _szDBPasswd, nLength, NULL, NULL) == 0 ) return false;

	nLength = WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, NULL, 0, NULL, NULL);
	if( nLength == 0 || DATABASE_NAME_STRLEN < (size_t)nLength - 1 ) return false;
	if( WideCharToMultiByte(CP_ACP, 0, pwszDBName, -1, _szDBName, nLength, NULL, NULL) == 0 ) return false;

	_uiPort = uiPort;

	for( int32 i = 0; i < _nMaxPoolSize; ++i )
	{
		_pMySQLConns[i] = new CBaseMySQL(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( !_pMySQLConns[i]->Connect() )
		{
			Clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
CBaseMySQL* CMySQLConnPool::GetMySQLConn(int32 nType)
{
	CBaseMySQL* pMySQLConn = _pMySQLConns[nType];

	if( pMySQLConn == nullptr || !pMySQLConn->IsConnected() )
	{
		if( pMySQLConn )
			SAFE_DELETE(pMySQLConn);

		pMySQLConn = new CBaseMySQL(_szDBHost, _szDBUserId, _szDBPasswd, _szDBName, _uiPort);
		if( !pMySQLConn->Connect() )
		{
			SAFE_DELETE(pMySQLConn);
			_pMySQLConns[nType] = nullptr;
			return nullptr;
		}

		LOG_DEBUG(_T("ReConnect MySQL...."));

		_pMySQLConns[nType] = pMySQLConn;
	}

	_pInUseFlag[nType] = true;

	return pMySQLConn;
}

//***************************************************************************
//
void CMySQLConnPool::Clear(void)
{
	for( int32 i = 0; i < _nMaxPoolSize; ++i )
		SAFE_DELETE(_pMySQLConns[i]);
}
