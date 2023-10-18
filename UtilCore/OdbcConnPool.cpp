
//***************************************************************************
// OdbcConnPool.cpp : implementation of the COdbcConnPool class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcConnPool.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

COdbcConnPool::COdbcConnPool(int32& nMaxThreadCnt)
	: m_dbClass(DB_CLASS::DB_NONE), m_nMaxThreadCnt(nMaxThreadCnt)
{
	m_pOdbcConns = new CBaseODBC * [m_nMaxThreadCnt];
	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
		m_pOdbcConns[i] = nullptr;

	memset(&m_tszDSN[0], 0, DATABASE_DSN_STRLEN);
}

COdbcConnPool::~COdbcConnPool(void)
{
	clear();
	SAFE_DELETE_ARRAY(m_pOdbcConns);
	m_nMaxThreadCnt = 0;
}

//***************************************************************************
//
bool COdbcConnPool::Init(const DB_CLASS dbClass, const TCHAR* ptszDSN)
{
	clear();
	m_dbClass = dbClass;
	_tcsncpy_s(m_tszDSN, _countof(m_tszDSN), ptszDSN, _TRUNCATE);

	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
	{
		m_pOdbcConns[i] = new CBaseODBC(dbClass, ptszDSN);
		if( !m_pOdbcConns[i]->Connect() )
		{
			clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
//
CBaseODBC* COdbcConnPool::GetOdbcConn(int32 nType)
{
	CBaseODBC* pOdbcConn = m_pOdbcConns[nType];

	if( pOdbcConn == nullptr || !pOdbcConn->IsConnected() )
	{
		if( pOdbcConn )
			SAFE_DELETE(pOdbcConn);

		pOdbcConn = new CBaseODBC(m_dbClass, m_tszDSN);
		if( !pOdbcConn->Connect() )
		{
			SAFE_DELETE(pOdbcConn);
			m_pOdbcConns[nType] = nullptr;
			return nullptr;
		}

		LOG_DEBUG(_T("ReConnect ODBC...."));

		m_pOdbcConns[nType] = pOdbcConn;
	}

	return pOdbcConn;
}

//***************************************************************************
//
void COdbcConnPool::clear(void)
{
	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
		SAFE_DELETE(m_pOdbcConns[i]);
}
