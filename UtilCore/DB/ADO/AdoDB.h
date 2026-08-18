
//***************************************************************************
// AdoDB.h : interface of the CAdoDB class.
//
//***************************************************************************

#ifndef __ADODB_H__
#define __ADODB_H__

#include <comutil.h>

#define  ADO_COM_CREATE_ERROR		-3
#define  ADO_OPEN_ERROR				-2
#define  ADO_COINITIALIZE_ERROR     -1
#define  ADO_OPEN_COMMAND_CREATE	1

#pragma warning(push)
#pragma warning(disable:4146)
#import "C:\Program Files\Common Files\System\ado\msado15.dll" no_namespace rename("EOF", "adoEOF")
#pragma warning(pop) 

class CAdoDB
{
public:
	CAdoDB();
	virtual ~CAdoDB();

	// 연결 및 상태 관리
	BOOL ISConnect();
	BOOL ISRSCon();
	BOOL ISOpen();
	BOOL ISCommand();
	int Connect(const EDBClass dbClass, LPCTSTR lptszConnstring, const int nTimeOut = 30);
	void ConClose();
	void RSClose();

	// 트랜잭션 관리
	long ConBeginTrans();
	void ConCommitTrans();
	void ConRollbackTrans();
	void ConCancel();

	// 포인터 및 상태 반환
	_CommandPtr GetCmdPointer();
	_RecordsetPtr GetRecPointer();
	BOOL GetDBCon();

	// 레코드 이동 및 탐색
	BOOL IsEOF();
	BOOL Next();
	BOOL Prev();
	BOOL First();
	BOOL Last();
	int GetRecCount();
	int GetFieldCount();

	// 필드 값 조회 (인덱스 기준)
	void GetFieldByIndex(_variant_t x, LPTSTR lptszValue, int nValueLen);
	void GetFieldByIndex(_variant_t x, long& lFieldValue);
	void GetFieldByIndex(_variant_t x, int32& nFieldValue);
	void GetFieldByIndex(_variant_t x, ulong& ulFieldValue);
	void GetFieldByIndex(_variant_t x, uint32& uFieldValue);
	void GetFieldByIndex(_variant_t x, double& dblFieldValue);
	void GetFieldByIndex(_variant_t x, _tstring& strFieldValue);

	// 필드 값 조회 (이름 기준)
	void GetFieldByName(LPCTSTR lptszFieldName, LPTSTR lptszValue, int nValueLen);
	void GetFieldByName(LPCTSTR lptszFieldName, long& lFieldValue);
	void GetFieldByName(LPCTSTR lptszFieldName, int32& nFieldValue);
	void GetFieldByName(LPCTSTR lptszFieldName, ulong& ulFieldValue);
	void GetFieldByName(LPCTSTR lptszFieldName, uint32& uFieldValue);
	void GetFieldByName(LPCTSTR lptszFieldName, double& dblFieldValue);
	void GetFieldByName(LPCTSTR lptszFieldName, _tstring& strFieldValue);

	// 쿼리 및 프로시저 실행
	BOOL Open(LPCTSTR lptszSourceBuf, const long lOption = -1);
	BOOL Execute(LPCTSTR lptszSourceBuf, const long lOption = -1);
	BOOL StoredProcedureExecute(LPCTSTR lptszStoredName, const long lOption = -1);

	// 프로시저 파라미터 관련
	long GetReturnValue();
	void CreateReturnParamAppend();
	void CreateArgParamAppend(_bstr_t bstrName, enum DataTypeEnum enumType, long lSize, _variant_t vt, BOOL bInOutCheck = TRUE);

	// 레코드셋 값 직접 조회 (GetRs)
	void GetRs(_variant_t x, _bstr_t& ret);
	void GetRs(_variant_t x, _variant_t& ret);
	void GetRs(_variant_t x, float& ret);
	void GetRs(_variant_t x, long& ret);
	void GetRs(_variant_t x, double& ret);

	// 서버 환경 정보 조회
	BOOL GetHostInfo(LPTSTR lptszOut, int nMaxLen);
	BOOL GetDBMSName(LPTSTR lptszOut, int nMaxLen);
	BOOL GetServerVersion(LPTSTR pszVersion, int nMaxLen);
	BOOL GetCharacterSetName(LPTSTR lptszOut, int nMaxLen);

private:
	_ConnectionPtr  m_pCon;     // ADO Connection 스마트 포인터
	_RecordsetPtr   m_pRs;      // ADO Recordset 스마트 포인터
	_CommandPtr     m_pCmd;     // ADO Command 스마트 포인터
	EDBClass        m_DbClass;  // 현재 연결된 데이터베이스 종류 분류
};

#endif // ndef __ADODB_H__