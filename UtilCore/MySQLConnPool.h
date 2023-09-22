
//***************************************************************************
// MySQLConnPool.h : interface for the CMySQLConnPool class.
//
//***************************************************************************

#ifndef __MYSQLCONNPOOL_H__
#define __MYSQLCONNPOOL_H__

#ifndef	__OBJECTPOOL_H__
#include <ObjectPool.h>
#endif

#ifndef	__BASEMYSQL_H__
#include <BaseMySQL.h>
#endif

class CMySQLConnPool : public CPoolObj
{
public:
	CMySQLConnPool(int32& nMaxThreadCnt);
	virtual ~CMySQLConnPool(void);

	BOOL		Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort);
	BOOL		Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort);
	CBaseMySQL*	GetMySQLConn(int32 nType);

protected:
	void		clear(void);

protected:
	CMySQLConnPool(const CMySQLConnPool& rhs);
	CMySQLConnPool& operator=(const CMySQLConnPool& rhs);

	CBaseMySQL**	m_pMySQLConns;
	int32			m_nMaxThreadCnt;

private:
	char	m_szDBHost[DATABASE_SERVER_NAME_STRLEN];
	char	m_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	char	m_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	char	m_szDBName[DATABASE_NAME_STRLEN];
	uint32	m_uiPort;
};

#endif // ndef __MYSQLCONNPOOL_H__