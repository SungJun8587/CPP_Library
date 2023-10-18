
//***************************************************************************
// BaseMySQL.h : interface for the CBaseMySQL class.
//
//***************************************************************************

#ifndef __BASEMYSQL_H__
#define __BASEMYSQL_H__

#include <mysql.h>
#include <mysqld_error.h>

#pragma comment (lib, "libmySQL.lib")

#include "MySQL_ParamAttr.inl"

#ifndef	__OBJECTPOOL_H__
#include <ObjectPool.h>
#endif

typedef struct _DBNode
{
	_DBNode(void)
		: nID(0), nPort(0)
	{
		_tmemset(tszIP, 0, IP6_STRLEN);
		_tmemset(tszDBName, 0, DATABASE_NAME_STRLEN);
		_tmemset(tszID, 0, DATABASE_DSN_USER_ID_STRLEN);
		_tmemset(tszPW, 0, DATABASE_DSN_USER_PASSWORD_STRLEN);
	}

	int16			nID;
	TCHAR			tszIP[IP6_STRLEN];
	uint16			nPort;
	TCHAR			tszDBName[DATABASE_NAME_STRLEN];
	TCHAR			tszID[DATABASE_DSN_USER_ID_STRLEN];
	TCHAR			tszPW[DATABASE_DSN_USER_PASSWORD_STRLEN];
} DBNode, * PDBNode;

//***************************************************************************
//
class CBaseMySQL : public CPoolObj
{
public:
	CBaseMySQL();
	~CBaseMySQL();

	void        SetPluginDir(const char* pszPluginDir) {
		strncpy_s(m_szPluginDir, FULLPATH_STRLEN, pszPluginDir, _TRUNCATE);
	}

	bool		Connect(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort);
	bool		Disconnect();

	MYSQL*		GetConnPtr();
	bool		IsConnected();
	bool		GetServerInfo(TCHAR* ptszServerInfo);
	bool		GetClientInfo(TCHAR* ptszClientInfo);
	bool        SetCharacterSetName(const TCHAR* ptszCharacterSetName);
	bool        GetCharacterSetName(TCHAR* ptszCharacterSetName);
	bool		GetCharacterSetInfo(MY_CHARSET_INFO& charset);
	bool		GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen);

	bool		Autocommit(bool bSetvalue);
	bool		StartTransaction();
	bool		Commit();
	bool		Rollback();

	uint64		GetAffectedRow();
	uint32		GetErrorNo();
	bool		GetErrorMessage(TCHAR* ptszMessage);
	bool		GetStmtErrorMessage(TCHAR* ptszMessage);
	
	bool		SelectDB(const char* pszSelectDBName);
	bool		SelectDB(const wchar_t* pszSelectDBName);

	bool		Prepare(const char* pszSQL);
	bool		Prepare(const wchar_t* pwszSQL);

	template< typename _TMain >
	bool		BindParam(_TMain tValue);
	bool		BindParam(const char* pszValue, ulong ulBufSize);
	bool		BindParam(const wchar_t* pwszValue, ulong ulBufSize);

	bool		PrepareExecute(uint64_t* pnIdx = NULL);

	bool		Execute(const char* pszSQL);
	bool		Execute(const wchar_t* pwszSQL);

	bool		Query(const char* pszSQL);
	bool		Query(const wchar_t* pwszSQL);
	bool		Query(const char* pszSQL, MYSQL_RES*& pRes);
	bool		Query(const wchar_t* pszSQL, MYSQL_RES*& pRes);

	bool		Query(const char* pszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));
	bool		Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));

	uint64		GetRowCount(MYSQL_RES* pRes);
	uint64		GetFieldCount(MYSQL_RES* pRes);
	bool		GetFields(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount);

	void		GetData(const MYSQL_ROW Row, const int nColNum, bool& bIsData);
	void		GetData(const MYSQL_ROW Row, const int nColNum, char* pszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, wchar_t* pwszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int32& i32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint32& ui32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int64& i64Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint64& ui64Data);

protected:
	bool		InitConnHandle(const int nConnectTimeOut = MYSQL_DEFAULT_CONNECTION_TIMEOUT, const int nReadTimeOut = MYSQL_DEFAULT_QUERY_READ_TIMEOUT, const int nWriteTimeOut = MYSQL_DEFAULT_QUERY_WRITE_TIMEOUT);

private:
	bool		Connect();
	bool		PrepareClose();

	void        ErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);
	void        StmtErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);

private:
	bool		m_bConnected;

	char		m_szDBHost[DATABASE_SERVER_NAME_STRLEN];
	char		m_szDBUserId[DATABASE_DSN_USER_ID_STRLEN];
	char		m_szDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];
	char		m_szDBName[DATABASE_NAME_STRLEN];

	char		m_szSelectDBName[DATABASE_NAME_STRLEN];
	char		m_szCharacterSet[DATABASE_CHARACTERSET_STRLEN];
	char		m_szPluginDir[FULLPATH_STRLEN];

	uint32		m_uiPort;
	uint32		m_uiBindCount;
	uint32		m_uiParamNum;

	MYSQL*		m_pConn;
	MYSQL_STMT* m_pStmt;
	MYSQL_BIND* m_pBind;

	CMySQLParamAttrMgr		m_DBParamAttrMgr;
};

#include "BaseMySQL.inl"

#endif // ndef __BASEMYSQL_H__