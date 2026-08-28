
//***************************************************************************
// BaseODBC.h : interface for the CBaseODBC class.
//
//***************************************************************************

#ifndef UC_BASEODBC_H
#define UC_BASEODBC_H

#include <sql.h>
#include <sqlext.h>

#include "DB_Error.inl"
#include "DB_ParamAttr.inl"
#include "DB_ColAttr.inl"

//***************************************************************************
// @brief ODBC 결과 세트 컬럼 상세 정보 구조체
// @detail 컬럼 이름, 데이터 타입, 크기, 소수점 자리수 및 널 허용 여부 등 
//         컬럼 기술(Describe Column) 정보를 보관하는 구조체입니다.
//***************************************************************************
typedef	struct _COL_DESCRIPTION
{
	TCHAR tszColName[128]; // 컬럼 이름 버퍼
	short NameLength;     // 컬럼 이름 길이
	short EDataType;      // SQL 데이터 타입
	DWORD dwColSize;      // 컬럼 크기 (정밀도)
	short DigitSize;      // 소수점 이하 자리수
	short Nullable;       // NULL 허용 여부 (SQL_NULLABLE, SQL_NO_NULLS 등)
	long  DispLength;     // 컬럼 출력 표시 크기
} COL_DESCRIPTION, * PCOL_DESCRIPTION;

//***************************************************************************
// @brief ODBC API 기반 데이터베이스 제어 기본 클래스
// @detail 환경, 연결, 문장(Statement) 핸들을 관리하며, 
//         파라미터/컬럼 바인딩, 쿼리 실행, 트랜잭션 처리 기능을 제공합니다.
//***************************************************************************
class CBaseODBC
{
public:
	CBaseODBC(const EDBClass dbClass = EDBClass::NONE, const bool bLoadExcelFile = false);
	CBaseODBC(const EDBClass dbClass, const TCHAR* ptszDSN, const bool bLoadExcelFile = false);
	~CBaseODBC();

	bool		Connect(const int64 lLoginTimeOut = DATABASE_DEFAULT_LOGIN_TIMEOUT, const int64 lConnectionTimeOut = DATABASE_DEFAULT_CONNECTION_TIMEOUT);
	bool		Disconnect();
	bool		IsConnected();

	//***************************************************************************
	// @brief ODBC 핸들(환경, 연결, 문장)의 유효성을 검사하여 현재 연결 가능 상태인지 확인합니다.
	// @return bool 모든 핸들이 유효하면 TRUE(1), 하나라도 유효하지 않으면 FALSE(0)
	//***************************************************************************
	bool		IsConnectionValid() {
		return (m_hEnv != SQL_NULL_HENV && m_hConn != SQL_NULL_HDBC && m_hStmt != SQL_NULL_HSTMT ? TRUE : FALSE);
	}

	//***************************************************************************
	// @brief 설정된 데이터베이스 종류(EDBClass)를 반환합니다.
	// @return EDBClass 현재 데이터베이스 클래스 구분 값
	//***************************************************************************
	EDBClass    GetDBClass() { return m_DbClass; }

	bool		GetServerName(TCHAR* ptszServerName, int32 nBufferLength);
	bool		GetDBMSName(TCHAR* ptszDBMSName, int32 nBufferLength);
	bool		GetDBMSVersion(TCHAR* ptszDBMSVersion, int32 nBufferLength);
	bool        GetServerCharacterSet(TCHAR* ptszCharset, int32 nBufferLength);

	bool		InitStmtHandle(const int64 lQueryTimeOut = DATABASE_DEFAULT_QUERY_TIMEOUT);
	void		FreeStmt(SQLUSMALLINT Option);
	void		ClearStmt(void);
	void		ResetParamStmt(void);
	void		UnBindColStmt(void);

	bool        BindParameter(SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLULEN cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLLEN cbValueMax, SQLLEN* pcbValue);

	template< typename _TMain >
	bool		BindParamInput(_TMain& tValue);
	bool		BindParamInput(const TCHAR* ptszValue, SQLLEN* plDataLength = nullptr);

	template< typename _TMain >
	bool		BindParamInput(int32 iParamIndex, _TMain& tValue);
	bool		BindParamInput(int32 iParamIndex, const TCHAR* ptszValue, SQLLEN& lDataLength);
	bool		BindParamInput(int32 iParamIndex, const BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength);

