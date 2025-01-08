
//***************************************************************************
// MySQLConnPool.h : interface for the CMySQLConnPool class.
//
//***************************************************************************

#ifndef __MYSQLCONNPOOL_H__
#define __MYSQLCONNPOOL_H__

#ifndef	__BASEMYSQL_H__
#include <BaseMySQL.h>
#endif

class CMySQLConnPool : public CPoolObj
{
public:
	CMySQLConnPool(int32& nMaxPoolSize);
	virtual ~CMySQLConnPool(void);

	bool		Init(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const uint32 uiPort);
	bool		Init(const wchar_t* pwszDBHost, const wchar_t* pwszDBUserId, const wchar_t* pwszDBPasswd, const wchar_t* pwszDBName, const uint32 uiPort);
	CBaseMySQL*	GetMySQLConn(int32 nType);

protected:
	void		Clear(void);

protected:
	CMySQLConnPool(const CMySQLConnPool& rhs);
	CMySQLConnPool& operator=(const CMySQLConnPool& rhs);

	CBaseMySQL**	 _pMySQLConns;			// 커넥션 풀 배열
	bool*			_pInUseFlag;			// 사용 상태 플래그 배열
	int32			_nMaxPoolSize;			// 최대 풀 크기

private:
	char	_szDBHost[DATABASE_SERVER_NAME_STRLEN];
	char	_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	char	_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	char	_szDBName[DATABASE_NAME_STRLEN];
	uint32	_uiPort;
};

#endif // ndef __MYSQLCONNPOOL_H__