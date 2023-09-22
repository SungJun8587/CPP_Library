
//***************************************************************************
// BaseODBC.h : interface for the CBaseODBC class.
//
//***************************************************************************

#ifndef __BASEODBC_H__
#define __BASEODBC_H__

#include <sql.h>
#include <sqlext.h>

#include "Db_Error.inl"
#include "Db_ParamAttr.inl"
#include "Db_ColAttr.inl"

typedef	struct _COL_DESCRIPTION
{
	TCHAR tszColName[128];
	short NameLength;
	short DataType;
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
	CBaseODBC(bool bLoadExcelFile = false);
	~CBaseODBC();

	BOOL		Connect(TCHAR* ptszDSN);
	BOOL		Disconnect();
	BOOL		IsConnected();
	BOOL		IsConnectionValid() {
		return (m_hConn != NULL && m_hEnv != NULL && m_hStmt != NULL ? TRUE : FALSE);
	}

	//***************************************************************************
	// ���ε� ����
	// - IoType = SQL_PARAM_INPUT | SQL_PARAM_OUTPUT | SQL_PARAM_INPUTOUTPUT
	// - SqlType = SQL_CHAR | SQL_VARCHAR | SQL_INT | SQL_BIGINT | SQL_NUMERIC | SQL_DATETIME | ...
	// - RetSize = SQL_NTS | SQL_NULL_DATA | SQL_DEFAULT_PARAM | SQL_LEN_DATA_AT_EXEC | SQL_DATA_AT_EXEC
	template< typename _TMain >
	BOOL		BindParamInput(_TMain& tValue);
	BOOL		BindParamInput(TCHAR* ptszValue);

	template< typename _TMain >
	BOOL		BindParamOutput(_TMain& tValue);
	BOOL		BindParamOutput(TCHAR* ptszValue, int32 nBuffSize);

	template< typename _TMain >
	BOOL		BindCol(_TMain& tValue);
	BOOL		BindCol(TCHAR* ptszValue, int32 nBuffSize);

	BOOL		PrepareQuery(TCHAR* ptszQueryInfo);
	BOOL		Execute();								// �غ�� SQL ������ ����
	BOOL		ExecDirect(TCHAR* ptszQueryInfo);		// SQL ������ �ٷ� ����

	BOOL		AllSets(LONG_PTR nQueryResultRecordSize, LONG_PTR nMaxRowSize);
	BOOL		Fetch(void);
	SQLRETURN	GetFetch(void);							//!< SQLFetch (�ܺ� ó����)
	SQLRETURN	MoreResults(void);
	SQLINTEGER	GetFetchedRows(void) {
		return m_nFetchedRows;
	}

	BOOL		AutoCommitOff(void);
	BOOL		Commit();
	BOOL		Rollback();
	BOOL		AutoCommitOn(void);

	long		NumResults();		// ���ڵ� �� 
	short		GetNumCols();		// �� ��
	long		RowCount();			// insert, update, delete�� ���� ���� �� ��
	long		RowNumber();		// ���� Ŀ���� ���ȣ
	BOOL		DescribeCol(int nColNum, COL_DESCRIPTION& ColDescription); // �� ����

	// Fetch �Ŀ� ���� �о�´�.
	template< typename _TMain >
	BOOL		GetData(int nColNum, _TMain& tValue);
	BOOL		GetData(int nColNum, TCHAR* ptszData, int nBufSize, long& lRetSize);

protected:
	BOOL		InitEnvHandle(void);
	BOOL		InitConnHandle(const LONG_PTR lLoginTimeOut=DATABASE_DEFAULT_LOGIN_TIMEOUT, const LONG_PTR lConnectionTimeOut= DATABASE_DEFAULT_CONNECTION_TIMEOUT);
	BOOL		InitStmtHandle(const LONG_PTR lQueryTimeOut=DATABASE_DEFAULT_QUERY_TIMEOUT);
	void		ClearStmt(void);

private:
	SQLHENV		m_hEnv;
	SQLHDBC		m_hConn;
	SQLHSTMT	m_hStmt;

	bool			m_bLoadExcelFile;
	int16			m_nParamNum;
	int16			m_nColNum;
	SQLINTEGER		m_nFetchedRows;

	CDBParamAttrMgr		m_DBParamAttrMgr;
	CDBColAttrMgr		m_DBColAttrMgr;

	TCHAR		m_tszQueryInfo[SQL_MAX_MESSAGE_LENGTH];
	TCHAR		m_tszLastError[DATABASE_ERRORMSG_STRLEN];
};

#include "BaseODBC.inl"

#endif // ndef __BASEODBC_H__
