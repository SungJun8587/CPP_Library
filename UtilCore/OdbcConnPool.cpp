
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
	: m_nMaxThreadCnt(nMaxThreadCnt)
{
	m_pOdbcConns = new CBaseODBC * [m_nMaxThreadCnt];
	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
		m_pOdbcConns[i] = nullptr;

	memset(&m_szConnStr[0], 0, DATABASE_DSN_STRLEN);
}

COdbcConnPool::~COdbcConnPool(void)
{
	clear();
	SAFE_DELETE_ARRAY(m_pOdbcConns);
	m_nMaxThreadCnt = 0;
}

//***************************************************************************
//
BOOL COdbcConnPool::Init(TCHAR* ptszConnStr)
{
	clear();
	_tcsncpy_s(&m_szConnStr[0], DATABASE_DSN_STRLEN, ptszConnStr, _TRUNCATE);

	for( int32 i = 0; i < m_nMaxThreadCnt; ++i )
	{
		m_pOdbcConns[i] = new CBaseODBC();
		if( !m_pOdbcConns[i]->Connect(m_szConnStr) )
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

		pOdbcConn = new CBaseODBC();
		if( !pOdbcConn->Connect(m_szConnStr) )
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
