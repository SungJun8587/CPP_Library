
//***************************************************************************
// OdbcConnPool.cpp : implementation of the COdbcConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcConnPool.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

COdbcConnPool::COdbcConnPool(int32& nMaxPoolSize)
	: _dbClass(EDBClass::NONE), _nMaxPoolSize(nMaxPoolSize)
{
	_pOdbcConns = new CBaseODBC * [_nMaxPoolSize];
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
		_pOdbcConns[i] = nullptr;

	_pInUseFlag = new bool[_nMaxPoolSize];
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
		_pInUseFlag[i] = false;

	memset(&_tszDSN[0], 0, DATABASE_DSN_STRLEN);
}

COdbcConnPool::~COdbcConnPool(void)
{
	Clear();
	SAFE_DELETE_ARRAY(_pOdbcConns);
	SAFE_DELETE_ARRAY(_pInUseFlag);
	_nMaxPoolSize = 0;
}

//***************************************************************************
//
bool COdbcConnPool::Init(const EDBClass dbClass, const TCHAR* ptszDSN)
{
	Clear();
	_dbClass = dbClass;
	_tcsncpy_s(_tszDSN, _countof(_tszDSN), ptszDSN, _TRUNCATE);

	for( int32 i = 0; i < _nMaxPoolSize; i++ )
	{
		_pOdbcConns[i] = new CBaseODBC(dbClass, ptszDSN);
		if( !_pOdbcConns[i]->Connect() )
		{
			Clear();
			SAFE_DELETE_ARRAY(_pInUseFlag);
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
CBaseODBC* COdbcConnPool::GetOdbcConn(int32 nType)
{
	CBaseODBC* pOdbcConn = _pOdbcConns[nType];

	if( pOdbcConn == nullptr || !pOdbcConn->IsConnected() )
	{
		if( pOdbcConn != nullptr )
			SAFE_DELETE(pOdbcConn);

		pOdbcConn = new CBaseODBC(_dbClass, _tszDSN);
		if( !pOdbcConn->Connect() )
		{
			SAFE_DELETE(pOdbcConn);
			_pOdbcConns[nType] = nullptr;
			return nullptr;
		}

		LOG_DEBUG(_T("ReConnect ODBC...."));

		_pOdbcConns[nType] = pOdbcConn;
	}

	_pInUseFlag[nType] = true;

	return pOdbcConn;
}

//***************************************************************************
//
void COdbcConnPool::Clear(void)
{
	for( int32 i = 0; i < _nMaxPoolSize; i++ )
		SAFE_DELETE(_pOdbcConns[i]);
}
