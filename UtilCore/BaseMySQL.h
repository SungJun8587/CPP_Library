
//***************************************************************************
// BaseMySQL.h : interface for the CBaseMySQL class.
//
//***************************************************************************

#ifndef __BASEMYSQL_H__
#define __BASEMYSQL_H__

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

	BOOL		Connect(const char* pszDBHost, const char* pszDBUserId, const char* pszDBPasswd, const char* pszDBName, const unsigned int nPort);
	BOOL		Disconnect();

	MYSQL*		GetConnPtr();
	BOOL		IsConnected();
	BOOL		GetServerInfo(TCHAR* ptszServerInfo);
	BOOL		GetClientInfo(TCHAR* ptszClientInfo);
	BOOL        SetCharacterSetName(const TCHAR* ptszCharacterSetName);
	BOOL        GetCharacterSetName(TCHAR* ptszCharacterSetName);
	BOOL		GetCharacterSetInfo(MY_CHARSET_INFO& charset);
	BOOL		GetEscapeString(char* pszDest, const char* pszSrc, int32 iLen);

	BOOL		Autocommit(bool bSetvalue);
	BOOL		StartTransaction();
	BOOL		Commit();
	BOOL		Rollback();

	uint64		GetAffectedRow();
	uint32		GetErrorNo();
	BOOL		GetErrorMessage(TCHAR* ptszMessage);
	BOOL		GetStmtErrorMessage(TCHAR* ptszMessage);
	
	BOOL		SelectDB(const char* pszSelectDBName);
	BOOL		SelectDB(const wchar_t* pszSelectDBName);

	BOOL		Prepare(const char* pszSQL);
	BOOL		Prepare(const wchar_t* pwszSQL);

	template< typename _TMain >
	BOOL		BindParam(_TMain tValue);
	BOOL		BindParam(const char* pszValue, ulong ulBufSize);
	BOOL		BindParam(const wchar_t* pwszValue, ulong ulBufSize);

	BOOL		PrepareExecute(uint64_t* pnIdx = NULL);

	BOOL		Execute(const char* pszSQL);
	BOOL		Execute(const wchar_t* pwszSQL);

	BOOL		Query(const char* pszSQL);
	BOOL		Query(const wchar_t* pwszSQL);
	BOOL		Query(const char* pszSQL, MYSQL_RES*& pRes);
	BOOL		Query(const wchar_t* pszSQL, MYSQL_RES*& pRes);

	BOOL		Query(const char* pszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));
	BOOL		Query(const wchar_t* pwszSQL, void* pclsData, bool (*FetchRow)(void*, MYSQL_ROW& Row));

	uint64		GetRowCount(MYSQL_RES* pRes);
	uint64		GetFieldCount(MYSQL_RES* pRes);
	BOOL		GetFields(MYSQL_RES* pRes, MYSQL_FIELD*& pFields, uint64& ui64FieldCount);

	void		GetData(const MYSQL_ROW Row, const int nColNum, bool& bIsData);
	void		GetData(const MYSQL_ROW Row, const int nColNum, char* pszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, wchar_t* pwszValue, int nBufSize);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int32& i32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint32& ui32Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, int64& i64Data);
	void		GetData(const MYSQL_ROW Row, const int nColNum, uint64& ui64Data);

protected:
	BOOL		InitConnHandle(const int nConnectTimeOut = MYSQL_DEFAULT_CONNECTION_TIMEOUT, const int nReadTimeOut = MYSQL_DEFAULT_QUERY_READ_TIMEOUT, const int nWriteTimeOut = MYSQL_DEFAULT_QUERY_WRITE_TIMEOUT);

private:
	BOOL		Connect();
	BOOL		PrepareClose();

	void        ErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);
	void        StmtErrorQuery(const char* pszFunc, const char* pszSQL, uint32 uiErrno = 0, const char* pszMessage = nullptr);

private:
	BOOL		m_bConnected;

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