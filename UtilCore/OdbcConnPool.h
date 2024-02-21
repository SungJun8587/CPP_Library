
//***************************************************************************
// OdbcConnPool.h : interface for the COdbcConnPool class.
//
//***************************************************************************

#ifndef __ODBCCONNPOOL_H__
#define __ODBCCONNPOOL_H__

#ifndef	__OBJECTPOOL_H__
#include <ObjectPool.h>
#endif

class COdbcConnPool : public CPoolObj
{
public:
	COdbcConnPool(int32& nMaxThreadCnt);
	virtual ~COdbcConnPool(void);

	bool Init(const EDBClass dbClass, const TCHAR* ptszDSN);
	CBaseODBC*	GetOdbcConn(int32 nType);

protected:
	void		clear(void);

protected:
	COdbcConnPool(const COdbcConnPool& rhs);
	COdbcConnPool& operator=(const COdbcConnPool& rhs);

	CBaseODBC**		m_pOdbcConns;
	EDBClass		m_dbClass;
	TCHAR			m_tszDSN[DATABASE_DSN_STRLEN];
	int32			m_nMaxThreadCnt;
};

#endif // ndef __ODBCCONNPOOL_H__