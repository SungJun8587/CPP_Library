
//***************************************************************************
// BaseODBC.h : interface for the CBaseODBC class.
//
//***************************************************************************

#ifndef __BASEODBC_H__
#define __BASEODBC_H__

#include <sql.h>
#include <sqlext.h>

#include "DB_Error.inl"
#include "DB_ParamAttr.inl"
#include "DB_ColAttr.inl"

typedef	struct _COL_DESCRIPTION
{
	TCHAR tszColName[128];
	short NameLength;
	short EDataType;
	DWORD dwColSize;
	short DigitSize;
	short Nullable;
	long  DispLength;
} COL_DESCRIPTION, *PCOL_DESCRIPTION;

//***************************************************************************
//
class CBaseODBC
{
public:
	CBaseODBC(const EDBClass dbClass = EDBClass::NONE, const bool bLoadExcelFile = false);
	CBaseODBC(const EDBClass dbClass, const TCHAR* ptszDSN, const bool bLoadExcelFile = false);
	~CBaseODBC();

	bool		Connect(const int64 lLoginTimeOut = DATABASE_DEFAULT_LOGIN_TIMEOUT, const int64 lConnectionTimeOut = DATABASE_DEFAULT_CONNECTION_TIMEOUT);
	bool		Disconnect();
	bool		IsConnected();
	bool		IsConnectionValid() {
		return (m_hEnv != SQL_NULL_HENV && m_hConn != SQL_NULL_HDBC && m_hStmt != SQL_NULL_HSTMT ? TRUE : FALSE);
	}
	EDBClass    GetDBClass() { return m_DbClass; }
	bool        DBMSInfo(TCHAR* ptszServerName, TCHAR* ptszDBMSName, TCHAR* ptszDBMSVersion);

	bool		InitStmtHandle(const int64 lQueryTimeOut = DATABASE_DEFAULT_QUERY_TIMEOUT);
	void		FreeStmt(SQLUSMALLINT Option);
	void		ClearStmt(void);
	void		ResetParamStmt(void);
	void		UnBindColStmt(void);

	//***************************************************************************
	// 바인딩 관련
	// - IoType = SQL_PARAM_INPUT | SQL_PARAM_OUTPUT | SQL_PARAM_INPUTOUTPUT
	// - SqlType = SQL_CHAR | SQL_VARCHAR | SQL_INT | SQL_BIGINT | SQL_NUMERIC | SQL_DATETIME | ...
	// - RetSize = SQL_NTS | SQL_NULL_DATA | SQL_DEFAULT_PARAM | SQL_LEN_DATA_AT_EXEC | SQL_DATA_AT_EXEC

	bool        BindParameter(SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLULEN cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLLEN cbValueMax, SQLLEN* pcbValue);

	template< typename _TMain >
	bool		BindParamInput(_TMain& tValue);
	bool		BindParamInput(const TCHAR* ptszValue);

	template< typename _TMain >
	bool		BindParamInput(int32 iParamIndex, _TMain& tValue);
	bool		BindParamInput(int32 iParamIndex, const TCHAR* ptszValue, SQLLEN& lRetSize);
	bool		BindParamInput(int32 iParamIndex, const BYTE* pbData, int32 iSize, SQLLEN& lRetSize);

	template< typename _TMain >
	bool		BindParamOutput(_TMain& tValue);
	bool		BindParamOutput(TCHAR* ptszValue, int32& iBuffSize);

	template< typename _TMain >
	bool		BindParamOutput(int32 iParamIndex, _TMain& tValue);
	bool		BindParamOutput(int32 iParamIndex, TCHAR* ptszValue, int32& iBuffSize, SQLLEN& lRetSize);
	bool		BindParamOutput(int32 iParamIndex, BYTE* pbData, int32 iSize, SQLLEN& lRetSize);

	bool		BindCol(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* StrLen_or_Ind);

	template< typename _TMain >
	bool		BindCol(_TMain& tValue);
	bool		BindCol(TCHAR* ptszValue, int32& iBuffSize);

	template< typename _TMain >
	bool		BindCol(int32 iColIndex, _TMain& tValue, SQLLEN& lRetSize);
	bool		BindCol(int32 iColIndex, TCHAR* ptszValue, int32& iBuffSize, SQLLEN& lRetSize);
	
	bool		BindCol(int32 iColIndex, SQLSMALLINT targetType, int64& tValue, SQLLEN& lRetSize);
	bool		BindCol(int32 iColIndex, SQLSMALLINT targetType, uint64& tValue, SQLLEN& lRetSize);

	bool		PrepareQuery(const TCHAR* ptszQueryInfo);
	bool		Execute();									// 준비된 SQL 구문의 실행
	bool		ExecDirect(const TCHAR* ptszQueryInfo);		// SQL 구문을 바로 실행
	bool		BulkOperations(SQLSMALLINT operation);

	bool		SetStmtAttr(SQLINTEGER fAttribute, SQLPOINTER rgbValue, SQLINTEGER cbValueMax);
	bool		AllSets(LONG_PTR nQueryResultRecordSize, LONG_PTR nMaxRowSize);

	bool		Fetch(void);
	SQLRETURN	GetFetch(void);								//!< SQLFetch (외부 처리용)
	SQLRETURN	MoreResults(void);
	SQLINTEGER	GetFetchedRows(void) {
		return m_nFetchedRows[0];
	}

	bool		SetAutoCommitMode(SQLPOINTER valuePtr);
	bool		Commit();
	bool		Rollback();

	short		GetNumCols();		// 열 수
	int64		RowCount();			// insert, update, delete, select(모든 레코드 Fetch 후에 RowCount 적용됨)에 영향 받은 행 수
	long		RowNumber();		// 현재 커서의 행번호
	bool		DescribeCol(int32 iColNum, COL_DESCRIPTION& ColDescription); // 열 정보

	// Fetch 후에 값을 읽어온다.
	template< typename _TMain >
	bool		GetData(int32 iColNum, _TMain& tValue);
	bool		GetData(int32 iColNum, TCHAR* ptszData, int32& iBuffSize);

private:
	SQLHENV		m_hEnv;
	SQLHDBC		m_hConn;
	SQLHSTMT	m_hStmt;

	EDBClass        m_DbClass;
	bool			m_bLoadExcelFile;
	int16			m_nParamNum;
	int16			m_nColNum;
	SQLINTEGER		m_nFetchedRows[1];

	CDBParamAttrMgr		m_DBParamAttrMgr;
	CDBColAttrMgr		m_DBColAttrMgr;

	TCHAR		m_tszDSN[DATABASE_DSN_STRLEN];
	TCHAR		m_tszQueryInfo[SQL_MAX_MESSAGE_LENGTH];
	TCHAR		m_tszLastError[DATABASE_ERRORMSG_STRLEN];
};

#include "BaseODBC.inl"

#endif // ndef __BASEODBC_H__