	template< typename _TMain >
	bool		BindParamOutput(_TMain& tValue);
	bool		BindParamOutput(TCHAR* ptszValue, int32& nBufferLength);

	template< typename _TMain >
	bool		BindParamOutput(int32 iParamIndex, _TMain& tValue);
	bool		BindParamOutput(int32 iParamIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength);
	bool		BindParamOutput(int32 iParamIndex, BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength);

	bool		BindCol(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* plDataLength);

	template< typename _TMain >
	bool		BindCol(_TMain& tValue);
	bool		BindCol(TCHAR* ptszValue, int32& nBufferLength, SQLLEN* plDataLength = nullptr);

	template< typename _TMain >
	bool		BindCol(int32 iColIndex, _TMain& tValue, SQLLEN& lDataLength);
	bool		BindCol(int32 iColIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength);

	bool		BindCol(int32 iColIndex, SQLSMALLINT targetType, int64& tValue, SQLLEN& lDataLength);
	bool		BindCol(int32 iColIndex, SQLSMALLINT targetType, uint64& tValue, SQLLEN& lDataLength);

	bool		GetData(SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue, SQLLEN BufferLength, SQLLEN* plDataLength);

	template< typename _TMain >
	bool		GetData(int32 iColNum, _TMain& tValue);
	bool		GetData(int32 iColNum, TCHAR* ptszData, int32& nBufferLength);

	bool		PrepareQuery(const TCHAR* ptszQueryInfo);
	bool		Execute();
	bool		ExecDirect(const TCHAR* ptszQueryInfo);
	bool		BulkOperations(SQLSMALLINT operation);

	bool		SetStmtAttr(SQLINTEGER fAttribute, SQLPOINTER rgbValue, SQLINTEGER cbValueMax);
	bool		AllSets(LONG_PTR nQueryResultRecordSize, LONG_PTR nMaxRowSize);

	bool		Fetch(void);
	SQLRETURN	GetFetch(void);
	SQLRETURN	MoreResults(void);

	//***************************************************************************
	// @brief Fetch 작업 시 한 번에 인출(Fetched)된 행의 개수를 반환합니다.
	// @return SQLINTEGER 인출된 행의 수
	//***************************************************************************
	SQLINTEGER	GetFetchedRows(void) {
		return m_nFetchedRows[0];
	}

	bool		SetAutoCommitMode(SQLPOINTER valuePtr);
	bool		Commit();
	bool		Rollback();

	short		GetNumCols();
	int64		RowCount();
	long		RowNumber();
	bool		DescribeCol(int32 iColNum, COL_DESCRIPTION& ColDescription);

private:
	SQLHENV		m_hEnv;                     // ODBC 환경 핸들 (Environment Handle)
	SQLHDBC		m_hConn;                    // ODBC 데이터베이스 연결 핸들 (Connection Handle)
	SQLHSTMT	m_hStmt;                    // SQL 쿼리 실행 및 결과를 다루기 위한 문장 핸들 (Statement Handle)

	EDBClass        m_DbClass;              // 데이터베이스 종류 (Enum 타입)
	bool			m_bLoadExcelFile;       // 엑셀 파일 로드 여부 플래그
	int16			m_nParamNum;            // SQL 쿼리에 바인딩된 파라미터 총 개수
	int16			m_nColNum;              // 쿼리 결과셋의 컬럼 총 개수
	SQLINTEGER		m_nFetchedRows[1];      // Fetch 작업 시 한 번에 가져온(fetched) 행의 수 배열

	CDBParamAttrMgr		m_DBParamAttrMgr;  // 파라미터 속성을 관리하는 매니저 객체
	CDBColAttrMgr		m_DBColAttrMgr;    // 컬럼 속성을 관리하는 매니저 객체

	TCHAR		m_tszDSN[DATABASE_DSN_STRLEN];          // 데이터 소스 이름 (DSN) 문자열 버퍼
	TCHAR		m_tszQueryInfo[SQL_MAX_MESSAGE_LENGTH]; // 실행할 쿼리문 또는 관련 정보를 저장하는 문자열 버퍼
};

#include "BaseODBC.inl"

#endif // ndef UC_BASEODBC_H